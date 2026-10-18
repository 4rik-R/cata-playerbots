/* GPL v2 or later. Donor provenance and Cata adaptations are in PORTING.md. */
#include "PlayerbotPosition.h"
#include "PlayerbotCombatMovement.h"
#include "PlayerbotTargetSelection.h"
#include "../../Bot/PlayerbotAI.h"
#include "Creature.h"
#include "MotionMaster.h"
#include "Player.h"
#include "PointMovementGenerator.h"
#include "../../Script/PlayerbotConfig.h"
namespace
{
using namespace PlayerbotPosition;
PhaseStamp Phase(Player const& player)
{
    auto const& shift = player.GetPhaseShift();
    PhaseStamp stamp;
    stamp.Flags = uint32(shift.GetFlags().AsUnderlyingType());
    stamp.Personal = uint64(shift.GetPersonalGuid());
    for (auto const& phase : shift.GetPhases()) stamp.Phases.emplace_back(phase.Id, uint32(phase.Flags.AsUnderlyingType()));
    for (auto const& entry : shift.GetVisibleMapIds()) stamp.Terrain.push_back(entry.first);
    for (auto const& entry : shift.GetUiWorldMapAreaIdSwaps()) stamp.UiMaps.push_back(entry.first);
    return stamp;
}
PositionMap* Positions(PlayerbotAI* ai)
{
    auto* context = ai ? ai->GetAiObjectContext() : nullptr;
    auto* value = context ? context->GetValue<PositionMap>("position") : nullptr;
    return value ? &value->RefGet() : nullptr;
}
bool Safe(PlayerbotAI* ai)
{
    Player* bot = ai ? ai->GetBot() : nullptr;
    Player* owner = ai ? ai->GetController() : nullptr;
    return PlayerbotModuleStayEnabled() && bot && owner && owner->IsInWorld() && owner->IsAlive() && !owner->IsBeingTeleported() &&
        bot->IsInPhase(owner) &&
        bot->IsWithinDistInMap(owner, 100.0f) && PlayerbotCombatMovement::CanMove(*bot) &&
        !bot->GetTransport() && !bot->IsFlying() && !bot->HasUnitMovementFlag(MOVEMENTFLAG_FALLING | MOVEMENTFLAG_FALLING_FAR) &&
        !bot->IsInCombat() && !bot->GetVictim() && !bot->IsNonMeleeSpellCast(false) &&
        !PlayerbotTargetSelection::HasNearbyPartyCombat(*bot, *owner) && !ai->GetRestSpellId() &&
        !ai->LootRequests().Pending() && !ai->LootPursuit().Active() && !ai->Rebuff().IsPending(getMSTime());
}
PositionInfo* StayPosition(PlayerbotAI* ai)
{
    auto* positions = Positions(ai);
    if (!positions) return nullptr;
    auto it = positions->find("stay");
    return it != positions->end() ? &it->second : nullptr;
}
bool Valid(PlayerbotAI* ai, PositionInfo const* position)
{
    Player* bot = ai ? ai->GetBot() : nullptr;
    Player* owner = ai ? ai->GetController() : nullptr;
    return bot && owner && owner->IsInWorld() && owner->IsAlive() && bot->IsAlive() &&
        !bot->IsBeingTeleported() && !owner->IsBeingTeleported() && bot->GetMap() == owner->GetMap() &&
        position && position->Matches(bot->GetMapId(), bot->GetInstanceId(), uint64(owner->GetGUID())) &&
        position->BotPhase == Phase(*bot) && position->OwnerPhase == Phase(*owner);
}
ReturnDecision Decision(PlayerbotAI* ai)
{
    auto* position = StayPosition(ai);
    if (!ai || !ai->IsStaying() || !Valid(ai, position) || !Safe(ai) || !position->CanAttempt(getMSTime())) return ReturnDecision::Wait;
    Player* bot = ai->GetBot();
    return DecideReturn(true, !bot->isMoving(), bot->GetDistance(position->X, position->Y, position->Z));
}
bool Capture(PlayerbotAI* ai)
{
    if (!Safe(ai)) return false;
    Player* bot = ai->GetBot();
    auto* positions = Positions(ai);
    if (!positions) return false;
    PositionInfo fresh;
    if (!fresh.Capture(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
        bot->GetMapId(), bot->GetInstanceId(), uint64(ai->GetController()->GetGUID()))) return false;
    fresh.BotPhase = Phase(*bot); fresh.OwnerPhase = Phase(*ai->GetController());
    (*positions)["stay"] = std::move(fresh); return true;
}
class CaptureAction final : public Action
{
public:
    explicit CaptureAction(PlayerbotAI* ai) : Action(ai, "set stay position") { }
    bool isPossible() override { return Safe(botAI); }
    bool Execute(Event) override { return Capture(botAI); }
};
class ReturnAction final : public Action
{
public:
    explicit ReturnAction(PlayerbotAI* ai) : Action(ai, "return to stay position") { }
    bool isUseful() override { return Decision(botAI) != ReturnDecision::Wait; }
    bool Execute(Event) override
    {
        auto decision = Decision(botAI);
        if (decision == ReturnDecision::Wait) return false;
        if (decision == ReturnDecision::Reanchor) return Capture(botAI);
        Player* bot = botAI->GetBot();
        PositionInfo position = *StayPosition(botAI);
        // Native path generation only. Submission is not arrival or path success.
        auto* stored = StayPosition(botAI);
        stored->Attempted = true; stored->LastAttempt = getMSTime();
        bot->GetMotionMaster()->MovePoint(ReturnMovementId, position.X, position.Y, position.Z, true);
        return true;
    }
};
class StayAction final : public Action
{
public:
    explicit StayAction(PlayerbotAI* ai) : Action(ai, "stay") { }
    bool isUseful() override
    {
        auto* position = StayPosition(botAI);
        Player* bot = botAI ? botAI->GetBot() : nullptr;
        return botAI && botAI->IsStaying() && Valid(botAI, position) && Safe(botAI) && bot->isMoving() &&
            bot->GetDistance(position->X, position->Y, position->Z) <= 3.0f;
    }
    bool Execute(Event) override
    {
        if (!isUseful()) return false;
        SuspendReturn(*botAI); // Never cancel another subsystem's point movement.
        return true;
    }
};
class ReturnTrigger final : public Trigger
{
public:
    explicit ReturnTrigger(PlayerbotAI* ai) : Trigger(ai, "return to stay position", 2) { }
    bool IsActive() override { return Decision(botAI) != ReturnDecision::Wait; }
};
}
bool PlayerbotPosition::CaptureStay(PlayerbotAI& ai) { return Capture(&ai); }
bool PlayerbotPosition::ValidStay(PlayerbotAI& ai) { return Valid(&ai, StayPosition(&ai)); }
void PlayerbotPosition::SuspendReturn(PlayerbotAI& ai)
{
    Player* bot = ai.GetBot();
    if (!bot) return;
    auto* movement = bot->GetMotionMaster()->GetMotionSlot(MOTION_SLOT_ACTIVE);
    if (!movement || movement->GetMovementGeneratorType() != POINT_MOTION_TYPE) return;
    auto* point = dynamic_cast<PointMovementGenerator<Player>*>(movement);
    if (!point || !OwnsReturn(point->GetMovementId())) return;
    bot->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
    bot->StopMoving();
}
void PlayerbotPosition::Release(PlayerbotAI& ai)
{
    SuspendReturn(ai);
    if (auto* positions = Positions(&ai)) positions->clear();
    ai.SetStaying(false);
}
void PlayerbotPosition::UpdateReturn(PlayerbotAI& ai)
{
    if (!ai.IsStaying()) return;
    auto* bot = ai.GetBot();
    auto* position = StayPosition(&ai);
    if (!Safe(&ai) || !Valid(&ai, position) ||
        bot->GetDistance(position->X, position->Y, position->Z) <= 3.0f)
        SuspendReturn(ai);
}
void PlayerbotPosition::AddContexts(SharedNamedObjectContextList<Strategy>& strategies,
    SharedNamedObjectContextList<Action>& actions, SharedNamedObjectContextList<Trigger>& triggers,
    SharedNamedObjectContextList<UntypedValue>& values)
{
    auto* value = new NamedObjectContext<UntypedValue>();
    value->creators["position"] = [](PlayerbotAI* ai) { return new PositionValue(ai); }; values.Add(value);
    auto* action = new NamedObjectContext<Action>();
    action->creators["set stay position"] = [](PlayerbotAI* ai) { return new CaptureAction(ai); };
    action->creators["return to stay position"] = [](PlayerbotAI* ai) { return new ReturnAction(ai); };
    action->creators["stay"] = [](PlayerbotAI* ai) { return new StayAction(ai); }; actions.Add(action);
    auto* trigger = new NamedObjectContext<Trigger>();
    trigger->creators["return to stay position"] = [](PlayerbotAI* ai) { return new ReturnTrigger(ai); }; triggers.Add(trigger);
    auto* strategy = new NamedObjectContext<Strategy>();
    strategy->creators["stay"] = [](PlayerbotAI* ai) { return new StayStrategy(ai); }; strategies.Add(strategy);
}
