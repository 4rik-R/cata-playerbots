/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerbotGroupStrategy.h"
#include "../Class/Rotation/PlayerbotClassRotation.h"
#include "../Class/Rotation/PlayerbotHybridHealer.h"
#include "Group.h"
#include "Map.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <vector>

namespace PlayerbotGroup
{
FollowPosition PositionFor(Player const& bot, Player const& owner)
{
    std::vector<std::uint32_t> roster;
    for (MapReference const& ref : bot.GetMap()->GetPlayers())
    {
        Player const* member = ref.GetSource();
        if (!member || !member->IsAlive() || member == &owner)
            continue;
        WorldSession const* session = member->GetSession();
        if (session && session->IsServerOrigin() &&
            (member == &bot || session->GetServerOriginFollowTargetGuidLow() == owner.GetGUID().GetCounter()))
            roster.push_back(member->GetGUID().GetCounter());
    }

    std::sort(roster.begin(), roster.end());
    auto it = std::find(roster.begin(), roster.end(), bot.GetGUID().GetCounter());
    bool caster = bot.getClass() == CLASS_MAGE || bot.getClass() == CLASS_PRIEST ||
        PlayerbotClassRotation::FightsAtRange(bot) || PlayerbotHybridHealer::IsPartyHealer(bot);
    if (it == roster.end())
        return WithCasterSpacing(PositionForSlot(0, 1), caster);
    return WithCasterSpacing(PositionForSlot(std::size_t(it - roster.begin()), roster.size()), caster);
}

bool CanResumeAfterDeath(Player const& bot, Player const& owner)
{
    if (!bot.IsAlive() || !owner.IsAlive() || bot.GetMapId() != owner.GetMapId() ||
        !bot.IsWithinDistInMap(&owner, 40.0f))
        return false;

    Group const* group = bot.GetGroup();
    WorldSession const* ownerSession = owner.GetSession();
    return group && owner.GetGroup() == group && group->IsMember(owner.GetGUID()) &&
        ownerSession && !ownerSession->IsServerOrigin();
}

void GreetOnJoin(Player& bot, Player& owner)
{
    if (sWorld->getBoolConfig(CONFIG_PLAYERBOTS_DEV_GREETING_ENABLED))
        bot.Whisper("Hello, ready to help.", LANG_UNIVERSAL, &owner);
}
}
