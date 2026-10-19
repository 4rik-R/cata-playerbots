/*
 * This file is part of the Cataclysm Playerbots port. See AUTHORS.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "../src/Bot/Random/RandomPlayerbotLevels.h"
#include <catch2/catch.hpp>
#include <set>
#include <vector>

using namespace RandomPlayerbotLevels;

TEST_CASE("Random levels: every level has a zone for both factions", "[playerbots][random]")
{
    for (uint32_t level = 1; level <= MaxLevel; ++level)
    {
        INFO("level " << level);
        CHECK(AnyZoneFor(level, Alliance));
        CHECK(AnyZoneFor(level, Horde));
    }
}

TEST_CASE("Random levels: zone table is consistent", "[playerbots][random]")
{
    std::set<uint32_t> ids;
    for (Zone const& zone : Zones)
    {
        INFO("zone " << zone.Id);
        CHECK(ids.insert(zone.Id).second);
        CHECK(zone.MinLevel >= 1);
        CHECK(zone.MinLevel <= zone.MaxLevel);
        CHECK(zone.MaxLevel <= MaxLevel);
        CHECK((zone.Teams & Both) != 0);
    }
}

TEST_CASE("Random levels: zone fit honours faction and slack", "[playerbots][random]")
{
    CHECK(ZoneFits(12, 5, Alliance));     // Elwynn
    CHECK_FALSE(ZoneFits(12, 5, Horde));
    CHECK(ZoneFits(12, 12, Alliance));    // two levels of slack
    CHECK_FALSE(ZoneFits(12, 13, Alliance));
    CHECK(ZoneFits(4922, 85, Horde));     // Twilight Highlands
    CHECK_FALSE(ZoneFits(4922, 81, Horde));
    CHECK_FALSE(ZoneFits(4714, 5, Alliance)); // Gilneas is not a levelling zone here
}

TEST_CASE("Random levels: target level is stable and spread over the range", "[playerbots][random]")
{
    std::vector<uint32_t> counts(MaxLevel + 1, 0);
    for (uint32_t guid = 1; guid <= 8500; ++guid)
    {
        uint32_t level = TargetLevel(guid, 1, 85);
        REQUIRE(level >= 1);
        REQUIRE(level <= 85);
        CHECK(TargetLevel(guid, 1, 85) == level);
        ++counts[level];
    }
    for (uint32_t level = 1; level <= MaxLevel; ++level)
        CHECK(counts[level] > 40); // about 100 each
    CHECK(TargetLevel(42, 30, 30) == 30);
    CHECK(TargetLevel(42, 90, 120) == 85);
    for (uint32_t guid = 1; guid < 200; ++guid)
    {
        uint32_t level = TargetLevel(guid, 20, 40);
        CHECK(level >= 20);
        CHECK(level <= 40);
    }
}

TEST_CASE("Random levels: only fresh bots are initialized", "[playerbots][random]")
{
    CHECK(NeedsInitialization(1, 40));
    CHECK(NeedsInitialization(5, 40));
    CHECK_FALSE(NeedsInitialization(6, 40));
    CHECK_FALSE(NeedsInitialization(1, 1));
    CHECK(NeedsInitialization(2, 3));
    CHECK(MobLevelLow(1) == 1);
    CHECK(MobLevelLow(40) == 38);
    CHECK(MobLevelHigh(85) == 85);
    CHECK(GearLevelLow(3) == 1);
    CHECK(GearLevelLow(40) == 34);
}
