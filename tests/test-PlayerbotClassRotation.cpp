/*
 * This file is part of the Cataclysm Playerbots port. See AUTHORS.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "../src/Ai/Class/Rotation/PlayerbotClassRotationData.h"
#include <catch2/catch.hpp>
#include <set>

using namespace PlayerbotClassRotation;

TEST_CASE("Class rotation tables cover rogue, hunter and warlock", "[playerbots][rotation]")
{
    for (uint8_t playerClass : { ClassRogue, ClassHunter, ClassWarlock })
    {
        INFO("class " << int(playerClass));
        REQUIRE(Supports(playerClass));
        auto steps = CombatSteps(playerClass);
        // The last step needs no condition so the bot always has something to do.
        REQUIRE((steps.back().Condition == When::Always || steps.back().Condition == When::NotInMelee));
        for (Step const& step : steps)
        {
            REQUIRE(step.SpellId > 0);
            REQUIRE(step.Name != nullptr);
            // Self-only conditions never target the enemy, so CanAutoCast accepts them.
            if (step.Condition == When::SelfBelow35 || step.Condition == When::ComboBuff ||
                step.Condition == When::ManaLowHealthOk)
                REQUIRE(step.On == Target::Self);
            if (step.Condition == When::PetHurt)
                REQUIRE(step.On == Target::Pet);
        }
    }
    REQUIRE_FALSE(Supports(1));
    REQUIRE_FALSE(Supports(2));
    REQUIRE(IsRanged(ClassHunter));
    REQUIRE(IsRanged(ClassWarlock));
    REQUIRE_FALSE(IsRanged(ClassRogue));
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
}

TEST_CASE("Class rotation keeps spell ids unique per table", "[playerbots][rotation]")
{
    for (uint8_t playerClass : { ClassRogue, ClassWarlock })
    {
        std::set<uint32_t> ids;
        for (Step const& step : CombatSteps(playerClass))
            REQUIRE(ids.insert(step.SpellId).second);
    }
}
