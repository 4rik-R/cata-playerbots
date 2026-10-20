/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "catch2/catch.hpp"
#include "../src/Ai/Class/Priest/PlayerbotPriestStrategy.h"
#include "../src/Ai/Base/PlayerbotHealerKit.h"
#include "../src/Ai/Base/PlayerbotHealingMana.h"
#include <limits>
#include <memory>

TEST_CASE("Playerbot low mana conserves routine heals without suppressing the donor tank emergency", "[PlayerbotPriest][Mana]")
{
    using namespace PlayerbotHealingMana;
    auto flash = ForPriestSpell(2061);
    auto heal = ForPriestSpell(2050);
    REQUIRE(Allows(true, 61, 50, false, flash));
    REQUIRE_FALSE(Allows(true, 60, 50, false, flash));
    REQUIRE(Allows(true, 60, 44, true, flash));
    REQUIRE_FALSE(Allows(true, 60, 45, true, flash));
    REQUIRE(Allows(true, 60, 50, false, heal));
    REQUIRE_FALSE(Allows(true, 60, 51, false, heal));
    REQUIRE(Allows(true, 60, 60, true, heal));
    REQUIRE_FALSE(Allows(true, 60, 65, true, heal));
}

TEST_CASE("Playerbot donor heal estimates keep tank scaling and efficient aura boundaries", "[PlayerbotPriest][Mana]")
{
    using namespace PlayerbotHealingMana;
    auto renew = ForPriestSpell(139);
    auto shield = ForPriestSpell(17);
    REQUIRE(Allows(true, 60, 85, false, renew));
    REQUIRE_FALSE(Allows(true, 60, 86, false, renew));
    REQUIRE(Allows(true, 60, 90, true, shield));
    REQUIRE_FALSE(Allows(true, 60, 91, true, shield));
    // Donor native percentages truncate before threshold comparison.
    REQUIRE_FALSE(Allows(true, 60.9f, 50, false, ForPriestSpell(2061)));
    REQUIRE(Allows(true, 60, 85.9f, false, renew));
}

TEST_CASE("Playerbot healer conservation preserves disabled and unported spell paths", "[PlayerbotPriest][Mana]")
{
    using namespace PlayerbotHealingMana;
    REQUIRE(Allows(false, 1, 95, false, ForPriestSpell(2061)));
    REQUIRE(Allows(true, 1, 95, false, ForPriestSpell(2006)));
    REQUIRE_FALSE(Allows(true, std::numeric_limits<float>::quiet_NaN(), 50, true, ForPriestSpell(2050)));
    REQUIRE_FALSE(Allows(true, 50, 0, true, ForPriestSpell(2050)));
    REQUIRE_FALSE(Allows(true, 101, 50, true, ForPriestSpell(2050)));
}

TEST_CASE("Playerbot named healing selection keeps distance probe before health thresholds", "[PlayerbotPriest]")
{
    struct Member { int Id; float Health; float Distance; };
    std::vector<Member> members = {{1, 54, 30}, {2, 55, 0}};
    int selected = 0;
    REQUIRE(PlayerbotPriest::TryInHealthOrder(members, [](Member const& member) { return member.Health; },
        [](Member const& member) { return member.Distance; },
        [&](Member const& member) { selected = member.Id; return true; }));
    REQUIRE(selected == 2);
}

TEST_CASE("Playerbot support selection retains donor controller healer tank other ordering", "[PlayerbotPriest]")
{
    using PlayerbotPartySupport::Role;
    struct Candidate { int Id; Role Type; bool Local; };
    std::vector<Candidate> candidates = {{1, Role::Other, true}, {2, Role::Tank, true},
        {3, Role::Healer, false}, {4, Role::Controller, false}, {5, Role::Healer, true},
        {6, Role::Tank, false}, {7, Role::Healer, true}};
    PlayerbotPartySupport::OrderCandidates(candidates, [](Candidate const& c) { return c.Type; },
        [](Candidate const& c) { return c.Local; });
    std::vector<int> ids;
    for (auto const& candidate : candidates) ids.push_back(candidate.Id);
    REQUIRE(ids == std::vector<int>{4, 5, 7, 3, 2, 6, 1});
}

TEST_CASE("Playerbot support ordering preserves ties and handles an empty party", "[PlayerbotPriest]")
{
    using PlayerbotPartySupport::Role;
    std::vector<int> candidates = {1, 2, 3};
    PlayerbotPartySupport::OrderCandidates(candidates, [](int) { return Role::Other; }, [](int) { return true; });
    REQUIRE(candidates == std::vector<int>{1, 2, 3});
    candidates.clear();
    PlayerbotPartySupport::OrderCandidates(candidates, [](int) { return Role::Other; }, [](int) { return true; });
    REQUIRE(candidates.empty());
}

TEST_CASE("Playerbot healing reach retains donor trigger action and priority", "[PlayerbotPriest]")
{
    std::vector<TriggerNode*> triggers;
    PlayerbotPriest::AddHealingReachTrigger(triggers);
    REQUIRE(triggers.size() == 1);
    std::unique_ptr<TriggerNode> trigger(triggers.front());
    REQUIRE(trigger->getName() == "party member to heal out of spell range");
    REQUIRE(trigger->getHandlers()[0].getName() == "reach party member to heal");
    REQUIRE(trigger->getHandlers()[0].getRelevance() == ACTION_CRITICAL_HEAL + 10);
}

TEST_CASE("Playerbot support reach intents cannot survive disabled movement", "[PlayerbotPriest]")
{
    PlayerbotPartySupport::ReachRequest request;
    using PlayerbotPartySupport::ReachKind;
    REQUIRE_FALSE(request.Submit());
    REQUIRE(request.Take() == ReachKind::None);
    request.Enable(true);
    REQUIRE(request.Submit());
    REQUIRE(request.Take() == ReachKind::Heal);
    REQUIRE(request.Take() == ReachKind::None);
    REQUIRE(request.Submit());
    request.Enable(false);
    request.Enable(true);
    REQUIRE(request.Take() == ReachKind::None);
}

TEST_CASE("Playerbot resurrection preserves its specialized donor reach prerequisite", "[PlayerbotPriest]")
{
    auto prerequisites = PlayerbotPriest::ResurrectionPrerequisites();
    REQUIRE(prerequisites.size() == 1);
    REQUIRE(prerequisites.front().getName() == "reach party member to resurrect");
}

TEST_CASE("Playerbot support reach requests retain their purpose without leaking intent", "[PlayerbotPriest]")
{
    using PlayerbotPartySupport::ReachKind;
    PlayerbotPartySupport::ReachRequest request;
    request.Enable(true);
    REQUIRE_FALSE(request.Submit(ReachKind::None));
    REQUIRE(request.Submit(ReachKind::Resurrect));
    REQUIRE(request.Take() == ReachKind::Resurrect);
    REQUIRE(request.Take() == ReachKind::None);
    REQUIRE(request.Submit(ReachKind::Resurrect));
    request.Enable(false);
    request.Enable(true);
    REQUIRE(request.Take() == ReachKind::None);
}

TEST_CASE("Playerbot resurrection approaches stay inside the companion envelope", "[PlayerbotPriest]")
{
    using PlayerbotPriest::NeedsResurrectionReach;
    REQUIRE(NeedsResurrectionReach(31, 20, 20));
    REQUIRE(NeedsResurrectionReach(40, 0, 25));
    REQUIRE_FALSE(NeedsResurrectionReach(20, 20, 20));
    REQUIRE_FALSE(NeedsResurrectionReach(40.1f, 0, 20));
    REQUIRE_FALSE(NeedsResurrectionReach(31, 20.1f, 20));
    REQUIRE_FALSE(NeedsResurrectionReach(31, -1, 20));
    REQUIRE_FALSE(NeedsResurrectionReach(31, 0, 30));
    REQUIRE_FALSE(NeedsResurrectionReach(std::numeric_limits<float>::quiet_NaN(), 0, 20));
}

TEST_CASE("Playerbot healing reach accepts only bounded qualified ranges", "[PlayerbotPriest]")
{
    using PlayerbotPriest::NeedsHealingReach;
    REQUIRE(NeedsHealingReach(50, 25, 20, 24));
    REQUIRE_FALSE(NeedsHealingReach(50, 24, 20, 24));
    REQUIRE_FALSE(NeedsHealingReach(50, 25, 20, 31));
    REQUIRE_FALSE(NeedsHealingReach(50, 25, 20, 1));
    REQUIRE_FALSE(NeedsHealingReach(50, 25, 20, std::numeric_limits<float>::quiet_NaN()));
}

TEST_CASE("Playerbot resurrection skips live targets pending requests and incoming casts", "[PlayerbotPriest]")
{
    using PlayerbotPriest::CanResurrect;
    REQUIRE(CanResurrect(true, false, false));
    REQUIRE_FALSE(CanResurrect(false, false, false));
    REQUIRE_FALSE(CanResurrect(true, true, false));
    REQUIRE_FALSE(CanResurrect(true, false, true));
    REQUIRE_FALSE(CanResurrect(true, true, true));
}

TEST_CASE("Playerbot healing reach yields after native completion or a bounded path interval", "[PlayerbotPriest]")
{
    using PlayerbotPriest::HealingReachMustYield;
    REQUIRE_FALSE(HealingReachMustYield(0, true));
    REQUIRE_FALSE(HealingReachMustYield(2999, true));
    REQUIRE(HealingReachMustYield(3000, true));
    REQUIRE(HealingReachMustYield(0, false));
    REQUIRE(HealingReachMustYield(5000, false));
}

TEST_CASE("Playerbot healing reach stays inside the bounded party gap", "[PlayerbotPriest]")
{
    using PlayerbotPriest::NeedsHealingReach;
    REQUIRE(NeedsHealingReach(50, 31, 20));
    REQUIRE(NeedsHealingReach(50, 40, 0));
    REQUIRE_FALSE(NeedsHealingReach(80, 31, 20));
    REQUIRE_FALSE(NeedsHealingReach(0, 31, 20));
    REQUIRE_FALSE(NeedsHealingReach(50, 30, 20));
    REQUIRE_FALSE(NeedsHealingReach(50, 40.1f, 20));
    REQUIRE_FALSE(NeedsHealingReach(50, 31, 20.1f));
    REQUIRE_FALSE(NeedsHealingReach(50, 31, -1));
    REQUIRE_FALSE(NeedsHealingReach(50, std::numeric_limits<float>::quiet_NaN(), 0));
}

TEST_CASE("Playerbot healing defers redundant routine casts but preserves emergencies and raids", "[PlayerbotPriest]")
{
    using PlayerbotPriest::DeferToIncomingHeal;
    REQUIRE(DeferToIncomingHeal(55.0f, false, true));
    REQUIRE(DeferToIncomingHeal(80.0f, false, true));
    REQUIRE_FALSE(DeferToIncomingHeal(54.9f, false, true));
    REQUIRE_FALSE(DeferToIncomingHeal(80.0f, true, true));
    REQUIRE_FALSE(DeferToIncomingHeal(80.0f, false, false));
}

TEST_CASE("Playerbot healing balances native distance with injury and falls back", "[PlayerbotPriest]")
{
    struct Candidate { int Id; float Health; float Distance; };
    std::vector<Candidate> candidates = {{1, 50, 30}, {2, 51, 0}, {3, 20, 30}, {4, 51, 0}};
    std::vector<int> attempted;
    REQUIRE(PlayerbotPriest::TryInHealthOrder(candidates,
        [](Candidate const& c) { return c.Health; }, [](Candidate const& c) { return c.Distance; },
        [&](Candidate const& c) { attempted.push_back(c.Id); return c.Id == 1; }));
    REQUIRE(attempted == std::vector<int>{3, 2, 4, 1});
}

TEST_CASE("Playerbot healing rejects invalid distance without losing eligible members", "[PlayerbotPriest]")
{
    std::vector<float> candidates = {-1, std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), 0, 30};
    std::vector<float> attempted;
    REQUIRE_FALSE(PlayerbotPriest::TryInHealthOrder(candidates,
        [](float) { return 50.0f; }, [](float range) { return range; },
        [&](float range) { attempted.push_back(range); return false; }));
    REQUIRE(attempted == std::vector<float>{0, 30});
}

TEST_CASE("Playerbot Priest healing tiers preserve emergency priority", "[PlayerbotPriest]")
{
    using PlayerbotPriest::HealTier;
    using PlayerbotPriest::TierForHealth;
    REQUIRE(TierForHealth(20.0f) == HealTier::Emergency);
    REQUIRE(TierForHealth(54.9f) == HealTier::Emergency);
    REQUIRE(TierForHealth(55.0f) == HealTier::Heal);
    REQUIRE(TierForHealth(79.9f) == HealTier::Heal);
    REQUIRE(TierForHealth(80.0f) == HealTier::Renew);
    REQUIRE(TierForHealth(89.9f) == HealTier::Renew);
    REQUIRE(TierForHealth(90.0f) == HealTier::None);
}

TEST_CASE("Playerbot healing tries the most injured first and falls back", "[PlayerbotPriest]")
{
    std::vector<float> candidates = { 85.0f, 20.0f, 60.0f };
    std::vector<float> attempted;
    bool healed = PlayerbotPriest::TryInHealthOrder(candidates,
        [](float pct) { return pct; },
        [&](float pct) { attempted.push_back(pct); return pct == 60.0f; });
    REQUIRE(healed);
    REQUIRE(attempted == std::vector<float>{ 20.0f, 60.0f });
}

TEST_CASE("Playerbot healing ignores healthy and invalid health values", "[PlayerbotPriest]")
{
    std::vector<float> candidates = { 100.0f, 90.0f, 0.0f, -1.0f,
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 89.0f };
    std::vector<float> attempted;
    REQUIRE_FALSE(PlayerbotPriest::TryInHealthOrder(candidates,
        [](float pct) { return pct; },
        [&](float pct) { attempted.push_back(pct); return false; }));
    REQUIRE(attempted == std::vector<float>{ 89.0f });
}

TEST_CASE("Playerbot healing preserves equal health tie order", "[PlayerbotPriest]")
{
    struct Candidate { int Id; float Health; };
    std::vector<Candidate> candidates = { { 1, 50.0f }, { 2, 20.0f }, { 3, 50.0f } };
    std::vector<int> attempted;
    REQUIRE_FALSE(PlayerbotPriest::TryInHealthOrder(candidates,
        [](Candidate const& candidate) { return candidate.Health; },
        [&](Candidate const& candidate) { attempted.push_back(candidate.Id); return false; }));
    REQUIRE(attempted == std::vector<int>{ 2, 1, 3 });
}

TEST_CASE("Playerbot healing safely handles an empty party candidate list", "[PlayerbotPriest]")
{
    std::vector<float> candidates;
    unsigned attempts = 0;
    REQUIRE_FALSE(PlayerbotPriest::TryInHealthOrder(candidates,
        [](float pct) { return pct; },
        [&](float) { ++attempts; return true; }));
    REQUIRE(attempts == 0);
}

TEST_CASE("Healer kits cover every healing class", "[playerbots][healing]")
{
    using namespace PlayerbotHealerKit;
    for (std::uint8_t playerClass : { ClassPaladin, ClassPriest, ClassShaman, ClassDruid })
    {
        INFO("class " << int(playerClass));
        Kit kit = For(playerClass);
        REQUIRE(Has(playerClass));
        REQUIRE(kit.Emergency[0] != 0);
        REQUIRE(kit.Heal[0] != 0);
        REQUIRE(kit.Resurrect != 0);
    }
    REQUIRE_FALSE(Has(1));
    REQUIRE_FALSE(Has(9));
    REQUIRE(IsHybrid(ClassPaladin));
    REQUIRE_FALSE(IsHybrid(ClassPriest));
    REQUIRE(TierForHealth(50.0f) == Tier::Emergency);
    REQUIRE(TierForHealth(70.0f) == Tier::Heal);
    REQUIRE(TierForHealth(85.0f) == Tier::Hot);
    REQUIRE(TierForHealth(95.0f) == Tier::None);
}

TEST_CASE("Healer mana rules follow the priest spell of the same job", "[playerbots][healing]")
{
    using namespace PlayerbotHealingMana;
    REQUIRE(ForJob(Job::Emergency).Amount == ForPriestSpell(2061).Amount);
    REQUIRE(ForJob(Job::Heal).Mana == ForPriestSpell(2050).Mana);
    REQUIRE(ForJob(Job::Hot).Mana == Efficiency::VeryHigh);
}
