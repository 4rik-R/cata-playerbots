/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerbotPriestStrategy.h"
#include "Corpse.h"
#include "Group.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotCombatDecision.h"
#include "PlayerbotPartyBuffStrategy.h"
#include "PlayerbotTargetSelection.h"
#include "PlayerbotHealingMana.h"
#include "PlayerbotRoles.h"
#include "../../../Script/PlayerbotConfig.h"
#include "Spell.h"
#include "SpellInfo.h"

void PlayerbotPriest::LogKnownAbilities(Player const& bot)
{
    TC_LOG_INFO("module.playerbots", "PB-HEAL: Priest level=%u Shield=%u Flash Heal=%u Heal=%u Renew=%u",
        bot.getLevel(), bot.HasSpell(17), bot.HasSpell(2061), bot.HasSpell(2050), bot.HasSpell(139));
}

bool PlayerbotPriest::MaintainBuff(Player& bot, Player& owner)
{
    if (bot.getClass() != CLASS_PRIEST)
        return false;
    // Cata's 21562 dummy cast applies 79104 alone or 79105 to the party.
    return PlayerbotDecision::MaintainPartyBuff(bot, owner, PlayerbotPartyBuff::Fortitude);
}

std::vector<Player*> PlayerbotPriest::HealCandidates(Player& bot, Player* owner)
{
    return bot.getClass() == CLASS_PRIEST ? PlayerbotPartySupport::Candidates(bot, owner) : std::vector<Player*>{};
}

namespace
{
template <typename Predicate>
bool HasIncomingPartySpell(Player const& bot, Player const& target, bool includeCorpse, Predicate matches)
{
    Group const* group = bot.GetGroup();
    if (!group || target.GetGroup() != group || !bot.IsInWorld() || !target.IsInWorld() ||
        bot.GetMap() != target.GetMap() || target.IsBeingTeleported())
        return false;
    Corpse* corpse = includeCorpse ? target.GetCorpse() : nullptr;
    ObjectGuid corpseGuid = corpse && corpse->GetMap() == bot.GetMap() ? corpse->GetGUID() : ObjectGuid::Empty;
    for (GroupReference const* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* caster = ref->GetSource();
        if (!caster || caster == &bot || !caster->IsInWorld() || !caster->IsAlive() ||
            caster->GetMap() != bot.GetMap() || caster->IsBeingTeleported())
            continue;
        for (uint32 type = CURRENT_GENERIC_SPELL; type < CURRENT_MAX_SPELL; ++type)
        {
            Spell* spell = caster->GetCurrentSpell(type);
            if (!spell || spell->getState() == SPELL_STATE_FINISHED)
                continue;
            bool targetMatches = spell->m_targets.GetUnitTargetGUID() == target.GetGUID() ||
                (!corpseGuid.IsEmpty() && spell->m_targets.GetCorpseTargetGUID() == corpseGuid);
            SpellInfo const* info = spell->GetSpellInfo();
            if (targetMatches && info && matches(*info))
                return true;
        }
    }
    return false; // No Spell or Player pointer survives this map-thread inspection.
}
}

bool PlayerbotPriest::HasIncomingDirectHeal(Player const& bot, Player const& target)
{
    return HasIncomingPartySpell(bot, target, false, [](SpellInfo const& info)
    {
        return info.HasEffect(SPELL_EFFECT_HEAL) || info.HasEffect(SPELL_EFFECT_HEAL_MAX_HEALTH) ||
            info.HasEffect(SPELL_EFFECT_HEAL_MECHANICAL);
    });
}

bool PlayerbotPriest::HasIncomingResurrection(Player const& bot, Player const& target)
{
    return HasIncomingPartySpell(bot, target, true, [](SpellInfo const& info)
    {
        return info.HasEffect(SPELL_EFFECT_RESURRECT) || info.HasEffect(SPELL_EFFECT_RESURRECT_NEW) ||
            info.HasEffect(SPELL_EFFECT_SELF_RESURRECT);
    });
}

bool PlayerbotPriest::ShouldDeferHealing(Player const& bot, Player const& target)
{
    Group const* group = bot.GetGroup();
    // Cheap policy gates avoid inspecting native casts for emergencies or raids.
    return group && DeferToIncomingHeal(target.GetHealthPct(), group->isRaidGroup(), true) &&
        HasIncomingDirectHeal(bot, target);
}

bool PlayerbotPriest::ManaAllowsHealing(Player const& bot, Player const& target, std::uint32_t spell)
{
    int32 maximum = bot.GetMaxPower(POWER_MANA);
    float mana = maximum > 0 ? 100.0f * bot.GetPower(POWER_MANA) / maximum : 0.0f;
    return PlayerbotHealingMana::Allows(PlayerbotModuleHealerSaveManaEnabled(), mana,
        target.GetHealthPct(), PlayerbotRoles::IsTank(target), PlayerbotHealingMana::ForPriestSpell(spell));
}

Player* PlayerbotPriest::ResurrectionTarget(Player& bot, float range, Player* owner, bool nearOwner)
{
    if (!bot.IsInWorld() || !bot.IsAlive() || bot.IsBeingTeleported() || bot.IsInCombat() ||
        bot.IsNonMeleeSpellCast(false) || !bot.HasSpell(2006) || !std::isfinite(range) || range < 2.0f || range > 40.0f)
        return nullptr;
    Group* group = bot.GetGroup();
    if (!group) return nullptr;
    if (nearOwner && (!owner || !owner->IsInWorld() || owner->GetMap() != bot.GetMap() || owner->GetGroup() != group))
        return nullptr;
    for (Player* member : PlayerbotPartySupport::OrderedPartyMembers(bot, owner))
    {
        if (member && member != &bot && member->IsInWorld() && !member->IsBeingTeleported() &&
            member->GetMap() == bot.GetMap() && bot.IsFriendlyTo(member) &&
            CanResurrect(member->getDeathState() == CORPSE, member->IsResurrectRequested(), false) &&
            bot.IsWithinDistInMap(member, range) && bot.IsWithinLOSInMap(member) &&
            (!nearOwner || owner->IsWithinDistInMap(member, 20.0f)) &&
            !HasIncomingResurrection(bot, *member))
            return member;
    }
    return nullptr; // Map-owned borrowed pointer; callers must not retain it.
}

ObjectGuid PlayerbotPriest::ResurrectionReachTarget(Player& bot, Player& owner, float range)
{
    if (!owner.IsInWorld() || !owner.IsAlive() || owner.IsBeingTeleported() ||
        bot.GetMap() != owner.GetMap() || !bot.GetGroup() || owner.GetGroup() != bot.GetGroup() ||
        !bot.IsWithinDistInMap(&owner, 35.0f) || PlayerbotTargetSelection::HasNearbyPartyCombat(bot, owner))
        return ObjectGuid::Empty;
    // Stabilize selection: never approach a different corpse when one is already
    // inside the configured positioning distance, and do not abandon live patients.
    if (ResurrectionTarget(bot, range, &owner)) return ObjectGuid::Empty;
    for (Player* member : HealCandidates(bot, &owner))
        if (member->GetHealthPct() < 90.0f && !ShouldDeferHealing(bot, *member))
            return ObjectGuid::Empty;
    Player* target = ResurrectionTarget(bot, 40.0f, &owner, true);
    return target && NeedsResurrectionReach(bot.GetDistance(target), owner.GetDistance(target), range) ?
        target->GetGUID() : ObjectGuid::Empty;
}

ObjectGuid PlayerbotPriest::HealingReachTarget(Player& bot, Player& owner, float range)
{
    Group* group = bot.GetGroup();
    if (!group || owner.GetGroup() != group || !bot.IsAlive() || !owner.IsAlive() ||
        bot.GetMap() != owner.GetMap() || !bot.IsWithinDistInMap(&owner, 35.0f) ||
        !(bot.HasSpell(2050) || bot.HasSpell(2061) || bot.HasSpell(139)))
        return ObjectGuid::Empty;
    // Do not abandon an injured member already in native healing range.
    for (Player* member : HealCandidates(bot, &owner))
        if (bot.IsWithinDistInMap(member, range) && member->GetHealthPct() < 90.0f && !ShouldDeferHealing(bot, *member))
            return ObjectGuid::Empty;
    ObjectGuid result;
    float best = 1000.0f;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == &bot || !member->IsInWorld() || !member->IsAlive() ||
            member->IsBeingTeleported() || member->GetMap() != bot.GetMap() || !bot.IsFriendlyTo(member) ||
            !bot.CanSeeOrDetect(member) || !bot.IsWithinLOSInMap(member))
            continue;
        float distance = bot.GetDistance(member);
        if (!NeedsHealingReach(member->GetHealthPct(), distance, owner.GetDistance(member), range) ||
            ShouldDeferHealing(bot, *member))
            continue;
        float score = member->GetHealthPct() + 30.0f; // donor far-range penalty
        if (score < best) { best = score; result = member->GetGUID(); }
    }
    return result;
}

bool PlayerbotPriest::HealParty(Player& bot, Player* owner)
{
    if (bot.IsNonMeleeSpellCast(false))
        return false;

    std::vector<Player*> candidates = HealCandidates(bot, owner);

    // The priority/health-trigger pattern is adapted from Playerbots' Priest
    // heal strategy. All casts still pass through Cata spell and range checks.
    return TryInHealthOrder(candidates, [](Player* member) { return member->GetHealthPct(); },
        [&](Player* member) { return bot.GetExactDist2d(member); }, [&](Player* target)
    {
        float healthPct = target->GetHealthPct();
        if (ShouldDeferHealing(bot, *target))
            return false;
        if (healthPct < 35.0f && bot.HasSpell(17) && ManaAllowsHealing(bot, *target, 17) &&
            !target->HasAura(17) && !target->HasAura(6788) &&
            PlayerbotDecision::TryCast(bot, *target, 17, "Power Word: Shield"))
            return true;

        switch (TierForHealth(healthPct))
        {
            case HealTier::Emergency:
                if (bot.HasSpell(2061) && ManaAllowsHealing(bot, *target, 2061) && PlayerbotDecision::TryCast(bot, *target, 2061, "Flash Heal"))
                    return true;
                [[fallthrough]];
            case HealTier::Heal:
                if (bot.HasSpell(2050) && ManaAllowsHealing(bot, *target, 2050) && PlayerbotDecision::TryCast(bot, *target, 2050, "Heal"))
                    return true;
                [[fallthrough]];
            case HealTier::Renew:
                return bot.HasSpell(139) && ManaAllowsHealing(bot, *target, 139) && !target->HasAura(139, bot.GetGUID()) &&
                    PlayerbotDecision::TryCast(bot, *target, 139, "Renew");
            case HealTier::None:
                return false;
        }
        return false;
    });
}
