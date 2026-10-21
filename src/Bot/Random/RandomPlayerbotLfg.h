/*
 * Random-population bots in the dungeon finder: they queue for the roles a
 * human's queue still lacks, accept the group, follow the human into the
 * dungeon and leave the group again once the humans are gone.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_LFG_H
#define CATA_RANDOM_PLAYERBOT_LFG_H

#include <string>
#include <vector>

class Player;

namespace RandomPlayerbotLfg
{
// World thread, every few seconds, with the random bots that are online.
void Update(std::vector<Player*> const& bots);
std::string Describe();
}

#endif
