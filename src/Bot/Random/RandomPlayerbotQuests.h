/*
 * Autonomous questing for random-population bots, adapted from mod-playerbots
 * NewRpgBaseAction (SearchQuestGiverAndAcceptOrReward, InteractWithNpcOrGameObjectForQuest,
 * OrganizeQuestLog) and GrindTargetValue::needForQuest at d750b4b.
 * See AUTHORS.md and PORTING.md. Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_QUESTS_H
#define CATA_RANDOM_PLAYERBOT_QUESTS_H

#include "ObjectGuid.h"
#include "Position.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Creature;
class Player;
class WorldSession;

namespace RandomPlayerbotQuests
{
// What a questing bot is busy with, counted for the status line.
enum class Activity : uint8_t
{
    None,
    Questing,   // working on quests at a spot, a giver or an object
    Travelling, // on the way to a quest spot or turn-in
    Grinding,   // nothing to quest here: fighting what is around
};

// Per bot, owned by its session behavior on the map thread (WantsRelocation is read by the world thread).
struct BotState
{
    struct Progress
    {
        uint32_t Signature = 0;
        uint32_t SinceMs = 0;
    };
    ObjectGuid Giver;
    uint32_t GiverSinceMs = 0;
    std::unordered_map<uint64_t, uint32_t> SkippedGivers; // giver guid -> skipped until
    // Quest object the bot walks to or is opening.
    ObjectGuid Object;
    uint32_t ObjectSinceMs = 0;
    std::unordered_map<uint64_t, uint32_t> SkippedObjects; // object guid -> skipped until
    std::unordered_map<uint32_t, Progress> Quests;
    std::unordered_set<uint32_t> Shelved; // abandoned this session, not taken again
    // Where the bot works on its quests: a mob or object spawn, a quest-map marker or a turn-in.
    bool HasSpot = false;
    Position Spot;
    uint32_t SpotArrivedMs = 0;
    uint32_t LastProgressMs = 0;
    std::vector<Position> TriedSpots; // given up on until the next progress
    Position TravelCheck;
    uint32_t TravelCheckMs = 0;
    bool Riding = false; // mounted by questing travel
    uint32_t NextScanMs = 0;
    uint32_t IdleSinceMs = 0;
    std::atomic<bool> WantsRelocation { false };
    Activity Doing = Activity::None;

    ~BotState();
    void ClearSpot() { HasSpot = false; SpotArrivedMs = 0; TravelCheckMs = 0; }
};

void Configure(bool enabled);
bool Enabled();
// World thread, once: quests bots can do, their givers, kill credits and drops.
void BuildIndex();
bool Ready();

// Map thread. Turns in and accepts quests at nearby givers, abandons stalled quests,
// opens quest objects and moves the bot's home to where its quests are done: mob
// and object spawns, quest-map markers and turn-ins. True while the bot walks or
// rides somewhere, so the caller skips idle wandering.
bool Update(WorldSession& session, Player& bot, BotState& state, Position& home, uint32_t nowMs);
// Map thread. Leaves a mount the bot took for questing travel.
void Dismount(Player& bot, BotState& state);
// Map thread. Nearest creature in QuestTargetRange that counts for an open quest.
Creature* SelectQuestTarget(Player& bot);
// World thread. Teleports the bot next to a quest giver with quests for it.
bool RelocateToGiver(Player& bot);
std::string Describe();
}

#endif
