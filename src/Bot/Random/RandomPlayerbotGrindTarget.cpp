/*
 * Native target search for solo grinding, adapted from mod-playerbots
 * GrindTargetValue::FindTargetForGrinding at d750b4b. See AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "RandomPlayerbotGrindTarget.h"
#include "RandomPlayerbotGrind.h"
#include "../../Ai/Base/PlayerbotTargetSelection.h"
#include "CellImpl.h"
#include "Creature.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Player.h"
#include <list>

bool RandomPlayerbotGrind::Eligible(Player& bot, Creature& creature)
{
    CreatureTemplate const* info = creature.GetCreatureTemplate();
    return info && creature.IsAlive() && creature.IsInWorld() && !creature.IsControlledByPlayer() &&
        !creature.IsInEvadeMode() && !creature.IsInCombat() && !creature.hasLootRecipient() &&
        info->rank == CREATURE_ELITE_NORMAL && !info->npcflag && !creature.IsGuard() && !creature.IsCivilian() &&
        !creature.IsTrigger() && !creature.IsCritter() && !creature.isWorldBoss() &&
        !creature.IsTotem() && !creature.IsPet() && !creature.IsSummon() &&
        RandomPlayerbotGrind::LevelAllows(bot.getLevel(), creature.getLevel()) &&
        !PlayerbotTargetSelection::IsProtectedTarget(creature) &&
        bot.IsValidAttackTarget(&creature) && bot.CanSeeOrDetect(&creature) && bot.IsWithinLOSInMap(&creature);
}

Creature* RandomPlayerbotGrind::SelectTarget(Player& bot)
{
    std::list<Unit*> units;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(&bot, &bot, PullRange);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(&bot, units, check);
    Cell::VisitAllObjects(&bot, searcher, PullRange);

    Creature* best = nullptr;
    float bestDistance = PullRange + 1.0f;
    for (Unit* unit : units)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature)
            continue;
        float distance = bot.GetDistance(creature);
        if (distance < bestDistance && Eligible(bot, *creature))
        {
            best = creature;
            bestDistance = distance;
        }
    }
    return best;
}

Creature* RandomPlayerbotGrind::SelectAttacker(Player& bot)
{
    Creature* best = nullptr;
    float bestDistance = PullRange + 5.0f;
    for (Unit* attacker : bot.getAttackers())
    {
        Creature* creature = attacker ? attacker->ToCreature() : nullptr;
        if (!creature || !creature->IsAlive() || creature->IsControlledByPlayer() ||
            PlayerbotTargetSelection::IsProtectedTarget(*creature) || !bot.IsValidAttackTarget(creature))
            continue;
        float distance = bot.GetDistance(creature);
        if (distance < bestDistance)
        {
            best = creature;
            bestDistance = distance;
        }
    }
    return best;
}
