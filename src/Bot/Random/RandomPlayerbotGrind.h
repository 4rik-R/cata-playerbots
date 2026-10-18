/*
 * Solo grinding for random-population bots, adapted from mod-playerbots
 * GrindTargetValue and NewRpg GoGrind/WanderRandom at d750b4b.
 * See AUTHORS.md and PORTING.md. Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_GRIND_H
#define CATA_RANDOM_PLAYERBOT_GRIND_H

#include <algorithm>
#include <cstdint>

namespace RandomPlayerbotGrind
{
// Pull radius stays inside the session's 35-yard combat leash.
constexpr float PullRange = 30.0f;
// Wandering keeps a bot near where it logged in until travel exists (M5).
constexpr float WanderRadius = 25.0f;
constexpr float HomeLeash = 40.0f;
constexpr uint32_t SearchIntervalMs = 3000;
constexpr uint32_t ReviveDelayMs = 30000;
// Grace for an opener (cast time, travel) before engagement is required.
constexpr uint32_t PullGraceMs = 5000;

// Donor GrindTargetValue: no grey or far-higher targets. Low levels may fight level 1 mobs.
inline bool LevelAllows(uint32_t botLevel, uint32_t targetLevel)
{
    uint32_t low = botLevel > 3 ? botLevel - 3 : 1;
    return targetLevel >= low && targetLevel <= botLevel + 1;
}

// Hysteresis: stop pulling below the low marks, resume only near full.
inline bool ReadyToPull(bool recovering, float healthPct, float manaPct, bool usesMana)
{
    if (recovering)
        return healthPct >= 90.0f && (!usesMana || manaPct >= 80.0f);
    return healthPct >= 60.0f && (!usesMana || manaPct >= 40.0f);
}

// Delay before the next idle wander, 8-20 seconds from a [0, 1) roll.
inline uint32_t WanderDelayMs(float roll)
{
    return 8000 + uint32_t(std::clamp(roll, 0.0f, 0.999f) * 12000.0f);
}
}

#endif
