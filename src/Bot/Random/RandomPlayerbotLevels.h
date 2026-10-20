/*
 * Level spread and zone rules for random-population bots, adapted from
 * mod-playerbots RandomPlayerbotMgr (RandomTeleportForLevel, level ranges) and
 * PlayerbotFactory at d750b4b. See AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_LEVELS_H
#define CATA_RANDOM_PLAYERBOT_LEVELS_H

#include <algorithm>
#include <array>
#include <cstdint>

namespace RandomPlayerbotLevels
{
constexpr uint32_t MaxLevel = 85;
// Bots this low stay in their race's start zone instead of being moved.
constexpr uint32_t StartZoneLevel = 5;
// Bots below this were never initialized (fresh or from the idle/grind milestones).
constexpr uint32_t InitializedLevel = 6;
// Zones stay valid a little past their range so a level-up does not move a bot at once.
constexpr uint32_t ZoneLevelSlack = 2;
// Death knights skip Acherus (native start 55); Worgen and Goblins skip Gilneas and Kezan.
constexpr uint32_t DeathKnightLevel = 58;
constexpr uint32_t PhasedRaceLevel = 10;
constexpr uint8_t ClassDeathKnight = 6;
constexpr uint8_t RaceGoblin = 9;
constexpr uint8_t RaceWorgen = 22;

enum TeamMask : uint8_t { Alliance = 1, Horde = 2, Both = 3 };

struct Zone
{
    uint32_t Id;
    uint8_t MinLevel;
    uint8_t MaxLevel;
    uint8_t Teams;
};

// Cataclysm 4.3.4 levelling zones (AreaTable zone ids). Phased start zones
// (Gilneas, Kezan, Lost Isles, Acherus) and underwater Vashj'ir are left out.
constexpr std::array<Zone, 62> Zones = {{
    // Start zones
    {12, 1, 10, Alliance}, {1, 1, 10, Alliance}, {141, 1, 10, Alliance}, {3524, 1, 10, Alliance},
    {14, 1, 10, Horde}, {215, 1, 10, Horde}, {85, 1, 10, Horde}, {3430, 1, 10, Horde},
    // 10-20
    {40, 10, 15, Alliance}, {38, 10, 20, Alliance}, {148, 10, 20, Alliance}, {3525, 10, 20, Alliance},
    {44, 15, 20, Alliance}, {16, 10, 20, Horde}, {17, 10, 20, Horde}, {130, 10, 20, Horde},
    {3433, 10, 20, Horde},
    // 20-30
    {10, 20, 25, Alliance}, {11, 20, 25, Alliance}, {267, 20, 25, Horde}, {331, 20, 25, Both},
    {406, 25, 30, Both}, {45, 25, 30, Both}, {33, 25, 30, Both},
    // 30-40
    {4709, 30, 35, Both}, {5287, 30, 35, Both}, {47, 30, 35, Both}, {405, 30, 35, Both},
    {15, 35, 40, Both}, {28, 35, 40, Both}, {357, 35, 40, Both},
    // 40-50
    {139, 40, 45, Both}, {400, 40, 45, Both}, {3, 44, 48, Both}, {440, 45, 50, Both},
    {361, 45, 50, Both}, {51, 47, 51, Both},
    // 50-60
    {46, 49, 52, Both}, {490, 50, 55, Both}, {8, 52, 54, Both}, {618, 50, 55, Both},
    {4, 54, 60, Both}, {1377, 55, 60, Both},
    // Outland
    {3483, 58, 63, Both}, {3521, 60, 64, Both}, {3519, 62, 65, Both}, {3518, 64, 67, Both},
    {3522, 65, 68, Both}, {3523, 67, 70, Both}, {3520, 67, 70, Both},
    // Northrend
    {3537, 68, 72, Both}, {495, 68, 72, Both}, {65, 71, 75, Both}, {394, 73, 75, Both},
    {66, 74, 76, Both}, {3711, 76, 78, Both}, {67, 77, 80, Both}, {210, 77, 80, Both},
    // Cataclysm
    {616, 80, 82, Both}, {5042, 82, 83, Both}, {5034, 83, 85, Both}, {4922, 84, 85, Both},
}};

inline Zone const* FindZone(uint32_t zoneId)
{
    for (Zone const& zone : Zones)
        if (zone.Id == zoneId)
            return &zone;
    return nullptr;
}

// True when the zone is a levelling zone for this team and close enough to its range.
inline bool ZoneFits(uint32_t zoneId, uint32_t level, uint8_t team)
{
    Zone const* zone = FindZone(zoneId);
    return zone && (zone->Teams & team) && level + ZoneLevelSlack >= zone->MinLevel &&
        level <= uint32_t(zone->MaxLevel) + ZoneLevelSlack;
}

inline bool AnyZoneFor(uint32_t level, uint8_t team)
{
    for (Zone const& zone : Zones)
        if ((zone.Teams & team) && level >= zone.MinLevel && level <= zone.MaxLevel)
            return true;
    return false;
}

// Stable per character, so a restart does not re-roll a bot's level.
inline uint32_t TargetLevel(uint32_t guidLow, uint32_t minLevel, uint32_t maxLevel)
{
    minLevel = std::clamp<uint32_t>(minLevel, 1, MaxLevel);
    maxLevel = std::clamp<uint32_t>(maxLevel, minLevel, MaxLevel);
    uint32_t h = guidLow * 2654435761u;
    h ^= h >> 16;
    return minLevel + h % (maxLevel - minLevel + 1);
}

// Bots of these start in a phased area and are moved to their capital once set up.
inline bool StartsPhased(uint8_t race, uint8_t playerClass)
{
    return playerClass == ClassDeathKnight || race == RaceWorgen || race == RaceGoblin;
}

// Lowest level a bot of this race and class is set up at.
inline uint32_t MinimumLevel(uint8_t race, uint8_t playerClass)
{
    if (playerClass == ClassDeathKnight)
        return DeathKnightLevel;
    return race == RaceWorgen || race == RaceGoblin ? PhasedRaceLevel : 1;
}

// TargetLevel within the configured range, raised to the race and class minimum.
inline uint32_t TargetLevelFor(uint32_t guidLow, uint8_t race, uint8_t playerClass, uint32_t minLevel, uint32_t maxLevel)
{
    uint32_t floor = MinimumLevel(race, playerClass);
    minLevel = std::max(minLevel, floor);
    return TargetLevel(guidLow, minLevel, std::max(maxLevel, minLevel));
}

// minimum: MinimumLevel of the bot; a death knight starts at 55, above InitializedLevel.
inline bool NeedsInitialization(uint32_t level, uint32_t target, uint32_t minimum = 1)
{
    return (level < InitializedLevel && level < target) || level < minimum;
}

// Mob levels a bot of this level is sent to: same band the grind target uses.
inline uint32_t MobLevelLow(uint32_t level) { return level > 2 ? level - 2 : 1; }
inline uint32_t MobLevelHigh(uint32_t level) { return std::min(level, MaxLevel + 3); }

// Gear window: items the bot may wear, not older than a few levels.
inline uint32_t GearLevelLow(uint32_t level) { return level > 6 ? level - 6 : 1; }

// Starting gold in copper: enough for repairs, not an economy.
inline uint32_t StartingMoney(uint32_t level) { return level * level * 40; }

// Riding at 20 in Cataclysm; apprentice riding plus one faction mount.
constexpr uint32_t RidingLevel = 20;
}

#endif
