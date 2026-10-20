/*
 * This file is part of the Cataclysm Playerbots port. See AUTHORS.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "../src/Bot/Random/RandomPlayerbotPolicy.h"
#include <catch2/catch.hpp>
#include <random>
#include <algorithm>
#include <set>

namespace
{
    struct SeededRoll
    {
        std::mt19937* Engine;
        uint32_t operator()(uint32_t bound) const { return std::uniform_int_distribution<uint32_t>(0, bound - 1)(*Engine); }
    };
}

TEST_CASE("Random population prefix is normalized and bounded", "[playerbots][random]")
{
    REQUIRE(NormalizeRandomPlayerbotPrefix("rndBot") == std::string("RNDBOT"));
    REQUIRE(NormalizeRandomPlayerbotPrefix("BOT7") == std::string("BOT7"));
    REQUIRE_FALSE(NormalizeRandomPlayerbotPrefix(""));
    REQUIRE_FALSE(NormalizeRandomPlayerbotPrefix("RND_BOT"));
    REQUIRE_FALSE(NormalizeRandomPlayerbotPrefix("ABCDEFGHIJK"));
}

TEST_CASE("Random population account names fit native limits", "[playerbots][random]")
{
    REQUIRE(RandomPlayerbotAccountName("RNDBOT", 1) == "RNDBOT001");
    REQUIRE(RandomPlayerbotAccountName("RNDBOT", 300) == "RNDBOT300");
    REQUIRE(RandomPlayerbotAccountName("RNDBOT", 1234) == "RNDBOT1234");
    REQUIRE(RandomPlayerbotAccountName("ABCDEFGHIJ", MaxRandomPlayerbots).size() <= 16);
}

TEST_CASE("Random population settings are clamped", "[playerbots][random]")
{
    RandomPlayerbotSettings settings;
    settings.Count = 999999;
    settings.LoginPerInterval = 0;
    settings.IntervalMs = 5;
    settings.AllianceRatio = 250;
    auto clamped = ClampRandomPlayerbotSettings(settings);
    REQUIRE(clamped.Count == MaxRandomPlayerbots);
    REQUIRE(clamped.LoginPerInterval == 1);
    REQUIRE(clamped.IntervalMs == 1000);
    REQUIRE(clamped.AllianceRatio == 100);
}

TEST_CASE("Random population admits in bounded batches", "[playerbots][random]")
{
    REQUIRE(RandomPlayerbotLoginBudget(0, 0, 50, 50, 10) == 10);
    REQUIRE(RandomPlayerbotLoginBudget(45, 0, 5, 50, 10) == 5);
    REQUIRE(RandomPlayerbotLoginBudget(40, 0, 3, 50, 10) == 3);
    REQUIRE(RandomPlayerbotLoginBudget(50, 0, 10, 50, 10) == 0);
    REQUIRE(RandomPlayerbotLoginBudget(60, 0, 10, 50, 10) == 0);
}

TEST_CASE("Random population profiles use supported classes and every race", "[playerbots][random]")
{
    std::mt19937 engine(7);
    std::set<uint8_t> alliance(RandomPlayerbotAllianceRaces.begin(), RandomPlayerbotAllianceRaces.end());
    std::set<uint8_t> horde(RandomPlayerbotHordeRaces.begin(), RandomPlayerbotHordeRaces.end());
    uint32_t allianceCount = 0;
    std::set<uint8_t> classes;
    for (int i = 0; i < 2000; ++i)
    {
        auto profile = RollRandomPlayerbotProfile(50, SeededRoll{&engine});
        REQUIRE(std::find(RandomPlayerbotClasses.begin(), RandomPlayerbotClasses.end(), profile.Class) !=
            RandomPlayerbotClasses.end());
        classes.insert(profile.Class);
        REQUIRE(profile.Gender <= 1);
        REQUIRE((alliance.count(profile.Race) || horde.count(profile.Race)));
        allianceCount += alliance.count(profile.Race) ? 1 : 0;
    }
    REQUIRE(classes.size() == RandomPlayerbotClasses.size());
    REQUIRE(alliance.count(22));
    REQUIRE(horde.count(9));
    REQUIRE(allianceCount > 800);
    REQUIRE(allianceCount < 1200);
    REQUIRE(horde.count(RollRandomPlayerbotProfile(0, SeededRoll{&engine}).Race));
    REQUIRE(alliance.count(RollRandomPlayerbotProfile(100, SeededRoll{&engine}).Race));
}

TEST_CASE("Random population names are short capitalized letters", "[playerbots][random]")
{
    std::mt19937 engine(11);
    std::set<std::string> distinct;
    for (int i = 0; i < 500; ++i)
    {
        std::string name = RollRandomPlayerbotName(SeededRoll{&engine});
        REQUIRE(name.size() >= 4);
        REQUIRE(name.size() <= 12);
        REQUIRE((name[0] >= 'A' && name[0] <= 'Z'));
        for (std::size_t c = 1; c < name.size(); ++c)
            REQUIRE((name[c] >= 'a' && name[c] <= 'z'));
        distinct.insert(name);
    }
    REQUIRE(distinct.size() > 300);
}

TEST_CASE("Random population class list parses ids and limits the roll", "[playerbots][random]")
{
    REQUIRE(ParseRandomPlayerbotClasses("") == 0);
    REQUIRE(ParseRandomPlayerbotClasses("4, 3,9") == ((1u << 3) | (1u << 2) | (1u << 8)));
    // 99 and 10 are no class.
    REQUIRE(ParseRandomPlayerbotClasses("6,99,10") == (1u << 5));
    REQUIRE(ParseRandomPlayerbotClasses("2,7,11") == ((1u << 1) | (1u << 6) | (1u << 10)));
    REQUIRE(RandomPlayerbotClassEnabled(0, 4));
    std::mt19937 engine(3);
    uint32_t mask = ParseRandomPlayerbotClasses("4,9");
    std::set<uint8_t> seen;
    for (int i = 0; i < 500; ++i)
        seen.insert(RollRandomPlayerbotProfile(50, SeededRoll{&engine}, mask).Class);
    REQUIRE(seen == std::set<uint8_t>{4, 9});
}

TEST_CASE("Random population can leave out death knights", "[playerbots][random]")
{
    uint32_t withoutAll = RandomPlayerbotClassesWithout(0, RandomPlayerbotDeathKnight);
    REQUIRE_FALSE(RandomPlayerbotClassEnabled(withoutAll, 6));
    for (uint8_t classId : RandomPlayerbotClasses)
        if (classId != 6)
            REQUIRE(RandomPlayerbotClassEnabled(withoutAll, classId));
    REQUIRE(RandomPlayerbotClassesWithout(ParseRandomPlayerbotClasses("4,6"), 6) == (1u << 3));
    std::mt19937 engine(5);
    for (int i = 0; i < 500; ++i)
        REQUIRE(RollRandomPlayerbotProfile(50, SeededRoll{&engine}, withoutAll).Class != 6);
}
