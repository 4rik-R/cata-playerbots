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
#include "RandomPlayerbotQuestPolicy.h"
#include "CellImpl.h"
#include "Creature.h"
#include "CreatureData.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "QuestDef.h"
#include "QuestPackets.h"
#include "Random.h"
#include "TerrainMgr.h"
#include "Timer.h"
#include "WorldSession.h"
#include <algorithm>
#include <cmath>
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
    float X, Y, Z, O;
};

struct Giver
{
    Point Pos;
    uint8 Teams;
    std::vector<uint32> Quests;
};

std::unordered_set<uint32> BotQuests;
// Creature entry -> kill objectives it counts for; kill objective -> entries that count.
std::unordered_map<uint32, std::vector<uint32>> CreditTargets, Creditors;
// Creature entry -> needed items it drops; item -> entries that drop it.
std::unordered_map<uint32, std::vector<uint32>> DroppedItems, Droppers;
std::unordered_map<uint32, std::vector<Point>> HuntSpawns;
std::vector<Giver> Givers;
std::vector<std::vector<uint32>> GiversByLevel; // level -> index into Givers
uint32 VerdictCounts[5] = {};
std::atomic<uint32> AcceptedCount { 0 }, CompletedCount { 0 }, AbandonedCount { 0 };

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

uint8 TeamsOf(CreatureTemplate const& info)
{
    FactionTemplateEntry const* alliance = sFactionTemplateStore.LookupEntry(AllianceFactionTemplate);
    FactionTemplateEntry const* horde = sFactionTemplateStore.LookupEntry(HordeFactionTemplate);
    FactionTemplateEntry const* faction = sFactionTemplateStore.LookupEntry(info.faction);
    if (!alliance || !horde || !faction)
        return 0;
    return (faction->IsHostileTo(*alliance) ? 0 : RandomPlayerbotLevels::Alliance) |
        (faction->IsHostileTo(*horde) ? 0 : RandomPlayerbotLevels::Horde);
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

bool NeedsKill(Player const& bot, uint32 target)
{
    bool needed = false;
    ForEachIncomplete(bot, [&](Quest const& quest)
    {
        for (uint32 i = 0; i < QUEST_OBJECTIVES_COUNT && !needed; ++i)
            if (quest.RequiredNpcOrGo[i] == int32(target) &&
                bot.GetReqKillOrCastCurrentCount(quest.GetQuestId(), int32(target)) < quest.RequiredNpcOrGoCount[i])
                needed = true;
    });
    return needed;
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

void Abandon(WorldSession& session, Player& bot, uint32 id)
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
        TC_LOG_DEBUG("module.playerbots", "PB-QUEST: %s abandoned stalled quest %u", bot.GetName().c_str(), id);
    }
}

// Abandons quests without progress for QuestStallMs; true when any quest is open.
bool TrackProgress(WorldSession& session, Player& bot, RandomPlayerbotQuests::BotState& state, uint32 now)
{
    std::unordered_set<uint32> seen;
    std::vector<uint32> stalled;
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 id = bot.GetQuestSlotQuestId(slot);
        Quest const* quest = id ? sObjectMgr->GetQuestTemplate(id) : nullptr;
        if (!quest)
            continue;
        seen.insert(id);
        if (bot.GetQuestStatus(id) != QUEST_STATUS_INCOMPLETE)
            continue;
        uint32 signature = ProgressSignature(Counts(bot, *quest));
        auto [it, inserted] = state.Quests.try_emplace(id, RandomPlayerbotQuests::BotState::Progress{ signature, now });
        if (inserted)
            continue;
        if (it->second.Signature != signature)
            it->second = { signature, now };
        else if (Stalled(now, it->second.SinceMs))
            stalled.push_back(id);
    }
    for (auto it = state.Quests.begin(); it != state.Quests.end();)
        it = seen.count(it->first) ? std::next(it) : state.Quests.erase(it);
    for (uint32 id : stalled)
    {
        state.Shelved.insert(id);
        state.Quests.erase(id);
        Abandon(session, bot, id);
    }
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
            state.SkippedGivers.count(creature->GetGUID().GetRawValue()))
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

// Nearest spawn of a mob the bot's quests still need, on its map and within SpawnSearchRange.
Point const* NearestHuntSpawn(Player const& bot)
{
    std::unordered_set<uint32> entries;
    ForEachIncomplete(bot, [&](Quest const& quest)
    {
        for (uint32 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
            if (quest.RequiredNpcOrGo[i] > 0 && bot.GetReqKillOrCastCurrentCount(quest.GetQuestId(),
                quest.RequiredNpcOrGo[i]) < quest.RequiredNpcOrGoCount[i])
                if (auto found = Creditors.find(uint32(quest.RequiredNpcOrGo[i])); found != Creditors.end())
                    entries.insert(found->second.begin(), found->second.end());
        for (uint32 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
            if (quest.RequiredItemId[i] && bot.GetItemCount(quest.RequiredItemId[i], true) < quest.RequiredItemCount[i])
                if (auto found = Droppers.find(quest.RequiredItemId[i]); found != Droppers.end())
                    entries.insert(found->second.begin(), found->second.end());
    });
    Point const* best = nullptr;
    float bestDistance = SpawnSearchRange * SpawnSearchRange;
    for (uint32 entry : entries)
    {
        auto found = HuntSpawns.find(entry);
        if (found == HuntSpawns.end())
            continue;
        for (Point const& point : found->second)
        {
            if (point.Map != bot.GetMapId())
                continue;
            float dx = point.X - bot.GetPositionX(), dy = point.Y - bot.GetPositionY();
            float distance = dx * dx + dy * dy;
            if (distance < bestDistance)
            {
                best = &point;
                bestDistance = distance;
            }
        }
    }
    return best;
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
    std::unordered_map<uint32, std::vector<Point>> spawns;
    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
        if (OpenWorld(data.mapId) && (data.phaseId == 0 || data.phaseId == DEFAULT_PHASE) && !data.phaseGroup &&
            data.terrainSwapMap == -1)
            spawns[data.id].push_back({ uint16(data.mapId), data.spawnPoint.GetPositionX(),
                data.spawnPoint.GetPositionY(), data.spawnPoint.GetPositionZ(), data.spawnPoint.GetOrientation() });

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
        if (verdict != Verdict::Supported || !starters.count(id))
            continue;
        bool ender = false;
        for (uint32 entry : sObjectMgr->GetCreatureQuestInvolvedRelationsReverse(id))
            ender = ender || spawns.count(entry);
        bool killable = true;
        for (uint32 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
            if (quest.RequiredNpcOrGo[i] > 0 && !creditors.count(uint32(quest.RequiredNpcOrGo[i])))
                killable = false;
        if (!ender || !killable)
            continue;
        candidates.push_back(&quest);
        for (uint32 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
            if (quest.RequiredItemId[i])
                items.insert(quest.RequiredItemId[i]);
    }

    // Creature drops of the candidates' items (gameobject loot comes with M4.2).
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

    std::set<uint32> huntEntries;
    for (Quest const* quest : candidates)
    {
        bool lootable = true;
        for (uint32 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        {
            uint32 item = quest->RequiredItemId[i];
            if (item && item != quest->GetSrcItemId() &&
                std::find(std::begin(quest->ItemDrop), std::end(quest->ItemDrop), item) == std::end(quest->ItemDrop) &&
                !droppers.count(item))
                lootable = false;
        }
        if (!lootable)
            continue;
        BotQuests.insert(quest->GetQuestId());
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

    uint32 withPoi = 0;
    for (uint32 id : BotQuests)
        if (QuestPOIData const* poi = sObjectMgr->GetQuestPOIData(int32(id)); poi && !poi->Blobs.empty())
            ++withPoi;

    IndexReady = true;
    uint32 const* counts = VerdictCounts;
    TC_LOG_INFO("module.playerbots", "Random playerbots: %u quests for bots (of %u simple; skipped %u unavailable, "
        "%u scripted, %u object, %u talk-to) at %u givers, %u with quest-map markers, in %u ms.",
        uint32(BotQuests.size()), counts[uint8(Verdict::Supported)], counts[uint8(Verdict::Unavailable)],
        counts[uint8(Verdict::Scripted)], counts[uint8(Verdict::GameObject)], counts[uint8(Verdict::NoObjective)],
        uint32(Givers.size()), withPoi, getMSTimeDiff(started, getMSTime()));
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

bool RandomPlayerbotQuests::Update(WorldSession& session, Player& bot, BotState& state, Position& home, uint32_t now)
{
    if (!Ready() || int32(state.NextScanMs - now) > 0)
        return !state.Giver.IsEmpty() || (state.Hunting && bot.isMoving());
    state.NextScanMs = now + ScanIntervalMs;
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
            bot.StopMoving();
            bot.SetFacingToObject(giver);
            TurnIn(session, bot, *giver);
            Accept(session, bot, state, *giver);
            // Whatever is left (log full, level) is not retried at this giver for a while.
            state.SkippedGivers[state.Giver.GetRawValue()] = now + GiverSkipMs;
            state.Giver.Clear();
            home = bot.GetPosition();
            return false;
        }
        else
        {
            if (!bot.isMoving())
                bot.GetMotionMaster()->MovePoint(0, giver->GetPosition(), true);
            return true;
        }
    }

    if (Creature* giver = FindGiver(bot, state, now))
    {
        state.Giver = giver->GetGUID();
        state.GiverSinceMs = now;
        state.Hunting = false;
        state.IdleSinceMs = 0;
        bot.GetMotionMaster()->MovePoint(0, giver->GetPosition(), true);
        return true;
    }

    if (open)
    {
        state.IdleSinceMs = 0;
        Point const* spawn = NearestHuntSpawn(bot);
        Position target = spawn ? Position(spawn->X, spawn->Y, spawn->Z) : home;
        // Close enough: grind there; the wander leash keeps the bot around the spot.
        if (!spawn || home.GetExactDist2d(&target) < RandomPlayerbotGrind::WanderRadius)
        {
            state.Hunting = state.Hunting && bot.isMoving();
            return state.Hunting;
        }
        home = target;
        state.Hunting = true;
        bot.GetMotionMaster()->MovePoint(0, target, true);
        return true;
    }

    state.Hunting = false;
    if (!state.IdleSinceMs)
        state.IdleSinceMs = now ? now : 1;
    else if (getMSTimeDiff(state.IdleSinceMs, now) >= IdleRelocateMs)
    {
        state.IdleSinceMs = 0;
        state.WantsRelocation = true;
    }
    return false;
}

bool RandomPlayerbotQuests::RelocateToGiver(Player& bot)
{
    if (!Ready())
        return false;
    uint32 level = std::min<uint32>(bot.getLevel(), RandomPlayerbotLevels::MaxLevel);
    uint8 team = bot.GetTeamId() == TEAM_ALLIANCE ? RandomPlayerbotLevels::Alliance : RandomPlayerbotLevels::Horde;
    std::vector<uint32> const& bucket = GiversByLevel[level];
    for (uint32 attempt = 0; attempt < MaxGiverAttempts && !bucket.empty(); ++attempt)
    {
        Giver const& giver = Givers[bucket[urand(0, uint32(bucket.size()) - 1)]];
        if (!(giver.Teams & team) || (giver.Pos.Map == bot.GetMapId() &&
            bot.GetExactDist2d(giver.Pos.X, giver.Pos.Y) < GiverRange))
            continue;
        bool offers = false;
        for (uint32 id : giver.Quests)
            if (Quest const* quest = sObjectMgr->GetQuestTemplate(id); quest && WouldTake(bot, nullptr, *quest))
                offers = true;
        if (!offers)
            continue;
        uint32 zoneId = 0, areaId = 0;
        sTerrainMgr.GetZoneAndAreaId(PhasingHandler::GetEmptyPhaseShift(), zoneId, areaId, giver.Pos.Map,
            giver.Pos.X, giver.Pos.Y, giver.Pos.Z);
        if (!RandomPlayerbotLevels::ZoneFits(zoneId, level, team))
            continue;
        // A few steps in front of the giver, facing it.
        float x = giver.Pos.X + 4.0f * std::cos(giver.Pos.O), y = giver.Pos.Y + 4.0f * std::sin(giver.Pos.O);
        if (!bot.TeleportTo(giver.Pos.Map, x, y, giver.Pos.Z + 0.5f, giver.Pos.O + float(M_PI)))
            return false;
        TC_LOG_DEBUG("module.playerbots", "PB-QUEST: %s level %u sent to a quest giver in zone %u", bot.GetName().c_str(),
            level, zoneId);
        return true;
    }
    return false;
}

std::string RandomPlayerbotQuests::Describe()
{
    if (!Ready())
        return QuestsEnabled ? "quests loading" : "quests off";
    return "quests " + std::to_string(BotQuests.size()) + " at " + std::to_string(Givers.size()) +
        " givers, accepted " + std::to_string(AcceptedCount.load()) + ", completed " +
        std::to_string(CompletedCount.load()) + ", abandoned " + std::to_string(AbandonedCount.load());
}
