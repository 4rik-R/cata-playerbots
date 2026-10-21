/*
 * Adapted from mod-playerbots UseFoodStrategy at
 * 7bae1b5c58c76a0aa20381155edc08096d1485b2. See AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef PLAYERBOT_REST_STRATEGY_H
#define PLAYERBOT_REST_STRATEGY_H
#include "../../Bot/Engine/Strategy/Strategy.h"
#include "../../Bot/Engine/NamedObjectContext.h"
#include <cstdint>

class PlayerbotAI;
class UntypedValue;
namespace PlayerbotRest
{
enum class Kind { Food, Drink };
inline constexpr float LowHealth = 40.0f;
inline constexpr float LowMana = 20.0f;
inline constexpr float RestPriority = 3.0f;
inline bool Needs(Kind kind, float health, float mana, bool usesMana)
{
    return kind == Kind::Food ? health > 0.0f && health < LowHealth : usesMana && mana < LowMana;
}
inline bool CanRest(bool enabled, bool alive, bool combat, bool transferring, bool mounted, bool partyCombat)
{
    return enabled && alive && !combat && !transferring && !mounted && !partyCombat;
}
class FoodStrategy final : public Strategy
{
public:
    explicit FoodStrategy(PlayerbotAI* ai) : Strategy(ai) { }
    std::string const getName() override { return "food"; }
    uint32_t GetType() const override { return STRATEGY_TYPE_NONCOMBAT; }
    void InitTriggers(std::vector<TriggerNode*>& nodes) override
    {
        nodes.push_back(new TriggerNode("low health", { NextAction("food", RestPriority) }));
        nodes.push_back(new TriggerNode("low mana", { NextAction("drink", RestPriority) }));
    }
};
void AddContexts(SharedNamedObjectContextList<Strategy>& strategies,
    SharedNamedObjectContextList<Action>& actions, SharedNamedObjectContextList<Trigger>& triggers);
// Map-thread only: eats or drinks now, whatever the usual thresholds, when the bot may rest. For dungeon breaks.
bool Begin(PlayerbotAI& ai, Kind kind);
// Map-thread only. Cancels only the aura recorded by this rest action.
bool Update(PlayerbotAI& ai, bool interrupt);
}
#endif
