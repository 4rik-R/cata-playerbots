/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information.
 * Released under GNU GPL v2 or any later version.
 */
#include "PlayerbotSessionHooks.h"
#include "PlayerbotConfig.h"
#include "PlayerbotDevFixture.h"
#include "PlayerbotManagedRoster.h"
#include "PlayerbotAddonProtocol.h"
#include "RandomPlayerbotFactory.h"
#include "Config.h"
#include "Log.h"
#include "World.h"
#include <atomic>
namespace { std::atomic<bool> ActionHistoryEnabled { false }; }
namespace { std::atomic<bool> RepairEnabled { false }; }
bool PlayerbotModuleRepairEnabled() { return RepairEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleActionHistoryEnabled() { return ActionHistoryEnabled.load(std::memory_order_relaxed); }
namespace { std::atomic<bool> RestEnabled { false }; }
namespace { std::atomic<bool> HealerSaveManaEnabled { false }; }
namespace { std::atomic<bool> StarterGearScoreEnabled { false }; }
namespace { std::atomic<bool> StarterEquipEnabled { false }; }
bool PlayerbotModuleStarterEquipEnabled() { return StarterEquipEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleStarterGearScoreEnabled() { return StarterGearScoreEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleHealerSaveManaEnabled() { return HealerSaveManaEnabled.load(std::memory_order_relaxed); }
namespace { std::atomic<bool> PotionsEnabled { false }; }
namespace { std::atomic<bool> MageArmorEnabled { false }; }
namespace { std::atomic<bool> LootPassEnabled { false }; }
namespace { std::atomic<bool> LootRollEnabled { false }; }
bool PlayerbotModuleLootRollEnabled()
{ return LootRollEnabled.load(std::memory_order_relaxed) && !PlayerbotModuleLootPassEnabled(); }
namespace { std::atomic<bool> CorpseLootEnabled { false }; }
namespace { std::atomic<bool> QuestShareEnabled { false }; }
namespace { std::atomic<bool> QuestAcceptEnabled { false }; }
namespace { std::atomic<bool> QuestRewardEnabled { false }; }
namespace { std::atomic<bool> QuestInspectionEnabled { false }; }
namespace { std::atomic<bool> QuestSendShareEnabled { false }; }
namespace { std::atomic<bool> QuestAbandonEnabled { false }; }
namespace { std::atomic<bool> QuestLootPriorityEnabled { false }; }
bool PlayerbotModuleQuestLootPriorityEnabled() { return QuestLootPriorityEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleQuestAbandonEnabled() { return QuestAbandonEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleQuestSendShareEnabled() { return QuestSendShareEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleQuestInspectionEnabled() { return QuestInspectionEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleQuestRewardEnabled() { return QuestRewardEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleQuestAcceptEnabled() { return QuestAcceptEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleQuestShareEnabled() { return QuestShareEnabled.load(std::memory_order_relaxed); }
namespace { std::atomic<bool> ReadyCheckEnabled { false }; }
namespace { std::atomic<bool> ReadyCheckRebuffEnabled { false }; }
namespace { std::atomic<bool> GroundMountEnabled { false }; }
namespace { std::atomic<bool> StayEnabled { false }; }
namespace { std::atomic<bool> StrategyControlEnabled { false }; }
namespace { std::atomic<bool> StrategyMutationEnabled { false }; }
namespace { std::atomic<bool> GroupStrategyMutationEnabled { false }; }
bool PlayerbotModuleGroupStrategyMutationEnabled() { return GroupStrategyMutationEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleStrategyMutationEnabled() { return StrategyMutationEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleStrategyControlEnabled() { return StrategyControlEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleStayEnabled() { return StayEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleGroundMountEnabled() { return GroundMountEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleReadyCheckRebuffEnabled() { return ReadyCheckRebuffEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleReadyCheckEnabled() { return ReadyCheckEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleCorpseLootEnabled() { return CorpseLootEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleLootPassEnabled() { return LootPassEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleMageArmorEnabled() { return MageArmorEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleRestEnabled() { return RestEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModulePotionsEnabled() { return PotionsEnabled.load(std::memory_order_relaxed); }
bool PlayerbotModuleEngineWarriorBuffEnabled()
{
    return sConfigMgr->GetBoolDefault("Playerbots.Dev.EngineWarriorBuff", false);
}

bool PlayerbotModuleEngineWarriorCombatEnabled()
{
    return sConfigMgr->GetBoolDefault("Playerbots.Dev.EngineWarriorCombat", false);
}

bool PlayerbotModuleEngineMageCombatEnabled()
{
    return sConfigMgr->GetBoolDefault("Playerbots.Dev.EngineMageCombat", false);
}

bool PlayerbotModuleEnginePartyBuffEnabled()
{
    return sConfigMgr->GetBoolDefault("Playerbots.Dev.EnginePartyBuff", false);
}

bool PlayerbotModuleEnginePriestHealEnabled()
{
    return sConfigMgr->GetBoolDefault("Playerbots.Dev.EnginePriestHeal", false);
}

void LoadPlayerbotModuleSettings(World& world, bool moduleConfigsValid)
{
    ActionHistoryEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Diagnostics.Actions.Enabled", false), std::memory_order_relaxed);
    QuestLootPriorityEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Quest.SyncLootWithPlayer.Enabled", false),
        std::memory_order_relaxed);
    QuestAbandonEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Quest.Abandon.Enabled", false),
        std::memory_order_relaxed);
    QuestSendShareEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Quest.Share.Enabled", false),
        std::memory_order_relaxed);
    QuestInspectionEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Quest.Inspection.Enabled", false),
        std::memory_order_relaxed);
    QuestRewardEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Quest.Reward.Enabled", false),
        std::memory_order_relaxed);
    RepairEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Maintenance.Repair.Enabled", false), std::memory_order_relaxed);
    QuestAcceptEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Quest.AcceptNpc.Enabled", false),
        std::memory_order_relaxed);
    QuestShareEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Quest.AcceptShared.Enabled", false),
        std::memory_order_relaxed);
    StarterGearScoreEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Equipment.StarterScore.Enabled", false),
        std::memory_order_relaxed);
    StarterEquipEnabled.store(PlayerbotModuleStarterGearScoreEnabled() &&
        sConfigMgr->GetBoolDefault("Playerbots.Equipment.StarterEquip.Enabled", false), std::memory_order_relaxed);
    HealerSaveManaEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Healing.SaveMana.Enabled", false),
        std::memory_order_relaxed);
    StrategyControlEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.StrategyControl.Enabled", false),
        std::memory_order_relaxed);
    StrategyMutationEnabled.store(PlayerbotModuleStrategyControlEnabled() && sConfigMgr->GetBoolDefault("Playerbots.StrategyControl.AddonMutations", false),
        std::memory_order_relaxed);
    GroupStrategyMutationEnabled.store(PlayerbotModuleStrategyMutationEnabled() &&
        sConfigMgr->GetBoolDefault("Playerbots.StrategyControl.GroupMutations", false), std::memory_order_relaxed);
    StayEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Movement.Stay.Enabled", false),
        std::memory_order_relaxed);
    GroundMountEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Mount.Ground.Enabled", false),
        std::memory_order_relaxed);
    ReadyCheckEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.ReadyCheck.Enabled", false),
        std::memory_order_relaxed);
    ReadyCheckRebuffEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.ReadyCheck.ForceRebuff", false),
        std::memory_order_relaxed);
    PlayerbotDevFixture::SetEnabled(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Dev.Enabled", false) &&
        sConfigMgr->GetBoolDefault("Playerbots.Dev.Fixture20.Enabled", false));
    PlayerbotDevFixture::SetRollFixtureEnabled(moduleConfigsValid &&
        sConfigMgr->GetBoolDefault("Playerbots.Dev.Enabled", false) &&
        sConfigMgr->GetBoolDefault("Playerbots.Dev.Fixture20.Enabled", false) &&
        sConfigMgr->GetBoolDefault("Playerbots.Dev.LootRollFixture.Enabled", false));
    CorpseLootEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Loot.Corpses.Enabled", false),
        std::memory_order_relaxed);
    LootPassEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Loot.PassOnGroupLoot", false),
        std::memory_order_relaxed);
    LootRollEnabled.store(PlayerbotModuleStarterGearScoreEnabled() &&
        sConfigMgr->GetBoolDefault("Playerbots.Loot.Rolls.Enabled", false), std::memory_order_relaxed);
    MageArmorEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Mage.Armor.Enabled", false),
        std::memory_order_relaxed);
    RestEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Rest.Enabled", false),
        std::memory_order_relaxed);
    PotionsEnabled.store(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Potions.Enabled", false),
        std::memory_order_relaxed);
    bool factoryEnabled = moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Factory.Enabled", false);
    RandomPlayerbotFactory::SetProvisioningEnabled(factoryEnabled);
    RandomPlayerbotFactory::SetInspectionEnabled(factoryEnabled || (moduleConfigsValid &&
        sConfigMgr->GetBoolDefault("Playerbots.Factory.InspectionEnabled", false)));
    SetPlayerbotAddonBridgeEnabled(moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.MultiBot.Enabled", false));
    world.setBoolConfig(CONFIG_PLAYERBOTS_DEV_ENABLED, moduleConfigsValid && sConfigMgr->GetBoolDefault("Playerbots.Dev.Enabled", false));
    world.setBoolConfig(CONFIG_PLAYERBOTS_DEV_GREETING_ENABLED, sConfigMgr->GetBoolDefault("Playerbots.Dev.GreetingOnJoin", true));
    bool managedRosterValid = PlayerbotManagedRoster::Configure(sConfigMgr->GetStringDefault("Playerbots.Managed.Characters", ""));
    if (!managedRosterValid)
        TC_LOG_ERROR("module.playerbots", "Playerbots.Managed.Characters has invalid or duplicate account:character bindings; managed admission disabled.");
    world.setBoolConfig(CONFIG_PLAYERBOTS_MANAGED_ENABLED, moduleConfigsValid && managedRosterValid &&
        sConfigMgr->GetBoolDefault("Playerbots.Managed.Enabled", false));
    bool accountLinksValid = PlayerbotManagedRoster::ConfigureAccountLinks(
        sConfigMgr->GetStringDefault("Playerbots.Managed.AccountLinks", ""));
    if (!accountLinksValid)
        TC_LOG_ERROR("module.playerbots", "Playerbots.Managed.AccountLinks is invalid; player lifecycle access disabled.");
    PlayerbotManagedRoster::SetPlayerControlEnabled(moduleConfigsValid && managedRosterValid && accountLinksValid &&
        sConfigMgr->GetBoolDefault("Playerbots.Managed.AllowPlayerControl", false));
    int32 devPlayerbotAccountId = sConfigMgr->GetIntDefault("Playerbots.Dev.AccountId", 0);
    int32 devPlayerbotCharacterGuid = sConfigMgr->GetIntDefault("Playerbots.Dev.CharacterGuid", 0);
    world.setIntConfig(CONFIG_PLAYERBOTS_DEV_ACCOUNT_ID, devPlayerbotAccountId > 0 ? uint32(devPlayerbotAccountId) : 0);
    world.setIntConfig(CONFIG_PLAYERBOTS_DEV_CHARACTER_GUID, devPlayerbotCharacterGuid > 0 ? uint32(devPlayerbotCharacterGuid) : 0);
    int32 devPlayerbotAccountId2 = sConfigMgr->GetIntDefault("Playerbots.Dev.AccountId2", 0);
    int32 devPlayerbotCharacterGuid2 = sConfigMgr->GetIntDefault("Playerbots.Dev.CharacterGuid2", 0);
    world.setIntConfig(CONFIG_PLAYERBOTS_DEV_ACCOUNT_ID_2, devPlayerbotAccountId2 > 0 ? uint32(devPlayerbotAccountId2) : 0);
    world.setIntConfig(CONFIG_PLAYERBOTS_DEV_CHARACTER_GUID_2, devPlayerbotCharacterGuid2 > 0 ? uint32(devPlayerbotCharacterGuid2) : 0);
    int32 devPlayerbotAccountId3 = sConfigMgr->GetIntDefault("Playerbots.Dev.AccountId3", 0);
    int32 devPlayerbotCharacterGuid3 = sConfigMgr->GetIntDefault("Playerbots.Dev.CharacterGuid3", 0);
    world.setIntConfig(CONFIG_PLAYERBOTS_DEV_ACCOUNT_ID_3, devPlayerbotAccountId3 > 0 ? uint32(devPlayerbotAccountId3) : 0);
    world.setIntConfig(CONFIG_PLAYERBOTS_DEV_CHARACTER_GUID_3, devPlayerbotCharacterGuid3 > 0 ? uint32(devPlayerbotCharacterGuid3) : 0);
    int32 devPlayerbotAccountId4 = sConfigMgr->GetIntDefault("Playerbots.Dev.AccountId4", 0);
    int32 devPlayerbotCharacterGuid4 = sConfigMgr->GetIntDefault("Playerbots.Dev.CharacterGuid4", 0);
    world.setIntConfig(CONFIG_PLAYERBOTS_DEV_ACCOUNT_ID_4, devPlayerbotAccountId4 > 0 ? uint32(devPlayerbotAccountId4) : 0);
    world.setIntConfig(CONFIG_PLAYERBOTS_DEV_CHARACTER_GUID_4, devPlayerbotCharacterGuid4 > 0 ? uint32(devPlayerbotCharacterGuid4) : 0);

}
