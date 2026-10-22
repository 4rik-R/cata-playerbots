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

TEST_CASE("The tank leads to the next pack and waits for the human", "[playerbots][random][dungeon]")
{
    // Pack 50 yd ahead along a clear path, human right behind: walk on.
    LeadView view { 8.0f, 50.0f, 55.0f, 1.0f, false, false, false };
    CHECK(Lead(view) == LeadStep::Walk);
    // Close and in sight with the human near: pull.
    view.TankToPack = 20.0f;
    view.PathToPack = 21.0f;
    view.PackInSight = true;
    CHECK(Lead(view) == LeadStep::Pull);
    // The human fell behind: wait, and keep waiting until they are close again.
    view = { 28.0f, 50.0f, 55.0f, 1.0f, false, false, false };
    CHECK(Lead(view) == LeadStep::Wait);
    view.HumanToTank = 20.0f;
    view.Waiting = true;
    CHECK(Lead(view) == LeadStep::Wait);
    view.HumanToTank = 12.0f;
    CHECK(Lead(view) == LeadStep::Walk);
    // A boss waits for the human right next to the tank.
    view = { 20.0f, 20.0f, 21.0f, 1.0f, true, true, false };
    CHECK(Lead(view) == LeadStep::Walk);
    view.HumanToTank = 10.0f;
    CHECK(Lead(view) == LeadStep::Pull);
    // No path, another floor, a long detour or a human gone elsewhere: follow.
    CHECK(Lead({ 8.0f, 50.0f, 0.0f, 1.0f, false, false, false }) == LeadStep::Follow);
    CHECK(Lead({ 8.0f, 50.0f, 55.0f, 9.0f, false, false, false }) == LeadStep::Follow);
    CHECK(Lead({ 8.0f, 30.0f, 90.0f, 1.0f, false, false, false }) == LeadStep::Follow);
    CHECK(Lead({ 50.0f, 50.0f, 55.0f, 1.0f, false, false, false }) == LeadStep::Follow);
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

TEST_CASE("A dead bot waits for a healer, then releases its spirit", "[playerbots][random][dungeon]")
{
    uint32_t delay = ReleaseDelayMs(7, 1);
    CHECK(delay >= ReleaseMinMs);
    CHECK(delay <= ReleaseMaxMs);
    // A healer is alive: wait through the fight and 30 s after it.
    DeathView view { 20000, 0, false, false, false, true };
    CHECK_FALSE(Release(view, delay));
    view.DeadMs = WaitWhileFightingMs;
    CHECK(Release(view, delay));
    view = { 60000, 10000, false, false, false, true };
    CHECK_FALSE(Release(view, delay));
    view.QuietMs = WaitForResurrectionMs;
    CHECK(Release(view, delay));
    // An offered resurrection is never run away from.
    view.ResurrectionOffered = true;
    CHECK_FALSE(Release(view, delay));
    // A wipe, a human already running back, or nobody to raise the bot: release after its own moment.
    CHECK_FALSE(Release({ delay - 1, 0, false, true, false, true }, delay));
    CHECK(Release({ delay, 0, false, true, false, true }, delay));
    CHECK(Release({ delay, 5000, false, false, true, true }, delay));
    CHECK(Release({ delay, 0, false, false, false, false }, delay));
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
