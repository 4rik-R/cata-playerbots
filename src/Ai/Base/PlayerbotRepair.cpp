/* Adapted from RepairAllAction at 79bd428115c8f33b74b13de23b133d49c3f75c60.
 * GPL v2 or later. Native Cata world-owner execution; see PORTING.md. */
#include "PlayerbotRepair.h"
#include "../../Script/PlayerbotConfig.h"
#include "PlayerbotSecurity.h"
#include "Chat.h"
#include "Creature.h"
#include "Item.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Player.h"
#include "Timer.h"
#include "WorldPacket.h"
#include "WorldSession.h"
namespace
{
uint64 EquippedDamage(Player const& player)
{
    uint64 missing = 0;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (Item* item = player.GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            uint32 maximum = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
            uint32 current = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
            if (maximum > current) missing += maximum - current;
        }
    return missing;
}
}
void PlayerbotRepair::Update(WorldSession& session, Mailbox& mailbox)
{
    auto request = mailbox.Take();
    if (!request) return;
    Player* bot = session.GetPlayer();
    Player* human = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(request->Requester));
    bool attached = session.GetServerOriginFollowTargetGuidLow() == request->Requester ||
        session.GetServerOriginPartyControllerGuidLow() == request->Requester;
    bool eligible = PlayerbotModuleRepairEnabled() && Fresh(*request, getMSTime()) && session.IsServerOrigin() &&
        !session.PlayerLoading() && !session.isLogingOut() && bot && bot->IsInWorld() && bot->IsAlive() &&
        !bot->IsBeingTeleported() && !bot->IsInCombat() && !bot->IsInFlight() && !bot->IsNonMeleeSpellCast(false) &&
        attached && human && human->GetSession() && !human->GetSession()->IsServerOrigin() && human->IsInWorld() &&
        !human->IsBeingTeleported() && bot->GetMapId() == request->Map && bot->GetInstanceId() == request->Instance &&
        human->GetMap() == bot->GetMap() && PlayerbotSecurity(*bot).CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, *human);
    ObjectGuid npc(request->Npc);
    Creature* repairer = eligible && npc.IsCreature() ? bot->GetNPCIfCanInteractWith(npc, UNIT_NPC_FLAG_REPAIR) : nullptr;
    if (repairer && human->GetNPCIfCanInteractWith(npc, UNIT_NPC_FLAG_REPAIR))
    {
        uint64 before = EquippedDamage(*bot);
        uint64 money = bot->GetMoney();
        bot->SetFacingToObject(repairer);
        WorldPacket packet(CMSG_REPAIR_ITEM, 17);
        packet << npc << ObjectGuid::Empty << uint8(0); // All carried items, personal funds only.
        session.HandleRepairItemOpcode(packet);
        // Handler is synchronous. Re-resolve identity before reporting its observed effects.
        bot = session.GetPlayer();
        human = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(request->Requester));
        if (bot && human && human->GetSession())
            ChatHandler(human->GetSession()).PSendSysMessage(
                "Playerbot %s: native repair processed; equipped missing durability %llu -> %llu, spent %llu copper. Remaining damage may require more funds.",
                bot->GetName().c_str(), static_cast<unsigned long long>(before),
                static_cast<unsigned long long>(EquippedDamage(*bot)),
                static_cast<unsigned long long>(money >= bot->GetMoney() ? money - bot->GetMoney() : 0));
    }
    else if (human && human->GetSession())
        ChatHandler(human->GetSession()).SendSysMessage("Playerbot repair rejected: request expired, control/state changed or selected repairer is not nearby and eligible for both players.");
    mailbox.Finish(request->Serial);
}
