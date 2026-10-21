/*
 * Voyages of random-population bots between continents and landmasses. The
 * donor mod-playerbots teleports its random bots between continents
 * (RandomPlayerbotMgr::RandomTeleport); here they take the ships, zeppelins
 * and portals players use. Released under GNU GPL v2 or any later version.
 */
#include "RandomPlayerbotCrossings.h"
#include "RandomPlayerbotLevels.h"
#include "RandomPlayerbotQuests.h"
#include "RandomPlayerbotTravelPolicy.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GameObjectData.h"
#include "GridDefines.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TerrainMgr.h"
#include "Timer.h"
#include "Transport.h"
#include "TransportMgr.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <optional>
#include <vector>

using namespace RandomPlayerbotTravelPolicy;
using Step = RandomPlayerbotQuests::Voyage::Step;

namespace
{
constexpr uint32 OpenWorldMaps[] = { 0, 1, 530, 571, 646, 732 };
constexpr uint32 AllianceFactionTemplate = 1;
constexpr uint32 HordeFactionTemplate = 2;
// Docks are looked for this far around a ship's stop, at about the height of its deck.
constexpr float LandingRadii[] = { 6.0f, 9.0f, 12.0f, 15.0f, 19.0f, 24.0f, 30.0f, 36.0f };
constexpr uint32 LandingAngles = 24;
constexpr float LandingHeight = 15.0f;
// A portal within its own region this close to its target is a hop inside a city, not a crossing.
constexpr float LocalPortalRange = 1000.0f;
// Within this the ride to a dock is over and the bot walks up; within that it is there.
constexpr float ApproachRange = 40.0f;
constexpr float DockRange = 4.0f;
// Portals are stepped into from a little further: a walk-in portal's centre may hang in the air.
constexpr float PortalRange = 10.0f;
constexpr uint32 ApproachTimeoutMs = 2 * 60 * 1000;
constexpr uint32 ThroughTimeoutMs = 30 * 1000;
constexpr float ArrivalRange = 80.0f;
// Bots board until this long before departure and leave from this long after arrival.
constexpr uint32 BoardMarginMs = 5 * 1000;
constexpr uint32 LeaveMarginMs = 3 * 1000;
constexpr uint32 PaceMs = 500;

enum Kind : uint8
{
    Ship = 1, // ships and zeppelins
    Portal,   // a portal object that casts a teleport
    WalkIn,   // a walk-in portal (area trigger)
};

struct Place
{
    uint32 Map;
    float X, Y, Z;
    Spot AsSpot() const { return { Map, X, Y }; }
};

struct CrossingData
{
    uint8 Kind;
    uint8 Teams;
    Place From, To;
    float ToO;
    ObjectGuid Transport;
    uint32 DepartAt, DepartDelay, ArriveAt, ArriveDelay;
    uint32 Spell;
};
std::vector<CrossingData> All;
// Per faction (0 Alliance, 1 Horde): the crossings it may take, for the planner, and their index into All.
std::vector<Crossing> Plannable[2];
std::vector<uint32> PlannableIndex[2];
bool IndexReady = false;
std::atomic<uint32> VoyageCount { 0 }, ShipCount { 0 }, PortalCount { 0 }, FailedVoyageCount { 0 };

bool OpenWorld(uint32 map)
{
    return std::find(std::begin(OpenWorldMaps), std::end(OpenWorldMaps), map) != std::end(OpenWorldMaps);
}

uint8 TeamIndex(Player const& bot)
{
    return bot.GetTeamId() == TEAM_ALLIANCE ? 0 : 1;
}

// Factions whose members may stand at a place: not in a town or city of the other faction.
uint8 TeamsAt(Place const& place)
{
    std::shared_ptr<TerrainInfo> terrain = sTerrainMgr.LoadTerrain(place.Map);
    if (!terrain)
        return 0;
    uint32 zoneId = 0, areaId = 0;
    terrain->GetZoneAndAreaId(PhasingHandler::GetEmptyPhaseShift(), place.Map, zoneId, areaId, place.X, place.Y, place.Z);
    uint8 teams = RandomPlayerbotLevels::Alliance | RandomPlayerbotLevels::Horde;
    for (uint32 id : { areaId, zoneId })
    {
        AreaTableEntry const* area = sAreaTableStore.LookupEntry(id);
        if (!area)
            continue;
        for (auto [factionTemplate, team] : { std::pair<uint32, uint8>(AllianceFactionTemplate, RandomPlayerbotLevels::Alliance),
            std::pair<uint32, uint8>(HordeFactionTemplate, RandomPlayerbotLevels::Horde) })
            if (FactionTemplateEntry const* own = sFactionTemplateStore.LookupEntry(factionTemplate);
                own && (area->FactionGroupMask & own->EnemyGroup) && !(area->FactionGroupMask & own->FriendGroup))
                teams &= ~team;
    }
    return teams;
}

// Dry ground at about deck height next to a ship's stop: the pier or the tower platform.
std::optional<Place> FindLanding(uint32 map, Position const& stop)
{
    std::shared_ptr<TerrainInfo> terrain = sTerrainMgr.LoadTerrain(map);
    if (!terrain)
        return {};
    PhaseShift const& phases = PhasingHandler::GetEmptyPhaseShift();
    for (float radius : LandingRadii)
        for (uint32 step = 0; step < LandingAngles; ++step)
        {
            float angle = 2.0f * float(M_PI) * float(step) / float(LandingAngles);
            float x = stop.GetPositionX() + radius * std::cos(angle), y = stop.GetPositionY() + radius * std::sin(angle);
            float ground = terrain->GetStaticHeight(phases, map, x, y, stop.GetPositionZ() + LandingHeight, true,
                2.0f * LandingHeight);
            if (ground <= INVALID_HEIGHT || std::fabs(ground - stop.GetPositionZ()) > LandingHeight ||
                terrain->IsInWater(phases, map, x, y, ground + 0.5f))
                continue;
            return Place { map, x, y, ground };
        }
    return {};
}

void AddCrossing(CrossingData crossing)
{
    crossing.Teams &= TeamsAt(crossing.From) & TeamsAt(crossing.To);
    if (!crossing.Teams)
        return;
    float cost = crossing.Kind == Ship ? ShipCost : PortalCost;
    uint32 index = uint32(All.size());
    All.push_back(crossing);
    for (uint8 team : { uint8(0), uint8(1) })
        if (crossing.Teams & (team == 0 ? RandomPlayerbotLevels::Alliance : RandomPlayerbotLevels::Horde))
        {
            Plannable[team].push_back({ crossing.From.AsSpot(), crossing.To.AsSpot(), cost });
            PlannableIndex[team].push_back(index);
        }
}

void IndexShips()
{
    QueryResult result = WorldDatabase.Query("SELECT guid, entry, phaseid, phasegroup FROM transports");
    if (!result)
        return;
    do
    {
        Field* fields = result->Fetch();
        uint32 guid = fields[0].GetUInt32(), entry = fields[1].GetUInt32();
        TransportTemplate const* info = sTransportMgr->GetTransportTemplate(entry);
        if (!info || info->InInstance || fields[2].GetInt32() || fields[3].GetInt32() || !info->TotalPathTime)
            continue;
        struct Stop { Place Land; uint32 Arrival, Delay; };
        std::vector<Stop> stops;
        bool open = true;
        for (TransportPathLeg const& leg : info->PathLegs)
        {
            open &= OpenWorld(leg.MapId);
            for (std::size_t i = 0; i + 1 < leg.Segments.size(); ++i)
            {
                TransportPathSegment const& segment = leg.Segments[i];
                if (!segment.Delay)
                    continue;
                Optional<Position> at = info->ComputePosition(segment.SegmentEndArrivalTimestamp, nullptr, nullptr);
                std::optional<Place> land = at ? FindLanding(leg.MapId, *at) : std::nullopt;
                if (land)
                    stops.push_back({ *land, segment.SegmentEndArrivalTimestamp, segment.Delay });
                else
                    open = false; // a stop without a dock (Vashj'ir, gunships) is no line for bots
            }
        }
        if (!open || stops.size() < 2)
            continue;
        for (std::size_t i = 0; i < stops.size(); ++i)
        {
            Stop const& from = stops[i];
            Stop const& to = stops[(i + 1) % stops.size()];
            CrossingData crossing {};
            crossing.Kind = Ship;
            crossing.Teams = RandomPlayerbotLevels::Alliance | RandomPlayerbotLevels::Horde;
            crossing.From = from.Land;
            crossing.To = to.Land;
            crossing.Transport = ObjectGuid(HighGuid::Mo_Transport, 0, guid);
            crossing.DepartAt = from.Arrival;
            crossing.DepartDelay = from.Delay;
            crossing.ArriveAt = to.Arrival;
            crossing.ArriveDelay = to.Delay;
            AddCrossing(crossing);
        }
    } while (result->NextRow());
}

void IndexPortals()
{
    for (auto const& [spawnId, data] : sObjectMgr->GetAllGameObjectData())
    {
        GameObjectTemplate const* info = sObjectMgr->GetGameObjectTemplate(data.id);
        if (!info || info->type != GAMEOBJECT_TYPE_SPELLCASTER || !OpenWorld(data.mapId) ||
            !(data.phaseId == 0 || data.phaseId == DEFAULT_PHASE) || data.phaseGroup || data.terrainSwapMap != -1)
            continue;
        SpellInfo const* spell = sSpellMgr->GetSpellInfo(info->spellcaster.spell);
        if (!spell)
            continue;
        for (uint8 index = 0; index < MAX_SPELL_EFFECTS; ++index)
        {
            if (spell->Effects[index].Effect != SPELL_EFFECT_TELEPORT_UNITS)
                continue;
            SpellTargetPosition const* target = sSpellMgr->GetSpellTargetPosition(spell->Id, SpellEffIndex(index));
            if (!target || !OpenWorld(target->target_mapId))
                break;
            CrossingData crossing {};
            crossing.Kind = Portal;
            crossing.Teams = RandomPlayerbotLevels::Alliance | RandomPlayerbotLevels::Horde;
            crossing.From = { data.mapId, data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY(),
                data.spawnPoint.GetPositionZ() };
            crossing.To = { target->target_mapId, target->target_X, target->target_Y, target->target_Z };
            crossing.ToO = target->target_Orientation;
            crossing.Spell = spell->Id;
            if (!SameRegion(crossing.From.AsSpot(), crossing.To.AsSpot()) ||
                Distance(crossing.From.AsSpot(), crossing.To.AsSpot()) > LocalPortalRange)
                AddCrossing(crossing);
            break;
        }
    }
}

void IndexWalkIns()
{
    QueryResult result = WorldDatabase.Query("SELECT ID, target_map, target_position_x, target_position_y, "
        "target_position_z, target_orientation FROM areatrigger_teleport");
    if (!result)
        return;
    do
    {
        Field* fields = result->Fetch();
        AreaTriggerEntry const* trigger = sAreaTriggerStore.LookupEntry(fields[0].GetUInt32());
        uint32 map = fields[1].GetUInt16();
        // Only those between open-world maps: the Dark Portal, the Ghostlands and the Plaguelands.
        if (!trigger || !OpenWorld(trigger->ContinentID) || !OpenWorld(map) || trigger->ContinentID == map)
            continue;
        CrossingData crossing {};
        crossing.Kind = WalkIn;
        crossing.Teams = RandomPlayerbotLevels::Alliance | RandomPlayerbotLevels::Horde;
        crossing.From = { trigger->ContinentID, trigger->Pos.X, trigger->Pos.Y, trigger->Pos.Z };
        crossing.To = { map, fields[2].GetFloat(), fields[3].GetFloat(), fields[4].GetFloat() };
        crossing.ToO = fields[5].GetFloat();
        AddCrossing(crossing);
    } while (result->NextRow());
}

CrossingData const& Current(RandomPlayerbotQuests::Voyage const& voyage)
{
    return All[voyage.Crossings[voyage.Next]];
}

void LeaveTransport(Player& bot)
{
    if (TransportBase* transport = bot.GetTransport())
        transport->RemovePassenger(&bot);
    bot.ResummonPetTemporaryUnSummonedIfAny();
}

// Ends the voyage with a teleport to its goal, as a failed journey does.
void Fail(Player& bot, RandomPlayerbotQuests::BotState& state, char const* reason)
{
    RandomPlayerbotQuests::Voyage voyage = std::move(state.Sea);
    state.Sea = {};
    LeaveTransport(bot);
    bot.StopMoving();
    ++FailedVoyageCount;
    RandomPlayerbotQuests::Track(state, RandomPlayerbotQuests::Activity::None);
    state.Trip = {};
    state.Trip.Map = voyage.GoalMap;
    state.Trip.Goal = voyage.Goal;
    state.TripFailed = true;
    TC_LOG_DEBUG("module.playerbots", "PB-TRAVEL: %s gives its voyage up: %s", bot.GetName().c_str(), reason);
}

// One crossing done: on to the next dock, or on a journey to the goal.
void Arrive(Player& bot, RandomPlayerbotQuests::BotState& state, uint32 now)
{
    RandomPlayerbotQuests::Voyage& voyage = state.Sea;
    if (++voyage.Next < voyage.Crossings.size())
    {
        voyage.Stage = Step::ToDock;
        voyage.StepSinceMs = now ? now : 1;
        return;
    }
    uint32 goalMap = voyage.GoalMap;
    Position goal = voyage.Goal;
    if (bot.GetMapId() != goalMap)
    {
        Fail(bot, state, "landed on the wrong map");
        return;
    }
    state.Sea = {};
    RandomPlayerbotQuests::Track(state, RandomPlayerbotQuests::Activity::None);
    if (!RandomPlayerbotQuests::SetOut(bot, state, goalMap, goal))
    {
        state.Trip = {};
        state.Trip.Map = goalMap;
        state.Trip.Goal = goal;
        state.TripFailed = true;
    }
}

void Board(Player& bot, RandomPlayerbotQuests::BotState& state, Transport& transport, CrossingData const& crossing, uint32 now)
{
    bot.UnsummonPetTemporaryIfAny();
    RandomPlayerbotQuests::Dismount(bot, state);
    if (bot.IsMounted())
        bot.RemoveAurasByType(SPELL_AURA_MOUNTED);
    bot.StopMoving();
    // Amidships, at the height of the deck where the transport has a model, else of the pier.
    float x = transport.GetPositionX(), y = transport.GetPositionY(), z = crossing.From.Z;
    float deck = bot.GetMap()->GetHeight(bot.GetPhaseShift(), x, y, z + LandingHeight, true, 2.0f * LandingHeight);
    if (deck > INVALID_HEIGHT && std::fabs(deck - z) < LandingHeight)
        z = deck;
    float o = transport.GetOrientation();
    float offsetX = x, offsetY = y, offsetZ = z + 0.5f, offsetO = o;
    transport.CalculatePassengerOffset(offsetX, offsetY, offsetZ, &offsetO);
    transport.AddPassenger(&bot);
    bot.m_movementInfo.transport.pos.Relocate(offsetX, offsetY, offsetZ, offsetO);
    bot.TeleportTo(bot.GetMapId(), x, y, z + 0.5f, o, TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_UNSUMMON_PET);
    state.Sea.Stage = Step::Aboard;
    state.Sea.StepSinceMs = now ? now : 1;
    ++ShipCount;
    TC_LOG_DEBUG("module.playerbots", "PB-TRAVEL: %s boards %s", bot.GetName().c_str(), transport.GetName().c_str());
}

void Disembark(Player& bot, RandomPlayerbotQuests::BotState& state, CrossingData const& crossing, uint32 now)
{
    if (TransportBase* transport = bot.GetTransport())
        transport->RemovePassenger(&bot);
    bot.TeleportTo(crossing.To.Map, crossing.To.X, crossing.To.Y, crossing.To.Z + 0.5f, bot.GetOrientation());
    bot.ResummonPetTemporaryUnSummonedIfAny();
    Arrive(bot, state, now);
}

uint32 TimerOf(Transport const& transport)
{
    uint32 period = transport.GetTransportPeriod();
    return period ? transport.GetTimer() % period : 0;
}

void StepThrough(Player& bot, RandomPlayerbotQuests::BotState& state, CrossingData const& crossing, uint32 now)
{
    RandomPlayerbotQuests::Dismount(bot, state);
    bot.StopMoving();
    bool gone = crossing.Kind == Portal ?
        bot.CastSpell(&bot, crossing.Spell, CastSpellExtraArgs(TRIGGERED_FULL_MASK)) == SPELL_CAST_OK :
        bot.TeleportTo(crossing.To.Map, crossing.To.X, crossing.To.Y, crossing.To.Z, crossing.ToO);
    if (!gone)
    {
        Fail(bot, state, "portal refused");
        return;
    }
    ++PortalCount;
    state.Sea.Stage = Step::Through;
    state.Sea.StepSinceMs = now ? now : 1;
}
}

void RandomPlayerbotCrossings::BuildIndex()
{
    if (IndexReady)
        return;
    uint32 started = getMSTime();
    IndexShips();
    uint32 ships = uint32(All.size());
    IndexPortals();
    uint32 portals = uint32(All.size()) - ships;
    IndexWalkIns();
    IndexReady = true;
    TC_LOG_INFO("module.playerbots", "Random playerbots: %u ship and zeppelin crossings, %u portals, %u walk-in portals "
        "in %u ms.", ships, portals, uint32(All.size()) - ships - portals, getMSTimeDiff(started, getMSTime()));
}

bool RandomPlayerbotCrossings::PlanVoyage(Player& bot, RandomPlayerbotQuests::BotState& state, uint32_t map,
    Position const& goal)
{
    if (!IndexReady || !OpenWorld(bot.GetMapId()) || !OpenWorld(map) || bot.GetTransport() || bot.IsInFlight())
        return false;
    uint8 team = TeamIndex(bot);
    std::vector<uint32_t> chain = PlanCrossings(Plannable[team], { bot.GetMapId(), bot.GetPositionX(), bot.GetPositionY() },
        { map, goal.GetPositionX(), goal.GetPositionY() });
    if (chain.empty())
        return false;
    RandomPlayerbotQuests::Voyage voyage;
    voyage.Stage = Step::ToDock;
    for (uint32 index : chain)
        voyage.Crossings.push_back(PlannableIndex[team][index]);
    voyage.GoalMap = map;
    voyage.Goal = goal;
    voyage.StartedMs = getMSTime();
    voyage.StepSinceMs = voyage.StartedMs;
    state.Sea = std::move(voyage);
    state.Trip = {};
    state.TripFailed = false;
    state.Giver.Clear();
    state.Object.Clear();
    state.ClearSpot();
    ++VoyageCount;
    TC_LOG_DEBUG("module.playerbots", "PB-TRAVEL: %s sets out on a voyage of %u crossings to map %u", bot.GetName().c_str(),
        uint32(chain.size()), map);
    return true;
}

bool RandomPlayerbotCrossings::Update(Player& bot, RandomPlayerbotQuests::BotState& state, uint32_t now)
{
    RandomPlayerbotQuests::Voyage& voyage = state.Sea;
    if (voyage.Stage == Step::None)
        return false;
    // The ride to a dock is a journey; the quest update runs it.
    if (state.Journeying() || state.TripFailed)
        return false;
    if (voyage.NextCheckMs && int32(voyage.NextCheckMs - now) > 0)
        return true;
    voyage.NextCheckMs = now + PaceMs;
    if (bot.IsBeingTeleported())
        return true;
    if (getMSTimeDiff(voyage.StartedMs, now) > VoyageTimeoutMs)
    {
        Fail(bot, state, "too long");
        return true;
    }
    RandomPlayerbotQuests::Track(state, RandomPlayerbotQuests::Activity::Sailing);
    CrossingData const& crossing = Current(voyage);
    uint32 inStep = getMSTimeDiff(voyage.StepSinceMs, now);
    switch (voyage.Stage)
    {
        case Step::ToDock:
        {
            Spot here { bot.GetMapId(), bot.GetPositionX(), bot.GetPositionY() };
            if (!SameRegion(here, crossing.From.AsSpot()))
            {
                Fail(bot, state, "not where the crossing starts");
                return true;
            }
            float distance = bot.GetExactDist2d(crossing.From.X, crossing.From.Y);
            if (distance > ApproachRange)
            {
                if (!RandomPlayerbotQuests::SetOut(bot, state, crossing.From.Map,
                    Position(crossing.From.X, crossing.From.Y, crossing.From.Z)))
                    Fail(bot, state, "no way to the dock");
                voyage.StepSinceMs = now ? now : 1;
                return true;
            }
            if (distance > (crossing.Kind == Ship ? DockRange : PortalRange))
            {
                if (inStep > ApproachTimeoutMs)
                    Fail(bot, state, "dock out of reach");
                else if (!bot.isMoving())
                    bot.GetMotionMaster()->MovePoint(0, crossing.From.X, crossing.From.Y, crossing.From.Z, true);
                return true;
            }
            bot.StopMoving();
            if (crossing.Kind == Ship)
            {
                voyage.Stage = Step::Waiting;
                voyage.StepSinceMs = now ? now : 1;
            }
            else
                StepThrough(bot, state, crossing, now);
            return true;
        }
        case Step::Waiting:
        {
            Transport* transport = bot.GetMap()->GetTransport(crossing.Transport);
            if (transport && AtStop(TimerOf(*transport), crossing.DepartAt, crossing.DepartDelay, BoardMarginMs))
                Board(bot, state, *transport, crossing, now);
            else if (inStep > ShipWaitMs)
                Fail(bot, state, "no ship came");
            return true;
        }
        case Step::Aboard:
        {
            Transport* transport = bot.GetMap()->GetTransport(crossing.Transport);
            if (!transport || bot.GetTransport() != transport)
            {
                if (inStep > ShipRideMs)
                    Fail(bot, state, "lost the ship");
                return true;
            }
            if (bot.GetMapId() == crossing.To.Map &&
                AtStop(TimerOf(*transport), crossing.ArriveAt, crossing.ArriveDelay, LeaveMarginMs))
                Disembark(bot, state, crossing, now);
            else if (inStep > ShipRideMs)
                Fail(bot, state, "the ship did not arrive");
            return true;
        }
        case Step::Through:
            if (bot.GetMapId() == crossing.To.Map && bot.GetExactDist2d(crossing.To.X, crossing.To.Y) < ArrivalRange)
                Arrive(bot, state, now);
            else if (inStep > ThroughTimeoutMs)
                Fail(bot, state, "the portal led elsewhere");
            return true;
        case Step::None:
            break;
    }
    return false;
}

void RandomPlayerbotCrossings::Abort(Player& bot, RandomPlayerbotQuests::BotState& state)
{
    if (!state.Voyaging())
        return;
    state.Sea = {};
    LeaveTransport(bot);
    RandomPlayerbotQuests::Track(state, RandomPlayerbotQuests::Activity::None);
}

std::string RandomPlayerbotCrossings::Describe()
{
    return "sailing " + std::to_string(RandomPlayerbotQuests::CountOf(RandomPlayerbotQuests::Activity::Sailing)) +
        ", voyages " + std::to_string(VoyageCount.load()) + ", ship rides " + std::to_string(ShipCount.load()) +
        ", portals " + std::to_string(PortalCount.load()) + ", voyages failed " + std::to_string(FailedVoyageCount.load()) +
        ", crossings known " + std::to_string(All.size());
}
