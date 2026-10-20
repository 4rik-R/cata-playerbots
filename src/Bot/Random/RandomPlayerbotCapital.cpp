/*
 * Capital visits for random-population bots. The donor mod-playerbots sends
 * bots to cities by teleport (RandomPlayerbotMgr::RandomTeleportForRpg); here
 * they use their hearthstone and walk to the city folk themselves.
 * Released under GNU GPL v2 or any later version.
 */
#include "RandomPlayerbotCapital.h"
#include "RandomPlayerbotCapitalPolicy.h"
#include "RandomPlayerbotLevels.h"
#include "RandomPlayerbotProgression.h"
#include "RandomPlayerbotQuests.h"
#include "Bag.h"
#include "Creature.h"
#include "CreatureData.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Item.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "Random.h"
#include "SpellHistory.h"
#include "Timer.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <atomic>
#include <unordered_map>
#include <vector>

using namespace RandomPlayerbotCapitalPolicy;
using Step = RandomPlayerbotQuests::CapitalVisit::Step;

namespace
{
constexpr uint32 Hearthstone = 8690;
constexpr uint32 AllianceFactionTemplate = 1;
constexpr uint32 HordeFactionTemplate = 2;
// Stormwind City and Orgrimmar.
constexpr uint32 CapitalZones[] = { 1519, 1637 };
// The city folk are looked for this close to their spawn, and talked to from this close.
constexpr float NpcSearchRange = 50.0f;
constexpr float TalkRange = 4.0f;
// A visit that cannot start for want of room is checked again this much later.
constexpr uint32 NoRoomRetryMs = 60 * 1000;
// Visits are run at this pace, not on every map update.
constexpr uint32 PaceMs = 500;

enum Kind : uint8
{
    RepairNpc = 1, // repair and sell, at a vendor who repairs
    TrainerNpc,    // the class trainer
    BrowseNpc,     // banker, auctioneer or innkeeper
};

struct CityNpc
{
    uint8 Kind;
    uint8 Class;
    uint8 Teams;
    uint16 Map;
    uint32 Zone;
    uint32 Entry;
    float X, Y, Z;
};
std::vector<CityNpc> CityNpcs;
bool IndexReady = false;
std::atomic<uint32> VisitCount { 0 }, FailedHearthCount { 0 };

uint8 TeamOf(Player const& bot)
{
    return bot.GetTeamId() == TEAM_ALLIANCE ? RandomPlayerbotLevels::Alliance : RandomPlayerbotLevels::Horde;
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

bool Serves(CityNpc const& npc, Player const& bot)
{
    return (npc.Teams & TeamOf(bot)) && npc.Map == bot.GetMapId() && npc.Zone == RandomPlayerbotProgression::CapitalZone(bot);
}

CityNpc const* Nearest(Player const& bot, uint8 kind)
{
    CityNpc const* best = nullptr;
    float bestDistance = 0.0f;
    for (CityNpc const& npc : CityNpcs)
    {
        if (npc.Kind != kind || !Serves(npc, bot) || (kind == TrainerNpc && npc.Class != bot.getClass()))
            continue;
        float distance = bot.GetExactDist2d(npc.X, npc.Y);
        if (!best || distance < bestDistance)
        {
            best = &npc;
            bestDistance = distance;
        }
    }
    return best;
}

CityNpc const* AnyBrowse(Player const& bot)
{
    std::vector<CityNpc const*> options;
    for (CityNpc const& npc : CityNpcs)
        if (npc.Kind == BrowseNpc && Serves(npc, bot))
            options.push_back(&npc);
    return options.empty() ? nullptr : options[urand(0, uint32(options.size()) - 1)];
}

RandomPlayerbotQuests::CapitalVisit::Errand ErrandAt(CityNpc const& npc)
{
    RandomPlayerbotQuests::CapitalVisit::Errand errand;
    errand.Kind = npc.Kind;
    errand.Entry = npc.Entry;
    errand.Pos.Relocate(npc.X, npc.Y, npc.Z);
    errand.PauseMs = npc.Kind == BrowseNpc ? urand(BrowseMinMs, BrowseMaxMs) : ErrandPauseMs;
    return errand;
}

// Lowest durability of an equipped item, in percent.
uint32 LowestDurability(Player const& bot)
{
    uint32 lowest = 100;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (Item* item = bot.GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            if (uint32 maximum = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY))
                lowest = std::min(lowest, 100 * item->GetUInt32Value(ITEM_FIELD_DURABILITY) / maximum);
    return lowest;
}

// Grey items a vendor buys, in the backpack and the bags.
std::vector<ObjectGuid> Junk(Player const& bot)
{
    std::vector<ObjectGuid> junk;
    auto check = [&junk](Item const* item)
    {
        if (item && item->GetTemplate()->GetQuality() == ITEM_QUALITY_POOR && item->GetTemplate()->GetSellPrice())
            junk.push_back(item->GetGUID());
    };
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        check(bot.GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        if (Bag const* bag = bot.GetBagByPos(bagSlot))
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                check(bag->GetItemByPos(uint8(slot)));
    return junk;
}

bool BagsFull(Player const& bot)
{
    return bot.GetFreeInventorySpace() == 0 && !Junk(bot).empty();
}

void RepairAndSell(WorldSession& session, Player& bot, Creature& vendor)
{
    bot.DurabilityRepairAll(true, bot.GetReputationPriceDiscount(&vendor), false);
    // Whatever the bot could not pay for is mended anyway, so the next visit is not due at once.
    bot.DurabilityRepairAll(false, 0.0f, false);
    if (vendor.HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_VENDOR))
        for (ObjectGuid const& guid : Junk(bot))
            if (Item* item = bot.GetItemByGuid(guid))
            {
                WorldPacket packet(CMSG_SELL_ITEM, 8 + 8 + 4);
                packet << vendor.GetGUID() << guid << uint32(item->GetCount());
                session.HandleSellItemOpcode(packet);
            }
    RandomPlayerbotProgression::Resupply(bot);
}

void EndVisit(Player& bot, RandomPlayerbotQuests::BotState& state, bool done)
{
    RandomPlayerbotQuests::CapitalVisit& visit = state.Visit;
    visit.Stage = Step::None;
    visit.Errands.clear();
    visit.Next = 0;
    visit.Arrived = false;
    if (done)
    {
        visit.VisitLevel = NextVisitLevel(bot.getLevel());
        // The world thread picks a quest hub and sends the bot there on a journey.
        state.WantsRelocation = true;
    }
    state.IdleSinceMs = 0;
    RandomPlayerbotQuests::Track(state, RandomPlayerbotQuests::Activity::None);
}

bool StartVisit(Player& bot, RandomPlayerbotQuests::BotState& state, uint32 now)
{
    RandomPlayerbotQuests::CapitalVisit& visit = state.Visit;
    uint32 level = bot.getLevel();
    if (state.Journeying() || level <= RandomPlayerbotLevels::StartZoneLevel || bot.GetMap()->Instanceable() ||
        RandomPlayerbotProgression::InPhasedStart(bot) || bot.GetTransport() || bot.IsInFlight() || bot.IsInCombat())
        return false;
    if (visit.RetryMs && int32(visit.RetryMs - now) > 0)
        return false;
    if (!visit.VisitLevel)
        visit.VisitLevel = FirstVisitLevel(level, bot.GetGUID().GetCounter());
    if (!VisitDue(level, visit.VisitLevel, LowestDurability(bot), BagsFull(bot)))
        return false;
    if (!RoomInCapitals(RandomPlayerbotQuests::CountOf(RandomPlayerbotQuests::Activity::Visiting),
        RandomPlayerbotQuests::ActiveBots()))
    {
        visit.RetryMs = now + NoRoomRetryMs;
        return false;
    }
    RandomPlayerbotQuests::Dismount(bot, state);
    bot.StopMoving();
    RandomPlayerbotProgression::BindInCapital(bot);
    if (bot.CastSpell(&bot, Hearthstone, CastSpellExtraArgs(TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD)) != SPELL_CAST_OK)
    {
        ++FailedHearthCount;
        visit.RetryMs = now + RetryMs;
        return false;
    }
    state.Giver.Clear();
    state.Object.Clear();
    state.ClearSpot();
    visit.Stage = Step::Hearthing;
    visit.StepSinceMs = now ? now : 1;
    RandomPlayerbotQuests::Track(state, RandomPlayerbotQuests::Activity::Visiting);
    ++VisitCount;
    TC_LOG_DEBUG("module.playerbots", "PB-CAPITAL: %s level %u hearths home", bot.GetName().c_str(), level);
    return true;
}

// Walks to the errand's NPC; the NPC once the bot can talk to it.
Creature* Approach(Player& bot, RandomPlayerbotQuests::CapitalVisit::Errand const& errand, bool& gone)
{
    gone = false;
    if (bot.GetExactDist2d(&errand.Pos) > NpcSearchRange)
    {
        if (!bot.isMoving())
            bot.GetMotionMaster()->MovePoint(0, errand.Pos, true);
        return nullptr;
    }
    Creature* npc = bot.FindNearestCreature(errand.Entry, NpcSearchRange);
    if (!npc)
    {
        gone = true;
        return nullptr;
    }
    if (bot.IsWithinDist(npc, TalkRange))
    {
        bot.StopMoving();
        return npc;
    }
    if (!bot.isMoving())
        bot.GetMotionMaster()->MovePoint(0, npc->GetNearPosition(2.0f, npc->GetRelativeAngle(&bot)), true);
    return nullptr;
}

bool RunErrands(WorldSession& session, Player& bot, RandomPlayerbotQuests::BotState& state, uint32 now)
{
    RandomPlayerbotQuests::CapitalVisit& visit = state.Visit;
    if (visit.Next >= visit.Errands.size())
    {
        // Errands done: stroll from one city NPC to the next until it is time to leave.
        if (int32(visit.LeaveMs - now) <= 0)
        {
            EndVisit(bot, state, true);
            TC_LOG_DEBUG("module.playerbots", "PB-CAPITAL: %s sets out again", bot.GetName().c_str());
            return false;
        }
        CityNpc const* npc = AnyBrowse(bot);
        if (!npc)
        {
            visit.LeaveMs = now;
            return true;
        }
        visit.Stage = Step::Browsing;
        visit.Errands.push_back(ErrandAt(*npc));
        visit.Arrived = false;
        visit.StepSinceMs = now ? now : 1;
    }
    RandomPlayerbotQuests::CapitalVisit::Errand const& errand = visit.Errands[visit.Next];
    if (visit.Arrived)
    {
        if (getMSTimeDiff(visit.StepSinceMs, now) >= errand.PauseMs)
        {
            ++visit.Next;
            visit.Arrived = false;
            visit.StepSinceMs = now ? now : 1;
        }
        return true;
    }
    bool gone = false;
    Creature* npc = Approach(bot, errand, gone);
    if (gone || (!npc && getMSTimeDiff(visit.StepSinceMs, now) > ErrandTimeoutMs))
    {
        ++visit.Next;
        visit.StepSinceMs = now ? now : 1;
        return true;
    }
    if (!npc)
        return true;
    bot.SetFacingToObject(npc);
    if (errand.Kind == RepairNpc)
        RepairAndSell(session, bot, *npc);
    else if (errand.Kind == TrainerNpc)
        bot.HandleEmoteCommand(EMOTE_ONESHOT_TALK);
    visit.Arrived = true;
    visit.StepSinceMs = now ? now : 1;
    return true;
}
}

void RandomPlayerbotCapital::BuildIndex()
{
    if (IndexReady)
        return;
    // CreatureData has no zone; the creature table carries it.
    std::unordered_map<uint64, uint32> zones;
    if (QueryResult result = WorldDatabase.PQuery("SELECT guid, zoneId FROM creature WHERE zoneId IN (%u, %u)",
        CapitalZones[0], CapitalZones[1]))
    {
        do
            zones[(*result)[0].GetUInt32()] = (*result)[1].GetUInt16();
        while (result->NextRow());
    }
    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
    {
        auto zone = zones.find(uint64(spawnId));
        if (zone == zones.end() || !(data.phaseId == 0 || data.phaseId == DEFAULT_PHASE) || data.phaseGroup ||
            data.terrainSwapMap != -1)
            continue;
        CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(data.id);
        if (!info)
            continue;
        uint8 kind = 0;
        if ((info->npcflag & UNIT_NPC_FLAG_REPAIR) && (info->npcflag & UNIT_NPC_FLAG_VENDOR))
            kind = RepairNpc;
        else if ((info->npcflag & UNIT_NPC_FLAG_TRAINER_CLASS) && info->trainer_class)
            kind = TrainerNpc;
        else if (info->npcflag & (UNIT_NPC_FLAG_BANKER | UNIT_NPC_FLAG_AUCTIONEER | UNIT_NPC_FLAG_INNKEEPER))
            kind = BrowseNpc;
        uint8 teams = TeamsOf(*info);
        if (!kind || !teams)
            continue;
        CityNpcs.push_back({ kind, uint8(info->trainer_class), teams, uint16(data.mapId), zone->second, data.id,
            data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY(), data.spawnPoint.GetPositionZ() });
    }
    IndexReady = true;
    uint32 counts[4] = {};
    for (CityNpc const& npc : CityNpcs)
        ++counts[npc.Kind];
    TC_LOG_INFO("module.playerbots", "Random playerbots: capitals have %u repairers, %u class trainers, %u bankers, "
        "auctioneers and innkeepers.", counts[RepairNpc], counts[TrainerNpc], counts[BrowseNpc]);
}

bool RandomPlayerbotCapital::Update(WorldSession& session, Player& bot, RandomPlayerbotQuests::BotState& state,
    Position& home, uint32_t now)
{
    if (!IndexReady || CityNpcs.empty())
        return false;
    RandomPlayerbotQuests::CapitalVisit& visit = state.Visit;
    // Starting a visit is checked at a slow pace; a visit under way at a quicker one.
    if (visit.NextCheckMs && int32(visit.NextCheckMs - now) > 0)
        return state.Visiting();
    visit.NextCheckMs = now + (state.Visiting() ? PaceMs : 10 * PaceMs);

    switch (visit.Stage)
    {
        case Step::None:
            return StartVisit(bot, state, now);
        case Step::Hearthing:
            if (bot.GetMapId() == (bot.GetTeamId() == TEAM_ALLIANCE ? 0u : 1u) &&
                bot.GetZoneId() == RandomPlayerbotProgression::CapitalZone(bot))
            {
                visit.Stage = Step::Errands;
                visit.Errands.clear();
                visit.Next = 0;
                visit.Arrived = false;
                for (uint8 kind : { uint8(RepairNpc), uint8(TrainerNpc) })
                    if (CityNpc const* npc = Nearest(bot, kind))
                        visit.Errands.push_back(ErrandAt(*npc));
                visit.StepSinceMs = now ? now : 1;
                visit.LeaveMs = now + urand(StayMinMs, StayMaxMs);
                home = bot.GetPosition();
                return true;
            }
            if (getMSTimeDiff(visit.StepSinceMs, now) > HearthTimeoutMs)
            {
                ++FailedHearthCount;
                EndVisit(bot, state, false);
                visit.RetryMs = now + RetryMs;
                return false;
            }
            return true;
        case Step::Errands:
        case Step::Browsing:
            home = bot.GetPosition();
            return RunErrands(session, bot, state, now);
    }
    return false;
}

void RandomPlayerbotCapital::Abort(RandomPlayerbotQuests::BotState& state)
{
    if (!state.Visiting())
        return;
    state.Visit.Stage = Step::None;
    state.Visit.Errands.clear();
    state.Visit.Next = 0;
    state.Visit.Arrived = false;
    RandomPlayerbotQuests::Track(state, RandomPlayerbotQuests::Activity::None);
}

std::string RandomPlayerbotCapital::Describe()
{
    return "in capitals " + std::to_string(RandomPlayerbotQuests::CountOf(RandomPlayerbotQuests::Activity::Visiting)) +
        ", capital visits " + std::to_string(VisitCount.load()) + ", hearthstones failed " +
        std::to_string(FailedHearthCount.load());
}
