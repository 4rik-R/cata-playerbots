/*
 * Random-bot chat: bots join General and Trade, post now and then where a
 * human can read it, comment on loot, levels, quests and rare kills, call for
 * heals in a group, answer whispers, party, say and channel lines in German,
 * and mirror emotes. With the local AI switched on, answers come from it.
 * The chat lines and occasions follow mod-playerbots (see
 * RandomPlayerbotChatTexts.h). Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_CHAT_H
#define CATA_RANDOM_PLAYERBOT_CHAT_H

#include "ObjectGuid.h"
#include <cstdint>
#include <string>
#include <vector>

class Channel;
class Creature;
class Group;
class Player;

namespace RandomPlayerbotChat
{
struct Settings
{
    bool Enabled = false;
    uint32_t ChatterMinMs = 120000;
    uint32_t ChatterMaxMs = 240000;
};

void Configure(Settings const& settings);
bool Enabled();
// World thread, a few times per second, with the random bots that are online.
void Update(std::vector<Player*> const& bots);
std::string Describe();

// Any thread. A human's chat line; only the facts are queued, the world thread answers.
void HumanWhisper(Player& human, Player& bot, std::string const& text);
void HumanParty(Player& human, Group& group, std::string const& text);
void HumanSay(Player& human, std::string const& text);
void HumanChannel(Player& human, Channel& channel, std::string const& text);
// Any thread. Occasions bots talk about.
void Loot(Player& bot, uint32_t itemId);
}

void AddSC_random_playerbot_chat();

#endif
