/* GPL v2 or later. See AUTHORS.md. Native Cata disposable level-20 test fixture. */
#include "PlayerbotDevFixture.h"
#include "DBCStores.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "WorldSession.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <tuple>
#include <vector>
namespace PlayerbotDevFixture
{
namespace
{
struct RequestData { Role role; uint32 timestamp; };
std::mutex mutex;
bool enabled = false;
bool rollFixtureEnabled = false;
std::map<ObjectGuid, RequestData> pending;

bool Equip(Player& bot, uint8 slot, uint32 entry)
{
    Item* old = bot.GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (old && old->GetEntry() == entry) return true;
    uint16 dest = 0;
    InventoryResult result = bot.CanEquipNewItem(slot, dest, entry, true);
    if (result != EQUIP_ERR_OK)
    {
        TC_LOG_ERROR("server", "PB-FIXTURE: %s cannot equip item %u slot %u; native result %u", bot.GetName().c_str(), entry, slot, uint32(result));
        return false;
    }
    if (old)
    {
        ItemPosCountVec stored;
        if (bot.CanStoreItem(NULL_BAG, NULL_SLOT, stored, old) != EQUIP_ERR_OK)
        {
            TC_LOG_ERROR("server", "PB-FIXTURE: %s cannot preserve old slot %u gear; no safe bag destination", bot.GetName().c_str(), slot);
            return false;
        }
        bot.RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);
        bot.StoreItem(stored, old, true); // preserve existing gear; never destroy it
    }
    return bot.EquipNewItem(dest, entry, true) != nullptr;
}
bool Carry(Player& bot, uint32 entry, uint32 count)
{
    uint32 carried = bot.GetItemCount(entry, false);
    bool ready = carried >= count || bot.StoreNewItemInBestSlots(entry, count - carried);
    if (!ready) TC_LOG_ERROR("server", "PB-FIXTURE: %s cannot carry item %u count %u", bot.GetName().c_str(), entry, count);
    return ready;
}
}
void SetEnabled(bool value)
{
    std::lock_guard<std::mutex> lock(mutex);
    enabled = value;
    if (!enabled) pending.clear();
}
bool Request(ObjectGuid guid, Role role)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!enabled || guid.IsEmpty() || pending.count(guid) || pending.size() >= 4) return false;
    pending.emplace(guid, RequestData{role, getMSTime()});
    return true;
}
void SetRollFixtureEnabled(bool value)
{
    std::lock_guard<std::mutex> lock(mutex);
    rollFixtureEnabled = value;
}
void Process(Player& bot)
{
    Role role;
    bool prepareRoll = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto found = pending.find(bot.GetGUID());
        if (!enabled || found == pending.end()) return;
        auto request = found->second;
        pending.erase(found);
        if (getMSTime() - request.timestamp > 5000) return;
        role = request.role;
        prepareRoll = rollFixtureEnabled;
    }
    uint8 playerClass = role == Role::Frost ? CLASS_MAGE : role == Role::Holy ? CLASS_PRIEST : CLASS_WARRIOR;
    uint8 tab = role == Role::Arms ? 0 : role == Role::Holy ? 1 : 2;
    uint32 const* tabs = sDBCManager.GetTalentTabPages(playerClass);
    uint32 tree = tabs ? tabs[tab] : 0;
    if (!bot.GetSession()->IsServerOrigin() || bot.getClass() != playerClass || bot.getLevel() != 20 ||
        !bot.IsAlive() || bot.IsInCombat() || bot.IsBeingTeleported() || bot.GetGroup() || !tree ||
        (bot.GetPrimaryTalentTree(bot.GetActiveSpec()) && bot.GetPrimaryTalentTree(bot.GetActiveSpec()) != tree))
    {
        TC_LOG_ERROR("server", "PB-FIXTURE: %s rejected; requires idle, alive, ungrouped level-20 configured role", bot.GetName().c_str());
        return;
    }
    if (!bot.GetPrimaryTalentTree(bot.GetActiveSpec()) && !bot.LearnPrimaryTalentSpecialization(tab)) return;
    std::vector<TalentEntry const*> talents;
    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
        if (TalentEntry const* talent = sTalentStore.LookupEntry(i))
            if (talent->TabID == tree) talents.push_back(talent);
    std::sort(talents.begin(), talents.end(), [](auto left, auto right)
    {
        return std::tie(left->TierID, left->ColumnIndex, left->ID) < std::tie(right->TierID, right->ColumnIndex, right->ID);
    });
    bool progressed = true;
    while (bot.GetFreeTalentPoints() && progressed)
    {
        progressed = false;
        for (auto talent : talents)
            for (uint32 rank = 0; rank < MAX_TALENT_RANK && bot.GetFreeTalentPoints(); ++rank)
                if (talent->SpellRank[rank] && !bot.HasSpell(talent->SpellRank[rank]) && bot.LearnTalent(talent->ID, rank))
                    progressed = true;
    }
    std::vector<uint32> spells = playerClass == CLASS_WARRIOR ? std::vector<uint32>{201,202,9116,8737,71,2457,6673,772,355,78,34428} :
        playerClass == CLASS_MAGE ? std::vector<uint32>{227,133,116,2136,122} : std::vector<uint32>{227,17,2061,2050,139,21562};
    bool ready = bot.GetFreeTalentPoints() == 0;
    for (uint32 spell : spells)
    {
        if (!sSpellMgr->GetSpellInfo(spell))
        {
            TC_LOG_ERROR("server", "PB-FIXTURE: %s missing native spell record %u", bot.GetName().c_str(), spell);
            ready = false; continue;
        }
        bot.LearnSpell(spell, false); // explicit fixture grants, not normal progression policy
        ready = bot.HasSpell(spell) && ready;
        if (!bot.HasSpell(spell)) TC_LOG_ERROR("server", "PB-FIXTURE: %s spell %u not learned", bot.GetName().c_str(), spell);
    }
    if (playerClass == CLASS_WARRIOR)
    {
        ready = Equip(bot, EQUIPMENT_SLOT_MAINHAND, role == Role::Protection ? 4765 : 4817) && ready;
        if (role == Role::Protection) ready = Equip(bot, EQUIPMENT_SLOT_OFFHAND, 1202) && ready;
        ready = Equip(bot, EQUIPMENT_SLOT_CHEST, 2866) && ready;
        ready = Equip(bot, EQUIPMENT_SLOT_WRISTS, 2867) && ready;
        ready = Equip(bot, EQUIPMENT_SLOT_HANDS, 3472) && ready;
    }
    else
    {
        ready = Equip(bot, EQUIPMENT_SLOT_MAINHAND, 1405) && ready;
        ready = Equip(bot, EQUIPMENT_SLOT_CHEST, 2585) && ready;
        ready = Carry(bot, 1205, 20) && ready;
    }
    if (prepareRoll && role == Role::Protection)
    {
        Item* chest = bot.GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_CHEST);
        ItemTemplate const* drop = sObjectMgr->GetItemTemplate(2866);
        ItemPosCountVec stored;
        if (!chest || chest->GetEntry() != 2866 || !drop || drop->GetQuality() < ITEM_QUALITY_UNCOMMON ||
            drop->GetRandomProperty() || drop->GetRandomSuffix() ||
            bot.CanStoreItem(NULL_BAG, NULL_SLOT, stored, chest) != EQUIP_ERR_OK)
            ready = false;
        else
        {
            ObjectGuid preserved = chest->GetGUID();
            bot.RemoveItem(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_CHEST, true);
            bot.StoreItem(stored, chest, true);
            ready = !bot.GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_CHEST) &&
                bot.GetItemByGuid(preserved) == chest && ready;
            TC_LOG_INFO("module.playerbots", "PB-FIXTURE: %s controlled roll chest empty; existing item 2866 preserved", bot.GetName().c_str());
        }
    }
    ready = Carry(bot, 3770, 20) && ready;
    bot.SaveToDB();
    TC_LOG_INFO("module.playerbots", "PB-FIXTURE: %s %s; primary tree %u; free points %u; native save requested",
        bot.GetName().c_str(), ready ? "prepared" : "incomplete", tree, bot.GetFreeTalentPoints());
}
}
