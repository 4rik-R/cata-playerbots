/*
 * Voyages of random-population bots between continents and landmasses: ships
 * and zeppelins, the portals of the capitals, Shattrath and Dalaran, and
 * walk-in portals such as the Dark Portal. Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_CROSSINGS_H
#define CATA_RANDOM_PLAYERBOT_CROSSINGS_H

#include <cstdint>
#include <string>

class Player;
class WorldSession;
struct Position;
namespace RandomPlayerbotQuests { struct BotState; }

namespace RandomPlayerbotCrossings
{
// World thread, once: docks and timetables of ships and zeppelins, portals and walk-in portals.
void BuildIndex();
// World thread. Sets the bot out on a voyage to a goal in another region; false when none leads there.
bool PlanVoyage(Player& bot, RandomPlayerbotQuests::BotState& state, uint32_t map, Position const& goal);
// Map thread. Runs a voyage; true while the bot is busy with it. The ride to each dock is a journey,
// run by the quest update, and the last crossing is followed by one to the goal.
bool Update(Player& bot, RandomPlayerbotQuests::BotState& state, uint32_t nowMs);
// Map thread. Drops a voyage, for a bot a human takes over.
void Abort(Player& bot, RandomPlayerbotQuests::BotState& state);
std::string Describe();
}

#endif
