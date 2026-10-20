/*
 * Table-driven combat for rogues, hunters and warlocks, adapted from the
 * generic DPS strategies of mod-playerbots at d750b4b. See AUTHORS.md and
 * PORTING.md. Released under GNU GPL v2 or any later version.
 */
#include "PlayerbotClassRotation.h"
#include "../../Base/PlayerbotCombatDecision.h"
#include "CharmInfo.h"
#include "Creature.h"
#include "Log.h"
#include "Pet.h"
#include "PetAI.h"
#include "Player.h"
#include "Spell.h"
#include "SpellMgr.h"

namespace
{
using namespace PlayerbotClassRotation;

float PowerPct(Player const& bot, PowerType power)
{
    int32 max = bot.GetMaxPower(power);
    return max > 0 ? 100.0f * bot.GetPower(power) / max : 100.0f;
}

Pet* LivePet(Player const& bot)
{
    Pet* pet = bot.GetPet();
    return pet && pet->IsAlive() && pet->IsInWorld() ? pet : nullptr;
}

bool Ready(Player& bot, Creature* target, Step const& step, bool inCombat)
{
    uint32 aura = step.Aura ? step.Aura : step.SpellId;
    switch (step.Condition)
    {
        case When::Always: return true;
        case When::TargetCasting: return target && target->IsNonMeleeSpellCast(false);
        case When::TargetBelow20: return target && target->GetHealthPct() < 20.0f;
        case When::MyDotMissing:
            return target && DotWorthCasting(target->HasAura(aura, bot.GetGUID()), target->GetHealthPct());
        case When::AnyAuraMissing: return target && !target->HasAura(aura);
        case When::SelfAuraMissing:
            if (step.SpellId == WarlockArmors[1] && bot.HasSpell(WarlockArmors[0]))
                return false; // Fel Armor replaces Demon Armor.
            for (uint32 armor : WarlockArmors)
                if (aura == armor)
                    return !bot.HasAura(WarlockArmors[0]) && !bot.HasAura(WarlockArmors[1]);
            return !bot.HasAura(aura);
        case When::ComboFinisher:
            return target && bot.GetComboTarget() == target->GetGUID() &&
                FinisherReady(bot.GetComboPoints(), target->GetHealthPct());
        case When::ComboBuff:
            return target && bot.GetComboTarget() == target->GetGUID() &&
                ComboBuffReady(bot.GetComboPoints(), bot.HasAura(aura));
        case When::SelfBelow35: return bot.GetHealthPct() < 35.0f;
        case When::SelfBelow40: return bot.GetHealthPct() < 40.0f;
        case When::ManaLowHealthOk: return LifeTapReady(PowerPct(bot, POWER_MANA), bot.GetHealthPct());
        case When::InMelee: return target && target->IsWithinMeleeRange(&bot);
        case When::NotInMelee: return target && !target->IsWithinMeleeRange(&bot);
        case When::NoAutoShot:
            return target && !target->IsWithinMeleeRange(&bot) && !bot.GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL);
        case When::FocusAtLeast50: return bot.GetPower(POWER_FOCUS) >= 50;
        case When::PetHurt:
        {
            Pet* pet = LivePet(bot);
            return PetHurt(pet != nullptr, pet ? pet->GetHealthPct() : 0.0f, inCombat);
        }
        case When::PetAttacking:
        {
            Pet* pet = LivePet(bot);
            return pet && target && pet->GetVictim() == target && pet->IsWithinMeleeRange(target);
        }
    }
    return false;
}

bool Run(Player& bot, Creature* target, std::span<Step const> steps, bool inCombat)
{
    for (Step const& step : steps)
    {
        if (!bot.HasSpell(step.SpellId) || !Ready(bot, target, step, inCombat))
            continue;
        Unit* castTarget = step.On == Target::Self ? static_cast<Unit*>(&bot) :
            step.On == Target::Pet ? static_cast<Unit*>(LivePet(bot)) : static_cast<Unit*>(target);
        if (castTarget && PlayerbotDecision::TryCast(bot, *castTarget, step.SpellId, step.Name))
            return true;
    }
    return false;
}

// Native PetAI follows its owner but only defends; send it at the bot's target
// the way CMSG_PET_ACTION COMMAND_ATTACK does.
void SendPet(Player& bot, Creature& target)
{
    Pet* pet = LivePet(bot);
    if (!pet || pet->GetVictim() == &target || !bot.IsValidAttackTarget(&target) || !pet->IsAIEnabled())
        return;
    CharmInfo* charm = pet->GetCharmInfo();
    if (!charm)
        return;
    pet->ClearUnitState(UNIT_STATE_FOLLOW);
    if (pet->GetVictim())
        pet->AttackStop();
    charm->SetIsCommandAttack(true);
    charm->SetIsAtStay(false);
    charm->SetIsFollowing(false);
    charm->SetIsCommandFollow(false);
    charm->SetIsReturning(false);
    if (PetAI* ai = dynamic_cast<PetAI*>(pet->AI()))
        ai->_AttackStart(&target);
    else
        pet->AI()->AttackStart(&target);
}

bool NeedsPet(Player const& bot)
{
    return HasPet(bot.getClass()) && !bot.GetPet() && !bot.IsMounted() &&
        (bot.HasSpell(HunterCallPet) || bot.HasSpell(WarlockImp) || bot.HasSpell(WarlockVoidwalker));
}

bool SummonPet(Player& bot)
{
    if (bot.getClass() == ClassHunter)
        // Call Pet fails while the stable pet is dead; Revive Pet then brings it back.
        return PlayerbotDecision::TryCast(bot, bot, HunterCallPet, "Call Pet") ||
            PlayerbotDecision::TryCast(bot, bot, HunterRevivePet, "Revive Pet");
    return PlayerbotDecision::TryCast(bot, bot, WarlockVoidwalker, "Summon Voidwalker") ||
        PlayerbotDecision::TryCast(bot, bot, WarlockImp, "Summon Imp");
}
}

bool PlayerbotClassRotation::Execute(Player& bot, Creature& target)
{
    if (!Supports(bot.getClass()) || !bot.IsAlive() || bot.IsNonMeleeSpellCast(false))
        return false;
    SendPet(bot, target);
    if (IsRanged(bot.getClass()) && !bot.IsWithinLOSInMap(&target))
        return false;
    if (!bot.HasInArc(2.0f * float(M_PI) / 3.0f, &target))
        bot.SetFacingToObject(&target);
    // A hunter caught in melee swings its weapon until the target is back at range.
    if (bot.getClass() == ClassHunter && target.IsWithinMeleeRange(&bot) && bot.GetVictim() == &target &&
        !bot.HasUnitState(UNIT_STATE_MELEE_ATTACKING))
        bot.Attack(&target, true);
    return Run(bot, &target, CombatSteps(bot.getClass()), true);
}

bool PlayerbotClassRotation::NeedsUpkeep(Player& bot)
{
    if (!Supports(bot.getClass()) || !bot.IsAlive() || bot.IsInCombat() || bot.IsMounted() ||
        bot.IsNonMeleeSpellCast(false))
        return false;
    if (NeedsPet(bot))
        return true;
    for (Step const& step : UpkeepSteps(bot.getClass()))
        if (bot.HasSpell(step.SpellId) && Ready(bot, nullptr, step, false))
            return true;
    return false;
}

bool PlayerbotClassRotation::Upkeep(Player& bot)
{
    if (!NeedsUpkeep(bot))
        return false;
    if (NeedsPet(bot) && SummonPet(bot))
        return true;
    return Run(bot, nullptr, UpkeepSteps(bot.getClass()), false);
}

void PlayerbotClassRotation::LogKnownAbilities(Player const& bot)
{
    std::string known;
    for (Step const& step : CombatSteps(bot.getClass()))
        if (bot.HasSpell(step.SpellId))
            known += std::string(known.empty() ? "" : ", ") + step.Name;
    TC_LOG_DEBUG("module.playerbots", "PB-02: %s level=%u class=%u known: %s pet=%u", bot.GetName().c_str(),
        bot.getLevel(), bot.getClass(), known.c_str(), bot.GetPet() != nullptr);
}

void PlayerbotClassRotation::ValidateSpells()
{
    uint32 missing = 0;
    auto check = [&missing](uint32 spellId, char const* name)
    {
        if (!sSpellMgr->GetSpellInfo(spellId))
        {
            TC_LOG_ERROR("module.playerbots", "PB-ROTATION: spell %u (%s) not in spell data; bots skip it", spellId, name);
            ++missing;
        }
    };
    for (uint8 playerClass : { ClassRogue, ClassHunter, ClassWarlock })
    {
        for (Step const& step : CombatSteps(playerClass))
            check(step.SpellId, step.Name);
        for (Step const& step : UpkeepSteps(playerClass))
            check(step.SpellId, step.Name);
    }
    for (uint32 spellId : { HunterCallPet, HunterRevivePet, WarlockVoidwalker, WarlockImp })
        check(spellId, "pet summon");
    TC_LOG_INFO("module.playerbots", "PB-ROTATION: class tables checked, %u spell(s) missing", missing);
}
