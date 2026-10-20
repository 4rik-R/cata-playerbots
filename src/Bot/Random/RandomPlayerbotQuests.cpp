/*
 * Autonomous questing for random-population bots, adapted from mod-playerbots
 * NewRpgBaseAction (SearchQuestGiverAndAcceptOrReward, IsQuestWorthDoing,
 * IsQuestCapableDoing, OrganizeQuestLog) and GrindTargetValue::needForQuest at
 * d750b4b. See AUTHORS.md and PORTING.md. Released under GNU GPL v2 or any later version.
 */
#include "RandomPlayerbotQuests.h"
#include "RandomPlayerbotGrind.h"
#include "RandomPlayerbotGrindTarget.h"
#include "RandomPlayerbotLevels.h"
#include "RandomPlayerbotProgression.h"
#include "RandomPlayerbotQuestPolicy.h"
#include "CellImpl.h"
#include "Creature.h"
#include "GameObject.h"
#include "GridDefines.h"
#include "CreatureData.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "QuestPackets.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Random.h"
#include "Timer.h"
#include "WorldSession.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <list>
#include <set>
#include <sstream>
#include <vector>

using namespace RandomPlayerbotQuestPolicy;

namespace
{
bool QuestsEnabled = false;
bool IndexReady = false;

// Continents and Cataclysm outdoor maps; phased start maps are left to M3b's capital move.
constexpr uint32 OpenWorldMaps[] = { 0, 1, 530, 571, 646, 732 };
constexpr uint32 AllianceFactionTemplate = 1;
constexpr uint32 HordeFactionTemplate = 2;
constexpr uint32 MaxGiverAttempts = 40;

struct Point
{
    uint16 Map;
    uint16 Zone;
    float X, Y, Z, O;
};

struct Giver
{
    Point Pos;
    uint8 Teams;
    std::vector<uint32> Quests;
    uint32 HubSize = 1; // givers within HubRadius, itself included
};
// Quest givers this close together count as one quest hub.
constexpr float HubRadius = 60.0f;
// A relocated bot compares this many fitting givers and takes the biggest hub.
constexpr uint32 HubCandidates = 6;

std::unordered_set<uint32> BotQuests;
// Creature entry -> kill objectives it counts for; kill objective -> entries that count.
std::unordered_map<uint32, std::vector<uint32>> CreditTargets, Creditors;
// Creature entry -> needed items it drops; item -> entries that drop it.
std::unordered_map<uint32, std::vector<uint32>> DroppedItems, Droppers;
std::unordered_map<uint32, std::vector<Point>> HuntSpawns;
// Quest objects: goober entries that are objectives, chests with needed items, their spawns.
std::unordered_set<uint32> ObjectiveObjects;
std::unordered_map<uint32, std::vector<uint32>> ObjectItems, ItemObjects;
std::unordered_map<uint32, std::vector<Point>> ObjectSpawns;
// Spawns of the creatures that take bot quests back.
std::unordered_map<uint32, std::vector<Point>> EnderSpawns;
// Lock type -> spells that open it without a reagent or item.
std::unordered_map<uint32, std::vector<uint32>> OpenSpells;
std::vector<Giver> Givers;
std::vector<std::vector<uint32>> GiversByLevel; // level -> index into Givers
uint32 VerdictCounts[5] = {};
std::atomic<uint32> ActivityCounts[4] = {};

void SetActivity(RandomPlayerbotQuests::BotState& state, RandomPlayerbotQuests::Activity activity)
{
    if (state.Doing == activity)
        return;
    if (state.Doing != RandomPlayerbotQuests::Activity::None)
        --ActivityCounts[uint8(state.Doing)];
    if (activity != RandomPlayerbotQuests::Activity::None)
        ++ActivityCounts[uint8(activity)];
    state.Doing = activity;
}
uint32 DeliveryCount = 0;
std::atomic<uint32> AcceptedCount { 0 }, CompletedCount { 0 }, AbandonedCount { 0 }, ObjectCount { 0 };

bool OpenWorld(uint32 map)
{
    return std::find(std::begin(OpenWorldMaps), std::end(OpenWorldMaps), map) != std::end(OpenWorldMaps);
}

Shape ShapeOf(Quest const& quest)
{
    Shape shape;
    std::string title = quest.GetTitle();
    shape.Deprecated = quest.HasFlag(QUEST_FLAGS_DEPRECATED) || title.find("DEPRECATED") != std::string::npos ||
        title.find("UNUSED") != std::string::npos || !quest.GetMinLevel() || quest.GetMinLevel() > RandomPlayerbotLevels::MaxLevel;
    shape.Repeating = quest.IsRepeatable() || quest.IsDailyOrWeekly() || quest.IsMonthly() || quest.IsSeasonal();
    shape.Timed = quest.GetTimeAllowed() || quest.HasSpecialFlag(QUEST_SPECIAL_FLAGS_TIMED);
    shape.Escort = quest.GetQuestInfoId() == EscortQuestInfo;
    shape.Group = quest.GetQuestInfoId() != 0 || quest.GetSuggestedPlayers() > 1;
    shape.Scripted = quest.HasFlag(QUEST_FLAGS_COMPLETION_EVENT | QUEST_FLAGS_COMPLETION_AREA_TRIGGER |
        QUEST_FLAGS_TRACKING_EVENT | QUEST_FLAGS_PLAYER_CAST_COMPLETE | QUEST_FLAGS_FLAGS_PVP) ||
        quest.HasSpecialFlag(QUEST_SPECIAL_FLAGS_CAST) || quest.IsStartAtAreaTrigger() || quest.IsTurnIn();
    shape.NeedsSpell = quest.GetRequiredSpell() != 0;
    shape.Currency = quest.GetReqCurrencyCount() != 0;
    shape.PlayerKills = quest.GetPlayersSlain() != 0;
    for (uint32 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
    {
        if (quest.RequiredNpcOrGo[i] > 0)
            ++shape.KillObjectives;
        else if (quest.RequiredNpcOrGo[i] < 0)
            ++shape.GameObjectObjectives;
    }
    for (uint32 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
    {
        uint32 item = quest.RequiredItemId[i];
        if (!item || item == quest.GetSrcItemId())
            continue;
        if (std::find(std::begin(quest.ItemDrop), std::end(quest.ItemDrop), item) != std::end(quest.ItemDrop))
            continue;
        ++shape.ItemObjectives;
    }
    return shape;
}

template<class Enders>
bool DeliveryNearby(std::vector<uint32> const& starters, Enders const& enders,
    std::unordered_map<uint32, std::vector<Point>> const& spawns)
{
    constexpr size_t MaxSpawnsChecked = 20;
    for (uint32 starter : starters)
    {
        auto from = spawns.find(starter);
        if (from == spawns.end())
            continue;
        for (uint32 ender : enders)
        {
            auto to = spawns.find(ender);
            if (to == spawns.end())
                continue;
            for (size_t i = 0; i < std::min(from->second.size(), MaxSpawnsChecked); ++i)
                for (size_t j = 0; j < std::min(to->second.size(), MaxSpawnsChecked); ++j)
                {
                    Point const& a = from->second[i];
                    Point const& b = to->second[j];
                    if (a.Map == b.Map && a.Zone && a.Zone == b.Zone &&
                        std::hypot(a.X - b.X, a.Y - b.Y) <= TravelRange)
                        return true;
                }
        }
    }
    return false;
}

uint8 TeamOf(Player const& bot)
{
    return bot.GetTeamId() == TEAM_ALLIANCE ? RandomPlayerbotLevels::Alliance : RandomPlayerbotLevels::Horde;
}

// Givers and quest mobs only count inside the bot's levelling zones, so quest
// chains do not pull bots into left-out areas such as the Ruins of Gilneas.
bool ZoneFitsBot(uint32 zoneId, Player const& bot)
{
    return RandomPlayerbotLevels::ZoneFits(zoneId, bot.getLevel(), TeamOf(bot));
}

uint8 TeamsOf(CreatureTemplate const& info)
{
    FactionTemplateEntry const* alliance = sFactionTemplateStore.LookupEntry(AllianceFactionTemplate);
    FactionTemplateEntry const* horde = sFactionTemplateStore.LookupEntry(HordeFactionTemplate);
    FactionTemplateEntry const* faction = sFactionTemplateStore.LookupEntry(info.faction);
    if (!alliance || !horde || !faction)
        return 0;
    return (faction->IsHostileTo(alliance) ? 0 : RandomPlayerbotLevels::Alliance) |
        (faction->IsHostileTo(horde) ? 0 : RandomPlayerbotLevels::Horde);
}

uint32 QuestLevelFor(Quest const& quest, uint32 level)
{
    return quest.GetQuestLevel() > 0 ? uint32(quest.GetQuestLevel()) : level;
}

// Objective counts of a quest in the log, kill objectives first, then items.
std::vector<uint32> Counts(Player const& bot, Quest const& quest)
{
    std::vector<uint32> counts;
    for (uint32 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        if (quest.RequiredNpcOrGo[i])
            counts.push_back(bot.GetReqKillOrCastCurrentCount(quest.GetQuestId(), quest.RequiredNpcOrGo[i]));
    for (uint32 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        if (quest.RequiredItemId[i])
            counts.push_back(bot.GetItemCount(quest.RequiredItemId[i], true));
    return counts;
}

template<class Visit>
void ForEachIncomplete(Player const& bot, Visit visit)
{
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 id = bot.GetQuestSlotQuestId(slot);
        if (!id || bot.GetQuestStatus(id) != QUEST_STATUS_INCOMPLETE)
            continue;
        if (Quest const* quest = sObjectMgr->GetQuestTemplate(id))
            visit(*quest);
    }
}

// objective: a creature entry, or minus a gameobject entry.
bool NeedsObjective(Player const& bot, int32 objective)
{
    bool needed = false;
    ForEachIncomplete(bot, [&](Quest const& quest)
    {
        for (uint32 i = 0; i < QUEST_OBJECTIVES_COUNT && !needed; ++i)
            if (quest.RequiredNpcOrGo[i] == objective &&
                bot.GetReqKillOrCastCurrentCount(quest.GetQuestId(), objective) < quest.RequiredNpcOrGoCount[i])
                needed = true;
    });
    return needed;
}

bool NeedsKill(Player const& bot, uint32 target)
{
    return NeedsObjective(bot, int32(target));
}

bool NeedsItem(Player const& bot, uint32 item)
{
    bool needed = false;
    ForEachIncomplete(bot, [&](Quest const& quest)
    {
        for (uint32 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT && !needed; ++i)
            if (quest.RequiredItemId[i] == item && bot.GetItemCount(item, true) < quest.RequiredItemCount[i])
                needed = true;
    });
    return needed;
}

bool NeedsCreature(Player const& bot, uint32 entry)
{
    if (auto found = CreditTargets.find(entry); found != CreditTargets.end())
        for (uint32 target : found->second)
            if (NeedsKill(bot, target))
                return true;
    if (auto found = DroppedItems.find(entry); found != DroppedItems.end())
        for (uint32 item : found->second)
            if (NeedsItem(bot, item))
                return true;
    return false;
}

bool NeedsObject(Player const& bot, uint32 entry)
{
    if (ObjectiveObjects.count(entry) && NeedsObjective(bot, -int32(entry)))
        return true;
    if (auto found = ObjectItems.find(entry); found != ObjectItems.end())
        for (uint32 item : found->second)
            if (NeedsItem(bot, item))
                return true;
    return false;
}

bool WouldTake(Player& bot, RandomPlayerbotQuests::BotState const* state, Quest const& quest)
{
    uint32 id = quest.GetQuestId();
    return BotQuests.count(id) && (!state || !state->Shelved.count(id)) &&
        bot.GetQuestStatus(id) == QUEST_STATUS_NONE &&
        LevelFits(bot.getLevel(), QuestLevelFor(quest, bot.getLevel()), quest.GetMinLevel()) &&
        bot.FindQuestSlot(0) < MAX_QUEST_LOG_SIZE && bot.CanTakeQuest(&quest, false) && bot.CanAddQuest(&quest, false);
}

bool HasWork(Player& bot, RandomPlayerbotQuests::BotState const& state, Creature& giver)
{
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        if (uint32 id = bot.GetQuestSlotQuestId(slot))
            if (bot.GetQuestStatus(id) == QUEST_STATUS_COMPLETE && giver.hasInvolvedQuest(id))
                return true;
    for (uint32 id : sObjectMgr->GetCreatureQuestRelations(giver.GetEntry()))
        if (Quest const* quest = sObjectMgr->GetQuestTemplate(id))
            if (WouldTake(bot, &state, *quest))
                return true;
    return false;
}

std::vector<RewardOption> RewardOptions(Player& bot, Quest const& quest)
{
    std::vector<RewardOption> options;
    for (uint8 slot = 0; slot < QUEST_REWARD_CHOICES_COUNT; ++slot)
    {
        ItemTemplate const* proto = quest.RewardChoiceItemId[slot] ?
            sObjectMgr->GetItemTemplate(quest.RewardChoiceItemId[slot]) : nullptr;
        if (!proto)
            continue;
        RewardOption option;
        option.Slot = slot;
        option.ItemLevel = proto->GetBaseItemLevel();
        option.SellPrice = proto->GetSellPrice();
        if (proto->GetInventoryType() != INVTYPE_NON_EQUIP && bot.CanUseItem(proto) == EQUIP_ERR_OK)
        {
            uint8 equipSlot = bot.FindEquipSlot(proto, NULL_SLOT, true);
            if (equipSlot != NULL_SLOT)
            {
                option.Usable = true;
                if (Item* worn = bot.GetItemByPos(INVENTORY_SLOT_BAG_0, equipSlot))
                    option.EquippedLevel = worn->GetTemplate()->GetBaseItemLevel();
            }
        }
        options.push_back(option);
    }
    return options;
}

void TurnIn(WorldSession& session, Player& bot, Creature& giver)
{
    std::vector<uint32> complete;
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        if (uint32 id = bot.GetQuestSlotQuestId(slot))
            if (bot.GetQuestStatus(id) == QUEST_STATUS_COMPLETE && giver.hasInvolvedQuest(id))
                complete.push_back(id);
    for (uint32 id : complete)
    {
        Quest const* quest = sObjectMgr->GetQuestTemplate(id);
        if (!quest || session.GetPlayer() != &bot)
            break;
        uint8 choice = ChooseReward(RewardOptions(bot, *quest));
        if (!bot.CanRewardQuest(quest, false) || !bot.CanRewardQuest(quest, choice, false))
            continue;
        WorldPackets::Quest::QuestGiverChooseReward packet(WorldPacket(CMSG_QUEST_GIVER_CHOOSE_REWARD, 0));
        packet.QuestGiverGUID = giver.GetGUID();
        packet.QuestID = int32(id);
        packet.ItemChoiceID = choice;
        session.HandleQuestgiverChooseRewardOpcode(packet);
        if (bot.GetQuestRewardStatus(id))
        {
            ++CompletedCount;
            TC_LOG_DEBUG("module.playerbots", "PB-QUEST: %s turned in quest %u", bot.GetName().c_str(), id);
        }
    }
}

void Accept(WorldSession& session, Player& bot, RandomPlayerbotQuests::BotState const& state, Creature& giver)
{
    std::vector<uint32> offered;
    for (uint32 id : sObjectMgr->GetCreatureQuestRelations(giver.GetEntry()))
        offered.push_back(id);
    for (uint32 id : offered)
    {
        Quest const* quest = sObjectMgr->GetQuestTemplate(id);
        if (!quest || session.GetPlayer() != &bot || !WouldTake(bot, &state, *quest))
            continue;
        WorldPackets::Quest::QuestGiverAcceptQuest packet(WorldPacket(CMSG_QUEST_GIVER_ACCEPT_QUEST, 0));
        packet.QuestGiverGUID = giver.GetGUID();
        packet.QuestID = id;
        packet.StartCheat = 0;
        session.HandleQuestgiverAcceptQuestOpcode(packet);
        if (bot.FindQuestSlot(id) < MAX_QUEST_LOG_SIZE)
        {
            ++AcceptedCount;
            TC_LOG_DEBUG("module.playerbots", "PB-QUEST: %s accepted quest %u", bot.GetName().c_str(), id);
        }
    }
}

void Abandon(WorldSession& session, Player& bot, uint32 id, char const* reason)
{
    uint16 slot = bot.FindQuestSlot(id);
    if (slot >= MAX_QUEST_LOG_SIZE)
        return;
    WorldPackets::Quest::QuestLogRemoveQuest packet(WorldPacket(CMSG_QUEST_LOG_REMOVE_QUEST, 0));
    packet.Entry = uint8(slot);
    session.HandleQuestLogRemoveQuest(packet);
    if (bot.FindQuestSlot(id) >= MAX_QUEST_LOG_SIZE)
    {
        ++AbandonedCount;
        TC_LOG_DEBUG("module.playerbots", "PB-QUEST: %s abandoned %s quest %u", bot.GetName().c_str(), reason, id);
    }
}

// Abandons quests without progress for QuestStallMs; true when any quest is in the log.
// Progress restarts the spot timer; a quest taken, finished or dropped picks a new spot.
bool TrackProgress(WorldSession& session, Player& bot, RandomPlayerbotQuests::BotState& state, uint32 now)
{
    std::unordered_set<uint32> seen, incomplete;
    std::vector<uint32> stalled;
    bool progress = false, changed = false;
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 id = bot.GetQuestSlotQuestId(slot);
        Quest const* quest = id ? sObjectMgr->GetQuestTemplate(id) : nullptr;
        if (!quest)
            continue;
        seen.insert(id);
        if (bot.GetQuestStatus(id) != QUEST_STATUS_INCOMPLETE)
            continue;
        incomplete.insert(id);
        uint32 signature = ProgressSignature(Counts(bot, *quest));
        auto [it, inserted] = state.Quests.try_emplace(id, RandomPlayerbotQuests::BotState::Progress{ signature, now });
        if (inserted)
            changed = true;
        else if (it->second.Signature != signature)
        {
            it->second = { signature, now };
            progress = true;
        }
        else if (Stalled(now, it->second.SinceMs))
            stalled.push_back(id);
    }
    for (auto it = state.Quests.begin(); it != state.Quests.end();)
    {
        if (incomplete.count(it->first))
            ++it;
        else
        {
            it = state.Quests.erase(it);
            changed = true;
        }
    }
    for (uint32 id : stalled)
    {
        state.Shelved.insert(id);
        state.Quests.erase(id);
        Abandon(session, bot, id, "stalled");
        changed = true;
    }
    if (progress || changed)
    {
        state.LastProgressMs = now;
        state.TriedSpots.clear();
    }
    if (changed)
        state.ClearSpot();
    return !seen.empty();
}

Creature* FindGiver(Player& bot, RandomPlayerbotQuests::BotState& state, uint32 now)
{
    std::list<Unit*> units;
    Trinity::AnyUnitInObjectRangeCheck check(&bot, GiverRange);
    Trinity::UnitListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(&bot, units, check);
    Cell::VisitAllObjects(&bot, searcher, GiverRange);
    for (auto it = state.SkippedGivers.begin(); it != state.SkippedGivers.end();)
        it = int32(it->second - now) <= 0 ? state.SkippedGivers.erase(it) : std::next(it);
    Creature* best = nullptr;
    float bestDistance = GiverRange + 1.0f;
    for (Unit* unit : units)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsQuestGiver() || creature->IsInCombat() || bot.IsHostileTo(creature) ||
            state.SkippedGivers.count(creature->GetGUID().GetRawValue()) || !ZoneFitsBot(creature->GetZoneId(), bot))
            continue;
        float distance = bot.GetDistance(creature);
        if (distance < bestDistance && HasWork(bot, state, *creature))
        {
            best = creature;
            bestDistance = distance;
        }
    }
    return best;
}

struct Needs
{
    std::unordered_set<uint32> Creatures, Objects;
    // Quest id -> quest_poi objective indexes still open.
    std::vector<std::pair<uint32, std::vector<int32>>> Markers;
    std::vector<uint32> Complete;
};

Needs CollectNeeds(Player const& bot)
{
    Needs needs;
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 id = bot.GetQuestSlotQuestId(slot);
        if (id && BotQuests.count(id) && bot.GetQuestStatus(id) == QUEST_STATUS_COMPLETE)
            needs.Complete.push_back(id);
    }
    ForEachIncomplete(bot, [&](Quest const& quest)
    {
        std::vector<int32> open;
        for (uint32 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        {
            int32 objective = quest.RequiredNpcOrGo[i];
            if (!objective || bot.GetReqKillOrCastCurrentCount(quest.GetQuestId(), objective) >= quest.RequiredNpcOrGoCount[i])
                continue;
            open.push_back(int32(i));
            if (objective > 0)
            {
                if (auto found = Creditors.find(uint32(objective)); found != Creditors.end())
                    needs.Creatures.insert(found->second.begin(), found->second.end());
            }
            else if (ObjectiveObjects.count(uint32(-objective)))
                needs.Objects.insert(uint32(-objective));
        }
        for (uint32 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        {
            uint32 item = quest.RequiredItemId[i];
            if (!item || bot.GetItemCount(item, true) >= quest.RequiredItemCount[i])
                continue;
            open.push_back(ItemObjectiveBase + int32(i));
            if (auto found = Droppers.find(item); found != Droppers.end())
                needs.Creatures.insert(found->second.begin(), found->second.end());
            if (auto found = ItemObjects.find(item); found != ItemObjects.end())
                needs.Objects.insert(found->second.begin(), found->second.end());
        }
        if (!open.empty())
            needs.Markers.emplace_back(quest.GetQuestId(), std::move(open));
    });
    return needs;
}

bool Tried(RandomPlayerbotQuests::BotState const& state, float x, float y)
{
    for (Position const& spot : state.TriedSpots)
        if (spot.GetExactDist2d(x, y) < TriedSpotRadius)
            return true;
    return false;
}

// Nearest spawn of the given entries on the bot's map, in a fitting zone and range.
Point const* NearestSpawn(Player const& bot, RandomPlayerbotQuests::BotState const& state,
    std::unordered_map<uint32, std::vector<Point>> const& spawns, std::unordered_set<uint32> const& entries,
    float minRange, float maxRange)
{
    Point const* best = nullptr;
    float bestDistance = maxRange * maxRange;
    for (uint32 entry : entries)
    {
        auto found = spawns.find(entry);
        if (found == spawns.end())
            continue;
        for (Point const& point : found->second)
        {
            if (point.Map != bot.GetMapId() || !ZoneFitsBot(point.Zone, bot) || Tried(state, point.X, point.Y))
                continue;
            float dx = point.X - bot.GetPositionX(), dy = point.Y - bot.GetPositionY();
            float distance = dx * dx + dy * dy;
            if (distance < bestDistance && distance >= minRange * minRange)
            {
                best = &point;
                bestDistance = distance;
            }
        }
    }
    return best;
}

// A random point in the quest-map marker of an open objective, on the ground of the bot's map.
std::optional<Position> MarkerSpot(Player& bot, RandomPlayerbotQuests::BotState const& state, Needs const& needs)
{
    std::vector<size_t> order(needs.Markers.size());
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::shuffle(order.begin(), order.end(), RandomEngine::Instance());
    for (size_t index : order)
    {
        auto const& [questId, objectives] = needs.Markers[index];
        QuestPOIData const* poi = sObjectMgr->GetQuestPOIData(int32(questId));
        if (!poi)
            continue;
        for (QuestPOIBlobData const& blob : poi->Blobs)
        {
            if (blob.MapID != int32(bot.GetMapId()) || blob.Points.empty() ||
                std::find(objectives.begin(), objectives.end(), blob.ObjectiveIndex) == objectives.end())
                continue;
            std::vector<MapPoint> polygon;
            for (QuestPOIBlobPoint const& point : blob.Points)
                polygon.push_back({ float(point.X), float(point.Y) });
            MapPoint point = RandomPointIn(polygon, [] { return rand_norm(); });
            if (bot.GetExactDist2d(point.X, point.Y) > TravelRange || Tried(state, point.X, point.Y))
                continue;
            Map* map = bot.GetMap();
            float ground = map->GetHeight(bot.GetPhaseShift(), point.X, point.Y, MAX_HEIGHT, false);
            if (ground <= INVALID_HEIGHT)
                continue;
            float z = map->GetHeight(bot.GetPhaseShift(), point.X, point.Y, ground + 5.0f);
            if (z <= INVALID_HEIGHT)
                z = ground;
            if (!ZoneFitsBot(map->GetZoneId(bot.GetPhaseShift(), point.X, point.Y, z), bot))
                continue;
            return Position(point.X, point.Y, z);
        }
    }
    return std::nullopt;
}

// Where to work next: nearby mobs and objects, then turn-ins, then markers, then further spawns.
std::optional<Position> NextSpot(Player& bot, RandomPlayerbotQuests::BotState const& state)
{
    Needs needs = CollectNeeds(bot);
    auto spot = [&](Point const* a, Point const* b) -> Point const*
    {
        if (!a || !b)
            return a ? a : b;
        return bot.GetExactDist2d(a->X, a->Y) <= bot.GetExactDist2d(b->X, b->Y) ? a : b;
    };
    Point const* nearby = spot(NearestSpawn(bot, state, HuntSpawns, needs.Creatures, 0.0f, SpawnSearchRange),
        NearestSpawn(bot, state, ObjectSpawns, needs.Objects, 0.0f, SpawnSearchRange));
    if (nearby)
        return Position(nearby->X, nearby->Y, nearby->Z);
    if (!needs.Complete.empty())
    {
        std::unordered_set<uint32> enders;
        for (uint32 id : needs.Complete)
            for (uint32 entry : sObjectMgr->GetCreatureQuestInvolvedRelationsReverse(id))
                enders.insert(entry);
        // Enders within GiverRange are found by FindGiver; one that is not has no work for the bot.
        if (Point const* ender = NearestSpawn(bot, state, EnderSpawns, enders, GiverRange, TravelRange))
            return Position(ender->X, ender->Y, ender->Z);
    }
    if (std::optional<Position> marker = MarkerSpot(bot, state, needs))
        return marker;
    Point const* far = spot(NearestSpawn(bot, state, HuntSpawns, needs.Creatures, 0.0f, TravelRange),
        NearestSpawn(bot, state, ObjectSpawns, needs.Objects, 0.0f, TravelRange));
    if (far)
        return Position(far->X, far->Y, far->Z);
    return std::nullopt;
}

// A known spell that opens the object's lock, or 0. Keys and profession skills the bot lacks do not count.
uint32 OpenSpellFor(Player const& bot, GameObject const& object)
{
    auto known = [&](uint32 lockType) -> uint32
    {
        if (auto found = OpenSpells.find(lockType); found != OpenSpells.end())
            for (uint32 spell : found->second)
                if (bot.HasSpell(spell))
                    return spell;
        return 0;
    };
    LockEntry const* lock = sLockStore.LookupEntry(object.GetGOInfo()->GetLockId());
    if (!lock)
    {
        // Without a lock any opening spell does.
        for (auto const& [lockType, spells] : OpenSpells)
            if (uint32 spell = known(lockType))
                return spell;
        return 0;
    }
    for (uint32 i = 0; i < MAX_LOCK_CASE; ++i)
    {
        if (lock->Type[i] != LOCK_KEY_SKILL)
            continue;
        SkillType skill = SkillByLockType(LockType(lock->Index[i]));
        if (skill != SKILL_NONE && bot.GetSkillValue(skill) < lock->Skill[i])
            continue;
        if (uint32 spell = known(lock->Index[i]))
            return spell;
    }
    return 0;
}

bool ClaimedByPlayer(Player& bot, GameObject const& object)
{
    for (MapReference const& ref : bot.GetMap()->GetPlayers())
        if (Player* player = ref.GetSource())
            if (player != &bot && player->GetSession() && !player->GetSession()->IsServerOrigin() &&
                player->IsWithinDist(&object, PlayerClaimRange))
                return true;
    return false;
}

bool Usable(Player& bot, RandomPlayerbotQuests::BotState const& state, GameObject const& object)
{
    if (!object.isSpawned() || object.getLootState() != GO_READY || state.SkippedObjects.count(object.GetGUID().GetRawValue()) ||
        !ZoneFitsBot(object.GetZoneId(), bot) || ClaimedByPlayer(bot, object))
        return false;
    switch (object.GetGoType())
    {
        case GAMEOBJECT_TYPE_GOOBER:
            return object.GetGoState() == GO_STATE_READY;
        case GAMEOBJECT_TYPE_CHEST:
            return OpenSpellFor(bot, object) != 0;
        default:
            return false;
    }
}

GameObject* FindObject(Player& bot, RandomPlayerbotQuests::BotState& state, uint32 now)
{
    for (auto it = state.SkippedObjects.begin(); it != state.SkippedObjects.end();)
        it = int32(it->second - now) <= 0 ? state.SkippedObjects.erase(it) : std::next(it);
    GameObject* best = nullptr;
    float bestDistance = ObjectRange + 1.0f;
    for (uint32 entry : CollectNeeds(bot).Objects)
    {
        std::list<GameObject*> objects;
        bot.GetGameObjectListWithEntryInGrid(objects, entry, ObjectRange);
        for (GameObject* object : objects)
        {
            float distance = bot.GetDistance(object);
            if (distance < bestDistance && NeedsObject(bot, entry) && Usable(bot, state, *object))
            {
                best = object;
                bestDistance = distance;
            }
        }
    }
    return best;
}

void SkipObject(RandomPlayerbotQuests::BotState& state, uint32 now)
{
    state.SkippedObjects[state.Object.GetRawValue()] = now + GiverSkipMs;
    state.Object.Clear();
}

// Takes every item the bot may loot from the open object, then closes it natively.
void LootObject(WorldSession& session, Player& bot, GameObject& object)
{
    size_t slots = object.loot.items.size() + object.loot.quest_items.size();
    for (size_t slot = 0; slot < slots && bot.GetLootGUID() == object.GetGUID(); ++slot)
    {
        WorldPacket store(CMSG_AUTOSTORE_LOOT_ITEM, 1);
        store << uint8(slot);
        session.HandleAutostoreLootItemOpcode(store);
    }
    if (bot.GetLootGUID() == object.GetGUID())
        session.DoLootRelease(object.GetGUID());
}

// Walks to the object and uses it; chests are opened with a native opening spell and looted.
// True while the bot is busy with the object.
bool UpdateObject(WorldSession& session, Player& bot, RandomPlayerbotQuests::BotState& state, uint32 now)
{
    GameObject* object = ObjectAccessor::GetGameObject(bot, state.Object);
    if (!object || getMSTimeDiff(state.ObjectSinceMs, now) > ObjectTimeoutMs)
    {
        if (object && bot.GetLootGUID() == object->GetGUID())
            session.DoLootRelease(object->GetGUID());
        SkipObject(state, now);
        return false;
    }
    if (bot.GetLootGUID() == object->GetGUID())
    {
        LootObject(session, bot, *object);
        ++ObjectCount;
        TC_LOG_DEBUG("module.playerbots", "PB-QUEST: %s looted object %u", bot.GetName().c_str(), object->GetEntry());
        SkipObject(state, now);
        return false;
    }
    if (!Usable(bot, state, *object))
    {
        SkipObject(state, now);
        return false;
    }
    if (!object->IsWithinDistInMap(&bot, object->GetInteractionDistance()))
    {
        if (!bot.isMoving())
            bot.GetMotionMaster()->MovePoint(0, object->GetPosition(), true);
        return true;
    }
    bot.StopMoving();
    bot.SetFacingToObject(object);
    if (object->GetGoType() == GAMEOBJECT_TYPE_GOOBER)
    {
        object->Use(&bot);
        ++ObjectCount;
        TC_LOG_DEBUG("module.playerbots", "PB-QUEST: %s used object %u", bot.GetName().c_str(), object->GetEntry());
        SkipObject(state, now);
        return false;
    }
    // The opening spell's cast bar runs; the loot is taken on a later scan.
    if (bot.CastSpell(object, OpenSpellFor(bot, *object)) != SPELL_CAST_OK)
        SkipObject(state, now);
    return !state.Object.IsEmpty();
}

bool Ride(Player& bot, RandomPlayerbotQuests::BotState& state, float distance)
{
    if (distance < RideDistance || bot.IsMounted() || bot.IsInWater() || !bot.IsOutdoors() ||
        bot.GetShapeshiftForm() != FORM_NONE || bot.IsInCombat())
        return false;
    uint32 spell = RandomPlayerbotProgression::MountSpell(bot);
    if (!spell || bot.CastSpell(&bot, spell) != SPELL_CAST_OK)
        return false;
    state.Riding = true;
    return true;
}

// Travels to the current spot in sections; true while on the way.
bool Travel(Player& bot, RandomPlayerbotQuests::BotState& state, Position& home, uint32 now)
{
    float distance = bot.GetExactDist2d(&state.Spot);
    if (distance <= RandomPlayerbotGrind::WanderRadius)
    {
        RandomPlayerbotQuests::Dismount(bot, state);
        if (!state.SpotArrivedMs)
            state.SpotArrivedMs = now ? now : 1;
        home = state.Spot;
        return false;
    }
    uint32 sinceCheck = state.TravelCheckMs ? getMSTimeDiff(state.TravelCheckMs, now) : 0;
    if (!state.TravelCheckMs || sinceCheck >= TravelCheckMs)
    {
        // Stuck behind something: give this spot up. A long gap (a fight, resting) is no measure.
        if (state.TravelCheckMs && sinceCheck < 2 * TravelCheckMs &&
            bot.GetExactDist2d(&state.TravelCheck) < TravelMinGain)
        {
            if (state.TriedSpots.size() >= MaxTriedSpots)
                state.TriedSpots.erase(state.TriedSpots.begin());
            state.TriedSpots.push_back(state.Spot);
            state.ClearSpot();
            RandomPlayerbotQuests::Dismount(bot, state);
            bot.StopMoving();
            return false;
        }
        state.TravelCheck = bot.GetPosition();
        state.TravelCheckMs = now ? now : 1;
    }
    if (bot.isMoving())
        return true;
    if (Ride(bot, state, distance))
        return true;
    bot.GetMotionMaster()->MovePoint(0, state.Spot, true);
    return true;
}

// Frees log slots before accepting new quests.
void TidyLog(WorldSession& session, Player& bot, RandomPlayerbotQuests::BotState& state)
{
    std::vector<LogEntry> log;
    uint32 free = 0;
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 id = bot.GetQuestSlotQuestId(slot);
        Quest const* quest = id ? sObjectMgr->GetQuestTemplate(id) : nullptr;
        if (!quest)
        {
            ++free;
            continue;
        }
        if (bot.GetQuestStatus(id) == QUEST_STATUS_COMPLETE)
            continue;
        log.push_back({ id, QuestLevelFor(*quest, bot.getLevel()) <= GrayLevel(bot.getLevel()), BotQuests.count(id) != 0 });
    }
    for (uint32 id : LogCleanup(log, free))
    {
        state.Shelved.insert(id);
        state.Quests.erase(id);
        Abandon(session, bot, id, "log-cleanup");
    }
}
}

void RandomPlayerbotQuests::Configure(bool enabled)
{
    QuestsEnabled = enabled;
}

bool RandomPlayerbotQuests::Enabled()
{
    return QuestsEnabled;
}

bool RandomPlayerbotQuests::Ready()
{
    return QuestsEnabled && IndexReady;
}

void RandomPlayerbotQuests::BuildIndex()
{
    if (IndexReady || !QuestsEnabled)
        return;
    uint32 started = getMSTime();

    // Spawned creature entries in the open world, with their positions.
    // CreatureData has no zone; the creature table carries it.
    std::unordered_map<uint64, uint16> spawnZones;
    if (QueryResult result = WorldDatabase.Query("SELECT guid, zoneId FROM creature"))
    {
        do
            spawnZones[(*result)[0].GetUInt32()] = (*result)[1].GetUInt16();
        while (result->NextRow());
    }
    std::unordered_map<uint32, std::vector<Point>> spawns;
    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
        if (OpenWorld(data.mapId) && (data.phaseId == 0 || data.phaseId == DEFAULT_PHASE) && !data.phaseGroup &&
            data.terrainSwapMap == -1)
            spawns[data.id].push_back({ uint16(data.mapId), spawnZones[uint64(spawnId)], data.spawnPoint.GetPositionX(),
                data.spawnPoint.GetPositionY(), data.spawnPoint.GetPositionZ(), data.spawnPoint.GetOrientation() });

    // Quest objects (goobers and chests) in the open world, zones from the gameobject table.
    std::unordered_map<uint64, uint16> objectZones;
    if (QueryResult result = WorldDatabase.Query("SELECT guid, zoneId FROM gameobject"))
    {
        do
            objectZones[(*result)[0].GetUInt32()] = (*result)[1].GetUInt16();
        while (result->NextRow());
    }
    std::unordered_map<uint32, std::vector<Point>> objectSpawns;
    for (auto const& [spawnId, data] : sObjectMgr->GetAllGameObjectData())
    {
        GameObjectTemplate const* info = sObjectMgr->GetGameObjectTemplate(data.id);
        if (info && (info->type == GAMEOBJECT_TYPE_GOOBER || info->type == GAMEOBJECT_TYPE_CHEST) &&
            OpenWorld(data.mapId) && (data.phaseId == 0 || data.phaseId == DEFAULT_PHASE) && !data.phaseGroup &&
            data.terrainSwapMap == -1)
            objectSpawns[data.id].push_back({ uint16(data.mapId), objectZones[uint64(spawnId)],
                data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY(), data.spawnPoint.GetPositionZ(),
                data.spawnPoint.GetOrientation() });
    }

    // Spells that open locks, by lock type: no reagents, no items, cast on an object.
    for (uint32 id = 0; id < sSpellMgr->GetSpellInfoStoreSize(); ++id)
    {
        SpellInfo const* spell = sSpellMgr->GetSpellInfo(id);
        if (!spell || spell->IsPassive() || spell->Reagent[0] > 0 || spell->RequiresSpellFocus)
            continue;
        for (SpellEffectInfo const& effect : spell->Effects)
            if (effect.Effect == SPELL_EFFECT_OPEN_LOCK && effect.TargetA.GetTarget() == TARGET_GAMEOBJECT_TARGET &&
                effect.MiscValue > 0)
                OpenSpells[uint32(effect.MiscValue)].push_back(id);
    }

    // Entries that count for each kill objective: the target itself and creatures crediting it.
    std::unordered_map<uint32, std::vector<uint32>> creditors;
    for (auto const& [entry, info] : *sObjectMgr->GetCreatureTemplates())
    {
        if (!spawns.count(entry))
            continue;
        creditors[entry].push_back(entry);
        for (uint32 credit : info.KillCredit)
            if (credit && credit != entry)
                creditors[credit].push_back(entry);
    }

    // Quest -> spawned creature starters and enders.
    std::unordered_map<uint32, std::vector<uint32>> starters;
    for (auto const& [entry, quest] : *sObjectMgr->GetCreatureQuestRelationMapHACK())
        if (spawns.count(entry))
            starters[quest].push_back(entry);

    std::vector<Quest const*> candidates;
    std::set<uint32> items;
    for (auto const& [id, quest] : sObjectMgr->GetQuestTemplates())
    {
        Verdict verdict = Classify(ShapeOf(quest));
        ++VerdictCounts[uint8(verdict)];
        if (!starters.count(id))
            continue;
        if (verdict == Verdict::NoObjective)
        {
            // Talk-to and delivery quests: only when the recipient stands in the giver's zone, close enough to walk.
            if (quest.GetMoneyReward(nullptr) < 0 || !DeliveryNearby(starters[id],
                sObjectMgr->GetCreatureQuestInvolvedRelationsReverse(id), spawns))
                continue;
            ++DeliveryCount;
            candidates.push_back(&quest);
            continue;
        }
        if (verdict != Verdict::Supported)
            continue;
        bool ender = false;
        for (uint32 entry : sObjectMgr->GetCreatureQuestInvolvedRelationsReverse(id))
            ender = ender || spawns.count(entry);
        bool killable = true, usable = true;
        for (uint32 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        {
            int32 objective = quest.RequiredNpcOrGo[i];
            if (objective > 0 && !creditors.count(uint32(objective)))
                killable = false;
            else if (objective < 0)
            {
                GameObjectTemplate const* info = sObjectMgr->GetGameObjectTemplate(uint32(-objective));
                if (!info || info->type != GAMEOBJECT_TYPE_GOOBER || !objectSpawns.count(uint32(-objective)))
                    usable = false;
            }
        }
        if (!usable)
        {
            --VerdictCounts[uint8(Verdict::Supported)];
            ++VerdictCounts[uint8(Verdict::GameObject)];
        }
        if (!ender || !killable || !usable)
            continue;
        candidates.push_back(&quest);
        for (uint32 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
            if (quest.RequiredItemId[i])
                items.insert(quest.RequiredItemId[i]);
    }

    // Creature drops of the candidates' items.
    std::unordered_map<uint32, std::vector<uint32>> droppers;
    if (!items.empty())
    {
        std::ostringstream list;
        for (uint32 item : items)
            list << (list.tellp() ? "," : "") << item;
        if (QueryResult result = WorldDatabase.PQuery("SELECT ct.entry, clt.Item FROM creature_template ct "
            "JOIN creature_loot_template clt ON clt.Entry = ct.lootid WHERE ct.lootid <> 0 AND clt.Reference = 0 "
            "AND clt.Item IN (%s)", list.str().c_str()))
        {
            do
            {
                uint32 entry = (*result)[0].GetUInt32();
                if (spawns.count(entry))
                    droppers[(*result)[1].GetUInt32()].push_back(entry);
            } while (result->NextRow());
        }
    }

    // Chests that hold the candidates' items.
    std::unordered_map<uint32, std::vector<uint32>> chests; // item -> chest entries
    if (!items.empty())
    {
        std::unordered_map<uint32, std::vector<uint32>> chestsByLoot;
        for (auto const& [entry, info] : *sObjectMgr->GetGameObjectTemplates())
            if (info.type == GAMEOBJECT_TYPE_CHEST && info.GetLootId() && objectSpawns.count(entry))
                chestsByLoot[info.GetLootId()].push_back(entry);
        std::ostringstream list;
        for (uint32 item : items)
            list << (list.tellp() ? "," : "") << item;
        if (QueryResult result = WorldDatabase.PQuery("SELECT Entry, Item FROM gameobject_loot_template "
            "WHERE Reference = 0 AND Item IN (%s)", list.str().c_str()))
        {
            do
            {
                if (auto found = chestsByLoot.find((*result)[0].GetUInt32()); found != chestsByLoot.end())
                    for (uint32 entry : found->second)
                        chests[(*result)[1].GetUInt32()].push_back(entry);
            } while (result->NextRow());
        }
    }

    std::set<uint32> huntEntries, objectEntries;
    for (Quest const* quest : candidates)
    {
        bool lootable = true;
        for (uint32 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        {
            uint32 item = quest->RequiredItemId[i];
            if (item && item != quest->GetSrcItemId() &&
                std::find(std::begin(quest->ItemDrop), std::end(quest->ItemDrop), item) == std::end(quest->ItemDrop) &&
                !droppers.count(item) && !chests.count(item))
                lootable = false;
        }
        if (!lootable)
            continue;
        BotQuests.insert(quest->GetQuestId());
        for (uint32 entry : sObjectMgr->GetCreatureQuestInvolvedRelationsReverse(quest->GetQuestId()))
            if (auto found = spawns.find(entry); found != spawns.end())
                EnderSpawns[entry] = found->second;
        for (uint32 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
            if (quest->RequiredNpcOrGo[i] < 0)
            {
                ObjectiveObjects.insert(uint32(-quest->RequiredNpcOrGo[i]));
                objectEntries.insert(uint32(-quest->RequiredNpcOrGo[i]));
            }
        for (uint32 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
            if (uint32 item = quest->RequiredItemId[i]; item && chests.count(item))
            {
                std::vector<uint32>& list = ItemObjects[item];
                if (list.empty())
                    for (uint32 entry : chests[item])
                    {
                        list.push_back(entry);
                        ObjectItems[entry].push_back(item);
                        objectEntries.insert(entry);
                    }
            }
        for (uint32 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
            if (quest->RequiredNpcOrGo[i] > 0)
            {
                uint32 target = uint32(quest->RequiredNpcOrGo[i]);
                std::vector<uint32>& list = Creditors[target];
                if (list.empty())
                    for (uint32 entry : creditors[target])
                    {
                        list.push_back(entry);
                        CreditTargets[entry].push_back(target);
                        huntEntries.insert(entry);
                    }
            }
        for (uint32 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
            if (uint32 item = quest->RequiredItemId[i]; item && droppers.count(item))
            {
                std::vector<uint32>& list = Droppers[item];
                if (list.empty())
                    for (uint32 entry : droppers[item])
                    {
                        list.push_back(entry);
                        DroppedItems[entry].push_back(item);
                        huntEntries.insert(entry);
                    }
            }
    }
    for (uint32 entry : huntEntries)
        HuntSpawns[entry] = spawns[entry];
    for (uint32 entry : objectEntries)
        ObjectSpawns[entry] = objectSpawns[entry];

    // Givers with at least one bot quest, bucketed by the levels their quests suit.
    GiversByLevel.assign(RandomPlayerbotLevels::MaxLevel + 1, {});
    for (auto const& [entry, info] : *sObjectMgr->GetCreatureTemplates())
    {
        auto found = spawns.find(entry);
        if (found == spawns.end() || !(info.npcflag & UNIT_NPC_FLAG_QUESTGIVER))
            continue;
        std::vector<uint32> quests;
        for (uint32 id : sObjectMgr->GetCreatureQuestRelations(entry))
            if (BotQuests.count(id))
                quests.push_back(id);
        uint8 teams = TeamsOf(info);
        if (quests.empty() || !teams)
            continue;
        for (Point const& point : found->second)
        {
            uint32 index = uint32(Givers.size());
            Givers.push_back({ point, teams, quests });
            std::set<uint32> levels;
            for (uint32 id : quests)
            {
                Quest const* quest = sObjectMgr->GetQuestTemplate(id);
                for (uint32 level = quest->GetMinLevel(); level <= RandomPlayerbotLevels::MaxLevel; ++level)
                    if (LevelFits(level, QuestLevelFor(*quest, level), quest->GetMinLevel()))
                        levels.insert(level);
                    else if (level > quest->GetMinLevel() + 20)
                        break;
            }
            for (uint32 level : levels)
                GiversByLevel[level].push_back(index);
        }
    }

    // Givers close together form quest hubs; relocated bots prefer big ones.
    std::unordered_map<uint64, std::vector<uint32>> cells; // map and 60-yard cell -> givers
    auto cellOf = [](Point const& point, int32 dx, int32 dy)
    {
        int64 x = int64(std::floor(point.X / HubRadius)) + dx, y = int64(std::floor(point.Y / HubRadius)) + dy;
        return (uint64(point.Map) << 48) ^ (uint64(x & 0xFFFFFF) << 24) ^ uint64(y & 0xFFFFFF);
    };
    for (uint32 i = 0; i < Givers.size(); ++i)
        cells[cellOf(Givers[i].Pos, 0, 0)].push_back(i);
    for (Giver& giver : Givers)
    {
        uint32 near = 0;
        for (int32 dx = -1; dx <= 1; ++dx)
            for (int32 dy = -1; dy <= 1; ++dy)
                if (auto found = cells.find(cellOf(giver.Pos, dx, dy)); found != cells.end())
                    for (uint32 other : found->second)
                    {
                        Point const& pos = Givers[other].Pos;
                        if (pos.Map == giver.Pos.Map && std::hypot(pos.X - giver.Pos.X, pos.Y - giver.Pos.Y) <= HubRadius)
                            ++near;
                    }
        giver.HubSize = std::max<uint32>(near, 1);
    }

    uint32 withPoi = 0;
    for (uint32 id : BotQuests)
        if (QuestPOIData const* poi = sObjectMgr->GetQuestPOIData(int32(id)); poi && !poi->Blobs.empty())
            ++withPoi;

    IndexReady = true;
    uint32 const* counts = VerdictCounts;
    TC_LOG_INFO("module.playerbots", "Random playerbots: %u quests for bots (of %u simple; skipped %u unavailable, "
        "%u scripted, %u unspawned object, %u talk-to of which %u in reach) at %u givers, %u with quest-map "
        "markers, %u quest objects, in %u ms.",
        uint32(BotQuests.size()), counts[uint8(Verdict::Supported)], counts[uint8(Verdict::Unavailable)],
        counts[uint8(Verdict::Scripted)], counts[uint8(Verdict::GameObject)], counts[uint8(Verdict::NoObjective)],
        DeliveryCount, uint32(Givers.size()), withPoi, uint32(ObjectSpawns.size()), getMSTimeDiff(started, getMSTime()));
}

Creature* RandomPlayerbotQuests::SelectQuestTarget(Player& bot)
{
    if (!Ready())
        return nullptr;
    bool open = false;
    ForEachIncomplete(bot, [&](Quest const&) { open = true; });
    if (!open)
        return nullptr;
    std::list<Unit*> units;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(&bot, &bot, QuestTargetRange);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(&bot, units, check);
    Cell::VisitAllObjects(&bot, searcher, QuestTargetRange);
    Creature* best = nullptr;
    float bestDistance = QuestTargetRange + 1.0f;
    for (Unit* unit : units)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature)
            continue;
        float distance = bot.GetDistance(creature);
        if (distance < bestDistance && NeedsCreature(bot, creature->GetEntry()) &&
            RandomPlayerbotGrind::Eligible(bot, *creature))
        {
            best = creature;
            bestDistance = distance;
        }
    }
    return best;
}

RandomPlayerbotQuests::BotState::~BotState()
{
    SetActivity(*this, Activity::None);
}

void RandomPlayerbotQuests::Dismount(Player& bot, BotState& state)
{
    if (state.Riding && bot.IsMounted())
        bot.RemoveAurasByType(SPELL_AURA_MOUNTED);
    state.Riding = false;
}

bool RandomPlayerbotQuests::Update(WorldSession& session, Player& bot, BotState& state, Position& home, uint32_t now)
{
    if (state.Riding && !bot.IsMounted() && !bot.IsNonMeleeSpellCast(false))
        state.Riding = false;
    if (!Ready() || int32(state.NextScanMs - now) > 0)
        return !state.Giver.IsEmpty() || !state.Object.IsEmpty() || (state.HasSpot && !state.SpotArrivedMs);
    state.NextScanMs = now + ScanIntervalMs;
    if (!state.LastProgressMs)
        state.LastProgressMs = now ? now : 1;
    bool open = TrackProgress(session, bot, state, now);

    if (!state.Giver.IsEmpty())
    {
        Creature* giver = ObjectAccessor::GetCreature(bot, state.Giver);
        if (!giver || !giver->IsAlive() || getMSTimeDiff(state.GiverSinceMs, now) > GiverTimeoutMs)
        {
            if (giver)
                state.SkippedGivers[state.Giver.GetRawValue()] = now + GiverSkipMs;
            state.Giver.Clear();
        }
        else if (bot.CanInteractWithQuestGiver(giver))
        {
            Dismount(bot, state);
            bot.StopMoving();
            bot.SetFacingToObject(giver);
            TurnIn(session, bot, *giver);
            TidyLog(session, bot, state);
            Accept(session, bot, state, *giver);
            // Whatever is left (log full, level) is not retried at this giver for a while.
            state.SkippedGivers[state.Giver.GetRawValue()] = now + GiverSkipMs;
            state.Giver.Clear();
            state.ClearSpot();
            home = bot.GetPosition();
            SetActivity(state, Activity::Questing);
            return false;
        }
        else
        {
            if (!bot.isMoving())
                bot.GetMotionMaster()->MovePoint(0, giver->GetPosition(), true);
            SetActivity(state, Activity::Questing);
            return true;
        }
    }

    if (Creature* giver = FindGiver(bot, state, now))
    {
        state.Giver = giver->GetGUID();
        state.GiverSinceMs = now;
        state.Object.Clear();
        state.IdleSinceMs = 0;
        bot.GetMotionMaster()->MovePoint(0, giver->GetPosition(), true);
        SetActivity(state, Activity::Questing);
        return true;
    }
    if (!open)
    {
        state.ClearSpot();
        Dismount(bot, state);
        SetActivity(state, Activity::Grinding);
        if (!state.IdleSinceMs)
            state.IdleSinceMs = now ? now : 1;
        else if (getMSTimeDiff(state.IdleSinceMs, now) >= IdleRelocateMs)
        {
            state.IdleSinceMs = 0;
            state.WantsRelocation = true;
        }
        return false;
    }

    if (!state.Object.IsEmpty() && UpdateObject(session, bot, state, now))
    {
        SetActivity(state, Activity::Questing);
        return true;
    }
    if (GameObject* object = FindObject(bot, state, now))
    {
        Dismount(bot, state);
        state.Object = object->GetGUID();
        state.ObjectSinceMs = now;
        state.IdleSinceMs = 0;
        SetActivity(state, Activity::Questing);
        return UpdateObject(session, bot, state, now);
    }

    if (state.HasSpot && state.SpotArrivedMs && SpotStalled(now, state.SpotArrivedMs, state.LastProgressMs))
    {
        if (state.TriedSpots.size() >= MaxTriedSpots)
            state.TriedSpots.erase(state.TriedSpots.begin());
        state.TriedSpots.push_back(state.Spot);
        state.ClearSpot();
    }
    if (!state.HasSpot)
    {
        std::optional<Position> spot = NextSpot(bot, state);
        if (!spot)
        {
            // Nothing reachable here: wait, then move on to another quest giver.
            Dismount(bot, state);
            SetActivity(state, Activity::Grinding);
            if (!state.IdleSinceMs)
                state.IdleSinceMs = now ? now : 1;
            else if (getMSTimeDiff(state.IdleSinceMs, now) >= IdleRelocateMs)
            {
                state.IdleSinceMs = 0;
                state.WantsRelocation = true;
            }
            return false;
        }
        state.HasSpot = true;
        state.Spot = *spot;
        state.SpotArrivedMs = 0;
        state.TravelCheckMs = 0;
    }
    state.IdleSinceMs = 0;
    // At the spot the grind finds quest mobs; the wander leash keeps the bot around it.
    bool travelling = Travel(bot, state, home, now);
    SetActivity(state, travelling ? Activity::Travelling : Activity::Questing);
    return travelling;
}

bool RandomPlayerbotQuests::RelocateToGiver(Player& bot)
{
    if (!Ready())
        return false;
    uint32 level = std::min<uint32>(bot.getLevel(), RandomPlayerbotLevels::MaxLevel);
    uint8 team = TeamOf(bot);
    std::vector<uint32> const& bucket = GiversByLevel[level];
    Giver const* best = nullptr;
    uint32 found = 0;
    for (uint32 attempt = 0; attempt < MaxGiverAttempts && !bucket.empty() && found < HubCandidates; ++attempt)
    {
        Giver const& giver = Givers[bucket[urand(0, uint32(bucket.size()) - 1)]];
        if (!(giver.Teams & team) || (giver.Pos.Map == bot.GetMapId() &&
            bot.GetExactDist2d(giver.Pos.X, giver.Pos.Y) < GiverRange) ||
            !RandomPlayerbotLevels::ZoneFits(giver.Pos.Zone, level, team))
            continue;
        bool offers = false;
        for (uint32 id : giver.Quests)
            if (Quest const* quest = sObjectMgr->GetQuestTemplate(id); quest && WouldTake(bot, nullptr, *quest))
                offers = true;
        if (!offers)
            continue;
        ++found;
        if (!best || giver.HubSize > best->HubSize)
            best = &giver;
    }
    if (!best)
        return false;
    // A few steps in front of the giver, facing it.
    float x = best->Pos.X + 4.0f * std::cos(best->Pos.O), y = best->Pos.Y + 4.0f * std::sin(best->Pos.O);
    if (!bot.TeleportTo(best->Pos.Map, x, y, best->Pos.Z + 0.5f, best->Pos.O + float(M_PI)))
        return false;
    TC_LOG_DEBUG("module.playerbots", "PB-QUEST: %s level %u sent to a quest hub of %u givers in zone %u",
        bot.GetName().c_str(), level, best->HubSize, uint32(best->Pos.Zone));
    return true;
}

std::string RandomPlayerbotQuests::Describe()
{
    if (!Ready())
        return QuestsEnabled ? "quests loading" : "quests off";
    return "quests " + std::to_string(BotQuests.size()) + " at " + std::to_string(Givers.size()) +
        " givers, accepted " + std::to_string(AcceptedCount.load()) + ", completed " +
        std::to_string(CompletedCount.load()) + ", abandoned " + std::to_string(AbandonedCount.load()) +
        ", objects used " + std::to_string(ObjectCount.load()) + "; bots questing " +
        std::to_string(ActivityCounts[uint8(Activity::Questing)].load()) + ", travelling " +
        std::to_string(ActivityCounts[uint8(Activity::Travelling)].load()) + ", grinding " +
        std::to_string(ActivityCounts[uint8(Activity::Grinding)].load());
}
