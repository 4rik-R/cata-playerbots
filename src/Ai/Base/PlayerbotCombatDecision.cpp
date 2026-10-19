/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerbotCombatDecision.h"
#include "PlayerbotPartySupport.h"
#include "PlayerbotRoles.h"
#include "Creature.h"
#include "Group.h"
#include "Log.h"
#include "Player.h"
#include "Spell.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "SpellAuras.h"
#include "../../Bot/ForceRebuff.h"
#include "Timer.h"

namespace
{
bool TryCastChecked(Player& bot, Unit& target, std::uint32_t spellId, char const* name, bool auraRefresh)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || !bot.HasSpell(spellId))
        return false;
    // Match native pending casts: authorize the learned base spell first,
    // then resolve any current action-bar override and native cost flags.
    TriggerCastFlags triggerFlags = TRIGGERED_NONE;
    spellInfo = bot.GetCastSpellInfo(spellInfo, triggerFlags);
    if (!spellInfo || bot.GetSpellHistory()->HasGlobalCooldown(spellInfo) ||
        bot.GetSpellHistory()->HasCooldown(spellInfo) || !bot.CanRequestSpellCast(spellInfo))
        return false;

    Spell* spell = new Spell(&bot, spellInfo, triggerFlags);
    bool valid = auraRefresh ? spell->CheckPetCast(&target) == SPELL_CAST_OK : spell->CanAutoCast(&target);
    if (!valid)
    {
        delete spell;
        return false;
    }

    SpellCastTargets targets;
    targets.SetUnitTarget(&target);
    std::string targetName = target.GetName();
    if (spell->prepare(targets) == SPELL_CAST_OK)
    {
        TC_LOG_INFO("module.playerbots", "PB-02: %s began %s on %s", bot.GetName().c_str(), name, targetName.c_str());
        return true;
    }
    return false;
}
}
bool PlayerbotDecision::TryCast(Player& bot, Unit& target, std::uint32_t spellId, char const* name)
{
    return TryCastChecked(bot, target, spellId, name, false);
}

bool PlayerbotDecision::ExecuteFirstAvailable(Player& bot, Creature& target, std::span<Action const> actions)
{
    auto eligible = [&bot, &target](Action const& action)
    {
        return action.Triggered(bot, target) && bot.HasSpell(action.SpellId);
    };
    auto attempt = [&bot, &target](Action const& action)
    {
        Unit& castTarget = action.Target == ActionTarget::Self ? static_cast<Unit&>(bot) : static_cast<Unit&>(target);
        return TryCast(bot, castTarget, action.SpellId, action.Name);
    };

    return ExecuteByPriority(actions, eligible, attempt);
}

std::vector<Player*> PlayerbotPartySupport::OrderedPartyMembers(Player& bot, Player* owner)
{
    std::vector<Player*> members;
    Group* group = bot.GetGroup();
    if (!bot.IsInWorld() || !group) return members;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsInWorld() && !member->IsBeingTeleported() &&
            member->GetMap() == bot.GetMap() && member->GetGroup() == group &&
            bot.IsFriendlyTo(member) && !member->IsGameMaster() && !member->IsCharmed())
            members.push_back(member);
    }
    auto role = [owner](Player* member)
    {
        using PlayerbotPartySupport::Role;
        if (member == owner) return Role::Controller;
        if (PlayerbotRoles::IsHealer(*member)) return Role::Healer;
        if (PlayerbotRoles::IsTank(*member)) return Role::Tank;
        return Role::Other;
    };
    OrderCandidates(members, role, [&](Player* member)
    {
        return group->GetMemberGroup(member->GetGUID()) == group->GetMemberGroup(bot.GetGUID());
    });
    return members; // Borrowed map-thread pointers, never retained by a value/queue.
}

std::vector<Player*> PlayerbotPartySupport::Candidates(Player& bot, Player* owner)
{
    std::vector<Player*> candidates;
    if (!bot.IsInWorld() || !bot.IsAlive() || bot.IsBeingTeleported())
        return candidates;
    auto consider = [&](Player* member)
    {
        if (!member || !member->IsInWorld() || !member->IsAlive() || member->IsBeingTeleported() ||
            member->IsGameMaster() || member->IsCharmed() || !bot.IsFriendlyTo(member) || member->GetMap() != bot.GetMap() ||
            (member != &bot && (!bot.IsWithinDistInMap(member, 30.0f) || !bot.IsWithinLOSInMap(member))))
            return;
        if (std::find(candidates.begin(), candidates.end(), member) == candidates.end())
            candidates.push_back(member);
    };
    consider(&bot);
    consider(owner);
    if (Group* group = bot.GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            consider(ref->GetSource());
    return candidates;
}

std::vector<Player*> PlayerbotPartySupport::LivingSupportCandidates(Player& bot, Player* owner)
{
    if (!bot.IsInWorld() || !bot.IsAlive() || bot.IsBeingTeleported()) return {};
    std::vector<Player*> members;
    if (bot.GetGroup())
        members = OrderedPartyMembers(bot, owner);
    else
    {
        // Preserve attached-controller support before grouping; donor is self-only here.
        members.push_back(&bot);
        if (owner && owner != &bot) members.push_back(owner);
    }
    std::erase_if(members, [&](Player* member)
    {
        return !member->IsInWorld() || !member->IsAlive() || member->IsBeingTeleported() ||
            member->GetMap() != bot.GetMap() || !bot.IsFriendlyTo(member) ||
            member->IsGameMaster() || member->IsCharmed() ||
            (member != &bot && (!bot.IsWithinDistInMap(member, 30.0f) || !bot.IsWithinLOSInMap(member)));
    });
    return members; // Immediate map-thread inspection only, never queued.
}

bool PlayerbotPartySupport::HasDispellableAura(Player& bot, Player& target, std::uint32_t spellId, std::uint32_t dispelType)
{
    SpellInfo const* spell = sSpellMgr->GetSpellInfo(spellId);
    if (!spell || !bot.IsInWorld() || !target.IsInWorld() || bot.IsBeingTeleported() || target.IsBeingTeleported() ||
        !target.IsAlive() || !bot.IsFriendlyTo(&target) || target.GetMap() != bot.GetMap() ||
        !bot.IsWithinDistInMap(&target, 30.0f) || !bot.IsWithinLOSInMap(&target))
        return false;
    bool matchingDispel = false;
    for (SpellEffectInfo const& effect : spell->Effects)
        if (effect.IsEffect(SPELL_EFFECT_DISPEL) && effect.MiscValue == int32(dispelType))
            matchingDispel = true;
    if (!matchingDispel)
        return false;
    DispelChargesList auras;
    target.GetDispellableAuraList(&bot, SpellInfo::GetDispelMask(DispelType(dispelType)), auras);
    return !auras.empty(); // No native aura pointer survives this call.
}

namespace
{
template <typename Attempt>
bool VisitPartyBuffCandidates(Player& bot, Player& owner, PlayerbotDecision::PartyBuff const& buff, Attempt&& attempt,
    ForceRebuffState const* rebuff = nullptr)
{
    if (!bot.IsAlive() || !owner.IsAlive() || bot.IsInCombat() || owner.IsInCombat() ||
        bot.IsNonMeleeSpellCast(false) || !bot.HasSpell(buff.SpellId) || bot.GetMap() != owner.GetMap())
        return false;

    auto apply = [&](Player* member)
    {
        if (!member || !member->IsAlive() || member->IsInCombat() || member->GetMap() != bot.GetMap() ||
            (member != &bot && (!bot.IsWithinDistInMap(member, 30.0f) || !bot.IsWithinLOSInMap(member))))
            return false;
        auto missing = [&](uint32 auraId)
        {
            Aura* aura = member->GetAura(auraId);
            return !aura || (rebuff && rebuff->BelowRefreshTarget(aura->GetDuration(), aura->GetMaxDuration(), getMSTime(), bot.IsInCombat()));
        };
        return missing(buff.SingleAuraId) && missing(buff.PartyAuraId) && attempt(*member);
    };

    return PlayerbotPartySupport::TryCandidates(PlayerbotPartySupport::LivingSupportCandidates(bot, &owner), apply);
}
}

bool PlayerbotDecision::PartyBuffNeeded(Player& bot, Player& owner, PartyBuff const& buff)
{
    return VisitPartyBuffCandidates(bot, owner, buff, [](Player&) { return true; });
}

bool PlayerbotDecision::MaintainPartyBuff(Player& bot, Player& owner, PartyBuff const& buff, ForceRebuffState const* rebuff)
{
    return VisitPartyBuffCandidates(bot, owner, buff, [&](Player& member)
    {
        // Native pet-autocast rejects identical auras before its cast checks.
        // Refresh only the two supported buffs on self/actual-group members;
        // retain normal autocast target selection for absent buffs/ungrouped owners.
        bool refresh = rebuff && rebuff->IsPending(getMSTime()) &&
            (buff.SpellId == 1459 || buff.SpellId == 21562) &&
            (&member == &bot || (bot.GetGroup() && bot.GetGroup() == member.GetGroup())) &&
            (member.HasAura(buff.SingleAuraId) || member.HasAura(buff.PartyAuraId));
        return TryCastChecked(bot, member, buff.SpellId, buff.Name, refresh);
    }, rebuff);
}

ObjectGuid PlayerbotDecision::PartyBuffTarget(Player& bot, Player& owner, PartyBuff const& buff, ForceRebuffState const* rebuff)
{
    ObjectGuid result;
    VisitPartyBuffCandidates(bot, owner, buff, [&](Player& member)
    {
        result = member.GetGUID();
        return true;
    }, rebuff);
    return result;
}
