/*
 * Party healing for Holy paladins, Restoration shamans and Restoration druids.
 * Reuses the priest's target order and mana rules with the class's spells from
 * PlayerbotHealerKit. Released under GNU GPL v2 or any later version.
 */
#ifndef PLAYERBOT_HYBRID_HEALER_H
#define PLAYERBOT_HYBRID_HEALER_H

class Player;

// Map-thread only; no Player pointer is retained.
namespace PlayerbotHybridHealer
{
// A paladin, shaman or druid whose primary talent tree heals.
bool IsPartyHealer(Player const& bot);
// Someone in healing range is below the heal-over-time threshold.
bool AnyoneNeedsHealing(Player& bot, Player* owner);
// One healing decision: heal, keep the tank buff, or resurrect out of combat.
bool HealParty(Player& bot, Player* owner);
}

#endif
