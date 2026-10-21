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

TEST_CASE("Random travel: map 530 splits into Outland, the draenei isles and the blood elf lands", "[playerbots][random][travel]")
{
    REQUIRE(RegionOf(0, 9000.0f, -12000.0f) == Mainland);
    REQUIRE(RegionOf(530, -1800.0f, 5300.0f) == Mainland);       // Shattrath
    REQUIRE(RegionOf(530, -4030.0f, -11572.0f) == DraeneiIsles); // the Exodar
    REQUIRE(RegionOf(530, 9500.0f, -7300.0f) == BloodElfLands);  // Silvermoon
}

TEST_CASE("Random travel: voyages chain ships and portals across regions", "[playerbots][random][travel]")
{
    // 0: Stormwind harbor -> Rut'theran (Kalimdor); 1: Menethil -> Theramore; 2: Stormwind portal -> Blasted Lands
    // 3: Dark Portal -> Outland; 4: Rut'theran -> Azuremyst.
    std::vector<Crossing> crossings = {
        { { 0, -8640.0f, 1330.0f }, { 1, 8180.0f, 1000.0f }, ShipCost },
        { { 0, -3900.0f, -600.0f }, { 1, -3830.0f, -4500.0f }, ShipCost },
        { { 0, -9000.0f, 860.0f }, { 0, -11700.0f, -3170.0f }, PortalCost }, // within a region, a shortcut
        { { 0, -11900.0f, -3200.0f }, { 530, -250.0f, 950.0f }, PortalCost },
        { { 1, 8550.0f, 1020.0f }, { 530, -4260.0f, -11300.0f }, ShipCost },
    };
    Spot stormwind { 0, -8833.0f, 622.0f };

    // Darkshore: the harbor ship beats riding to Menethil.
    REQUIRE(PlanCrossings(crossings, stormwind, { 1, 7300.0f, -200.0f }) == std::vector<uint32_t>{ 0 });
    // Dustwallow Marsh from the Wetlands: Menethil.
    REQUIRE(PlanCrossings(crossings, { 0, -3700.0f, -800.0f }, { 1, -3800.0f, -4400.0f }) == std::vector<uint32_t>{ 1 });
    // Outland: the portal to the Blasted Lands, then the Dark Portal.
    REQUIRE(PlanCrossings(crossings, stormwind, { 530, -300.0f, 2000.0f }) == std::vector<uint32_t>{ 2, 3 });
    REQUIRE(PlanCrossings(crossings, { 0, -10500.0f, -2500.0f }, { 530, -300.0f, 2000.0f }) == std::vector<uint32_t>{ 3 });
    // The draenei isles: two ships.
    REQUIRE(PlanCrossings(crossings, stormwind, { 530, -4000.0f, -11600.0f }) == std::vector<uint32_t>{ 0, 4 });
    // Same region, or no way there.
    REQUIRE(PlanCrossings(crossings, stormwind, { 0, -11000.0f, -3000.0f }).empty());
    REQUIRE(PlanCrossings(crossings, stormwind, { 571, 0.0f, 0.0f }).empty());
}

TEST_CASE("Random travel: bots board while a transport waits at the stop", "[playerbots][random][travel]")
{
    REQUIRE_FALSE(AtStop(999, 1000, 30000, 5000));
    REQUIRE(AtStop(1000, 1000, 30000, 5000));
    REQUIRE(AtStop(25999, 1000, 30000, 5000));
    REQUIRE_FALSE(AtStop(26000, 1000, 30000, 5000));
    REQUIRE_FALSE(AtStop(1000, 1000, 4000, 5000));
}
