/*
 * Adapted from AzerothCore mod-playerbots AiObjectContext.h at
 * 8827dd6fcbb2bb25988787a40f06fc93daf8e02d. See AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef PLAYERBOTS_ROTATION_AIOBJECTCONTEXT_H
#define PLAYERBOTS_ROTATION_AIOBJECTCONTEXT_H

#include "../../../Bot/Engine/AiObjectContext.h"

// Shared rest, loot, potion and movement contexts for the table-driven classes,
// plus a non-combat "nc" strategy that keeps pets and self buffs up.
class RotationAiObjectContext final : public AiObjectContext
{
public:
    explicit RotationAiObjectContext(PlayerbotAI* botAI);
};

#endif
