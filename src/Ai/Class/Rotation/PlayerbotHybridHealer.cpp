/*
 * Party healing for Holy paladins, Restoration shamans and Restoration druids,
 * adapted from mod-playerbots' generic heal strategies at d750b4b. See
 * AUTHORS.md and PORTING.md. Released under GNU GPL v2 or any later version.
 */
#include "PlayerbotHybridHealer.h"
#include "../../Base/PlayerbotCombatDecision.h"
#include "../../Base/PlayerbotHealerKit.h"
#include "../../Base/PlayerbotHealingMana.h"
#include "../../Base/PlayerbotRoles.h"
#include "../Priest/PlayerbotPriestStrategy.h"
#include "../../../Script/PlayerbotConfig.h"
#include "Player.h"

namespace
{
using PlayerbotHealingMana::Job;

bool ManaAllows(Player const& bot, Player const& target, Job job)
{
    int32 maximum = bot.GetMaxPower(POWER_MANA);
    float mana = maximum > 0 ? 100.0f * bot.GetPower(POWER_MANA) / maximum : 0.0f;
    return PlayerbotHealingMana::Allows(PlayerbotModuleHealerSaveManaEnabled(), mana, target.GetHealthPct(),
        PlayerbotRoles::IsTank(target), PlayerbotHealingMana::ForJob(job));
}

template <std::size_t N>
bool TryFirst(Player& bot, Player& target, std::array<std::uint32_t, N> const& spells, Job job, char const* name)
{
    for (std::uint32_t spell : spells)
        if (spell && bot.HasSpell(spell) && ManaAllows(bot, target, job) &&
            PlayerbotDecision::TryCast(bot, target, spell, name))
            return true;
    return false;
}

bool HealTarget(Player& bot, Player& target, PlayerbotHealerKit::Kit const& kit)
{
    using PlayerbotHealerKit::Tier;
    if (PlayerbotPriest::ShouldDeferHealing(bot, target))
        return false;
    switch (PlayerbotHealerKit::TierForHealth(target.GetHealthPct()))
    {
        case Tier::Emergency:
            if (TryFirst(bot, target, kit.Emergency, Job::Emergency, "emergency heal"))
                return true;
            [[fallthrough]];
        case Tier::Heal:
            if (TryFirst(bot, target, kit.Heal, Job::Heal, "heal"))
                return true;
            [[fallthrough]];
        case Tier::Hot:
            return kit.Hot && bot.HasSpell(kit.Hot) && !target.HasAura(kit.Hot, bot.GetGUID()) &&
                ManaAllows(bot, target, Job::Hot) && PlayerbotDecision::TryCast(bot, target, kit.Hot, "heal over time");
        case Tier::None:
            return false;
    }
    return false;
}

// Earth Shield stays on the tank; Lifebloom only while the party fights.
bool KeepTankBuff(Player& bot, Player* owner, PlayerbotHealerKit::Kit const& kit)
{
    if (!kit.TankBuff || !bot.HasSpell(kit.TankBuff))
        return false;
    Player* tank = nullptr;
    for (Player* member : PlayerbotPriest::HealCandidates(bot, owner))
        if (member != &bot && PlayerbotRoles::IsTankOrMainTank(*member))
        {
            tank = member;
            break;
        }
    if (!tank)
        tank = owner && owner != &bot ? owner : nullptr;
    if (!tank || tank->HasAura(kit.TankBuff, bot.GetGUID()))
        return false;
    if (bot.getClass() == PlayerbotHealerKit::ClassDruid && !tank->IsInCombat())
        return false;
    return PlayerbotDecision::TryCast(bot, *tank, kit.TankBuff, "tank buff");
}
}

bool PlayerbotHybridHealer::IsPartyHealer(Player const& bot)
{
    return PlayerbotHealerKit::IsHybrid(bot.getClass()) && PlayerbotRoles::IsHealer(bot);
}

bool PlayerbotHybridHealer::AnyoneNeedsHealing(Player& bot, Player* owner)
{
    for (Player* member : PlayerbotPriest::HealCandidates(bot, owner))
        if (member->GetHealthPct() < 90.0f)
            return true;
    return false;
}

bool PlayerbotHybridHealer::HealParty(Player& bot, Player* owner)
{
    if (!IsPartyHealer(bot) || !bot.IsAlive() || bot.IsNonMeleeSpellCast(false))
        return false;
    PlayerbotHealerKit::Kit kit = PlayerbotHealerKit::For(bot.getClass());
    std::vector<Player*> candidates = PlayerbotPriest::HealCandidates(bot, owner);
    if (PlayerbotPriest::TryInHealthOrder(candidates, [](Player* member) { return member->GetHealthPct(); },
        [&](Player* member) { return bot.GetExactDist2d(member); },
        [&](Player* target) { return HealTarget(bot, *target, kit); }))
        return true;
    if (KeepTankBuff(bot, owner, kit))
        return true;
    if (Player* dead = PlayerbotPriest::ResurrectionTarget(bot, 30.0f, owner))
        return PlayerbotDecision::TryCast(bot, *dead, kit.Resurrect, "resurrection");
    return false;
}
