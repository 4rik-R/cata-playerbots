/* Adapted from mod-playerbots HealerAutoSaveManaMultiplier::GetValue and
 * PriestActions.h at 037c01418b5d01506917a3db9b44fd56ac5f965c.
 * GPL v2 or later. See AUTHORS.md and PORTING.md. */
#ifndef PLAYERBOT_HEALING_MANA_H
#define PLAYERBOT_HEALING_MANA_H
#include <cmath>
#include <cstdint>

namespace PlayerbotHealingMana
{
enum class Efficiency { Low, Medium, VeryHigh };
struct Estimate { std::uint8_t Amount; Efficiency Mana; };
inline Estimate ForPriestSpell(std::uint32_t spell)
{
    switch (spell)
    {
        case 2050: return {50, Efficiency::Medium}; // heal on party
        case 2061: return {15, Efficiency::Low};    // flash heal on party
        case 17:                                  // shield on party
        case 139: return {15, Efficiency::VeryHigh}; // renew on party
        default: return {0, Efficiency::VeryHigh}; // Unported actions are unaffected.
    }
}
// Other healers map their spells to the priest spell of the same job:
// fast emergency heal, efficient heal, heal over time.
enum class Job { Emergency, Heal, Hot };
inline Estimate ForJob(Job job)
{
    switch (job)
    {
        case Job::Emergency: return ForPriestSpell(2061);
        case Job::Heal: return ForPriestSpell(2050);
        default: return ForPriestSpell(139);
    }
}
// Donor percentages are integer snapshots; the estimated amount is scheduling
// metadata, not a prediction or replacement for native Cata healing formulas.
inline bool Allows(bool enabled, float mana, float health, bool tank, Estimate estimate)
{
    if (!enabled || !estimate.Amount) return true;
    if (!std::isfinite(mana) || !std::isfinite(health) || mana < 0 || mana > 100 || health <= 0 || health > 100)
        return false;
    if (std::uint8_t(mana) > 60) return true;
    std::uint8_t pct = std::uint8_t(health);
    std::uint8_t amount = tank ? std::uint8_t(estimate.Amount / 1.5) : estimate.Amount;
    unsigned loss = 100 - pct;
    if (pct >= 65 && (loss < amount || estimate.Mana <= Efficiency::Medium)) return false;
    if (tank)
        return pct < 45 || (loss >= amount && estimate.Mana > Efficiency::Low);
    return loss >= amount && estimate.Mana > Efficiency::Low;
}
}
#endif
