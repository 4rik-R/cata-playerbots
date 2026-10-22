/*
 * Group heals and mana cooldowns for party healers. See PlayerbotGroupHeal.h,
 * AUTHORS.md and PORTING.md. Released under GNU GPL v2 or any later version.
 */
#include "PlayerbotGroupHeal.h"
#include "PlayerbotCombatDecision.h"
#include "PlayerbotRoles.h"
#include "../Class/Priest/PlayerbotPriestStrategy.h"
#include "Player.h"

bool PlayerbotGroupHeal::Try(Player& bot, Player* owner)
{
    if (!bot.IsAlive() || !bot.GetGroup() || bot.IsNonMeleeSpellCast(false))
        return false;
    std::vector<Player*> members = PlayerbotPriest::HealCandidates(bot, owner);
    std::uint32_t injured = 0;
    Player* mostHurt = nullptr;
    Unit* tankTarget = nullptr;
    for (Player* member : members)
    {
        if (member->GetHealthPct() < InjuredBelowPct)
            ++injured;
        if (!mostHurt || member->GetHealthPct() < mostHurt->GetHealthPct())
            mostHurt = member;
        if (!tankTarget && PlayerbotRoles::IsTankOrMainTank(*member) && member->GetVictim())
            tankTarget = member->GetVictim();
    }
    auto target = [&](Spell const& spell) -> Unit*
    {
        switch (spell.Target)
        {
            case On::Self: return &bot;
            case On::MostHurt: return mostHurt;
            case On::TankTarget: return tankTarget;
        }
        return nullptr;
    };
    if (GroupHealWorth(injured))
        for (Spell const& spell : GroupSpells(bot.getClass()))
        {
            Unit* on = target(spell);
            if (on && bot.HasSpell(spell.SpellId) && bot.GetPower(POWER_HOLY_POWER) >= int32(spell.HolyPower) &&
                PlayerbotDecision::TryCast(bot, *on, spell.SpellId, spell.Name))
                return true;
        }
    int32 maximum = bot.GetMaxPower(POWER_MANA);
    float mana = maximum > 0 ? 100.0f * bot.GetPower(POWER_MANA) / maximum : 100.0f;
    for (Spell const& spell : ManaSpells(bot.getClass()))
    {
        Unit* on = target(spell);
        if (on && bot.HasSpell(spell.SpellId) && ManaSpellWorth(spell, mana, bot.IsInCombat()) &&
            PlayerbotDecision::TryCast(bot, *on, spell.SpellId, spell.Name))
            return true;
    }
    return false;
}
