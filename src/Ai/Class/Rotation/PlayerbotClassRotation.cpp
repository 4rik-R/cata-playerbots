/*
 * Table-driven combat for rogues, hunters, warlocks, paladins, shamans,
 * druids and death knights, adapted from the generic strategies of mod-playerbots at d750b4b. See AUTHORS.md and
 * PORTING.md. Released under GNU GPL v2 or any later version.
 */
#include "PlayerbotClassRotation.h"
#include "../../Base/PlayerbotCombatDecision.h"
#include "CharmInfo.h"
#include "Creature.h"
#include "Log.h"
#include "Map.h"
#include "Pet.h"
#include "PetAI.h"
#include "Player.h"
#include "Item.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"

namespace
{
using namespace PlayerbotClassRotation;

float PowerPct(Player const& bot, PowerType power)
{
    int32 max = bot.GetMaxPower(power);
    return max > 0 ? 100.0f * bot.GetPower(power) / max : 100.0f;
}

uint32 Tree(Player const& bot) { return bot.GetPrimaryTalentTree(bot.GetActiveSpec()); }

std::span<Step const> Combat(Player const& bot)
{
    return CombatSteps(bot.getClass(), Tree(bot), bot.HasSpell(DruidCatForm));
}

// Any member of the step's exclusive group already worn counts as present.
bool SelfAuraPresent(Player const& bot, uint32 aura)
{
    std::span<uint32_t const> group = ExclusiveGroup(aura);
    if (group.empty())
        return bot.HasAura(aura);
    for (uint32 member : group)
        if (bot.HasAura(member))
            return true;
    return false;
}

// The bot's own fire totem, still close enough to reach the target.
bool FireTotemNear(Player const& bot, Creature const* target)
{
    ObjectGuid guid = bot.m_SummonSlot[SUMMON_SLOT_TOTEM_FIRE];
    Creature* totem = guid.IsEmpty() || !bot.IsInWorld() ? nullptr : bot.GetMap()->GetCreature(guid);
    return totem && totem->IsAlive() && (!target || totem->IsWithinDistInMap(target, 20.0f));
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
        case When::SelfAuraMissing: return !SelfAuraPresent(bot, aura);
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
        case When::SelfBelow20: return bot.GetHealthPct() < 20.0f;
        case When::SelfHotMissing: return SelfHotReady(bot.GetHealthPct(), bot.HasAura(aura, bot.GetGUID()));
        case When::HolyPower3: return bot.GetPower(POWER_HOLY_POWER) >= 3;
        case When::TargetNotOnMe: return target && target->GetVictim() && target->GetVictim() != &bot;
        case When::FireTotemMissing: return target && inCombat && !FireTotemNear(bot, target);
        case When::ComboFinisherDot:
            return target && bot.GetComboTarget() == target->GetGUID() &&
                BleedFinisherReady(bot.GetComboPoints(), target->GetHealthPct(), target->HasAura(aura, bot.GetGUID()));
    }
    return false;
}

bool Run(Player& bot, Creature* target, std::span<Step const> steps, bool inCombat)
{
    for (Step const& step : steps)
    {
        if (!bot.HasSpell(step.SpellId) || (step.Over && bot.HasSpell(step.Over)) || !Ready(bot, target, step, inCombat))
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

// Shaman weapon imbue on the main hand; enhancement prefers Windfury.
uint32 WantedImbue(Player const& bot)
{
    if (bot.getClass() != ClassShaman)
        return 0;
    Item* weapon = bot.GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!weapon || weapon->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
        return 0;
    if (Tree(bot) == TreeShamanEnhancement && bot.HasSpell(ShamanWindfury))
        return ShamanWindfury;
    return bot.HasSpell(ShamanFlametongue) ? ShamanFlametongue : 0;
}

// Imbues target an item, so the unit-target TryCast does not fit.
bool ApplyImbue(Player& bot, uint32 spellId)
{
    Item* weapon = bot.GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!weapon || !info || bot.GetSpellHistory()->HasGlobalCooldown(info) || bot.GetSpellHistory()->HasCooldown(info))
        return false;
    Spell* spell = new Spell(&bot, info, TRIGGERED_NONE);
    SpellCastTargets targets;
    targets.SetItemTarget(weapon);
    if (spell->prepare(targets) != SPELL_CAST_OK)
        return false;
    TC_LOG_DEBUG("module.playerbots", "PB-02: %s began weapon imbue %u", bot.GetName().c_str(), spellId);
    return true;
}

// Druids leave cat and moonkin form after combat so they can eat, drink and mount.
bool InCombatForm(Player const& bot)
{
    return bot.getClass() == ClassDruid && bot.HasAuraType(SPELL_AURA_MOD_SHAPESHIFT);
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
    if (FightsAtRange(bot) && !bot.IsWithinLOSInMap(&target))
        return false;
    if (!bot.HasInArc(2.0f * float(M_PI) / 3.0f, &target))
        bot.SetFacingToObject(&target);
    // A hunter caught in melee swings its weapon until the target is back at range.
    if (bot.getClass() == ClassHunter && target.IsWithinMeleeRange(&bot) && bot.GetVictim() == &target &&
        !bot.HasUnitState(UNIT_STATE_MELEE_ATTACKING))
        bot.Attack(&target, true);
    return Run(bot, &target, Combat(bot), true);
}

bool PlayerbotClassRotation::FightsAtRange(Player const& bot)
{
    return IsRanged(bot.getClass(), Tree(bot), bot.HasSpell(DruidCatForm));
}

bool PlayerbotClassRotation::NeedsUpkeep(Player& bot)
{
    if (!Supports(bot.getClass()) || !bot.IsAlive() || bot.IsInCombat() || bot.IsMounted() ||
        bot.IsNonMeleeSpellCast(false))
        return false;
    if (NeedsPet(bot) || WantedImbue(bot) || InCombatForm(bot))
        return true;
    for (Step const& step : UpkeepSteps(bot.getClass(), Tree(bot)))
        if (bot.HasSpell(step.SpellId) && !(step.Over && bot.HasSpell(step.Over)) && Ready(bot, nullptr, step, false))
            return true;
    return false;
}

bool PlayerbotClassRotation::Upkeep(Player& bot)
{
    if (!NeedsUpkeep(bot))
        return false;
    if (InCombatForm(bot))
    {
        bot.RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
        return true;
    }
    if (NeedsPet(bot) && SummonPet(bot))
        return true;
    if (uint32 imbue = WantedImbue(bot))
        if (ApplyImbue(bot, imbue))
            return true;
    return Run(bot, nullptr, UpkeepSteps(bot.getClass(), Tree(bot)), false);
}

void PlayerbotClassRotation::LogKnownAbilities(Player const& bot)
{
    std::string known;
    for (Step const& step : Combat(bot))
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
    struct Spec { uint8 Class; uint32 Tree; bool CatForm; };
    for (Spec spec : std::initializer_list<Spec>{ { ClassRogue, 0, false }, { ClassHunter, 0, false },
        { ClassWarlock, 0, false }, { ClassPaladin, TreePaladinHoly, false },
        { ClassPaladin, TreePaladinProtection, false }, { ClassPaladin, TreePaladinRetribution, false },
        { ClassShaman, TreeShamanElemental, false }, { ClassShaman, TreeShamanEnhancement, false },
        { ClassShaman, TreeShamanRestoration, false }, { ClassDruid, TreeDruidBalance, false },
        { ClassDruid, TreeDruidFeral, true }, { ClassDeathKnight, TreeDeathKnightBlood, false },
        { ClassDeathKnight, TreeDeathKnightFrost, false }, { ClassDeathKnight, TreeDeathKnightUnholy, false } })
    {
        for (Step const& step : CombatSteps(spec.Class, spec.Tree, spec.CatForm))
            check(step.SpellId, step.Name);
        for (Step const& step : UpkeepSteps(spec.Class, spec.Tree))
            check(step.SpellId, step.Name);
    }
    for (uint32 spellId : { HunterCallPet, HunterRevivePet, WarlockVoidwalker, WarlockImp, ShamanWindfury,
        ShamanFlametongue })
        check(spellId, "pet summon or imbue");
    TC_LOG_INFO("module.playerbots", "PB-ROTATION: class tables checked, %u spell(s) missing", missing);
}
