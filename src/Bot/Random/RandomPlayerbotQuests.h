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

class Creature;
class Player;
class WorldSession;

namespace RandomPlayerbotQuests
{
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
    std::unordered_map<uint32_t, Progress> Quests;
    std::unordered_set<uint32_t> Shelved; // abandoned this session, not taken again
    uint32_t NextScanMs = 0;
    uint32_t IdleSinceMs = 0;
    bool Hunting = false;
    std::atomic<bool> WantsRelocation { false };
};

void Configure(bool enabled);
bool Enabled();
// World thread, once: quests bots can do, their givers, kill credits and drops.
void BuildIndex();
bool Ready();

// Map thread. Turns in and accepts quests at nearby givers, abandons stalled quests
// and moves the bot's home towards mobs its quests need. True while the bot walks
// to a giver or a hunting spot, so the caller skips idle wandering.
bool Update(WorldSession& session, Player& bot, BotState& state, Position& home, uint32_t nowMs);
// Map thread. Nearest creature in QuestTargetRange that counts for an open quest.
Creature* SelectQuestTarget(Player& bot);
// World thread. Teleports the bot next to a quest giver with quests for it.
bool RelocateToGiver(Player& bot);
std::string Describe();
}

#endif
