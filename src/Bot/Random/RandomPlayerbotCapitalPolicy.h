/*
 * When random-population bots visit their capital by hearthstone and how long
 * they stay. Pure logic, no core types, so it is unit tested.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_CAPITAL_POLICY_H
#define CATA_RANDOM_PLAYERBOT_CAPITAL_POLICY_H

#include <algorithm>
#include <cstdint>

namespace RandomPlayerbotCapitalPolicy
{
// A bot goes home about every this many levels.
constexpr uint32_t VisitEveryLevels = 5;
// ... or earlier when an equipped item is down to this durability.
constexpr uint32_t WornPercent = 25;
// At most one bot in this many is in a capital at once.
constexpr uint32_t VisitorShare = 5;
// A hearthstone cast that did not bring the bot home by then has failed.
constexpr uint32_t HearthTimeoutMs = 30 * 1000;
// A vendor or trainer the bot cannot reach by then is left out.
constexpr uint32_t ErrandTimeoutMs = 90 * 1000;
// Standing at a vendor or trainer.
constexpr uint32_t ErrandPauseMs = 6 * 1000;
// Time in the city, errands included.
constexpr uint32_t StayMinMs = 2 * 60 * 1000;
constexpr uint32_t StayMaxMs = 4 * 60 * 1000;
// Standing at a banker, auctioneer or innkeeper before walking on.
constexpr uint32_t BrowseMinMs = 20 * 1000;
constexpr uint32_t BrowseMaxMs = 50 * 1000;
// After a failed hearthstone the bot tries again this much later.
constexpr uint32_t RetryMs = 10 * 60 * 1000;

// The level of a bot's first visit, spread over the next few levels by character.
inline uint32_t FirstVisitLevel(uint32_t level, uint32_t guidLow)
{
    return level + 1 + guidLow % VisitEveryLevels;
}

inline uint32_t NextVisitLevel(uint32_t level)
{
    return level + VisitEveryLevels;
}

// wornPercent: lowest durability of an equipped item; bagsFull: no free slot but junk to sell.
inline bool VisitDue(uint32_t level, uint32_t visitLevel, uint32_t wornPercent, bool bagsFull)
{
    return level >= visitLevel || wornPercent <= WornPercent || bagsFull;
}

// visitors: bots in a capital now; bots: random bots out in the world.
inline bool RoomInCapitals(uint32_t visitors, uint32_t bots)
{
    return visitors < std::max<uint32_t>(1, bots / VisitorShare);
}
}

#endif
