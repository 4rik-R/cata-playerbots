/*
 * Levelling and placement for random-population bots, adapted from
 * mod-playerbots PlayerbotFactory (InitSpells, InitTalents, InitEquipment,
 * InitFood) and RandomPlayerbotMgr::RandomTeleportForLevel at d750b4b.
 * See AUTHORS.md and PORTING.md. Released under GNU GPL v2 or any later version.
 */
#include "RandomPlayerbotProgression.h"
#include "../../Ai/Class/Rotation/PlayerbotClassRotation.h"
#include "RandomPlayerbotLevels.h"
#include "../../Ai/Base/PlayerbotRestItem.h"
#include "Bag.h"
#include "CreatureData.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Item.h"
#include "ItemEnchantmentMgr.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "Random.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TerrainMgr.h"
#include "Timer.h"
#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

using namespace RandomPlayerbotLevels;

namespace
{
RandomPlayerbotProgression::Settings ProgressionSettings;
bool IndexesReady = false;

// Faction templates of a plain human and orc, used to classify mob hostility.
constexpr uint32 AllianceFactionTemplate = 1;
constexpr uint32 HordeFactionTemplate = 2;
constexpr uint32 MaxRelocationAttempts = 40;
constexpr uint32 SupplyCount = 20;
constexpr uint32 BlueGearMaxItemLevel = 346; // heroic Cataclysm dungeon level
constexpr uint32 ApprenticeRiding = 33388;
constexpr uint32 AllianceMount = 458;  // Brown Horse
constexpr uint32 HordeMount = 580;     // Timber Wolf
constexpr uint32 RandomSuffixTries = 12;

struct Spawn
{
    uint16 Map;
    uint8 Teams;
    float X, Y, Z, O;
};
std::vector<Spawn> Spawns;
std::vector<std::vector<uint32>> SpawnsByLevel; // index into Spawns

std::unordered_map<uint8, std::vector<ItemTemplate const*>> GearByInventoryType;
std::map<uint8, std::vector<std::pair<uint8, uint32>>> TrainerSpells; // class -> (level, spell)
struct Supply { uint32 RequiredLevel; uint32 Entry; };
std::vector<Supply> Foods, Drinks;
uint32 BagEntry = 0;

// Agile: leather and mail damage dealers that live on agility (rogue, hunter).
enum class Role { Melee, Tank, Caster, Healer, Agile };

uint8 TeamOf(Player const& bot) { return bot.GetTeamId() == TEAM_ALLIANCE ? Alliance : Horde; }

uint32 Hash(uint32 value)
{
    value *= 2654435761u;
    return value ^ (value >> 16);
}

// Stable spec per character: most classes pick any tree, priests heal.
// Hybrids (paladin, shaman, druid) heal in one tree of three.
uint8 TalentTab(Player const& bot)
{
    uint32 roll = Hash(bot.GetGUID().GetCounter() + 7);
    switch (bot.getClass())
    {
        case CLASS_PRIEST: return uint8(roll % 2);  // Discipline or Holy
        default: return uint8(roll % 3);
    }
}

Role RoleOf(Player const& bot)
{
    uint8 tab = TalentTab(bot);
    switch (bot.getClass())
    {
        case CLASS_WARRIOR: return tab == 2 ? Role::Tank : Role::Melee;
        case CLASS_PRIEST: return Role::Healer;
        // Tab order is the talent tree order of TalentTab.dbc.
        case CLASS_PALADIN: return tab == 0 ? Role::Healer : tab == 1 ? Role::Tank : Role::Melee;
        case CLASS_SHAMAN: return tab == 0 ? Role::Caster : tab == 1 ? Role::Agile : Role::Healer;
        case CLASS_DRUID: return tab == 0 ? Role::Caster : tab == 1 ? Role::Agile : Role::Healer;
        case CLASS_ROGUE:
        case CLASS_HUNTER: return Role::Agile;
        default: return Role::Caster;
    }
}

float StatWeight(Role role, uint32 stat)
{
    bool caster = role == Role::Caster || role == Role::Healer;
    bool agile = role == Role::Agile;
    switch (stat)
    {
        case ITEM_MOD_STAMINA: return role == Role::Tank ? 3.0f : 1.0f;
        case ITEM_MOD_STRENGTH: return caster || agile ? -10.0f : 3.0f;
        case ITEM_MOD_AGILITY: return agile ? 3.0f : caster ? -10.0f : (role == Role::Tank ? 0.5f : -10.0f);
        case ITEM_MOD_INTELLECT: return caster ? 3.0f : -10.0f;
        case ITEM_MOD_SPIRIT: return role == Role::Healer ? 2.0f : (caster ? 1.0f : -10.0f);
        case ITEM_MOD_SPELL_POWER: return caster ? 2.5f : -10.0f;
        case ITEM_MOD_DODGE_RATING:
        case ITEM_MOD_PARRY_RATING: return role == Role::Tank ? 1.5f : (caster ? -10.0f : agile ? 0.0f : 0.2f);
        case ITEM_MOD_ATTACK_POWER:
        case ITEM_MOD_EXPERTISE_RATING: return caster ? -10.0f : 1.0f;
        case ITEM_MOD_HIT_RATING:
        case ITEM_MOD_CRIT_RATING:
        case ITEM_MOD_HASTE_RATING:
        case ITEM_MOD_MASTERY_RATING: return 1.0f;
        default: return 0.0f;
    }
}

// Sum of known stats; negative when the item carries a stat this role must not wear.
float FixedStatScore(ItemTemplate const& proto, Player const& bot, Role role)
{
    float score = 0.0f;
    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        int32 type = proto.GetItemStatType(i);
        uint32 value = proto.GetStatValue(i, &bot);
        if (type <= 0 || !value)
            continue;
        float weight = StatWeight(role, uint32(type));
        if (weight < 0.0f)
            return -1.0f;
        score += weight * float(value);
    }
    return score;
}

float EnchantmentScore(uint32 enchantmentId, float amount, Role role)
{
    SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantmentId);
    if (!enchant)
        return 0.0f;
    float score = 0.0f;
    for (uint32 i = 0; i < MAX_ITEM_ENCHANTMENT_EFFECTS; ++i)
        if (enchant->Effect[i] == ITEM_ENCHANTMENT_TYPE_STAT)
            score += StatWeight(role, enchant->EffectArg[i]) * (amount > 0.0f ? amount : float(enchant->EffectPointsMin[i]));
    return score;
}

// Cata greens mostly roll a random suffix. Native item creation leaves it empty,
// so pick the best of a few native rolls for this role ("of the Bear" for a tank).
ItemRandomEnchantmentId PickRandomEnchantment(ItemTemplate const& proto, Role role)
{
    ItemRandomEnchantmentId best;
    float bestScore = -1e9f;
    for (uint32 attempt = 0; attempt < RandomSuffixTries; ++attempt)
    {
        ItemRandomEnchantmentId id = GenerateItemRandomPropertyId(proto.GetId());
        if (!id.Id)
            break;
        float score = 0.0f;
        if (id.Type == ItemRandomEnchantmentType::Suffix)
        {
            if (ItemRandomSuffixEntry const* suffix = sItemRandomSuffixStore.LookupEntry(id.Id))
                for (uint32 i = 0; i < 5; ++i)
                    score += EnchantmentScore(suffix->Enchantment[i], float(suffix->AllocationPct[i]), role);
        }
        else if (ItemRandomPropertiesEntry const* property = sItemRandomPropertiesStore.LookupEntry(id.Id))
            for (uint32 i = 0; i < 5; ++i)
                score += EnchantmentScore(property->Enchantment[i], 0.0f, role);
        if (score > bestScore)
        {
            bestScore = score;
            best = id;
        }
    }
    return best;
}

bool Excluded(ItemTemplate const& proto)
{
    if (proto.GetRequiredReputationFaction() || proto.GetRequiredSpell() || proto.GetStartQuest() ||
        proto.GetHolidayID() || proto.GetMap() || proto.GetArea() || proto.GetItemLimitCategory() ||
        proto.GetItemSet() || proto.GetRequiredSkill() || (proto.GetFlags() & ITEM_FLAG_IS_BOUND_TO_ACCOUNT))
        return true;
    std::string name = proto.GetDefaultLocaleName() ? proto.GetDefaultLocaleName() : "";
    for (char const* marker : { "Test", "TEST", "Deprecated", "DEPRECATED", "[PH]", "OLD", "Monster -", "QA" })
        if (name.find(marker) != std::string::npos)
            return true;
    return false;
}

void BuildGearIndex()
{
    for (auto const& [entry, proto] : *sObjectMgr->GetItemTemplateStore())
    {
        if ((proto.GetClass() != ITEM_CLASS_ARMOR && proto.GetClass() != ITEM_CLASS_WEAPON) ||
            (proto.GetQuality() != ITEM_QUALITY_UNCOMMON && proto.GetQuality() != ITEM_QUALITY_RARE) ||
            proto.GetRequiredLevel() < 1 || proto.GetRequiredLevel() > int32(MaxLevel) || Excluded(proto))
            continue;
        GearByInventoryType[proto.GetInventoryType()].push_back(&proto);
    }
}

void BuildSupplyIndex()
{
    for (auto const& [entry, proto] : *sObjectMgr->GetItemTemplateStore())
    {
        if (proto.GetClass() == ITEM_CLASS_CONTAINER && proto.GetSubClass() == ITEM_SUBCLASS_CONTAINER &&
            proto.GetQuality() <= ITEM_QUALITY_UNCOMMON && proto.GetBuyPrice() && proto.GetContainerSlots() <= 16 &&
            !Excluded(proto))
        {
            ItemTemplate const* current = BagEntry ? sObjectMgr->GetItemTemplate(BagEntry) : nullptr;
            if (!current || proto.GetContainerSlots() > current->GetContainerSlots() ||
                (proto.GetContainerSlots() == current->GetContainerSlots() && entry < BagEntry))
                BagEntry = entry;
            continue;
        }
        if (!PlayerbotRest::FoodItem(proto.GetClass(), proto.GetSubClass()) || !proto.GetBuyPrice() ||
            proto.GetQuality() > ITEM_QUALITY_NORMAL || proto.IsConjuredConsumable() || Excluded(proto))
            continue;
        for (ItemEffect const& effect : proto.Effects)
        {
            if (!effect.SpellID || effect.Trigger != ITEM_SPELLTRIGGER_ON_USE)
                continue;
            SpellInfo const* spell = sSpellMgr->GetSpellInfo(effect.SpellID);
            if (!spell || spell->Effects[1].Effect) // plain food or drink only, no buff food
                break;
            uint32 category = PlayerbotRest::ItemCategory(effect.Category, spell->GetCategory());
            uint32 level = uint32(std::max(proto.GetRequiredLevel(), 1));
            if (category == SPELL_CATEGORY_FOOD && (spell->HasAura(SPELL_AURA_MOD_REGEN) || spell->HasAura(SPELL_AURA_OBS_MOD_HEALTH)))
                Foods.push_back({ level, entry });
            else if (category == SPELL_CATEGORY_DRINK && (spell->HasAura(SPELL_AURA_MOD_POWER_REGEN) || spell->HasAura(SPELL_AURA_OBS_MOD_POWER)))
                Drinks.push_back({ level, entry });
            break;
        }
    }
    auto order = [](Supply const& a, Supply const& b) { return std::tie(a.RequiredLevel, a.Entry) < std::tie(b.RequiredLevel, b.Entry); };
    std::sort(Foods.begin(), Foods.end(), order);
    std::sort(Drinks.begin(), Drinks.end(), order);
}

uint8 ClassOfFamily(uint32 family)
{
    switch (family)
    {
        case SPELLFAMILY_MAGE: return CLASS_MAGE;
        case SPELLFAMILY_WARRIOR: return CLASS_WARRIOR;
        case SPELLFAMILY_WARLOCK: return CLASS_WARLOCK;
        case SPELLFAMILY_PRIEST: return CLASS_PRIEST;
        case SPELLFAMILY_DRUID: return CLASS_DRUID;
        case SPELLFAMILY_ROGUE: return CLASS_ROGUE;
        case SPELLFAMILY_HUNTER: return CLASS_HUNTER;
        case SPELLFAMILY_PALADIN: return CLASS_PALADIN;
        case SPELLFAMILY_SHAMAN: return CLASS_SHAMAN;
        case SPELLFAMILY_DEATHKNIGHT: return CLASS_DEATH_KNIGHT;
        default: return 0;
    }
}

// Class trainers carry no class in `trainer` (Requirement is 0), so each
// taught spell is assigned to the class of its spell family.
void BuildTrainerIndex()
{
    QueryResult result = WorldDatabase.Query(
        "SELECT ts.SpellId, ts.ReqLevel FROM trainer t JOIN trainer_spell ts ON ts.TrainerId = t.Id WHERE t.Type = 0");
    if (!result)
        return;
    std::map<uint8, std::map<uint32, uint8>> lowest;
    do
    {
        Field* fields = result->Fetch();
        uint32 spell = fields[0].GetUInt32();
        uint8 level = fields[1].GetUInt8();
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spell);
        if (!info)
            continue;
        uint32 family = info->SpellFamilyName;
        if (!family && info->HasEffect(SPELL_EFFECT_LEARN_SPELL))
            for (SpellEffectInfo const& effect : info->Effects)
                if (effect.Effect == SPELL_EFFECT_LEARN_SPELL)
                    if (SpellInfo const* taught = sSpellMgr->GetSpellInfo(effect.TriggerSpell))
                        family = taught->SpellFamilyName;
        uint8 playerClass = ClassOfFamily(family);
        if (!playerClass)
            continue;
        auto [it, inserted] = lowest[playerClass].emplace(spell, level);
        if (!inserted)
            it->second = std::min(it->second, level);
    } while (result->NextRow());
    for (auto const& [playerClass, spells] : lowest)
    {
        auto& list = TrainerSpells[playerClass];
        for (auto const& [spell, level] : spells)
            list.emplace_back(level, spell);
        std::sort(list.begin(), list.end());
    }
}

void BuildSpawnIndex()
{
    FactionTemplateEntry const* alliance = sFactionTemplateStore.LookupEntry(AllianceFactionTemplate);
    FactionTemplateEntry const* horde = sFactionTemplateStore.LookupEntry(HordeFactionTemplate);
    if (!alliance || !horde)
        return;
    SpawnsByLevel.assign(MaxLevel + 4, {});
    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
    {
        if ((data.mapId != 0 && data.mapId != 1 && data.mapId != 530 && data.mapId != 571) ||
            (data.phaseId != 0 && data.phaseId != DEFAULT_PHASE) || data.phaseGroup || data.terrainSwapMap != -1)
            continue;
        CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(data.id);
        if (!info || info->rank != CREATURE_ELITE_NORMAL || info->npcflag || info->type == CREATURE_TYPE_CRITTER ||
            !info->minlevel || info->maxlevel > MaxLevel + 3 ||
            (info->flags_extra & (CREATURE_FLAG_EXTRA_CIVILIAN | CREATURE_FLAG_EXTRA_TRIGGER | CREATURE_FLAG_EXTRA_GUARD)) ||
            (info->unit_flags & (UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC)))
            continue;
        FactionTemplateEntry const* faction = sFactionTemplateStore.LookupEntry(info->faction);
        if (!faction)
            continue;
        uint8 teams = (faction->IsFriendlyTo(alliance) ? 0 : Alliance) | (faction->IsFriendlyTo(horde) ? 0 : Horde);
        if (!teams)
            continue;
        uint32 index = uint32(Spawns.size());
        Spawns.push_back({ uint16(data.mapId), teams, data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY(),
            data.spawnPoint.GetPositionZ(), data.spawnPoint.GetOrientation() });
        for (uint32 level = info->minlevel; level <= info->maxlevel && level < SpawnsByLevel.size(); ++level)
            SpawnsByLevel[level].push_back(index);
    }
}

void LearnClassSpells(Player& bot)
{
    auto found = TrainerSpells.find(bot.getClass());
    if (found == TrainerSpells.end())
        return;
    for (auto const& [level, spellId] : found->second)
    {
        if (level > bot.getLevel())
            break;
        SpellInfo const* spell = sSpellMgr->GetSpellInfo(spellId);
        if (!spell)
            continue;
        if (spell->HasEffect(SPELL_EFFECT_LEARN_SPELL))
        {
            for (SpellEffectInfo const& effect : spell->Effects)
                if (effect.Effect == SPELL_EFFECT_LEARN_SPELL && effect.TriggerSpell && !bot.HasSpell(effect.TriggerSpell))
                    bot.LearnSpell(effect.TriggerSpell, false);
        }
        else if (!bot.HasSpell(spellId))
            bot.LearnSpell(spellId, false);
    }
}

// Same greedy tier order as the dev fixture, for the bot's stable spec.
void LearnTalents(Player& bot)
{
    if (bot.getLevel() < 10)
        return;
    uint8 tab = TalentTab(bot);
    uint32 const* tabs = sDBCManager.GetTalentTabPages(bot.getClass());
    uint32 tree = tabs ? tabs[tab] : 0;
    if (!tree)
        return;
    if (!bot.GetPrimaryTalentTree(bot.GetActiveSpec()) && !bot.LearnPrimaryTalentSpecialization(tab))
        return;
    tree = bot.GetPrimaryTalentTree(bot.GetActiveSpec());
    std::vector<TalentEntry const*> talents;
    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
        if (TalentEntry const* talent = sTalentStore.LookupEntry(i))
            if (talent->TabID == tree)
                talents.push_back(talent);
    std::sort(talents.begin(), talents.end(), [](auto left, auto right)
    {
        return std::tie(left->TierID, left->ColumnIndex, left->ID) < std::tie(right->TierID, right->ColumnIndex, right->ID);
    });
    bool progressed = true;
    while (bot.GetFreeTalentPoints() && progressed)
    {
        progressed = false;
        for (TalentEntry const* talent : talents)
            for (uint32 rank = 0; rank < MAX_TALENT_RANK && bot.GetFreeTalentPoints(); ++rank)
                if (talent->SpellRank[rank] && !bot.HasSpell(talent->SpellRank[rank]) && bot.LearnTalent(talent->ID, rank))
                    progressed = true;
    }
}

struct SlotRule
{
    uint8 Slot;
    std::vector<uint8> InventoryTypes;
    std::set<uint32> Subclasses; // empty: armor slot uses the class's armor type
};

uint32 ArmorSubclass(Player const& bot)
{
    switch (bot.getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT: return bot.getLevel() >= 40 ? ITEM_SUBCLASS_ARMOR_PLATE : ITEM_SUBCLASS_ARMOR_MAIL;
        case CLASS_HUNTER:
        case CLASS_SHAMAN: return bot.getLevel() >= 40 ? ITEM_SUBCLASS_ARMOR_MAIL : ITEM_SUBCLASS_ARMOR_LEATHER;
        case CLASS_ROGUE:
        case CLASS_DRUID: return ITEM_SUBCLASS_ARMOR_LEATHER;
        default: return ITEM_SUBCLASS_ARMOR_CLOTH;
    }
}

// Two-handed weapon types each melee class can train.
std::set<uint32> TwoHanders(uint8 playerClass)
{
    switch (playerClass)
    {
        case CLASS_SHAMAN: return { ITEM_SUBCLASS_WEAPON_AXE2, ITEM_SUBCLASS_WEAPON_MACE2, ITEM_SUBCLASS_WEAPON_STAFF };
        case CLASS_DRUID: return { ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_POLEARM, ITEM_SUBCLASS_WEAPON_MACE2 };
        default:
            return { ITEM_SUBCLASS_WEAPON_AXE2, ITEM_SUBCLASS_WEAPON_MACE2, ITEM_SUBCLASS_WEAPON_SWORD2,
                ITEM_SUBCLASS_WEAPON_POLEARM };
    }
}

std::vector<SlotRule> SlotRules(Player const& bot, Role role)
{
    std::vector<SlotRule> rules = {
        { EQUIPMENT_SLOT_HEAD, { INVTYPE_HEAD }, {} },
        { EQUIPMENT_SLOT_NECK, { INVTYPE_NECK }, { ITEM_SUBCLASS_ARMOR_MISCELLANEOUS } },
        { EQUIPMENT_SLOT_SHOULDERS, { INVTYPE_SHOULDERS }, {} },
        { EQUIPMENT_SLOT_CHEST, { INVTYPE_CHEST, INVTYPE_ROBE }, {} },
        { EQUIPMENT_SLOT_WAIST, { INVTYPE_WAIST }, {} },
        { EQUIPMENT_SLOT_LEGS, { INVTYPE_LEGS }, {} },
        { EQUIPMENT_SLOT_FEET, { INVTYPE_FEET }, {} },
        { EQUIPMENT_SLOT_WRISTS, { INVTYPE_WRISTS }, {} },
        { EQUIPMENT_SLOT_HANDS, { INVTYPE_HANDS }, {} },
        { EQUIPMENT_SLOT_FINGER1, { INVTYPE_FINGER }, { ITEM_SUBCLASS_ARMOR_MISCELLANEOUS } },
        { EQUIPMENT_SLOT_FINGER2, { INVTYPE_FINGER }, { ITEM_SUBCLASS_ARMOR_MISCELLANEOUS } },
        { EQUIPMENT_SLOT_BACK, { INVTYPE_CLOAK }, { ITEM_SUBCLASS_ARMOR_CLOTH } },
    };
    if (role == Role::Tank)
    {
        rules.push_back({ EQUIPMENT_SLOT_MAINHAND, { INVTYPE_WEAPON, INVTYPE_WEAPONMAINHAND },
            { ITEM_SUBCLASS_WEAPON_AXE, ITEM_SUBCLASS_WEAPON_MACE, ITEM_SUBCLASS_WEAPON_SWORD } });
        rules.push_back({ EQUIPMENT_SLOT_OFFHAND, { INVTYPE_SHIELD }, { ITEM_SUBCLASS_ARMOR_SHIELD } });
    }
    else if (bot.getClass() == CLASS_ROGUE)
    {
        // Two one-handers; the off hand only fits once Dual Wield is trained at 10.
        std::set<uint32> oneHanders = { ITEM_SUBCLASS_WEAPON_DAGGER, ITEM_SUBCLASS_WEAPON_SWORD,
            ITEM_SUBCLASS_WEAPON_MACE, ITEM_SUBCLASS_WEAPON_AXE, ITEM_SUBCLASS_WEAPON_FIST_WEAPON };
        rules.push_back({ EQUIPMENT_SLOT_MAINHAND, { INVTYPE_WEAPON, INVTYPE_WEAPONMAINHAND }, oneHanders });
        rules.push_back({ EQUIPMENT_SLOT_OFFHAND, { INVTYPE_WEAPON, INVTYPE_WEAPONOFFHAND }, oneHanders });
    }
    else if ((role == Role::Healer || role == Role::Caster) &&
        (bot.getClass() == CLASS_PALADIN || bot.getClass() == CLASS_SHAMAN))
    {
        // Holy paladins and caster shamans: one-hander and shield.
        std::set<uint32> oneHanders = { ITEM_SUBCLASS_WEAPON_MACE, ITEM_SUBCLASS_WEAPON_AXE };
        if (bot.getClass() == CLASS_PALADIN)
            oneHanders.insert(ITEM_SUBCLASS_WEAPON_SWORD);
        else
        {
            oneHanders.insert(ITEM_SUBCLASS_WEAPON_DAGGER);
            oneHanders.insert(ITEM_SUBCLASS_WEAPON_FIST_WEAPON);
        }
        rules.push_back({ EQUIPMENT_SLOT_MAINHAND, { INVTYPE_WEAPON, INVTYPE_WEAPONMAINHAND }, oneHanders });
        rules.push_back({ EQUIPMENT_SLOT_OFFHAND, { INVTYPE_SHIELD }, { ITEM_SUBCLASS_ARMOR_SHIELD } });
    }
    else if (role == Role::Melee || role == Role::Agile)
        rules.push_back({ EQUIPMENT_SLOT_MAINHAND, { INVTYPE_2HWEAPON }, TwoHanders(bot.getClass()) });
    else
        rules.push_back({ EQUIPMENT_SLOT_MAINHAND, { INVTYPE_2HWEAPON }, { ITEM_SUBCLASS_WEAPON_STAFF } });
    if (bot.getClass() == CLASS_MAGE || bot.getClass() == CLASS_PRIEST || bot.getClass() == CLASS_WARLOCK)
        rules.push_back({ EQUIPMENT_SLOT_RANGED, { INVTYPE_RANGEDRIGHT }, { ITEM_SUBCLASS_WEAPON_WAND } });
    else if (bot.getClass() == CLASS_HUNTER)
        rules.push_back({ EQUIPMENT_SLOT_RANGED, { INVTYPE_RANGED, INVTYPE_RANGEDRIGHT },
            { ITEM_SUBCLASS_WEAPON_BOW, ITEM_SUBCLASS_WEAPON_GUN, ITEM_SUBCLASS_WEAPON_CROSSBOW } });
    else if (bot.getClass() == CLASS_WARRIOR || bot.getClass() == CLASS_ROGUE)
        rules.push_back({ EQUIPMENT_SLOT_RANGED, { INVTYPE_THROWN, INVTYPE_RANGED, INVTYPE_RANGEDRIGHT },
            { ITEM_SUBCLASS_WEAPON_THROWN, ITEM_SUBCLASS_WEAPON_BOW, ITEM_SUBCLASS_WEAPON_GUN, ITEM_SUBCLASS_WEAPON_CROSSBOW } });
    return rules;
}

ItemTemplate const* PickGear(Player& bot, SlotRule const& rule, Role role, uint32 quality, uint32 excludeEntry)
{
    uint32 level = bot.getLevel();
    uint32 armor = ArmorSubclass(bot);
    std::vector<ItemTemplate const*> candidates;
    uint32 bestItemLevel = 0;
    for (uint8 inventoryType : rule.InventoryTypes)
    {
        auto found = GearByInventoryType.find(inventoryType);
        if (found == GearByInventoryType.end())
            continue;
        for (ItemTemplate const* proto : found->second)
        {
            uint32 required = uint32(proto->GetRequiredLevel());
            if (proto->GetQuality() != quality || required > level || required < GearLevelLow(level) ||
                proto->GetId() == excludeEntry || (quality == ITEM_QUALITY_RARE && proto->GetBaseItemLevel() > BlueGearMaxItemLevel))
                continue;
            if (rule.Subclasses.empty() ? (proto->GetClass() != ITEM_CLASS_ARMOR || proto->GetSubClass() != armor) :
                !rule.Subclasses.count(proto->GetSubClass()))
                continue;
            if ((proto->GetAllowableClass() & bot.getClassMask()) == 0 || bot.CanUseItem(proto) != EQUIP_ERR_OK ||
                FixedStatScore(*proto, bot, role) < 0.0f)
                continue;
            candidates.push_back(proto);
            bestItemLevel = std::max(bestItemLevel, proto->GetBaseItemLevel());
        }
    }
    // Any of the top items, so bots of one level do not all look alike.
    candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [&](ItemTemplate const* proto)
    {
        return proto->GetBaseItemLevel() + 3 < bestItemLevel;
    }), candidates.end());
    return candidates.empty() ? nullptr : candidates[urand(0, uint32(candidates.size()) - 1)];
}

bool EquipFresh(Player& bot, uint8 slot, ItemTemplate const& proto, Role role)
{
    if (bot.GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        bot.DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);
    uint16 dest = 0;
    if (bot.CanEquipNewItem(slot, dest, proto.GetId(), false) != EQUIP_ERR_OK)
        return false;
    Item* item = Item::CreateItem(proto.GetId(), 1, &bot);
    if (!item)
        return false;
    if (proto.GetRandomProperty() || proto.GetRandomSuffix())
        item->SetItemRandomProperties(PickRandomEnchantment(proto, role));
    return bot.EquipItem(dest, item, true) != nullptr;
}

void EquipForLevel(Player& bot)
{
    Role role = RoleOf(bot);
    uint32 quality = bot.getLevel() >= MaxLevel ? ITEM_QUALITY_RARE : ITEM_QUALITY_UNCOMMON;
    uint32 equipped = 0, missing = 0;
    uint32 firstRing = 0;
    for (SlotRule const& rule : SlotRules(bot, role))
    {
        ItemTemplate const* proto = PickGear(bot, rule, role, quality, rule.Slot == EQUIPMENT_SLOT_FINGER2 ? firstRing : 0);
        if (!proto && quality == ITEM_QUALITY_RARE)
            proto = PickGear(bot, rule, role, ITEM_QUALITY_UNCOMMON, rule.Slot == EQUIPMENT_SLOT_FINGER2 ? firstRing : 0);
        if (proto && EquipFresh(bot, rule.Slot, *proto, role))
        {
            ++equipped;
            if (rule.Slot == EQUIPMENT_SLOT_FINGER1)
                firstRing = proto->GetId();
        }
        else
            ++missing;
    }
    TC_LOG_DEBUG("module.playerbots", "PB-RANDOM: %s level %u equipped %u slots, %u without a fitting item",
        bot.GetName().c_str(), bot.getLevel(), equipped, missing);
}

uint32 BestSupply(std::vector<Supply> const& list, uint32 level)
{
    uint32 entry = 0;
    for (Supply const& supply : list)
    {
        if (supply.RequiredLevel > level)
            break;
        entry = supply.Entry;
    }
    return entry;
}

void Restock(Player& bot)
{
    for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END && BagEntry; ++slot)
        if (!bot.GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            uint16 dest = 0;
            if (bot.CanEquipNewItem(slot, dest, BagEntry, false) == EQUIP_ERR_OK)
                bot.EquipNewItem(dest, BagEntry, true);
        }
    auto carry = [&](uint32 entry)
    {
        uint32 count = entry ? bot.GetItemCount(entry) : SupplyCount;
        if (count < SupplyCount)
            bot.StoreNewItemInBestSlots(entry, SupplyCount - count);
    };
    carry(BestSupply(Foods, bot.getLevel()));
    if (bot.GetPowerType() == POWER_MANA)
        carry(BestSupply(Drinks, bot.getLevel()));
}

void LearnRiding(Player& bot)
{
    if (bot.getLevel() < RidingLevel)
        return;
    uint32 mount = bot.GetTeamId() == TEAM_ALLIANCE ? AllianceMount : HordeMount;
    for (uint32 spell : { ApprenticeRiding, mount })
        if (sSpellMgr->GetSpellInfo(spell) && !bot.HasSpell(spell))
            bot.LearnSpell(spell, false);
}
}

void RandomPlayerbotProgression::Configure(Settings const& settings)
{
    ProgressionSettings = settings;
    ProgressionSettings.MinLevel = std::clamp<uint32_t>(settings.MinLevel, 1, MaxLevel);
    ProgressionSettings.MaxLevel = std::clamp<uint32_t>(settings.MaxLevel, ProgressionSettings.MinLevel, MaxLevel);
}

RandomPlayerbotProgression::Settings const& RandomPlayerbotProgression::GetSettings()
{
    return ProgressionSettings;
}

void RandomPlayerbotProgression::BuildIndexes()
{
    if (IndexesReady)
        return;
    uint32 started = getMSTime();
    BuildSpawnIndex();
    BuildGearIndex();
    BuildSupplyIndex();
    BuildTrainerIndex();
    PlayerbotClassRotation::ValidateSpells();
    IndexesReady = true;
    size_t gear = 0;
    for (auto const& [type, list] : GearByInventoryType)
        gear += list.size();
    TC_LOG_INFO("module.playerbots", "Random playerbots: indexed %u grind spawns, %u gear items, %u foods, %u drinks, "
        "trainer spells for %u classes in %u ms.", uint32(Spawns.size()), uint32(gear), uint32(Foods.size()),
        uint32(Drinks.size()), uint32(TrainerSpells.size()), getMSTimeDiff(started, getMSTime()));
}

bool RandomPlayerbotProgression::Ready()
{
    return IndexesReady;
}

void RandomPlayerbotProgression::Initialize(Player& bot, uint32_t level)
{
    uint32 previous = bot.getLevel();
    if (level > previous)
        bot.GiveLevel(uint8(level));
    bot.SetUInt32Value(PLAYER_XP, 0);
    LearnClassSpells(bot);
    LearnTalents(bot);
    EquipForLevel(bot);
    LearnRiding(bot);
    Restock(bot);
    if (bot.GetMoney() < StartingMoney(level))
        bot.ModifyMoney(int64(StartingMoney(level) - bot.GetMoney()));
    bot.SetFullHealth();
    if (bot.GetPowerType() == POWER_MANA)
        bot.SetPower(POWER_MANA, bot.GetMaxPower(POWER_MANA));
    bot.SaveToDB();
    TC_LOG_INFO("module.playerbots", "PB-RANDOM: %s prepared for level %u (from %u)", bot.GetName().c_str(), bot.getLevel(), previous);
}

void RandomPlayerbotProgression::ApplyLevelUp(Player& bot, uint32_t previousLevel)
{
    LearnClassSpells(bot);
    LearnTalents(bot);
    LearnRiding(bot);
    // Gear every five levels, or when crossing an armor-type boundary.
    if (bot.getLevel() / 5 != previousLevel / 5 || (previousLevel < 40 && bot.getLevel() >= 40))
        EquipForLevel(bot);
    Restock(bot);
}

bool RandomPlayerbotProgression::InFittingZone(Player& bot)
{
    return ZoneFits(bot.GetZoneId(), bot.getLevel(), TeamOf(bot));
}

bool RandomPlayerbotProgression::Relocate(Player& bot)
{
    if (!IndexesReady || SpawnsByLevel.empty())
        return false;
    uint8 team = TeamOf(bot);
    FactionTemplateEntry const* own = sFactionTemplateStore.LookupEntry(team == Alliance ? AllianceFactionTemplate : HordeFactionTemplate);
    uint32 level = bot.getLevel();
    uint32 low = MobLevelLow(level), high = std::min<uint32>(MobLevelHigh(level), uint32(SpawnsByLevel.size()) - 1);
    for (uint32 attempt = 0; attempt < MaxRelocationAttempts; ++attempt)
    {
        auto const& bucket = SpawnsByLevel[urand(low, high)];
        if (bucket.empty())
            continue;
        Spawn const& spawn = Spawns[bucket[urand(0, uint32(bucket.size()) - 1)]];
        if (!(spawn.Teams & team))
            continue;
        uint32 zoneId = 0, areaId = 0;
        sTerrainMgr.GetZoneAndAreaId(PhasingHandler::GetEmptyPhaseShift(), zoneId, areaId, spawn.Map, spawn.X, spawn.Y, spawn.Z);
        if (!ZoneFits(zoneId, level, team))
            continue;
        // Skip subzones that belong to the other faction only (their towns and outposts).
        if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(areaId); area && own &&
            (area->FactionGroupMask & own->EnemyGroup) && !(area->FactionGroupMask & own->FriendGroup))
            continue;
        if (bot.TeleportTo(spawn.Map, spawn.X, spawn.Y, spawn.Z + 0.5f, spawn.O))
        {
            TC_LOG_DEBUG("module.playerbots", "PB-RANDOM: %s level %u sent to zone %u map %u", bot.GetName().c_str(),
                level, zoneId, spawn.Map);
            return true;
        }
        return false;
    }
    TC_LOG_DEBUG("module.playerbots", "PB-RANDOM: %s level %u found no grind spot", bot.GetName().c_str(), level);
    return false;
}
