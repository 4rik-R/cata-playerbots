/*
 * This file is part of the Cataclysm Playerbots port. See AUTHORS.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "../src/Bot/Random/RandomPlayerbotGrind.h"
#include <catch2/catch.hpp>

using namespace RandomPlayerbotGrind;

TEST_CASE("Random grind level band skips grey and far-higher targets", "[playerbots][random]")
{
    CHECK(LevelAllows(1, 1));
    CHECK(LevelAllows(1, 2));
    CHECK_FALSE(LevelAllows(1, 3));
    CHECK(LevelAllows(3, 1));
    CHECK(LevelAllows(10, 7));
    CHECK_FALSE(LevelAllows(10, 6));
    CHECK(LevelAllows(10, 11));
    CHECK_FALSE(LevelAllows(10, 12));
    CHECK(LevelAllows(85, 86));
}

TEST_CASE("Random grind recovery uses hysteresis", "[playerbots][random]")
{
    CHECK(ReadyToPull(false, 60.0f, 40.0f, true));
    CHECK_FALSE(ReadyToPull(false, 59.0f, 100.0f, true));
    CHECK_FALSE(ReadyToPull(false, 100.0f, 39.0f, true));
    CHECK(ReadyToPull(false, 60.0f, 0.0f, false));
    CHECK_FALSE(ReadyToPull(true, 70.0f, 70.0f, true));
    CHECK(ReadyToPull(true, 90.0f, 80.0f, true));
    CHECK(ReadyToPull(true, 95.0f, 0.0f, false));
}

TEST_CASE("Random grind wander delay stays in its window", "[playerbots][random]")
{
    CHECK(WanderDelayMs(0.0f) == 8000);
    CHECK(WanderDelayMs(-1.0f) == 8000);
    CHECK(WanderDelayMs(0.5f) == 14000);
    CHECK(WanderDelayMs(1.0f) < 20000);
    CHECK(WanderDelayMs(2.0f) >= 19980);
}
