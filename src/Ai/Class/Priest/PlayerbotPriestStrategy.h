/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_PRIEST_STRATEGY_H
#define PLAYERBOT_PRIEST_STRATEGY_H

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include "../../Base/PlayerbotPartySupport.h"
#include "../../../Bot/Engine/Strategy/Strategy.h"

class Player;
class ObjectGuid;

namespace PlayerbotPriest
{
enum class HealTier { None, Renew, Heal, Emergency };
inline std::vector<NextAction> ResurrectionPrerequisites()
{
    return { NextAction("reach party member to resurrect") };
}
inline bool NeedsResurrectionReach(float distance, float ownerDistance, float range)
{
    return std::isfinite(distance) && std::isfinite(ownerDistance) && std::isfinite(range) &&
        range >= 2.0f && range <= 25.0f && distance > range && distance <= 40.0f &&
        ownerDistance >= 0.0f && ownerDistance <= 20.0f;
}
inline void AddHealingReachTrigger(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("party member to heal out of spell range",
        { NextAction("reach party member to heal", ACTION_CRITICAL_HEAL + 10) }));
}
inline bool CanResurrect(bool corpse, bool requested, bool incomingResurrection)
{
    return corpse && !requested && !incomingResurrection;
}
// Adapted donor duplicate-cast policy with this adapter's critical-health
// threshold. Emergencies and raid healing may deliberately overlap casts.
inline bool DeferToIncomingHeal(float healthPct, bool raid, bool incomingDirectHeal)
{
    return incomingDirectHeal && !raid && healthPct >= 55.0f;
}
inline bool NeedsHealingReach(float healthPct, float distance, float ownerDistance, float range = 30.0f)
{
    return healthPct > 0.0f && healthPct < 80.0f && std::isfinite(distance) &&
        std::isfinite(range) && range >= 2.0f && range <= 30.0f &&
        distance > range && distance <= 40.0f && std::isfinite(ownerDistance) &&
        ownerDistance >= 0.0f && ownerDistance <= 20.0f;
}
inline bool HealingReachMustYield(std::uint32_t elapsedMs, bool nativePointMotion)
{
    return elapsedMs >= 3000 || !nativePointMotion;
}
inline HealTier TierForHealth(float healthPct)
{
    if (healthPct < 55.0f)
        return HealTier::Emergency;
    if (healthPct < 80.0f)
        return HealTier::Heal;
    if (healthPct < 90.0f)
        return HealTier::Renew;
    return HealTier::None;
}

// Candidates are already checked for life, map, range and line of sight.
// This health filter belongs only to healing, never other party support.
// Donor PartyMemberToHeal uses health + distance / 10 within healing range.
// Native candidates are already within 30 yards, so the far-range penalty is
// not needed in this adapter. Cures deliberately retain their separate order.
template <typename Candidate, typename Health, typename Distance, typename Attempt>
bool TryInHealthOrder(std::vector<Candidate>& candidates, Health&& health, Distance&& distance, Attempt&& attempt)
{
    std::erase_if(candidates, [&](Candidate const& candidate)
    {
        float pct = health(candidate);
        float range = distance(candidate);
        return !(pct > 0.0f && pct < 90.0f) || !std::isfinite(range) || range < 0.0f;
    });
    std::stable_sort(candidates.begin(), candidates.end(), [&](Candidate const& left, Candidate const& right)
    {
        return health(left) + distance(left) / 10.0f < health(right) + distance(right) / 10.0f;
    });
    for (Candidate const& candidate : candidates)
        if (attempt(candidate))
            return true;
    return false;
}
template <typename Candidate, typename Health, typename Attempt>
bool TryInHealthOrder(std::vector<Candidate>& candidates, Health&& health, Attempt&& attempt)
{
    return TryInHealthOrder(candidates, std::forward<Health>(health),
        [](Candidate const&) { return 0.0f; }, std::forward<Attempt>(attempt));
}

void LogKnownAbilities(Player const& bot);
std::vector<Player*> HealCandidates(Player& bot, Player* owner);
bool HasIncomingDirectHeal(Player const& bot, Player const& target);
bool HasIncomingResurrection(Player const& bot, Player const& target);
bool ShouldDeferHealing(Player const& bot, Player const& target);
bool ManaAllowsHealing(Player const& bot, Player const& target, std::uint32_t spell);
// Shared with paladin, shaman and druid healers through PlayerbotHealerKit.
bool KnowsHealingSpell(Player const& bot);
ObjectGuid HealingReachTarget(Player& bot, Player& owner, float range = 30.0f);
Player* ResurrectionTarget(Player& bot, float range = 30.0f, Player* owner = nullptr, bool nearOwner = false);
ObjectGuid ResurrectionReachTarget(Player& bot, Player& owner, float range);
bool HealParty(Player& bot, Player* owner);
bool MaintainBuff(Player& bot, Player& owner);
}

#endif
