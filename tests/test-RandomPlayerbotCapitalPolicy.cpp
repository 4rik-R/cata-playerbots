/*
 * This file is part of the Cataclysm Playerbots port. See AUTHORS.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "../src/Bot/Random/RandomPlayerbotCapitalPolicy.h"
#include <catch2/catch.hpp>

using namespace RandomPlayerbotCapitalPolicy;

TEST_CASE("Random capital: bots go home every few levels or when worn down", "[playerbots][random][capital]")
{
    REQUIRE(FirstVisitLevel(20, 0) == 21);
    REQUIRE(FirstVisitLevel(20, 4) == 25);
    REQUIRE(FirstVisitLevel(20, 5) == 21);
    REQUIRE(NextVisitLevel(23) == 28);

    REQUIRE_FALSE(VisitDue(22, 23, 100, false));
    REQUIRE(VisitDue(23, 23, 100, false));
    REQUIRE(VisitDue(22, 23, WornPercent, false));
    REQUIRE_FALSE(VisitDue(22, 23, WornPercent + 1, false));
    REQUIRE(VisitDue(22, 23, 100, true));
}

TEST_CASE("Random capital: at most a fifth of the bots are in a capital", "[playerbots][random][capital]")
{
    REQUIRE(RoomInCapitals(0, 0));
    REQUIRE_FALSE(RoomInCapitals(1, 4));
    REQUIRE(RoomInCapitals(9, 50));
    REQUIRE_FALSE(RoomInCapitals(10, 50));
    REQUIRE(RoomInCapitals(59, 300));
    REQUIRE_FALSE(RoomInCapitals(60, 300));
}
