/*
 * Random population manager, adapted from mod-playerbots RandomPlayerbotMgr
 * (AddRandomBots/ProcessBot) and RandomPlayerbotFactory::CreateRandomBots at
 * d750b4b. See AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "RandomPlayerbotMgr.h"
#include "RandomPlayerbotFactory.h"
#include "RandomPlayerbotProgression.h"
#include "NativeCharacterCreationReceipt.h"
#include "AccountMgr.h"
#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "../../Script/PlayerbotConfig.h"
#include "ObjectMgr.h"
#include "Random.h"
#include "World.h"
#include "WorldSession.h"
#include "Player.h"
#include <memory>
#include <shared_mutex>
#include <unordered_set>
#include <vector>

namespace
{
    // Per interval: native account inserts are synchronous, keep them few.
    constexpr uint32 AccountsPerInterval = 25;
    // Leave native provisioning contexts (16) for the console factory.
    constexpr uint32 MaxProvisioningInFlight = 8;
    constexpr uint32 MaxNameAttempts = 20;
    constexpr uint32 FailureBackoffIntervals = 6;

    struct RandomBotSlot
    {
        uint32 AccountId = 0;
        uint32 GuidLow = 0;
        std::shared_ptr<NativeCharacterCreationReceipt const> Receipt;
        uint32 SkipIntervals = 0;
        uint32 Failures = 0;
    };

    RandomPlayerbotSettings Settings;
    std::vector<RandomBotSlot> Slots;
    uint32 Timer = 0;
    bool Paused = false;
    bool ResetPending = false;
    bool ResumeAfterReset = false;
    bool GateWarningLogged = false;
    uint32 LastOnline = 0;
    uint32 LastProvisioning = 0;

    // Read from map threads (solo behavior), written on the world thread.
    std::shared_mutex RandomAccountsLock;
    std::unordered_set<uint32> RandomAccounts;

    void RegisterRandomAccount(uint32 accountId)
    {
        std::unique_lock lock(RandomAccountsLock);
        RandomAccounts.insert(accountId);
    }

    uint32 Roll(uint32 bound) { return bound ? urand(0, bound - 1) : 0; }

    std::string RollPassword()
    {
        static constexpr char Alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
        std::string password;
        for (int i = 0; i < 16; ++i)
            password.push_back(Alphabet[Roll(sizeof(Alphabet) - 1)]);
        return password;
    }

    bool ResolveAccount(RandomBotSlot& slot, std::string const& name, uint32& createdThisInterval)
    {
        if ((slot.AccountId = sAccountMgr->GetId(name)))
        {
            RegisterRandomAccount(slot.AccountId);
            return true;
        }
        if (createdThisInterval >= AccountsPerInterval)
            return false;
        ++createdThisInterval;
        // The password is never stored: the account is not usable for human login.
        if (sAccountMgr->CreateAccount(name, RollPassword()) != AccountOpResult::AOR_OK)
        {
            TC_LOG_ERROR("module.playerbots", "Random playerbots: could not create account %s.", name.c_str());
            return false;
        }
        slot.AccountId = sAccountMgr->GetId(name);
        if (slot.AccountId)
            RegisterRandomAccount(slot.AccountId);
        return slot.AccountId != 0;
    }

    // Adopt an existing character (previous run) or submit one native creation.
    void ResolveCharacter(RandomBotSlot& slot, uint32& provisioning)
    {
        if (slot.Receipt)
        {
            using State = NativeCharacterCreationReceipt::State;
            switch (slot.Receipt->GetState())
            {
                case State::Pending:
                case State::Reconciling:
                    ++provisioning;
                    return;
                case State::Succeeded:
                    slot.GuidLow = slot.Receipt->GetCharacterGuidLow();
                    slot.Receipt.reset();
                    return;
                default:
                    // Name taken, accounting failed or abandoned: re-read the account next interval.
                    TC_LOG_DEBUG("module.playerbots", "Random playerbots: creation for account %u ended with result %u.",
                        slot.AccountId, slot.Receipt->GetNativeResult());
                    slot.Receipt.reset();
                    ++slot.Failures;
                    slot.SkipIntervals = 1;
                    return;
            }
        }

        QueryResult existing = CharacterDatabase.PQuery("SELECT guid FROM characters WHERE account=%u ORDER BY guid LIMIT 1", slot.AccountId);
        if (existing)
        {
            slot.GuidLow = (*existing)[0].GetUInt32();
            return;
        }
        if (provisioning >= MaxProvisioningInFlight || sWorld->IsCharacterProvisioningAccount(slot.AccountId))
            return;

        for (uint32 attempt = 0; attempt < MaxNameAttempts; ++attempt)
        {
            RandomPlayerbotProfile profile = RollRandomPlayerbotProfile(Settings.AllianceRatio, Roll, Settings.ClassMask);
            std::string failure;
            auto appearance = RandomPlayerbotFactory::PreviewAppearance(profile.Race, profile.Class, profile.Gender, failure);
            if (!appearance)
                continue;
            std::string name = RollRandomPlayerbotName(Roll);
            if (!normalizePlayerName(name) ||
                ObjectMgr::CheckPlayerName(name, sWorld->GetDefaultDbcLocale(), true) != CHAR_NAME_SUCCESS ||
                !sCharacterCache->GetCharacterGuidByName(name).IsEmpty())
                continue;
            CharacterCreateInfo request(name, profile.Race, profile.Class, profile.Gender,
                appearance->Skin, appearance->Face, appearance->HairStyle, appearance->HairColor, appearance->FacialHair);
            slot.Receipt = sWorld->BeginCharacterProvisioning(slot.AccountId, request);
            if (slot.Receipt)
                ++provisioning;
            else
                slot.SkipIntervals = 1;
            return;
        }
    }

    void StopAll(std::size_t fromIndex)
    {
        for (std::size_t i = fromIndex; i < Slots.size(); ++i)
            if (Slots[i].GuidLow)
                sWorld->RequestStopServerOriginPlayerbot(ObjectGuid::Create<HighGuid::Player>(Slots[i].GuidLow));
    }
}

namespace
{
    // Prefix plus digits only, so accounts that merely start with the prefix are never touched.
    bool IsRandomAccountName(std::string const& name)
    {
        std::string const& prefix = Settings.AccountPrefix;
        if (name.size() <= prefix.size() || name.compare(0, prefix.size(), prefix) != 0)
            return false;
        for (std::size_t i = prefix.size(); i < name.size(); ++i)
            if (name[i] < '0' || name[i] > '9')
                return false;
        return true;
    }

    // Runs once every random bot has logged out. Uses the native permanent delete,
    // which also removes items, pets, mail and the character cache entry.
    void FinishReset()
    {
        uint32 deleted = 0, accounts = 0;
        QueryResult result = LoginDatabase.PQuery("SELECT id, username FROM account WHERE username LIKE '%s%%'",
            Settings.AccountPrefix.c_str());
        if (result)
        {
            do
            {
                uint32 accountId = (*result)[0].GetUInt32();
                if (!IsRandomAccountName((*result)[1].GetString()))
                    continue;
                ++accounts;
                QueryResult characters = CharacterDatabase.PQuery("SELECT guid FROM characters WHERE account=%u", accountId);
                if (!characters)
                    continue;
                do
                {
                    ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>((*characters)[0].GetUInt32());
                    Player::DeleteFromDB(guid, accountId, true, true);
                    ++deleted;
                } while (characters->NextRow());
            } while (result->NextRow());
        }
        // The deletes commit asynchronously; wait two passes before re-reading the accounts.
        for (RandomBotSlot& slot : Slots)
        {
            slot = RandomBotSlot{};
            slot.SkipIntervals = 2;
        }
        ResetPending = false;
        Paused = !ResumeAfterReset;
        TC_LOG_INFO("server.worldserver", "Random playerbots: deleted %u characters on %u %s accounts; new ones are created on the next passes.",
            deleted, accounts, Settings.AccountPrefix.c_str());
    }

    bool AnyRandomBotOnline()
    {
        for (RandomBotSlot const& slot : Slots)
            if ((slot.GuidLow && sWorld->FindServerOriginPlayerbot(ObjectGuid::Create<HighGuid::Player>(slot.GuidLow))) ||
                (slot.AccountId && sWorld->FindSession(slot.AccountId)) || (slot.Receipt &&
                (slot.Receipt->GetState() == NativeCharacterCreationReceipt::State::Pending ||
                 slot.Receipt->GetState() == NativeCharacterCreationReceipt::State::Reconciling)))
                return true;
        return false;
    }
}

void RandomPlayerbotMgr::RequestReset()
{
    ResumeAfterReset = !Paused;
    ResetPending = true;
    Paused = true;
    StopAll(0);
    Timer = Settings.IntervalMs;
}

void RandomPlayerbotMgr::Configure(RandomPlayerbotSettings const& settings)
{
    Settings = ClampRandomPlayerbotSettings(settings);
    if (auto prefix = NormalizeRandomPlayerbotPrefix(Settings.AccountPrefix))
        Settings.AccountPrefix = *prefix;
    else
    {
        TC_LOG_ERROR("module.playerbots", "Playerbots.Random.AccountPrefix must be 1-%u letters or digits; random playerbots disabled.",
            uint32(MaxRandomPlayerbotPrefix));
        Settings.Enabled = false;
    }
    // Run the first pass on the next world update.
    Timer = Settings.IntervalMs;
    GateWarningLogged = false;
}

void RandomPlayerbotMgr::SetPaused(bool paused)
{
    if (ResetPending)
    {
        ResumeAfterReset = !paused; // applies once the reset is done
        return;
    }
    Paused = paused;
    if (paused)
        StopAll(0);
    else
        Timer = Settings.IntervalMs;
}

void RandomPlayerbotMgr::Update(uint32_t diff)
{
    if (!Settings.Enabled)
        return;
    Timer += diff;
    if (Timer < Settings.IntervalMs)
        return;
    Timer = 0;
    if (sWorld->IsStopped() || sWorld->IsShuttingDown())
        return;
    if (ResetPending)
    {
        if (AnyRandomBotOnline())
            StopAll(0);
        else
            FinishReset();
        return;
    }
    if (Paused)
        return;
    // Server-origin admission is gated by the managed (or dev) switch in the core.
    if (!sWorld->getBoolConfig(CONFIG_PLAYERBOTS_MANAGED_ENABLED) && !sWorld->getBoolConfig(CONFIG_PLAYERBOTS_DEV_ENABLED))
    {
        if (!GateWarningLogged)
            TC_LOG_ERROR("module.playerbots", "Random playerbots need Playerbots.Managed.Enabled = 1; no bots admitted.");
        GateWarningLogged = true;
        return;
    }

    // Levelling indexes before the first admission, so bots are prepared on login.
    if (RandomPlayerbotProgression::GetSettings().Enabled)
        RandomPlayerbotProgression::BuildIndexes();

    if (Slots.size() > Settings.Count)
        StopAll(Settings.Count);
    Slots.resize(Settings.Count);

    uint32 created = 0, provisioning = 0, online = 0;
    std::vector<RandomBotSlot*> ready;
    for (std::size_t i = 0; i < Slots.size(); ++i)
    {
        RandomBotSlot& slot = Slots[i];
        if (slot.SkipIntervals)
        {
            --slot.SkipIntervals;
            continue;
        }
        if (!slot.AccountId && !ResolveAccount(slot, RandomPlayerbotAccountName(Settings.AccountPrefix, uint32(i + 1)), created))
            continue;
        if (!slot.GuidLow)
        {
            ResolveCharacter(slot, provisioning);
            if (!slot.GuidLow)
                continue;
        }
        if (sWorld->FindServerOriginPlayerbot(ObjectGuid::Create<HighGuid::Player>(slot.GuidLow)))
            ++online;
        else if (!sWorld->FindSession(slot.AccountId))
            ready.push_back(&slot);
    }

    uint32 budget = RandomPlayerbotLoginBudget(online, 0, uint32(ready.size()), Settings.Count, Settings.LoginPerInterval);
    for (uint32 i = 0; i < budget; ++i)
    {
        RandomBotSlot& slot = *ready[i];
        if (sWorld->TryStartServerOriginPlayerbot(slot.AccountId, ObjectGuid::Create<HighGuid::Player>(slot.GuidLow)))
        {
            ++online;
            slot.Failures = 0;
        }
        else
        {
            ++slot.Failures;
            slot.SkipIntervals = FailureBackoffIntervals;
            TC_LOG_DEBUG("module.playerbots", "Random playerbots: admission of character %u (account %u) rejected.",
                slot.GuidLow, slot.AccountId);
        }
    }
    LastOnline = online;
    LastProvisioning = provisioning;
}

std::string RandomPlayerbotMgr::Describe()
{
    uint32 accounts = 0, characters = 0, failing = 0;
    for (RandomBotSlot const& slot : Slots)
    {
        accounts += slot.AccountId ? 1 : 0;
        characters += slot.GuidLow ? 1 : 0;
        failing += slot.Failures ? 1 : 0;
    }
    std::string state = !Settings.Enabled ? "disabled" : ResetPending ? "resetting" : Paused ? "paused" : "running";
    return "Random playerbots " + state + ": target " + std::to_string(Settings.Count) +
        ", accounts " + std::to_string(accounts) + ", characters " + std::to_string(characters) +
        ", creating " + std::to_string(LastProvisioning) + ", online " + std::to_string(LastOnline) +
        ", with failures " + std::to_string(failing) +
        ", grinding " + (PlayerbotModuleRandomGrindEnabled() ? "on" : "off") +
        ", levels " + (RandomPlayerbotProgression::GetSettings().Enabled ?
            std::to_string(RandomPlayerbotProgression::GetSettings().MinLevel) + "-" +
            std::to_string(RandomPlayerbotProgression::GetSettings().MaxLevel) : std::string("off")) + ".";
}

bool RandomPlayerbotMgr::IsRandomAccount(uint32_t accountId)
{
    std::shared_lock lock(RandomAccountsLock);
    return RandomAccounts.count(accountId) != 0;
}
