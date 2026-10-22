/*
 * Table-driven combat for rogues, hunters, warlocks, paladins, shamans and
 * druids. See
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
// Warriors and mages: an area spell on a pack, before their own strategy. True when one was cast.
bool ExecuteAoe(Player& bot, Creature& target);
// Class and specialization fight from spell range (druids only once they have Cat Form).
bool FightsAtRange(Player const& bot);
// Out of combat: pet, weapon imbue and self buffs up, druid forms dropped. True while work remains.
bool NeedsUpkeep(Player& bot);
bool Upkeep(Player& bot);
void LogKnownAbilities(Player const& bot);
// World thread at startup: logs table spells missing from the loaded spell data.
void ValidateSpells();
}

#endif
