/*
 * Quest rules for random-population bots, adapted from mod-playerbots
 * NewRpgBaseAction (IsQuestWorthDoing, IsQuestCapableDoing, quest-log
 * organisation and give-up timers) at d750b4b. See AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_QUEST_POLICY_H
#define CATA_RANDOM_PLAYERBOT_QUEST_POLICY_H

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
    GameObject,  // using objects comes with M4.2
};

inline Verdict Classify(Shape const& shape)
{
    if (shape.Deprecated || shape.Repeating || shape.Timed || shape.Escort || shape.Group)
        return Verdict::Unavailable;
    if (shape.Scripted || shape.NeedsSpell || shape.Currency || shape.PlayerKills)
        return Verdict::Scripted;
    if (shape.GameObjectObjectives)
        return Verdict::GameObject;
    if (!shape.KillObjectives && !shape.ItemObjectives)
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
}

#endif
