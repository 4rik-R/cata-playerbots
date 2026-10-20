/*
 * Table-driven combat for rogues, hunters and warlocks. See
 * PlayerbotClassRotationData.h. Released under GNU GPL v2 or any later version.
 */
#ifndef PLAYERBOT_CLASS_ROTATION_H
#define PLAYERBOT_CLASS_ROTATION_H

#include "PlayerbotClassRotationData.h"

class Creature;
class Player;

// Map-thread only, like the other class strategies.
namespace PlayerbotClassRotation
{
// One combat decision: keeps the pet on the target, then casts the first ready step.
bool Execute(Player& bot, Creature& target);
// Out of combat: pet present and healthy, self buffs up. True while work remains.
bool NeedsUpkeep(Player& bot);
bool Upkeep(Player& bot);
void LogKnownAbilities(Player const& bot);
// World thread at startup: logs table spells missing from the loaded spell data.
void ValidateSpells();
}

#endif
