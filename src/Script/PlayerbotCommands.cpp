/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information.
 * Released under GNU GPL v2 or any later version.
 */
#include "PlayerbotModuleCommands.h"
#include "PlayerbotManagedRoster.h"
#include "PlayerbotDevFixture.h"
#include "Player.h"
#include "RandomPlayerbotFactory.h"
#include "RandomPlayerbotMgr.h"
#include "CharacterCache.h"
#include "Chat.h"
#include "RBAC.h"
#include "World.h"
#include "WorldSession.h"
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <limits>
#include <sstream>
namespace
{
class PlayerbotDevCommands
{
public:
    static std::vector<ChatCommand> GetCommands()
    {
        static std::vector<ChatCommand> devPlayerbotCommandTable =
        {
            { "start",  rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotStartCommand,  "" },
            { "stop",   rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotStopCommand,   "" },
            { "follow", rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotFollowCommand, "" },
            { "hold",   rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotHoldCommand,   "" },
            { "attack", rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotAttackCommand, "" },
            { "cease",  rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotCeaseCommand,  "" },
            { "joininstance", rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotJoinInstanceCommand, "" },
            { "status", rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotStatusCommand, "" },
            { "start2", rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotStart2Command, "" },
            { "stop2", rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotStop2Command, "" },
            { "follow2", rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotFollow2Command, "" },
            { "hold2", rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotHold2Command, "" },
            { "attack2", rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotAttack2Command, "" },
            { "cease2", rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotCease2Command, "" },
            { "joininstance2", rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotJoinInstance2Command, "" },
            { "status2", rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotStatus2Command, "" },
            { "slot", rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleDevPlayerbotSlotCommand, "" },
            { "managed", rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleManagedPlayerbotCommand, "" },
            { "random", rbac::RBAC_PERM_COMMAND_SERVER_DEBUG, true, &HandleRandomPlayerbotCommand, "" },
        };

        return devPlayerbotCommandTable;
    }
    static bool HandleDevPlayerbotStartCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (handler->GetSession())
        {
            handler->SendSysMessage("PB-00 controls are console-only during the lifecycle proof.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sWorld->TryStartDevPlayerbot())
        {
            handler->SendSysMessage("PB-00 start rejected; check the feature flag, dedicated account/supported-class GUID, admission rules, and shutdown state.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->SendSysMessage("PB-00 session admitted; character loading is asynchronous. Use .server playerbotdev status to check progress.");
        return true;
    }

    static bool HandleDevPlayerbotStopCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (handler->GetSession())
        {
            handler->SendSysMessage("PB-00 controls are console-only during the lifecycle proof.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sWorld->RequestStopDevPlayerbot())
        {
            handler->SendSysMessage("No PB-00 session is active.");
            return true;
        }

        handler->SendSysMessage("PB-00 exit requested; save/logout will run in the next safe world-session update.");
        return true;
    }

    static bool HandleDevPlayerbotFollowCommand(ChatHandler* handler, char const* args)
    {
        if (handler->GetSession())
        {
            handler->SendSysMessage("PB-01 controls are console-only.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        char* end = nullptr;
        errno = 0;
        unsigned long guidLow = args ? std::strtoul(args, &end, 10) : 0;
        if (!args || !*args || !end || *end || errno || !guidLow || guidLow > std::numeric_limits<uint32>::max())
        {
            handler->SendSysMessage("Usage: server playerbotdev follow <player character GUID>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 accountId = sWorld->getIntConfig(CONFIG_PLAYERBOTS_DEV_ACCOUNT_ID);
        WorldSession* bot = accountId ? sWorld->FindSession(accountId) : nullptr;
        if (!bot || !bot->IsServerOrigin() || !bot->GetPlayer())
        {
            handler->SendSysMessage("PB-01 bot is not in world; start it first.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        bot->RequestServerOriginFollow(uint32(guidLow));
        handler->PSendSysMessage("PB-01 follow requested for character GUID %u; target must be alive on the bot's map.", uint32(guidLow));
        return true;
    }

    static bool HandleDevPlayerbotHoldCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (handler->GetSession())
        {
            handler->SendSysMessage("PB-01 controls are console-only.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 accountId = sWorld->getIntConfig(CONFIG_PLAYERBOTS_DEV_ACCOUNT_ID);
        WorldSession* bot = accountId ? sWorld->FindSession(accountId) : nullptr;
        if (!bot || !bot->IsServerOrigin() || !bot->GetPlayer())
        {
            handler->SendSysMessage("No in-world PB-01 bot is active.");
            return true;
        }

        bot->RequestServerOriginHold();
        handler->SendSysMessage("PB-01 hold requested.");
        return true;
    }

    static bool HandleDevPlayerbotAttackCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (handler->GetSession())
        {
            handler->SendSysMessage("PB-02 controls are console-only.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 accountId = sWorld->getIntConfig(CONFIG_PLAYERBOTS_DEV_ACCOUNT_ID);
        WorldSession* bot = accountId ? sWorld->FindSession(accountId) : nullptr;
        if (!bot || !bot->IsServerOrigin() || !bot->GetPlayer() || !bot->GetServerOriginFollowTargetGuidLow())
        {
            handler->SendSysMessage("PB-02 attack requires an in-world bot following a human owner.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        bot->RequestServerOriginAttack();
        handler->SendSysMessage("PB-02 attack requested for the owner's selected nearby hostile creature.");
        return true;
    }

    static bool HandleDevPlayerbotCeaseCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (handler->GetSession())
        {
            handler->SendSysMessage("PB-02 controls are console-only.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 accountId = sWorld->getIntConfig(CONFIG_PLAYERBOTS_DEV_ACCOUNT_ID);
        WorldSession* bot = accountId ? sWorld->FindSession(accountId) : nullptr;
        if (!bot || !bot->IsServerOrigin() || !bot->GetPlayer())
        {
            handler->SendSysMessage("No in-world PB-02 bot is active.");
            return true;
        }

        bot->RequestServerOriginCease();
        handler->SendSysMessage("PB-02 cease-fire requested; follow will resume if the owner remains on-map.");
        return true;
    }

    static bool HandleDevPlayerbotJoinInstanceCommand(ChatHandler* handler, char const* args)
    {
        if (handler->GetSession())
        {
            handler->SendSysMessage("PB-PARTY dungeon transfer is console-only.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        char* end = nullptr;
        errno = 0;
        unsigned long mapId = args ? std::strtoul(args, &end, 10) : 0;
        if (!args || !*args || !end || *end || errno || !mapId || mapId > std::numeric_limits<uint32>::max())
        {
            handler->SendSysMessage("Usage: server playerbotdev joininstance <dungeon map ID>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 accountId = sWorld->getIntConfig(CONFIG_PLAYERBOTS_DEV_ACCOUNT_ID);
        WorldSession* bot = accountId ? sWorld->FindSession(accountId) : nullptr;
        if (!bot || !bot->IsServerOrigin() || !bot->GetPlayer())
        {
            handler->SendSysMessage("PB-PARTY dungeon transfer requires an in-world bot.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        bot->RequestServerOriginInstanceJoin(uint32(mapId));
        handler->PSendSysMessage("PB-PARTY requested dungeon map %u; the bot will validate its party and core entry rights.", uint32(mapId));
        return true;
    }

    static bool HandleDevPlayerbotStatusCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (handler->GetSession())
        {
            handler->SendSysMessage("PB-00 controls are console-only during the lifecycle proof.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!sWorld->getBoolConfig(CONFIG_PLAYERBOTS_DEV_ENABLED))
        {
            handler->SendSysMessage("PB-00 is disabled in configuration.");
            return true;
        }

        uint32 accountId = sWorld->getIntConfig(CONFIG_PLAYERBOTS_DEV_ACCOUNT_ID);
        WorldSession* session = accountId ? sWorld->FindSession(accountId) : nullptr;
        if (!session || !session->IsServerOrigin())
        {
            handler->SendSysMessage("No PB-00 session is active.");
            return true;
        }

        char const* state = "created";
        switch (session->GetInitializationState())
        {
            case WorldSessionInitializationState::Created: break;
            case WorldSessionInitializationState::Loading: state = "account loading"; break;
            case WorldSessionInitializationState::Ready: state = session->GetPlayer() ? "character in world" : "character loading"; break;
            case WorldSessionInitializationState::Failed: state = "initialization failed"; break;
        }

        handler->PSendSysMessage("PB-00 session: %s%s.", state, session->PlayerLoading() ? " (login query pending)" : "");
        if (uint32 ownerGuid = session->GetServerOriginFollowTargetGuidLow())
            handler->PSendSysMessage("PB-01 following character GUID %u.", ownerGuid);
        else
            handler->SendSysMessage("PB-01 holding.");
        handler->PSendSysMessage("PB-02 %s.", session->IsServerOriginAttacking() ? "attacking" : "not attacking");
        return true;
    }

    static WorldSession* GetDevPlayerbot2()
    {
        uint32 accountId = sWorld->getIntConfig(CONFIG_PLAYERBOTS_DEV_ACCOUNT_ID_2);
        uint32 configuredGuid = sWorld->getIntConfig(CONFIG_PLAYERBOTS_DEV_CHARACTER_GUID_2);
        WorldSession* session = accountId ? sWorld->FindSession(accountId) : nullptr;
        return session && session->IsServerOrigin() && session->GetServerOriginCharacterGuid().GetCounter() == configuredGuid ? session : nullptr;
    }

    static bool CheckDevPlayerbot2Console(ChatHandler* handler)
    {
        if (!handler->GetSession())
            return true;

        handler->SendSysMessage("Playerbot development controls are console-only.");
        handler->SetSentErrorMessage(true);
        return false;
    }

    static bool ParseDevPlayerbot2Id(ChatHandler* handler, char const* args, char const* usage, uint32& id)
    {
        char* end = nullptr;
        errno = 0;
        unsigned long parsed = args ? std::strtoul(args, &end, 10) : 0;
        if (!args || !*args || !end || *end || errno || !parsed || parsed > std::numeric_limits<uint32>::max())
        {
            handler->SendSysMessage(usage);
            handler->SetSentErrorMessage(true);
            return false;
        }

        id = uint32(parsed);
        return true;
    }

    static bool HandleDevPlayerbotStart2Command(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckDevPlayerbot2Console(handler))
            return false;

        if (!sWorld->TryStartDevPlayerbot(true))
        {
            handler->SendSysMessage("Second bot start rejected; check feature flag, distinct dedicated account/supported-class GUID, admission rules, and shutdown state.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->SendSysMessage("Second bot session admitted; character loading is asynchronous. Use .server playerbotdev status2.");
        return true;
    }

    static bool HandleDevPlayerbotStop2Command(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckDevPlayerbot2Console(handler))
            return false;

        if (sWorld->RequestStopDevPlayerbot(true))
            handler->SendSysMessage("Second bot exit requested; save/logout will run in the next safe world-session update.");
        else
            handler->SendSysMessage("No second bot session is active.");
        return true;
    }

    static bool HandleDevPlayerbotFollow2Command(ChatHandler* handler, char const* args)
    {
        if (!CheckDevPlayerbot2Console(handler))
            return false;

        uint32 ownerGuid;
        if (!ParseDevPlayerbot2Id(handler, args, "Usage: server playerbotdev follow2 <player character GUID>", ownerGuid))
            return false;

        WorldSession* bot = GetDevPlayerbot2();
        if (!bot || !bot->GetPlayer())
        {
            handler->SendSysMessage("Second bot is not in world; start it first.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        bot->RequestServerOriginFollow(ownerGuid);
        handler->PSendSysMessage("Second bot follow requested for character GUID %u; target must be alive on the bot's map.", ownerGuid);
        return true;
    }

    static bool HandleDevPlayerbotHold2Command(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckDevPlayerbot2Console(handler))
            return false;

        WorldSession* bot = GetDevPlayerbot2();
        if (bot && bot->GetPlayer())
        {
            bot->RequestServerOriginHold();
            handler->SendSysMessage("Second bot hold requested.");
        }
        else
            handler->SendSysMessage("No in-world second bot is active.");
        return true;
    }

    static bool HandleDevPlayerbotAttack2Command(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckDevPlayerbot2Console(handler))
            return false;

        WorldSession* bot = GetDevPlayerbot2();
        if (!bot || !bot->GetPlayer() || !bot->GetServerOriginFollowTargetGuidLow())
        {
            handler->SendSysMessage("Second bot attack requires an in-world bot following a human owner.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        bot->RequestServerOriginAttack();
        handler->SendSysMessage("Second bot attack requested for the owner's selected nearby hostile creature.");
        return true;
    }

    static bool HandleDevPlayerbotCease2Command(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckDevPlayerbot2Console(handler))
            return false;

        WorldSession* bot = GetDevPlayerbot2();
        if (bot && bot->GetPlayer())
        {
            bot->RequestServerOriginCease();
            handler->SendSysMessage("Second bot cease-fire requested; follow will resume if owner remains on-map.");
        }
        else
            handler->SendSysMessage("No in-world second bot is active.");
        return true;
    }

    static bool HandleDevPlayerbotJoinInstance2Command(ChatHandler* handler, char const* args)
    {
        if (!CheckDevPlayerbot2Console(handler))
            return false;

        uint32 mapId;
        if (!ParseDevPlayerbot2Id(handler, args, "Usage: server playerbotdev joininstance2 <dungeon map ID>", mapId))
            return false;

        WorldSession* bot = GetDevPlayerbot2();
        if (!bot || !bot->GetPlayer())
        {
            handler->SendSysMessage("Second bot dungeon transfer requires an in-world bot.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        bot->RequestServerOriginInstanceJoin(mapId);
        handler->PSendSysMessage("Second bot requested dungeon map %u; party and entry rights will be validated.", mapId);
        return true;
    }

    static bool HandleDevPlayerbotStatus2Command(ChatHandler* handler, char const* /*args*/)
    {
        if (!CheckDevPlayerbot2Console(handler))
            return false;

        if (!sWorld->getBoolConfig(CONFIG_PLAYERBOTS_DEV_ENABLED))
        {
            handler->SendSysMessage("Playerbot development is disabled in configuration.");
            return true;
        }

        uint32 accountId = sWorld->getIntConfig(CONFIG_PLAYERBOTS_DEV_ACCOUNT_ID_2);
        uint32 configuredGuid = sWorld->getIntConfig(CONFIG_PLAYERBOTS_DEV_CHARACTER_GUID_2);
        if (!accountId || !configuredGuid)
        {
            handler->SendSysMessage("Second bot is not configured (AccountId2 and CharacterGuid2 required).");
            return true;
        }

        WorldSession* session = GetDevPlayerbot2();
        if (!session)
        {
            handler->SendSysMessage("No second bot session is active.");
            return true;
        }

        char const* state = "created";
        switch (session->GetInitializationState())
        {
            case WorldSessionInitializationState::Created: break;
            case WorldSessionInitializationState::Loading: state = "account loading"; break;
            case WorldSessionInitializationState::Ready: state = session->GetPlayer() ? "character in world" : "character loading"; break;
            case WorldSessionInitializationState::Failed: state = "initialization failed"; break;
        }

        handler->PSendSysMessage("Second bot session: %s%s.", state, session->PlayerLoading() ? " (login query pending)" : "");
        if (uint32 ownerGuid = session->GetServerOriginFollowTargetGuidLow())
            handler->PSendSysMessage("Second bot following character GUID %u.", ownerGuid);
        else
            handler->SendSysMessage("Second bot holding.");
        handler->PSendSysMessage("Second bot %s.", session->IsServerOriginAttacking() ? "attacking" : "not attacking");
        return true;
    }

    // Compact multi-bot control for the full-party development replay. The
    // original one- and two-bot commands remain available for older scripts.
    static bool HandleDevPlayerbotSlotCommand(ChatHandler* handler, char const* args)
    {
        if (!CheckDevPlayerbot2Console(handler))
            return false;

        std::istringstream input(args ? args : "");
        uint32 slot = 0;
        std::string action;
        if (!(input >> slot >> action) || slot < 1 || slot > 4)
        {
            handler->SendSysMessage("Usage: server playerbotdev slot <1-4> <start|stop|status|follow|hold|attack|cease|joininstance|fixture20> [character GUID or dungeon map ID]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        bool needsId = action == "follow" || action == "joininstance";
        uint32 id = 0;
        if (needsId && (!(input >> id) || !id))
        {
            handler->SendSysMessage("This slot action requires a nonzero character GUID or dungeon map ID.");
            handler->SetSentErrorMessage(true);
            return false;
        }
        input >> std::ws;
        if (!input.eof())
        {
            handler->SendSysMessage("Unexpected extra slot-command argument.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint8 slotId = uint8(slot);
        if (action == "start")
        {
            if (!sWorld->TryStartDevPlayerbotSlot(slotId))
            {
                handler->PSendSysMessage("Playerbot slot %u start rejected; check distinct allowlisted account, supported-class GUID, and admission rules.", slot);
                handler->SetSentErrorMessage(true);
                return false;
            }
            handler->PSendSysMessage("Playerbot slot %u admitted; character loading is asynchronous.", slot);
            return true;
        }
        if (action == "stop")
        {
            handler->PSendSysMessage("Playerbot slot %u %s.", slot,
                sWorld->RequestStopDevPlayerbotSlot(slotId) ? "exit requested" : "is not active");
            return true;
        }

        WorldSession* bot = sWorld->FindDevPlayerbotSlot(slotId);
        if (action == "fixture20")
        {
            auto role = slot == 1 ? PlayerbotDevFixture::Role::Protection : slot == 2 ? PlayerbotDevFixture::Role::Arms :
                slot == 3 ? PlayerbotDevFixture::Role::Frost : PlayerbotDevFixture::Role::Holy;
            bool accepted = bot && bot->GetPlayer() && PlayerbotDevFixture::Request(bot->GetPlayer()->GetGUID(), role);
            handler->PSendSysMessage("Playerbot slot %u fixture20 %s; completion is reported by PB-FIXTURE on the map thread.",
                slot, accepted ? "requested" : "rejected (requires explicit fixture flag and ready slot)");
            return accepted;
        }
        if (action == "status")
        {
            if (!bot)
                handler->PSendSysMessage("Playerbot slot %u is not active.", slot);
            else
                handler->PSendSysMessage("Playerbot slot %u: %s; character %s; follow GUID %u; %s.", slot,
                    bot->PlayerLoading() ? "loading" : "ready", bot->GetPlayer() ? "in world" : "not in world",
                    bot->GetServerOriginFollowTargetGuidLow(), bot->IsServerOriginAttacking() ? "attacking" : "not attacking");
            return true;
        }
        if (!bot || !bot->GetPlayer())
        {
            handler->PSendSysMessage("Playerbot slot %u requires an in-world bot.", slot);
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (action == "follow")
            bot->RequestServerOriginFollow(id);
        else if (action == "hold")
            bot->RequestServerOriginHold();
        else if (action == "attack")
        {
            if (!bot->GetServerOriginFollowTargetGuidLow())
            {
                handler->SendSysMessage("Attack requires a followed human owner.");
                handler->SetSentErrorMessage(true);
                return false;
            }
            bot->RequestServerOriginAttack();
        }
        else if (action == "cease")
            bot->RequestServerOriginCease();
        else if (action == "joininstance")
            bot->RequestServerOriginInstanceJoin(id);
        else
        {
            handler->SendSysMessage("Unknown slot action.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Playerbot slot %u %s requested.", slot, action.c_str());
        return true;
    }

    static bool HandleRandomPlayerbotCommand(ChatHandler* handler, char const* args)
    {
        if (handler->GetSession())
        {
            handler->SendSysMessage("Random Playerbot controls are console-only.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::istringstream input(args ? args : "");
        std::string action, extra;
        input >> action >> extra;
        if (!extra.empty() || (action != "status" && action != "start" && action != "stop"))
        {
            handler->SendSysMessage("Usage: server playerbotdev random <status|start|stop>");
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (action != "status")
            RandomPlayerbotMgr::SetPaused(action == "stop");
        handler->SendSysMessage(RandomPlayerbotMgr::Describe().c_str());
        return true;
    }

    static bool HandleManagedPlayerbotCommand(ChatHandler* handler, char const* args)
    {
        if (handler->GetSession())
        {
            handler->SendSysMessage("Managed Playerbot controls are console-only.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::istringstream input(args ? args : "");
        std::string action;
        std::string guidText;
        std::string extra;
        input >> action >> guidText;
        if (action == "enroll" || action == "provision" || action == "factory-status")
        {
            uint32 accountId = 0;
            auto parseNumber = [](std::string const& text, uint32& value)
            {
                auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
                return !text.empty() && error == std::errc{} && end == text.data() + text.size();
            };
            bool valid = parseNumber(guidText, accountId) && accountId;
            std::string name, raceText, classText, genderText;
            uint32 race = 0, classId = 0, gender = 0;
            if (action == "enroll")
            {
                input >> name >> raceText >> classText >> genderText;
                valid = valid && !name.empty() && parseNumber(raceText, race) && race <= 255 &&
                    parseNumber(classText, classId) && classId <= 255 && parseNumber(genderText, gender) && gender <= 1;
            }
            input >> extra;
            if (!valid || !extra.empty())
            {
                handler->SendSysMessage("Usage: managed enroll <dedicated account ID> <name> <race ID> <class ID> <gender 0|1> | provision <account ID> | factory-status <account ID>");
                handler->SetSentErrorMessage(true);
                return false;
            }
            if (action == "factory-status")
            {
                handler->PSendSysMessage("Factory status: %s.", RandomPlayerbotFactory::DescribeAttempt(accountId).c_str());
                return true;
            }
            std::string detail;
            bool accepted = action == "enroll" ? RandomPlayerbotFactory::EnrollAccount(
                {accountId, name, uint8(race), uint8(classId), uint8(gender)}, detail) :
                RandomPlayerbotFactory::ProvisionAccount(accountId, detail);
            handler->PSendSysMessage("Factory %s: %s.", accepted ? "request accepted" : "request rejected", detail.c_str());
            if (!accepted)
                handler->SetSentErrorMessage(true);
            return accepted;
        }
        if (action == "inspect")
        {
            input >> extra;
            uint32 accountId = 0;
            auto [end, error] = std::from_chars(guidText.data(), guidText.data() + guidText.size(), accountId);
            if (guidText.empty() || !extra.empty() || error != std::errc{} ||
                end != guidText.data() + guidText.size() || !accountId)
            {
                handler->SendSysMessage("Usage: server playerbotdev managed inspect <dedicated account ID>");
                handler->SetSentErrorMessage(true);
                return false;
            }
            std::string detail;
            auto decision = RandomPlayerbotFactory::InspectOwnedAccount(accountId, detail);
            char const* label = decision == PlayerbotFactoryDecision::Create ? "create draft" :
                decision == PlayerbotFactoryDecision::Reuse ? "reuse draft" : "rejected";
            handler->PSendSysMessage("Read-only factory inspection: %s; %s. No writes or admission performed.", label, detail.c_str());
            return true;
        }
        if (action == "appearance")
        {
            std::string classText, genderText;
            input >> classText >> genderText >> extra;
            auto parseByte = [](std::string const& text, uint8& value)
            {
                uint32 parsed = 0;
                auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
                if (text.empty() || error != std::errc{} || end != text.data() + text.size() || parsed > 255)
                    return false;
                value = uint8(parsed);
                return true;
            };
            uint8 race = 0, classId = 0, gender = 0;
            if (!extra.empty() || !parseByte(guidText, race) || !parseByte(classText, classId) || !parseByte(genderText, gender))
            {
                handler->SendSysMessage("Usage: server playerbotdev managed appearance <race ID> <class ID> <gender 0|1>");
                handler->SetSentErrorMessage(true);
                return false;
            }
            std::string failure;
            auto appearance = RandomPlayerbotFactory::PreviewAppearance(race, classId, gender, failure);
            if (!appearance)
            {
                handler->PSendSysMessage("Playerbot appearance draft rejected: %s.", failure.c_str());
                handler->SetSentErrorMessage(true);
                return false;
            }
            handler->PSendSysMessage("Read-only Playerbot appearance: race %u class %u gender %u skin %u face %u hair %u color %u facial %u. No account or character created; account/name eligibility not checked.",
                uint32(race), uint32(classId), uint32(gender), uint32(appearance->Skin), uint32(appearance->Face),
                uint32(appearance->HairStyle), uint32(appearance->HairColor), uint32(appearance->FacialHair));
            return true;
        }
        if (action == "list" && guidText.empty())
        {
            if (PlayerbotManagedRoster::List().empty())
                handler->SendSysMessage("No managed Playerbot identities are configured.");
            for (ManagedPlayerbotIdentity const& identity : PlayerbotManagedRoster::List())
            {
                ObjectGuid characterGuid = ObjectGuid::Create<HighGuid::Player>(identity.CharacterGuidLow);
                CharacterCacheEntry const* character = sCharacterCache->GetCharacterCacheByGuid(characterGuid);
                WorldSession* session = sWorld->FindServerOriginPlayerbot(characterGuid);
                auto receipt = session ? session->GetServerOriginLifecycle() : identity.Lifecycle;
                char const* state = receipt ? ServerOriginPlayerbotLifecycle::Describe(receipt->GetState()) : "offline";
                if (character && character->AccountId == identity.AccountId)
                    handler->PSendSysMessage("Playerbot %s (GUID %u, account %u, level %u, class %u): %s.",
                        character->Name.c_str(), identity.CharacterGuidLow, identity.AccountId,
                        uint32(character->Level), uint32(character->Class), state);
                else
                    handler->PSendSysMessage("Playerbot GUID %u (account %u): invalid or missing character cache entry.",
                        identity.CharacterGuidLow, identity.AccountId);
            }
            return true;
        }

        input >> extra;
        uint32 guidLow = 0;
        auto [end, error] = std::from_chars(guidText.data(), guidText.data() + guidText.size(), guidLow);
        if ((action != "start" && action != "stop") || guidText.empty() || !extra.empty() ||
            error != std::errc{} || end != guidText.data() + guidText.size() || !guidLow)
        {
            handler->SendSysMessage("Usage: server playerbotdev managed <list|start GUID|stop GUID>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        ObjectGuid characterGuid = ObjectGuid::Create<HighGuid::Player>(guidLow);
        ManagedPlayerbotIdentity const* identity = PlayerbotManagedRoster::Find(guidLow);
        if (!identity)
        {
            handler->SendSysMessage("That character is not in the managed Playerbot roster.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (action == "start")
        {
            if (!sWorld->getBoolConfig(CONFIG_PLAYERBOTS_MANAGED_ENABLED))
            {
                handler->SendSysMessage("Managed Playerbot admission is disabled or its roster is invalid.");
                handler->SetSentErrorMessage(true);
                return false;
            }
            if (!sWorld->TryStartServerOriginPlayerbot(identity->AccountId, characterGuid))
            {
                handler->SendSysMessage("Managed Playerbot admission rejected; check account ownership, class, online state and session limits.");
                handler->SetSentErrorMessage(true);
                return false;
            }
            WorldSession* session = sWorld->FindServerOriginPlayerbot(characterGuid);
            ASSERT(session && session->GetAccountId() == identity->AccountId);
            PlayerbotManagedRoster::Track(identity->AccountId, guidLow, session->GetServerOriginLifecycle());
            handler->PSendSysMessage("Managed Playerbot %u admitted; character loading is asynchronous.", guidLow);
        }
        else
        {
            if (WorldSession* session = sWorld->FindServerOriginPlayerbot(characterGuid))
            {
                if (session->GetAccountId() != identity->AccountId)
                {
                    handler->SendSysMessage("Managed Playerbot account does not match the active session.");
                    handler->SetSentErrorMessage(true);
                    return false;
                }
                PlayerbotManagedRoster::Track(identity->AccountId, guidLow, session->GetServerOriginLifecycle());
            }
            handler->PSendSysMessage("Managed Playerbot %u %s.", guidLow,
                sWorld->RequestStopServerOriginPlayerbot(characterGuid) ? "exit requested" : "is offline");
        }
        return true;
    }

    // Triggering corpses expire check in world
};
}
std::vector<ChatCommand> GetPlayerbotModuleCommands()
{
    return PlayerbotDevCommands::GetCommands();
}
