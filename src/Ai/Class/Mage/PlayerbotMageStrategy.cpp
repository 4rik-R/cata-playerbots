/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerbotMageStrategy.h"
#include "Creature.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotCombatDecision.h"
#include "PlayerbotPartyBuffStrategy.h"
#include <cmath>

namespace
{
bool IsBeingPressed(Player const& bot, Creature const& target)
{
    return target.GetVictim() == &bot && bot.IsWithinDistInMap(&target, 8.0f);
}

bool Always(Player const& /*bot*/, Creature const& /*target*/)
{
    return true;
}

// The trigger/priority shape follows Playerbots' Frost Mage strategy. These
// spell IDs and levels are checked against the pinned Cata 4.3.4 DBC, and
// the shared cast action still validates learned spells, range and cooldown.
constexpr PlayerbotDecision::Action MageActions[] =
{
    { 122, "Frost Nova", &IsBeingPressed, PlayerbotDecision::ActionTarget::Self },
    { 2136, "Fire Blast", &IsBeingPressed, PlayerbotDecision::ActionTarget::Enemy },
    { 116, "Frostbolt", &Always, PlayerbotDecision::ActionTarget::Enemy },
    { 133, "Fireball", &Always, PlayerbotDecision::ActionTarget::Enemy }
};
}

void PlayerbotMage::LogKnownAbilities(Player const& bot)
{
    TC_LOG_INFO("module.playerbots", "PB-02: Mage level=%u Frost Nova=%u Fire Blast=%u Frostbolt=%u Fireball=%u",
        bot.getLevel(), bot.HasSpell(122), bot.HasSpell(2136), bot.HasSpell(116), bot.HasSpell(133));
}

bool PlayerbotMage::Execute(Player& bot, Creature& target)
{
    if (bot.getClass() != CLASS_MAGE || bot.IsNonMeleeSpellCast(false) ||
        !bot.IsWithinDistInMap(&target, 30.0f) || !bot.IsWithinLOSInMap(&target) ||
        !bot.HasInArc(2.0f * float(M_PI) / 3.0f, &target))
        return false;

    return PlayerbotDecision::ExecuteFirstAvailable(bot, target, MageActions);
}

bool PlayerbotMage::MaintainBuff(Player& bot, Player& owner)
{
    if (bot.getClass() != CLASS_MAGE || !bot.IsAlive() || bot.IsInCombat() || owner.IsInCombat() ||
        bot.IsNonMeleeSpellCast(false))
        return false;

    // Armor choice belongs to the upcoming talent-aware rotation profile;
    // do not impose one armor preference on all three Mage specializations.
    // The core's generic buff script expands 1459 into single/party auras.
    return PlayerbotDecision::MaintainPartyBuff(bot, owner, PlayerbotPartyBuff::Brilliance);
}
