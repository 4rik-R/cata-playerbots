/*
 * Adapted from AzerothCore mod-playerbots generic non-combat strategies at
 * 8827dd6fcbb2bb25988787a40f06fc93daf8e02d. See AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "RotationAiObjectContext.h"
#include "PlayerbotClassRotation.h"
#include "../../Base/PlayerbotRestStrategy.h"
#include "../../Base/PlayerbotPotionStrategy.h"
#include "../../Base/PlayerbotCombatValues.h"
#include "../../Base/PlayerbotCombatMovement.h"
#include "../../Base/PlayerbotPosition.h"
#include "../../Base/PlayerbotThreatStrategy.h"
#include "../../Base/PlayerbotCorpseLoot.h"
#include "../../../Bot/PlayerbotAI.h"
#include "Player.h"

namespace
{
class UpkeepStrategy final : public Strategy
{
public:
    explicit UpkeepStrategy(PlayerbotAI* ai) : Strategy(ai) { }
    std::string const getName() override { return "nc"; }
    uint32_t GetType() const override { return STRATEGY_TYPE_NONCOMBAT; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override
    {
        triggers.push_back(new TriggerNode("class upkeep", {NextAction("class upkeep", ACTION_NORMAL)}));
    }
};

class UpkeepTrigger final : public Trigger
{
public:
    explicit UpkeepTrigger(PlayerbotAI* ai) : Trigger(ai, "class upkeep", 2) { }
    bool IsActive() override
    {
        Player* bot = botAI ? botAI->GetBot() : nullptr;
        return bot && PlayerbotClassRotation::NeedsUpkeep(*bot);
    }
    bool IsBuffTrigger() override { return true; }
};

class UpkeepAction final : public Action
{
public:
    explicit UpkeepAction(PlayerbotAI* ai) : Action(ai, "class upkeep") { }
    bool isUseful() override
    {
        Player* bot = botAI ? botAI->GetBot() : nullptr;
        return bot && PlayerbotClassRotation::NeedsUpkeep(*bot);
    }
    bool Execute([[maybe_unused]] Event event) override
    {
        Player* bot = botAI ? botAI->GetBot() : nullptr;
        return bot && PlayerbotClassRotation::Upkeep(*bot);
    }
};

struct SharedRotationContexts
{
    SharedNamedObjectContextList<Strategy> strategies;
    SharedNamedObjectContextList<Action> actions;
    SharedNamedObjectContextList<Trigger> triggers;
    SharedNamedObjectContextList<UntypedValue> values;

    SharedRotationContexts()
    {
        PlayerbotCombatValues::AddContexts(values);
        PlayerbotCombatMovement::AddContexts(actions, triggers, values);
        PlayerbotPosition::AddContexts(strategies, actions, triggers, values);
        PlayerbotThreat::AddContexts(strategies);
        PlayerbotRest::AddContexts(strategies, actions, triggers);
        PlayerbotPotion::AddContexts(strategies, actions, triggers);
        PlayerbotCorpseLoot::AddContexts(strategies, actions, triggers);
        auto* strategyFactory = new NamedObjectContext<Strategy>();
        strategyFactory->creators["nc"] = [](PlayerbotAI* ai) { return new UpkeepStrategy(ai); };
        strategies.Add(strategyFactory);
        auto* actionFactory = new NamedObjectContext<Action>();
        actionFactory->creators["class upkeep"] = [](PlayerbotAI* ai) { return new UpkeepAction(ai); };
        actions.Add(actionFactory);
        auto* triggerFactory = new NamedObjectContext<Trigger>();
        triggerFactory->creators["class upkeep"] = [](PlayerbotAI* ai) { return new UpkeepTrigger(ai); };
        triggers.Add(triggerFactory);
    }
};

SharedRotationContexts& Shared()
{
    static SharedRotationContexts contexts;
    return contexts;
}
}

RotationAiObjectContext::RotationAiObjectContext(PlayerbotAI* botAI)
    : AiObjectContext(botAI, Shared().strategies, Shared().actions, Shared().triggers, Shared().values) { }
