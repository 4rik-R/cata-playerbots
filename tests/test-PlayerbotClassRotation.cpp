/*
 * This file is part of the Cataclysm Playerbots port. See AUTHORS.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "../src/Ai/Class/Rotation/PlayerbotClassRotationData.h"
#include <catch2/catch.hpp>
#include <set>

using namespace PlayerbotClassRotation;

namespace
{
struct Spec { uint8_t Class; uint32_t Tree; bool CatForm; };
constexpr Spec AllSpecs[] = {
    { ClassRogue, 0, false }, { ClassHunter, 0, false }, { ClassWarlock, 0, false },
    { ClassPaladin, TreePaladinHoly, false }, { ClassPaladin, TreePaladinProtection, false },
    { ClassPaladin, TreePaladinRetribution, false }, { ClassPaladin, 0, false },
    { ClassShaman, TreeShamanElemental, false }, { ClassShaman, TreeShamanEnhancement, false },
    { ClassShaman, TreeShamanRestoration, false }, { ClassShaman, 0, false },
    { ClassDruid, TreeDruidBalance, false }, { ClassDruid, TreeDruidFeral, true },
    { ClassDruid, TreeDruidFeral, false }, { ClassDruid, TreeDruidRestoration, false }, { ClassDruid, 0, false },
    { ClassDeathKnight, TreeDeathKnightBlood, false }, { ClassDeathKnight, TreeDeathKnightFrost, false },
    { ClassDeathKnight, TreeDeathKnightUnholy, false }, { ClassDeathKnight, 0, false },
};
}

TEST_CASE("Class rotation tables cover every table class and spec", "[playerbots][rotation]")
{
    for (Spec spec : AllSpecs)
    {
        INFO("class " << int(spec.Class) << " tree " << spec.Tree);
        uint8_t playerClass = spec.Class;
        REQUIRE(Supports(playerClass));
        auto steps = CombatSteps(playerClass, spec.Tree, spec.CatForm);
        // The last step needs no condition so the bot always has something to do.
        REQUIRE((steps.back().Condition == When::Always || steps.back().Condition == When::NotInMelee));
        for (Step const& step : steps)
        {
            REQUIRE(step.SpellId > 0);
            REQUIRE(step.Name != nullptr);
            // Self-only conditions never target the enemy, so CanAutoCast accepts them.
            if (step.Condition == When::SelfBelow35 || step.Condition == When::ComboBuff ||
                step.Condition == When::ManaLowHealthOk || step.Condition == When::SelfBelow20 ||
                step.Condition == When::SelfBelow40 || step.Condition == When::SelfHotMissing ||
                step.Condition == When::SelfAuraMissing || step.Condition == When::FireTotemMissing)
                // Drain Life and Death Strike heal by hitting the enemy.
                REQUIRE((step.On == Target::Self || step.SpellId == 689 || step.SpellId == 49998));
            // A step replaced by a better spell must not replace itself.
            REQUIRE(step.Over != step.SpellId);
            if (step.Condition == When::PetHurt)
                REQUIRE(step.On == Target::Pet);
        }
    }
    REQUIRE_FALSE(Supports(1));
    REQUIRE(Supports(ClassDeathKnight));
    REQUIRE_FALSE(IsRanged(ClassDeathKnight, TreeDeathKnightFrost));
    REQUIRE(IsRanged(ClassHunter));
    REQUIRE(IsRanged(ClassWarlock));
    REQUIRE_FALSE(IsRanged(ClassRogue));
    REQUIRE_FALSE(IsRanged(ClassPaladin, TreePaladinHoly));
    REQUIRE(IsRanged(ClassShaman, TreeShamanRestoration));
    REQUIRE_FALSE(IsRanged(ClassShaman, TreeShamanEnhancement));
    REQUIRE(IsRanged(ClassDruid, TreeDruidFeral, false)); // no Cat Form yet
    REQUIRE_FALSE(IsRanged(ClassDruid, TreeDruidFeral, true));
    REQUIRE(HasPet(ClassHunter));
    REQUIRE_FALSE(HasPet(ClassRogue));
}

TEST_CASE("Class rotation decisions", "[playerbots][rotation]")
{
    REQUIRE_FALSE(FinisherReady(3, 80.0f));
    REQUIRE(FinisherReady(4, 80.0f));
    REQUIRE(FinisherReady(1, 15.0f));
    REQUIRE_FALSE(FinisherReady(0, 5.0f));
    REQUIRE(ComboBuffReady(2, false));
    REQUIRE_FALSE(ComboBuffReady(2, true));
    REQUIRE_FALSE(ComboBuffReady(1, false));
    REQUIRE(LifeTapReady(20.0f, 80.0f));
    REQUIRE_FALSE(LifeTapReady(20.0f, 40.0f));
    REQUIRE_FALSE(LifeTapReady(50.0f, 80.0f));
    REQUIRE(DotWorthCasting(false, 90.0f));
    REQUIRE_FALSE(DotWorthCasting(true, 90.0f));
    REQUIRE_FALSE(DotWorthCasting(false, 20.0f));
    REQUIRE(PetHurt(true, 50.0f, false));
    REQUIRE_FALSE(PetHurt(true, 50.0f, true));
    REQUIRE_FALSE(PetHurt(false, 10.0f, true));
    REQUIRE(BleedFinisherReady(5, 80.0f, false));
    REQUIRE_FALSE(BleedFinisherReady(5, 30.0f, false));
    REQUIRE_FALSE(BleedFinisherReady(3, 80.0f, false));
    REQUIRE_FALSE(BleedFinisherReady(5, 80.0f, true));
    REQUIRE(SelfHotReady(50.0f, false));
    REQUIRE_FALSE(SelfHotReady(70.0f, false));
    REQUIRE_FALSE(SelfHotReady(50.0f, true));
}

TEST_CASE("Class rotation picks tables by specialization", "[playerbots][rotation]")
{
    REQUIRE(CombatSteps(ClassPaladin, TreePaladinProtection).data() == PaladinProtection);
    REQUIRE(CombatSteps(ClassPaladin, TreePaladinHoly).data() == PaladinHoly);
    REQUIRE(CombatSteps(ClassPaladin, 0).data() == PaladinRetribution);
    REQUIRE(CombatSteps(ClassShaman, TreeShamanEnhancement).data() == ShamanEnhancement);
    REQUIRE(CombatSteps(ClassShaman, TreeShamanRestoration).data() == ShamanCaster);
    REQUIRE(CombatSteps(ClassDruid, TreeDruidFeral, true).data() == DruidCat);
    REQUIRE(CombatSteps(ClassDruid, TreeDruidFeral, false).data() == DruidCaster);
    REQUIRE(UpkeepSteps(ClassShaman, TreeShamanRestoration).data() == ShamanRestorationUpkeep);
    REQUIRE(UpkeepSteps(ClassShaman, TreeShamanElemental).data() == ShamanUpkeep);
    REQUIRE(UpkeepSteps(ClassDruid).empty());
    REQUIRE(CombatSteps(ClassDeathKnight, TreeDeathKnightBlood).data() == DeathKnightBlood);
    REQUIRE(CombatSteps(ClassDeathKnight, TreeDeathKnightUnholy).data() == DeathKnightUnholy);
    REQUIRE(CombatSteps(ClassDeathKnight, 0).data() == DeathKnightFrost);
    REQUIRE(UpkeepSteps(ClassDeathKnight, TreeDeathKnightBlood).data() == DeathKnightBloodUpkeep);
    REQUIRE(UpkeepSteps(ClassDeathKnight, TreeDeathKnightUnholy).data() == DeathKnightUnholyUpkeep);
    REQUIRE(UpkeepSteps(ClassDeathKnight, TreeDeathKnightFrost).data() == DeathKnightFrostUpkeep);
    // Only the prot table taunts; only Retribution spends holy power on damage.
    auto has = [](std::span<Step const> steps, uint32_t spell)
    {
        for (Step const& step : steps)
            if (step.SpellId == spell) return true;
        return false;
    };
    REQUIRE(has(PaladinProtection, 62124));
    REQUIRE_FALSE(has(PaladinRetribution, 62124));
    REQUIRE(has(PaladinRetribution, 85256));
    // Only Blood taunts; every death knight spec applies both diseases.
    REQUIRE(has(DeathKnightBlood, 56222));
    REQUIRE_FALSE(has(DeathKnightFrost, 56222));
    REQUIRE_FALSE(has(DeathKnightUnholy, 49576));
    for (auto steps : { std::span<Step const>(DeathKnightBlood), std::span<Step const>(DeathKnightFrost),
        std::span<Step const>(DeathKnightUnholy) })
    {
        REQUIRE(has(steps, 45477));
        REQUIRE(has(steps, 45462));
        for (Step const& step : steps)
            if (step.SpellId == 45477 || step.SpellId == 45462)
                REQUIRE((step.Condition == When::MyDotMissing && (step.Aura == 55095 || step.Aura == 55078)));
    }
}

TEST_CASE("Exclusive self buffs count each other as worn", "[playerbots][rotation]")
{
    REQUIRE(ExclusiveGroup(687).size() == 2);
    REQUIRE(ExclusiveGroup(31801)[1] == 20154);
    REQUIRE(ExclusiveGroup(324)[0] == 52127);
    REQUIRE(ExclusiveGroup(768).empty());
    REQUIRE(ExclusiveGroup(0).empty());
    // Presences: three members, and no padding zero counts as a member.
    REQUIRE(ExclusiveGroup(48265).size() == 3);
    REQUIRE(ExclusiveGroup(48263)[1] == 48266);
    for (auto const& group : ExclusiveSelfAuras)
        REQUIRE(ExclusiveGroup(group[0]).size() >= 2);
}

TEST_CASE("Class rotation keeps spell ids unique per table", "[playerbots][rotation]")
{
    for (Spec spec : AllSpecs)
    {
        if (spec.Class == ClassHunter)
            continue; // Arcane Shot appears twice on purpose
        std::set<uint32_t> ids;
        for (Step const& step : CombatSteps(spec.Class, spec.Tree, spec.CatForm))
            REQUIRE(ids.insert(step.SpellId).second);
    }
}

TEST_CASE("Tanking feral druids fight as bears in melee", "[playerbots][rotation]")
{
    auto bear = CombatSteps(ClassDruid, TreeDruidFeral, true, true);
    REQUIRE(bear.data() == std::begin(DruidBear));
    REQUIRE(bear[0].SpellId == DruidBearForm);
    REQUIRE_FALSE(IsRanged(ClassDruid, TreeDruidFeral, false, true));
    // A bear without the feral tree is still a caster.
    REQUIRE(CombatSteps(ClassDruid, TreeDruidBalance, true, true).data() == std::begin(DruidCaster));
    std::set<uint32_t> ids;
    for (Step const& step : bear)
        REQUIRE(ids.insert(step.SpellId).second);
    // About half of the feral druids tank.
    uint32_t tanks = 0;
    for (uint32_t guid = 1; guid <= 1000; ++guid)
        tanks += FeralTank(guid) ? 1 : 0;
    REQUIRE(tanks > 400);
    REQUIRE(tanks < 600);
}

TEST_CASE("Area spells need three enemies and no crowd control", "[playerbots][rotation]")
{
    REQUIRE(AoeWorthCasting(3, false));
    REQUIRE_FALSE(AoeWorthCasting(2, false));
    REQUIRE_FALSE(AoeWorthCasting(5, true));
    REQUIRE(AoeSteps(ClassWarrior).size() == 3);
    REQUIRE(AoeSteps(ClassMage)[0].SpellId == 2120);
    REQUIRE(AoeSteps(ClassRogue).empty());
}
