/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information.
 * Released under GNU GPL v2 or any later version.
 */
#include "ScriptMgr.h"
#include "Log.h"
#include "World.h"
#include "RandomPlayerbotFactory.h"
#include "RandomPlayerbotMgr.h"

void AddSC_playerbot_chat_commands();
void UpdatePlayerbotAddonStrategyBatches(); // World-thread inbox/ACK pump; never reads engines.

namespace
{
class PlayerbotsModuleWorldScript final : public WorldScript
{
public:
    PlayerbotsModuleWorldScript() : WorldScript("PlayerbotsModuleWorldScript") { }
    void OnUpdate(uint32 diff) override
    {
        RandomPlayerbotFactory::UpdateCallbacks();
        RandomPlayerbotMgr::Update(diff);
        UpdatePlayerbotAddonStrategyBatches();
    }
    void OnStartup() override
    {
        TC_LOG_INFO("server.loading", "Optional mod-playerbots loaded; development admission %s",
            sWorld->getBoolConfig(CONFIG_PLAYERBOTS_DEV_ENABLED) ? "enabled" : "disabled");
    }
};
}

void AddSC_playerbots_module()
{
    new PlayerbotsModuleWorldScript();
    AddSC_playerbot_chat_commands();
}
