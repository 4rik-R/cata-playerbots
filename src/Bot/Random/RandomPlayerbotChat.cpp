/*
 * Random-bot chat. Occasions and German lines follow mod-playerbots'
 * broadcast and suggestion texts (ai_playerbot_texts); keyword replies and the
 * AI hand-off are written for this port. Hooks only queue facts; everything
 * that touches other players runs on the world thread in Update().
 * Released under GNU GPL v2 or any later version.
 */
#include "RandomPlayerbotChat.h"
#include "RandomPlayerbotChatAi.h"
#include "RandomPlayerbotChatPolicy.h"
#include "RandomPlayerbotChatTexts.h"
#include "RandomPlayerbotMgr.h"
#include "../../Ai/Base/PlayerbotRoles.h"
#include "Channel.h"
#include "Chat.h"
#include "ChatPackets.h"
#include "Creature.h"
#include "DBCStores.h"
#include "Group.h"
#include "Bag.h"
#include "Item.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "Timer.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

using namespace RandomPlayerbotChatPolicy;
using RandomPlayerbotChatTexts::Lines;

namespace
{
// Chat channel ids (ChatChannels.dbc).
constexpr uint32 ChannelGeneral = 1;
constexpr uint32 ChannelTrade = 2;
constexpr uint32 ChannelLookingForGroup = 26;
// Unsolicited lines: at most one per bot in this time.
constexpr uint32 OwnLineGapMs = 30000;
constexpr uint32 CombatLineGapMs = 60000;
constexpr uint32 AoeGapMs = 300000;
// A bot freshly logged in does not cheer for levels it got on login.
constexpr uint32 SettleMs = 60000;
constexpr uint32 SayRange = 25;
constexpr uint32 EmoteRange = 40;
constexpr std::size_t MaxOutgoing = 64;
constexpr uint32 HistoryKeepMs = 600000;
constexpr uint32 ChannelJoinRetryMs = 60000;

// Text emote ids (EmotesText.dbc).
enum TextEmote : uint32
{
    TE_BOW = 17, TE_BYE = 19, TE_CHEER = 21, TE_CHICKEN = 22, TE_DANCE = 34, TE_FLEX = 41, TE_GREET = 48,
    TE_HELLO = 55, TE_HUG = 56, TE_KISS = 58, TE_LAUGH = 60, TE_NO = 66, TE_RUDE = 77, TE_SALUTE = 78,
    TE_THANK = 97, TE_WAVE = 101, TE_WELCOME = 102, TE_SMILE = 163
};

uint32 AnswerEmote(uint32 emote)
{
    switch (emote)
    {
        case TE_WAVE: case TE_HELLO: case TE_BYE: return TE_WAVE;
        case TE_GREET: case TE_BOW: return TE_BOW;
        case TE_THANK: return TE_WELCOME;
        case TE_WELCOME: return TE_THANK;
        case TE_DANCE: return TE_DANCE;
        case TE_CHEER: return TE_CHEER;
        case TE_LAUGH: case TE_FLEX: return TE_LAUGH;
        case TE_SALUTE: return TE_SALUTE;
        case TE_HUG: case TE_KISS: case TE_SMILE: return TE_SMILE;
        case TE_RUDE: case TE_CHICKEN: return TE_NO;
        default: return 0;
    }
}

bool Reached(uint32 now, uint32 at) { return int32(now - at) >= 0; }

enum class Kind : uint8 { Whisper, Party, Say, Channel, Emote, Level, Quest, Kill, Loot };

struct Event
{
    Kind What = Kind::Whisper;
    ObjectGuid Actor;
    ObjectGuid Target;
    ObjectGuid GroupGuid;
    Channel const* Chan = nullptr; // compared only, never dereferenced from here
    std::string Text;
    uint32 Value = 0;
    uint32 Value2 = 0;
};

enum class Where : uint8 { Whisper, Party, Say, Channel, Emote, EmoteStop };

struct Line
{
    uint32 SendAt = 0;
    uint32 Created = 0;
    ObjectGuid Bot;
    Where To = Where::Say;
    ObjectGuid Target;
    ObjectGuid GroupGuid;
    Channel const* Chan = nullptr;
    std::string Text;
    std::string Key;
    uint64 AiId = 0;
    uint32 AiDeadline = 0;
    uint32 Depth = 0;
    uint32 EmoteId = 0;
    bool Reply = false;
};

struct Said
{
    ObjectGuid Who;
    std::string Name;
    std::string Text;
};

struct Conversation
{
    std::deque<Said> Lines;
    uint32 Touched = 0;
};

RandomPlayerbotChat::Settings Config;
std::atomic<bool> IsEnabled { false };
std::mutex EventLock;
std::vector<Event> Events;

// World thread only from here.
std::vector<Line> Outgoing;
std::unordered_map<std::string, Conversation> History;
std::unordered_map<ObjectGuid, uint32> FirstSeen, LastSpoke, LastCombatLine, JoinTry;
std::unordered_map<ObjectGuid, ObjectGuid> GroupOf;
std::unordered_map<ObjectGuid, uint32> GroupGreeted, GroupAoe, HumanWrote;
std::unordered_map<uint64, uint32> ChatterAt;
uint32 NextChatterScan = 0, NextCombatScan = 0, NextPrune = 0;
uint64 NextAiId = 0;
uint32 EventCounter = 0;
std::atomic<uint32> LineCount { 0 }, ReplyCount { 0 }, EmoteCount { 0 }, AiUsed { 0 };

void Queue(Event event)
{
    std::lock_guard lock(EventLock);
    if (Events.size() < 256)
        Events.push_back(std::move(event));
}

bool IsRandomBot(Player const& player)
{
    WorldSession* session = player.GetSession();
    return session && session->IsServerOrigin() && RandomPlayerbotMgr::IsRandomAccount(session->GetAccountId());
}

bool IsHuman(Player const& player)
{
    return player.GetSession() && !player.GetSession()->IsServerOrigin();
}

uint32 HumansIn(Group const& group)
{
    uint32 humans = 0;
    for (GroupReference const* ref = group.GetFirstMember(); ref; ref = ref->next())
        if (Player const* member = ref->GetSource())
            humans += IsHuman(*member) ? 1 : 0;
    return humans;
}

std::vector<Player*> BotsIn(Group const& group)
{
    std::vector<Player*> bots;
    for (GroupReference const* ref = group.GetFirstMember(); ref; ref = ref->next())
        if (Player* member = ref->GetSource())
            if (member->IsInWorld() && IsRandomBot(*member))
                bots.push_back(member);
    return bots;
}

template <typename T>
T* Pick(std::vector<T*> const& items)
{
    return items.empty() ? nullptr : items[urand(0, uint32(items.size() - 1))];
}

bool Chance(uint32 percent) { return percent >= 100 || urand(0, 99) < percent; }

// ---- German facts about a bot ----

char const* ClassName(uint8 playerClass)
{
    switch (playerClass)
    {
        case CLASS_WARRIOR: return "Krieger";
        case CLASS_PALADIN: return "Paladin";
        case CLASS_HUNTER: return "Jäger";
        case CLASS_ROGUE: return "Schurke";
        case CLASS_PRIEST: return "Priester";
        case CLASS_DEATH_KNIGHT: return "Todesritter";
        case CLASS_SHAMAN: return "Schamane";
        case CLASS_MAGE: return "Magier";
        case CLASS_WARLOCK: return "Hexenmeister";
        case CLASS_DRUID: return "Druide";
        default: return "Abenteurer";
    }
}

char const* RaceName(uint8 race)
{
    switch (race)
    {
        case RACE_HUMAN: return "Mensch";
        case RACE_ORC: return "Orc";
        case RACE_DWARF: return "Zwerg";
        case RACE_NIGHTELF: return "Nachtelf";
        case RACE_UNDEAD_PLAYER: return "Untoter";
        case RACE_TAUREN: return "Taure";
        case RACE_GNOME: return "Gnom";
        case RACE_TROLL: return "Troll";
        case RACE_GOBLIN: return "Goblin";
        case RACE_BLOODELF: return "Blutelf";
        case RACE_DRAENEI: return "Draenei";
        case RACE_WORGEN: return "Worgen";
        default: return "Held";
    }
}

std::string RoleName(Player const& bot)
{
    if (PlayerbotRoles::IsTank(bot))
        return "Tank";
    if (PlayerbotRoles::IsHealer(bot))
        return "Heiler";
    return "DD";
}

std::string ZoneName(Player const& bot)
{
    if (AreaTableEntry const* zone = sAreaTableStore.LookupEntry(bot.GetZoneId()))
        if (zone->AreaName && *zone->AreaName)
            return zone->AreaName;
    return "der Gegend";
}

bool InCapital(Player const& bot)
{
    switch (bot.GetZoneId())
    {
        case 1519: case 1537: case 1657: case 3557: // Stormwind, Ironforge, Darnassus, Exodar
        case 1637: case 1638: case 1497: case 3487: // Orgrimmar, Thunder Bluff, Undercity, Silvermoon
        case 4395: case 3703: // Dalaran, Shattrath
            return true;
        default:
            return false;
    }
}

bool HasQuest(Player const& bot)
{
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        if (bot.GetQuestSlotQuestId(slot))
            return true;
    return false;
}

enum class Doing : uint8 { Questing, Grinding, Fighting, Dungeon, Capital, Flying, Sailing, Dead, Resting };

Doing Activity(Player const& bot)
{
    if (!bot.IsAlive())
        return Doing::Dead;
    if (bot.IsInFlight())
        return Doing::Flying;
    if (bot.GetTransport())
        return Doing::Sailing;
    if (bot.GetMap() && bot.GetMap()->IsDungeon())
        return Doing::Dungeon;
    if (bot.IsInCombat())
        return Doing::Fighting;
    if (InCapital(bot))
        return Doing::Capital;
    if (bot.IsSitState())
        return Doing::Resting;
    return HasQuest(bot) ? Doing::Questing : Doing::Grinding;
}

Lines ActivityLines(Doing doing)
{
    switch (doing)
    {
        case Doing::Questing: return Lines::ActivityQuesting;
        case Doing::Fighting: return Lines::ActivityFighting;
        case Doing::Dungeon: return Lines::ActivityDungeon;
        case Doing::Capital: return Lines::ActivityCapital;
        case Doing::Flying: return Lines::ActivityFlying;
        case Doing::Sailing: return Lines::ActivitySailing;
        case Doing::Dead: return Lines::ActivityDead;
        case Doing::Resting: return Lines::ActivityResting;
        default: return Lines::ActivityGrinding;
    }
}

char const* ActivityForAi(Doing doing)
{
    switch (doing)
    {
        case Doing::Questing: return "erledigst gerade Quests";
        case Doing::Fighting: return "kämpfst gerade gegen Gegner";
        case Doing::Dungeon: return "bist gerade in einem Dungeon";
        case Doing::Capital: return "bist zum Reparieren und Verkaufen in der Hauptstadt";
        case Doing::Flying: return "fliegst gerade mit einem Flugtier";
        case Doing::Sailing: return "fährst gerade mit dem Schiff oder Zeppelin";
        case Doing::Dead: return "bist gestorben und läufst als Geist zurück";
        case Doing::Resting: return "isst und trinkst gerade";
        default: return "jagst Gegner für Erfahrung";
    }
}

std::string RandomLine(Lines lines)
{
    auto set = RandomPlayerbotChatTexts::Get(lines);
    return set.Size ? set.Items[urand(0, set.Size - 1)] : std::string();
}

Values BasicValues(Player const& bot, std::string const& partner)
{
    Values values;
    values["zone_name"] = ZoneName(bot);
    values["my_level"] = std::to_string(bot.getLevel());
    values["my_class"] = ClassName(bot.getClass());
    values["my_race"] = RaceName(bot.getRace());
    values["my_role"] = RoleName(bot);
    values["activity"] = RandomLine(ActivityLines(Activity(bot)));
    if (!partner.empty())
        values["s"] = values["other_name"] = partner;
    return values;
}

// A filled line of the set, or empty if none fits.
std::string Compose(Lines lines, Values const& values)
{
    auto set = RandomPlayerbotChatTexts::Get(lines);
    if (!set.Size)
        return {};
    uint32 start = urand(0, set.Size - 1);
    for (uint32 i = 0; i < std::min<uint32>(set.Size, 12); ++i)
        if (auto text = Fill(set.Items[(start + i * 7) % set.Size], values))
            return CutUtf8(*text, 250);
    return {};
}

std::string ItemLink(ItemTemplate const& proto)
{
    char color[16];
    uint32 quality = std::min<uint32>(proto.GetQuality(), MAX_ITEM_QUALITY - 1);
    snprintf(color, sizeof(color), "|c%08x", ItemQualityColors[quality]);
    char const* name = proto.GetName(LOCALE_deDE);
    return std::string(color) + "|Hitem:" + std::to_string(proto.GetId()) + ":0:0:0:0:0:0:0:0:0|h[" +
        (name && *name ? name : proto.GetDefaultLocaleName()) + "]|h|r";
}

std::string QuestTitle(Quest const& quest)
{
    std::string title = quest.GetTitle();
    if (QuestLocale const* locale = sObjectMgr->GetQuestLocale(quest.GetQuestId()))
        ObjectMgr::GetLocaleString(locale->Title, LOCALE_deDE, title);
    return title;
}

std::string QuestLink(Quest const& quest, uint32 level)
{
    int32 questLevel = quest.GetQuestLevel() > 0 ? quest.GetQuestLevel() : int32(level);
    return "|cffffff00|Hquest:" + std::to_string(quest.GetQuestId()) + ":" + std::to_string(questLevel) + "|h[" +
        QuestTitle(quest) + "]|h|r";
}

std::string AnyQuestLink(Player const& bot)
{
    std::vector<Quest const*> quests;
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        if (uint32 id = bot.GetQuestSlotQuestId(slot))
            if (Quest const* quest = sObjectMgr->GetQuestTemplate(id))
                quests.push_back(quest);
    Quest const* quest = Pick(quests);
    return quest ? QuestLink(*quest, bot.getLevel()) : std::string();
}

std::string DungeonFor(Player const& bot)
{
    std::vector<char const*> names;
    uint32 level = bot.getLevel();
    for (uint32 i = 0; i < sLFGDungeonStore.GetNumRows(); ++i)
    {
        LFGDungeonEntry const* dungeon = sLFGDungeonStore.LookupEntry(i);
        if (!dungeon || dungeon->TypeID != 1 || !dungeon->Name || !*dungeon->Name)
            continue;
        if (dungeon->MapID == 938 || dungeon->MapID == 939 || dungeon->MapID == 940) // 4.3 dungeons, not scripted
            continue;
        bool heroic = dungeon->DifficultyID == 1;
        if ((heroic && level < 85) || dungeon->DifficultyID > 1 || level < dungeon->MinLevel || level > dungeon->Maxlevel)
            continue;
        names.push_back(dungeon->Name);
    }
    if (names.empty())
        return {};
    return names[urand(0, uint32(names.size() - 1))];
}

std::string Gold(uint32 copper)
{
    uint32 gold = copper / 10000, silver = (copper / 100) % 100;
    if (gold >= 10)
        return std::to_string(gold) + "g";
    if (gold)
        return std::to_string(gold) + "g " + std::to_string(silver) + "s";
    return std::to_string(std::max<uint32>(silver, 1)) + "s";
}

// An item from the bags a bot could sell to a player, as a link and a price.
bool SellOffer(Player const& bot, Values& values)
{
    std::vector<Item*> items;
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (Item* item = bot.GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            items.push_back(item);
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        if (Bag const* container = bot.GetBagByPos(bag))
            for (uint32 slot = 0; slot < container->GetBagSize(); ++slot)
                if (Item* item = container->GetItemByPos(uint8(slot)))
                    items.push_back(item);
    std::vector<Item*> offers;
    for (Item* item : items)
    {
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto || item->IsSoulBound() || !proto->GetSellPrice() || proto->GetClass() == ITEM_CLASS_QUEST)
            continue;
        if (proto->GetQuality() >= ITEM_QUALITY_UNCOMMON || proto->GetClass() == ITEM_CLASS_TRADE_GOODS)
            offers.push_back(item);
    }
    Item* offer = Pick(offers);
    if (!offer)
        return false;
    ItemTemplate const* proto = offer->GetTemplate();
    std::string link = ItemLink(*proto);
    if (offer->GetCount() > 1)
        link += "x" + std::to_string(offer->GetCount());
    values["item_formatted_link"] = link;
    values["cost_gold"] = Gold(proto->GetSellPrice() * offer->GetCount() * urand(4, 9));
    return true;
}

char const* TradeCategory(uint32 level)
{
    static char const* const low[] = { "Kupfererz", "Leinenstoff", "Friedensblumen", "leichtes Leder", "Zinnerz",
        "Wollstoff", "Silberblatt", "Kupferbarren" };
    static char const* const mid[] = { "Eisenerz", "Seidenstoff", "Mithrilerz", "Magiestoff", "Königsblut",
        "Thoriumerz", "Runenstoff", "dickes Leder" };
    static char const* const high[] = { "Teufelseisenerz", "Netherstoff", "Kobalterz", "Froststoff", "Titanerz",
        "Boreanisches Leder", "Ewige Essenzen", "Lichblüten" };
    static char const* const cata[] = { "Obsidiumerz", "Glutseidenstoff", "Elementiumerz", "Wolkenleder",
        "Sturmwolkenstoff", "Herzblüte", "Peitschenwurz", "Pyriumbarren", "Hypnotischer Staub", "Chaoskristalle" };
    if (level >= 80)
        return cata[urand(0, 9)];
    if (level >= 58)
        return high[urand(0, 7)];
    if (level >= 30)
        return mid[urand(0, 7)];
    return low[urand(0, 7)];
}

// ---- Sending ----

Channel* JoinedChannel(Player const& bot, uint32 channelId)
{
    for (Channel* channel : bot.GetJoinedChannels())
        if (channel->GetChannelId() == channelId)
            return channel;
    return nullptr;
}

Channel* JoinedChannel(Player const& bot, Channel const* wanted)
{
    for (Channel* channel : bot.GetJoinedChannels())
        if (channel == wanted)
            return channel;
    return nullptr;
}

std::string KeyWhisper(ObjectGuid a, ObjectGuid b)
{
    uint64 lo = std::min(a.GetRawValue(), b.GetRawValue()), hi = std::max(a.GetRawValue(), b.GetRawValue());
    return "w" + std::to_string(lo) + ":" + std::to_string(hi);
}
std::string KeyParty(ObjectGuid group) { return "p" + std::to_string(group.GetRawValue()); }
std::string KeySay(ObjectGuid bot) { return "s" + std::to_string(bot.GetRawValue()); }
std::string KeyChannel(Channel const* channel) { return "c" + std::to_string(reinterpret_cast<uintptr_t>(channel)); }

void Remember(std::string const& key, ObjectGuid who, std::string const& name, std::string const& text, uint32 now)
{
    Conversation& conversation = History[key];
    conversation.Lines.push_back({ who, name, CutUtf8(text, 240) });
    while (conversation.Lines.size() > HistoryLines)
        conversation.Lines.pop_front();
    conversation.Touched = now;
}

void TextEmote(Player& bot, uint32 emoteId, ObjectGuid target)
{
    EmotesTextEntry const* entry = sEmotesTextStore.LookupEntry(emoteId);
    if (!entry || !bot.IsAlive())
        return;
    if (entry->EmoteID == EMOTE_STATE_DANCE)
        bot.SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_DANCE);
    else if (entry->EmoteID)
        bot.HandleEmoteCommand(entry->EmoteID);
    WorldPackets::Chat::STextEmote packet;
    packet.SourceGUID = bot.GetGUID();
    packet.EmoteID = int32(emoteId);
    packet.SoundIndex = -1;
    if (Unit* unit = target.IsEmpty() ? nullptr : ObjectAccessor::GetUnit(bot, target))
        packet.Target = unit->GetName();
    bot.SendMessageToSetInRange(packet.Write(), sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_TEXTEMOTE), true);
    EmoteCount.fetch_add(1, std::memory_order_relaxed);
}

void Schedule(Line line)
{
    if (Outgoing.size() >= MaxOutgoing)
        return;
    Outgoing.push_back(std::move(line));
}

// A line a bot says on its own, not as an answer.
bool MaySpeak(Player const& bot, uint32 now)
{
    auto it = LastSpoke.find(bot.GetGUID());
    return it == LastSpoke.end() || now - it->second >= OwnLineGapMs;
}

void SayToParty(Player& bot, std::string const& text, uint32 now, uint32 delayMin, uint32 delayMax)
{
    Group* group = bot.GetGroup();
    if (!group || text.empty())
        return;
    Line line;
    line.Bot = bot.GetGUID();
    line.To = Where::Party;
    line.GroupGuid = group->GetGUID();
    line.Key = KeyParty(group->GetGUID());
    line.Text = text;
    line.Created = now;
    line.SendAt = now + urand(delayMin, delayMax);
    LastSpoke[bot.GetGUID()] = now;
    Schedule(std::move(line));
}

void SayInChannel(Player& bot, uint32 channelId, std::string const& text, uint32 now)
{
    Channel* channel = JoinedChannel(bot, channelId);
    if (!channel || text.empty())
        return;
    Line line;
    line.Bot = bot.GetGUID();
    line.To = Where::Channel;
    line.Chan = channel;
    line.Key = KeyChannel(channel);
    line.Text = text;
    line.Created = now;
    line.SendAt = now + urand(500, 4000);
    LastSpoke[bot.GetGUID()] = now;
    Schedule(std::move(line));
}

bool HumanInZone(Player const& bot)
{
    for (auto const& [account, session] : sWorld->GetAllSessions())
        if (Player* player = session ? session->GetPlayer() : nullptr)
            if (!session->IsServerOrigin() && player->IsInWorld() && player->GetTeam() == bot.GetTeam() &&
                player->GetZoneId() == bot.GetZoneId())
                return true;
    return false;
}

// ---- Answers ----

std::vector<ChatTurn> TurnsFor(ObjectGuid bot, std::string const& key)
{
    std::vector<ChatTurn> turns;
    auto it = History.find(key);
    if (it == History.end())
        return turns;
    for (Said const& said : it->second.Lines)
    {
        bool own = said.Who == bot;
        ChatTurn turn { own, own ? said.Text : said.Name + ": " + said.Text };
        // The chat API wants alternating roles; merge neighbours from the same side.
        if (!turns.empty() && turns.back().FromBot == turn.FromBot)
            turns.back().Text += "\n" + turn.Text;
        else
            turns.push_back(std::move(turn));
    }
    if (!turns.empty() && turns.back().FromBot)
        turns.pop_back();
    return turns;
}

Lines LinesFor(Intent intent)
{
    switch (intent)
    {
        case Intent::Greeting: return Lines::ReplyGreeting;
        case Intent::Farewell: return Lines::ReplyFarewell;
        case Intent::Thanks: return Lines::ReplyThanks;
        case Intent::WhereWhat: return Lines::ReplyWhere;
        case Intent::LevelClass: return Lines::ReplyLevel;
        case Intent::LookingForGroup: return Lines::ReplyLfg;
        case Intent::Help: return Lines::ReplyHelp;
        case Intent::BotQuestion: return Lines::ReplyBot;
        case Intent::Insult: return Lines::ReplyInsult;
        case Intent::Praise: return Lines::ReplyPraise;
        case Intent::Laugh: return Lines::ReplyLaugh;
        case Intent::Yes: return Lines::ReplyYes;
        case Intent::No: return Lines::ReplyNo;
        case Intent::Question: return Lines::ReplyQuestion;
        default: return Lines::ReplyUnknown;
    }
}

std::string CannedReply(Player& bot, Player* partner, std::string const& partnerName, Understanding understanding)
{
    Values values = BasicValues(bot, partnerName);
    Lines lines = LinesFor(understanding.Kind);
    if (understanding.Kind == Intent::Invite)
    {
        Group* group = bot.GetGroup();
        if (group && partner && group == partner->GetGroup())
            lines = Lines::ReplyInviteAlready;
        else if (group)
            lines = Lines::ReplyInviteGrouped;
        else if (bot.GetMap() && bot.GetMap()->Instanceable())
            lines = Lines::ReplyInviteDungeon;
        else
            lines = Lines::ReplyInviteYes;
    }
    std::string text = Compose(lines, values);
    if (understanding.Greeted && understanding.Kind != Intent::Greeting && understanding.Kind != Intent::Farewell &&
        !text.empty() && Chance(50))
        text = std::string(Chance(50) ? "hi, " : "moin, ") + text;
    return text;
}

Facts FactsFor(Player& bot, std::string const& partner, Where where, bool partnerInGroup)
{
    Facts facts;
    facts.Name = bot.GetName();
    facts.Class = ClassName(bot.getClass());
    facts.Race = RaceName(bot.getRace());
    facts.Role = RoleName(bot);
    facts.Zone = ZoneName(bot);
    facts.Activity = ActivityForAi(Activity(bot));
    facts.Partner = partner;
    facts.Level = bot.getLevel();
    facts.Whisper = where == Where::Whisper;
    facts.InGroupWithPartner = partnerInGroup;
    return facts;
}

// Queues an answer: the own line right away, swapped for the AI's line if it comes in time.
void QueueAnswer(Player& bot, Where where, ObjectGuid target, Channel const* channel, std::string const& key,
    std::string canned, std::string const& partner, bool partnerInGroup, uint32 depth, uint32 now)
{
    Line line;
    line.Bot = bot.GetGUID();
    line.To = where;
    line.Target = target;
    line.Chan = channel;
    line.Key = key;
    line.Reply = true;
    line.Depth = depth;
    line.Created = now;
    if (Group* group = bot.GetGroup(); where == Where::Party && group)
        line.GroupGuid = group->GetGUID();
    line.Text = std::move(canned);
    line.SendAt = now + ReplyDelayMs(bot.GetGUID().GetCounter(), ++EventCounter, line.Text.size(), bot.IsInCombat());
    if (RandomPlayerbotChatAi::Enabled())
    {
        RandomPlayerbotChatAi::Request request;
        request.Id = ++NextAiId;
        request.BotName = bot.GetName();
        request.System = SystemPrompt(FactsFor(bot, partner, where, partnerInGroup), bot.GetGUID().GetCounter());
        request.Turns = TurnsFor(bot.GetGUID(), key);
        if (!request.Turns.empty() && RandomPlayerbotChatAi::Submit(std::move(request), now))
        {
            line.AiId = NextAiId;
            line.AiDeadline = now + RandomPlayerbotChatAi::TimeoutMs() + 500;
        }
    }
    if (line.Text.empty() && !line.AiId)
        return;
    LastSpoke[bot.GetGUID()] = now;
    Schedule(std::move(line));
}

void FollowUp(Player& speaker, Line const& said, uint32 now)
{
    // Bots answer each other only with the AI, only while a human writes, and only a few times.
    Group* group = speaker.GetGroup();
    if (!group || !RandomPlayerbotChatAi::Enabled() || !Chance(FollowUpChance(said.Depth)))
        return;
    auto wrote = HumanWrote.find(group->GetGUID());
    if (wrote == HumanWrote.end() || !HumanActive(now, wrote->second, true))
        return;
    std::vector<Player*> others;
    for (Player* bot : BotsIn(*group))
        if (bot != &speaker && bot->IsAlive())
            others.push_back(bot);
    if (Player* other = Pick(others))
        QueueAnswer(*other, Where::Party, ObjectGuid::Empty, nullptr, said.Key, std::string(), speaker.GetName(), true,
            said.Depth + 1, now);
}

void Send(Line& line, Player& bot, uint32 now)
{
    std::string const& text = line.Text;
    switch (line.To)
    {
        case Where::Whisper:
            if (Player* target = ObjectAccessor::FindConnectedPlayer(line.Target))
                bot.Whisper(text, LANG_UNIVERSAL, target);
            else
                return;
            break;
        case Where::Party:
        {
            Group* group = bot.GetGroup();
            if (!group || group->GetGUID() != line.GroupGuid)
                return;
            WorldPacket data;
            ChatHandler::BuildChatPacket(data, group->IsLeader(bot.GetGUID()) ? CHAT_MSG_PARTY_LEADER : CHAT_MSG_PARTY,
                LANG_UNIVERSAL, &bot, nullptr, text);
            group->BroadcastPacket(&data, false, group->GetMemberGroup(bot.GetGUID()));
            break;
        }
        case Where::Say:
            bot.Say(text, LANG_UNIVERSAL);
            break;
        case Where::Channel:
            if (Channel* channel = JoinedChannel(bot, line.Chan))
                channel->Say(bot.GetGUID(), text, LANG_UNIVERSAL);
            else
                return;
            break;
        case Where::Emote:
            TextEmote(bot, line.EmoteId, line.Target);
            if (line.EmoteId == TE_DANCE)
            {
                Line stop;
                stop.Bot = bot.GetGUID();
                stop.To = Where::EmoteStop;
                stop.SendAt = now + urand(6000, 12000);
                Schedule(std::move(stop));
            }
            return;
        case Where::EmoteStop:
            if (bot.GetUInt32Value(UNIT_NPC_EMOTESTATE) == EMOTE_STATE_DANCE)
                bot.SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_NONE);
            return;
    }
    (line.Reply ? ReplyCount : LineCount).fetch_add(1, std::memory_order_relaxed);
    if (!line.Key.empty())
        Remember(line.Key, bot.GetGUID(), bot.GetName(), text, now);
    if (line.To == Where::Party && line.Reply)
        FollowUp(bot, line, now);
}

// ---- Occasions ----

using BotMap = std::unordered_map<ObjectGuid, Player*>;

bool Settled(ObjectGuid bot, uint32 now)
{
    auto it = FirstSeen.find(bot);
    return it != FirstSeen.end() && now - it->second >= SettleMs;
}

void Congratulate(Group& group, Player& winner, Lines lines, uint32 percent, uint32 now)
{
    std::vector<Player*> others;
    for (Player* bot : BotsIn(group))
        if (bot != &winner && bot->IsAlive() && MaySpeak(*bot, now))
            others.push_back(bot);
    if (Player* other = Pick(others); other && Chance(percent))
        SayToParty(*other, Compose(lines, BasicValues(*other, winner.GetName())), now, 2500, 6000);
}

// A bot's own news: in a group with a human it tells the group, alone it sometimes tells the zone.
void Announce(Player& bot, Lines lines, Values const& values, uint32 groupChance, uint32 zoneChance, uint32 now,
    Lines congrats)
{
    if (!MaySpeak(bot, now))
        return;
    Group* group = bot.GetGroup();
    if (group && HumansIn(*group))
    {
        if (!Chance(groupChance))
            return;
        SayToParty(bot, Compose(lines, values), now, 1500, 4000);
        Congratulate(*group, bot, congrats, 40, now);
        return;
    }
    if (!group && Chance(zoneChance) && HumanInZone(bot))
        SayInChannel(bot, ChannelGeneral, Compose(lines, values), now);
}

void HandleChat(Event const& event, BotMap const& bots, uint32 now)
{
    Player* human = ObjectAccessor::FindConnectedPlayer(event.Actor);
    if (!human || !human->IsInWorld())
        return;
    Understanding understanding = Classify(event.Text);
    std::string const name = human->GetName();
    switch (event.What)
    {
        case Kind::Whisper:
        {
            auto it = bots.find(event.Target);
            if (it == bots.end())
                return;
            Player& bot = *it->second;
            std::string key = KeyWhisper(human->GetGUID(), bot.GetGUID());
            Remember(key, human->GetGUID(), name, event.Text, now);
            bool together = bot.GetGroup() && bot.GetGroup() == human->GetGroup();
            QueueAnswer(bot, Where::Whisper, human->GetGUID(), nullptr, key, CannedReply(bot, human, name, understanding),
                name, together, 0, now);
            return;
        }
        case Kind::Party:
        {
            Group* group = human->GetGroup();
            if (!group || group->GetGUID() != event.GroupGuid)
                return;
            HumanWrote[group->GetGUID()] = now;
            std::string key = KeyParty(group->GetGUID());
            Remember(key, human->GetGUID(), name, event.Text, now);
            std::vector<Player*> members = BotsIn(*group);
            std::vector<Player*> named;
            for (Player* bot : members)
                if (Mentions(event.Text, bot->GetName()))
                    named.push_back(bot);
            bool onlyBots = HumansIn(*group) == 1;
            if (named.empty())
            {
                Player* bot = Pick(members);
                if (bot && Chance(ReplyChance(Source::Party, false, understanding.Kind, onlyBots)))
                    named.push_back(bot);
            }
            // A greeting to the whole group may get a second answer.
            if (named.size() == 1 && understanding.Kind == Intent::Greeting && members.size() > 1 && Chance(35))
                for (Player* bot : members)
                    if (bot != named.front())
                    {
                        named.push_back(bot);
                        break;
                    }
            for (Player* bot : named)
                QueueAnswer(*bot, Where::Party, ObjectGuid::Empty, nullptr, key, CannedReply(*bot, human, name, understanding),
                    name, true, 0, now);
            return;
        }
        case Kind::Say:
        {
            std::vector<Player*> near, named;
            for (auto const& [guid, bot] : bots)
                if (bot->IsInWorld() && bot->GetMap() == human->GetMap() && bot->IsAlive() &&
                    bot->IsWithinDistInMap(human, float(SayRange)))
                {
                    near.push_back(bot);
                    if (Mentions(event.Text, bot->GetName()))
                        named.push_back(bot);
                }
            Player* bot = named.empty() ? nullptr : named.front();
            if (!bot && !near.empty())
            {
                // The closest bot feels addressed.
                bot = *std::min_element(near.begin(), near.end(), [&](Player* a, Player* b)
                    { return a->GetDistance(human) < b->GetDistance(human); });
                if (!Chance(ReplyChance(Source::Say, false, understanding.Kind, false)))
                    bot = nullptr;
            }
            if (!bot)
                return;
            std::string key = KeySay(bot->GetGUID());
            Remember(key, human->GetGUID(), name, event.Text, now);
            bool together = bot->GetGroup() && bot->GetGroup() == human->GetGroup();
            QueueAnswer(*bot, Where::Say, ObjectGuid::Empty, nullptr, key, CannedReply(*bot, human, name, understanding),
                name, together, 0, now);
            return;
        }
        case Kind::Channel:
        {
            std::string key = KeyChannel(event.Chan);
            Remember(key, human->GetGUID(), name, event.Text, now);
            std::vector<Player*> listeners, named;
            for (auto const& [guid, bot] : bots)
                if (bot->IsAlive() && JoinedChannel(*bot, event.Chan))
                {
                    listeners.push_back(bot);
                    if (Mentions(event.Text, bot->GetName()))
                        named.push_back(bot);
                }
            Player* bot = named.empty() ? Pick(listeners) : named.front();
            if (!bot || !Chance(ReplyChance(Source::Channel, !named.empty(), understanding.Kind, false)))
                return;
            QueueAnswer(*bot, Where::Channel, ObjectGuid::Empty, event.Chan, key, CannedReply(*bot, human, name, understanding),
                name, false, 0, now);
            return;
        }
        default:
            return;
    }
}

void HandleEmote(Event const& event, BotMap const& bots, uint32 now)
{
    Player* human = ObjectAccessor::FindConnectedPlayer(event.Actor);
    uint32 answer = AnswerEmote(event.Value);
    if (!human || !human->IsInWorld() || !answer)
        return;
    auto emoteBack = [&](Player& bot, ObjectGuid target, uint32 minMs, uint32 maxMs)
    {
        Line line;
        line.Bot = bot.GetGUID();
        line.To = Where::Emote;
        line.EmoteId = answer;
        line.Target = target;
        line.SendAt = now + urand(minMs, maxMs);
        Schedule(std::move(line));
    };
    if (auto it = bots.find(event.Target); it != bots.end())
    {
        Player& bot = *it->second;
        if (bot.IsAlive() && !bot.IsInCombat() && bot.GetMap() == human->GetMap() &&
            bot.IsWithinDistInMap(human, float(EmoteRange)))
            emoteBack(bot, human->GetGUID(), 1000, 3000);
        return;
    }
    if (!event.Target.IsEmpty())
        return;
    // An emote to nobody in particular: a bot or two nearby may join in.
    uint32 joined = 0;
    for (auto const& [guid, bot] : bots)
        if (joined < 2 && bot->IsAlive() && !bot->IsInCombat() && bot->GetMap() == human->GetMap() &&
            bot->IsWithinDistInMap(human, 15.0f) && Chance(25))
        {
            emoteBack(*bot, ObjectGuid::Empty, 1500, 5000);
            ++joined;
        }
}

void HandleOccasion(Event const& event, BotMap const& bots, uint32 now)
{
    Player* player = ObjectAccessor::FindConnectedPlayer(event.Actor);
    if (!player || !player->IsInWorld())
        return;
    auto it = bots.find(event.Actor);
    Player* bot = it == bots.end() ? nullptr : it->second;
    // A human's news: a bot of the group congratulates.
    if (!bot)
    {
        Group* group = player->GetGroup();
        if (!group || !IsHuman(*player))
            return;
        if (event.What == Kind::Level)
            Congratulate(*group, *player, Lines::Congrats, 70, now);
        else if (event.What == Kind::Kill)
            Congratulate(*group, *player, Lines::Congrats, 30, now);
        return;
    }
    if (!Settled(bot->GetGUID(), now))
        return;
    Values values = BasicValues(*bot, std::string());
    switch (event.What)
    {
        case Kind::Level:
        {
            if (event.Value != event.Value2 + 1)
                return; // set on login or lifted for the dungeon finder, not levelled
            bool max = event.Value >= 85;
            bool ten = event.Value % 10 == 0;
            Announce(*bot, max ? Lines::LevelUpMax : ten ? Lines::LevelUpTen : Lines::LevelUp, values,
                max ? 90 : ten ? 70 : 45, max ? 60 : ten ? 25 : 8, now, Lines::Congrats);
            return;
        }
        case Kind::Quest:
        {
            Quest const* quest = sObjectMgr->GetQuestTemplate(event.Value);
            if (!quest)
                return;
            values["quest_link"] = QuestLink(*quest, bot->getLevel());
            Announce(*bot, Lines::QuestTurnedIn, values, 20, 3, now, Lines::Congrats);
            return;
        }
        case Kind::Kill:
            values["victim_name"] = event.Text;
            Announce(*bot, event.Value == CREATURE_ELITE_WORLDBOSS ? Lines::KilledWorldBoss : Lines::KilledRare, values,
                60, 30, now, Lines::Congrats);
            return;
        case Kind::Loot:
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(event.Value);
            if (!proto)
                return;
            bool epic = proto->GetQuality() >= ITEM_QUALITY_EPIC;
            values["item_link"] = ItemLink(*proto);
            Announce(*bot, epic ? Lines::LootEpic : Lines::LootRare, values, epic ? 90 : 50, epic ? 40 : 10, now,
                Lines::NiceLoot);
            return;
        }
        default:
            return;
    }
}

// ---- Things bots do by themselves ----

void JoinChannels(std::vector<Player*> const& bots, uint32 now)
{
    for (Player* bot : bots)
    {
        if (!bot->IsInWorld() || bot->IsBeingTeleported() || !bot->GetJoinedChannels().empty())
            continue;
        uint32& at = JoinTry[bot->GetGUID()];
        if (at && !Reached(now, at))
            continue;
        at = now + ChannelJoinRetryMs;
        bot->UpdateLocalChannels(bot->GetZoneId());
    }
}

std::string ChatterLine(Player& bot, uint32 channelId)
{
    Values values = BasicValues(bot, std::string());
    uint32 roll = urand(0, 99);
    if (channelId == ChannelTrade)
    {
        if (roll < 50 && SellOffer(bot, values))
            return Compose(Lines::SuggestSell, values);
        values["category"] = TradeCategory(bot.getLevel());
        return Compose(Lines::SuggestTrade, values);
    }
    if (roll < 30)
    {
        values["instance_name"] = DungeonFor(bot);
        if (!values["instance_name"].empty())
            return Compose(Lines::SuggestInstance, values);
    }
    if (roll < 50)
    {
        values["quest_link"] = AnyQuestLink(bot);
        if (!values["quest_link"].empty())
            return Compose(Lines::SuggestQuest, values);
    }
    if (roll < 60)
    {
        values["category"] = TradeCategory(bot.getLevel());
        return Compose(Lines::SuggestTrade, values);
    }
    return Compose(Lines::SuggestSomething, values);
}

void Chatter(std::vector<Player*> const& bots, uint32 now)
{
    if (!Reached(now, NextChatterScan))
        return;
    NextChatterScan = now + 1000;
    std::unordered_set<uint64> places;
    for (auto const& [account, session] : sWorld->GetAllSessions())
    {
        Player* player = session ? session->GetPlayer() : nullptr;
        if (!player || session->IsServerOrigin() || !player->IsInWorld() || player->GetMap()->Instanceable())
            continue;
        for (uint32 channelId : { ChannelGeneral, ChannelTrade })
            if (JoinedChannel(*player, channelId) || channelId == ChannelGeneral)
                places.insert((uint64(player->GetTeam()) << 40) | (uint64(player->GetZoneId()) << 8) | channelId);
    }
    for (auto it = ChatterAt.begin(); it != ChatterAt.end();)
        it = places.count(it->first) ? std::next(it) : ChatterAt.erase(it);
    for (uint64 place : places)
    {
        uint32 team = uint32(place >> 40), zone = uint32((place >> 8) & 0xFFFFFFFF), channelId = uint32(place & 0xFF);
        auto found = ChatterAt.find(place);
        if (found == ChatterAt.end())
        {
            ChatterAt[place] = now + urand(Config.ChatterMinMs / 3, Config.ChatterMaxMs);
            continue;
        }
        if (!Reached(now, found->second))
            continue;
        found->second = now + urand(Config.ChatterMinMs, std::max(Config.ChatterMinMs, Config.ChatterMaxMs));
        std::vector<Player*> speakers;
        for (Player* bot : bots)
            if (bot->IsInWorld() && bot->GetTeam() == team && bot->GetZoneId() == zone && bot->IsAlive() &&
                !bot->IsInCombat() && !bot->GetMap()->Instanceable() && MaySpeak(*bot, now) && JoinedChannel(*bot, channelId))
                speakers.push_back(bot);
        if (Player* bot = Pick(speakers))
            SayInChannel(*bot, channelId, ChatterLine(*bot, channelId), now);
    }
}

void CombatLines(std::vector<Player*> const& bots, uint32 now)
{
    if (!Reached(now, NextCombatScan))
        return;
    NextCombatScan = now + 2000;
    for (Player* bot : bots)
    {
        Group* group = bot->GetGroup();
        if (!group || !bot->IsAlive() || !bot->IsInCombat() || !HumansIn(*group))
            continue;
        // Greeting is handled elsewhere; here only calls in a fight.
        if (auto it = LastCombatLine.find(bot->GetGUID()); it != LastCombatLine.end() && now - it->second < CombatLineGapMs)
            continue;
        float health = bot->GetHealthPct();
        int32 maxMana = bot->GetMaxPower(POWER_MANA);
        float mana = bot->GetPowerType() == POWER_MANA && maxMana > 0 ? 100.0f * bot->GetPower(POWER_MANA) / maxMana : 100.0f;
        Lines lines = Lines::Count;
        if (health < 20.0f && Chance(40))
            lines = Lines::CriticalHealth;
        else if (health < 35.0f && !PlayerbotRoles::IsHealer(*bot) && Chance(20))
            lines = Lines::LowHealth;
        else if (mana < 10.0f && Chance(30))
            lines = Lines::LowMana;
        else if (bot->getAttackers().size() >= 4 && Chance(25))
        {
            auto aoe = GroupAoe.find(group->GetGUID());
            if (aoe == GroupAoe.end() || now - aoe->second >= AoeGapMs)
            {
                GroupAoe[group->GetGUID()] = now;
                lines = Lines::Aoe;
            }
        }
        if (lines == Lines::Count)
            continue;
        LastCombatLine[bot->GetGUID()] = now;
        SayToParty(*bot, RandomLine(lines), now, 200, 1200);
    }
}

void GreetGroups(std::vector<Player*> const& bots, uint32 now)
{
    for (Player* bot : bots)
    {
        Group* group = bot->GetGroup();
        if (!group)
        {
            GroupOf.erase(bot->GetGUID());
            continue;
        }
        ObjectGuid& known = GroupOf[bot->GetGUID()];
        if (known == group->GetGUID())
            continue;
        known = group->GetGUID();
        if (!HumansIn(*group) || !Settled(bot->GetGUID(), now))
            continue;
        uint32& greeted = GroupGreeted[group->GetGUID()];
        // The first bot of a new group says hello often, a second one sometimes.
        uint32 chance = greeted ? 25 : 60;
        greeted += 1;
        if (greeted <= 2 && Chance(chance))
            SayToParty(*bot, RandomLine(Lines::PartyHello), now, 2500, 8000);
    }
}

void Prune(BotMap const& bots, uint32 now)
{
    if (!Reached(now, NextPrune))
        return;
    NextPrune = now + 60000;
    for (auto it = History.begin(); it != History.end();)
        it = now - it->second.Touched > HistoryKeepMs ? History.erase(it) : std::next(it);
    for (auto* table : { &FirstSeen, &LastSpoke, &LastCombatLine, &JoinTry })
        for (auto it = table->begin(); it != table->end();)
            it = bots.count(it->first) ? std::next(it) : table->erase(it);
    for (auto* table : { &GroupAoe, &HumanWrote })
        for (auto it = table->begin(); it != table->end();)
            it = now - it->second > HistoryKeepMs ? table->erase(it) : std::next(it);
    if (GroupGreeted.size() > 512)
        GroupGreeted.clear();
}

class RandomPlayerbotChatScript final : public PlayerScript
{
public:
    RandomPlayerbotChatScript() : PlayerScript("RandomPlayerbotChatScript") { }

    void OnTextEmote(Player* player, uint32 /*soundIndex*/, uint32 emote, ObjectGuid target) override
    {
        // The core passes the sound index first and the text emote second.
        if (!IsEnabled.load(std::memory_order_relaxed) || !player || !IsHuman(*player))
            return;
        Event event;
        event.What = Kind::Emote;
        event.Actor = player->GetGUID();
        event.Target = target;
        event.Value = emote;
        Queue(std::move(event));
    }

    void OnLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (!IsEnabled.load(std::memory_order_relaxed) || !player || !player->IsInWorld())
            return;
        Event event;
        event.What = Kind::Level;
        event.Actor = player->GetGUID();
        event.Value = player->getLevel();
        event.Value2 = oldLevel;
        Queue(std::move(event));
    }

    void OnQuestStatusChange(Player* player, uint32 questId) override
    {
        if (!IsEnabled.load(std::memory_order_relaxed) || !player || !IsRandomBot(*player) ||
            player->GetQuestStatus(questId) != QUEST_STATUS_REWARDED)
            return;
        Event event;
        event.What = Kind::Quest;
        event.Actor = player->GetGUID();
        event.Value = questId;
        Queue(std::move(event));
    }

    void OnCreatureKill(Player* killer, Creature* killed) override
    {
        if (!IsEnabled.load(std::memory_order_relaxed) || !killer || !killed)
            return;
        CreatureTemplate const* info = killed->GetCreatureTemplate();
        if (!info || (info->rank != CREATURE_ELITE_RARE && info->rank != CREATURE_ELITE_RAREELITE &&
            info->rank != CREATURE_ELITE_WORLDBOSS))
            return;
        Event event;
        event.What = Kind::Kill;
        event.Actor = killer->GetGUID();
        event.Value = info->rank;
        event.Text = killed->GetName();
        if (CreatureLocale const* locale = sObjectMgr->GetCreatureLocale(info->Entry))
            ObjectMgr::GetLocaleString(locale->Name, LOCALE_deDE, event.Text);
        Queue(std::move(event));
    }
};
}

void RandomPlayerbotChat::Configure(Settings const& settings)
{
    Config = settings;
    if (Config.ChatterMaxMs < Config.ChatterMinMs)
        Config.ChatterMaxMs = Config.ChatterMinMs;
    Config.ChatterMinMs = std::max<uint32>(Config.ChatterMinMs, 10000);
    IsEnabled.store(settings.Enabled, std::memory_order_relaxed);
}

bool RandomPlayerbotChat::Enabled() { return IsEnabled.load(std::memory_order_relaxed); }

void RandomPlayerbotChat::Update(std::vector<Player*> const& bots)
{
    if (!Enabled())
        return;
    uint32 now = getMSTime();
    BotMap byGuid;
    for (Player* bot : bots)
    {
        byGuid[bot->GetGUID()] = bot;
        FirstSeen.emplace(bot->GetGUID(), now);
    }

    std::vector<Event> events;
    {
        std::lock_guard lock(EventLock);
        events.swap(Events);
    }
    for (Event const& event : events)
    {
        switch (event.What)
        {
            case Kind::Whisper: case Kind::Party: case Kind::Say: case Kind::Channel:
                HandleChat(event, byGuid, now);
                break;
            case Kind::Emote:
                HandleEmote(event, byGuid, now);
                break;
            default:
                HandleOccasion(event, byGuid, now);
                break;
        }
    }

    for (RandomPlayerbotChatAi::Answer& answer : RandomPlayerbotChatAi::Collect())
        for (Line& line : Outgoing)
            if (line.AiId == answer.Id)
            {
                line.AiId = 0;
                if (answer.Ok)
                {
                    line.Text = std::move(answer.Text);
                    AiUsed.fetch_add(1, std::memory_order_relaxed);
                    // Typing time for the longer line, counted from when the human wrote.
                    uint32 typed = line.Created + ReplyDelayMs(line.Bot.GetCounter(), ++EventCounter, line.Text.size(), false);
                    line.SendAt = Reached(now, typed) ? now + urand(300, 1200) : typed;
                }
                break;
            }

    std::vector<Line> due;
    for (auto it = Outgoing.begin(); it != Outgoing.end();)
    {
        bool waitingForAi = it->AiId && !Reached(now, it->AiDeadline);
        if (Reached(now, it->SendAt) && !waitingForAi)
        {
            due.push_back(std::move(*it));
            it = Outgoing.erase(it);
        }
        else
            ++it;
    }
    for (Line& line : due)
    {
        auto bot = byGuid.find(line.Bot);
        if (bot == byGuid.end() || !bot->second->IsInWorld() || (line.Text.empty() && line.To != Where::Emote &&
            line.To != Where::EmoteStop))
            continue;
        Send(line, *bot->second, now);
    }

    JoinChannels(bots, now);
    Chatter(bots, now);
    CombatLines(bots, now);
    GreetGroups(bots, now);
    Prune(byGuid, now);
}

std::string RandomPlayerbotChat::Describe()
{
    if (!Enabled())
        return "chat off";
    return "chat lines " + std::to_string(LineCount.load()) + ", chat replies " + std::to_string(ReplyCount.load()) +
        ", emotes " + std::to_string(EmoteCount.load()) + ", " + RandomPlayerbotChatAi::Describe() +
        ", ai lines said " + std::to_string(AiUsed.load());
}

void RandomPlayerbotChat::HumanWhisper(Player& human, Player& bot, std::string const& text)
{
    if (!Enabled() || !IsHuman(human) || !IsRandomBot(bot) || text.empty())
        return;
    Queue({ Kind::Whisper, human.GetGUID(), bot.GetGUID(), ObjectGuid::Empty, nullptr, text });
}

void RandomPlayerbotChat::HumanParty(Player& human, Group& group, std::string const& text)
{
    if (!Enabled() || !IsHuman(human) || text.empty())
        return;
    Queue({ Kind::Party, human.GetGUID(), ObjectGuid::Empty, group.GetGUID(), nullptr, text });
}

void RandomPlayerbotChat::HumanSay(Player& human, std::string const& text)
{
    if (!Enabled() || !IsHuman(human) || text.empty())
        return;
    Queue({ Kind::Say, human.GetGUID(), ObjectGuid::Empty, ObjectGuid::Empty, nullptr, text });
}

void RandomPlayerbotChat::HumanChannel(Player& human, Channel& channel, std::string const& text)
{
    uint32 id = channel.GetChannelId();
    if (!Enabled() || !IsHuman(human) || text.empty() ||
        (id != ChannelGeneral && id != ChannelTrade && id != ChannelLookingForGroup))
        return;
    Queue({ Kind::Channel, human.GetGUID(), ObjectGuid::Empty, ObjectGuid::Empty, &channel, text });
}

void RandomPlayerbotChat::Loot(Player& bot, uint32_t itemId)
{
    if (!Enabled() || !IsRandomBot(bot))
        return;
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto || proto->GetQuality() < ITEM_QUALITY_RARE)
        return;
    Event event;
    event.What = Kind::Loot;
    event.Actor = bot.GetGUID();
    event.Value = itemId;
    Queue(std::move(event));
}

void AddSC_random_playerbot_chat()
{
    new RandomPlayerbotChatScript();
}
