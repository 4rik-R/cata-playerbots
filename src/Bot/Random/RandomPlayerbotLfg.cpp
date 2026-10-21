/*
 * Random-population bots in the dungeon finder. The donor mod-playerbots
 * queues its random bots for the dungeons real players wait for
 * (RandomPlayerbotMgr::CheckLfgQueue, LfgActions); here the bots fill exactly
 * the roles a human's queue lacks, and a bot is lifted to the human's level
 * when none is close enough. Released under GNU GPL v2 or any later version.
 */
#include "RandomPlayerbotLfg.h"
#include "RandomPlayerbotLfgPolicy.h"
#include "RandomPlayerbotMgr.h"
#include "RandomPlayerbotProgression.h"
#include "CharacterCache.h"
#include "Group.h"
#include "LFGMgr.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
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
}

void RandomPlayerbotLfg::Update(std::vector<Player*> const& bots)
{
    uint32 now = getMSTime();
    std::vector<HumanQueue> queues = HumanQueues();
    Answer(bots, queues);
    Accompany(bots, now);
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
        ", left dungeons " + std::to_string(LeftCount.load());
}
