/*
 * Adapted from AzerothCore mod-playerbots PlayerbotSecurity.cpp at
 * 8827dd6fcbb2bb25988787a40f06fc93daf8e02d. See AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "PlayerbotSecurity.h"
#include "RandomPlayerbotMgr.h"
#include "Group.h"
#include "Player.h"
#include "WorldSession.h"

PlayerbotSecurityLevel PlayerbotSecurity::LevelFor(Player const& from) const
{
    WorldSession const* botSession = bot.GetSession();
    WorldSession const* fromSession = from.GetSession();
    // An ungrouped random-population bot is its own controller while grinding.
    if (&bot == &from && bot.IsInWorld() && botSession && botSession->IsServerOrigin() &&
        botSession->GetServerOriginCharacterGuid() == bot.GetGUID() && !bot.GetGroup() &&
        RandomPlayerbotMgr::IsRandomAccount(botSession->GetAccountId()))
        return PLAYERBOT_SECURITY_ALLOW_ALL;
    if (!bot.IsInWorld() || !from.IsInWorld() || &bot == &from || !botSession ||
        !botSession->IsServerOrigin() || botSession->GetServerOriginCharacterGuid() != bot.GetGUID() ||
        !fromSession || fromSession->IsServerOrigin())
        return PLAYERBOT_SECURITY_DENY_ALL;

    // Keep the donor's GM override. Ordinary player control is tied to the
    // invitation-adopted controller, not to an arbitrary group member.
    if (from.CanBeGameMaster())
        return PLAYERBOT_SECURITY_ALLOW_ALL;

    if (from.GetTeam() != bot.GetTeam())
        return PLAYERBOT_SECURITY_DENY_ALL;

    Group const* botGroup = bot.GetGroup();
    if (botGroup && botGroup->IsMember(from.GetGUID()))
    {
        if (botSession->GetServerOriginPartyControllerGuidLow() == from.GetGUID().GetCounter())
            return PLAYERBOT_SECURITY_ALLOW_ALL;
        return PLAYERBOT_SECURITY_TALK;
    }

    // The current development roster accepts any human invitation allowed by
    // the core. A grouped bot cannot be recruited by another player here.
    return botGroup ? PLAYERBOT_SECURITY_TALK : PLAYERBOT_SECURITY_INVITE;
}
