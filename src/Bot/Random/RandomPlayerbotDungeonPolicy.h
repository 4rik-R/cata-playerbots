/*
 * How random-population bots play a dungeon with a human: when the tank pulls,
 * which enemy gets which raid mark, who crowd-controls what, when the group takes
 * a mana break and where a bot steps out of a harmful area. Pure logic, no core
 * types, so it is unit tested.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_DUNGEON_POLICY_H
#define CATA_RANDOM_PLAYERBOT_DUNGEON_POLICY_H

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace RandomPlayerbotDungeonPolicy
{
// Everyone has this much health and mana before the tank pulls.
constexpr float ReadyPct = 80.0f;
// Between fights a bot drinks below this much mana and eats below this much health.
constexpr float DrinkBelowPct = 60.0f;
constexpr float EatBelowPct = 50.0f;
// The tank pulls only with the human this close, and only enemies this close to the human.
constexpr float HumanNearTank = 30.0f;
constexpr float PullRange = 25.0f;
// A boss is only pulled with the human right next to the tank.
constexpr float HumanNearTankForBoss = 15.0f;
// Enemies more than this far above or below the human stand on another floor.
constexpr float FloorHeight = 6.0f;
// Once everyone is ready the tank waits a moment, like a player checking the group.
constexpr uint32_t PullPauseMinMs = 1500, PullPauseMaxMs = 4000;
// Area spells from this many enemies around the target.
constexpr uint32_t AoeEnemies = 3;
constexpr float AoeRadius = 8.0f;
// The tank sets the moon for crowd control from this many enemies on.
constexpr uint32_t CrowdControlEnemies = 4;
// A bot reacts to a new fight, a new mark or a harmful area after this delay.
constexpr uint32_t ReactMinMs = 300, ReactMaxMs = 1200;
// A bot stands this much outside a harmful area.
constexpr float HazardMargin = 2.5f;
// Harmful areas are looked for this often and this far around a bot.
constexpr uint32_t HazardCheckMs = 400;
constexpr float HazardScanRange = 30.0f;

// Raid marks (Group target icons).
constexpr uint8_t Moon = 4, Cross = 6, Skull = 7;

// Creature types (CreatureType in the core).
constexpr uint32_t TypeBeast = 1, TypeDragonkin = 2, TypeDemon = 3, TypeElemental = 4, TypeUndead = 6,
    TypeHumanoid = 7;
constexpr uint32_t TypeBit(uint32_t type) { return 1u << type; }

// Classes (Classes in the core).
constexpr uint8_t ClassPriest = 5, ClassMage = 8, ClassWarlock = 9, ClassDruid = 11;

struct CrowdControlSpell
{
    uint8_t Class;
    uint32_t SpellId;
    char const* Name;
    uint32_t Types; // creature types it works on
};

// Spells that keep one enemy out of the fight until it is attacked.
constexpr CrowdControlSpell CrowdControlSpells[] = {
    { ClassMage, 118, "Polymorph", TypeBit(TypeHumanoid) | TypeBit(TypeBeast) },
    { ClassDruid, 2637, "Hibernate", TypeBit(TypeBeast) | TypeBit(TypeDragonkin) },
    { ClassPriest, 9484, "Shackle Undead", TypeBit(TypeUndead) },
    { ClassWarlock, 710, "Banish", TypeBit(TypeDemon) | TypeBit(TypeElemental) },
};

inline CrowdControlSpell const* CrowdControlFor(uint8_t playerClass, uint32_t creatureType)
{
    for (CrowdControlSpell const& spell : CrowdControlSpells)
        if (spell.Class == playerClass && (spell.Types & TypeBit(creatureType)))
            return &spell;
    return nullptr;
}

inline bool Ready(float healthPct, float manaPct, bool usesMana)
{
    return healthPct >= ReadyPct && (!usesMana || manaPct >= ReadyPct);
}

enum class Break { None, Drink, Eat };
inline Break BreakNeeded(float healthPct, float manaPct, bool usesMana)
{
    if (usesMana && manaPct < DrinkBelowPct)
        return Break::Drink;
    return healthPct < EatBelowPct ? Break::Eat : Break::None;
}

// Stable per-bot, per-event spread, so bots never act in the same moment.
inline uint32_t Spread(uint32_t seed, uint32_t event, uint32_t minMs, uint32_t maxMs)
{
    uint32_t hash = (seed * 2654435761u) ^ (event * 2246822519u);
    hash ^= hash >> 15;
    hash *= 2654435761u;
    hash ^= hash >> 13;
    return maxMs > minMs ? minMs + hash % (maxMs - minMs + 1) : minMs;
}
inline uint32_t ReactMs(uint32_t seed, uint32_t event) { return Spread(seed, event, ReactMinMs, ReactMaxMs); }
inline uint32_t PullPauseMs(uint32_t seed, uint32_t event) { return Spread(seed, event, PullPauseMinMs, PullPauseMaxMs); }

struct PullCandidate
{
    float DistanceToHuman;
    float HeightToHuman; // absolute
    bool Boss;
};

inline bool PullAllowed(PullCandidate const& candidate, float humanToTank)
{
    if (candidate.DistanceToHuman > PullRange || candidate.HeightToHuman > FloorHeight)
        return false;
    return humanToTank <= (candidate.Boss ? HumanNearTankForBoss : HumanNearTank);
}

// Nearest to the human first: the human sets the direction, the tank takes what lies ahead of them.
inline bool BetterPull(PullCandidate const& next, PullCandidate const& old)
{
    return next.DistanceToHuman < old.DistanceToHuman;
}

// With nothing to pull near the human, the tank walks on to the next pack like a player tank:
// packs this far from the tank count, the walk follows the path, and the tank stops to wait
// once the human falls this far behind, going on when they caught up again.
constexpr float LeadRange = 70.0f;
constexpr float LeadPathFactor = 1.6f; // a path much longer than the straight line leads around a wall
constexpr float LeadWaitBeyond = 25.0f;
constexpr float LeadGoWithin = 15.0f;
// Further away than this the human went elsewhere: the tank follows instead of leading.
constexpr float LeadHumanLost = 45.0f;
constexpr uint32_t LeadMoveEveryMs = 2000;

enum class LeadStep { Follow, Walk, Wait, Pull };
struct LeadView
{
    float HumanToTank;
    float TankToPack;     // straight line
    float PathToPack;     // along the walkable path; 0 when there is none
    float HeightToPack;   // absolute, tank to pack
    bool PackInSight;
    bool Boss;
    bool Waiting;         // the tank waited for the human last time
};

inline LeadStep Lead(LeadView const& view)
{
    if (view.HumanToTank > LeadHumanLost || view.PathToPack <= 0.0f || view.HeightToPack > FloorHeight ||
        view.TankToPack > LeadRange || view.PathToPack > view.TankToPack * LeadPathFactor + 10.0f)
        return LeadStep::Follow;
    if (view.PackInSight && PullAllowed({ view.TankToPack, view.HeightToPack, view.Boss }, view.HumanToTank))
        return LeadStep::Pull;
    if (view.HumanToTank > (view.Waiting ? LeadGoWithin : LeadWaitBeyond))
        return LeadStep::Wait;
    return LeadStep::Walk;
}

struct Enemy
{
    uint64_t Guid;
    float Health;
    bool CanBeControlled; // a bot in the group has a crowd-control spell for it
    bool Boss;
};

struct Marks
{
    uint64_t Skull = 0, Cross = 0, Moon = 0;
};

// Skull is the tank's target, cross the healthiest other enemy, moon the next controllable one
// once the pack is big enough. A mark the human set stays, and is not given to another enemy.
inline Marks PlanMarks(uint64_t tankTarget, std::vector<Enemy> const& enemies, Marks const& human)
{
    Marks marks = human;
    auto taken = [&marks](uint64_t guid) { return guid && (guid == marks.Skull || guid == marks.Cross || guid == marks.Moon); };
    if (!marks.Skull && tankTarget && !taken(tankTarget))
        marks.Skull = tankTarget;
    if (!marks.Moon && enemies.size() >= CrowdControlEnemies)
    {
        Enemy const* best = nullptr;
        for (Enemy const& enemy : enemies)
            if (enemy.CanBeControlled && !enemy.Boss && !taken(enemy.Guid) && (!best || enemy.Health > best->Health))
                best = &enemy;
        if (best)
            marks.Moon = best->Guid;
    }
    if (!marks.Cross)
    {
        Enemy const* best = nullptr;
        for (Enemy const& enemy : enemies)
            if (!taken(enemy.Guid) && (!best || enemy.Health > best->Health))
                best = &enemy;
        if (best)
            marks.Cross = best->Guid;
    }
    return marks;
}

struct Point
{
    float X = 0.0f, Y = 0.0f;
};

inline float Distance(Point a, Point b) { return std::hypot(a.X - b.X, a.Y - b.Y); }

inline bool Inside(Point bot, Point center, float radius) { return Distance(bot, center) < radius + 0.5f; }

// Where a bot inside a harmful area steps to: just outside it, on the side towards its group,
// turned a little by a per-bot offset so not everyone lands on the same spot.
inline Point EscapePoint(Point bot, Point center, float radius, Point group, float offsetRadians)
{
    float away = Distance(bot, center) > 0.1f ? std::atan2(bot.Y - center.Y, bot.X - center.X) :
        std::atan2(group.Y - center.Y, group.X - center.X);
    float reach = radius + HazardMargin;
    Point best;
    float bestScore = 1e9f;
    // A few directions from the bot's own way out; the one closest to the group wins.
    for (float turn : { 0.0f, 0.6f, -0.6f, 1.2f, -1.2f })
    {
        float angle = away + turn + offsetRadians;
        Point option { center.X + std::cos(angle) * reach, center.Y + std::sin(angle) * reach };
        float score = Distance(option, group) + Distance(option, bot) * 0.5f;
        if (score < bestScore)
        {
            bestScore = score;
            best = option;
        }
    }
    return best;
}

// Per-bot offset for EscapePoint in [-0.35, 0.35] radians.
inline float EscapeOffset(uint32_t seed) { return (float(Spread(seed, 7, 0, 700)) - 350.0f) / 1000.0f; }

enum class Command { None, Wait, Go };

// "bots warten" / "bots weiter", said or written in the group's chat.
inline Command ParseCommand(std::string_view text)
{
    std::string lower;
    for (char c : text)
        lower += char(std::tolower(static_cast<unsigned char>(c)));
    auto trimmed = [](std::string s)
    {
        while (!s.empty() && (s.back() == ' ' || s.back() == '!' || s.back() == '.'))
            s.pop_back();
        size_t start = s.find_first_not_of(' ');
        return start == std::string::npos ? std::string() : s.substr(start);
    };
    lower = trimmed(lower);
    if (lower.rfind("bots ", 0) != 0)
        return Command::None;
    lower = trimmed(lower.substr(5));
    if (lower == "warten" || lower == "wartet" || lower == "stopp")
        return Command::Wait;
    if (lower == "weiter" || lower == "los" || lower == "weitermachen")
        return Command::Go;
    return Command::None;
}
}

#endif
