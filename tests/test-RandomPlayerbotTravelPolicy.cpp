/*
 * This file is part of the Cataclysm Playerbots port. See AUTHORS.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "../src/Bot/Random/RandomPlayerbotTravelPolicy.h"
#include <catch2/catch.hpp>

using namespace RandomPlayerbotTravelPolicy;

TEST_CASE("Random travel: flight routes take the shortest way over the taxi network", "[playerbots][random][travel]")
{
    TaxiLegs legs;
    legs[1] = { { 2, 100.0f, 10 }, { 3, 500.0f, 50 } };
    legs[2] = { { 3, 100.0f, 10 }, { 1, 100.0f, 10 } };
    legs[3] = { { 4, 200.0f, 20 } };

    Route route = FindRoute(legs, 1, 4);
    REQUIRE(route.Nodes == std::vector<uint32_t>{ 1, 2, 3, 4 });
    REQUIRE(route.Cost == 40);

    REQUIRE(FindRoute(legs, 1, 2).Nodes == std::vector<uint32_t>{ 1, 2 });
    REQUIRE(FindRoute(legs, 4, 1).Nodes.empty()); // one-way legs only
    REQUIRE(FindRoute(legs, 1, 9).Nodes.empty());
    REQUIRE(FindRoute(legs, 2, 2).Nodes.empty());
}

TEST_CASE("Random travel: only far goals are worth a flight", "[playerbots][random][travel]")
{
    REQUIRE_FALSE(WorthFlying(RideOnlyRange, 0.0f, 0.0f));
    REQUIRE(WorthFlying(2000.0f, 200.0f, 300.0f));
    REQUIRE_FALSE(WorthFlying(2000.0f, 900.0f, 900.0f));
}
