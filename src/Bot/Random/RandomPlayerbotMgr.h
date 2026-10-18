/*
 * Random population manager, adapted from mod-playerbots RandomPlayerbotMgr
 * (AddRandomBots/ProcessBot) and RandomPlayerbotFactory::CreateRandomBots at
 * d750b4b. See AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_MGR_H
#define CATA_RANDOM_PLAYERBOT_MGR_H

#include "RandomPlayerbotPolicy.h"
#include <string>

// World-thread-only. Creates the configured number of dedicated accounts
// (prefix + index), one native character each, and admits them as
// server-origin sessions in batches. Bots idle where they log in.
class RandomPlayerbotMgr
{
public:
    static void Configure(RandomPlayerbotSettings const& settings);
    static void Update(uint32_t diff);
    // Console controls: pause stops every random bot and halts admission until resumed.
    static void SetPaused(bool paused);
    static std::string Describe();
};

#endif
