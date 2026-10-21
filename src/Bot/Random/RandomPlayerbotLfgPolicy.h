/*
 * Which dungeon-finder roles random-population bots fill for a human queue
 * and which bots fit. Pure logic, no core types, so it is unit tested.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_LFG_POLICY_H
#define CATA_RANDOM_PLAYERBOT_LFG_POLICY_H

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <vector>

namespace RandomPlayerbotLfgPolicy
{
// Dungeon-finder role bits, as the core's PLAYER_ROLE_*.
constexpr uint8_t Tank = 0x02;
constexpr uint8_t Healer = 0x04;
constexpr uint8_t Damage = 0x08;
// A five-player dungeon group.
constexpr uint32_t Tanks = 1, Healers = 1, Dealers = 3;
// Bots this many levels from the human's are taken as they are; others are lifted to the human's level.
constexpr uint32_t LevelSpread = 3;
// The queues are looked at this often.
constexpr uint32_t PaceMs = 2 * 1000;
// A bot the dungeon finder turned away is not asked again for this long.
constexpr uint32_t RefusedRetryMs = 10 * 60 * 1000;
// At most one bot is lifted per queue in this time.
constexpr uint32_t LiftEveryMs = 10 * 1000;
// Bots leave a dungeon group this long after the last human left it, or went offline.
constexpr uint32_t HumanLeftMs = 5 * 1000;
constexpr uint32_t HumanOfflineMs = 2 * 60 * 1000;
// A bot that did not reach its group's dungeon by then is sent in again.
constexpr uint32_t ArriveMs = 15 * 1000;

struct Need
{
    uint32_t Tanks = 0, Healers = 0, Dealers = 0;
    uint32_t Total() const { return Tanks + Healers + Dealers; }
    bool operator==(Need const& other) const
    {
        return Tanks == other.Tanks && Healers == other.Healers && Dealers == other.Dealers;
    }
};

// Roles still open once the humans (each taking one role they chose) and the bots already queued for them are counted.
// Humans with fewer choices go first; among their choices the scarce roles come first.
inline Need Missing(std::vector<uint8_t> humans, std::vector<uint8_t> const& queuedBots)
{
    Need open { Tanks, Healers, Dealers };
    std::stable_sort(humans.begin(), humans.end(), [](uint8_t a, uint8_t b)
    {
        return std::bitset<8>(a & (Tank | Healer | Damage)).count() < std::bitset<8>(b & (Tank | Healer | Damage)).count();
    });
    auto take = [&open](uint8_t roles)
    {
        if ((roles & Tank) && open.Tanks) --open.Tanks;
        else if ((roles & Healer) && open.Healers) --open.Healers;
        else if ((roles & Damage) && open.Dealers) --open.Dealers;
    };
    for (uint8_t roles : humans)
        take(roles);
    for (uint8_t role : queuedBots)
        take(role);
    return open;
}

// The single role to queue the next bot for, or 0 when the group is full.
inline uint8_t NextRole(Need const& open)
{
    return open.Tanks ? Tank : open.Healers ? Healer : open.Dealers ? Damage : 0;
}

inline uint32_t LevelGap(uint32_t a, uint32_t b)
{
    return a > b ? a - b : b - a;
}

// A bot joins as it is when close to the human's level.
inline bool JoinsAsIs(uint32_t botLevel, uint32_t humanLevel)
{
    return LevelGap(botLevel, humanLevel) <= LevelSpread;
}

// Bots are only ever lifted, never lowered.
inline bool Liftable(uint32_t botLevel, uint32_t humanLevel)
{
    return botLevel < humanLevel;
}
}

#endif
