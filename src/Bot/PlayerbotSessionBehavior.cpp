/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information.
 * Released under GNU GPL v2 or any later version.
 */
#include "PlayerbotSessionBehavior.h"
#include "../Ai/Base/PlayerbotEquipmentInspection.h"
#include "Cmd/PlayerbotAddonMutation.h"
#include "Cmd/PlayerbotStrategyCompletions.h"
#include "PlayerbotDevFixture.h"
#include "../Ai/Base/PlayerbotRestStrategy.h"
#include "../Ai/Base/PlayerbotSpecStrategy.h"
#include "../Ai/Base/PlayerbotTargetSelection.h"
#include "../Ai/Base/PlayerbotCombatMovement.h"
#include "../Ai/Base/PlayerbotPartyBuffStrategy.h"
#include "../Ai/Base/PlayerbotReadyCheckSupplies.h"
#include "../Ai/Base/PlayerbotRoles.h"
#include "Chat.h"
#include "WorldSession.h"
#include "Creature.h"
#include "GameClient.h"
#include "DBCStores.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MovementPackets.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PartyPackets.h"
#include "Player.h"
#include "PlayerbotGroupStrategy.h"
#include "PlayerbotSecurity.h"
#include "MageAiObjectContext.h"
#include "PlayerbotMageStrategy.h"
#include "PriestAiObjectContext.h"
#include "PlayerbotPriestStrategy.h"
#include "../Script/PlayerbotConfig.h"
#include "PlayerbotWarriorStrategy.h"
#include "PlayerbotClassRotation.h"
#include "PlayerbotHybridHealer.h"
#include "RotationAiObjectContext.h"
#include "World.h"
#include "Random.h"
#include "Random/RandomPlayerbotCapital.h"
#include "Random/RandomPlayerbotCrossings.h"
#include "Random/RandomPlayerbotMgr.h"
#include "Random/RandomPlayerbotGrind.h"
#include "Random/RandomPlayerbotGrindTarget.h"
#include "Random/RandomPlayerbotLevels.h"
#include "Random/RandomPlayerbotProgression.h"
#include "WorldPacket.h"
#include "../Ai/Base/PlayerbotItemUsage.h"
#include "WarriorAiObjectContext.h"
#include "Timer.h"
namespace
{
bool PassiveEngineEnabled(PlayerbotAI const& ai)
{
    Player* bot = ai.GetBot();
    return ai.Rebuff().IsPending(getMSTime()) || PlayerbotModuleRestEnabled() || PlayerbotModuleMageArmorEnabled() || PlayerbotModuleCorpseLootEnabled() ||
        (bot && bot->getClass() == CLASS_MAGE && PlayerbotModuleEngineMageCombatEnabled());
}
}
PlayerbotSessionBehavior::PlayerbotSessionBehavior(WorldSession& session)
    : _session(session), _ai(session) { }

std::unique_ptr<PlayerbotSessionHooks> CreatePlayerbotSessionHooks(WorldSession& session)
{
    return std::make_unique<PlayerbotSessionBehavior>(session);
}
bool PlayerbotModuleSupportsClass(uint8 playerClass)
{
    return playerClass == CLASS_WARRIOR || playerClass == CLASS_MAGE || playerClass == CLASS_PRIEST ||
        PlayerbotClassRotation::Supports(playerClass);
}
PlayerbotContextState PlayerbotSessionBehavior::GetContextStateForMap() const
{
    PlayerbotContextState result;
    result.ActionHistory = _actionHistory.Read(getMSTime());
    if (!_stateEngines || !_engine || _engineTransferSuspended)
        return result;
    result.Available = true;
    result.Staying = _ai.IsStaying();
    switch (_stateEngines->Current())
    {
        case StateEngines::State::NonCombat: result.EngineState = "noncombat"; break;
        case StateEngines::State::Combat: result.EngineState = "combat"; break;
        case StateEngines::State::Dead: result.EngineState = "dead"; break;
    }
    result.LastExecutedAction = _engine->GetLastAction();
    result.QueuedCount = _engine->QueuedCount();
    result.CombatStrategies = _stateEngines->Get(StateEngines::State::Combat).GetStrategies();
    result.NonCombatStrategies = _stateEngines->Get(StateEngines::State::NonCombat).GetStrategies();
    return result;
}

void PlayerbotSessionBehavior::UpdateMap(uint32 diff)
{
    std::string strategyAck;
    uint32 strategyAckRequester = 0;
    std::optional<PlayerbotAddonProtocol::StrategyBatch::Completion> groupCompletion;
    _engineTickedThisUpdate = false;
    _ai.SupportReachRequests().Enable(false); // Intent never survives an update/control boundary.
    CompleteNearTeleport();
    if (Player* bot = _ai.GetBot()) PlayerbotDevFixture::Process(*bot);
    // Native group roll creation consumes this preference. Do not inspect or
    // mutate Group/Roll objects here, or fabricate a world-thread loot packet.
    if (Player* bot = _ai.GetBot())
        if (auto preference = _lootPassPreference.Update(PlayerbotModuleLootPassEnabled(), bot->GetPassOnGroupLoot()))
            bot->SetPassOnGroupLoot(*preference);
    // Construct class objects only after login yields the actual Player.
    if (!_engine)
        if (Player* bot = _ai.GetBot())
            if (bot->getClass() == CLASS_WARRIOR)
            {
                _aiContext = std::make_unique<WarriorAiObjectContext>(&_ai);
                _stateEngines = std::make_unique<StateEngines>(&_ai, *_aiContext);
                _engine = &_stateEngines->Active();
                _engine->AddStrategy("nc");
            }
            else if (bot->getClass() == CLASS_MAGE)
            {
                _aiContext = std::make_unique<MageAiObjectContext>(&_ai);
                _stateEngines = std::make_unique<StateEngines>(&_ai, *_aiContext);
                _engine = &_stateEngines->Active();
                _engine->AddStrategy("buff");
                _engine->AddStrategy("bmana");
                _engine->AddStrategy("bdps");
                _engine->AddStrategy("cure");
            }
            else if (bot->getClass() == CLASS_PRIEST)
            {
                _aiContext = std::make_unique<PriestAiObjectContext>(&_ai);
                _stateEngines = std::make_unique<StateEngines>(&_ai, *_aiContext);
                _engine = &_stateEngines->Active();
                _engine->AddStrategy("cure");
                _engine->AddStrategy("heal");
                _engine->AddStrategy("nc");
                _engine->AddStrategy("buff");
            }
            else if (PlayerbotClassRotation::Supports(bot->getClass()))
            {
                _aiContext = std::make_unique<RotationAiObjectContext>(&_ai);
                _stateEngines = std::make_unique<StateEngines>(&_ai, *_aiContext);
                _engine = &_stateEngines->Active();
                _engine->AddStrategy("nc");
            }

    if (_stateEngines && (!_traceObserversInstalled || _actionHistory.Enabled() != PlayerbotModuleActionHistoryEnabled()))
    {
        bool enabled = PlayerbotModuleActionHistoryEnabled();
        _actionHistory.Enable(enabled);
        for (auto const& [state, name] : std::array<std::pair<StateEngines::State, char const*>, 3>{{
            {StateEngines::State::NonCombat, "noncombat"}, {StateEngines::State::Combat, "combat"}, {StateEngines::State::Dead, "dead"}}})
        {
            std::function<void(ActionTraceRecord)> observer;
            if (enabled) observer = [this, name](ActionTraceRecord record) { _actionHistory.Record(name, std::move(record)); };
            _stateEngines->Get(state).SetTraceObserver(std::move(observer));
        }
        _traceObserversInstalled = true;
    }

    if (_engine)
        if (Player* bot = _ai.GetBot())
        {
            Engine& combat = _stateEngines->Get(StateEngines::State::Combat);
            PlayerbotSpec::Refresh(combat, bot->getClass(), bot->GetPrimaryTalentTree(bot->GetActiveSpec()));
            if (!_combatDefaultsInitialized || !PlayerbotModuleStrategyControlEnabled())
            {
                PlayerbotStrategyControl::RestoreCombatDefaults(combat,
                    bot->getClass() == CLASS_MAGE || bot->getClass() == CLASS_PRIEST);
                _combatDefaultsInitialized = true;
            }
            if (bot->getClass() == CLASS_MAGE || bot->getClass() == CLASS_PRIEST)
            {
                if (!combat.HasStrategy("cure")) combat.AddStrategy("cure");
            }
            if (bot->getClass() == CLASS_PRIEST && !combat.HasStrategy("healer dps"))
                combat.AddStrategy("healer dps");
            _strategyRoleMask.store(combat.GetStrategyTypeMask() & PlayerbotRoles::RoleFlags, std::memory_order_relaxed);
        }

    if (_stateEngines)
    {
        Engine& noncombat = _stateEngines->Get(StateEngines::State::NonCombat);
        if (!_noncombatDefaultsInitialized || !PlayerbotModuleStrategyControlEnabled())
        {
            if (!noncombat.HasStrategy("food")) noncombat.AddStrategy("food");
            if (!noncombat.HasStrategy("loot")) noncombat.AddStrategy("loot");
            _noncombatDefaultsInitialized = true;
        }
        SelectEngineState();
    }
    UpdateServerOriginParty();
    UpdateSoloController();
    if (_serverOriginResurrectionCheckTimer > diff)
        _serverOriginResurrectionCheckTimer -= diff;
    else
    {
        _serverOriginResurrectionCheckTimer = 2000;
        UpdateServerOriginResurrection();
    }
    UpdateServerOriginMovement(diff);
    UpdateServerOriginInstanceJoin();
    // Armor admission/cast locks are temporary. Leave the copied request queued
    // until idle; expiry and world retry handle longer fights within native roll lifetime.
    if (Player* actor = _ai.GetBot(); actor && actor->IsInWorld() && actor->IsAlive() &&
        !actor->IsInCombat() && !actor->IsNonMeleeSpellCast(false) && !actor->IsBeingTeleported())
    if (auto request = _rollCommands.Take())
    {
        Player* bot = _ai.GetBot();
        Player* controller = bot && bot->IsInWorld() ? bot->GetMap()->GetPlayer(
            ObjectGuid::Create<HighGuid::Player>(request->Controller)) : nullptr;
        bool attached = request->Controller == _serverOriginFollowTargetGuidLow.load() ||
            request->Controller == _serverOriginPartyControllerGuidLow.load();
        uint8 choice = ROLL_PASS;
        if (PlayerbotModuleLootRollEnabled() && PlayerbotRoll::Fresh(*request, getMSTime()) &&
            bot && bot->IsAlive() && bot->IsInWorld() && !bot->IsBeingTeleported() && _aiContext &&
            bot->GetMapId() == request->Identity.Map && bot->GetInstanceId() == request->Identity.Instance &&
            attached && controller && controller->GetSession() && !controller->GetSession()->IsServerOrigin() &&
            !controller->IsBeingTeleported() && PlayerbotSecurity(*bot).CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, *controller))
        {
            ItemTemplate const* item = sObjectMgr->GetItemTemplate(request->Identity.Entry);
            using namespace PlayerbotItemUsage;
            Fact fact;
            Kind kind = Kind::Other;
            if (item)
            {
                std::string query = std::to_string(request->Identity.Entry);
                if (item->GetClass() == ITEM_CLASS_WEAPON || item->GetClass() == ITEM_CLASS_ARMOR)
                {
                    kind = Kind::Equipment;
                    auto survey = PlayerbotEquipment::CompareLoot(_ai, request->Identity);
                    if (survey.Available && survey.Owner == bot->GetGUID() && !survey.Items.empty())
                        fact = {SurveyUsage(survey, request->Identity.Entry, survey.Items.front().Input.Property),
                            Scope::NativeLootEquipment};
                }
                else if (item->GetClass() == ITEM_CLASS_CONSUMABLE &&
                    !request->Identity.Property && !request->Identity.SuffixFactor)
                    if (auto* value = _aiContext->GetValue<Fact>("item usage", query))
                    {
                        if (auto* stock = _aiContext->GetValue<PlayerbotConsumable::Usage>("consumable usage", query)) stock->Reset();
                        value->Reset(); fact = value->Get();
                        if (fact.Source != Scope::ConsumableStock) fact = {};
                    }
                if (auto vote = ChooseRoll(fact.Result, kind, {2, true, false, false}, true, false, false))
                    choice = uint8(*vote);
            }
        }
        // Unknown/unsupported or invalid map eligibility falls back to pass;
        // world execution still rechecks the live identity/controller and mask.
        if (!PlayerbotLootRoll::Admits(true, request->Identity.Mask, choice)) choice = ROLL_PASS;
        _rollCommands.Complete(*request, choice, getMSTime());
    }
    if (auto request = _equipCommands.Take())
    {
        Player* bot = _ai.GetBot();
        Player* requester = bot && bot->IsInWorld() ? bot->GetMap()->GetPlayer(ObjectGuid::Create<HighGuid::Player>(request->Requester)) : nullptr;
        bool attached = request->Requester == _serverOriginFollowTargetGuidLow.load() ||
            request->Requester == _serverOriginPartyControllerGuidLow.load();
        auto result = PlayerbotEquipmentApply::Result::Unavailable;
        if (bot && requester && requester->GetSession() && !requester->GetSession()->IsServerOrigin() &&
            attached && PlayerbotEquipmentApply::Fresh(*request, getMSTime()) &&
            PlayerbotModuleStarterEquipEnabled() && PlayerbotModuleStarterGearScoreEnabled() &&
            requester->IsAlive() && !requester->IsBeingTeleported() && !requester->IsInCombat() &&
            !_engineTransferSuspended && !_serverOriginAttacking.load() && !_ai.GetRestSpellId() &&
            !_ai.LootRequests().Pending() && !_ai.LootPursuit().Active() &&
            !PlayerbotTargetSelection::HasNearbyPartyCombat(*bot, *requester) &&
            PlayerbotSecurity(*bot).CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, *requester))
            result = PlayerbotEquipmentApply::ApplyOne(_session, _ai);
        _equipCommands.Finish(request->Serial);
        if (result == PlayerbotEquipmentApply::Result::Confirmed)
        {
            if (_stateEngines) _stateEngines->CancelPendingActions();
            std::atomic_store(&_strategySnapshot, std::shared_ptr<PlayerbotStrategySnapshot const>{});
        }
        if (requester && requester->GetSession())
            ChatHandler(requester->GetSession()).PSendSysMessage("Playerbot %s: gear apply %s.", bot->GetName().c_str(),
                result == PlayerbotEquipmentApply::Result::Confirmed ? "confirmed one native slot change" :
                result == PlayerbotEquipmentApply::Result::NoChange ? "found no qualified safe change" : "rejected; authority, state or inventory was not eligible");
        if (bot) TC_LOG_INFO("module.playerbots", "PB-EQUIP: %s result=%u", bot->GetName().c_str(), uint32(result));
    }
    if (auto request = _strategyCommands.Take(getMSTime()))
    {
        Player* bot = _ai.GetBot();
        Player* requester = bot && bot->IsInWorld() ? bot->GetMap()->GetPlayer(
            ObjectGuid::Create<HighGuid::Player>(request->Requester)) : nullptr;
        Player* controller = _ai.GetController();
        bool addon = !request->Token.empty();
        bool succeeded = false;
        Group* requesterGroup = requester ? requester->GetGroup() : nullptr;
        Group* botGroup = bot ? bot->GetGroup() : nullptr;
        bool nativeMembers = !request->Batch || request->Binding.Scope == "ALL" ||
            (requesterGroup && botGroup && requesterGroup->IsMember(requester->GetGUID()) && botGroup->IsMember(bot->GetGUID()));
        bool bindingValid = !request->Batch || (PlayerbotModuleGroupStrategyMutationEnabled() && request->Binding.Valid() &&
            requester && requester->GetSession() && !requester->GetSession()->isLogingOut() &&
            request->Binding.SessionMatches(requester->GetSession()->GetAccountId(), requester->GetSession()->GetPlayerbotRequestIdentity()) &&
            nativeMembers && request->Binding.GroupMatches(requesterGroup ? requesterGroup->GetGUID().GetRawValue() : 0,
                botGroup ? botGroup->GetGUID().GetRawValue() : 0, requesterGroup && requesterGroup->isRaidGroup()));
        char const* reason = addon && !PlayerbotModuleStrategyMutationEnabled() ? "DISABLED" : "FORBIDDEN";
        // Resolve all native objects anew at execution; never borrow another session's engine.
        if (bindingValid && PlayerbotModuleStrategyControlEnabled() && (!addon || PlayerbotModuleStrategyMutationEnabled()) && _stateEngines && !_engineTransferSuspended &&
            requester && requester->GetSession() && !requester->GetSession()->IsServerOrigin() &&
            !requester->IsBeingTeleported() && controller && controller->GetGUID() == requester->GetGUID() &&
            bot->IsInPhase(requester) && PlayerbotSecurity(*bot).CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, *requester))
        {
            auto const& command = request->Value;
            bool busy = command.Mutation && (!bot->IsAlive() || bot->IsInCombat() || IsAttacking() ||
                bot->IsNonMeleeSpellCast(false) || _ai.GetRestSpellId() || _ai.LootRequests().Pending() ||
                _ai.Rebuff().IsPending(getMSTime()) || !_serverOriginSupportReachGuid.IsEmpty() ||
                PlayerbotTargetSelection::HasNearbyPartyCombat(*bot, *requester));
            ChatHandler reply(requester->GetSession());
            if (busy)
            {
                reason = "BUSY";
                if (!addon) reply.PSendSysMessage("Playerbot %s: strategy change rejected while busy; retry when idle.", bot->GetName().c_str());
            }
            else
            {
                auto state = command.State == PlayerbotStrategyControl::Scope::Combat ? StateEngines::State::Combat :
                    command.State == PlayerbotStrategyControl::Scope::Dead ? StateEngines::State::Dead : StateEngines::State::NonCombat;
                Engine& selected = _stateEngines->Get(state);
                auto result = selected.ChangeStrategies(command.Param, PlayerbotStrategyControl::MutableStrategies(command.State));
                if (result.Status == Engine::StrategyChangeStatus::Rejected)
                {
                    reason = "UNSUPPORTED_STRATEGY";
                    if (!addon) reply.PSendSysMessage("Playerbot %s: unsupported strategy request rejected.", bot->GetName().c_str());
                }
                else
                {
                    succeeded = true;
                    reason = "OK";
                    char const* scope = command.State == PlayerbotStrategyControl::Scope::Combat ? "co" :
                        command.State == PlayerbotStrategyControl::Scope::Dead ? "de" : "nc";
                    if (!addon) reply.PSendSysMessage("Playerbot %s: %s strategies %s.", bot->GetName().c_str(), scope,
                        result.Status == Engine::StrategyChangeStatus::Changed ? "changed" : "unchanged");
                    if (result.Query && !addon)
                    {
                        std::string names;
                        for (auto const& name : selected.GetStrategies()) { if (!names.empty()) names += ", "; names += name; }
                        reply.PSendSysMessage("Playerbot %s %s: %s", bot->GetName().c_str(), scope, names.empty() ? "(none)" : names.c_str());
                    }
                }
            }
        }
        else if (!addon && requester && requester->GetSession() && !requester->GetSession()->IsServerOrigin())
            ChatHandler(requester->GetSession()).SendSysMessage("Playerbot strategy request rejected: controller, authority or live state changed.");
        if (addon)
        {
            if (request->Batch)
                groupCompletion = PlayerbotAddonProtocol::StrategyBatch::Completion{request->Batch, request->Requester,
                    bot ? bot->GetGUID().GetCounter() : 0, request->Token,
                    request->Value.State == PlayerbotStrategyControl::Scope::Combat ? "C" : "N",
                    PlayerbotAddonProtocol::StrategyBatch::Stage::Executed, succeeded};
            else
            {
                PlayerbotAddonProtocol::StrategyMutation response{"BOT", request->Target, request->Token,
                    request->Value.State == PlayerbotStrategyControl::Scope::Combat ? "C" : "N", {}};
                strategyAck = PlayerbotAddonProtocol::StrategyAck(response, 1, succeeded ? 1 : 0, succeeded ? 0 : 1, reason);
                strategyAckRequester = request->Requester;
            }
        }
    }
    if (auto command = _rangeCommands.Take(getMSTime()))
        if (_engine && !_engineTransferSuspended)
            _engine->ExecuteAction("range", Event("chat", command->Param,
                ObjectGuid::Create<HighGuid::Player>(command->Requester)));
    if (auto request = _rebuffCommands.Take(getMSTime()))
    {
        Player* bot = _ai.GetBot();
        Player* requester = bot && bot->IsInWorld() ? bot->GetMap()->GetPlayer(ObjectGuid::Create<HighGuid::Player>(request->Requester)) : nullptr;
        if (_engine && !_engineTransferSuspended && bot && bot->IsAlive() && requester &&
            !requester->IsBeingTeleported() && _ai.GetController() && PlayerbotModuleEnginePartyBuffEnabled() &&
            (bot->getClass() == CLASS_MAGE || bot->getClass() == CLASS_PRIEST) &&
            PlayerbotSecurity(*bot).CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, *requester))
        {
            _ai.Rebuff().Begin(getMSTime());
            _restCheckTimer = 0;
            ChatHandler(requester->GetSession()).PSendSysMessage("Playerbot %s: rebuff pass started.", bot->GetName().c_str());
        }
    }
    UpdateServerOriginCombat(diff);
    UpdateSoloGrind(diff);
    SelectEngineState();
    if (auto request = _readyChecks.TakeRequest(getMSTime()))
    {
        CancelDeferredReadyCheck();
        Player* bot = _ai.GetBot();
        Player* initiator = bot && bot->IsInWorld() ?
            bot->GetMap()->GetPlayer(ObjectGuid::Create<HighGuid::Player>(request->Initiator)) : nullptr;
        Player* controller = _ai.GetController();
        uint32 buff = bot && bot->getClass() == CLASS_MAGE ? PlayerbotPartyBuff::Brilliance.SpellId :
            bot && bot->getClass() == CLASS_PRIEST ? PlayerbotPartyBuff::Fortitude.SpellId : 0;
        if (PlayerbotModuleReadyCheckEnabled() && PlayerbotModuleReadyCheckRebuffEnabled() && PlayerbotModuleEnginePartyBuffEnabled() &&
            _engine && !_engineTransferSuspended && bot && bot->IsAlive() && !bot->IsInCombat() && !IsAttacking() &&
            !bot->IsBeingTeleported() && !_serverOriginInstanceJoinRequest.load() && controller && initiator &&
            controller->IsAlive() && controller->IsInWorld() && !controller->IsBeingTeleported() &&
            !controller->IsInCombat() && !initiator->IsInCombat() &&
            controller->GetGroup() == bot->GetGroup() &&
            !initiator->IsBeingTeleported() && bot->GetGroup() && bot->GetGroup() == initiator->GetGroup() &&
            uint64(bot->GetGroup()->GetGUID()) == request->Group && buff && bot->HasSpell(buff) &&
            _readyChecks.IsCurrent(*request, getMSTime()) &&
            PlayerbotSecurity(*bot).CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, *initiator))
        {
            bool ownsPass = !_ai.Rebuff().IsPending(getMSTime());
            if (ownsPass) _ai.Rebuff().Begin(getMSTime()); // Preserve an existing manual pass.
            _deferredReadyCheck = PlayerbotReadyCheck::DeferredPass{*request, uint64(controller->GetGUID()),
                _ai.Rebuff().Serial(), ownsPass};
            _restCheckTimer = 0;
        }
        else
            _readyChecks.Complete(*request, EvaluateReadyCheck(*request), getMSTime());
    }
    if (PassiveEngineEnabled(_ai) || _ai.IsStaying())
    {
        if (_restCheckTimer > diff)
            _restCheckTimer -= diff;
        else
        {
            _restCheckTimer = 2000;
            Player* bot = _ai.GetBot();
            if (bot && bot->IsAlive() && !bot->IsInCombat() && !_serverOriginAttacking.load() &&
                _serverOriginSupportReachGuid.IsEmpty())
                TickEngine();
        }
    }
    UpdateDeferredReadyCheck();
    Player* snapshotBot = _ai.GetBot();
    Player* snapshotController = _ai.GetController();
    bool strategyReadEnabled = PlayerbotModuleStrategyControlEnabled();
    bool gearReadEnabled = PlayerbotModuleStarterGearScoreEnabled();
    if ((strategyReadEnabled || gearReadEnabled) && _stateEngines && !_engineTransferSuspended &&
        snapshotBot && snapshotBot->IsInWorld() && !snapshotBot->IsBeingTeleported())
    {
        uint32 botGuid = snapshotBot->GetGUID().GetCounter();
        uint32 controllerGuid = snapshotController ? snapshotController->GetGUID().GetCounter() : 0;
        uint32 created = getMSTime();
        auto combat = strategyReadEnabled ? _stateEngines->Get(StateEngines::State::Combat).GetStrategies() : std::vector<std::string>{};
        auto noncombat = strategyReadEnabled ? _stateEngines->Get(StateEngines::State::NonCombat).GetStrategies() : std::vector<std::string>{};
        auto prior = GetStrategySnapshot();
        bool sameIdentity = prior && prior->Bot == botGuid && prior->Controller == controllerGuid;
        bool gearDue = PlayerbotEquipmentInspection::Due(gearReadEnabled, prior && prior->EquipmentEnabled,
            sameIdentity, created, prior ? prior->EquipmentCreated : 0);
        if (!prior || !sameIdentity || gearDue || prior->StrategiesReady != strategyReadEnabled ||
            prior->EquipmentEnabled != gearReadEnabled ||
            prior->Combat != combat || prior->NonCombat != noncombat || uint32(created - prior->Created) >= 1000)
        {
            auto snapshot = std::make_shared<PlayerbotStrategySnapshot>();
            snapshot->Bot = botGuid;
            snapshot->Controller = controllerGuid;
            snapshot->Created = created;
            snapshot->Combat = std::move(combat);
            snapshot->NonCombat = std::move(noncombat);
            snapshot->StrategiesReady = strategyReadEnabled;
            snapshot->EquipmentEnabled = gearReadEnabled;
            if (gearReadEnabled)
            {
                if (gearDue)
                {
                    snapshot->EquipmentCreated = created;
                    auto* value = _aiContext ? _aiContext->GetValue<PlayerbotEquipment::Survey>("starter equipment comparisons") : nullptr;
                    auto survey = value ? value->Get() : PlayerbotEquipment::Survey{};
                    snapshot->EquipmentAvailable = survey.Available && survey.Owner == snapshotBot->GetGUID();
                    if (snapshot->EquipmentAvailable)
                    {
                        snapshot->EquipmentTotal = uint32(survey.Items.size());
                        snapshot->EquipmentRows = PlayerbotEquipmentInspection::Rows(survey);
                    }
                }
                else if (prior)
                {
                    snapshot->EquipmentCreated = prior->EquipmentCreated;
                    snapshot->EquipmentAvailable = prior->EquipmentAvailable;
                    snapshot->EquipmentTotal = prior->EquipmentTotal;
                    snapshot->EquipmentRows = prior->EquipmentRows;
                }
            }
            std::atomic_store(&_strategySnapshot, std::shared_ptr<PlayerbotStrategySnapshot const>(std::move(snapshot)));
        }
    }
    else
        std::atomic_store(&_strategySnapshot, std::shared_ptr<PlayerbotStrategySnapshot const>{});
    // Publish the changed read model before acknowledging it to a client that
    // may immediately request STATE. Resolve the recipient again; retain no Player*.
    if (groupCompletion)
        PlayerbotAddonProtocol::GroupStrategyCompletions().Submit(std::move(*groupCompletion));
    if (!strategyAck.empty() && snapshotBot && snapshotBot->IsInWorld())
        if (Player* recipient = snapshotBot->GetMap()->GetPlayer(ObjectGuid::Create<HighGuid::Player>(strategyAckRequester)))
            if (recipient->GetSession() && !recipient->GetSession()->IsServerOrigin())
            {
                WorldPacket packet;
                ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, recipient, recipient,
                    strategyAck, 0U, "", DEFAULT_LOCALE, "MBOT");
                recipient->SendDirectMessage(&packet);
            }
}
bool PlayerbotSessionBehavior::EvaluateReadyCheck(PlayerbotReadyCheck::Request const& request)
{
    Player* bot = _ai.GetBot();
    if (!bot || !PlayerbotModuleReadyCheckEnabled()) return false;
    Player* initiator = bot->IsInWorld() ? bot->GetMap()->GetPlayer(
        ObjectGuid::Create<HighGuid::Player>(request.Initiator)) : nullptr;
    bool initiatorNearby = initiator && !initiator->IsBeingTeleported() && bot->GetDistance(initiator) <= 100.0f;
    float mana = bot->GetMaxPower(POWER_MANA) > 0 ?
        100.0f * bot->GetPower(POWER_MANA) / bot->GetMaxPower(POWER_MANA) : 0.0f;
    auto supplies = PlayerbotReadyCheck::CarriedSupplies(*bot);
    bool usesMana = bot->GetPowerType() == POWER_MANA;
    bool partyCombat = initiator && PlayerbotTargetSelection::HasNearbyPartyCombat(*bot, *initiator);
    bool ready = PlayerbotReadyCheck::BasicReadiness(bot->IsInWorld(), bot->IsAlive(), bot->IsInCombat() || IsAttacking() || partyCombat,
        bot->IsBeingTeleported() || _engineTransferSuspended || _serverOriginInstanceJoinRequest.load(),
        bot->IsNonMeleeSpellCast(false) || _ai.Rebuff().IsPending(getMSTime()), bot->GetHealthPct(),
        usesMana, mana, initiatorNearby) && supplies.Ready(usesMana);
    if (initiator)
        ChatHandler(initiator->GetSession()).PSendSysMessage(
            "Playerbot %s readiness snapshot: %s; food %u, drink %u, healing potions %u, mana potions %u.",
            bot->GetName().c_str(), ready ? "ready" : "not ready", supplies.Food, supplies.Drink,
            supplies.HealingPotion, supplies.ManaPotion);
    return ready;
}
void PlayerbotSessionBehavior::CancelDeferredReadyCheck()
{
    if (_deferredReadyCheck && _deferredReadyCheck->OwnsPass &&
        _ai.Rebuff().Serial() == _deferredReadyCheck->RebuffSerial)
        _ai.Rebuff().End(); // Never cancel a replacement manual pass.
    _deferredReadyCheck.reset();
}
void PlayerbotSessionBehavior::UpdateDeferredReadyCheck()
{
    if (!_deferredReadyCheck) return;
    auto pass = *_deferredReadyCheck;
    Player* bot = _ai.GetBot();
    Player* controller = _ai.GetController();
    auto result = PlayerbotReadyCheck::PollDeferred(pass.Identity, getMSTime(),
        PlayerbotModuleReadyCheckEnabled() && _readyChecks.IsCurrent(pass.Identity, getMSTime()),
        controller && uint64(controller->GetGUID()) == pass.Controller,
        _ai.Rebuff().Serial() == pass.RebuffSerial,
        PlayerbotModuleReadyCheckRebuffEnabled() && PlayerbotModuleEnginePartyBuffEnabled() &&
            bot && controller && controller->IsAlive() && controller->IsInWorld() &&
            !controller->IsBeingTeleported() && !controller->IsInCombat() && controller->GetGroup() == bot->GetGroup() &&
            bot->IsInWorld() && bot->IsAlive() && !bot->IsInCombat() && !IsAttacking() &&
            !bot->IsBeingTeleported() && !_engineTransferSuspended && !_serverOriginInstanceJoinRequest.load(),
        _ai.Rebuff().IsPending(getMSTime()), _ai.Rebuff().Completed());
    if (result == PlayerbotReadyCheck::DeferredResult::Wait) return;
    bool ready = result == PlayerbotReadyCheck::DeferredResult::Evaluate && EvaluateReadyCheck(pass.Identity);
    CancelDeferredReadyCheck();
    if (result != PlayerbotReadyCheck::DeferredResult::Cancel)
        _readyChecks.Complete(pass.Identity, ready, getMSTime());
    else
        _readyChecks.Cancel(pass.Identity);
}
void PlayerbotSessionBehavior::CompleteNearTeleport()
{
    Player* bot = _ai.GetBot();
    if (!bot || !bot->IsInWorld() || !bot->IsBeingTeleportedNear() || bot->IsHasDelayedTeleport())
        return;
    ReleaseStay(); // Invalidate before native ACK clears the near-transfer semaphore.
    _strategyCommands.Cancel();
    GameClient* client = _session.GetGameClient();
    if (!client || !client->IsAllowedToMove(bot->GetGUID()) ||
        (client->GetActivelyMovedUnit() && client->GetActivelyMovedUnit() != bot))
        return; // Never seize a revoked/charmed/vehicle mover to acknowledge self.
    if (!client->GetActivelyMovedUnit())
    {
        WorldPackets::Movement::SetActiveMover active{WorldPacket(CMSG_SET_ACTIVE_MOVER)};
        active.ActiveMover = bot->GetGUID();
        _session.HandleSetActiveMoverOpcode(active); // Native allowed-mover validation.
    }
    WorldPackets::Movement::MoveTeleportAck ack{WorldPacket(MSG_MOVE_TELEPORT_ACK)};
    ack.MoverGUID = bot->GetGUID();
    _session.HandleMoveTeleportAck(ack);
    if (bot->IsBeingTeleported() || !bot->IsInWorld())
        return;
    if (_stateEngines) _stateEngines->CancelPendingActions();
    _serverOriginSupportReachGuid.Clear();
    _serverOriginFormationSignature = 0;
    _serverOriginPathCatchUpActive = false;
    _serverOriginPathRefreshMs = 0;
    bot->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
    bot->StopMoving();
    uint8 expected = 0;
    _serverOriginCombatRequest.compare_exchange_strong(expected, 2);
    TC_LOG_INFO("module.playerbots", "PB-TRANSFER: %s completed native same-map teleport", bot->GetName().c_str());
}

void PlayerbotSessionBehavior::TickEngine(bool inCombat)
{
    SelectEngineState();
    if (_engine && !_engineTransferSuspended && _stateEngines->Current() != StateEngines::State::Dead &&
        (!PassiveEngineEnabled(_ai) || !_engineTickedThisUpdate))
    {
        _engineTickedThisUpdate = true;
        auto& rebuff = _ai.Rebuff();
        uint32 now = getMSTime();
        if (!rebuff.IsPending(now)) rebuff.End();
        rebuff.BeginCycle();
        bool combat = inCombat || _stateEngines->Current() == StateEngines::State::Combat;
        _engine->Tick(false, rebuff.IsPending(now), combat);
        Player* bot = _ai.GetBot();
        if (rebuff.IsPending(getMSTime()) && bot && !combat && !bot->IsNonMeleeSpellCast(false) &&
            !rebuff.HasWork() && !PlayerbotPartyBuff::RebuffOnGlobalCooldown(_ai))
        {
            rebuff.Finish();
            TC_LOG_INFO("module.playerbots", "PB-REBUFF: %s ended pass with no current eligible buff work", bot->GetName().c_str());
        }
    }
}
void PlayerbotSessionBehavior::SelectEngineState()
{
    if (!_stateEngines) return;
    Player* bot = _ai.GetBot();
    bool suspended = !bot || bot->IsBeingTeleported() || _serverOriginInstanceJoinRequest.load();
    if (suspended) _strategyCommands.Cancel();
    if (suspended || !bot->IsAlive()) _stayCommands.Cancel();
    if (_ai.IsStaying() && !PlayerbotPosition::KeepStay(PlayerbotModuleStayEnabled(), suspended, PlayerbotPosition::ValidStay(_ai)))
    {
        ReleaseStay();
        if (!_serverOriginFollowTargetGuidLow.load()) _ai.ClearController();
        _serverOriginRecoveryOwnerGuidLow = 0;
    }
    PlayerbotPosition::UpdateReturn(_ai);
    if (suspended || !bot->IsAlive()) _ai.Rebuff().End();
    if (suspended != _engineTransferSuspended)
        _stateEngines->CancelPendingActions();
    _engineTransferSuspended = suspended;
    auto state = !bot || !bot->IsAlive() ? StateEngines::State::Dead :
        (bot->IsInCombat() || _serverOriginAttacking.load() ? StateEngines::State::Combat : StateEngines::State::NonCombat);
    if (_stateEngines->Select(state) && bot)
        TC_LOG_INFO("module.playerbots", "PB-STATE: %s entered %s", bot->GetName().c_str(),
            state == StateEngines::State::Dead ? "dead" : state == StateEngines::State::Combat ? "combat" : "noncombat");
    _engine = &_stateEngines->Active();
}
void PlayerbotSessionBehavior::UpdateWorld()
{
    UpdateRandomProgression();
    PlayerbotRepair::Update(_session, _repairCommands);
    PlayerbotQuestAccept::Update(_session, _questAcceptCommands);
    PlayerbotQuestShare::Update(_session, _questShareAttempt);
    if (!PlayerbotModuleLootRollEnabled()) _rollCommands.Cancel();
    else
    {
        Player* bot = _session.GetPlayer();
        Group* group = bot ? bot->GetGroup() : nullptr;
        auto attached = [&](uint32 controller)
        {
            return group && controller && group->IsMember(ObjectGuid::Create<HighGuid::Player>(controller)) &&
                (controller == _serverOriginPartyControllerGuidLow.load() || controller == _serverOriginFollowTargetGuidLow.load());
        };
        if (auto reply = _rollCommands.Consume(getMSTime()))
            if (bot && bot->IsInWorld() && !bot->IsBeingTeleported() && attached(reply->Original.Controller) &&
                group->ValidatePlayerbotLootVote(*bot, reply->Original.Identity, reply->Choice))
            {
                WorldPacket packet(CMSG_LOOT_ROLL, 13);
                packet << ObjectGuid(reply->Original.Identity.Roll) << uint32(reply->Original.Identity.Slot) << reply->Choice;
                _session.HandleLootRoll(packet);
                TC_LOG_INFO("module.playerbots", "PB-ROLL: %s submitted native vote %u for item %u roll %llu", bot->GetName().c_str(),
                    uint32(reply->Choice), reply->Original.Identity.Entry, static_cast<unsigned long long>(reply->Original.Identity.Roll));
            }
        uint32 now = getMSTime();
        uint32 controller = _serverOriginPartyControllerGuidLow.load();
        if (!controller) controller = _serverOriginFollowTargetGuidLow.load();
        if (bot && bot->IsInWorld() && !bot->IsBeingTeleported() && attached(controller) && uint32(now - _rollPollTime) >= 1000)
        {
            _rollPollTime = now;
            PlayerbotLootRoll roll;
            if (group->GetPlayerbotPendingLootRoll(*bot, roll)) _rollCommands.Post(roll, controller, now);
        }
    }
    auto validCheck = [&](PlayerbotReadyCheck::Request const& request)
    {
        Player* bot = _session.GetPlayer();
        Group* group = bot ? bot->GetGroup() : nullptr;
        ObjectGuid initiator = ObjectGuid::Create<HighGuid::Player>(request.Initiator);
        Player* requester = ObjectAccessor::FindConnectedPlayer(initiator);
        return PlayerbotModuleReadyCheckEnabled() && bot && group &&
            uint64(group->GetGUID()) == request.Group && group->IsMember(bot->GetGUID()) &&
            group->IsMember(initiator) && requester && requester->GetGroup() == group &&
            (group->IsLeader(initiator) || group->IsAssistant(initiator)) &&
            group->MatchesPlayerbotReadyCheck(request.Check, initiator, getMSTime());
    };
    if (auto active = _readyChecks.Active(getMSTime()))
        if (!validCheck(*active)) _readyChecks.Cancel(*active);
    if (auto reply = _readyChecks.TakeReply(getMSTime()))
    {
        Player* bot = _session.GetPlayer();
        if (validCheck(reply->Identity))
        {
            WorldPacket answer(MSG_RAID_READY_CHECK, 1);
            answer << uint8(reply->Ready ? 1 : 0); // Cata input is state only, NOT donor GUID + state.
            _session.HandleRaidReadyCheckOpcode(answer);
            TC_LOG_INFO("module.playerbots", "PB-READY: %s answered ready check %llu: %s", bot->GetName().c_str(),
                static_cast<unsigned long long>(reply->Identity.Check), reply->Ready ? "ready" : "not ready");
        }
    }
    if (_serverOriginMovementRequest.load() || _serverOriginCombatRequest.load() ||
        _serverOriginInstanceJoinRequest.load() || _serverOriginAttacking.load())
        _ai.LootRequests().Cancel();
    uint32 lootController = _serverOriginFollowTargetGuidLow.load();
    if (!lootController && _soloActive.load())
        lootController = _session.GetServerOriginCharacterGuid().GetCounter(); // solo bots loot for themselves
    PlayerbotCorpseLoot::ProcessWorld(_session, _ai.LootRequests(),
        ObjectGuid::Create<HighGuid::Player>(lootController));
    Player* _player = _session.GetPlayer();
    if (_session.IsServerOrigin() && _player && _serverOriginInstanceAckBudget && _player->IsBeingTeleportedFar())
    {
        --_serverOriginInstanceAckBudget;
        _session.HandleMoveWorldportAck();
        _player = _session.GetPlayer();
        if (_player && _player->IsInWorld())
        {
            if (_player->GetMapId() == _serverOriginInstanceJoinMapId)
            {
                TC_LOG_INFO("module.playerbots", "PB-PARTY: %s entered dungeon map %u instance %u", _player->GetName().c_str(),
                    _player->GetMapId(), _player->GetInstanceId());
                uint32 controllerGuidLow = _serverOriginPartyLeaderGuidLow;
                Group* group = _player->GetGroup();
                if (controllerGuidLow && group && group->IsMember(ObjectGuid::Create<HighGuid::Player>(controllerGuidLow)))
                {
                    RequestServerOriginFollow(controllerGuidLow);
                    _serverOriginPartyControllerGuidLow.store(controllerGuidLow);
                }
                else
                    RequestServerOriginHold();
            }
            else
                TC_LOG_INFO("module.playerbots", "PB-PARTY: %s dungeon transfer failed; arrived on map %u", _player->GetName().c_str(), _player->GetMapId());
            _serverOriginInstanceAckBudget = 0;
        }
    }

}
void PlayerbotSessionBehavior::RequestServerOriginFollow(uint32 characterGuidLow)
{
    ASSERT(_session.IsServerOrigin() && characterGuidLow);
    RequestServerOriginCease();
    _serverOriginPartyControllerGuidLow.store(0);
    _serverOriginMovementRequest.store(characterGuidLow);
}

void PlayerbotSessionBehavior::RequestServerOriginHold()
{
    ASSERT(_session.IsServerOrigin());
    RequestServerOriginCease();
    _serverOriginPartyControllerGuidLow.store(0);
    _serverOriginMovementRequest.store(uint64(-1));
}

void PlayerbotSessionBehavior::RequestPartyControllerFollow(uint32 characterGuidLow)
{
    ASSERT(_session.IsServerOrigin() && characterGuidLow);
    if (_serverOriginPartyControllerGuidLow.load() != characterGuidLow)
        return;
    RequestServerOriginCease();
    _serverOriginMovementRequest.store(characterGuidLow);
}

void PlayerbotSessionBehavior::RequestPartyControllerHold()
{
    ASSERT(_session.IsServerOrigin());
    if (!_serverOriginPartyControllerGuidLow.load())
        return;
    RequestServerOriginCease();
    _serverOriginMovementRequest.store(uint64(-1));
}

void PlayerbotSessionBehavior::RequestServerOriginAttack()
{
    ASSERT(_session.IsServerOrigin());
    _stayCommands.Cancel();
    _serverOriginCombatRequest.store(1);
}

void PlayerbotSessionBehavior::RequestServerOriginCease()
{
    ASSERT(_session.IsServerOrigin());
    _stayCommands.Cancel();
    _serverOriginAutoAssistEnabled.store(false);
    _serverOriginCombatRequest.store(2);
}

void PlayerbotSessionBehavior::RequestServerOriginInstanceJoin(uint32 mapId)
{
    ASSERT(_session.IsServerOrigin() && mapId);
    _stayCommands.Cancel();
    _serverOriginInstanceJoinRequest.store(mapId);
}
bool PlayerbotSessionBehavior::RequestPlayerbotRange(uint32 requesterGuidLow, std::string const& param)
{
    return _rangeCommands.Post(requesterGuidLow, param, getMSTime());
}
bool PlayerbotSessionBehavior::RequestPlayerbotEquip(uint32 requesterGuidLow)
{
    return PlayerbotModuleStarterGearScoreEnabled() && PlayerbotModuleStarterEquipEnabled() &&
        _equipCommands.Post(requesterGuidLow, getMSTime());
}
bool PlayerbotSessionBehavior::RequestPlayerbotRepair(uint32 requesterGuidLow, uint64 npc, uint32 map, uint32 instance)
{
    return PlayerbotModuleRepairEnabled() && _repairCommands.Post(requesterGuidLow, npc, map, instance, getMSTime());
}
bool PlayerbotSessionBehavior::RequestPlayerbotQuestCommand(uint32 requesterGuidLow, uint32 quest, uint64 giver, uint32 map, uint32 instance, uint32 operation, uint32 item)
{
    auto action = static_cast<PlayerbotQuestAccept::Operation>(operation);
    bool enabled = (action == PlayerbotQuestAccept::Operation::Accept || action == PlayerbotQuestAccept::Operation::AcceptAll) ? PlayerbotModuleQuestAcceptEnabled() :
        (action == PlayerbotQuestAccept::Operation::Reward || action == PlayerbotQuestAccept::Operation::RewardAll) ? PlayerbotModuleQuestRewardEnabled() :
        PlayerbotQuestAccept::IsInspection(action) ? PlayerbotModuleQuestInspectionEnabled() :
        action == PlayerbotQuestAccept::Operation::Share ? PlayerbotModuleQuestSendShareEnabled() :
        action == PlayerbotQuestAccept::Operation::Abandon && PlayerbotModuleQuestAbandonEnabled();
    return enabled && _questAcceptCommands.Post(requesterGuidLow, quest, giver, map, instance, getMSTime(), action, item);
}
bool PlayerbotSessionBehavior::RequestPlayerbotStrategy(uint32 requesterGuidLow, std::string const& command,
    std::string const& token, std::string const& target, uint64 batch, PlayerbotStrategyBinding const& binding)
{
    return PlayerbotModuleStrategyControlEnabled() && (token.empty() || PlayerbotModuleStrategyMutationEnabled()) &&
        (!batch || PlayerbotModuleGroupStrategyMutationEnabled()) &&
        _strategyCommands.Post(requesterGuidLow, command, getMSTime(), token, target, batch, binding);
}
bool PlayerbotSessionBehavior::RequestPlayerbotRebuff(uint32 requesterGuidLow)
{
    return _rebuffCommands.Post(requesterGuidLow, getMSTime());
}
bool PlayerbotSessionBehavior::RequestPlayerbotStay(uint32 requesterGuidLow)
{
    return PlayerbotModuleStayEnabled() && !_serverOriginMovementRequest.load() && !_serverOriginCombatRequest.load() &&
        !_serverOriginInstanceJoinRequest.load() && _stayCommands.Post(requesterGuidLow, getMSTime());
}
void PlayerbotSessionBehavior::ReleaseStay()
{
    _stayCommands.Cancel();
    if (_stateEngines)
    {
        _stateEngines->Get(StateEngines::State::NonCombat).RemoveStrategy("stay");
        if (_ai.IsStaying()) _stateEngines->CancelPendingActions();
    }
    PlayerbotPosition::Release(_ai);
}
void PlayerbotSessionBehavior::RequestPlayerbotReadyCheck(uint64 group, uint64 check, uint32 initiator, uint32 created)
{
    if (PlayerbotModuleReadyCheckEnabled())
        _readyChecks.Post({group, check, initiator, created});
}

void PlayerbotSessionBehavior::UpdateServerOriginMovement(uint32 diff)
{
    Player* _player = _session.GetPlayer();
    if (!_session.IsServerOrigin() || !_player || !_player->IsInWorld())
        return;

    auto hold = [this, _player](bool retainOwner)
    {
        if (_stateEngines) _stateEngines->CancelPendingActions();
        _serverOriginFollowTargetGuidLow.store(0);
        if (!retainOwner)
        {
            _ai.ClearController();
            _serverOriginLastFollowOwnerGuidLow = 0;
            _serverOriginRecoveryOwnerGuidLow = 0;
        }
        _serverOriginAutoAssistEnabled.store(false);
        _serverOriginFormationSignature = 0;
        _serverOriginPathRefreshMs = 0;
        _serverOriginPathCatchUpActive = false;
        _serverOriginHealCheckTimer = 0;
        _serverOriginSupportReachGuid.Clear();
        _serverOriginFollowTraceRemainingMs = 0;
        _serverOriginFollowTraceSampleTimerMs = 0;
        _player->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
        _player->GetMotionMaster()->Clear(MOTION_SLOT_IDLE);
        _player->GetMotionMaster()->MoveIdle();
        _player->StopMoving();
    };

    uint64 request = _serverOriginMovementRequest.exchange(0);
    bool activateStay = false;
    if (request || _serverOriginCombatRequest.load() || _serverOriginInstanceJoinRequest.load()) _strategyCommands.Cancel();
    if (request || _serverOriginCombatRequest.load() || _serverOriginInstanceJoinRequest.load()) ReleaseStay();
    if (auto stay = _stayCommands.Take(getMSTime()))
    {
        Player* requester = _player->GetMap()->GetPlayer(ObjectGuid::Create<HighGuid::Player>(stay->Requester));
        if (!request && !_serverOriginCombatRequest.load() && !_serverOriginInstanceJoinRequest.load() &&
            requester && requester == _ai.GetController() && _stateEngines &&
            PlayerbotSecurity(*_player).CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, *requester) &&
            PlayerbotPosition::CaptureStay(_ai))
        {
            activateStay = true; request = uint64(-1);
            RequestServerOriginCease(); // Native cease is processed later in this same map update.
        }
        else if (requester)
            ChatHandler(requester->GetSession()).PSendSysMessage("Playerbot %s: stay rejected; requires the current controller and safe idle ground state.", _player->GetName().c_str());
    }
    if (!_serverOriginSupportReachGuid.IsEmpty() && (request || _serverOriginCombatRequest.load() ||
        _serverOriginInstanceJoinRequest.load() || !_player->IsAlive() || _player->IsBeingTeleported()))
    {
        _player->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
        _player->StopMoving();
        _serverOriginSupportReachGuid.Clear();
    }
    if (request && _stateEngines) _stateEngines->CancelPendingActions();
    if (request || _serverOriginCombatRequest.load() || _serverOriginAttacking.load() ||
        !PlayerbotModuleCorpseLootEnabled() || !_player->IsAlive() || _player->IsBeingTeleported())
        _ai.LootRequests().Cancel();
    bool looting = _ai.LootRequests().Pending();
    bool lootEnded = _ai.LootRequests().TakeResumeNeeded();
    bool pursuitEnded = false;
    looting = PlayerbotCorpseLoot::UpdateMovement(_ai, request != 0 || _serverOriginCombatRequest.load() ||
        _serverOriginAttacking.load() || _serverOriginInstanceJoinRequest.load(), pursuitEnded) || looting;
    lootEnded = lootEnded || pursuitEnded;
    bool wasResting = _ai.GetRestSpellId() != 0;
    bool resting = PlayerbotRest::Update(_ai, request != 0 || _serverOriginCombatRequest.load() != 0 ||
        _serverOriginAttacking.load());
    bool restEnded = wasResting && !resting;
    auto mount = PlayerbotMount::Update(_ai, _groundMount, _serverOriginFollowTargetGuidLow.load() != 0,
        request != 0 || _serverOriginCombatRequest.load() || _serverOriginAttacking.load() ||
        _serverOriginInstanceJoinRequest.load(), getMSTime());
    if (mount.ResetFollow) _serverOriginFormationSignature = 0;
    if (request == uint64(-1))
    {
        if (!activateStay) ReleaseStay();
        hold(activateStay);
        if (activateStay)
        {
            _ai.SetStaying(true);
            _stateEngines->Get(StateEngines::State::NonCombat).AddStrategy("stay");
            TC_LOG_INFO("module.playerbots", "PB-STAY: %s captured current position", _player->GetName().c_str());
            if (Player* owner = _ai.GetController())
                ChatHandler(owner->GetSession()).PSendSysMessage("Playerbot %s: stay position set.", _player->GetName().c_str());
        }
    }
    else if (request)
    {
        ObjectGuid ownerGuid = ObjectGuid::Create<HighGuid::Player>(uint32(request));
        Player* owner = _player->GetMap()->GetPlayer(ownerGuid);
        if (owner && owner != _player && !owner->GetSession()->IsServerOrigin() && owner->IsAlive() && _player->IsAlive())
        {
            PlayerbotGroup::FollowPosition position = PlayerbotGroup::PositionFor(*_player, *owner);
            bool canMove = PlayerbotCombatMovement::CanMove(*_player);
            if (canMove) _player->GetMotionMaster()->MoveFollow(owner, position.Distance, position.Angle);
            _serverOriginFormationSignature = canMove ? position.Signature : 0; // Retry after native control ends.
            _serverOriginPathRefreshMs = 0;
            _serverOriginPathCatchUpActive = false;
            _serverOriginHealCheckTimer = 0;
            _serverOriginFollowTargetGuidLow.store(uint32(request));
            _ai.SetController(ownerGuid);
            _serverOriginLastFollowOwnerGuidLow = uint32(request);
            _serverOriginRecoveryOwnerGuidLow = 0;
            _serverOriginAutoAssistEnabled.store(true);
            _serverOriginFollowTraceRemainingMs = 60000;
            _serverOriginFollowTraceSampleTimerMs = 0;
            TC_LOG_DEBUG("module.playerbots", "PB-01: %s following %s", _player->GetName().c_str(), owner->GetName().c_str());
            if (_player->getClass() == CLASS_PRIEST)
                PlayerbotPriest::LogKnownAbilities(*_player);
        }
        else
        {
            hold(false);
            TC_LOG_DEBUG("module.playerbots", "PB-01: follow target %u unavailable on bot map; holding", uint32(request));
        }
    }

    uint32 activeOwner = _serverOriginFollowTargetGuidLow.load();
    if (activeOwner)
    {
        Player* owner = _player->GetMap()->GetPlayer(ObjectGuid::Create<HighGuid::Player>(activeOwner));
        if (!owner || !owner->IsAlive() || !_player->IsAlive())
        {
            bool died = (owner && !owner->IsAlive()) || !_player->IsAlive();
            hold(died || (!owner && _player->IsAlive()));
            if (died)
                _serverOriginRecoveryOwnerGuidLow = activeOwner;
            if (!_player->IsAlive())
                TC_LOG_INFO("module.playerbots", "PB-DEATH: bot %s died while following; holding", _player->GetName().c_str());
            else if (owner && !owner->IsAlive())
                TC_LOG_INFO("module.playerbots", "PB-DEATH: human owner %s died; bot %s holding", owner->GetName().c_str(), _player->GetName().c_str());
            else
                TC_LOG_DEBUG("module.playerbots", "PB-01: follow target left bot map; holding");
        }
        else if (_serverOriginFollowTraceRemainingMs)
        {
            if (_serverOriginFollowTraceSampleTimerMs <= diff)
            {
                TC_LOG_DEBUG("module.playerbots", "PB-01 trace: t=%us owner=(%.2f,%.2f,%.2f) bot=(%.2f,%.2f,%.2f) distance=%.1f ownerFlags=%u botMotion=%u",
                    (60000 - _serverOriginFollowTraceRemainingMs) / 1000,
                    owner->GetPositionX(), owner->GetPositionY(), owner->GetPositionZ(),
                    _player->GetPositionX(), _player->GetPositionY(), _player->GetPositionZ(),
                    _player->GetDistance(owner), owner->GetUnitMovementFlags(),
                    uint32(_player->GetMotionMaster()->GetCurrentMovementGeneratorType()));
                _serverOriginFollowTraceSampleTimerMs = 1000;
            }
            else
                _serverOriginFollowTraceSampleTimerMs -= diff;

            _serverOriginFollowTraceRemainingMs = _serverOriginFollowTraceRemainingMs > diff ? _serverOriginFollowTraceRemainingMs - diff : 0;
        }
        if (owner && owner->IsAlive() && _player->IsAlive() && !resting && !looting && !mount.BlockFollow && !_serverOriginAttacking.load() &&
            !_player->IsInCombat() && _serverOriginSupportReachGuid.IsEmpty() && PlayerbotCombatMovement::CanMove(*_player))
        {
            PlayerbotGroup::FollowPosition position = PlayerbotGroup::PositionFor(*_player, *owner);
            bool usePath = PlayerbotGroup::ShouldPathCatchUp(_serverOriginPathCatchUpActive, _player->GetExactDist2d(owner));
            if (usePath)
            {
                if (_serverOriginPathRefreshMs <= diff)
                {
                    // Playerbots' far-follow action uses a path to close long gaps.
                    // Let the core pathfinder handle this leg, then resume formation.
                    _player->GetMotionMaster()->MovePoint(0, owner->GetPosition(), true);
                    _serverOriginPathRefreshMs = 3000;
                }
                else
                    _serverOriginPathRefreshMs -= diff;
                _serverOriginPathCatchUpActive = true;
            }
            else if (restEnded || lootEnded || _serverOriginPathCatchUpActive || position.Signature != _serverOriginFormationSignature)
            {
                if (_serverOriginPathCatchUpActive)
                    _player->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
                _player->GetMotionMaster()->MoveFollow(owner, position.Distance, position.Angle);
                _serverOriginFormationSignature = position.Signature;
                _serverOriginPathCatchUpActive = false;
                _serverOriginPathRefreshMs = 0;
            }
        }
    }
    else if (_serverOriginRecoveryOwnerGuidLow)
    {
        Player* owner = _player->GetMap()->GetPlayer(ObjectGuid::Create<HighGuid::Player>(_serverOriginRecoveryOwnerGuidLow));
        if (owner && PlayerbotGroup::CanResumeAfterDeath(*_player, *owner))
        {
            uint32 ownerGuidLow = _serverOriginRecoveryOwnerGuidLow;
            bool partyControlled = _serverOriginPartyControllerGuidLow.load() == ownerGuidLow;
            _serverOriginRecoveryOwnerGuidLow = 0;
            RequestServerOriginFollow(ownerGuidLow);
            if (partyControlled)
                _serverOriginPartyControllerGuidLow.store(ownerGuidLow);
            TC_LOG_INFO("module.playerbots", "PB-RECOVERY: %s and owner %s are alive nearby; resuming follow", _player->GetName().c_str(), owner->GetName().c_str());
        }
    }
}

void PlayerbotSessionBehavior::UpdateServerOriginParty()
{
    Player* _player = _session.GetPlayer();
    if (!_session.IsServerOrigin() || !_player || !_player->IsInWorld())
        return;

    if (uint32 controllerGuidLow = _serverOriginPartyControllerGuidLow.load())
    {
        Group* group = _player->GetGroup();
        if (!group || !group->IsMember(ObjectGuid::Create<HighGuid::Player>(controllerGuidLow)))
        {
            RequestServerOriginHold();
            _serverOriginPartyLeaderGuidLow = 0;
            TC_LOG_INFO("module.playerbots", "PB-PARTY: %s lost invitation-adopted controller GUID %u; holding", _player->GetName().c_str(), controllerGuidLow);
        }
    }

    if (Group* group = _player->GetGroup())
    {
        if (!_serverOriginPartyControllerGuidLow.load() && group->isLFGGroup() &&
            RandomPlayerbotMgr::IsRandomAccount(_session.GetAccountId()))
            AdoptDungeonGroupHuman(*group);
        return;
    }

    Group* invitation = _player->GetGroupInvite();
    if (!invitation)
        return;

    // Adapt mod-playerbots' AcceptInvitationAction: resolve the inviting
    // leader, use the core accept flow, then adopt that human as controller.
    // This default-off dev roster keeps the donor invite/control distinction:
    // an ungrouped bot may accept an eligible human invite, while full control
    // belongs to the adopted party controller. The core also checks the invite.
    Player* owner = ObjectAccessor::FindPlayer(invitation->GetLeaderGUID());
    WorldPackets::Party::PartyInviteResponse response(WorldPacket(CMSG_PARTY_INVITE_RESPONSE, 0));
    if (!owner || !PlayerbotSecurity(*_player).CheckLevelFor(PLAYERBOT_SECURITY_INVITE, *owner))
    {
        response.Accept = false;
        _session.HandlePartyInviteResponseOpcode(response);
        TC_LOG_INFO("module.playerbots", "PB-PARTY: %s declined invitation without an available human leader", _player->GetName().c_str());
        return;
    }

    uint32 ownerGuidLow = owner->GetGUID().GetCounter();
    response.Accept = true;
    _session.HandlePartyInviteResponseOpcode(response);
    if (_player->GetGroup() && _player->GetGroup()->IsMember(owner->GetGUID()))
    {
        _serverOriginPartyLeaderGuidLow = ownerGuidLow;
        RequestServerOriginCease();
        if (owner->GetMap() == _player->GetMap() && owner->IsAlive() && _player->IsAlive())
            RequestServerOriginFollow(ownerGuidLow);
        else
        {
            RequestServerOriginHold();
        }
        _serverOriginPartyControllerGuidLow.store(ownerGuidLow);
        TC_LOG_INFO("module.playerbots", "PB-PARTY: %s joined followed leader GUID %u's party; controller adopted from invitation", _player->GetName().c_str(), ownerGuidLow);
        PlayerbotGroup::GreetOnJoin(*_player, *owner);
    }
}

void PlayerbotSessionBehavior::AdoptDungeonGroupHuman(Group& group)
{
    // A dungeon-finder group has no invitation to adopt a controller from: the bot follows a human in its dungeon,
    // the leader first, once both are on the same map.
    Player* bot = _session.GetPlayer();
    Player* human = nullptr;
    for (GroupReference* ref = group.GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || member->GetSession()->IsServerOrigin() || !member->IsInWorld() ||
            member->GetMap() != bot->GetMap())
            continue;
        if (!human || group.IsLeader(member->GetGUID()))
            human = member;
    }
    if (!human)
        return;
    uint32 humanGuidLow = human->GetGUID().GetCounter();
    _serverOriginPartyLeaderGuidLow = humanGuidLow;
    RequestServerOriginCease();
    if (human->IsAlive() && bot->IsAlive())
        RequestServerOriginFollow(humanGuidLow);
    else
        RequestServerOriginHold();
    _serverOriginPartyControllerGuidLow.store(humanGuidLow);
    TC_LOG_INFO("module.playerbots", "PB-LFG: %s follows %s in its dungeon group", bot->GetName().c_str(), human->GetName().c_str());
    PlayerbotGroup::GreetOnJoin(*bot, *human);
}

void PlayerbotSessionBehavior::UpdateServerOriginResurrection()
{
    Player* bot = _ai.GetBot();
    if (!bot || bot->IsAlive() || !bot->IsResurrectRequested() ||
        !PlayerbotModuleEnginePriestHealEnabled())
        return;

    Group* group = bot->GetGroup();
    if (!group)
        return;

    // Adapt donor AcceptResurrectAction: answer only a request from a current
    // party member and let Cata's normal handler apply raid and request rules.
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
            !bot->IsResurrectRequestedBy(member->GetGUID()))
            continue;

        WorldPacket response(CMSG_RESURRECT_RESPONSE, 9);
        response << member->GetGUID() << uint8(1);
        _session.HandleResurrectResponseOpcode(response);
        if (bot->IsAlive())
            TC_LOG_INFO("module.playerbots", "PB-RECOVERY: %s accepted resurrection from %s", bot->GetName().c_str(), member->GetName().c_str());
        return;
    }
}

void PlayerbotSessionBehavior::UpdateServerOriginInstanceJoin()
{
    Player* _player = _session.GetPlayer();
    if (!_session.IsServerOrigin() || !_player || !_player->IsInWorld())
        return;

    uint32 mapId = _serverOriginInstanceJoinRequest.exchange(0);
    if (!mapId)
        return;

    Group* group = _player->GetGroup();
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
    AreaTriggerStruct const* entrance = mapEntry && mapEntry->IsDungeon() && !mapEntry->IsRaid() ?
        sObjectMgr->GetMapEntranceTrigger(mapId) : nullptr;
    if (!group || !_serverOriginPartyLeaderGuidLow ||
        group->GetLeaderGUID() != ObjectGuid::Create<HighGuid::Player>(_serverOriginPartyLeaderGuidLow) ||
        !_player->IsAlive() || _player->IsInCombat() || _player->IsBeingTeleportedFar() ||
        !entrance || !group->GetBoundInstance(mapEntry))
    {
        TC_LOG_INFO("module.playerbots", "PB-PARTY: dungeon join rejected for %s map %u; requires a living, idle bot in its followed leader's bound party dungeon", _player->GetName().c_str(), mapId);
        return;
    }

    if (!_player->TeleportTo(mapId, entrance->target_X, entrance->target_Y, entrance->target_Z, entrance->target_Orientation))
    {
        TC_LOG_INFO("module.playerbots", "PB-PARTY: dungeon join denied by core entry checks for %s map %u", _player->GetName().c_str(), mapId);
        return;
    }

    _serverOriginInstanceJoinMapId = mapId;
    _serverOriginInstanceAckBudget = 2; // one target transfer, at most one homebind fallback
    TC_LOG_INFO("module.playerbots", "PB-PARTY: %s began dungeon transfer to map %u", _player->GetName().c_str(), mapId);
}

bool PlayerbotSessionBehavior::UpdateSupportReach(PlayerbotPartySupport::ReachKind start)
{
    Player* bot = _ai.GetBot();
    Player* owner = _ai.GetController();
    ObjectGuid next;
    bool hybrid = bot && PlayerbotHybridHealer::IsPartyHealer(*bot);
    bool enabled = bot && owner && (bot->getClass() == CLASS_PRIEST || hybrid) &&
        bot->IsInWorld() && owner->IsInWorld() && bot->IsAlive() && owner->IsAlive() &&
        _serverOriginFollowTargetGuidLow.load() && _serverOriginAutoAssistEnabled.load() &&
        (hybrid || PlayerbotModuleEnginePriestHealEnabled()) && !_engineTransferSuspended &&
        !bot->IsBeingTeleported() && !owner->IsBeingTeleported() && !bot->IsNonMeleeSpellCast(false) &&
        !bot->IsMounted() && PlayerbotCombatMovement::CanMove(*bot) &&
        !_serverOriginInstanceJoinRequest.load() && !_ai.GetRestSpellId() &&
        !_ai.LootRequests().Pending() && !_ai.LootPursuit().Active();
    _ai.SupportReachRequests().Enable(enabled);
    auto kind = start != PlayerbotPartySupport::ReachKind::None ? start : _serverOriginSupportReachKind;
    if (enabled && (start != PlayerbotPartySupport::ReachKind::None || !_serverOriginSupportReachGuid.IsEmpty()))
        next = kind == PlayerbotPartySupport::ReachKind::Resurrect ?
            PlayerbotPriest::ResurrectionReachTarget(*bot, *owner, PlayerbotCombatMovement::GetRange(_ai, "spell")) :
            PlayerbotPriest::HealingReachTarget(*bot, *owner, PlayerbotCombatMovement::GetRange(_ai, "heal"));
    if (bot && !next.IsEmpty() && next == _serverOriginSupportReachGuid &&
        PlayerbotPriest::HealingReachMustYield(getMSTimeDiff(_serverOriginSupportReachStartedMs, getMSTime()),
            bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == POINT_MOTION_TYPE))
    {
        next.Clear(); // Yield one decision tick, then re-evaluate a fresh destination.
        _ai.SupportReachRequests().Enable(false); // Do not reacquire in this same engine tick.
    }
    if (next != _serverOriginSupportReachGuid)
    {
        if (bot && !_serverOriginSupportReachGuid.IsEmpty())
        {
            bot->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
            bot->StopMoving();
        }
        _serverOriginSupportReachGuid = next;
        _serverOriginSupportReachKind = next.IsEmpty() ? PlayerbotPartySupport::ReachKind::None : kind;
        _serverOriginSupportReachStartedMs = getMSTime();
        if (bot && !next.IsEmpty())
            if (Player* target = bot->GetMap()->GetPlayer(next))
            {
                // Snapshot destination, native pathfinding; no retained Player pointer.
                // Re-evaluate at the healing cadence and stop upon entering heal range.
                bot->GetMotionMaster()->MovePoint(0, target->GetPosition(), true);
                TC_LOG_INFO("module.playerbots", "PB-HEAL: %s closing %s gap to %s",
                    bot->GetName().c_str(), kind == PlayerbotPartySupport::ReachKind::Resurrect ? "resurrection" : "healing", target->GetName().c_str());
            }
    }
    return !_serverOriginSupportReachGuid.IsEmpty();
}

void PlayerbotSessionBehavior::UpdateServerOriginCombat(uint32 diff)
{
    Player* _player = _session.GetPlayer();
    if (!_session.IsServerOrigin() || !_player || !_player->IsInWorld())
        return;
    bool const solo = _soloActive.load();
    // Followed human, or the solo bot itself as its own controller.
    auto combatOwner = [this, _player, solo]() -> Player*
    {
        uint32 ownerGuidLow = _serverOriginFollowTargetGuidLow.load();
        if (ownerGuidLow)
            return _player->GetMap()->GetPlayer(ObjectGuid::Create<HighGuid::Player>(ownerGuidLow));
        return solo ? _player : nullptr;
    };

    // Called only after commands and the offensive target have been validated.
    // Holy paladins, Restoration shamans and druids heal in a party; alone they fight.
    bool const hybridHealer = !solo && PlayerbotHybridHealer::IsPartyHealer(*_player);
    bool const supportHealer = (_player->getClass() == CLASS_PRIEST && !solo) || hybridHealer;
    auto tickPriest = [this, _player, diff, solo, hybridHealer]()
    {
        if ((_player->getClass() != CLASS_PRIEST && !hybridHealer) ||
            !(_serverOriginFollowTargetGuidLow.load() || _serverOriginRecoveryOwnerGuidLow || solo))
            return;
        if (_serverOriginHealCheckTimer > diff)
            _serverOriginHealCheckTimer -= diff;
        else
        {
            _serverOriginHealCheckTimer = 750;
            if (UpdateSupportReach())
                return;
            if (hybridHealer)
                PlayerbotHybridHealer::HealParty(*_player, _ai.GetController());
            else if (_engine && PlayerbotModuleEnginePriestHealEnabled())
            {
                if (!_priestEngineHealAnnounced)
                {
                    TC_LOG_INFO("module.playerbots", "PB-ENGINE: %s Priest healing routed through engine", _player->GetName().c_str());
                    _priestEngineHealAnnounced = true;
                }
                TickEngine(_player->IsInCombat());
                auto reach = _ai.SupportReachRequests().Take();
                if (reach != PlayerbotPartySupport::ReachKind::None) UpdateSupportReach(reach);
            }
            else
                PlayerbotPriest::HealParty(*_player, _ai.GetController());
        }
    };

    auto cease = [this, _player, supportHealer]()
    {
        if (_stateEngines) _stateEngines->CancelPendingActions();
        _ai.ClearCurrentTarget();
        _serverOriginSupportReachGuid.Clear();
        if (_serverOriginAttacking.exchange(false))
            TC_LOG_DEBUG("module.playerbots", "PB-02: %s ceased attack", _player->GetName().c_str());
        _serverOriginCombatTargetGuid.Clear();
        _serverOriginAutoAssistedAttack = false;
        _serverOriginActionCheckTimer = 0;
        _serverOriginMeleeStance = 0;
        _player->AttackStop();
        _player->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
        _player->StopMoving();
        if (_player->getClass() == CLASS_PRIEST || supportHealer)
        {
            // Priest support replaced idle follow, unlike active Mage/Warrior chase.
            // Restore it even while native combat flags are still draining.
            uint32 ownerGuidLow = _serverOriginFollowTargetGuidLow.load();
            Player* owner = ownerGuidLow ? _player->GetMap()->GetPlayer(ObjectGuid::Create<HighGuid::Player>(ownerGuidLow)) : nullptr;
            _serverOriginFormationSignature = 0; // Follow can resume after native control restrictions end.
            if (owner && owner->IsAlive() && PlayerbotCombatMovement::CanMove(*_player))
            {
                auto position = PlayerbotGroup::PositionFor(*_player, *owner);
                _player->GetMotionMaster()->MoveFollow(owner, position.Distance, position.Angle);
                _serverOriginFormationSignature = position.Signature;
            }
        }
    };

    auto validTarget = [_player](Player* owner, Creature* target)
    {
        return owner && owner->IsAlive() && target && !target->IsControlledByPlayer() && target->IsAlive() && _player->IsAlive() &&
            !PlayerbotTargetSelection::IsProtectedTarget(*target) &&
            _player->IsValidAttackTarget(target) && _player->IsWithinDistInMap(target, 25.0f) &&
            owner->IsWithinDistInMap(target, 25.0f) && _player->IsWithinLOSInMap(target);
    };

    auto beginAttack = [this, _player, &cease, solo, supportHealer](Creature* target, bool autoAssisted)
    {
        // A solo Priest damages its own target from spell range; a party healer supports.
        bool priest = supportHealer;
        bool rotation = PlayerbotClassRotation::Supports(_player->getClass());
        bool ranged = _player->getClass() == CLASS_MAGE || (_player->getClass() == CLASS_PRIEST && solo) ||
            (rotation && PlayerbotClassRotation::FightsAtRange(*_player));
        if (_player->getClass() == CLASS_PRIEST && !PlayerbotModuleEnginePriestHealEnabled())
            return false;
        if (!priest && !ranged && _player->getClass() != CLASS_WARRIOR && !rotation)
            return false;
        cease();
        // Donor AttackAction faces before native attack/chase submission.
        // Failure to turn does not bypass native movement/cast restrictions.
        PlayerbotCombatMovement::FaceForAttack(*_player, *target);
        if (!priest && !_player->Attack(target, !ranged))
            return false;

        _serverOriginCombatTargetGuid = target->GetGUID();
        _ai.SetCurrentTarget(_serverOriginCombatTargetGuid);
        _serverOriginAttacking.store(true);
        _serverOriginAutoAssistedAttack = autoAssisted;
        _serverOriginActionCheckTimer = 0;
        _serverOriginPathCatchUpActive = false;
        if (priest)
        {
            // A support target is not a melee victim or a new chase owner.
            _player->GetMotionMaster()->Clear(MOTION_SLOT_IDLE);
            _player->GetMotionMaster()->MoveIdle();
            _player->StopMoving();
        }
        else if (ranged)
        {
            if (PlayerbotCombatMovement::CanMove(*_player))
                _player->GetMotionMaster()->MoveChase(target, PlayerbotCombatMovement::GetRange(_ai, "spell"));
        }
        else if (PlayerbotCombatMovement::CanMove(*_player))
        {
            bool tanking = PlayerbotCombatMovement::UsesFrontPosition(
                target->GetVictim() == _player, PlayerbotRoles::IsTank(*_player));
            _serverOriginMeleeStance = tanking ? 2 : 1;
            _player->GetMotionMaster()->MoveChase(target, std::nullopt, ChaseAngle(PlayerbotGroup::MeleeChaseAngle(tanking)));
        }
        TC_LOG_DEBUG("module.playerbots", "PB-02: %s %s %s", _player->GetName().c_str(), autoAssisted ? "auto-assisting" : "attacking", target->GetName().c_str());
        if (_player->getClass() == CLASS_PRIEST)
            PlayerbotPriest::LogKnownAbilities(*_player);
        else if (rotation)
            PlayerbotClassRotation::LogKnownAbilities(*_player);
        else if (ranged)
            PlayerbotMage::LogKnownAbilities(*_player);
        else
            PlayerbotWarrior::LogKnownAbilities(*_player);
        return true;
    };

    uint8 request = _serverOriginCombatRequest.exchange(0);
    if (request == 2)
        cease();
    else if (request == 1)
    {
        uint32 ownerGuidLow = _serverOriginFollowTargetGuidLow.load();
        Player* owner = ownerGuidLow ? _player->GetMap()->GetPlayer(ObjectGuid::Create<HighGuid::Player>(ownerGuidLow)) : nullptr;
        Unit* selected = owner ? owner->GetSelectedUnit() : nullptr;
        Creature* target = selected ? selected->ToCreature() : nullptr;
        if (!validTarget(owner, target))
        {
            TC_LOG_DEBUG("module.playerbots", "PB-02: attack rejected; selected=%s ownerDist=%.1f botTargetDist=%.1f ownerTargetDist=%.1f targetAlive=%u validTarget=%u lineOfSight=%u",
                selected ? selected->GetName().c_str() : "none", owner ? _player->GetDistance(owner) : -1.0f,
                target ? _player->GetDistance(target) : -1.0f, owner && target ? owner->GetDistance(target) : -1.0f,
                target && target->IsAlive(), target && _player->IsValidAttackTarget(target), target && _player->IsWithinLOSInMap(target));
            cease();
        }
        else
        {
            if (!beginAttack(target, false))
                TC_LOG_DEBUG("module.playerbots", "PB-02: attack rejected by core combat rules");
        }
    }

    if (!request && !_serverOriginAttacking.load() && _serverOriginAutoAssistEnabled.load())
    {
        if (_serverOriginAssistCheckTimer > diff)
            _serverOriginAssistCheckTimer -= diff;
        else
        {
            _serverOriginAssistCheckTimer = 500;
            uint32 ownerGuidLow = _serverOriginFollowTargetGuidLow.load();
            Player* owner = ownerGuidLow ? _player->GetMap()->GetPlayer(ObjectGuid::Create<HighGuid::Player>(ownerGuidLow)) : nullptr;
            Unit* selected = owner ? owner->GetSelectedUnit() : nullptr;
            Creature* target = selected ? selected->ToCreature() : nullptr;
            char const* fallback = _engine ? PlayerbotTargetSelection::FallbackValue(_player->getClass(),
                _player->GetPrimaryTalentTree(_player->GetActiveSpec()), PlayerbotModuleEngineWarriorCombatEnabled(),
                PlayerbotModuleEngineMageCombatEnabled(), PlayerbotModuleEnginePriestHealEnabled()) : nullptr;
            bool tank = fallback && std::string_view(fallback) == "tank target";
            if (fallback && (tank || !validTarget(owner, target) || !PlayerbotTargetSelection::IsEngagedWithAttachedParty(*_player, *owner, *target)))
            {
                Value<ObjectGuid>* value = _aiContext ? _aiContext->GetValue<ObjectGuid>(fallback) : nullptr;
                ObjectGuid candidate = value ? value->Get() : ObjectGuid::Empty;
                Creature* resolved = candidate.IsEmpty() ? nullptr : ObjectAccessor::GetCreature(*_player, candidate);
                if (!tank || resolved)
                    target = resolved;
            }
            if (validTarget(owner, target) && PlayerbotTargetSelection::IsEngagedWithAttachedParty(*_player, *owner, *target))
                beginAttack(target, true);
        }
    }

    if (solo && !request && !_serverOriginAttacking.load() && _player->IsAlive() && !_player->IsBeingTeleported())
    {
        // Donor GrindTargetValue: defend first, then pull a fresh target when recovered.
        Creature* target = _player->IsInCombat() ? RandomPlayerbotGrind::SelectAttacker(*_player) : nullptr;
        if (!target && !_player->IsInCombat())
        {
            if (_soloSearchTimer > diff)
                _soloSearchTimer -= diff;
            else
            {
                _soloSearchTimer = RandomPlayerbotGrind::SearchIntervalMs;
                int32 maxMana = _player->GetMaxPower(POWER_MANA);
                bool usesMana = _player->GetPowerType() == POWER_MANA && maxMana > 0;
                float mana = usesMana ? 100.0f * _player->GetPower(POWER_MANA) / maxMana : 100.0f;
                if (!_soloRecovering && !_ai.GetRestSpellId() && !_ai.LootRequests().Pending() &&
                    !_ai.LootPursuit().Active() && !_player->IsNonMeleeSpellCast(false) && !_player->IsMounted() &&
                    RandomPlayerbotGrind::ReadyToPull(false, _player->GetHealthPct(), mana, usesMana))
                {
                    // Mobs an open quest needs first, from further out; none on a journey.
                    if (!_questState.Journeying() && !_questState.Visiting() && !_questState.Voyaging())
                    {
                        target = RandomPlayerbotQuests::SelectQuestTarget(*_player);
                        if (!target)
                            target = RandomPlayerbotGrind::SelectTarget(*_player);
                    }
                }
            }
        }
        if (target)
        {
            if (_player->GetStandState() != UNIT_STAND_STATE_STAND)
                _player->SetStandState(UNIT_STAND_STATE_STAND);
            if (beginAttack(target, false))
            {
                _soloPullStartedMs = getMSTime();
                // Priest opener; later casts come from the healer-dps engine once engaged.
                if (_player->getClass() == CLASS_PRIEST && _player->HasSpell(585))
                    _player->CastSpell(target, 585);
            }
        }
    }

    if (!_serverOriginAttacking.load())
        tickPriest();

    if (!_serverOriginAttacking.load() && _serverOriginFollowTargetGuidLow.load() && !_player->IsInCombat())
    {
        if (_serverOriginBuffCheckTimer > diff)
            _serverOriginBuffCheckTimer -= diff;
        else
        {
            _serverOriginBuffCheckTimer = 2000;
            if (_player->getClass() == CLASS_WARRIOR && _engine &&
                PlayerbotModuleEngineWarriorBuffEnabled())
            {
                if (!_warriorEngineBuffAnnounced)
                {
                    TC_LOG_INFO("module.playerbots", "PB-ENGINE: %s Warrior buff routed through engine", _player->GetName().c_str());
                    _warriorEngineBuffAnnounced = true;
                }
                TickEngine();
            }
            else
                PlayerbotWarrior::MaintainBuff(*_player);
            bool enginePartyBuff = _engine && PlayerbotModuleEnginePartyBuffEnabled() &&
                (_player->getClass() == CLASS_MAGE || _player->getClass() == CLASS_PRIEST);
            if (enginePartyBuff)
            {
                // Priest healing already ticks this same engine at its own cadence.
                // Do not run a second decision or also invoke the direct fallback.
                if (_player->getClass() != CLASS_PRIEST || !PlayerbotModuleEnginePriestHealEnabled())
                    TickEngine();
            }
            else if (Player* owner = _player->GetMap()->GetPlayer(ObjectGuid::Create<HighGuid::Player>(_serverOriginFollowTargetGuidLow.load())))
            {
                PlayerbotMage::MaintainBuff(*_player, *owner);
                PlayerbotPriest::MaintainBuff(*_player, *owner);
            }
        }
    }

    if (_serverOriginAttacking.load())
    {
        Player* owner = combatOwner();
        Creature* target = ObjectAccessor::GetCreature(*_player, _serverOriginCombatTargetGuid);
        bool pullGrace = solo && getMSTimeDiff(_soloPullStartedMs, getMSTime()) < RandomPlayerbotGrind::PullGraceMs;
        bool ownerBeyondLeash = owner && !_player->IsWithinDistInMap(owner, 35.0f);
        bool targetBeyondLeash = target && !_player->IsWithinDistInMap(target, 35.0f);
        if (!owner || !owner->IsAlive() || !target || target->IsControlledByPlayer() || !target->IsAlive() || !_player->IsAlive() ||
            PlayerbotTargetSelection::IsProtectedTarget(*target) ||
            (_player->getClass() != CLASS_PRIEST && !supportHealer && _player->GetVictim() != target) ||
            !_player->IsValidAttackTarget(target) ||
            ((_player->getClass() == CLASS_PRIEST || supportHealer) && (_player->IsBeingTeleported() ||
                (_player->getClass() == CLASS_PRIEST && !PlayerbotModuleEnginePriestHealEnabled()) ||
                (!pullGrace && !PlayerbotTargetSelection::IsEngagedWithAttachedParty(*_player, *owner, *target)))) ||
            (_serverOriginAutoAssistedAttack && !PlayerbotTargetSelection::IsEngagedWithAttachedParty(*_player, *owner, *target)) ||
            ownerBeyondLeash || targetBeyondLeash)
        {
            if (target && !target->IsAlive())
            {
                _ai.RecordDefeatedCreature(target->GetGUID(), getMSTime());
                TC_LOG_DEBUG("module.playerbots", "PB-02: %s target died; returning to follow", _player->GetName().c_str());
            }
            else if (target && PlayerbotTargetSelection::IsProtectedTarget(*target))
                TC_LOG_DEBUG("module.playerbots", "PB-02: %s target protected by control; returning to follow", _player->GetName().c_str());
            else if (ownerBeyondLeash || targetBeyondLeash)
                TC_LOG_DEBUG("module.playerbots", "PB-02: %s exceeded combat leash; returning to follow", _player->GetName().c_str());
            cease();
            tickPriest();
            return;
        }

        if (_player->getClass() == CLASS_PRIEST)
        {
            tickPriest();
            return; // never fall through to the Warrior melee path
        }

        if (hybridHealer)
        {
            tickPriest();
            // Nobody needs healing: help from where the healer stands.
            if (_serverOriginActionCheckTimer > diff)
                _serverOriginActionCheckTimer -= diff;
            else
            {
                _serverOriginActionCheckTimer = 1000;
                if (!_player->IsNonMeleeSpellCast(false) && _serverOriginSupportReachGuid.IsEmpty() &&
                    !PlayerbotHybridHealer::AnyoneNeedsHealing(*_player, _ai.GetController()))
                    PlayerbotClassRotation::Execute(*_player, *target);
            }
            return;
        }

        if (_serverOriginActionCheckTimer > diff)
        {
            _serverOriginActionCheckTimer -= diff;
            return;
        }
        _serverOriginActionCheckTimer = 1000;

        bool supportedDamageRole =
            (_player->getClass() == CLASS_MAGE && PlayerbotModuleEngineMageCombatEnabled()) ||
            (_player->getClass() == CLASS_WARRIOR && PlayerbotModuleEngineWarriorCombatEnabled());
        if (_engine && PlayerbotTargetSelection::ShouldReassessDpsTarget(
            _serverOriginAutoAssistedAttack, supportedDamageRole,
            PlayerbotRoles::IsTankOrMainTank(*_player), _player->IsNonMeleeSpellCast(false)))
        {
            // Donor DpsAssistStrategy / NotDpsTargetActiveTrigger: adopt the
            // newly ranked target rather than waiting for the old one to die.
            // Resolve fresh on the map thread; admission still forbids new pulls.
            ObjectGuid candidateGuid = PlayerbotTargetSelection::SelectDpsTarget(_ai, *_engine);
            Creature* candidate = candidateGuid.IsEmpty() ? nullptr : ObjectAccessor::GetCreature(*_player, candidateGuid);
            if (PlayerbotTargetSelection::DpsTargetChanged(candidate != nullptr, candidate == target) &&
                validTarget(owner, candidate) && PlayerbotTargetSelection::IsEngagedWithAttachedParty(*_player, *owner, *candidate))
            {
                if (!beginAttack(candidate, true))
                    return; // Native Attack rejected the replacement after cease.
                target = candidate;
                _serverOriginActionCheckTimer = 1000;
                TC_LOG_INFO("module.playerbots", "PB-ROLE: %s reassessed DPS target to %s",
                    _player->GetName().c_str(), target->GetName().c_str());
            }
        }

        if (_engine && PlayerbotModuleEngineWarriorCombatEnabled() &&
            _player->getClass() == CLASS_WARRIOR &&
            PlayerbotRoles::IsTank(*_player) &&
            _serverOriginAutoAssistedAttack && !_player->IsNonMeleeSpellCast(false))
        {
            // Fresh native GUID resolution on the map thread. Donor TankTargetValue
            // ranks lost aggro first; retain this adapter's authorized owner combat
            // scope and do not replace explicit attack commands or steal tank aggro.
            ObjectGuid candidateGuid = PlayerbotTargetSelection::SelectTankTarget(_ai, *_engine);
            Creature* candidate = candidateGuid.IsEmpty() ? nullptr : ObjectAccessor::GetCreature(*_player, candidateGuid);
            Player* victim = candidate && candidate->GetVictim() ? candidate->GetVictim()->ToPlayer() : nullptr;
            bool partyVictim = victim && (victim == owner || victim == _player ||
                (_player->GetGroup() && victim->GetGroup() == _player->GetGroup()));
            bool otherTank = victim && PlayerbotRoles::IsTankOrMainTank(*victim);
            if (PlayerbotTargetSelection::ShouldProtectPartyMember(_serverOriginAutoAssistedAttack,
                candidate && candidate != target, partyVictim, victim == _player, otherTank) &&
                validTarget(owner, candidate) && PlayerbotTargetSelection::IsEngagedWithAttachedParty(*_player, *owner, *candidate))
            {
                if (beginAttack(candidate, true))
                {
                    target = candidate;
                    _serverOriginActionCheckTimer = 1000;
                    TC_LOG_INFO("module.playerbots", "PB-ROLE: %s recovering party aggro from %s",
                        _player->GetName().c_str(), victim->GetName().c_str());
                }
                else
                    return; // Native Attack rejected the replacement after cease.
            }
        }

        bool rotation = PlayerbotClassRotation::Supports(_player->getClass());
        if (rotation && PlayerbotClassRotation::FightsAtRange(*_player))
        {
            // Table classes at range cast from their class table; potions stay in the engine.
            if (!PlayerbotClassRotation::Execute(*_player, *target) && _engine)
                TickEngine(true);
            return;
        }

        if (_player->getClass() == CLASS_MAGE)
        {
            if (_ai.NeedsSpellChaseRefresh() && PlayerbotCombatMovement::RefreshSpellChase(_ai))
                _ai.ClearSpellChaseRefresh();
            if (!PlayerbotModuleEngineMageCombatEnabled() && !_player->IsNonMeleeSpellCast(false))
                _player->SetFacingToObject(target);
            if (_engine && PlayerbotModuleEngineMageCombatEnabled())
            {
                uint32 primaryTree = _player->GetPrimaryTalentTree(_player->GetActiveSpec());
                char const* strategy = PlayerbotSpec::CombatStrategy(CLASS_MAGE, primaryTree);
                if (!_mageEngineCombatAnnounced)
                {
                    TC_LOG_INFO("module.playerbots", "PB-ENGINE: %s Mage combat routed through %s strategy", _player->GetName().c_str(), strategy);
                    _mageEngineCombatAnnounced = true;
                }
                TickEngine(true);
            }
            else
                PlayerbotMage::Execute(*_player, *target);
            return;
        }

        bool tanking = PlayerbotCombatMovement::UsesFrontPosition(
            target->GetVictim() == _player, PlayerbotRoles::IsTank(*_player));
        uint8 stance = tanking ? 2 : 1;
        if (stance != _serverOriginMeleeStance && PlayerbotCombatMovement::CanMove(*_player))
        {
            _player->GetMotionMaster()->MoveChase(target, std::nullopt, ChaseAngle(PlayerbotGroup::MeleeChaseAngle(tanking)));
            _serverOriginMeleeStance = stance;
        }

        if (rotation)
        {
            if (!PlayerbotClassRotation::Execute(*_player, *target) && _engine)
                TickEngine(true);
            return;
        }

        if (_engine && PlayerbotModuleEngineWarriorCombatEnabled())
        {
            uint32 primaryTree = _player->GetPrimaryTalentTree(_player->GetActiveSpec());
            char const* strategy = PlayerbotSpec::CombatStrategy(CLASS_WARRIOR, primaryTree);
            if (!_warriorEngineCombatAnnounced)
            {
                TC_LOG_INFO("module.playerbots", "PB-ENGINE: %s Warrior combat routed through %s strategy", _player->GetName().c_str(), strategy);
                _warriorEngineCombatAnnounced = true;
            }
            TickEngine(true);
        }
        else
            PlayerbotWarrior::Execute(*_player, *target);
    }
}

bool PlayerbotSessionBehavior::IsSoloGrinder(Player const& bot) const
{
    return PlayerbotModuleRandomGrindEnabled() && RandomPlayerbotMgr::IsRandomAccount(_session.GetAccountId()) &&
        !_serverOriginFollowTargetGuidLow.load() && !_serverOriginPartyControllerGuidLow.load() &&
        !_serverOriginRecoveryOwnerGuidLow && !bot.GetGroup() && !_ai.IsStaying();
}

void PlayerbotSessionBehavior::UpdateSoloController()
{
    Player* bot = _ai.GetBot();
    if (!bot)
    {
        _soloActive = false;
        return;
    }
    bool solo = IsSoloGrinder(*bot);
    if (_soloHomeReset.exchange(false))
    {
        _soloHomeSet = false; // relocated by the world thread: grind around the new spot
        _questState.Giver.Clear();
        _questState.Object.Clear();
        _questState.ClearSpot();
    }
    if (solo)
    {
        _ai.SetController(bot->GetGUID());
        if (!_soloHomeSet || _soloHomeMap != bot->GetMapId())
        {
            _soloHome = bot->GetPosition();
            _soloHomeMap = bot->GetMapId();
            _soloHomeSet = true;
        }
    }
    else if (_soloActive.load())
    {
        // A human took over (invite or follow); hand control back to the party path.
        if (_ai.GetController() == bot)
            _ai.ClearController();
        // A journey is dropped: the bot goes where its party goes.
        _questState.Trip = {};
        _questState.TripFailed = false;
        RandomPlayerbotCapital::Abort(_questState);
        RandomPlayerbotCrossings::Abort(*bot, _questState);
        RandomPlayerbotQuests::Dismount(*bot, _questState);
        _soloRecovering = false;
        _soloDeadMs = 0;
    }
    _soloActive = solo;
}

void PlayerbotSessionBehavior::UpdateSoloGrind(uint32 diff)
{
    Player* bot = _ai.GetBot();
    if (!bot || !_soloActive.load() || bot->IsBeingTeleported())
        return;

    if (!bot->IsAlive())
    {
        // No spirit healer walk yet: revive in place after a short delay.
        _soloDeadMs += diff;
        if (_soloDeadMs < RandomPlayerbotGrind::ReviveDelayMs)
            return;
        _soloDeadMs = 0;
        bot->ResurrectPlayer(0.5f);
        bot->SpawnCorpseBones();
        _soloRecovering = true;
        _soloWanderTimer = RandomPlayerbotGrind::WanderDelayMs(frand(0.0f, 1.0f));
        TC_LOG_DEBUG("module.playerbots", "PB-RANDOM: %s revived in place", bot->GetName().c_str());
        return;
    }
    _soloDeadMs = 0;

    // A mount taken for questing travel is left as soon as a fight starts.
    if (_questState.Riding && (_serverOriginAttacking.load() || bot->IsInCombat()))
        RandomPlayerbotQuests::Dismount(*bot, _questState);
    if (_serverOriginAttacking.load() || bot->IsInCombat() || _ai.GetRestSpellId() ||
        _ai.LootRequests().Pending() || _ai.LootPursuit().Active() || bot->IsNonMeleeSpellCast(false) ||
        (bot->IsMounted() && !_questState.Riding) || bot->IsInFlight())
        return;

    // A voyage goes on before resting: a bot aboard a ship must get off at its stop.
    if (RandomPlayerbotQuests::Enabled() && RandomPlayerbotCrossings::Update(*bot, _questState, getMSTime()))
        return;

    int32 maximum = bot->GetMaxPower(POWER_MANA);
    bool usesMana = bot->GetPowerType() == POWER_MANA && maximum > 0;
    float mana = usesMana ? 100.0f * bot->GetPower(POWER_MANA) / maximum : 100.0f;
    if (!RandomPlayerbotGrind::ReadyToPull(_soloRecovering, bot->GetHealthPct(), mana, usesMana))
    {
        RandomPlayerbotQuests::Dismount(*bot, _questState);
        if (!_soloRecovering && !bot->isMoving())
        {
            _soloRecovering = true;
            if (bot->GetStandState() == UNIT_STAND_STATE_STAND)
                bot->SetStandState(UNIT_STAND_STATE_SIT);
        }
        return;
    }
    if (_soloRecovering)
    {
        _soloRecovering = false;
        if (bot->GetStandState() == UNIT_STAND_STATE_SIT)
            bot->SetStandState(UNIT_STAND_STATE_STAND);
    }

    // A capital visit comes before questing; quest givers and quest mobs come before idle wandering.
    if (_soloHomeSet && RandomPlayerbotQuests::Enabled() && PlayerbotCombatMovement::CanMove(*bot) &&
        RandomPlayerbotCapital::Update(_session, *bot, _questState, _soloHome, getMSTime()))
        return;
    if (_soloHomeSet && RandomPlayerbotQuests::Enabled() && PlayerbotCombatMovement::CanMove(*bot) &&
        RandomPlayerbotQuests::Update(_session, *bot, _questState, _soloHome, getMSTime()))
        return;

    if (_soloWanderTimer > diff)
    {
        _soloWanderTimer -= diff;
        return;
    }
    _soloWanderTimer = RandomPlayerbotGrind::WanderDelayMs(frand(0.0f, 1.0f));
    if (!PlayerbotCombatMovement::CanMove(*bot) || bot->isMoving() || !_soloHomeSet)
        return;

    Position destination;
    if (!bot->IsWithinDist2d(&_soloHome, RandomPlayerbotGrind::HomeLeash))
        destination = _soloHome;
    else
    {
        float x, y, z;
        bot->GetRandomPoint(_soloHome, RandomPlayerbotGrind::WanderRadius, x, y, z);
        destination.Relocate(x, y, z);
    }
    bot->GetMotionMaster()->MovePoint(0, destination, true);
}

void PlayerbotSessionBehavior::UpdateRandomProgression()
{
    RandomPlayerbotProgression::Settings const& settings = RandomPlayerbotProgression::GetSettings();
    Player* bot = _session.GetPlayer();
    if (!settings.Enabled || !_session.IsServerOrigin() || !bot || !RandomPlayerbotProgression::Ready() ||
        !RandomPlayerbotMgr::IsRandomAccount(_session.GetAccountId()))
        return;

    // A relocation, hearthstone, ship, portal or dungeon-finder teleport to another map needs the client's worldport
    // acknowledgement; a random bot has no client, so every far teleport is acknowledged here.
    if (bot->IsBeingTeleportedFar())
    {
        _progressFarTeleport = false;
        _session.HandleMoveWorldportAck();
        _soloHomeReset = true;
        return;
    }
    _progressFarTeleport = false;
    // Gilneas, Kezan and Acherus keep a bot in fights with their scripted NPCs; leaving them must not wait for peace.
    bool phasedStart = RandomPlayerbotProgression::InPhasedStart(*bot);
    if (!bot->IsInWorld() || bot->IsBeingTeleported() || !bot->IsAlive() || (bot->IsInCombat() && !phasedStart))
        return;
    uint32 now = getMSTime();
    if (getMSTimeDiff(_progressCheckMs, now) < 1000)
        return;
    _progressCheckMs = now;

    uint32 level = bot->getLevel();
    if (!_progressChecked)
    {
        _progressChecked = true;
        uint32 target = RandomPlayerbotLevels::TargetLevelFor(bot->GetGUID().GetCounter(), bot->getRace(),
            bot->getClass(), settings.MinLevel, settings.MaxLevel);
        if (!bot->GetGroup() && RandomPlayerbotLevels::NeedsInitialization(level, target,
            RandomPlayerbotLevels::MinimumLevel(bot->getRace(), bot->getClass())))
        {
            RandomPlayerbotProgression::Initialize(*bot, target);
            _progressRelocateMs = 0; // place it right away
        }
        _progressLevel = bot->getLevel();
        level = _progressLevel;
    }
    else if (level != _progressLevel)
    {
        if (level > _progressLevel)
            RandomPlayerbotProgression::ApplyLevelUp(*bot, _progressLevel);
        _progressLevel = level;
    }

    // Death knights, Worgen and Goblins leave Acherus, Gilneas and Kezan for their capital first.
    if (!bot->GetGroup() && level > RandomPlayerbotLevels::StartZoneLevel && phasedStart)
    {
        if (_progressRelocateMs && getMSTimeDiff(_progressRelocateMs, now) < 60000)
            return;
        _progressRelocateMs = now ? now : 1;
        if (bot->IsInCombat())
            bot->CombatStop(true);
        if (RandomPlayerbotProgression::MoveToCapital(*bot))
        {
            _progressFarTeleport = bot->IsBeingTeleportedFar();
            _soloHomeReset = true;
            _progressRelocateMs = 0; // move on to a grind spot once there
        }
        return;
    }

    if (bot->IsInCombat())
        return;

    // Hunters created without a pet tame one, so Call Pet and Revive Pet have something to bring.
    if (!_hunterPetChecked && !bot->IsMounted() && !bot->IsInFlight())
    {
        _hunterPetChecked = true;
        RandomPlayerbotProgression::EnsureHunterPet(*bot);
    }
    if (getMSTimeDiff(_hunterPetKeptMs, now) >= 10000)
    {
        _hunterPetKeptMs = now;
        RandomPlayerbotProgression::KeepHunterPet(*bot);
    }

    // A journey that got stuck ends with a teleport to its goal; one under way is left alone.
    if (_questState.TripFailed.load())
    {
        if (RandomPlayerbotQuests::FinishFailedTrip(*bot, _questState))
            _progressFarTeleport = bot->IsBeingTeleportedFar();
        _soloHomeReset = true;
        return;
    }
    // A bot in its capital stays until it sets out again; one on a voyage until it arrives.
    if (_questState.Journeying() || _questState.Visiting() || _questState.Voyaging())
        return;

    // A questing bot with nothing left around it moves on to another quest giver.
    if (_questState.WantsRelocation.exchange(false) && _soloActive.load() && !bot->GetGroup() &&
        level > RandomPlayerbotLevels::StartZoneLevel && RandomPlayerbotQuests::RelocateToGiver(*bot, &_questState))
    {
        _progressFarTeleport = bot->IsBeingTeleportedFar();
        _soloHomeReset = true;
        return;
    }

    // Only ungrouped grinders move; start-zone levels stay where they were created.
    if (!_soloActive.load() || bot->GetGroup() || level <= RandomPlayerbotLevels::StartZoneLevel ||
        RandomPlayerbotProgression::InFittingZone(*bot))
        return;
    if (_progressRelocateMs && getMSTimeDiff(_progressRelocateMs, now) < 60000)
        return;
    _progressRelocateMs = now ? now : 1;
    if (RandomPlayerbotProgression::Relocate(*bot, &_questState))
    {
        _progressFarTeleport = bot->IsBeingTeleportedFar();
        _soloHomeReset = true;
    }
}
