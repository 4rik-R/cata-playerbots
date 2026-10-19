/* Cata adaptation of donor facing/reach/behind actions at
 * 037c01418b5d01506917a3db9b44fd56ac5f965c. See AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version. */
#include "PlayerbotCombatMovement.h"
#include "PlayerbotGroupStrategy.h"
#include "PlayerbotTargetSelection.h"
#include "PlayerbotRoles.h"
#include "../../Bot/PlayerbotAI.h"
#include "../../Script/PlayerbotConfig.h"
#include "Creature.h"
#include "Log.h"
#include "MotionMaster.h"
#include "Player.h"
#include "Map.h"
#include "Chat.h"
#include "PlayerbotSecurity.h"

namespace
{
using namespace PlayerbotCombatMovement;
bool Ready(PlayerbotAI* ai, Step step, bool refresh = false)
{
    Player* bot = ai ? ai->GetBot() : nullptr;
    Player* owner = ai ? ai->GetController() : nullptr;
    Creature* target = ai ? ai->GetCurrentTarget() : nullptr;
    if (!bot || !owner || !target || ai->IsStaying()) return false;
    bool caster = bot->getClass() == CLASS_MAGE;
    bool enabled = caster ? PlayerbotModuleEngineMageCombatEnabled() :
        bot->getClass() == CLASS_WARRIOR && PlayerbotModuleEngineWarriorCombatEnabled();
    if (!enabled || (caster && (step == Step::Reach || step == Step::Behind)) ||
        (!caster && step == Step::ReachSpell) || !bot->IsInWorld() || !owner->IsInWorld() || !target->IsInWorld() ||
        !bot->IsAlive() || !owner->IsAlive() || !target->IsAlive() ||
        bot->IsBeingTeleported() || owner->IsBeingTeleported() ||
        bot->GetMap() != owner->GetMap() || bot->GetMap() != target->GetMap() ||
        bot->GetVictim() != target || target->IsControlledByPlayer() || PlayerbotTargetSelection::IsProtectedTarget(*target) ||
        !bot->IsValidAttackTarget(target) ||
        !bot->IsWithinDistInMap(owner, 35.0f) || !bot->IsWithinDistInMap(target, 35.0f) ||
        !bot->CanSeeOrDetect(target) || !bot->IsWithinLOSInMap(target) ||
        !CanMove(*bot) || bot->IsMounted() || bot->IsNonMeleeSpellCast(false) ||
        ai->GetRestSpellId() || ai->LootRequests().Pending() || ai->LootPursuit().Active())
        return false;
    if (refresh) return caster;
    bool tanking = UsesFrontPosition(target->GetVictim() == bot, PlayerbotRoles::IsTank(*bot));
    PositionState state{target->IsWithinMeleeRange(bot), bot->HasInArc(2.0f * float(M_PI) / 3.0f, target),
        !target->HasInArc(float(M_PI), bot), tanking, !bot->IsStopped(), target->isMoving(),
        bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE,
        caster && bot->IsWithinDistInMap(target, GetRange(*ai, "spell"))};
    return Useful(step, state);
}
class MovementTrigger final : public Trigger
{
public:
    MovementTrigger(PlayerbotAI* ai, char const* name, Step step) : Trigger(ai, name, 1), step(step) { }
    bool IsActive() override { return Ready(botAI, step); }
private:
    Step step;
};
class MovementAction final : public Action
{
public:
    MovementAction(PlayerbotAI* ai, char const* name, Step step) : Action(ai, name), step(step) { }
    bool isUseful() override { return Ready(botAI, step); }
    bool Execute([[maybe_unused]] Event event) override
    {
        if (!isUseful()) return false;
        Player* bot = botAI->GetBot();
        Creature* target = botAI->GetCurrentTarget();
        if (step == Step::Facing)
        {
            if (!FaceForAttack(*bot, *target)) return false;
        }
        else if (step == Step::ReachSpell)
            bot->GetMotionMaster()->MoveChase(target, GetRange(*botAI, "spell"));
        else
            bot->GetMotionMaster()->MoveChase(target, std::nullopt,
                ChaseAngle(PlayerbotGroup::MeleeChaseAngle(
                    UsesFrontPosition(target->GetVictim() == bot, PlayerbotRoles::IsTank(*bot)))));
        TC_LOG_INFO("module.playerbots", "PB-MOVE: %s requested %s toward %s", bot->GetName().c_str(), name.c_str(), target->GetName().c_str());
        return true; // Chase submission is not confirmed arrival.
    }
private:
    Step step;
};
class RangeAction final : public Action
{
public:
    explicit RangeAction(PlayerbotAI* ai) : Action(ai, "range") { }
    bool Execute(Event event) override
    {
        Player* bot = botAI ? botAI->GetBot() : nullptr;
        Player* requester = bot && !event.getOwnerGuid().IsEmpty() ? bot->GetMap()->GetPlayer(event.getOwnerGuid()) : nullptr;
        AiObjectContext* context = botAI ? botAI->GetAiObjectContext() : nullptr;
        if (!bot || bot->IsBeingTeleported() || !requester || requester->IsBeingTeleported() || !context ||
            !PlayerbotSecurity(*bot).CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, *requester))
            return false;
        RangeCommand command = ParseRangeCommand(event.getParam());
        if (command.Operation == RangeOperation::Invalid) return false;
        auto report = [&](std::string const& type)
        {
            Value<float>* range = context->GetValue<float>("range", type);
            if (!range) return false;
            ChatHandler(requester->GetSession()).PSendSysMessage("Playerbot %s: %s",
                bot->GetName().c_str(), FormatRange(type, range->Get()).c_str());
            return true;
        };
        if (command.Operation == RangeOperation::QueryAll)
            return report("spell") && report("heal");
        Value<float>* range = context->GetValue<float>("range", command.Type);
        if (!range) return false;
        if (command.Operation == RangeOperation::Set)
        {
            bool changed = command.Type == "spell" && ResolveRange("spell", range->Get()) != ResolveRange("spell", command.Value);
            range->Set(command.Value);
            if (changed) botAI->RequestSpellChaseRefresh();
        }
        return report(command.Type);
    }
};
}
bool PlayerbotCombatMovement::CanMove(Player const& bot)
{
    MotionMaster const* motion = bot.GetMotionMaster();
    return CanMove(ControlState{bot.IsInWorld(), bot.IsAlive(), bot.IsBeingTeleported(),
        bot.IsInFlight() || motion->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE,
        bot.GetVehicle() != nullptr,
        bot.HasUnitState(UNIT_STATE_CANNOT_TURN | UNIT_STATE_NOT_MOVE),
        bot.IsCharmed(), bot.isFrozen(), bot.IsPolymorphed(),
        motion->GetMotionSlot(MOTION_SLOT_CONTROLLED) != nullptr}); // Cata uses MAX_MOTION_TYPE for an empty slot.
}
bool PlayerbotCombatMovement::RefreshSpellChase(PlayerbotAI& ai)
{
    if (!Ready(&ai, Step::ReachSpell, true)) return false;
    Player* bot = ai.GetBot();
    if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != CHASE_MOTION_TYPE) return false;
    bot->GetMotionMaster()->MoveChase(ai.GetCurrentTarget(), GetRange(ai, "spell"));
    return true; // Submission, not arrival; no target acquisition or new movement owner.
}
bool PlayerbotCombatMovement::FaceForAttack(Player& bot, Creature& target)
{
    if (!bot.IsInWorld() || !target.IsInWorld() || !bot.IsAlive() || !target.IsAlive() ||
        bot.GetMap() != target.GetMap() || bot.IsBeingTeleported() || target.IsControlledByPlayer() ||
        PlayerbotTargetSelection::IsProtectedTarget(target) ||
        !bot.IsValidAttackTarget(&target) || !CanMove(bot) || bot.IsMounted() ||
        bot.IsNonMeleeSpellCast(false))
        return false;
    constexpr float arc = 2.0f * float(M_PI) / 3.0f;
    if (!bot.HasInArc(arc, &target)) bot.SetFacingToObject(&target);
    return bot.HasInArc(arc, &target); // Native refuses unfinished movement splines; never force.
}
float PlayerbotCombatMovement::GetRange(PlayerbotAI& ai, std::string const& type)
{
    if (type != "spell" && type != "heal") return 0.0f;
    AiObjectContext* context = ai.GetAiObjectContext();
    Value<float>* range = context ? context->GetValue<float>("range", type) : nullptr;
    return ResolveRange(type, range ? range->Get() : 0.0f);
}
void PlayerbotCombatMovement::AddRangeContexts(SharedNamedObjectContextList<Action>& actions,
    SharedNamedObjectContextList<UntypedValue>& values)
{
    auto* value = new NamedObjectContext<UntypedValue>();
    value->creators["range"] = [](PlayerbotAI* ai) { return new RangeValue(ai); };
    values.Add(value);
    auto* action = new NamedObjectContext<Action>();
    action->creators["range"] = [](PlayerbotAI* ai) { return new RangeAction(ai); };
    actions.Add(action);
}
void PlayerbotCombatMovement::AddContexts(SharedNamedObjectContextList<Action>& actions,
    SharedNamedObjectContextList<Trigger>& triggers, SharedNamedObjectContextList<UntypedValue>& values)
{
    AddRangeContexts(actions, values);
    auto* action = new NamedObjectContext<Action>();
    action->creators["set facing"] = [](PlayerbotAI* ai) { return new MovementAction(ai, "set facing", Step::Facing); };
    action->creators["reach melee"] = [](PlayerbotAI* ai) { return new MovementAction(ai, "reach melee", Step::Reach); };
    action->creators["reach spell"] = [](PlayerbotAI* ai) { return new MovementAction(ai, "reach spell", Step::ReachSpell); };
    action->creators["set behind"] = [](PlayerbotAI* ai) { return new MovementAction(ai, "set behind", Step::Behind); };
    actions.Add(action);
    auto* trigger = new NamedObjectContext<Trigger>();
    trigger->creators["not facing target"] = [](PlayerbotAI* ai) { return new MovementTrigger(ai, "not facing target", Step::Facing); };
    trigger->creators["enemy out of melee"] = [](PlayerbotAI* ai) { return new MovementTrigger(ai, "enemy out of melee", Step::Reach); };
    trigger->creators["enemy out of spell"] = [](PlayerbotAI* ai) { return new MovementTrigger(ai, "enemy out of spell", Step::ReachSpell); };
    trigger->creators["not behind target"] = [](PlayerbotAI* ai) { return new MovementTrigger(ai, "not behind target", Step::Behind); };
    triggers.Add(trigger);
}
