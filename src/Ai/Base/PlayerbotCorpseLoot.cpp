/*
 * Adapted from mod-playerbots LootAction / OpenLootAction / StoreLootAction at
 * 7bae1b5c58c76a0aa20381155edc08096d1485b2. See PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "PlayerbotCorpseLoot.h"
#include "PlayerbotQuestItem.h"
#include "PlayerbotTargetSelection.h"
#include "../../Bot/PlayerbotAI.h"
#include "../../Bot/Engine/Value/Value.h"
#include "../../Script/PlayerbotConfig.h"
#include "../../Bot/Random/RandomPlayerbotChat.h"
#include "PlayerbotSecurity.h"
#include "Creature.h"
#include "Loot.h"
#include "LootPackets.h"
#include "Log.h"
#include "MotionMaster.h"
#include "PlayerbotCombatMovement.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Timer.h"
#include "WorldSession.h"

namespace
{
bool Ready(Player* bot, Player* owner)
{
    return PlayerbotModuleCorpseLootEnabled() && bot && owner && bot->IsAlive() && owner->IsAlive() &&
        bot->IsInWorld() && owner->IsInWorld() && bot->GetMap() == owner->GetMap() &&
        !PlayerbotTargetSelection::HasNearbyPartyCombat(*bot, *owner) && !bot->IsBeingTeleported() &&
        !owner->IsBeingTeleported() && !bot->IsMounted() && !bot->IsInFlight() &&
        !bot->IsNonMeleeSpellCast(false) && bot->IsWithinDistInMap(owner, 25.0f);
}
bool EligibleCorpse(Player& bot, Creature* corpse)
{
    return corpse && !corpse->IsAlive() && !corpse->IsControlledByPlayer() &&
        corpse->HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE) &&
        (corpse->GetLootRecipientGUID() == bot.GetGUID() ||
            (corpse->GetLootRecipientGroup() && corpse->GetLootRecipientGroup() == bot.GetGroup())) &&
        corpse->loot.loot_type != LOOT_SKINNING &&
        bot.IsWithinLOSInMap(corpse);
}
bool NearbyCorpse(Player& bot, Creature* corpse)
{
    return EligibleCorpse(bot, corpse) && bot.IsWithinDistInMap(corpse, INTERACTION_DISTANCE);
}
bool ReachableCorpse(Player& bot, Player& owner, Creature* corpse)
{
    // Small detour only; native pathfinding owns the actual route. No teleport.
    return EligibleCorpse(bot, corpse) && bot.IsWithinDistInMap(corpse, 15.0f) &&
        owner.IsWithinDistInMap(corpse, 20.0f);
}
ObjectGuid Candidate(PlayerbotAI* ai, bool nearby = true)
{
    Player* bot = ai ? ai->GetBot() : nullptr;
    Player* owner = ai ? ai->GetController() : nullptr;
    if (ai->IsStaying() || !Ready(bot, owner) || ai->GetRestSpellId() || ai->LootRequests().Pending())
        return ObjectGuid::Empty;
    uint32 now = getMSTime();
    Unit* selected = owner->GetSelectedUnit();
    if (Creature* corpse = selected ? selected->ToCreature() : nullptr)
        if (ReachableCorpse(*bot, *owner, corpse))
            ai->LootCandidates().Add(corpse->GetGUID(), now);
    ObjectGuid result;
    float nearest = 100.0f;
    ai->LootCandidates().Visit(now, [&](ObjectGuid guid)
    {
        Creature* corpse = ObjectAccessor::GetCreature(*bot, guid);
        if (!ReachableCorpse(*bot, *owner, corpse) || (nearby && !NearbyCorpse(*bot, corpse)) ||
            !ai->LootAttempts().CanAttempt(guid, now))
            return;
        float distance = bot->GetDistance(corpse);
        if (distance < nearest)
        {
            nearest = distance;
            result = guid;
        }
    });
    return result;
}
class CandidateValue final : public CalculatedValue<ObjectGuid>
{
public:
    explicit CandidateValue(PlayerbotAI* ai) : CalculatedValue<ObjectGuid>(ai, "loot target", 2) { }
private:
    ObjectGuid Calculate() override { return Candidate(botAI); }
};
class CanLootTrigger final : public Trigger
{
public:
    explicit CanLootTrigger(PlayerbotAI* ai) : Trigger(ai, "can loot", 2), candidate(ai) { }
    bool IsActive() override { return !candidate.Get().IsEmpty(); }
private:
    CandidateValue candidate;
};
class OpenLootAction final : public Action
{
public:
    explicit OpenLootAction(PlayerbotAI* ai) : Action(ai, "open loot") { }
    bool isUseful() override { return !Candidate(botAI).IsEmpty(); }
    bool Execute([[maybe_unused]] Event event) override
    {
        ObjectGuid corpse = Candidate(botAI);
        if (corpse.IsEmpty())
            return false;
        Player* bot = botAI->GetBot();
        Player* owner = botAI->GetController();
        if (!botAI->LootRequests().Submit(corpse, owner->GetGUID()))
            return false;
        botAI->LootAttempts().Record(corpse, getMSTime());
        bot->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
        bot->GetMotionMaster()->Clear(MOTION_SLOT_IDLE);
        bot->GetMotionMaster()->MoveIdle();
        bot->StopMoving();
        return true; // request accepted, not an item-award receipt
    }
};
class FarLootTrigger final : public Trigger
{
public:
    explicit FarLootTrigger(PlayerbotAI* ai) : Trigger(ai, "far from loot target", 2) { }
    bool IsActive() override
    {
        if (botAI->LootPursuit().Active())
            return false;
        ObjectGuid guid = Candidate(botAI, false);
        Player* bot = botAI->GetBot();
        return !guid.IsEmpty() && bot && !NearbyCorpse(*bot, ObjectAccessor::GetCreature(*bot, guid));
    }
};
class MoveLootAction final : public Action
{
public:
    explicit MoveLootAction(PlayerbotAI* ai) : Action(ai, "move to loot") { }
    bool Execute([[maybe_unused]] Event event) override
    {
        ObjectGuid guid = Candidate(botAI, false);
        Player* bot = botAI->GetBot();
        Creature* corpse = bot && !guid.IsEmpty() ? ObjectAccessor::GetCreature(*bot, guid) : nullptr;
        if (!corpse || !PlayerbotCombatMovement::CanMove(*bot) ||
            NearbyCorpse(*bot, corpse) || !botAI->LootPursuit().Begin(guid, getMSTime()))
            return false;
        bot->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
        bot->GetMotionMaster()->Clear(MOTION_SLOT_IDLE);
        bot->GetMotionMaster()->MovePoint(0, corpse->GetPosition(), true);
        return true;
    }
};
}
void PlayerbotCorpseLoot::AddContexts(SharedNamedObjectContextList<Strategy>& strategies,
    SharedNamedObjectContextList<Action>& actions, SharedNamedObjectContextList<Trigger>& triggers)
{
    auto* strategyFactory = new NamedObjectContext<Strategy>();
    strategyFactory->creators["loot"] = [](PlayerbotAI* ai) { return new LootStrategy(ai); };
    strategies.Add(strategyFactory);
    auto* actionFactory = new NamedObjectContext<Action>();
    actionFactory->creators["open loot"] = [](PlayerbotAI* ai) { return new OpenLootAction(ai); };
    actions.Add(actionFactory);
    actionFactory->creators["move to loot"] = [](PlayerbotAI* ai) { return new MoveLootAction(ai); };
    auto* triggerFactory = new NamedObjectContext<Trigger>();
    triggerFactory->creators["can loot"] = [](PlayerbotAI* ai) { return new CanLootTrigger(ai); };
    triggers.Add(triggerFactory);
    triggerFactory->creators["far from loot target"] = [](PlayerbotAI* ai) { return new FarLootTrigger(ai); };
}
bool PlayerbotCorpseLoot::UpdateMovement(PlayerbotAI& ai, bool interrupt, bool& ended)
{
    ended = false;
    auto& pursuit = ai.LootPursuit();
    if (!pursuit.Active())
        return false;
    Player* bot = ai.GetBot();
    Player* owner = ai.GetController();
    Creature* corpse = bot ? ObjectAccessor::GetCreature(*bot, pursuit.Corpse()) : nullptr;
    bool timeout = pursuit.Expired(getMSTime());
    if (interrupt || !Ready(bot, owner) || !PlayerbotCombatMovement::CanMove(*bot) ||
        ai.GetRestSpellId() || timeout ||
        !ReachableCorpse(*bot, *owner, corpse) || NearbyCorpse(*bot, corpse) || ai.LootRequests().Pending())
    {
        if (timeout)
            ai.LootAttempts().Record(pursuit.Corpse(), getMSTime());
        pursuit.End();
        ended = true;
        if (bot)
        {
            bot->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
            bot->GetMotionMaster()->MoveIdle();
            bot->StopMoving();
        }
        return false;
    }
    return true;
}
void PlayerbotCorpseLoot::ProcessWorld(WorldSession& session, Mailbox& mailbox, ObjectGuid currentController)
{
    auto request = mailbox.Take();
    if (!request)
        return;
    // All calls below run in the native thread-unsafe session update context.
    Player* bot = session.GetPlayer();
    Player* owner = ObjectAccessor::FindConnectedPlayer(request->Controller);
    if (session.IsServerOrigin() && currentController == request->Controller && Ready(bot, owner) &&
        bot->GetLootGUID().IsEmpty() && PlayerbotSecurity(*bot).CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, *owner) &&
        NearbyCorpse(*bot, ObjectAccessor::GetCreature(*bot, request->Corpse)))
    {
        WorldPackets::Loot::LootResponse view;
        bot->SendLoot(request->Corpse, LOOT_CORPSE, &view);
        // Preserve HandleLootOpcode's post-open effects as well as SendLoot's
        // permission logic; this typed call does not dispatch a client opcode.
        if (bot->IsNonMeleeSpellCast(false))
            bot->InterruptNonMeleeSpells(false);
        bot->RemoveAurasWithInterruptFlags(SpellAuraInterruptFlags::Looting);
        if (bot->GetLootGUID() == request->Corpse)
        {
            TC_LOG_INFO("module.playerbots", "PB-LOOT: %s opened corpse %u through native loot permissions",
                bot->GetName().c_str(), request->Corpse.GetCounter());
            if (view.Owner == request->Corpse && view.AcquireReason == LOOT_CORPSE)
            {
                if (view.Coins)
                {
                    WorldPacket money(CMSG_LOOT_MONEY, 0);
                    session.HandleLootMoneyOpcode(money);
                }
                // Only the native allowed/owner view, never blocked or master slots.
                // One attempt per visible item; native inventory errors do not spin.
                for (auto const& item : view.Items)
                {
                    if (item.UIType != LOOT_SLOT_TYPE_ALLOW_LOOT && item.UIType != LOOT_SLOT_TYPE_OWNER)
                        continue;
                    if (bot->GetLootGUID() != request->Corpse)
                        break;
                    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(item.Loot.ItemID);
                    bool perPlayerCopy = false;
                    if (PlayerbotModuleQuestLootPriorityEnabled() && proto && proto->GetClass() == ITEM_CLASS_QUEST)
                    {
                        // Resolve this viewer's native slot, including quest-slot mapping.
                        // freeforall is per-player consumption, not the group loot method.
                        Creature* corpse = ObjectAccessor::GetCreature(*bot, request->Corpse);
                        LootItem const* nativeItem = corpse ? corpse->loot.LootItemInSlot(item.LootListID, bot) : nullptr;
                        perPlayerCopy = nativeItem && nativeItem->itemid == item.Loot.ItemID && nativeItem->freeforall;
                    }
                    if (PlayerbotModuleQuestLootPriorityEnabled() && proto && proto->GetClass() == ITEM_CLASS_QUEST &&
                        owner->GetSession() && !owner->GetSession()->IsServerOrigin() &&
                        PlayerbotQuestItem::DeferCorpseQuestItem(true, true, owner->HasQuestForItem(item.Loot.ItemID), perPlayerCopy))
                    {
                        TC_LOG_INFO("module.playerbots", "PB-QUEST-LOOT: %s deferred quest item %u needed by %s; native ownership unchanged",
                            bot->GetName().c_str(), item.Loot.ItemID, owner->GetName().c_str());
                        continue;
                    }
                    WorldPacket store(CMSG_AUTOSTORE_LOOT_ITEM, 1);
                    store << uint8(item.LootListID);
                    session.HandleAutostoreLootItemOpcode(store);
                    if (proto && proto->GetQuality() >= ITEM_QUALITY_RARE && bot->HasItemCount(item.Loot.ItemID, 1, true))
                        RandomPlayerbotChat::Loot(*bot, item.Loot.ItemID);
                }
            }
            if (bot->GetLootGUID() == request->Corpse)
                session.DoLootRelease(request->Corpse);
        }
    }
    mailbox.Finish(request->Generation);
}
