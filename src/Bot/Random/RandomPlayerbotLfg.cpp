/*
 * Random-population bots in the dungeon finder. The donor mod-playerbots
 * queues its random bots for the dungeons real players wait for
 * (RandomPlayerbotMgr::CheckLfgQueue, LfgActions); here the bots fill exactly
 * the roles a human's queue lacks, and a bot is lifted to the human's level
 * when none is close enough. Released under GNU GPL v2 or any later version.
 */
#include "RandomPlayerbotLfg.h"
#include "RandomPlayerbotLfgPolicy.h"
#include "RandomPlayerbotDungeonPolicy.h"
#include "RandomPlayerbotMgr.h"
#include "RandomPlayerbotProgression.h"
#include "../../Ai/Base/PlayerbotHealerKit.h"
#include "CharacterCache.h"
#include "DBCStores.h"
#include "Group.h"
#include "LFGMgr.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "Player.h"
#include "Timer.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <atomic>
#include <map>
#include <unordered_map>

using namespace RandomPlayerbotLfgPolicy;

namespace
{
// A human queue: one solo player or a group with at least one human.
struct HumanQueue
{
    ObjectGuid Key;
    uint32 Team = 0;
    uint32 Level = 0;
    std::vector<uint8_t> HumanRoles;
    std::vector<uint8_t> BotRoles; // bots already in the human's group
    lfg::LfgDungeonSet Dungeons;
};

struct QueuedBot
{
    ObjectGuid Key;
    uint8_t Role = 0;
};

std::unordered_map<ObjectGuid, QueuedBot> Queued;
std::unordered_map<ObjectGuid, uint32> RefusedUntil;
std::map<ObjectGuid, uint32> LastLift;
// Dungeon groups whose humans left or went offline, since when.
std::unordered_map<ObjectGuid, uint32> HumansGoneSince;
std::unordered_map<ObjectGuid, uint32> AwaySince;
std::atomic<uint32> QueuedCount { 0 }, GroupCount { 0 }, InDungeonCount { 0 };
std::atomic<uint32> JoinCount { 0 }, LiftCount { 0 }, RefusedCount { 0 }, LeftCount { 0 };
// Death in a dungeon: since when a bot lies dead, since when its group is out of combat, since when it runs back.
std::unordered_map<ObjectGuid, uint32> DeadSince, QuietSince, GhostSince, NextGhostMove;
std::unordered_map<ObjectGuid, uint32> Deaths; // per bot, varies its release moment
std::vector<ObjectGuid> WipedGroups;
std::atomic<uint32> DeathCount { 0 }, ReleaseCount { 0 }, WipeCount { 0 }, ReturnCount { 0 }, FetchCount { 0 };

bool IsHuman(ObjectGuid guid)
{
    if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
        return !player->GetSession()->IsServerOrigin();
    uint32 account = sCharacterCache->GetCharacterAccountIdByGuid(guid);
    return account && !RandomPlayerbotMgr::IsRandomAccount(account);
}

bool Due(std::unordered_map<ObjectGuid, uint32> const& until, ObjectGuid guid, uint32 now)
{
    auto it = until.find(guid);
    return it == until.end() || int32(it->second - now) <= 0;
}

// Not grouped, not busy with a fight, a flight, a ship or a battleground, and in the open world.
bool Free(Player& bot)
{
    return bot.IsInWorld() && !bot.IsBeingTeleported() && bot.IsAlive() && !bot.IsInCombat() && !bot.GetGroup() &&
        !bot.GetGroupInvite() && !bot.IsInFlight() && !bot.GetTransport() && !bot.GetMap()->Instanceable() &&
        !bot.InBattleground() && !bot.InBattlegroundQueue() && !RandomPlayerbotProgression::InPhasedStart(bot) &&
        sLFGMgr->GetState(bot.GetGUID()) == lfg::LFG_STATE_NONE;
}

std::vector<HumanQueue> HumanQueues()
{
    std::vector<HumanQueue> queues;
    std::vector<ObjectGuid> seen;
    for (auto const& [account, session] : sWorld->GetAllSessions())
    {
        Player* player = session ? session->GetPlayer() : nullptr;
        if (!player || session->IsServerOrigin() || !player->IsInWorld())
            continue;
        Group* group = player->GetGroup();
        ObjectGuid key = group ? group->GetGUID() : player->GetGUID();
        if (std::find(seen.begin(), seen.end(), key) != seen.end() || sLFGMgr->GetState(key) != lfg::LFG_STATE_QUEUED)
            continue;
        seen.push_back(key);
        HumanQueue queue;
        queue.Key = key;
        queue.Team = player->GetTeam();
        queue.Dungeons = sLFGMgr->GetSelectedDungeons(player->GetGUID());
        if (group)
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                if (Player* member = ref->GetSource())
                {
                    uint8_t roles = sLFGMgr->GetRoles(member->GetGUID());
                    if (member->GetSession()->IsServerOrigin())
                        queue.BotRoles.push_back(roles);
                    else
                    {
                        queue.HumanRoles.push_back(roles);
                        queue.Level = std::max<uint32>(queue.Level, member->getLevel());
                    }
                }
        }
        else
        {
            queue.HumanRoles.push_back(sLFGMgr->GetRoles(player->GetGUID()));
            queue.Level = player->getLevel();
        }
        if (!queue.Dungeons.empty())
            queues.push_back(std::move(queue));
    }
    return queues;
}

bool Join(Player& bot, HumanQueue const& queue, uint8_t role, uint32 now)
{
    lfg::LfgDungeonSet dungeons = queue.Dungeons;
    sLFGMgr->JoinLfg(&bot, role, dungeons, "");
    if (sLFGMgr->GetState(bot.GetGUID()) != lfg::LFG_STATE_QUEUED)
    {
        RefusedUntil[bot.GetGUID()] = now + RefusedRetryMs;
        ++RefusedCount;
        TC_LOG_DEBUG("module.playerbots", "PB-LFG: dungeon finder refused %s (level %u) as role %u", bot.GetName().c_str(),
            bot.getLevel(), uint32(role));
        return false;
    }
    Queued[bot.GetGUID()] = { queue.Key, role };
    ++JoinCount;
    TC_LOG_INFO("module.playerbots", "PB-LFG: %s (level %u) queued as role %u for a human's dungeon", bot.GetName().c_str(),
        bot.getLevel(), uint32(role));
    return true;
}

// Queues one bot for the role: one close to the human's level, else the highest lower one lifted to it.
bool Fill(std::vector<Player*> const& bots, HumanQueue const& queue, uint8_t role, uint32 now)
{
    Player* closest = nullptr;
    Player* liftable = nullptr;
    for (Player* bot : bots)
    {
        if (!bot || bot->GetTeam() != queue.Team || RandomPlayerbotProgression::DungeonRole(*bot) != role ||
            !Due(RefusedUntil, bot->GetGUID(), now) || Queued.count(bot->GetGUID()) || !Free(*bot))
            continue;
        if (JoinsAsIs(bot->getLevel(), queue.Level))
        {
            if (!closest || LevelGap(bot->getLevel(), queue.Level) < LevelGap(closest->getLevel(), queue.Level))
                closest = bot;
        }
        else if (Liftable(bot->getLevel(), queue.Level) && (!liftable || bot->getLevel() > liftable->getLevel()))
            liftable = bot;
    }
    if (closest)
        return Join(*closest, queue, role, now);
    auto last = LastLift.find(queue.Key);
    if (!liftable || (last != LastLift.end() && getMSTimeDiff(last->second, now) < LiftEveryMs))
        return false;
    LastLift[queue.Key] = now;
    TC_LOG_INFO("module.playerbots", "PB-LFG: lifting %s from level %u to %u for a missing role %u", liftable->GetName().c_str(),
        liftable->getLevel(), queue.Level, uint32(role));
    RandomPlayerbotProgression::Initialize(*liftable, queue.Level);
    ++LiftCount;
    return Join(*liftable, queue, role, now);
}

// Bots waiting for a queue that is gone leave it; proposals and role checks are answered.
void Answer(std::vector<Player*> const& bots, std::vector<HumanQueue> const& queues)
{
    // A group of bots alone is turned down: bots only go along with a human.
    bool humanProposed = false;
    for (auto const& [account, session] : sWorld->GetAllSessions())
        if (Player* player = session ? session->GetPlayer() : nullptr)
            if (!session->IsServerOrigin() && sLFGMgr->GetState(player->GetGUID()) == lfg::LFG_STATE_PROPOSAL)
                humanProposed = true;
    for (Player* bot : bots)
    {
        if (!bot)
            continue;
        ObjectGuid guid = bot->GetGUID();
        lfg::LfgState state = sLFGMgr->GetState(guid);
        if (state == lfg::LFG_STATE_PROPOSAL)
        {
            if (uint32 proposal = sLFGMgr->GetPendingProposal(guid))
            {
                sLFGMgr->UpdateProposal(proposal, guid, humanProposed);
                TC_LOG_INFO("module.playerbots", "PB-LFG: %s %s dungeon proposal %u", bot->GetName().c_str(),
                    humanProposed ? "accepted" : "declined (no human in it)", proposal);
            }
            continue;
        }
        if (state == lfg::LFG_STATE_ROLECHECK)
        {
            if (Group* group = bot->GetGroup())
                sLFGMgr->UpdateRoleCheck(group->GetGUID(), guid, RandomPlayerbotProgression::DungeonRole(*bot));
            continue;
        }
        auto queued = Queued.find(guid);
        if (queued == Queued.end())
            continue;
        if (state != lfg::LFG_STATE_QUEUED)
        {
            Queued.erase(queued); // grouped, or the core dropped it
            continue;
        }
        bool wanted = std::any_of(queues.begin(), queues.end(),
            [&](HumanQueue const& queue) { return queue.Key == queued->second.Key; });
        if (!wanted)
        {
            sLFGMgr->LeaveLfg(guid);
            Queued.erase(queued);
            TC_LOG_INFO("module.playerbots", "PB-LFG: %s left the queue; its human stopped waiting", bot->GetName().c_str());
        }
    }
    // Bots that logged out drop out of the bookkeeping.
    for (auto it = Queued.begin(); it != Queued.end();)
        it = ObjectAccessor::FindConnectedPlayer(it->first) ? std::next(it) : Queued.erase(it);
}

// In a dungeon group: a human leads, bots that missed the teleport are sent in, and once the humans are gone the
// bots leave the dungeon and the group.
void Accompany(std::vector<Player*> const& bots, uint32 now)
{
    uint32 groups = 0, inDungeon = 0;
    std::vector<Group*> counted;
    for (Player* bot : bots)
    {
        Group* group = bot ? bot->GetGroup() : nullptr;
        if (!group || !group->isLFGGroup() || bot->IsBeingTeleported() || !bot->IsInWorld())
            continue;
        if (std::find(counted.begin(), counted.end(), group) == counted.end())
        {
            counted.push_back(group);
            ++groups;
        }
        ObjectGuid guid = bot->GetGUID();
        Player* online = nullptr;
        bool anyHuman = false;
        for (Group::MemberSlot const& slot : group->GetMemberSlots())
            if (IsHuman(slot.guid))
            {
                anyHuman = true;
                Player* human = ObjectAccessor::FindConnectedPlayer(slot.guid);
                if (human && (!online || slot.guid == group->GetLeaderGUID()))
                    online = human;
            }
        uint32 dungeonMap = sLFGMgr->GetDungeonMapId(group->GetGUID());
        bool inside = dungeonMap && bot->GetMapId() == dungeonMap;
        inDungeon += inside ? 1 : 0;

        if (online)
        {
            HumansGoneSince.erase(guid);
            if (!IsHuman(group->GetLeaderGUID()))
                group->ChangeLeader(online->GetGUID());
            // The human is in the dungeon and the bot is not: it missed the group's teleport.
            bool away = dungeonMap && online->GetMapId() == dungeonMap && !inside;
            if (!away)
                AwaySince.erase(guid);
            else if (!AwaySince.count(guid))
                AwaySince[guid] = now;
            else if (getMSTimeDiff(AwaySince[guid], now) >= ArriveMs && bot->IsAlive() && !bot->IsInCombat())
            {
                AwaySince.erase(guid);
                sLFGMgr->TeleportPlayer(bot, false);
                TC_LOG_INFO("module.playerbots", "PB-LFG: %s sent into its group's dungeon", bot->GetName().c_str());
            }
            continue;
        }
        auto gone = HumansGoneSince.find(guid);
        if (gone == HumansGoneSince.end())
        {
            HumansGoneSince[guid] = now;
            continue;
        }
        if (getMSTimeDiff(gone->second, now) < (anyHuman ? HumanOfflineMs : HumanLeftMs) || bot->IsInCombat())
            continue;
        HumansGoneSince.erase(gone);
        AwaySince.erase(guid);
        if (!bot->IsAlive())
        {
            bot->ResurrectPlayer(1.0f);
            bot->SpawnCorpseBones();
        }
        sLFGMgr->TeleportPlayer(bot, true);
        group->RemoveMember(guid);
        ++LeftCount;
        TC_LOG_INFO("module.playerbots", "PB-LFG: %s left its dungeon group; no human is left in it", bot->GetName().c_str());
    }
    GroupCount = groups;
    InDungeonCount = inDungeon;
}

// The dungeon entrance on the ghost's continent: the area trigger that leads into the group's dungeon.
struct Entrance
{
    AreaTriggerEntry const* Trigger = nullptr;
    AreaTriggerStruct const* Inside = nullptr;
};

Entrance EntranceFor(Player const& ghost, uint32 dungeonMap)
{
    Entrance best;
    float bestDistance = 0.0f;
    for (uint32 i = 0; i < sAreaTriggerStore.GetNumRows(); ++i)
    {
        AreaTriggerEntry const* trigger = sAreaTriggerStore.LookupEntry(i);
        if (!trigger || trigger->ContinentID != ghost.GetMapId())
            continue;
        AreaTriggerStruct const* inside = sObjectMgr->GetAreaTrigger(trigger->ID);
        if (!inside || inside->target_mapId != dungeonMap)
            continue;
        float distance = ghost.GetDistance(trigger->Pos.X, trigger->Pos.Y, trigger->Pos.Z);
        if (!best.Trigger || distance < bestDistance)
        {
            best = { trigger, inside };
            bestDistance = distance;
        }
    }
    return best;
}

bool AtEntrance(Player const& ghost, AreaTriggerEntry const& trigger)
{
    using RandomPlayerbotDungeonPolicy::EntranceReach;
    if (trigger.Radius > 0.0f)
        return ghost.GetDistance(trigger.Pos.X, trigger.Pos.Y, trigger.Pos.Z) <= trigger.Radius + EntranceReach;
    float half = std::max(trigger.Box_length, trigger.Box_width) / 2.0f;
    return ghost.GetExactDist2d(trigger.Pos.X, trigger.Pos.Y) <= half + EntranceReach &&
        std::abs(ghost.GetPositionZ() - trigger.Pos.Z) <= trigger.Box_height / 2.0f + EntranceReach;
}

// Into the dungeon at its entrance; a ghost whose corpse lies there comes back to life on arrival.
bool EnterDungeon(Player& ghost, uint32 dungeonMap, AreaTriggerStruct const* inside)
{
    if (!inside)
        inside = sObjectMgr->GetMapEntranceTrigger(dungeonMap);
    return inside && ghost.TeleportTo(inside->target_mapId, inside->target_X, inside->target_Y, inside->target_Z,
        inside->target_Orientation);
}

// Dead bots of a dungeon group with a human: wait for a resurrection, release the spirit, run back from the
// graveyard as a ghost and come in again at the entrance, like players do. After a wipe everybody runs back.
void Recover(std::vector<Player*> const& bots, uint32 now)
{
    using namespace RandomPlayerbotDungeonPolicy;
    std::vector<ObjectGuid> wiped;
    for (Player* bot : bots)
    {
        if (!bot)
            continue;
        ObjectGuid guid = bot->GetGUID();
        Group* group = bot->GetGroup();
        uint32 dungeonMap = group && group->isLFGGroup() ? sLFGMgr->GetDungeonMapId(group->GetGUID()) : 0;
        if (!dungeonMap || bot->IsAlive())
        {
            DeadSince.erase(guid);
            QuietSince.erase(guid);
            GhostSince.erase(guid);
            NextGhostMove.erase(guid);
            continue;
        }
        if (!bot->IsInWorld() || bot->IsBeingTeleported())
            continue;

        bool humanOnline = false, humanReleased = false, anyoneAlive = false, fighting = false, canResurrect = false;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsInWorld())
                continue;
            bool human = !member->GetSession()->IsServerOrigin();
            humanOnline = humanOnline || human;
            humanReleased = humanReleased || (human && member->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST));
            if (member->GetMapId() != dungeonMap || !member->IsAlive())
                continue;
            anyoneAlive = true;
            fighting = fighting || member->IsInCombat();
            uint32 resurrection = PlayerbotHealerKit::For(member->getClass()).Resurrect;
            canResurrect = canResurrect || (resurrection && member->HasSpell(resurrection));
        }
        if (!humanOnline)
            continue; // the bots leave with the group (Accompany)

        if (!bot->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
        {
            // Lying dead in the dungeon.
            if (bot->GetMapId() != dungeonMap || bot->getDeathState() != CORPSE)
                continue;
            if (!DeadSince.count(guid))
            {
                DeadSince[guid] = now;
                ++Deaths[guid];
                ++DeathCount;
            }
            if (!anyoneAlive)
            {
                wiped.push_back(group->GetGUID());
                if (std::find(WipedGroups.begin(), WipedGroups.end(), group->GetGUID()) == WipedGroups.end())
                {
                    WipedGroups.push_back(group->GetGUID());
                    ++WipeCount;
                    TC_LOG_INFO("module.playerbots", "PB-DUNGEON: wipe in %s's group", bot->GetName().c_str());
                }
            }
            if (fighting)
                QuietSince.erase(guid);
            else if (!QuietSince.count(guid))
                QuietSince[guid] = now;
            DeathView view { getMSTimeDiff(DeadSince[guid], now),
                QuietSince.count(guid) ? std::max<uint32>(getMSTimeDiff(QuietSince[guid], now), 1) : 0,
                bot->IsResurrectRequested(), !anyoneAlive, humanReleased, canResurrect };
            if (!Release(view, ReleaseDelayMs(guid.GetCounter(), Deaths[guid])))
                continue;
            // As the client's spirit release (WorldSession::HandleRepopRequestOpcode).
            bot->RemovePet(nullptr, PET_SAVE_DISMISS, true);
            bot->BuildPlayerRepop();
            bot->RepopAtGraveyard();
            DeadSince.erase(guid);
            QuietSince.erase(guid);
            GhostSince[guid] = now;
            ++ReleaseCount;
            TC_LOG_INFO("module.playerbots", "PB-DUNGEON: %s released its spirit", bot->GetName().c_str());
            continue;
        }

        // A ghost: back to the entrance, or brought there when it takes too long.
        if (!GhostSince.count(guid))
            GhostSince[guid] = now;
        if (bot->GetMapId() == dungeonMap)
        {
            // A graveyard inside the dungeon: the ghost stands right next to its group.
            bot->ResurrectPlayer(0.5f);
            bot->SpawnCorpseBones();
            ++ReturnCount;
            continue;
        }
        Entrance entrance = EntranceFor(*bot, dungeonMap);
        bool late = getMSTimeDiff(GhostSince[guid], now) >= GhostReturnMaxMs;
        if (!entrance.Trigger || late)
        {
            if (EnterDungeon(*bot, dungeonMap, entrance.Inside))
            {
                GhostSince.erase(guid);
                ++FetchCount;
                TC_LOG_INFO("module.playerbots", "PB-DUNGEON: %s brought to the dungeon entrance", bot->GetName().c_str());
            }
            continue;
        }
        if (AtEntrance(*bot, *entrance.Trigger))
        {
            if (EnterDungeon(*bot, dungeonMap, entrance.Inside))
            {
                GhostSince.erase(guid);
                ++ReturnCount;
                TC_LOG_INFO("module.playerbots", "PB-DUNGEON: %s ran back into the dungeon", bot->GetName().c_str());
            }
            continue;
        }
        auto next = NextGhostMove.find(guid);
        if (next == NextGhostMove.end() || int32(now - next->second) >= 0)
        {
            bot->GetMotionMaster()->MovePoint(0, entrance.Trigger->Pos.X, entrance.Trigger->Pos.Y, entrance.Trigger->Pos.Z, true);
            NextGhostMove[guid] = now + GhostMoveEveryMs;
        }
    }
    // A group with someone alive again in the dungeon can wipe anew.
    WipedGroups.erase(std::remove_if(WipedGroups.begin(), WipedGroups.end(),
        [&](ObjectGuid group) { return std::find(wiped.begin(), wiped.end(), group) == wiped.end(); }), WipedGroups.end());
}
}

void RandomPlayerbotLfg::Update(std::vector<Player*> const& bots)
{
    uint32 now = getMSTime();
    std::vector<HumanQueue> queues = HumanQueues();
    Answer(bots, queues);
    Accompany(bots, now);
    Recover(bots, now);
    for (HumanQueue const& queue : queues)
    {
        std::vector<uint8_t> queuedRoles;
        for (auto const& [guid, bot] : Queued)
            if (bot.Key == queue.Key)
                queuedRoles.push_back(bot.Role);
        std::vector<uint8_t> filled = queuedRoles;
        filled.insert(filled.end(), queue.BotRoles.begin(), queue.BotRoles.end());
        Need open = Missing(queue.HumanRoles, filled);
        // One bot per role and pass: a refused bot is skipped next time.
        while (uint8_t role = NextRole(open))
        {
            if (role == Tank) --open.Tanks;
            else if (role == Healer) --open.Healers;
            else --open.Dealers;
            Fill(bots, queue, role, now);
        }
    }
    QueuedCount = uint32(Queued.size());
}

std::string RandomPlayerbotLfg::Describe()
{
    return "lfg queued " + std::to_string(QueuedCount.load()) + ", lfg groups " + std::to_string(GroupCount.load()) +
        ", in dungeons " + std::to_string(InDungeonCount.load()) + ", lfg joins " + std::to_string(JoinCount.load()) +
        ", lifted " + std::to_string(LiftCount.load()) + ", lfg refused " + std::to_string(RefusedCount.load()) +
        ", left dungeons " + std::to_string(LeftCount.load()) + "; dungeon deaths " + std::to_string(DeathCount.load()) +
        ", wipes " + std::to_string(WipeCount.load()) + ", released " + std::to_string(ReleaseCount.load()) +
        ", ran back " + std::to_string(ReturnCount.load()) + ", brought back " + std::to_string(FetchCount.load());
}
