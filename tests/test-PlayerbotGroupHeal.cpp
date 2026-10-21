/*
 * This file is part of the Cataclysm Playerbots port. See AUTHORS.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "../src/Ai/Base/PlayerbotGroupHeal.h"
#include <catch2/catch.hpp>
#include <set>

using namespace PlayerbotGroupHeal;

TEST_CASE("Every party healer has group heals and a mana cooldown", "[playerbots][healing]")
{
    std::set<std::uint32_t> ids;
    for (std::uint8_t playerClass : { ClassPriest, ClassPaladin, ClassShaman, ClassDruid })
    {
        REQUIRE_FALSE(GroupSpells(playerClass).empty());
        REQUIRE_FALSE(ManaSpells(playerClass).empty());
        for (Spell const& spell : GroupSpells(playerClass))
            REQUIRE(ids.insert(spell.SpellId).second);
        for (Spell const& spell : ManaSpells(playerClass))
        {
            REQUIRE(ids.insert(spell.SpellId).second);
            REQUIRE(spell.ManaBelowPct > 0.0f);
        }
    }
    REQUIRE(GroupSpells(1).empty());
}

TEST_CASE("Group heals wait for three injured, mana cooldowns for combat", "[playerbots][healing]")
{
    REQUIRE_FALSE(GroupHealWorth(2));
    REQUIRE(GroupHealWorth(3));
    Spell const& tide = ManaSpells(ClassShaman)[0];
    REQUIRE(ManaSpellWorth(tide, 30.0f, true));
    REQUIRE_FALSE(ManaSpellWorth(tide, 30.0f, false));
    REQUIRE_FALSE(ManaSpellWorth(tide, 45.0f, true));
}
