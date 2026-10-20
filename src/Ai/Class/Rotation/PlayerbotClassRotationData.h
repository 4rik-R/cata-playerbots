/*
 * Priority tables for classes without a full strategy context, following the
 * shape of mod-playerbots' generic DPS strategies (RogueAiObjectContext,
 * HunterAiObjectContext, WarlockAiObjectContext, PaladinAiObjectContext,
 * ShamanAiObjectContext, DruidAiObjectContext) at d750b4b. Spell ids are
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
    SelfBelow20,        // last-resort cooldown
    SelfHotMissing,     // below 60 % and this bot's heal over time (Aura) missing on self
    HolyPower3,         // three charges of holy power
    TargetNotOnMe,      // taunt: the target attacks someone else
    FireTotemMissing,   // no fire totem of this bot near the target
    ComboFinisherDot,   // finisher ready, target healthy and this bot's bleed missing
};

struct Step
{
    uint32_t SpellId;
    char const* Name;
    When Condition;
    Target On;
    uint32_t Aura = 0; // aura to test, defaults to SpellId
    uint32_t Over = 0; // skip this step once the bot knows this better spell
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

// Paladins fight in melee in every spec; Holy heals only in a group.
constexpr Step PaladinRetribution[] = {
    { 96231, "Rebuke", When::TargetCasting, Target::Enemy },
    { 633, "Lay on Hands", When::SelfBelow20, Target::Self },
    { 498, "Divine Protection", When::SelfBelow35, Target::Self },
    { 85673, "Word of Glory", When::SelfBelow40, Target::Self },
    { 19750, "Flash of Light", When::SelfBelow40, Target::Self },
    { 635, "Holy Light", When::SelfBelow40, Target::Self, 0, 19750 },
    { 31801, "Seal of Truth", When::SelfAuraMissing, Target::Self },
    { 20154, "Seal of Righteousness", When::SelfAuraMissing, Target::Self, 0, 31801 },
    { 24275, "Hammer of Wrath", When::TargetBelow20, Target::Enemy },
    { 85256, "Templar's Verdict", When::HolyPower3, Target::Enemy },
    { 35395, "Crusader Strike", When::InMelee, Target::Enemy },
    { 20271, "Judgement", When::Always, Target::Enemy },
    { 879, "Exorcism", When::Always, Target::Enemy },
};

constexpr Step PaladinProtection[] = {
    { 96231, "Rebuke", When::TargetCasting, Target::Enemy },
    { 633, "Lay on Hands", When::SelfBelow20, Target::Self },
    { 498, "Divine Protection", When::SelfBelow35, Target::Self },
    { 85673, "Word of Glory", When::SelfBelow40, Target::Self },
    { 31801, "Seal of Truth", When::SelfAuraMissing, Target::Self },
    { 20154, "Seal of Righteousness", When::SelfAuraMissing, Target::Self, 0, 31801 },
    { 62124, "Hand of Reckoning", When::TargetNotOnMe, Target::Enemy },
    { 31935, "Avenger's Shield", When::Always, Target::Enemy },
    { 53600, "Shield of the Righteous", When::HolyPower3, Target::Enemy },
    { 24275, "Hammer of Wrath", When::TargetBelow20, Target::Enemy },
    { 35395, "Crusader Strike", When::InMelee, Target::Enemy },
    { 20271, "Judgement", When::Always, Target::Enemy },
    { 26573, "Consecration", When::InMelee, Target::Self },
    { 879, "Exorcism", When::Always, Target::Enemy },
};

// Holy alone: Holy Shock as damage, otherwise like Retribution.
constexpr Step PaladinHoly[] = {
    { 96231, "Rebuke", When::TargetCasting, Target::Enemy },
    { 633, "Lay on Hands", When::SelfBelow20, Target::Self },
    { 498, "Divine Protection", When::SelfBelow35, Target::Self },
    { 85673, "Word of Glory", When::SelfBelow40, Target::Self },
    { 19750, "Flash of Light", When::SelfBelow40, Target::Self },
    { 635, "Holy Light", When::SelfBelow40, Target::Self, 0, 19750 },
    { 31801, "Seal of Truth", When::SelfAuraMissing, Target::Self },
    { 20154, "Seal of Righteousness", When::SelfAuraMissing, Target::Self, 0, 31801 },
    { 24275, "Hammer of Wrath", When::TargetBelow20, Target::Enemy },
    { 20473, "Holy Shock", When::Always, Target::Enemy },
    { 35395, "Crusader Strike", When::InMelee, Target::Enemy },
    { 20271, "Judgement", When::Always, Target::Enemy },
    { 879, "Exorcism", When::Always, Target::Enemy },
};

// Elemental and Restoration alone fight from range.
constexpr Step ShamanCaster[] = {
    { 57994, "Wind Shear", When::TargetCasting, Target::Enemy },
    { 8004, "Healing Surge", When::SelfBelow40, Target::Self },
    { 331, "Healing Wave", When::SelfBelow40, Target::Self, 0, 8004 },
    { 324, "Lightning Shield", When::SelfAuraMissing, Target::Self },
    { 3599, "Searing Totem", When::FireTotemMissing, Target::Self },
    { 8050, "Flame Shock", When::MyDotMissing, Target::Enemy },
    { 51505, "Lava Burst", When::Always, Target::Enemy },
    { 403, "Lightning Bolt", When::Always, Target::Enemy },
};

constexpr Step ShamanEnhancement[] = {
    { 57994, "Wind Shear", When::TargetCasting, Target::Enemy },
    { 8004, "Healing Surge", When::SelfBelow40, Target::Self },
    { 331, "Healing Wave", When::SelfBelow40, Target::Self, 0, 8004 },
    { 324, "Lightning Shield", When::SelfAuraMissing, Target::Self },
    { 3599, "Searing Totem", When::FireTotemMissing, Target::Self },
    { 17364, "Stormstrike", When::InMelee, Target::Enemy },
    { 73899, "Primal Strike", When::InMelee, Target::Enemy, 0, 17364 },
    { 60103, "Lava Lash", When::InMelee, Target::Enemy },
    { 8050, "Flame Shock", When::MyDotMissing, Target::Enemy },
    { 8042, "Earth Shock", When::Always, Target::Enemy },
    { 403, "Lightning Bolt", When::NotInMelee, Target::Enemy },
};

// Balance and Restoration alone, and every druid before Cat Form.
constexpr Step DruidCaster[] = {
    { 24858, "Moonkin Form", When::SelfAuraMissing, Target::Self },
    { 8936, "Regrowth", When::SelfBelow40, Target::Self },
    { 774, "Rejuvenation", When::SelfHotMissing, Target::Self },
    { 5570, "Insect Swarm", When::MyDotMissing, Target::Enemy },
    { 8921, "Moonfire", When::MyDotMissing, Target::Enemy },
    { 78674, "Starsurge", When::Always, Target::Enemy },
    { 5176, "Wrath", When::Always, Target::Enemy },
};

constexpr Step DruidCat[] = {
    { 768, "Cat Form", When::SelfAuraMissing, Target::Self },
    { 1079, "Rip", When::ComboFinisherDot, Target::Enemy },
    { 22568, "Ferocious Bite", When::ComboFinisher, Target::Enemy },
    { 1822, "Rake", When::MyDotMissing, Target::Enemy },
    { 33876, "Mangle", When::AnyAuraMissing, Target::Enemy },
    { 1082, "Claw", When::Always, Target::Enemy },
};

// Out of combat, before the next pull.
constexpr Step HunterUpkeep[] = {
    { 13165, "Aspect of the Hawk", When::SelfAuraMissing, Target::Self },
    { 136, "Mend Pet", When::PetHurt, Target::Pet },
};

constexpr Step PaladinUpkeep[] = {
    { 7294, "Retribution Aura", When::SelfAuraMissing, Target::Self },
    { 465, "Devotion Aura", When::SelfAuraMissing, Target::Self, 0, 7294 },
    { 31801, "Seal of Truth", When::SelfAuraMissing, Target::Self },
    { 20154, "Seal of Righteousness", When::SelfAuraMissing, Target::Self, 0, 31801 },
};

// Restoration keeps Water Shield for mana; the others Lightning Shield.
constexpr Step ShamanUpkeep[] = {
    { 324, "Lightning Shield", When::SelfAuraMissing, Target::Self },
};

constexpr Step ShamanRestorationUpkeep[] = {
    { 52127, "Water Shield", When::SelfAuraMissing, Target::Self },
    { 324, "Lightning Shield", When::SelfAuraMissing, Target::Self, 0, 52127 },
};

constexpr Step WarlockUpkeep[] = {
    { 28176, "Fel Armor", When::SelfAuraMissing, Target::Self },
    { 687, "Demon Armor", When::SelfAuraMissing, Target::Self, 0, 28176 },
};

// Pet summons, tried in order when the bot has no pet.
constexpr uint32_t HunterCallPet = 883;
constexpr uint32_t HunterRevivePet = 982;
constexpr uint32_t WarlockVoidwalker = 697;
constexpr uint32_t WarlockImp = 688;
// Shaman weapon imbues, applied to the main hand out of combat.
constexpr uint32_t ShamanWindfury = 8232;
constexpr uint32_t ShamanFlametongue = 8024;
constexpr uint32_t DruidCatForm = 768;

// Self buffs of one group replace each other, so any of them counts as worn.
constexpr uint32_t ExclusiveSelfAuras[][2] = {
    { 28176, 687 },   // Fel Armor, Demon Armor
    { 31801, 20154 }, // Seal of Truth, Seal of Righteousness
    { 7294, 465 },    // Retribution Aura, Devotion Aura
    { 52127, 324 },   // Water Shield, Lightning Shield
};

constexpr uint8_t ClassPaladin = 2, ClassHunter = 3, ClassRogue = 4, ClassShaman = 7, ClassWarlock = 9,
    ClassDruid = 11;

// Talent tree ids (TalentTab.dbc, 4.3.4); 0 is "no tree chosen yet".
constexpr uint32_t TreePaladinHoly = 831, TreePaladinProtection = 839, TreePaladinRetribution = 855;
constexpr uint32_t TreeShamanElemental = 261, TreeShamanEnhancement = 263, TreeShamanRestoration = 262;
constexpr uint32_t TreeDruidBalance = 752, TreeDruidFeral = 750, TreeDruidRestoration = 748;

// catForm: the druid already knows Cat Form; Feral fights as a caster before that.
inline std::span<Step const> CombatSteps(uint8_t playerClass, uint32_t tree = 0, bool catForm = false)
{
    switch (playerClass)
    {
        case ClassRogue: return RogueCombat;
        case ClassHunter: return HunterCombat;
        case ClassWarlock: return WarlockCombat;
        case ClassPaladin:
            return tree == TreePaladinProtection ? std::span<Step const>(PaladinProtection) :
                tree == TreePaladinHoly ? std::span<Step const>(PaladinHoly) : std::span<Step const>(PaladinRetribution);
        case ClassShaman:
            return tree == TreeShamanEnhancement ? std::span<Step const>(ShamanEnhancement) : std::span<Step const>(ShamanCaster);
        case ClassDruid:
            return tree == TreeDruidFeral && catForm ? std::span<Step const>(DruidCat) : std::span<Step const>(DruidCaster);
        default: return {};
    }
}

inline std::span<Step const> UpkeepSteps(uint8_t playerClass, uint32_t tree = 0)
{
    switch (playerClass)
    {
        case ClassHunter: return HunterUpkeep;
        case ClassWarlock: return WarlockUpkeep;
        case ClassPaladin: return PaladinUpkeep;
        case ClassShaman:
            return tree == TreeShamanRestoration ? std::span<Step const>(ShamanRestorationUpkeep) : std::span<Step const>(ShamanUpkeep);
        default: return {};
    }
}

inline bool Supports(uint8_t playerClass) { return !CombatSteps(playerClass).empty(); }
inline bool IsRanged(uint8_t playerClass, uint32_t tree = 0, bool catForm = false)
{
    switch (playerClass)
    {
        case ClassHunter:
        case ClassWarlock: return true;
        case ClassShaman: return tree != TreeShamanEnhancement;
        case ClassDruid: return tree != TreeDruidFeral || !catForm;
        default: return false;
    }
}
inline bool HasPet(uint8_t playerClass) { return playerClass == ClassHunter || playerClass == ClassWarlock; }
// Spell is a member of an exclusive self-buff group; the group's spells, or none.
inline std::span<uint32_t const> ExclusiveGroup(uint32_t spellId)
{
    for (auto const& group : ExclusiveSelfAuras)
        for (uint32_t member : group)
            if (member == spellId)
                return group;
    return {};
}

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
// A bleed finisher only pays off on a target that lives long enough.
inline bool BleedFinisherReady(uint8_t comboPoints, float targetHealthPct, bool auraPresent)
{
    return comboPoints >= 4 && targetHealthPct > 50.0f && !auraPresent;
}
inline bool SelfHotReady(float healthPct, bool auraPresent) { return healthPct < 60.0f && !auraPresent; }
}

#endif
