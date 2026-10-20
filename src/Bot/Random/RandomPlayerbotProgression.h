/*
 * Levelling and placement for random-population bots, adapted from
 * mod-playerbots PlayerbotFactory (InitSpells, InitTalents, InitEquipment,
 * InitFood) and RandomPlayerbotMgr::RandomTeleportForLevel at d750b4b.
 * See AUTHORS.md and PORTING.md. Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_PROGRESSION_H
#define CATA_RANDOM_PLAYERBOT_PROGRESSION_H

#include <cstdint>

class Player;

// World-thread only: callers run inside the session's world update, while map
// threads are idle. Terrain lookups (zone of a spawn point) are not thread-safe.
namespace RandomPlayerbotProgression
{
struct Settings
{
    bool Enabled = false;
    uint32_t MinLevel = 1;
    uint32_t MaxLevel = 85;
};
void Configure(Settings const& settings);
Settings const& GetSettings();

// Builds spawn, item, trainer and food indexes once (a few seconds at most).
void BuildIndexes();
bool Ready();

// Brings a fresh bot to its target level: spells, talents, gear, supplies.
void Initialize(Player& bot, uint32_t level);
// After a native level-up: new spells and talents, gear every few levels.
void ApplyLevelUp(Player& bot, uint32_t previousLevel);
// True when the bot stands in a levelling zone that suits its level and faction.
bool InFittingZone(Player& bot);
// Starts a teleport to a fitting grind spot; false when none was found.
bool Relocate(Player& bot);
// Death knights, Worgen and Goblins are created in Acherus, Gilneas or Kezan.
bool InPhasedStart(Player const& bot);
// Binds the bot in Stormwind or Orgrimmar and starts the teleport there.
bool MoveToCapital(Player& bot);
// Death knights, Worgen and Goblins skip Acherus, Gilneas and Kezan.
bool SkipsStartArea(uint8_t race, uint8_t playerClass);
// Puts a new character that never logged in at its capital entrance and binds it there (synchronous).
void PlaceInCapital(uint32_t guidLow);
// Marks the quests of the skipped start area as done and teaches Death Gate.
void CompleteStartChain(Player& bot);
// Hunters with Call Pet but no pet in the slot Call Pet brings tame a beast. True when a pet was added.
bool EnsureHunterPet(Player& bot);
// The ground mount the bot learned with riding (any thread), or 0.
uint32_t MountSpell(Player const& bot);
}

#endif
