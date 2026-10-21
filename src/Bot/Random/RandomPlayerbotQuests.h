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
    Flying,     // on a journey to another quest hub: riding to a flight master, flying or riding on
    Visiting,   // home in the capital: hearthstone, vendor, trainer, a stroll
    Sailing,    // crossing to another continent: at a dock, aboard a ship or zeppelin, through a portal
};

// A move to another quest hub: by flight when that saves riding, else on the mount.
struct Journey
{
    enum class Leg : uint8_t
    {
        None,
        ToMaster, // riding to the flight master
        Flying,
        ToGoal,   // riding to the quest giver
    };
    Leg Stage = Leg::None;
    uint32_t Map = 0;
    Position Master;
    Position Goal;
    std::vector<uint32_t> Nodes;
    uint32_t Cost = 0;
    uint32_t StartedMs = 0;
    uint32_t Stalls = 0;
};

// A stay in the capital, from the hearthstone to setting out again.
struct CapitalVisit
{
    enum class Step : uint8_t
    {
        None,
        Hearthing, // casting the hearthstone, or on the way home
        Errands,   // repairing and selling, then the class trainer
        Browsing,  // standing around at bankers, auctioneers and innkeepers
    };
    struct Errand
    {
        uint8_t Kind = 0;
        uint32_t Entry = 0;
        Position Pos;
        uint32_t PauseMs = 0; // standing there once arrived
    };
    Step Stage = Step::None;
    uint32_t StepSinceMs = 0;
    uint32_t LeaveMs = 0;
    std::vector<Errand> Errands;
    std::size_t Next = 0;
    bool Arrived = false; // at the current errand, pausing
    uint32_t VisitLevel = 0; // level of the next visit, 0 until first checked
    uint32_t RetryMs = 0;
    uint32_t NextCheckMs = 0;
};

// A move to another continent or landmass: ships, zeppelins and portals, then a journey to the goal.
struct Voyage
{
    enum class Step : uint8_t
    {
        None,
        ToDock,  // on the way to the dock, portal or walk-in portal
        Waiting, // at the dock until the ship or zeppelin is in
        Aboard,
        Through, // stepped through a portal, arriving
    };
    Step Stage = Step::None;
    std::vector<uint32_t> Crossings; // in order, from RandomPlayerbotCrossings
    std::size_t Next = 0;
    uint32_t GoalMap = 0;
    Position Goal;
    uint32_t StartedMs = 0;
    uint32_t StepSinceMs = 0;
    uint32_t NextCheckMs = 0;
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
    Journey Trip;
    std::atomic<bool> TripFailed { false }; // set on the map thread, finished by teleport on the world thread
    CapitalVisit Visit;
    Voyage Sea;

    ~BotState();
    void ClearSpot() { HasSpot = false; SpotArrivedMs = 0; TravelCheckMs = 0; }
    bool Journeying() const { return Trip.Stage != Journey::Leg::None; }
    bool Visiting() const { return Visit.Stage != CapitalVisit::Step::None; }
    bool Voyaging() const { return Sea.Stage != Voyage::Step::None; }
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
// World thread. Sends the bot to a quest giver with quests for it: on a journey when a state
// is given and the giver is on the same continent, else by teleport.
bool RelocateToGiver(Player& bot, BotState* state = nullptr);
// Any thread that owns the bot. Sets out on a journey to a goal in the bot's region; false when only a teleport gets there.
bool SetOut(Player& bot, BotState& state, uint32_t map, Position const& goal);
// World thread. Teleports a bot whose journey failed to its goal.
bool FinishFailedTrip(Player& bot, BotState& state);
// Counts the bot under an activity for the status line.
void Track(BotState& state, Activity activity);
// Random bots counted under any activity.
uint32_t ActiveBots();
uint32_t CountOf(Activity activity);
std::string Describe();
}

#endif
