/*
 * This file is part of the Cataclysm Playerbots port. See AUTHORS.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "PlayerbotControl.h"
#include "PlayerbotControlChat.h"
#include "../Bot/Cmd/PlayerbotStrategyControl.h"
#include "PlayerbotRoster.h"
#include "../Ai/Base/PlayerbotCombatMovement.h"
#include "../Ai/Base/PlayerbotEquipmentInspection.h"
#include "../Ai/Base/PlayerbotQuestAccept.h"
#include "../Bot/Cmd/PlayerbotQuestChat.h"
#include "PlayerbotSecurity.h"
#include "PlayerbotSessionHooks.h"
#include "PlayerbotConfig.h"
#include "Timer.h"
#include "Chat.h"
#include "Group.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "WorldSession.h"
#include "World.h"

namespace
{
void RequestRepair(Player& sender, ObjectGuid botGuid)
{
    ChatHandler reply(sender.GetSession());
    WorldSession* session = sWorld->FindServerOriginPlayerbot(botGuid);
    Player* bot = session ? session->GetPlayer() : nullptr;
    ObjectGuid npc = sender.GetTarget();
    uint32 requester = sender.GetGUID().GetCounter();
    if (!PlayerbotModuleRepairEnabled()) { reply.SendSysMessage("Playerbot repair is disabled."); return; }
    if (!bot || !bot->IsInWorld() || !sender.IsInWorld() || sender.GetMap() != bot->GetMap() ||
        !npc.IsCreature() || !PlayerbotSecurity(*bot).CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, sender) ||
        (session->GetServerOriginFollowTargetGuidLow() != requester && session->GetServerOriginPartyControllerGuidLow() != requester))
    { reply.SendSysMessage("Select a nearby repair NPC with your controlled bot on the same map/instance."); return; }
    reply.SendSysMessage(session->RequestPlayerbotRepair(requester, npc.GetRawValue(), sender.GetMapId(), sender.GetInstanceId()) ?
        "Repair requested using the bot's own money; wait for the native result." : "Repair request busy or disabled.");
}
void RequestQuestCommand(Player& sender, ObjectGuid botGuid, uint32 quest,
    PlayerbotQuestAccept::Operation action = PlayerbotQuestAccept::Operation::Accept, uint32 item = 0)
{
    ChatHandler reply(sender.GetSession());
    bool inspect = PlayerbotQuestAccept::IsInspection(action);
    bool share = action == PlayerbotQuestAccept::Operation::Share;
    bool abandon = action == PlayerbotQuestAccept::Operation::Abandon;
    bool acceptAll = action == PlayerbotQuestAccept::Operation::AcceptAll;
    bool rewardAll = action == PlayerbotQuestAccept::Operation::RewardAll;
    if (!inspect && !acceptAll && !rewardAll && !quest) { reply.SendSysMessage("Select a nearby quest giver and use accept <quest ID/link> or accept *."); return; }
    bool reward = action == PlayerbotQuestAccept::Operation::Reward || rewardAll;
    if (!(abandon ? PlayerbotModuleQuestAbandonEnabled() : share ? PlayerbotModuleQuestSendShareEnabled() : inspect ? PlayerbotModuleQuestInspectionEnabled() : reward ? PlayerbotModuleQuestRewardEnabled() : PlayerbotModuleQuestAcceptEnabled()))
    { reply.SendSysMessage("That native quest operation is disabled."); return; }
    ObjectGuid giver = (inspect || share || abandon) ? ObjectGuid::Empty : sender.GetTarget();
    if (!inspect && !share && !abandon && !giver.IsCreature() && !giver.IsGameObject())
    { reply.SendSysMessage("Select a nearby creature/gameobject quest giver first."); return; }
    WorldSession* session = sWorld->FindServerOriginPlayerbot(botGuid);
    Player* bot = session ? session->GetPlayer() : nullptr;
    if (!bot || !bot->IsInWorld() || bot->GetGUID() != botGuid)
    { reply.SendSysMessage("That Playerbot is unavailable."); return; }
    if (!sender.IsInWorld() || sender.GetMap() != bot->GetMap() ||
        !PlayerbotSecurity(*bot).CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, sender))
    { reply.SendSysMessage("Quest acceptance requires control and the same map/instance."); return; }
    uint32 requester = sender.GetGUID().GetCounter();
    if (session->GetServerOriginFollowTargetGuidLow() != requester &&
        session->GetServerOriginPartyControllerGuidLow() != requester)
    { reply.SendSysMessage("That Playerbot must be attached to you before accepting quests."); return; }
    reply.SendSysMessage(session->RequestPlayerbotQuestCommand(requester, quest, giver.GetRawValue(),
        sender.GetMapId(), sender.GetInstanceId(), uint32(action), item) ?
        "Quest operation requested; wait for the native completion reply." : "Quest request busy or disabled.");
}
void RequestGearApply(Player& sender, ObjectGuid botGuid)
{
    ChatHandler reply(sender.GetSession());
    WorldSession* session = sWorld->FindServerOriginPlayerbot(botGuid);
    Player* bot = session ? session->GetPlayer() : nullptr;
    if (!bot || !bot->IsInWorld() || bot->GetGUID() != botGuid)
    { reply.SendSysMessage("That Playerbot is unavailable."); return; }
    if (!PlayerbotSecurity(*bot).CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, sender))
    { reply.SendSysMessage("You do not control that Playerbot."); return; }
    if (!PlayerbotModuleStarterEquipEnabled() || !PlayerbotModuleStarterGearScoreEnabled())
    { reply.SendSysMessage("Native starter gear changes are disabled."); return; }
    if (session->GetServerOriginFollowTargetGuidLow() != sender.GetGUID().GetCounter() &&
        session->GetServerOriginPartyControllerGuidLow() != sender.GetGUID().GetCounter())
    { reply.SendSysMessage("That Playerbot must be attached to you before applying gear."); return; }
    reply.SendSysMessage(session->RequestPlayerbotEquip(sender.GetGUID().GetCounter()) ?
        "One gear change requested; wait for the map-thread completion reply." : "A gear request is already pending or the gate changed.");
}
void ReportGear(Player& sender, ObjectGuid botGuid)
{
    ChatHandler reply(sender.GetSession());
    WorldSession* session = sWorld->FindServerOriginPlayerbot(botGuid);
    Player* bot = session ? session->GetPlayer() : nullptr;
    if (!bot || !bot->IsInWorld() || bot->GetGUID() != botGuid)
    { reply.SendSysMessage("That Playerbot is unavailable."); return; }
    if (!PlayerbotSecurity(*bot).CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, sender))
    { reply.SendSysMessage("You do not control that Playerbot."); return; }
    bool enabled = PlayerbotModuleStarterGearScoreEnabled();
    if (!enabled)
    { reply.SendSysMessage("Read-only starter gear inspection is disabled."); return; }
    auto snapshot = session->GetPlayerbotStrategySnapshot();
    if (!snapshot || snapshot->Bot != botGuid.GetCounter() || !bot->IsAlive() || bot->IsBeingTeleported() ||
        !PlayerbotEquipmentInspection::Fresh(enabled, snapshot->EquipmentEnabled, getMSTime(), snapshot->EquipmentCreated))
    { reply.SendSysMessage("The gear inspection snapshot is not ready; try again shortly."); return; }
    if (!snapshot->EquipmentAvailable)
    { reply.PSendSysMessage("Playerbot %s: starter gear survey unavailable for this level/spec/state (supported levels 10-39).", bot->GetName().c_str()); return; }
    reply.PSendSysMessage("Playerbot %s: read-only gear survey, %u slot alternatives; showing %u. No items changed.",
        bot->GetName().c_str(), snapshot->EquipmentTotal, uint32(snapshot->EquipmentRows.size()));
    for (auto const& row : snapshot->EquipmentRows) reply.SendSysMessage(row.c_str());
}
class PlayerbotChatCommands final : public PlayerScript
{
public:
    PlayerbotChatCommands() : PlayerScript("PlayerbotChatCommands") { }

    void OnChat(Player* sender, uint32 type, uint32 lang, std::string& message, Player* receiver) override
    {
        if (type != CHAT_MSG_WHISPER || lang == LANG_ADDON || !sender || !receiver ||
            !sender->GetSession() || sender->GetSession()->IsServerOrigin() ||
            !receiver->GetSession() || !receiver->GetSession()->IsServerOrigin())
            return;

        std::string command = NormalizePlayerbotControlChat(message);
        ChatHandler reply(sender->GetSession());
        if (command == "repair") { RequestRepair(*sender, receiver->GetGUID()); return; }
        if (auto inspection = PlayerbotQuestAccept::ParseInspection(command))
        { RequestQuestCommand(*sender, receiver->GetGUID(), 0, *inspection); return; }
        if (PlayerbotQuestAccept::RecognizesDrop(command))
        {
            auto args = PlayerbotQuestChat::Parse(message);
            if (!args) reply.SendSysMessage("Whisper one controlled bot: drop <active quest ID or quest link>. Native abandonment can remove quest-provided items.");
            else RequestQuestCommand(*sender, receiver->GetGUID(), args->Quest, args->Action);
            return;
        }
        if (PlayerbotQuestAccept::RecognizesShare(command))
        {
            auto args = PlayerbotQuestChat::Parse(message);
            if (!args) reply.SendSysMessage("Whisper one controlled bot: share <quest ID or quest link>.");
            else RequestQuestCommand(*sender, receiver->GetGUID(), args->Quest, args->Action);
            return;
        }
        if (IsPlayerbotGearInspection(command)) { ReportGear(*sender, receiver->GetGUID()); return; }
        if (IsPlayerbotGearApply(command)) { RequestGearApply(*sender, receiver->GetGUID()); return; }
        if (PlayerbotQuestAccept::Recognizes(command))
        {
            auto args = PlayerbotQuestChat::Parse(message);
            if (!args) reply.SendSysMessage("Select a nearby quest giver and use accept <quest ID or quest link>.");
            else RequestQuestCommand(*sender, receiver->GetGUID(), args->Quest, args->Action);
            return;
        }
        if (PlayerbotQuestAccept::RecognizesReward(command))
        {
            auto args = PlayerbotQuestChat::Parse(message);
            if (!args) reply.SendSysMessage("Select the quest giver and use reward <quest ID> <item ID>; item 0 only for no choices.");
            else RequestQuestCommand(*sender, receiver->GetGUID(), args->Quest, args->Action, args->Item);
            return;
        }
        if (command == "list")
        {
            std::vector<PlayerbotRosterEntry> roster = PlayerbotRoster::ListActiveFor(*sender);
            if (roster.empty())
                reply.SendSysMessage("No controllable Playerbots are currently online.");
            for (PlayerbotRosterEntry const& entry : roster)
                reply.PSendSysMessage("Playerbot: %s (level %u, class %u).", entry.Name.c_str(),
                    uint32(entry.Level), uint32(entry.Class));
            return;
        }

        PlayerbotControlCommand action = PlayerbotControlCommand::Cease;
        std::string rangeParam;
        bool range = ExtractPlayerbotRangeChat(command, rangeParam);
        bool strategy = PlayerbotStrategyControl::Recognizes(command);
        if (!range && !strategy && !ParsePlayerbotControlChat(command, action))
            return;

        switch (strategy ? PlayerbotControl::DispatchStrategy(*sender, receiver->GetGUID(), command) :
            range ? PlayerbotControl::DispatchRange(*sender, receiver->GetGUID(), rangeParam) :
            PlayerbotControl::Dispatch(*sender, receiver->GetGUID(), action))
        {
            case PlayerbotControlResult::Queued:
                reply.PSendSysMessage("Playerbot %s: %s requested.", receiver->GetName().c_str(), command.c_str());
                break;
            case PlayerbotControlResult::Unauthorized:
                reply.SendSysMessage("You do not control that Playerbot.");
                break;
            case PlayerbotControlResult::NotFollowing:
                reply.SendSysMessage(strategy ? "That Playerbot must be attached to you before using strategy controls." :
                    "That Playerbot must follow you before it can attack your target.");
                break;
            case PlayerbotControlResult::BotUnavailable:
                reply.SendSysMessage("That Playerbot is unavailable.");
                break;
            case PlayerbotControlResult::Busy:
                reply.SendSysMessage("That Playerbot already has a request queued; try again shortly.");
                break;
            case PlayerbotControlResult::InvalidCommand:
                reply.SendSysMessage(strategy ? "Strategies: co|nc|de ?; nc +food,-loot,? (only food/loot support +, -, ~)." :
                    "Range: range ?, range spell|heal ?, or range spell|heal <yards>. Use 0 to reset; spell 2-25, heal 2-30.");
                break;
        }
    }

    void OnChat(Player* sender, uint32 type, uint32 lang, std::string& message, Group* group) override
    {
        bool partyChat = type == CHAT_MSG_PARTY || type == CHAT_MSG_PARTY_LEADER;
        bool raidChat = type == CHAT_MSG_RAID || type == CHAT_MSG_RAID_LEADER;
        if ((!partyChat && !raidChat) || lang == LANG_ADDON || !sender || !group ||
            group->isBGGroup() || !group->IsMember(sender->GetGUID()) || !sender->IsInWorld() ||
            !sender->GetSession() || sender->GetSession()->IsServerOrigin())
            return;

        std::string command = NormalizePlayerbotControlChat(message);
        PlayerbotControlCommand action = PlayerbotControlCommand::Cease;
        std::string rangeParam;
        bool range = ExtractPlayerbotRangeChat(command, rangeParam);
        bool strategy = PlayerbotStrategyControl::Recognizes(command);
        bool gear = IsPlayerbotGearInspection(command);
        bool repair = command == "repair";
        bool applyGear = IsPlayerbotGearApply(command);
        bool acceptQuest = PlayerbotQuestAccept::Recognizes(command);
        auto inspectQuests = PlayerbotQuestAccept::ParseInspection(command);
        bool rewardQuest = PlayerbotQuestAccept::RecognizesReward(command);
        auto rewardArgs = rewardQuest ? PlayerbotQuestChat::Parse(message) : std::nullopt;
        if (rewardQuest && !rewardArgs)
        { ChatHandler(sender->GetSession()).SendSysMessage("Select the quest giver: reward <quest ID> <item ID>, or reward * for zero/single-choice quests. Item 0 only for no choices."); return; }
        auto acceptArgs = acceptQuest ? PlayerbotQuestChat::Parse(message) : std::nullopt;
        uint32 quest = acceptArgs ? acceptArgs->Quest : 0;
        if (acceptQuest && !acceptArgs)
        { ChatHandler(sender->GetSession()).SendSysMessage("Select a nearby quest giver and use accept <numeric quest ID>."); return; }
        if (!repair && !inspectQuests && !rewardQuest && !acceptQuest && !applyGear && !gear && !range && !strategy && !ParsePlayerbotControlChat(command, action))
            return;
        if (strategy && !PlayerbotStrategyControl::Parse(command))
        {
            ChatHandler(sender->GetSession()).SendSysMessage("Strategies: co|nc|de ?; nc +food,-loot,? (only food/loot support +, -, ~).");
            return;
        }
        if (range && PlayerbotCombatMovement::ParseRangeCommand(rangeParam).Operation == PlayerbotCombatMovement::RangeOperation::Invalid)
        {
            ChatHandler(sender->GetSession()).SendSysMessage("Range: range ?, range spell|heal ?, or range spell|heal <yards>. Use 0 to reset; spell 2-25, heal 2-30.");
            return;
        }
        uint32 queued = 0;
        uint32 rejected = 0;
        for (auto const& entry : PlayerbotRoster::ListActiveFor(*sender))
        {
            WorldSession* session = sWorld->FindServerOriginPlayerbot(entry.Guid);
            Player* bot = session ? session->GetPlayer() : nullptr;
            if (!bot || bot->GetGroup() != group ||
                !PlayerbotControlChatReaches(raidChat, group->GetMemberGroup(sender->GetGUID()),
                    group->GetMemberGroup(entry.Guid)))
                continue;
            // Dispatch independently rechecks native identity and full control.
            if (repair) { RequestRepair(*sender, entry.Guid); continue; }
            if (gear) { ReportGear(*sender, entry.Guid); continue; }
            if (applyGear) { RequestGearApply(*sender, entry.Guid); continue; }
            if (acceptQuest) { RequestQuestCommand(*sender, entry.Guid, quest, acceptArgs->Action); continue; }
            if (inspectQuests) { RequestQuestCommand(*sender, entry.Guid, 0, *inspectQuests); continue; }
            if (rewardQuest) { RequestQuestCommand(*sender, entry.Guid, rewardArgs->Quest, rewardArgs->Action, rewardArgs->Item); continue; }
            if ((strategy ? PlayerbotControl::DispatchStrategy(*sender, entry.Guid, command) :
                range ? PlayerbotControl::DispatchRange(*sender, entry.Guid, rangeParam) :
                PlayerbotControl::Dispatch(*sender, entry.Guid, action)) == PlayerbotControlResult::Queued)
                ++queued;
            else
                ++rejected;
        }
        if (queued || rejected)
            ChatHandler(sender->GetSession()).PSendSysMessage("Playerbots: %s requested for %u bot(s); %u rejected.",
                command.c_str(), queued, rejected);
        // Preserve normal chat delivery, including to human group members.
    }
};
}

void AddSC_playerbot_chat_commands()
{
    new PlayerbotChatCommands();
}
