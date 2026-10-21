/*
 * Travel decisions for random-population bots: flight routes over the taxi
 * network and when a flight beats riding. Pure logic, no core types, so it is
 * unit tested. Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_TRAVEL_POLICY_H
#define CATA_RANDOM_PLAYERBOT_TRAVEL_POLICY_H

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

namespace RandomPlayerbotTravelPolicy
{
// Closer goals are ridden to directly.
constexpr float RideOnlyRange = 600.0f;
// A flight has to save at least this much riding to be worth the detour to a flight master.
constexpr float FlightSaving = 300.0f;
// Without a flight, goals further than this are teleported to.
constexpr float MaxRideRange = 3000.0f;
// A flight master counts for the taxi node it stands this close to.
constexpr float MasterNodeRange = 60.0f;
// A journey that takes longer is given up and finished by teleport.
constexpr uint32_t JourneyTimeoutMs = 20 * 60 * 1000;
// A journey stuck this often (no headway for half a minute each) is finished by teleport.
constexpr uint32_t MaxJourneyStalls = 4;

struct TaxiLeg
{
    uint32_t To;
    float Length;
    uint32_t Cost;
};
using TaxiLegs = std::unordered_map<uint32_t, std::vector<TaxiLeg>>;

struct Route
{
    std::vector<uint32_t> Nodes; // from first to last; empty when there is none
    uint32_t Cost = 0;
};

// Shortest route by flight length between two taxi nodes.
inline Route FindRoute(TaxiLegs const& legs, uint32_t from, uint32_t to)
{
    Route route;
    if (from == to)
        return route;
    using Entry = std::pair<float, uint32_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
    std::unordered_map<uint32_t, float> best;
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> previous; // node -> (node before, leg cost)
    best[from] = 0.0f;
    open.push({ 0.0f, from });
    while (!open.empty())
    {
        auto [length, node] = open.top();
        open.pop();
        if (length > best[node])
            continue;
        if (node == to)
            break;
        auto found = legs.find(node);
        if (found == legs.end())
            continue;
        for (TaxiLeg const& leg : found->second)
        {
            float next = length + leg.Length;
            auto known = best.find(leg.To);
            if (known != best.end() && known->second <= next)
                continue;
            best[leg.To] = next;
            previous[leg.To] = { node, leg.Cost };
            open.push({ next, leg.To });
        }
    }
    if (!previous.count(to))
        return route;
    for (uint32_t node = to; node != from; node = previous[node].first)
    {
        route.Nodes.push_back(node);
        route.Cost += previous[node].second;
    }
    route.Nodes.push_back(from);
    return { std::vector<uint32_t>(route.Nodes.rbegin(), route.Nodes.rend()), route.Cost };
}

// Fly when the goal is far and riding to and from the flight masters saves enough.
inline bool WorthFlying(float direct, float toMaster, float fromLanding)
{
    return direct > RideOnlyRange && toMaster + fromLanding + FlightSaving < direct;
}

// Map 530 holds three landmasses: Outland, the draenei isles and the blood elf lands.
enum Region : uint8_t
{
    Mainland = 0,
    DraeneiIsles = 1,
    BloodElfLands = 2,
};

// The landmass of a point: places in one region are reached by riding and flying.
inline uint8_t RegionOf(uint32_t map, float x, float y)
{
    if (map != 530)
        return Mainland;
    if (y < -9000.0f)
        return DraeneiIsles;
    return x > 6000.0f ? BloodElfLands : Mainland;
}

// A ship, zeppelin, portal or walk-in portal from one region to another.
constexpr float ShipCost = 900.0f;
constexpr float PortalCost = 100.0f;
// A voyage takes at most this many crossings.
constexpr uint32_t MaxCrossings = 4;
// Waiting for a ship, riding it and the whole voyage are given up after these.
constexpr uint32_t ShipWaitMs = 20 * 60 * 1000;
constexpr uint32_t ShipRideMs = 20 * 60 * 1000;
constexpr uint32_t VoyageTimeoutMs = 60 * 60 * 1000;

struct Spot
{
    uint32_t Map = 0;
    float X = 0.0f, Y = 0.0f;
    uint8_t Region() const { return RegionOf(Map, X, Y); }
};

struct Crossing
{
    Spot From, To;
    float Cost = 0.0f;
};

inline bool SameRegion(Spot const& a, Spot const& b)
{
    return a.Map == b.Map && a.Region() == b.Region();
}

inline float Distance(Spot const& a, Spot const& b)
{
    return std::hypot(a.X - b.X, a.Y - b.Y);
}

// Cheapest chain of crossings from start to a goal in another region, by distance
// travelled in each region plus each crossing's cost; empty when there is none.
inline std::vector<uint32_t> PlanCrossings(std::vector<Crossing> const& crossings, Spot const& start, Spot const& goal)
{
    if (SameRegion(start, goal))
        return {};
    constexpr float Unreached = std::numeric_limits<float>::max();
    std::vector<float> best(crossings.size(), Unreached);
    std::vector<uint32_t> hops(crossings.size(), 0), previous(crossings.size(), UINT32_MAX);
    using Entry = std::pair<float, uint32_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
    for (uint32_t i = 0; i < crossings.size(); ++i)
        if (SameRegion(start, crossings[i].From))
        {
            best[i] = Distance(start, crossings[i].From) + crossings[i].Cost;
            hops[i] = 1;
            open.push({ best[i], i });
        }
    float bestTotal = Unreached;
    uint32_t last = UINT32_MAX;
    while (!open.empty())
    {
        auto [cost, index] = open.top();
        open.pop();
        if (cost > best[index] || cost >= bestTotal)
            continue;
        Crossing const& here = crossings[index];
        if (SameRegion(here.To, goal))
        {
            float total = cost + Distance(here.To, goal);
            if (total < bestTotal)
            {
                bestTotal = total;
                last = index;
            }
            continue;
        }
        if (hops[index] >= MaxCrossings)
            continue;
        for (uint32_t next = 0; next < crossings.size(); ++next)
        {
            if (!SameRegion(here.To, crossings[next].From))
                continue;
            float through = cost + Distance(here.To, crossings[next].From) + crossings[next].Cost;
            if (through >= best[next])
                continue;
            best[next] = through;
            hops[next] = hops[index] + 1;
            previous[next] = index;
            open.push({ through, next });
        }
    }
    std::vector<uint32_t> chain;
    for (uint32_t index = last; index != UINT32_MAX; index = previous[index])
        chain.insert(chain.begin(), index);
    return chain;
}

// True while a transport waits at a stop: the window opens on arrival, minus a margin at its end.
inline bool AtStop(uint32_t timer, uint32_t arrival, uint32_t delay, uint32_t margin)
{
    return delay > margin && timer >= arrival && timer < arrival + delay - margin;
}
}

#endif
