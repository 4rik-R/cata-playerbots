/*
 * Quest rules for random-population bots, adapted from mod-playerbots
 * NewRpgBaseAction (IsQuestWorthDoing, IsQuestCapableDoing, quest-log
 * organisation and give-up timers) at d750b4b. See AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_QUEST_POLICY_H
#define CATA_RANDOM_PLAYERBOT_QUEST_POLICY_H

#include <algorithm>
#include <cstdint>
#include <vector>

namespace RandomPlayerbotQuestPolicy
{
// Quest givers a bot walks to on its own.
constexpr float GiverRange = 80.0f;
// Quest mobs are searched further out than ordinary grind targets.
constexpr float QuestTargetRange = 60.0f;
// Spawns of a needed mob a bot walks towards when none is in sight.
constexpr float SpawnSearchRange = 400.0f;
constexpr uint32_t ScanIntervalMs = 2000;
// Walking to a giver gives up after this long; the giver is skipped for a while.
constexpr uint32_t GiverTimeoutMs = 60000;
constexpr uint32_t GiverSkipMs = 300000;
// A quest without progress for this long is abandoned and not taken again this session.
constexpr uint32_t QuestStallMs = 30 * 60 * 1000;
// A bot with nothing to do here for this long is placed at another quest giver.
constexpr uint32_t IdleRelocateMs = 5 * 60 * 1000;
// Quests at most this many levels above the bot.
constexpr uint32_t MaxLevelsAbove = 3;
constexpr uint32_t EscortQuestInfo = 84;

// Quest objects (chests, plants, crates) searched around the bot.
constexpr float ObjectRange = 40.0f;
// An object a real player stands next to is left to them.
constexpr float PlayerClaimRange = 10.0f;
// Quest areas and turn-ins a bot walks or rides to, within its zone.
constexpr float TravelRange = 1500.0f;
// Bots with a mount ride to targets further away than this.
constexpr float RideDistance = 120.0f;
constexpr uint32_t ObjectTimeoutMs = 30000;
// At a quest spot without progress for this long, the bot tries another spot.
constexpr uint32_t SpotStallMs = 5 * 60 * 1000;
constexpr float TriedSpotRadius = 60.0f;
constexpr uint32_t MaxTriedSpots = 16;
// Travel that gains less than this in TravelCheckMs counts as stuck.
constexpr uint32_t TravelCheckMs = 30000;
constexpr float TravelMinGain = 5.0f;
// The log is tidied when fewer slots than this are free.
constexpr uint32_t MinFreeLogSlots = 2;
// quest_poi objective indexes: -1 is the turn-in, 0-3 kill or object objectives, 4-9 item objectives.
constexpr int32_t TurnInObjective = -1;
constexpr int32_t ItemObjectiveBase = 4;

// Facts read from quest_template and its addon; computed once at startup.
struct Shape
{
    bool Deprecated = false;
    bool Repeating = false;      // repeatable, daily, weekly, monthly or seasonal
    bool Timed = false;
    bool Escort = false;
    bool Group = false;          // elite, dungeon, raid, PvP type or suggested players > 1
    bool Scripted = false;       // event, area trigger, spell cast, tracking, player-cast or PvP flags
    bool NeedsSpell = false;     // RequiredSpell
    bool Currency = false;
    bool PlayerKills = false;
    uint32_t KillObjectives = 0;
    uint32_t GameObjectObjectives = 0;
    uint32_t ItemObjectives = 0; // items the quest itself does not hand out
};

enum class Verdict : uint8_t
{
    Supported,
    Unavailable, // deprecated, repeating, timed, escort or group
    Scripted,
    NoObjective, // talk-to and delivery quests come with M4.3
    GameObject,  // needs objects that are not spawned in the open world (counted by the index)
};

inline Verdict Classify(Shape const& shape)
{
    if (shape.Deprecated || shape.Repeating || shape.Timed || shape.Escort || shape.Group)
        return Verdict::Unavailable;
    if (shape.Scripted || shape.NeedsSpell || shape.Currency || shape.PlayerKills)
        return Verdict::Scripted;
    if (!shape.KillObjectives && !shape.GameObjectObjectives && !shape.ItemObjectives)
        return Verdict::NoObjective;
    return Verdict::Supported;
}

// Native Trinity::XP::GetGrayLevel.
inline uint32_t GrayLevel(uint32_t level)
{
    if (level <= 5)
        return 0;
    if (level <= 39)
        return level - 5 - level / 10;
    if (level <= 59)
        return level - 1 - level / 5;
    return level - 9;
}

// questLevel is the level for this bot (scaling quests already resolved).
inline bool LevelFits(uint32_t botLevel, uint32_t questLevel, uint32_t minLevel)
{
    return minLevel <= botLevel && questLevel <= botLevel + MaxLevelsAbove && questLevel > GrayLevel(botLevel);
}

// Sum of objective counts; any change counts as progress.
inline uint32_t ProgressSignature(std::vector<uint32_t> const& counts)
{
    uint32_t signature = 0;
    for (uint32_t count : counts)
        signature = signature * 31 + count + 1;
    return signature;
}

inline bool Stalled(uint32_t nowMs, uint32_t lastProgressMs)
{
    return uint32_t(nowMs - lastProgressMs) >= QuestStallMs;
}

struct RewardOption
{
    uint8_t Slot = 0;
    bool Usable = false;      // the bot can equip it and it suits its role
    uint32_t ItemLevel = 0;
    uint32_t EquippedLevel = 0; // item level in the slot it would replace, 0 if empty
    uint32_t SellPrice = 0;
};

// The best upgrade; without one, the item that sells for most. 0 when there is no choice.
inline uint8_t ChooseReward(std::vector<RewardOption> const& options)
{
    RewardOption const* best = nullptr;
    int64_t bestGain = 0;
    for (RewardOption const& option : options)
    {
        int64_t gain = option.Usable ? int64_t(option.ItemLevel) - int64_t(option.EquippedLevel) : 0;
        if (gain > bestGain)
        {
            best = &option;
            bestGain = gain;
        }
    }
    if (best)
        return best->Slot;
    for (RewardOption const& option : options)
        if (!best || option.SellPrice > best->SellPrice)
            best = &option;
    return best ? best->Slot : 0;
}

struct MapPoint
{
    float X = 0.0f;
    float Y = 0.0f;
};

// Even-odd rule; points on an edge may count either way.
inline bool InPolygon(std::vector<MapPoint> const& polygon, float x, float y)
{
    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++)
    {
        MapPoint const& a = polygon[i];
        MapPoint const& b = polygon[j];
        if ((a.Y > y) != (b.Y > y) && x < (b.X - a.X) * (y - a.Y) / (b.Y - a.Y) + a.X)
            inside = !inside;
    }
    return inside;
}

// A random point inside a quest-map marker. roll() returns a value in [0, 1).
// Markers too thin to hit fall back to the average of their corners.
template<class Roll>
MapPoint RandomPointIn(std::vector<MapPoint> const& polygon, Roll roll, uint32_t attempts = 30)
{
    if (polygon.empty())
        return {};
    MapPoint low = polygon.front(), high = polygon.front(), mean;
    for (MapPoint const& point : polygon)
    {
        low = { std::min(low.X, point.X), std::min(low.Y, point.Y) };
        high = { std::max(high.X, point.X), std::max(high.Y, point.Y) };
        mean.X += point.X / polygon.size();
        mean.Y += point.Y / polygon.size();
    }
    if (polygon.size() >= 3)
        for (uint32_t i = 0; i < attempts; ++i)
        {
            MapPoint candidate = { low.X + (high.X - low.X) * float(roll()), low.Y + (high.Y - low.Y) * float(roll()) };
            if (InPolygon(polygon, candidate.X, candidate.Y))
                return candidate;
        }
    return mean;
}

// Neither the arrival at the spot nor the last progress is SpotStallMs old.
inline bool SpotStalled(uint32_t nowMs, uint32_t arrivedMs, uint32_t lastProgressMs)
{
    return uint32_t(nowMs - arrivedMs) >= SpotStallMs && uint32_t(nowMs - lastProgressMs) >= SpotStallMs;
}

struct LogEntry
{
    uint32_t Id = 0;
    bool Gray = false;   // no experience any more
    bool Doable = false; // a quest bots can finish on their own
};

// Quests to abandon so that MinFreeLogSlots stay free: grey ones first, then
// ones bots cannot do. Quests a bot can still do are never dropped.
inline std::vector<uint32_t> LogCleanup(std::vector<LogEntry> const& log, uint32_t freeSlots)
{
    std::vector<uint32_t> drop;
    for (bool grayPass : { true, false })
        for (LogEntry const& entry : log)
        {
            if (freeSlots + drop.size() >= MinFreeLogSlots)
                return drop;
            bool pick = grayPass ? entry.Gray : (!entry.Gray && !entry.Doable);
            if (pick)
                drop.push_back(entry.Id);
        }
    return drop;
}
}

#endif
