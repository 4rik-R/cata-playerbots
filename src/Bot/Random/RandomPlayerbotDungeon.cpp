/*
 * Random-population bots playing a dungeon with a human. See
 * RandomPlayerbotDungeon.h, AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "RandomPlayerbotDungeon.h"
#include "RandomPlayerbotMgr.h"
#include "RandomPlayerbotProgression.h"
#include "../PlayerbotAI.h"
#include "../../Ai/Base/PlayerbotCombatDecision.h"
#include "../../Ai/Base/PlayerbotCombatMovement.h"
#include "../../Ai/Base/PlayerbotRestStrategy.h"
#include "../../Ai/Base/PlayerbotRoles.h"
#include "../../Ai/Base/PlayerbotTargetSelection.h"
#include "CellImpl.h"
#include "Creature.h"
#include "DynamicObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "LFG.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "PathGenerator.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "WorldSession.h"
#include <algorithm>
#include <atomic>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

using namespace RandomPlayerbotDungeonPolicy;

namespace
{
std::atomic<uint32_t> PullCount { 0 }, MarkCount { 0 }, ControlCount { 0 }, AvoidCount { 0 }, BreakCount { 0 },
    LeadCount { 0 };

// What the last dungeon tank did and why, for the status line.
std::mutex NoteLock;
std::string TankNote;

void Note(Player const& tank, std::string const& what)
{
    std::string note = tank.GetName() + " " + what;
    std::lock_guard<std::mutex> guard(NoteLock);
    if (TankNote != note)
        TankNote = std::move(note);
}

std::mutex PausedLock;
std::set<ObjectGuid> PausedGroups;

constexpr uint8_t MarkIcons[3] = { Skull, Cross, Moon };
constexpr uint32_t MarkEveryMs = 1000;
constexpr uint32_t BreakEveryMs = 2000;
constexpr uint32_t EscapeMs = 3000;
constexpr float GroupRange = 40.0f;

bool IsHuman(Player const* member)
{
    return member && member->GetSession() && !member->GetSession()->IsServerOrigin();
}

float ManaPct(Player const& player)
{
    int32 maximum = player.GetMaxPower(POWER_MANA);
    return maximum > 0 ? 100.0f * player.GetPower(POWER_MANA) / maximum : 100.0f;
}

bool UsesMana(Player const& player)
{
    return player.GetPowerType() == POWER_MANA && player.GetMaxPower(POWER_MANA) > 0;
}

bool Boss(Creature const& creature)
{
    return creature.IsDungeonBoss() || creature.isWorldBoss();
}

std::vector<Creature*> HostilesAround(WorldObject& center, Player& bot, float range)
{
    std::list<Unit*> units;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(&center, &bot, range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(&center, units, check);
    Cell::VisitAllObjects(&center, searcher, range);
    std::vector<Creature*> creatures;
    for (Unit* unit : units)
        if (Creature* creature = unit ? unit->ToCreature() : nullptr)
            if (creature->IsAlive() && !creature->IsControlledByPlayer() && !creature->IsTotem() && !creature->IsPet() &&
                !creature->IsCritter() && !creature->IsTrigger() && !creature->IsCivilian())
                creatures.push_back(creature);
    return creatures;
}

bool FightsGroup(Creature const& creature, Group const& group)
{
    if (!creature.IsInCombat())
        return false;
    for (GroupReference const* ref = group.GetFirstMember(); ref; ref = ref->next())
        if (Player* member = ref->GetSource(); member && member->IsInWorld() && creature.IsInCombatWith(member))
            return true;
    return false;
}

// A group bot that could crowd-control this creature with a spell it knows; a healer only when nobody else can.
Player* Controller(Group const& group, Creature const& creature)
{
    Player* best = nullptr;
    auto rank = [](Player const* member) { return std::make_pair(PlayerbotRoles::IsHealer(*member), member->GetGUID()); };
    for (GroupReference const* ref = group.GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || IsHuman(member) || !member->IsInWorld() || !member->IsAlive() || member->GetMap() != creature.GetMap())
            continue;
        CrowdControlSpell const* spell = CrowdControlFor(member->getClass(), creature.GetCreatureType());
        if (spell && member->HasSpell(spell->SpellId) && (!best || rank(member) < rank(best)))
            best = member;
    }
    return best;
}

bool Controlled(Creature const& creature)
{
    for (CrowdControlSpell const& spell : CrowdControlSpells)
        if (creature.HasAura(spell.SpellId))
            return true;
    return PlayerbotTargetSelection::IsProtectedTarget(creature);
}

struct Hazard
{
    Position Center;
    float Radius;
};

// Ground effects of hostile casters (fire, poison, void zones) and invisible trigger creatures
// that pulse a harmful area spell.
std::vector<Hazard> HazardsAround(Player& bot)
{
    std::vector<Hazard> hazards;
    std::list<WorldObject*> objects;
    Trinity::AllWorldObjectsInRange check(&bot, HazardScanRange);
    Trinity::WorldObjectListSearcher<Trinity::AllWorldObjectsInRange> searcher(&bot, objects, check,
        GRID_MAP_TYPE_MASK_DYNAMICOBJECT | GRID_MAP_TYPE_MASK_CREATURE);
    Cell::VisitAllObjects(&bot, searcher, HazardScanRange);
    for (WorldObject* object : objects)
    {
        if (DynamicObject* area = object->ToDynObject())
        {
            Unit* caster = area->GetCaster();
            SpellInfo const* spell = sSpellMgr->GetSpellInfo(area->GetSpellId());
            float radius = area->GetRadius();
            if (caster && caster != &bot && caster->IsHostileTo(&bot) && spell && !spell->IsPositive() &&
                radius > 0.0f && radius <= 20.0f)
                hazards.push_back({ area->GetPosition(), radius });
            continue;
        }
        Creature* creature = object->ToCreature();
        if (!creature || !creature->IsAlive() || !creature->IsHostileTo(&bot) ||
            !(creature->IsTrigger() || creature->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE)))
            continue;
        for (AuraEffect const* effect : creature->GetAuraEffectsByType(SPELL_AURA_PERIODIC_TRIGGER_SPELL))
        {
            SpellInfo const* aura = effect->GetSpellInfo();
            SpellInfo const* pulse = aura ? sSpellMgr->GetSpellInfo(aura->Effects[effect->GetEffIndex()].TriggerSpell) : nullptr;
            if (!pulse || pulse->IsPositive())
                continue;
            float radius = 0.0f;
            for (SpellEffectInfo const& pulseEffect : pulse->Effects)
                if (pulseEffect.IsEffect() && pulseEffect.HasRadius(SpellTargetIndex::TargetA))
                    radius = std::max(radius, pulseEffect.CalcRadius());
            if (radius > 0.0f && radius <= 20.0f)
                hazards.push_back({ creature->GetPosition(), radius });
        }
    }
    return hazards;
}

Hazard const* Covering(std::vector<Hazard> const& hazards, Position const& point)
{
    for (Hazard const& hazard : hazards)
        if (std::abs(point.GetPositionZ() - hazard.Center.GetPositionZ()) < 5.0f &&
            Inside({ point.GetPositionX(), point.GetPositionY() },
                { hazard.Center.GetPositionX(), hazard.Center.GetPositionY() }, hazard.Radius))
            return &hazard;
    return nullptr;
}
}

Player* RandomPlayerbotDungeon::Human(Player& bot, uint32_t accountId)
{
    Group* group = bot.GetGroup();
    if (!group || !group->isLFGGroup() || !bot.IsInWorld() || !bot.GetMap()->IsDungeon() ||
        !RandomPlayerbotMgr::IsRandomAccount(accountId))
        return nullptr;
    Player* human = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (IsHuman(member) && member->IsInWorld() && member->GetMap() == bot.GetMap() &&
            (!human || group->IsLeader(member->GetGUID())))
            human = member;
    }
    return human;
}

bool RandomPlayerbotDungeon::IsTank(Player const& bot)
{
    return RandomPlayerbotProgression::DungeonRole(bot) == lfg::PLAYER_ROLE_TANK || PlayerbotRoles::IsTank(bot);
}

Creature* RandomPlayerbotDungeon::SelectPull(Player& tank, Player& human, State& state, uint32_t now)
{
    auto hold = [&](std::string const& why) -> Creature*
    {
        state.ReadySinceMs = 0;
        StopLeading(tank, state);
        Note(tank, why);
        return nullptr;
    };
    Group* group = tank.GetGroup();
    if (!group)
        return hold("no group");
    if (Paused(*group))
        return hold("waits (bots warten)");
    if (!tank.IsAlive() || !human.IsAlive())
        return hold("waits (someone is dead)");
    if (tank.IsInCombat() || human.IsInCombat() || PlayerbotTargetSelection::HasNearbyPartyCombat(tank, human))
        return hold("fights");
    if (tank.IsNonMeleeSpellCast(false))
        return nullptr;
    float humanToTank = tank.GetDistance(&human);
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsInWorld() || member->GetMap() != tank.GetMap() || member == &tank)
            continue;
        // A member still on the way, dead or below the line holds the pull, like a tank checking the party frames.
        if (!member->IsAlive())
            return hold("waits for " + member->GetName() + " (dead)");
        if (member != &human && !member->IsWithinDistInMap(&human, GroupRange))
            return hold("waits for " + member->GetName() + " (far away)");
        if (member->GetHealthPct() < ReadyPct)
            return hold("waits for " + member->GetName() + " (health)");
        if (!RandomPlayerbotDungeonPolicy::Ready(100.0f, ManaPct(*member), UsesMana(*member)))
            return hold("waits for " + member->GetName() + " (mana)");
    }
    if (!RandomPlayerbotDungeonPolicy::Ready(tank.GetHealthPct(), ManaPct(tank), UsesMana(tank)))
        return hold("waits (own health or mana)");
    if (!state.ReadySinceMs)
    {
        state.ReadySinceMs = now;
        state.PullAfterMs = now + PullPauseMs(tank.GetGUID().GetCounter(), state.Pulls);
    }
    if (int32(now - state.PullAfterMs) < 0)
        return nullptr;

    auto pullable = [&tank](Creature* creature)
    {
        CreatureTemplate const* info = creature->GetCreatureTemplate();
        return info && !info->npcflag && !creature->IsInEvadeMode() && creature->IsHostileTo(&tank) &&
            !PlayerbotTargetSelection::IsProtectedTarget(*creature) && tank.IsValidAttackTarget(creature) &&
            tank.CanSeeOrDetect(creature);
    };

    // An enemy right in front of the human first: the human sets the direction.
    Creature* best = nullptr;
    PullCandidate bestCandidate {};
    for (Creature* creature : HostilesAround(human, tank, PullRange))
    {
        if (!pullable(creature) || !tank.IsWithinLOSInMap(creature) || !human.IsWithinLOSInMap(creature))
            continue;
        PullCandidate candidate { human.GetDistance(creature),
            std::abs(creature->GetPositionZ() - human.GetPositionZ()), Boss(*creature) };
        if (PullAllowed(candidate, humanToTank) && (!best || BetterPull(candidate, bestCandidate)))
        {
            best = creature;
            bestCandidate = candidate;
        }
    }
    if (best)
    {
        StopLeading(tank, state);
        return best;
    }

    // Otherwise the tank walks on to the nearest pack it can reach, the one it already heads for first.
    Creature* pack = state.LeadTarget.IsEmpty() ? nullptr : ObjectAccessor::GetCreature(tank, state.LeadTarget);
    if (pack && (!pack->IsAlive() || !pullable(pack)))
        pack = nullptr;
    LeadView view {};
    auto measure = [&](Creature* creature) -> bool
    {
        float straight = tank.GetDistance(creature);
        float height = std::abs(creature->GetPositionZ() - tank.GetPositionZ());
        if (straight > LeadRange || height > FloorHeight)
            return false;
        PathGenerator path(&tank);
        path.CalculatePath(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ());
        float length = 0.0f;
        if (path.GetPathType() & (PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH))
        {
            Movement::PointsArray const& points = path.GetPath();
            for (size_t i = 1; i < points.size(); ++i)
                length += (points[i] - points[i - 1]).length();
            length = std::max(length, 0.1f);
        }
        view = { humanToTank, straight, length, height, tank.IsWithinLOSInMap(creature), Boss(*creature), state.Waiting };
        return length > 0.0f && length <= straight * LeadPathFactor + 10.0f;
    };
    if (pack && !measure(pack))
        pack = nullptr;
    if (!pack)
    {
        std::vector<Creature*> around = HostilesAround(tank, tank, LeadRange);
        std::sort(around.begin(), around.end(),
            [&tank](Creature* a, Creature* b) { return tank.GetExactDistSq(a) < tank.GetExactDistSq(b); });
        uint32_t tried = 0;
        for (Creature* creature : around)
        {
            if (!pullable(creature))
                continue;
            if (++tried > 6) // paths are not free; the nearest few decide
                break;
            if (measure(creature))
            {
                pack = creature;
                break;
            }
        }
    }
    if (!pack)
        return hold("follows (no pack in reach)");

    switch (RandomPlayerbotDungeonPolicy::Lead(view))
    {
        case LeadStep::Follow:
            return hold("follows (human elsewhere)");
        case LeadStep::Pull:
            StopLeading(tank, state);
            return pack;
        case LeadStep::Wait:
            if (!state.Waiting || !state.Leading)
            {
                tank.GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
                tank.StopMoving();
                tank.SetFacingToObject(&human);
            }
            state.Leading = true;
            state.Waiting = true;
            state.LeadTarget = pack->GetGUID();
            Note(tank, "waits for " + human.GetName() + " before " + pack->GetName());
            return nullptr;
        case LeadStep::Walk:
            break;
    }
    if (!state.Leading || state.Waiting || state.LeadTarget != pack->GetGUID() || int32(now - state.NextLeadMoveMs) >= 0)
    {
        if (PlayerbotCombatMovement::CanMove(tank))
            tank.GetMotionMaster()->MovePoint(0, pack->GetPosition(), true);
        state.NextLeadMoveMs = now + LeadMoveEveryMs;
    }
    if (!state.Leading)
        ++LeadCount;
    state.Leading = true;
    state.Waiting = false;
    state.LeadTarget = pack->GetGUID();
    Note(tank, "leads to " + pack->GetName());
    return nullptr;
}

void RandomPlayerbotDungeon::StopLeading(Player& tank, State& state)
{
    if (state.Leading)
    {
        tank.GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
        tank.StopMoving();
    }
    state.Leading = false;
    state.Waiting = false;
    state.LeadTarget.Clear();
}

void RandomPlayerbotDungeon::Pulled(Player& tank, Creature& target, State& state, uint32_t now)
{
    ++state.Pulls;
    state.ReadySinceMs = 0;
    state.NextMarkMs = 0;
    ++PullCount;
    TC_LOG_DEBUG("module.playerbots", "PB-DUNGEON: %s pulls %s", tank.GetName().c_str(), target.GetName().c_str());
    UpdateMarks(tank, &target, state, now);
}

void RandomPlayerbotDungeon::UpdateMarks(Player& tank, Creature* target, State& state, uint32_t now)
{
    Group* group = tank.GetGroup();
    if (!group || int32(now - state.NextMarkMs) < 0)
        return;
    state.NextMarkMs = now + MarkEveryMs;

    auto alive = [&tank](ObjectGuid guid) -> Creature*
    {
        Creature* creature = guid.IsEmpty() ? nullptr : ObjectAccessor::GetCreature(tank, guid);
        return creature && creature->IsAlive() ? creature : nullptr;
    };
    // Every living mark stays; a dead one is free again. The cross moves up once the skull is dead.
    ObjectGuid current[3];
    for (int i = 0; i < 3; ++i)
        if (alive(group->GetTargetIcon(MarkIcons[i])))
            current[i] = group->GetTargetIcon(MarkIcons[i]);
    bool ownCross = !current[1].IsEmpty() && current[1] == state.Marked[1];
    if (current[0].IsEmpty() && ownCross)
    {
        current[0] = current[1];
        current[1].Clear();
    }

    std::vector<Enemy> enemies;
    std::map<uint64_t, ObjectGuid> guids;
    for (ObjectGuid const& mark : current)
        guids[mark.GetRawValue()] = mark;
    if (target)
        guids[target->GetGUID().GetRawValue()] = target->GetGUID();
    for (Creature* creature : HostilesAround(tank, tank, 30.0f))
        if (FightsGroup(*creature, *group))
        {
            guids[creature->GetGUID().GetRawValue()] = creature->GetGUID();
            enemies.push_back({ creature->GetGUID().GetRawValue(), float(creature->GetHealth()),
                !Boss(*creature) && Controller(*group, *creature) != nullptr, Boss(*creature) });
        }
    uint64_t tankTarget = target && target->IsAlive() ? target->GetGUID().GetRawValue() : 0;
    Marks fixed { current[0].GetRawValue(), current[1].GetRawValue(), current[2].GetRawValue() };
    Marks planned = PlanMarks(tankTarget, enemies, fixed);
    uint64_t wanted[3] = { planned.Skull, planned.Cross, planned.Moon };
    for (int i = 0; i < 3; ++i)
    {
        if (!wanted[i] || wanted[i] == group->GetTargetIcon(MarkIcons[i]).GetRawValue())
            continue;
        ObjectGuid guid = guids[wanted[i]];
        group->SetTargetIcon(MarkIcons[i], tank.GetGUID(), guid);
        state.Marked[i] = guid;
        ++MarkCount;
    }
}

bool RandomPlayerbotDungeon::CrowdControl(Player& bot, State& state, uint32_t now)
{
    Group* group = bot.GetGroup();
    if (!group || !bot.IsAlive() || bot.IsNonMeleeSpellCast(false))
        return false;
    ObjectGuid moonGuid = group->GetTargetIcon(Moon);
    Creature* moon = moonGuid.IsEmpty() ? nullptr : ObjectAccessor::GetCreature(bot, moonGuid);
    if (!moon || !moon->IsAlive() || Boss(*moon) || Controlled(*moon) || Controller(*group, *moon) != &bot)
    {
        state.CrowdControlNoticed.Clear();
        return false;
    }
    CrowdControlSpell const* spell = CrowdControlFor(bot.getClass(), moon->GetCreatureType());
    if (!spell || !bot.IsWithinDistInMap(moon, 30.0f) || !bot.IsWithinLOSInMap(moon))
        return false;
    // A moment to notice the mark, like a player reading it.
    if (state.CrowdControlNoticed != moonGuid)
    {
        state.CrowdControlNoticed = moonGuid;
        state.CrowdControlAfterMs = now + ReactMs(bot.GetGUID().GetCounter(), moonGuid.GetCounter());
    }
    if (int32(now - state.CrowdControlAfterMs) < 0)
        return true;
    if (!PlayerbotDecision::TryCast(bot, *moon, spell->SpellId, spell->Name))
        return false;
    ++ControlCount;
    TC_LOG_DEBUG("module.playerbots", "PB-DUNGEON: %s casts %s on %s", bot.GetName().c_str(), spell->Name,
        moon->GetName().c_str());
    return true;
}

bool RandomPlayerbotDungeon::AvoidHazards(Player& bot, Player& human, State& state, uint32_t now)
{
    if (int32(now - state.EscapeUntilMs) < 0 && bot.isMoving())
        return true;
    if (int32(now - state.NextHazardCheckMs) < 0 || !bot.IsAlive() || bot.HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_STUNNED))
        return false;
    state.NextHazardCheckMs = now + HazardCheckMs;
    std::vector<Hazard> hazards = HazardsAround(bot);
    Hazard const* hazard = Covering(hazards, bot.GetPosition());
    if (!hazard)
    {
        state.HazardNoticedMs = 0;
        return false;
    }
    // Half a reaction: players see the fire under their feet a moment later.
    if (!state.HazardNoticedMs)
        state.HazardNoticedMs = now + ReactMs(bot.GetGUID().GetCounter(), now / 1000) / 2;
    if (int32(now - state.HazardNoticedMs) < 0)
        return false;
    state.HazardNoticedMs = 0;

    Point center { hazard->Center.GetPositionX(), hazard->Center.GetPositionY() };
    Point anchor { human.GetPositionX(), human.GetPositionY() };
    float offset = EscapeOffset(bot.GetGUID().GetCounter());
    for (float extra : { 0.0f, 0.8f, -0.8f, 1.6f })
    {
        Point out = EscapePoint({ bot.GetPositionX(), bot.GetPositionY() }, center, hazard->Radius, anchor, offset + extra);
        float z = bot.GetMap()->GetHeight(bot.GetPhaseShift(), out.X, out.Y, bot.GetPositionZ() + 2.0f);
        Position destination(out.X, out.Y, z);
        if (z <= INVALID_HEIGHT || std::abs(z - bot.GetPositionZ()) > 4.0f || Covering(hazards, destination) ||
            !bot.IsWithinLOS(out.X, out.Y, z + 1.0f))
            continue;
        bot.InterruptNonMeleeSpells(false);
        bot.GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
        bot.GetMotionMaster()->MovePoint(0, destination, true);
        state.EscapeUntilMs = now + EscapeMs;
        ++AvoidCount;
        TC_LOG_DEBUG("module.playerbots", "PB-DUNGEON: %s steps out of a harmful area", bot.GetName().c_str());
        return true;
    }
    return false;
}

bool RandomPlayerbotDungeon::HazardAt(Player& bot, Position const& point)
{
    return Covering(HazardsAround(bot), point) != nullptr;
}

bool RandomPlayerbotDungeon::TakeBreak(PlayerbotAI& ai, Player& bot, State& state, uint32_t now)
{
    if (int32(now - state.NextBreakMs) < 0 || ai.GetRestSpellId() || !bot.IsAlive() || bot.IsInCombat() ||
        bot.IsMounted() || bot.isMoving() || bot.IsNonMeleeSpellCast(false))
        return false;
    state.NextBreakMs = now + BreakEveryMs + ReactMs(bot.GetGUID().GetCounter(), now / 1000);
    // A druid drinks in caster form; a bear tank lives on rage.
    bool usesMana = bot.GetMaxPower(POWER_MANA) > 0 && (bot.GetPowerType() == POWER_MANA ||
        (bot.getClass() == CLASS_DRUID && bot.GetShapeshiftForm() != FORM_BEAR));
    Break need = BreakNeeded(bot.GetHealthPct(), ManaPct(bot), usesMana);
    if (need == Break::None)
        return false;
    if (bot.getClass() == CLASS_DRUID && bot.GetShapeshiftForm() != FORM_NONE && bot.GetShapeshiftForm() != FORM_BEAR)
        bot.RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
    if (!PlayerbotRest::Begin(ai, need == Break::Drink ? PlayerbotRest::Kind::Drink : PlayerbotRest::Kind::Food))
        return false;
    ++BreakCount;
    return true;
}

void RandomPlayerbotDungeon::Command(Player& sender, Group& group, RandomPlayerbotDungeonPolicy::Command command)
{
    if (command == RandomPlayerbotDungeonPolicy::Command::None || !group.isLFGGroup())
        return;
    bool wait = command == RandomPlayerbotDungeonPolicy::Command::Wait;
    {
        std::lock_guard<std::mutex> guard(PausedLock);
        if (wait)
            PausedGroups.insert(group.GetGUID());
        else
            PausedGroups.erase(group.GetGUID());
    }
    // The bot tank answers like a player would.
    for (GroupReference* ref = group.GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && !IsHuman(member) && member->IsInWorld() && member->GetMap() == sender.GetMap() && IsTank(*member))
        {
            member->Say(wait ? "Ok, ich warte." : "Weiter geht's.", LANG_UNIVERSAL);
            break;
        }
    }
}

bool RandomPlayerbotDungeon::Paused(Group const& group)
{
    std::lock_guard<std::mutex> guard(PausedLock);
    return PausedGroups.count(group.GetGUID()) != 0;
}

std::string RandomPlayerbotDungeon::Describe()
{
    std::string tank;
    {
        std::lock_guard<std::mutex> guard(NoteLock);
        if (!TankNote.empty())
            tank = "; tank " + TankNote;
    }
    return "pulls " + std::to_string(PullCount.load()) + ", marks " + std::to_string(MarkCount.load()) +
        ", crowd controls " + std::to_string(ControlCount.load()) + ", areas avoided " + std::to_string(AvoidCount.load()) +
        ", breaks " + std::to_string(BreakCount.load()) + ", leads " + std::to_string(LeadCount.load()) + tank;
}
