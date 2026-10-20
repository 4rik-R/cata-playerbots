/*
 * This file is part of the Cataclysm Playerbots port. See AUTHORS.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "../src/Bot/Random/RandomPlayerbotQuestPolicy.h"
#include <catch2/catch.hpp>

using namespace RandomPlayerbotQuestPolicy;

TEST_CASE("Random quests: only plain kill, collect and object quests are taken", "[playerbots][random][quest]")
{
    Shape kill;
    kill.KillObjectives = 1;
    REQUIRE(Classify(kill) == Verdict::Supported);
    Shape collect;
    collect.ItemObjectives = 2;
    REQUIRE(Classify(collect) == Verdict::Supported);

    REQUIRE(Classify(Shape{}) == Verdict::NoObjective);
    Shape object;
    object.GameObjectObjectives = 1;
    REQUIRE(Classify(object) == Verdict::Supported);

    auto with = [&](auto field)
    {
        Shape shape = kill;
        shape.*field = true;
        return Classify(shape);
    };
    REQUIRE(with(&Shape::Deprecated) == Verdict::Unavailable);
    REQUIRE(with(&Shape::Repeating) == Verdict::Unavailable);
    REQUIRE(with(&Shape::Timed) == Verdict::Unavailable);
    REQUIRE(with(&Shape::Escort) == Verdict::Unavailable);
    REQUIRE(with(&Shape::Group) == Verdict::Unavailable);
    REQUIRE(with(&Shape::Scripted) == Verdict::Scripted);
    REQUIRE(with(&Shape::NeedsSpell) == Verdict::Scripted);
    REQUIRE(with(&Shape::Currency) == Verdict::Scripted);
    REQUIRE(with(&Shape::PlayerKills) == Verdict::Scripted);
}

TEST_CASE("Random quests: level window matches native grey and three levels up", "[playerbots][random][quest]")
{
    REQUIRE(GrayLevel(5) == 0);
    REQUIRE(GrayLevel(20) == 13);
    REQUIRE(GrayLevel(50) == 39);
    REQUIRE(GrayLevel(85) == 76);
    REQUIRE(LevelFits(10, 10, 8));
    REQUIRE(LevelFits(10, 13, 8));
    REQUIRE_FALSE(LevelFits(10, 14, 8));  // too high
    REQUIRE_FALSE(LevelFits(10, 10, 11)); // below its minimum level
    REQUIRE_FALSE(LevelFits(20, 13, 10)); // grey
    REQUIRE(LevelFits(20, 14, 10));
    REQUIRE(LevelFits(1, 1, 1));
}

TEST_CASE("Random quests: progress resets the stall timer", "[playerbots][random][quest]")
{
    REQUIRE(ProgressSignature({ 0, 0 }) != ProgressSignature({ 1, 0 }));
    REQUIRE(ProgressSignature({ 1, 0 }) != ProgressSignature({ 0, 1 }));
    REQUIRE(ProgressSignature({ 2, 3 }) == ProgressSignature({ 2, 3 }));
    REQUIRE_FALSE(Stalled(1000, 1000));
    REQUIRE_FALSE(Stalled(QuestStallMs - 1, 0));
    REQUIRE(Stalled(QuestStallMs, 0));
    // The millisecond clock wraps around.
    REQUIRE(Stalled(QuestStallMs - 100, 0xFFFFFF00u));
}

TEST_CASE("Random quests: reward is the best upgrade, else the most valuable item", "[playerbots][random][quest]")
{
    REQUIRE(ChooseReward({}) == 0);
    std::vector<RewardOption> upgrade = {
        { 0, true, 20, 18, 50 },  // +2
        { 1, true, 22, 15, 10 },  // +7
        { 2, false, 30, 0, 900 }, // not usable
    };
    REQUIRE(ChooseReward(upgrade) == 1);
    std::vector<RewardOption> noUpgrade = {
        { 0, true, 20, 25, 50 },  // worse than worn
        { 1, false, 22, 0, 300 },
        { 2, false, 20, 0, 120 },
    };
    REQUIRE(ChooseReward(noUpgrade) == 1);
    // An empty slot is always an upgrade.
    REQUIRE(ChooseReward({ { 0, false, 30, 0, 999 }, { 3, true, 5, 0, 1 } }) == 3);
}

TEST_CASE("Random quests: points inside quest-map markers", "[playerbots][random][quest]")
{
    std::vector<MapPoint> square = { { 0, 0 }, { 10, 0 }, { 10, 10 }, { 0, 10 } };
    REQUIRE(InPolygon(square, 5, 5));
    REQUIRE_FALSE(InPolygon(square, 15, 5));
    REQUIRE_FALSE(InPolygon(square, 5, -1));
    // An L shape: the inner corner is outside.
    std::vector<MapPoint> ell = { { 0, 0 }, { 10, 0 }, { 10, 4 }, { 4, 4 }, { 4, 10 }, { 0, 10 } };
    REQUIRE(InPolygon(ell, 2, 8));
    REQUIRE_FALSE(InPolygon(ell, 8, 8));

    uint32_t state = 12345;
    auto roll = [&] { state = state * 1103515245u + 12345u; return double((state >> 8) & 0xFFFF) / 65536.0; };
    for (int i = 0; i < 500; ++i)
    {
        MapPoint point = RandomPointIn(ell, roll);
        REQUIRE(InPolygon(ell, point.X, point.Y));
    }
    // Single points and lines fall back to their middle.
    MapPoint single = RandomPointIn({ { 3, 4 } }, roll);
    REQUIRE(single.X == 3);
    REQUIRE(single.Y == 4);
    MapPoint line = RandomPointIn({ { 0, 0 }, { 10, 20 } }, roll);
    REQUIRE(line.X == 5);
    REQUIRE(line.Y == 10);
    REQUIRE(RandomPointIn({}, roll).X == 0);
}

TEST_CASE("Random quests: a spot without progress is given up after five minutes", "[playerbots][random][quest]")
{
    REQUIRE_FALSE(SpotStalled(SpotStallMs - 1, 0, 0));
    REQUIRE(SpotStalled(SpotStallMs, 0, 0));
    // Progress after arriving restarts the wait.
    REQUIRE_FALSE(SpotStalled(SpotStallMs + 1000, 0, 2000));
    REQUIRE(SpotStalled(SpotStallMs + 2000, 0, 2000));
    // Arriving late restarts it as well.
    REQUIRE_FALSE(SpotStalled(SpotStallMs + 1000, 2000, 0));
    // The millisecond clock wraps around.
    REQUIRE(SpotStalled(SpotStallMs - 100, 0xFFFFFF00u, 0xFFFFFF00u));
}

TEST_CASE("Random quests: a full log drops grey quests, then ones bots cannot do", "[playerbots][random][quest]")
{
    std::vector<LogEntry> log = {
        { 1, false, true },
        { 2, false, false }, // not doable
        { 3, true, true },   // grey
        { 4, true, false },  // grey
        { 5, false, true },
    };
    REQUIRE(LogCleanup(log, 2).empty());
    REQUIRE(LogCleanup(log, 1) == std::vector<uint32_t>{ 3 });
    REQUIRE(LogCleanup(log, 0) == std::vector<uint32_t>{ 3, 4 });
    std::vector<LogEntry> doable = { { 1, false, true }, { 2, false, false } };
    REQUIRE(LogCleanup(doable, 0) == std::vector<uint32_t>{ 2 });
    // Quests bots can do stay, even when the log stays full.
    REQUIRE(LogCleanup({ { 1, false, true } }, 0).empty());
}
