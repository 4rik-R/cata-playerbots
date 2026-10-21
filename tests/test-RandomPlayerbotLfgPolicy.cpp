/*
 * This file is part of the Cataclysm Playerbots port. See AUTHORS.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "../src/Bot/Random/RandomPlayerbotLfgPolicy.h"
#include <catch2/catch.hpp>

using namespace RandomPlayerbotLfgPolicy;

TEST_CASE("A solo human leaves four roles to bots", "[playerbots][random][lfg]")
{
    CHECK(Missing({ Damage }, {}) == Need { 1, 1, 2 });
    CHECK(Missing({ Tank }, {}) == Need { 0, 1, 3 });
    CHECK(Missing({ Healer | Damage }, {}) == Need { 1, 0, 3 });
    CHECK(Missing({ Tank | Healer | Damage }, {}) == Need { 0, 1, 3 });
}

TEST_CASE("Two humans and queued bots are counted", "[playerbots][random][lfg]")
{
    CHECK(Missing({ Damage, Damage }, {}) == Need { 1, 1, 1 });
    // The human who can only tank tanks; the flexible one heals.
    CHECK(Missing({ Tank | Healer, Tank }, {}) == Need { 0, 0, 3 });
    CHECK(Missing({ Damage }, { Tank, Damage }) == Need { 0, 1, 1 });
    CHECK(Missing({ Damage }, { Tank, Healer, Damage, Damage }).Total() == 0);
    // A surplus bot fills nothing.
    CHECK(Missing({ Tank }, { Tank }) == Need { 0, 1, 3 });
}

TEST_CASE("Bots are queued tank first, then healer, then damage", "[playerbots][random][lfg]")
{
    CHECK(NextRole({ 1, 1, 2 }) == Tank);
    CHECK(NextRole({ 0, 1, 2 }) == Healer);
    CHECK(NextRole({ 0, 0, 2 }) == Damage);
    CHECK(NextRole({ 0, 0, 0 }) == 0);
}

TEST_CASE("Bots near the human's level join, lower ones are lifted", "[playerbots][random][lfg]")
{
    CHECK(JoinsAsIs(20, 20));
    CHECK(JoinsAsIs(17, 20));
    CHECK(JoinsAsIs(23, 20));
    CHECK_FALSE(JoinsAsIs(16, 20));
    CHECK(Liftable(16, 20));
    CHECK_FALSE(Liftable(30, 20));
    CHECK_FALSE(Liftable(20, 20));
}
