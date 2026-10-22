/* Adapted from donor TargetValue / DpsTargetValue / RtiValue at
 * 7bae1b5c58c76a0aa20381155edc08096d1485b2. See PORTING.md.
 * Released under GNU GPL v2 or any later version. */
#include "PlayerbotTargetSelection.h"
#include "PlayerbotRoles.h"
#include "../../Bot/Engine/Engine.h"
#include "../../Bot/PlayerbotAI.h"
#include "Creature.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ThreatManager.h"
#include <algorithm>

namespace
{
bool ImplementedTank(Player const* player)
{
    return player && PlayerbotRoles::IsTank(*player);
}
bool ImplementedNonTank(Player const* player)
{
    return player && !PlayerbotRoles::IsTankOrMainTank(*player);
}
class TargetGuidValue final : public CalculatedValue<ObjectGuid>
{
public:
    TargetGuidValue(PlayerbotAI* ai, bool tank) : CalculatedValue(ai, tank ? "tank target" : "dps target"), tank(tank) { }
protected:
    ObjectGuid Calculate() override
    {
        Engine const* engine = botAI ? botAI->GetDecisionEngine() : nullptr;
        if (!engine) return ObjectGuid::Empty;
        return tank ? PlayerbotTargetSelection::SelectTankTarget(*botAI, *engine) :
            PlayerbotTargetSelection::SelectDpsTarget(*botAI, *engine);
    }
private:
    bool tank;
};
}
bool PlayerbotTargetSelection::IsProtectedTarget(Creature const& target)
{
    return !CrowdControlAllows(target.IsPolymorphed(), target.IsCharmed(), target.isFeared(),
        target.HasUnitState(UNIT_STATE_ISOLATED));
}
void PlayerbotTargetSelection::AddContexts(SharedNamedObjectContextList<UntypedValue>& values)
{
    auto* factory = new NamedObjectContext<UntypedValue>();
    factory->creators["prioritized targets"] = [](PlayerbotAI* ai) { return new ManualSetValue<std::vector<ObjectGuid>>(ai, {}, "prioritized targets"); };
    factory->creators["rti"] = [](PlayerbotAI* ai) { return new ManualSetValue<std::string>(ai, "skull", "rti"); };
    factory->creators["rti cc"] = [](PlayerbotAI* ai) { return new ManualSetValue<std::string>(ai, "moon", "rti cc"); };
    factory->creators["dps target"] = [](PlayerbotAI* ai) { return new TargetGuidValue(ai, false); };
    factory->creators["tank target"] = [](PlayerbotAI* ai) { return new TargetGuidValue(ai, true); };
    values.Add(factory);
}
namespace
{
bool NearbyPartyMember(Player const& bot, Player const& owner, Player const* member)
{
    return member && member->IsInWorld() && member->IsAlive() && !member->IsBeingTeleported() &&
        member->GetMap() == bot.GetMap() && owner.IsWithinDistInMap(member, 35.0f);
}
}

bool PlayerbotTargetSelection::HasNearbyPartyCombat(Player const& bot, Player const& owner)
{
    if (bot.IsInCombat() || owner.IsInCombat()) return true;
    if (!bot.IsInWorld() || !owner.IsInWorld() || bot.GetMap() != owner.GetMap()) return false;
    Group const* group = bot.GetGroup();
    bool attached = group && owner.GetGroup() == group && group->IsMember(owner.GetGUID()) && group->IsMember(bot.GetGUID());
    if (!attached) return false;
    for (GroupReference const* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        bool eligible = NearbyPartyMember(bot, owner, member);
        if (CombatScopeAllows(false, attached, eligible, eligible && member->IsInCombat())) return true;
    }
    return false;
}

bool PlayerbotTargetSelection::IsEngagedWithAttachedParty(Player const& bot, Player const& owner, Creature const& target)
{
    if (!bot.IsInWorld() || !owner.IsInWorld() || !bot.IsAlive() || !owner.IsAlive() ||
        bot.IsBeingTeleported() || owner.IsBeingTeleported() || bot.GetMap() != owner.GetMap() ||
        target.GetMap() != bot.GetMap() || !target.IsInWorld() || !target.IsAlive() ||
        target.IsControlledByPlayer() || !bot.IsWithinDistInMap(&owner, 35.0f))
        return false;
    if (owner.IsInCombatWith(&target)) return true;
    Group const* group = bot.GetGroup();
    bool attached = group && owner.GetGroup() == group && group->IsMember(owner.GetGUID()) && group->IsMember(bot.GetGUID());
    if (!attached) return false;
    for (GroupReference const* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        bool eligible = NearbyPartyMember(bot, owner, member);
        if (CombatScopeAllows(false, attached, eligible, eligible && member->IsInCombatWith(&target)))
            return true;
    }
    return false;
}

static ObjectGuid SelectTarget(PlayerbotAI& ai, Engine const& engine, bool tank)
{
    using namespace PlayerbotTargetSelection;
    Player* bot = ai.GetBot();
    Player* owner = ai.GetController();
    AiObjectContext* context = ai.GetAiObjectContext();
    if (!bot || !owner || !context || !bot->IsAlive() || !owner->IsAlive() ||
        bot->IsBeingTeleported() || owner->IsBeingTeleported() || !bot->IsInWorld() || !owner->IsInWorld() ||
        bot->GetMap() != owner->GetMap() || !bot->IsWithinDistInMap(owner, 35.0f))
        return ObjectGuid::Empty;
    if (tank && !ImplementedTank(bot)) return ObjectGuid::Empty;
    GuidSet excluded = engine.GatherTargetExclusions(tank ? TargetValueExclusionType::Tank : TargetValueExclusionType::Dps);
    Group* group = bot->GetGroup();
    ObjectGuid moon = group ? group->GetTargetIcon(4) : ObjectGuid::Empty;
    auto* cc = context->GetValue<std::string>("rti cc");
    int ccIndex = cc ? IconIndex(cc->Get()) : -1;
    ObjectGuid ccGuid = group && ccIndex >= 0 ? group->GetTargetIcon(ccIndex) : ObjectGuid::Empty;
    auto eligible = [&](ObjectGuid guid) -> Creature*
    {
        if (guid.IsEmpty() || guid == moon || guid == ccGuid || excluded.count(guid)) return nullptr;
        Creature* target = ObjectAccessor::GetCreature(*bot, guid);
        if (!target || !target->IsAlive() || target->IsControlledByPlayer() || target->IsInEvadeMode() ||
            IsProtectedTarget(*target) || !IsEngagedWithAttachedParty(*bot, *owner, *target) || !bot->IsValidAttackTarget(target) ||
            !bot->CanSeeOrDetect(target) || !bot->IsWithinLOSInMap(target) ||
            !bot->IsWithinDistInMap(target, 25.0f) || !owner->IsWithinDistInMap(target, 25.0f) ||
            (target->IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL) && target->IsImmunedToDamage(SPELL_SCHOOL_MASK_MAGIC)))
            return nullptr;
        return target;
    };
    auto* rti = context->GetValue<std::string>("rti");
    int index = rti ? IconIndex(rti->Get()) : -1;
    ObjectGuid icon = group && index >= 0 ? group->GetTargetIcon(index) : ObjectGuid::Empty;
    if (Creature* marked = eligible(icon))
    {
        // Tank icon fast path only for a non-tank player victim in this role subset.
        Unit* victim = marked->GetVictim();
        if (!tank || (victim && victim != bot && ImplementedNonTank(victim->ToPlayer()))) return icon;
    }
    auto* attackers = context->GetValue<std::vector<ObjectGuid>>("attackers");
    if (!attackers) return ObjectGuid::Empty;
    auto* priorityValue = context->GetValue<std::vector<ObjectGuid>>("prioritized targets");
    std::vector<ObjectGuid> priority = priorityValue ? priorityValue->Get() : std::vector<ObjectGuid>{};
    auto* dpsValue = tank ? nullptr : context->GetValue<float>("estimated group dps");
    float dps = dpsValue ? dpsValue->Get() : 0.0f;
    bool estimated = ValidEstimate(dps);
    unsigned nearCount = 1;
    if (group)
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource(); member && member != bot && member->IsInWorld() &&
                member->IsAlive() && member->GetMap() == bot->GetMap() && bot->GetExactDist(member) <= 100.0f)
                ++nearCount;
    bool casterRanking = estimated && nearCount > 3 && PlayerbotRoles::IsRanged(*bot);
    ObjectGuid skull = group ? group->GetTargetIcon(7) : ObjectGuid::Empty;
    // Cross comes next once the skull is dead or out of reach.
    if (group && !eligible(skull))
        skull = group->GetTargetIcon(6);
    Creature* current = ai.GetCurrentTarget();
    bool retainTankTarget = tank && RetainMainTankTarget(PlayerbotRoles::IsExplicitMainTank(*bot),
        PlayerbotRoles::GroupTankCount(*bot));
    Candidate best{};
    TankCandidate tankBest{};
    ObjectGuid result;
    for (ObjectGuid guid : attackers->Get())
    {
        Creature* target = eligible(guid);
        if (!target) continue;
        float distance = bot->GetDistance(target);
        if (tank)
        {
            Unit* victim = target->GetVictim();
            TankCandidate next{distance, target->GetThreatManager().GetThreat(bot),
                HasTankAggro(victim != nullptr, victim == bot, victim && victim->ToPlayer() &&
                    PlayerbotRoles::IsTankOrMainTank(*victim->ToPlayer())),
                bot->IsWithinMeleeRange(target), current && current->GetGUID() == guid};
            if (result.IsEmpty() || BetterTank(next, tankBest, retainTankTarget)) { tankBest = next; result = guid; }
            continue;
        }
        bool ranged = PlayerbotRoles::IsRanged(*bot);
        Candidate next{distance, estimated ? float(target->GetHealth()) / dps : float(target->GetHealth()),
            ranged ? distance < 30.0f : bot->IsWithinMeleeRange(target), current && current->GetGUID() == guid,
            guid == skull || std::find(priority.begin(), priority.end(), guid) != priority.end()};
        // Unknown DPS uses general health/range ranking, never unsafe lifetime division.
        if (result.IsEmpty() || Better(next, best, casterRanking)) { best = next; result = guid; }
    }
    return result;
}
ObjectGuid PlayerbotTargetSelection::SelectDpsTarget(PlayerbotAI& ai, Engine const& engine)
{
    return SelectTarget(ai, engine, false);
}
ObjectGuid PlayerbotTargetSelection::SelectTankTarget(PlayerbotAI& ai, Engine const& engine)
{
    return SelectTarget(ai, engine, true);
}
