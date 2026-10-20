/*
 * Travel decisions for random-population bots: flight routes over the taxi
 * network and when a flight beats riding. Pure logic, no core types, so it is
 * unit tested. Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_TRAVEL_POLICY_H
#define CATA_RANDOM_PLAYERBOT_TRAVEL_POLICY_H

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
}

#endif
