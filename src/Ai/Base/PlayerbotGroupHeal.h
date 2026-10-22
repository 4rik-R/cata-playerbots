/*
 * Group heals and mana cooldowns for party healers, following mod-playerbots'
 * healer strategies (Prayer of Healing, Chain Heal, Wild Growth, Light of Dawn;
 * Hymn of Hope, Mana Tide Totem, Innervate) at d750b4b. Spell ids are
 * Cataclysm 4.3.4 trainer or specialization spells. See AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef PLAYERBOT_GROUP_HEAL_H
#define PLAYERBOT_GROUP_HEAL_H

#include <cstdint>
#include <span>

class Player;

namespace PlayerbotGroupHeal
{
// A group heal is worth it once this many members are below this much health.
constexpr std::uint32_t InjuredForGroupHeal = 3;
constexpr float InjuredBelowPct = 75.0f;

enum class On : std::uint8_t { Self, MostHurt, TankTarget };

struct Spell
{
    std::uint32_t SpellId;
    char const* Name;
    On Target;
    float ManaBelowPct = 0.0f;     // mana cooldowns: only below this much mana
    std::uint32_t HolyPower = 0;   // paladin: holy power needed
};

constexpr std::uint8_t ClassPaladin = 2, ClassPriest = 5, ClassShaman = 7, ClassDruid = 11;

constexpr Spell PriestGroup[] = {
    { 34861, "Circle of Healing", On::MostHurt },
    { 596, "Prayer of Healing", On::MostHurt },
};
constexpr Spell PaladinGroup[] = {
    { 85222, "Light of Dawn", On::Self, 0.0f, 3 },
    { 82327, "Holy Radiance", On::Self },
};
constexpr Spell ShamanGroup[] = {
    { 1064, "Chain Heal", On::MostHurt },
};
constexpr Spell DruidGroup[] = {
    { 48438, "Wild Growth", On::MostHurt },
};

constexpr Spell PriestMana[] = {
    { 34433, "Shadowfiend", On::TankTarget, 50.0f },
    { 64901, "Hymn of Hope", On::Self, 25.0f },
};
constexpr Spell PaladinMana[] = {
    { 54428, "Divine Plea", On::Self, 50.0f },
};
constexpr Spell ShamanMana[] = {
    { 16190, "Mana Tide Totem", On::Self, 40.0f },
};
constexpr Spell DruidMana[] = {
    { 29166, "Innervate", On::Self, 40.0f },
};

inline std::span<Spell const> GroupSpells(std::uint8_t playerClass)
{
    switch (playerClass)
    {
        case ClassPriest: return PriestGroup;
        case ClassPaladin: return PaladinGroup;
        case ClassShaman: return ShamanGroup;
        case ClassDruid: return DruidGroup;
        default: return {};
    }
}

inline std::span<Spell const> ManaSpells(std::uint8_t playerClass)
{
    switch (playerClass)
    {
        case ClassPriest: return PriestMana;
        case ClassPaladin: return PaladinMana;
        case ClassShaman: return ShamanMana;
        case ClassDruid: return DruidMana;
        default: return {};
    }
}

inline bool GroupHealWorth(std::uint32_t injured) { return injured >= InjuredForGroupHeal; }
inline bool ManaSpellWorth(Spell const& spell, float manaPct, bool inCombat)
{
    return inCombat && spell.ManaBelowPct > 0.0f && manaPct < spell.ManaBelowPct;
}

// Map-thread only: one group heal or mana cooldown when the situation calls for it.
bool Try(Player& bot, Player* owner);
}

#endif
