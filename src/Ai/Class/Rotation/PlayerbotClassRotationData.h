/*
 * Priority tables for classes without a full strategy context, following the
 * shape of mod-playerbots' generic DPS strategies (RogueAiObjectContext,
 * HunterAiObjectContext, WarlockAiObjectContext) at d750b4b. Spell ids are
 * Cataclysm 4.3.4: each is a trainer_spell row of TDB 434.22011 or a native
 * starting spell. See AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef PLAYERBOT_CLASS_ROTATION_DATA_H
#define PLAYERBOT_CLASS_ROTATION_DATA_H

#include <cstdint>
#include <span>

namespace PlayerbotClassRotation
{
enum class Target : uint8_t { Enemy, Self, Pet };

// Conditions stay data so the tables can be checked without a running world.
enum class When : uint8_t
{
    Always,
    TargetCasting,      // interrupt
    TargetBelow20,      // execute range
    MyDotMissing,       // aura from this bot missing on the target, target not nearly dead
    AnyAuraMissing,     // aura from anyone missing on the target
    SelfAuraMissing,    // self buff missing (Aura names the buff)
    ComboFinisher,      // at least 4 combo points, or 1+ when the target is nearly dead
    ComboBuff,          // at least 2 combo points and Aura missing on self
    SelfBelow35,        // defensive cooldown
    SelfBelow40,        // drain or heal
    ManaLowHealthOk,    // life tap
    InMelee,
    NotInMelee,
    NoAutoShot,         // auto shot not running and the target out of melee
    FocusAtLeast50,
    PetHurt,            // pet alive below 40 % (combat) or 70 % (upkeep)
    PetAttacking,       // pet alive and on the target
};

struct Step
{
    uint32_t SpellId;
    char const* Name;
    When Condition;
    Target On;
    uint32_t Aura = 0; // aura to test, defaults to SpellId
};

// Highest priority first. Spells the bot has not learned are skipped at runtime.
constexpr Step RogueCombat[] = {
    { 1766, "Kick", When::TargetCasting, Target::Enemy },
    { 5277, "Evasion", When::SelfBelow35, Target::Self },
    { 5171, "Slice and Dice", When::ComboBuff, Target::Self },
    { 2098, "Eviscerate", When::ComboFinisher, Target::Enemy },
    { 1752, "Sinister Strike", When::Always, Target::Enemy },
};

constexpr Step HunterCombat[] = {
    { 75, "Auto Shot", When::NoAutoShot, Target::Enemy },
    { 136, "Mend Pet", When::PetHurt, Target::Pet },
    { 2973, "Raptor Strike", When::InMelee, Target::Enemy },
    { 1130, "Hunter's Mark", When::AnyAuraMissing, Target::Enemy },
    { 53351, "Kill Shot", When::TargetBelow20, Target::Enemy },
    { 1978, "Serpent Sting", When::MyDotMissing, Target::Enemy },
    { 34026, "Kill Command", When::PetAttacking, Target::Enemy },
    { 3044, "Arcane Shot", When::FocusAtLeast50, Target::Enemy },
    { 56641, "Steady Shot", When::NotInMelee, Target::Enemy },
    { 3044, "Arcane Shot", When::NotInMelee, Target::Enemy },
};

constexpr Step WarlockCombat[] = {
    { 1454, "Life Tap", When::ManaLowHealthOk, Target::Self },
    { 689, "Drain Life", When::SelfBelow40, Target::Enemy },
    { 1120, "Drain Soul", When::TargetBelow20, Target::Enemy },
    { 980, "Bane of Agony", When::MyDotMissing, Target::Enemy },
    { 172, "Corruption", When::MyDotMissing, Target::Enemy },
    { 348, "Immolate", When::MyDotMissing, Target::Enemy },
    { 686, "Shadow Bolt", When::Always, Target::Enemy },
};

// Out of combat, before the next pull.
constexpr Step HunterUpkeep[] = {
    { 13165, "Aspect of the Hawk", When::SelfAuraMissing, Target::Self },
    { 136, "Mend Pet", When::PetHurt, Target::Pet },
};

constexpr Step WarlockUpkeep[] = {
    { 28176, "Fel Armor", When::SelfAuraMissing, Target::Self },
    { 687, "Demon Armor", When::SelfAuraMissing, Target::Self },
};

// Pet summons, tried in order when the bot has no pet.
constexpr uint32_t HunterCallPet = 883;
constexpr uint32_t HunterRevivePet = 982;
constexpr uint32_t WarlockVoidwalker = 697;
constexpr uint32_t WarlockImp = 688;
// Either armor counts as worn; Fel Armor replaces Demon Armor once learned.
constexpr uint32_t WarlockArmors[] = { 28176, 687 };

constexpr uint8_t ClassRogue = 4, ClassHunter = 3, ClassWarlock = 9;

inline std::span<Step const> CombatSteps(uint8_t playerClass)
{
    switch (playerClass)
    {
        case ClassRogue: return RogueCombat;
        case ClassHunter: return HunterCombat;
        case ClassWarlock: return WarlockCombat;
        default: return {};
    }
}

inline std::span<Step const> UpkeepSteps(uint8_t playerClass)
{
    switch (playerClass)
    {
        case ClassHunter: return HunterUpkeep;
        case ClassWarlock: return WarlockUpkeep;
        default: return {};
    }
}

inline bool Supports(uint8_t playerClass) { return !CombatSteps(playerClass).empty(); }
inline bool IsRanged(uint8_t playerClass) { return playerClass == ClassHunter || playerClass == ClassWarlock; }
inline bool HasPet(uint8_t playerClass) { return playerClass == ClassHunter || playerClass == ClassWarlock; }

// Pure decision pieces, shared by runtime and tests.
inline bool FinisherReady(uint8_t comboPoints, float targetHealthPct)
{
    return comboPoints >= 4 || (comboPoints >= 1 && targetHealthPct < 20.0f);
}
inline bool ComboBuffReady(uint8_t comboPoints, bool auraPresent) { return comboPoints >= 2 && !auraPresent; }
inline bool LifeTapReady(float manaPct, float healthPct) { return manaPct < 30.0f && healthPct > 50.0f; }
// A dot is not worth casting on a target that dies in the next hit or two.
inline bool DotWorthCasting(bool auraPresent, float targetHealthPct) { return !auraPresent && targetHealthPct > 25.0f; }
inline bool PetHurt(bool petAlive, float petHealthPct, bool inCombat)
{
    return petAlive && petHealthPct < (inCombat ? 40.0f : 70.0f);
}
}

#endif
