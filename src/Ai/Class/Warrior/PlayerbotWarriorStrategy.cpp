/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerbotWarriorStrategy.h"
#include "Creature.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotCombatDecision.h"
#include <cmath>

namespace
{
// The priority and trigger pattern is adapted from mod-playerbots' Warrior
// strategies. IDs and conditions use the pinned Cata 4.3.4.15595 spell data.
bool HasVictoryRushProc(Player const& bot, Creature const& /*target*/)
{
    return bot.HasAura(32216) || bot.HasAura(82368);
}

bool NeedsRend(Player const& bot, Creature const& target)
{
    // Casting 772 applies the periodic aura 94009 in the Cata SpellEffect DBC.
    return !target.HasAura(94009, bot.GetGUID());
}

bool NeedsBattleShout(Player const& bot, Creature const& /*target*/)
{
    // The Cata 6673 cast applies its attack-power aura to the caster's party.
    return !bot.HasAura(6673);
}

bool Always(Player const& /*bot*/, Creature const& /*target*/)
{
    return true;
}

constexpr PlayerbotDecision::Action WarriorActions[] =
{
    { 34428, "Victory Rush", &HasVictoryRushProc, PlayerbotDecision::ActionTarget::Enemy }, // level 5, trainer learned
    { 6673, "Battle Shout", &NeedsBattleShout, PlayerbotDecision::ActionTarget::Self },     // level 20, trainer learned
    { 772, "Rend", &NeedsRend, PlayerbotDecision::ActionTarget::Enemy },                    // level 7, trainer learned
    { 88161, "Strike", &Always, PlayerbotDecision::ActionTarget::Enemy }                    // level 1, automatic
};
}

void PlayerbotWarrior::LogKnownAbilities(Player const& bot)
{
    TC_LOG_INFO("module.playerbots", "PB-02: Strike %s for %s", bot.HasSpell(88161) ? "known" : "not learned", bot.GetName().c_str());
    TC_LOG_INFO("module.playerbots", "PB-02: Warrior level=%u Victory Rush=%u Rend=%u Battle Shout=%u", bot.getLevel(),
        bot.HasSpell(34428), bot.HasSpell(772), bot.HasSpell(6673));
}

bool PlayerbotWarrior::Execute(Player& bot, Creature& target)
{
    if (bot.getClass() != CLASS_WARRIOR || !target.IsWithinMeleeRange(&bot) || !bot.IsWithinLOSInMap(&target) ||
        !bot.HasInArc(2.0f * float(M_PI) / 3.0f, &target))
        return false;

    return PlayerbotDecision::ExecuteFirstAvailable(bot, target, WarriorActions);
}

bool PlayerbotWarrior::MaintainBuff(Player& bot)
{
    if (bot.getClass() != CLASS_WARRIOR || !bot.IsAlive() || bot.IsInCombat() ||
        !bot.HasSpell(6673) || bot.HasAura(6673))
        return false;

    return PlayerbotDecision::TryCast(bot, bot, 6673, "Battle Shout");
}
