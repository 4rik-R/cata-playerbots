/*
 * Cata adaptation of mod-playerbots UseFoodStrategy / UseItemAction at
 * 7bae1b5c58c76a0aa20381155edc08096d1485b2. See PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "PlayerbotRestStrategy.h"
#include "PlayerbotRestItem.h"
#include "PlayerbotTargetSelection.h"
#include "../../Bot/PlayerbotAI.h"
#include "../../Bot/Engine/Value/Value.h"
#include "../../Script/PlayerbotConfig.h"
#include "Bag.h"
#include "Item.h"
#include "Log.h"
#include "MotionMaster.h"
#include "Player.h"
#include "SpellCastRequest.h"
#include "SpellInfo.h"
#include "SpellHistory.h"
#include "SpellMgr.h"

namespace
{
bool Ready(PlayerbotAI* ai)
{
    Player* bot = ai ? ai->GetBot() : nullptr;
    Player* owner = ai ? ai->GetController() : nullptr;
    return bot && owner && !ai->LootRequests().Pending() && !ai->LootPursuit().Active() && PlayerbotRest::CanRest(PlayerbotModuleRestEnabled(), bot->IsAlive(),
        bot->IsInCombat(), bot->IsBeingTeleported(), bot->IsMounted() || bot->IsInFlight(),
        PlayerbotTargetSelection::HasNearbyPartyCombat(*bot, *owner)) &&
        owner->IsAlive() && bot->IsInWorld() && owner->IsInWorld() && !owner->IsBeingTeleported() &&
        bot->GetMap() == owner->GetMap() && bot->IsWithinDistInMap(owner, 25.0f) && !bot->IsNonMeleeSpellCast(false);
}
bool Needed(Player& bot, PlayerbotRest::Kind kind)
{
    int32 maximum = bot.GetMaxPower(POWER_MANA);
    float mana = maximum > 0 ? 100.0f * bot.GetPower(POWER_MANA) / maximum : 100.0f;
    return PlayerbotRest::Needs(kind, bot.GetHealthPct(), mana,
        bot.GetPowerType() == POWER_MANA && maximum > 0);
}
SpellInfo const* RestSpell(Item const& item, PlayerbotRest::Kind kind)
{
    ItemTemplate const* proto = item.GetTemplate();
    if (!proto || !PlayerbotRest::FoodItem(proto->GetClass(), proto->GetSubClass()))
        return nullptr;
    for (ItemEffect const& effect : proto->Effects)
        if (effect.SpellID && effect.Trigger == ITEM_SPELLTRIGGER_ON_USE)
        {
            SpellInfo const* spell = sSpellMgr->GetSpellInfo(effect.SpellID);
            // Native CastItemUseSpell executes the first valid on-use effect.
            if (!spell)
                continue;
            uint32 category = PlayerbotRest::ItemCategory(effect.Category, spell->GetCategory());
            bool food = category == SPELL_CATEGORY_FOOD &&
                (spell->HasAura(SPELL_AURA_MOD_REGEN) || spell->HasAura(SPELL_AURA_OBS_MOD_HEALTH));
            bool drink = category == SPELL_CATEGORY_DRINK &&
                (spell->HasAura(SPELL_AURA_MOD_POWER_REGEN) || spell->HasAura(SPELL_AURA_OBS_MOD_POWER));
            return (kind == PlayerbotRest::Kind::Food ? food : drink) ? spell : nullptr;
        }
    return nullptr;
}
// First usable food or drink in the backpack, then the bags.
ObjectGuid FindRestItem(Player& bot, PlayerbotRest::Kind kind)
{
    auto eligible = [&](Item* item)
    {
        SpellInfo const* spell = item ? RestSpell(*item, kind) : nullptr;
        return spell && bot.CanUseItem(item) == EQUIP_ERR_OK && !bot.GetSpellHistory()->HasCooldown(spell);
    };
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (Item* item = bot.GetItemByPos(INVENTORY_SLOT_BAG_0, slot); eligible(item))
            return item->GetGUID();
    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        if (Bag* bag = bot.GetBagByPos(bagSlot))
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                if (Item* item = bag->GetItemByPos(slot); eligible(item))
                    return item->GetGUID();
    return ObjectGuid::Empty;
}

// Native item use of the food or drink; true once its aura is on the bot.
bool StartRest(PlayerbotAI& ai, ObjectGuid itemGuid, PlayerbotRest::Kind kind, std::string const& name)
{
    Player* bot = ai.GetBot();
    Item* item = bot ? bot->GetItemByGuid(itemGuid) : nullptr;
    SpellInfo const* spell = item ? RestSpell(*item, kind) : nullptr;
    if (!item || !spell || !Player::IsInventoryPos(item->GetBagSlot(), item->GetSlot()) ||
        bot->GetUseableItemByPos(item->GetBagSlot(), item->GetSlot()) != item ||
        bot->CanUseItem(item) != EQUIP_ERR_OK || bot->GetSpellHistory()->HasCooldown(spell) ||
        !bot->CanRequestSpellCast(spell) || bot->HasUnitState(UNIT_STATE_CASTING) ||
        bot->GetSpellHistory()->GetRemainingGlobalCooldown(spell) > 0)
        return false;
    // Typed native request: no packet layouts, direct restoration or manual consumption.
    WorldPackets::Spells::SpellCastRequest request;
    request.SpellID = spell->Id;
    request.Target.Flags = TARGET_FLAG_UNIT;
    request.Target.Unit = bot->GetGUID();
    auto pending = std::make_unique<PendingSpellCastRequest>(std::move(request),
        SpellCastRequestItemData(item->GetBagSlot(), item->GetSlot(), item->GetGUID()));
    bot->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
    bot->GetMotionMaster()->Clear(MOTION_SLOT_IDLE);
    bot->GetMotionMaster()->MoveIdle();
    bot->StopMoving();
    ai.BeginRest(spell->Id, kind == PlayerbotRest::Kind::Drink);
    bot->RequestSpellCast(std::move(pending), spell);
    // Submission is not native cast success. Update clears failed/absent auras.
    bool started = bot->HasAura(spell->Id);
    if (started)
        TC_LOG_INFO("module.playerbots", "PB-REST: %s began %s using spell %u", bot->GetName().c_str(),
            name.c_str(), spell->Id);
    return started;
}

class InventoryRestItemValue final : public CalculatedValue<ObjectGuid>
{
public:
    InventoryRestItemValue(PlayerbotAI* ai, PlayerbotRest::Kind kind)
        : CalculatedValue<ObjectGuid>(ai, "inventory items", 2), kind(kind) { }
private:
    ObjectGuid Calculate() override
    {
        Player* bot = botAI ? botAI->GetBot() : nullptr;
        if (!bot || !Ready(botAI) || !Needed(*bot, kind))
            return ObjectGuid::Empty;
        return FindRestItem(*bot, kind);
    }
    PlayerbotRest::Kind kind;
};
class RestTrigger final : public Trigger
{
public:
    RestTrigger(PlayerbotAI* ai, char const* name, PlayerbotRest::Kind kind)
        : Trigger(ai, name, 2), kind(kind) { }
    bool IsActive() override
    {
        Player* bot = botAI ? botAI->GetBot() : nullptr;
        return bot && Ready(botAI) && !botAI->GetRestSpellId() && Needed(*bot, kind);
    }
private:
    PlayerbotRest::Kind kind;
};
class RestAction final : public Action
{
public:
    RestAction(PlayerbotAI* ai, char const* name, PlayerbotRest::Kind kind)
        : Action(ai, name), kind(kind), candidate(ai, kind) { }
    bool isUseful() override
    {
        Player* bot = botAI ? botAI->GetBot() : nullptr;
        return bot && Ready(botAI) && !botAI->GetRestSpellId() && Needed(*bot, kind) && !candidate.Get().IsEmpty();
    }
    bool Execute([[maybe_unused]] Event event) override
    {
        if (!isUseful())
            return false;
        ObjectGuid item = candidate.Get();
        candidate.Reset();
        return StartRest(*botAI, item, kind, name);
    }
private:
    PlayerbotRest::Kind kind;
    InventoryRestItemValue candidate;
};
}

void PlayerbotRest::AddContexts(SharedNamedObjectContextList<Strategy>& strategies,
    SharedNamedObjectContextList<Action>& actions, SharedNamedObjectContextList<Trigger>& triggers)
{
    auto* strategy = new NamedObjectContext<Strategy>();
    strategy->creators["food"] = [](PlayerbotAI* ai) { return new FoodStrategy(ai); };
    strategies.Add(strategy);
    auto* action = new NamedObjectContext<Action>();
    action->creators["food"] = [](PlayerbotAI* ai) { return new RestAction(ai, "food", Kind::Food); };
    action->creators["drink"] = [](PlayerbotAI* ai) { return new RestAction(ai, "drink", Kind::Drink); };
    actions.Add(action);
    auto* trigger = new NamedObjectContext<Trigger>();
    trigger->creators["low health"] = [](PlayerbotAI* ai) { return new RestTrigger(ai, "low health", Kind::Food); };
    trigger->creators["low mana"] = [](PlayerbotAI* ai) { return new RestTrigger(ai, "low mana", Kind::Drink); };
    triggers.Add(trigger);
}

bool PlayerbotRest::Update(PlayerbotAI& ai, bool interrupt)
{
    uint32 spellId = ai.GetRestSpellId();
    if (!spellId)
        return false;
    Player* bot = ai.GetBot();
    Player* owner = ai.GetController();
    SpellInfo const* spell = sSpellMgr->GetSpellInfo(spellId);
    int32 maximum = bot ? bot->GetMaxPower(POWER_MANA) : 0;
    float mana = maximum > 0 ? 100.0f * bot->GetPower(POWER_MANA) / maximum : 100.0f;
    bool finished = !bot || !owner || !spell || ai.RestFinished(bot->GetHealthPct(), mana);
    if (interrupt || !Ready(&ai) || finished || !bot->HasAura(spellId))
    {
        if (bot)
        {
            bot->RemoveAurasDueToSpell(spellId);
            if (bot->IsAlive() && bot->GetStandState() == UNIT_STAND_STATE_SIT)
                bot->SetStandState(UNIT_STAND_STATE_STAND);
        }
        ai.ClearRest();
        return false;
    }
    return true;
}

bool PlayerbotRest::Begin(PlayerbotAI& ai, Kind kind)
{
    Player* bot = ai.GetBot();
    if (!bot || ai.GetRestSpellId() || !Ready(&ai))
        return false;
    ObjectGuid item = FindRestItem(*bot, kind);
    return !item.IsEmpty() && StartRest(ai, item, kind, kind == Kind::Drink ? "drink" : "food");
}
