/*
 * Capital visits for random-population bots: home by hearthstone every few
 * levels, repair and sell at a vendor, see the class trainer, stroll past
 * bankers, auctioneers and innkeepers, then set out again by flight master.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_CAPITAL_H
#define CATA_RANDOM_PLAYERBOT_CAPITAL_H

#include <cstdint>
#include <string>

class Player;
class WorldSession;
struct Position;
namespace RandomPlayerbotQuests { struct BotState; }

namespace RandomPlayerbotCapital
{
// World thread, once: vendors, trainers and city folk of Stormwind and Orgrimmar.
void BuildIndex();
// Map thread. Starts a visit when one is due and runs it; true while the bot is busy in its capital.
// At the end the bot asks for a new quest hub, which it reaches on a journey.
bool Update(WorldSession& session, Player& bot, RandomPlayerbotQuests::BotState& state, Position& home, uint32_t nowMs);
// Map thread. Drops a visit, for a bot a human takes over.
void Abort(RandomPlayerbotQuests::BotState& state);
std::string Describe();
}

#endif
