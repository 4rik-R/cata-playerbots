/*
 * Random-population bots playing a dungeon with a human: the bot tank pulls
 * and marks, a crowd-control class keeps the moon asleep, everyone steps out
 * of harmful areas and drinks between fights. Adapted from mod-playerbots'
 * dungeon behaviour (PullStrategy, RtiStrategy, CcStrategy, AvoidAoeStrategy,
 * UseFoodStrategy) at d750b4b. See AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_DUNGEON_H
#define CATA_RANDOM_PLAYERBOT_DUNGEON_H

#include "RandomPlayerbotDungeonPolicy.h"
#include "ObjectGuid.h"
#include <cstdint>
#include <string>

class Creature;
class Group;
class Player;
class PlayerbotAI;
struct Position;

namespace RandomPlayerbotDungeon
{
// Per bot, owned by its session on the map thread.
struct State
{
    uint32_t ReadySinceMs = 0;
    uint32_t PullAfterMs = 0;
    uint32_t Pulls = 0;
    uint32_t NextMarkMs = 0;
    ObjectGuid Marked[3]; // skull, cross, moon as this tank set them
    uint32_t CrowdControlAfterMs = 0;
    ObjectGuid CrowdControlNoticed;
    uint32_t NextHazardCheckMs = 0;
    uint32_t HazardNoticedMs = 0;
    uint32_t EscapeUntilMs = 0;
    uint32_t NextBreakMs = 0;
    // Tank walking ahead to the next pack; while set the bot does not follow the human.
    bool Leading = false;
    bool Waiting = false;
    ObjectGuid LeadTarget;
    uint32_t NextLeadMoveMs = 0;
};

// A human of the bot's dungeon-finder group in the same dungeon, when the bot is a random bot there.
Player* Human(Player& bot, uint32_t accountId);
// The bot fills the tank role of its dungeon group (a feral druid tank before Bear Form is up, too).
bool IsTank(Player const& bot);

// Tank: the next enemy to pull, once the group is ready and the human close; nullptr otherwise.
// With nothing near the human the tank walks on to the next pack (State::Leading) and waits for the human.
Creature* SelectPull(Player& tank, Player& human, State& state, uint32_t now);
// Back to following the human.
void StopLeading(Player& tank, State& state);
void Pulled(Player& tank, Creature& target, State& state, uint32_t now);
// Tank in combat: skull on its target, cross on the next, moon for crowd control on a big pack.
void UpdateMarks(Player& tank, Creature* target, State& state, uint32_t now);
// Crowd-control classes: keep the moon controlled. True while the bot cast or waits to cast.
bool CrowdControl(Player& bot, State& state, uint32_t now);
// Everyone: true while the bot steps out of a harmful area.
bool AvoidHazards(Player& bot, Player& human, State& state, uint32_t now);
// A harmful area covers this point.
bool HazardAt(Player& bot, Position const& point);
// Out of combat: eat or drink before the next pull. True when a break started.
bool TakeBreak(PlayerbotAI& ai, Player& bot, State& state, uint32_t now);

// "bots warten" / "bots weiter" from a human of the group.
void Command(Player& sender, Group& group, RandomPlayerbotDungeonPolicy::Command command);
bool Paused(Group const& group);

// Status line part: "pulls 3, marks 5, crowd controls 1, areas avoided 2, breaks 4; tank Name: leads to X".
std::string Describe();
}

#endif
