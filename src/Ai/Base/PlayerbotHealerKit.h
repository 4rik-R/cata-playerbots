/*
 * Healing spells per class, so one party-healing routine serves every healer.
 * The situations follow mod-playerbots' generic heal strategies
 * (HealPaladinStrategy, HealShamanStrategy, HealDruidStrategy) at d750b4b.
 * Spell ids are Cataclysm 4.3.4 trainer spells or specialization spells.
 * Released under GNU GPL v2 or any later version. See AUTHORS.md and PORTING.md.
 */
#ifndef PLAYERBOT_HEALER_KIT_H
#define PLAYERBOT_HEALER_KIT_H

#include <array>
#include <cstdint>

namespace PlayerbotHealerKit
{
// Each list is tried in order; spells the bot has not learned are skipped.
struct Kit
{
    std::array<std::uint32_t, 3> Emergency{}; // below 55 %
    std::array<std::uint32_t, 3> Heal{};      // below 80 %
    std::uint32_t Hot = 0;                    // below 90 %, when this bot's copy is missing
    std::uint32_t TankBuff = 0;               // kept on the party's tank or controller
    std::uint32_t Resurrect = 0;
};

constexpr std::uint8_t ClassPaladin = 2, ClassPriest = 5, ClassShaman = 7, ClassDruid = 11;

inline Kit For(std::uint8_t playerClass)
{
    switch (playerClass)
    {
        // Holy Shock, Flash of Light, Holy Light; Divine Light, Holy Light, Flash of Light.
        case ClassPaladin: return { { 20473, 19750, 635 }, { 82326, 635, 19750 }, 0, 0, 7328 };
        // Riptide, Healing Surge, Healing Wave; Greater Healing Wave, Healing Wave; Earth Shield.
        case ClassShaman: return { { 61295, 8004, 331 }, { 77472, 331, 8004 }, 61295, 974, 2008 };
        // Swiftmend, Regrowth, Nourish; Healing Touch, Nourish; Rejuvenation; Lifebloom.
        case ClassDruid: return { { 18562, 8936, 50464 }, { 5185, 50464, 8936 }, 774, 33763, 50769 };
        // Flash Heal; Heal; Renew. Priests keep their own strategy; this is for shared checks.
        case ClassPriest: return { { 2061, 0, 0 }, { 2050, 0, 0 }, 139, 0, 2006 };
        default: return {};
    }
}

inline bool Has(std::uint8_t playerClass) { return For(playerClass).Resurrect != 0; }
// Paladins, shamans and druids heal only with a healing specialization.
inline bool IsHybrid(std::uint8_t playerClass)
{
    return playerClass == ClassPaladin || playerClass == ClassShaman || playerClass == ClassDruid;
}

enum class Tier { None, Hot, Heal, Emergency };
inline Tier TierForHealth(float healthPct)
{
    if (healthPct < 55.0f) return Tier::Emergency;
    if (healthPct < 80.0f) return Tier::Heal;
    if (healthPct < 90.0f) return Tier::Hot;
    return Tier::None;
}
}

#endif
