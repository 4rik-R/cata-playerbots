/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef PLAYERBOT_SESSION_BEHAVIOR_H
#define PLAYERBOT_SESSION_BEHAVIOR_H
#include "PlayerbotSessionHooks.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotReadyCheck.h"
#include "../Ai/Base/PlayerbotMount.h"
#include "../Ai/Base/PlayerbotPosition.h"
#include "../Ai/Base/PlayerbotLootPolicy.h"
#include "../Ai/Base/PlayerbotEquipmentApply.h"
#include "../Ai/Base/PlayerbotLootRoll.h"
#include "../Ai/Base/PlayerbotQuestShare.h"
#include "../Ai/Base/PlayerbotQuestAccept.h"
#include "../Ai/Base/PlayerbotRepair.h"
#include "Engine.h"
#include "StateEngines.h"
#include "../Ai/Base/PlayerbotCombatMovement.h"
#include "Cmd/PlayerbotStrategyControl.h"
#include <atomic>
#include <memory>
class PlayerbotSessionBehavior final : public PlayerbotSessionHooks
{
public:
    explicit PlayerbotSessionBehavior(WorldSession& session);
    void RequestServerOriginFollow(uint32 characterGuidLow) override;
    void RequestServerOriginHold() override;
    void RequestPartyControllerFollow(uint32 characterGuidLow) override;
    void RequestPartyControllerHold() override;
    void RequestServerOriginAttack() override;
    void RequestServerOriginCease() override;
    void RequestServerOriginInstanceJoin(uint32 mapId) override;
    bool RequestPlayerbotRange(uint32 requesterGuidLow, std::string const& param) override;
    bool RequestPlayerbotEquip(uint32 requesterGuidLow) override;
    bool RequestPlayerbotRepair(uint32 requesterGuidLow, uint64 npc, uint32 map, uint32 instance) override;
    bool RequestPlayerbotQuestCommand(uint32 requesterGuidLow, uint32 quest, uint64 giver, uint32 map, uint32 instance, uint32 operation, uint32 item) override;
    bool RequestPlayerbotStrategy(uint32 requesterGuidLow, std::string const& command,
        std::string const& token, std::string const& target, uint64 batch, PlayerbotStrategyBinding const& binding) override;
    bool RequestPlayerbotRebuff(uint32 requesterGuidLow) override;
    bool RequestPlayerbotStay(uint32 requesterGuidLow) override;
    void RequestPlayerbotReadyCheck(uint64 group, uint64 check, uint32 initiator, uint32 created) override;
    uint32 GetFollowTargetGuidLow() const override { return _serverOriginFollowTargetGuidLow.load(); }
    uint32 GetPartyControllerGuidLow() const override { return _serverOriginPartyControllerGuidLow.load(); }
    bool IsAttacking() const override { return _serverOriginAttacking.load(); }
    uint32 GetStrategyRoleMask() const override { return _strategyRoleMask.load(std::memory_order_relaxed); }
    std::shared_ptr<PlayerbotStrategySnapshot const> GetStrategySnapshot() const override
    { return std::atomic_load(&_strategySnapshot); }
    void UpdateMap(uint32 diff) override;
    PlayerbotContextState GetContextStateForMap() const override;
    void UpdateWorld() override;
private:
    void UpdateServerOriginMovement(uint32 diff);
    void CompleteNearTeleport();
    void UpdateServerOriginParty();
    void UpdateServerOriginResurrection();
    void UpdateServerOriginInstanceJoin();
    void UpdateServerOriginCombat(uint32 diff);
    bool UpdateSupportReach(PlayerbotPartySupport::ReachKind start = PlayerbotPartySupport::ReachKind::None);
    void TickEngine(bool inCombat = false);
    void SelectEngineState();
    void ReleaseStay();
    bool EvaluateReadyCheck(PlayerbotReadyCheck::Request const& request);
    void UpdateDeferredReadyCheck();
    void CancelDeferredReadyCheck();
    bool _engineTickedThisUpdate = false;
    PlayerbotQuestShare::Attempt _questShareAttempt; // world-owned native share receipt only
    PlayerbotQuestAccept::Mailbox _questAcceptCommands;
    PlayerbotRepair::Mailbox _repairCommands;
    uint32 _restCheckTimer = 0;
    WorldSession& _session;
    // The adapter exists before login. The context/engine are made after the
    // class is known and destroyed in reverse order, with no cached Player*.
    PlayerbotAI _ai;
    PlayerbotLoot::PassPreference _lootPassPreference;
    PlayerbotCombatMovement::RangeMailbox _rangeCommands;
    PlayerbotEquipmentApply::Mailbox _equipCommands;
    PlayerbotRoll::Mailbox _rollCommands;
    uint32 _rollPollTime = 0; // world-owned polling timestamp
    PlayerbotStrategyControl::Mailbox _strategyCommands;
    bool _noncombatDefaultsInitialized = false; // map-owned; overrides last until logout or gate disable
    bool _combatDefaultsInitialized = false; // shared utility overrides, not role/spec defaults
    std::shared_ptr<PlayerbotStrategySnapshot const> _strategySnapshot; // access only through atomic load/store
    ForceRebuffMailbox _rebuffCommands;
    PlayerbotMount::State _groundMount;
    PlayerbotPosition::Mailbox _stayCommands;
    PlayerbotReadyCheck::Mailbox _readyChecks;
    std::atomic<uint32> _strategyRoleMask { 0 }; // Copied combat roles, no cross-session engine access.
    std::optional<PlayerbotReadyCheck::DeferredPass> _deferredReadyCheck; // map-owned identities only
    std::unique_ptr<AiObjectContext> _aiContext;
    ActionTraceBuffer _actionHistory; // map-owned; outlives engine callbacks
    bool _traceObserversInstalled = false;
    std::unique_ptr<StateEngines> _stateEngines;
    Engine* _engine = nullptr; // borrowed active state engine
    bool _engineTransferSuspended = false;
    bool _warriorEngineBuffAnnounced = false;
    bool _warriorEngineCombatAnnounced = false;
    bool _mageEngineCombatAnnounced = false;
    bool _priestEngineHealAnnounced = false;
    // World-thread command mailbox; movement itself is changed only in the map update.
    std::atomic<uint64> _serverOriginMovementRequest { 0 }; // UINT64_MAX means hold
    std::atomic<uint32> _serverOriginFollowTargetGuidLow { 0 };
    uint32 _serverOriginLastFollowOwnerGuidLow = 0;
    uint32 _serverOriginRecoveryOwnerGuidLow = 0;
    // Invitation ownership is separate from the console-only follow path.
    std::atomic<uint32> _serverOriginPartyControllerGuidLow { 0 };
    uint32 _serverOriginFormationSignature = 0;
    uint32 _serverOriginPathRefreshMs = 0;
    bool _serverOriginPathCatchUpActive = false;
    uint8 _serverOriginMeleeStance = 0; // 0 unset, 1 behind, 2 tank/front
    uint32 _serverOriginHealCheckTimer = 0;
    ObjectGuid _serverOriginSupportReachGuid; // map-thread movement ownership only
    uint32 _serverOriginSupportReachStartedMs = 0;
    PlayerbotPartySupport::ReachKind _serverOriginSupportReachKind = PlayerbotPartySupport::ReachKind::None;
    uint32 _serverOriginResurrectionCheckTimer = 0;
    std::atomic<bool> _serverOriginAutoAssistEnabled { false };
    uint32 _serverOriginFollowTraceRemainingMs = 0;
    uint32 _serverOriginFollowTraceSampleTimerMs = 0;
    std::atomic<uint32> _serverOriginInstanceJoinRequest { 0 };
    uint32 _serverOriginPartyLeaderGuidLow = 0;
    uint32 _serverOriginInstanceJoinMapId = 0;
    uint8 _serverOriginInstanceAckBudget = 0;
    std::atomic<uint8> _serverOriginCombatRequest { 0 }; // 1 attack selected creature, 2 cease
    std::atomic<bool> _serverOriginAttacking { false };
    bool _serverOriginAutoAssistedAttack = false;
    ObjectGuid _serverOriginCombatTargetGuid;
    uint32 _serverOriginActionCheckTimer = 0;
    uint32 _serverOriginAssistCheckTimer = 0;
    uint32 _serverOriginBuffCheckTimer = 0;

};
#endif
