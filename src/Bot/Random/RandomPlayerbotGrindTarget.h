/*
 * Native target search for solo grinding. See RandomPlayerbotGrind.h.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_GRIND_TARGET_H
#define CATA_RANDOM_PLAYERBOT_GRIND_TARGET_H

class Creature;
class Player;

namespace RandomPlayerbotGrind
{
// Map-thread only. Nearest untapped, idle, normal-rank creature in the bot's
// level band within PullRange and line of sight; nullptr if none qualifies.
Creature* SelectTarget(Player& bot);
// Map-thread only. A creature currently attacking the bot, if any.
Creature* SelectAttacker(Player& bot);
}

#endif
