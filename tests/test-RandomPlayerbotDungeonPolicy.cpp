/*
 * This file is part of the Cataclysm Playerbots port. See AUTHORS.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "../src/Bot/Random/RandomPlayerbotDungeonPolicy.h"
#include <catch2/catch.hpp>

using namespace RandomPlayerbotDungeonPolicy;

TEST_CASE("The group is ready with health and mana up", "[playerbots][random][dungeon]")
{
    CHECK(Ready(100.0f, 100.0f, true));
    CHECK(Ready(80.0f, 0.0f, false));
    CHECK_FALSE(Ready(79.0f, 100.0f, true));
    CHECK_FALSE(Ready(100.0f, 70.0f, true));
    CHECK(BreakNeeded(100.0f, 59.0f, true) == Break::Drink);
    CHECK(BreakNeeded(40.0f, 59.0f, true) == Break::Drink);
    CHECK(BreakNeeded(40.0f, 10.0f, false) == Break::Eat);
    CHECK(BreakNeeded(90.0f, 61.0f, true) == Break::None);
}

TEST_CASE("Reaction delays differ between bots and stay in range", "[playerbots][random][dungeon]")
{
    bool differs = false;
    for (uint32_t seed = 1; seed < 50; ++seed)
    {
        uint32_t ms = ReactMs(seed, 3);
        CHECK(ms >= ReactMinMs);
        CHECK(ms <= ReactMaxMs);
        CHECK(ReactMs(seed, 3) == ms);
        differs = differs || ms != ReactMs(seed + 1, 3);
        float offset = EscapeOffset(seed);
        CHECK(offset >= -0.35f);
        CHECK(offset <= 0.35f);
    }
    CHECK(differs);
}

TEST_CASE("The tank pulls near the human and bosses only with the human close", "[playerbots][random][dungeon]")
{
    CHECK(PullAllowed({ 20.0f, 1.0f, false }, 25.0f));
    CHECK_FALSE(PullAllowed({ 26.0f, 1.0f, false }, 5.0f));
    CHECK_FALSE(PullAllowed({ 10.0f, 8.0f, false }, 5.0f));
    CHECK_FALSE(PullAllowed({ 10.0f, 1.0f, false }, 35.0f));
    CHECK_FALSE(PullAllowed({ 10.0f, 1.0f, true }, 20.0f));
    CHECK(PullAllowed({ 10.0f, 1.0f, true }, 10.0f));
    CHECK(BetterPull({ 5.0f, 0.0f, false }, { 9.0f, 0.0f, false }));
}

TEST_CASE("Marks follow the tank and keep the human's", "[playerbots][random][dungeon]")
{
    std::vector<Enemy> pack = { { 1, 100, true, false }, { 2, 300, true, false }, { 3, 200, false, false } };
    Marks marks = PlanMarks(1, pack, {});
    CHECK(marks.Skull == 1);
    CHECK(marks.Cross == 2);
    CHECK(marks.Moon == 0); // three enemies: no crowd control

    pack.push_back({ 4, 250, true, false });
    marks = PlanMarks(1, pack, {});
    CHECK(marks.Skull == 1);
    CHECK(marks.Moon == 2);  // healthiest controllable one
    CHECK(marks.Cross == 4); // healthiest of the rest

    // The human marked enemy 3 with a skull: the tank target does not take it over.
    marks = PlanMarks(1, pack, { 3, 0, 0 });
    CHECK(marks.Skull == 3);
    CHECK(marks.Moon == 2);
    CHECK(marks.Cross == 4);

    // A boss is never put to sleep.
    std::vector<Enemy> bosses = { { 1, 900, true, true }, { 2, 10, false, false }, { 3, 10, false, false },
        { 4, 10, false, false } };
    CHECK(PlanMarks(2, bosses, {}).Moon == 0);
}

TEST_CASE("Crowd control matches class and creature type", "[playerbots][random][dungeon]")
{
    CHECK(CrowdControlFor(ClassMage, TypeHumanoid)->SpellId == 118);
    CHECK(CrowdControlFor(ClassMage, TypeUndead) == nullptr);
    CHECK(CrowdControlFor(ClassPriest, TypeUndead)->SpellId == 9484);
    CHECK(CrowdControlFor(ClassDruid, TypeDragonkin)->SpellId == 2637);
    CHECK(CrowdControlFor(ClassWarlock, TypeElemental)->SpellId == 710);
    CHECK(CrowdControlFor(1, TypeHumanoid) == nullptr);
}

TEST_CASE("A bot steps just outside a harmful area towards its group", "[playerbots][random][dungeon]")
{
    Point center { 0.0f, 0.0f };
    Point group { 20.0f, 0.0f };
    Point out = EscapePoint({ 1.0f, 1.0f }, center, 5.0f, group, 0.0f);
    CHECK(Distance(out, center) == Approx(5.0f + HazardMargin).margin(0.01f));
    CHECK(out.X > 0.0f);
    CHECK_FALSE(Inside(out, center, 5.0f));
    CHECK(Inside({ 1.0f, 1.0f }, center, 5.0f));
    // Standing right in the middle, the bot still leaves towards the group.
    Point fromMiddle = EscapePoint(center, center, 5.0f, group, 0.2f);
    CHECK(fromMiddle.X > 0.0f);
}

TEST_CASE("Wait and go commands in German", "[playerbots][random][dungeon]")
{
    CHECK(ParseCommand("bots warten") == Command::Wait);
    CHECK(ParseCommand("Bots Warten!") == Command::Wait);
    CHECK(ParseCommand("  bots weiter ") == Command::Go);
    CHECK(ParseCommand("bots los") == Command::Go);
    CHECK(ParseCommand("warten") == Command::None);
    CHECK(ParseCommand("bots heiler") == Command::None);
}
