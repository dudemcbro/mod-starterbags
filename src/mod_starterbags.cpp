#include "Config.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "Log.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"

#include <algorithm>

namespace
{
constexpr uint32 DEFAULT_BAG_ENTRY = 4241; // Green Woolen Bag (8 slots)
constexpr uint8 MAX_BANK_BAG_SLOTS = uint8(BANK_SLOT_BAG_END - BANK_SLOT_BAG_START); // 7 on WotLK 3.3.5

struct StarterBagsConfig
{
    bool enabled = true;
    uint32 bagEntry = DEFAULT_BAG_ENTRY;
    bool unlockBankBagSlots = false;
};

StarterBagsConfig LoadConfig()
{
    StarterBagsConfig config;
    config.enabled = sConfigMgr->GetOption<bool>("StarterBags.Enable", true);
    config.bagEntry = sConfigMgr->GetOption<uint32>("StarterBags.BagEntry", DEFAULT_BAG_ENTRY);
    config.unlockBankBagSlots = sConfigMgr->GetOption<bool>("StarterBags.UnlockBankBagSlots", false);
    return config;
}

bool IsValidBag(uint32 entry)
{
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
    return proto && proto->Class == ITEM_CLASS_CONTAINER && proto->InventoryType == INVTYPE_BAG;
}

void MailItemToPlayer(Player* player, Item* item)
{
    if (!player || !item)
        return;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    item->SaveToDB(trans);

    MailDraft("Starter bag", "Your starter bag could not be placed into your inventory/bank, so it was mailed to you.")
        .AddItem(item)
        .SendMailTo(trans, player, MailSender(player, MAIL_STATIONERY_GM));

    // The mail is committed right away, while the rest of the grant is only
    // saved with the character. Clear the first login flag in the same
    // transaction so a crash before the next character save cannot lead to
    // the bags being granted (and mailed) a second time.
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_REM_AT_LOGIN_FLAG);
    stmt->SetData(0, uint16(AT_LOGIN_FIRST));
    stmt->SetData(1, player->GetGUID().GetCounter());
    trans->Append(stmt);

    CharacterDatabase.CommitTransaction(trans);
}

bool TryStoreInInventory(Player* player, Item* item)
{
    ItemPosCountVec dest;
    if (player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false) != EQUIP_ERR_OK)
        return false;

    return player->StoreItem(dest, item, true) != nullptr;
}

bool TryStoreInBank(Player* player, Item* item)
{
    ItemPosCountVec dest;
    if (player->CanBankItem(NULL_BAG, NULL_SLOT, dest, item, false, true) != EQUIP_ERR_OK)
        return false;

    return player->BankItem(dest, item, true) != nullptr;
}

void UnlockBankBagSlots(Player* player)
{
    if (player->GetBankBagSlotCount() < MAX_BANK_BAG_SLOTS)
        player->SetBankBagSlotCount(MAX_BANK_BAG_SLOTS);
}

void FillEquippedBagSlots(Player* player, uint32 bagEntry)
{
    for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
    {
        if (player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            continue;

        uint16 dest = 0;
        InventoryResult err = player->CanEquipNewItem(slot, dest, bagEntry, false);
        if (err == EQUIP_ERR_OK)
        {
            if (Item* item = player->EquipNewItem(dest, bagEntry, true))
                item->SetState(ITEM_CHANGED, player);
            continue;
        }

        LOG_ERROR("module.starterbags", "StarterBags: Failed to equip starter bag (entry {}) in bag slot {} for player {} ({}). Error {}.",
                  bagEntry, slot, player->GetName(), player->GetGUID().ToString(), uint32(err));

        if (Item* item = Item::CreateItem(bagEntry, 1, player))
        {
            if (!TryStoreInBank(player, item) && !TryStoreInInventory(player, item))
                MailItemToPlayer(player, item);
        }
    }
}

void FillBankBagSlots(Player* player, uint32 bagEntry)
{
    // Only fill bank bag slots the character has unlocked. A locked slot
    // cannot hold a bag, and putting the bag anywhere else would only clutter
    // the bank or the mailbox.
    uint8 const unlocked = std::min<uint8>(player->GetBankBagSlotCount(), MAX_BANK_BAG_SLOTS);
    uint8 const endSlot = uint8(BANK_SLOT_BAG_START + unlocked);

    for (uint8 slot = BANK_SLOT_BAG_START; slot < endSlot; ++slot)
    {
        if (player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            continue;

        Item* item = Item::CreateItem(bagEntry, 1, player);
        if (!item)
            continue;

        ItemPosCountVec dest;
        InventoryResult err = player->CanBankItem(INVENTORY_SLOT_BAG_0, slot, dest, item, false, true);
        if (err == EQUIP_ERR_OK && player->BankItem(dest, item, true))
        {
            item->SetState(ITEM_CHANGED, player);
            continue;
        }

        // Fallback: try any bank slot first, then inventory, then mail.
        if (TryStoreInBank(player, item) || TryStoreInInventory(player, item))
            continue;

        LOG_ERROR("module.starterbags", "StarterBags: Failed to bank starter bag (entry {}) into bank bag slot {} for player {} ({}). Error {}. Mailing item.",
                  bagEntry, slot, player->GetName(), player->GetGUID().ToString(), uint32(err));

        MailItemToPlayer(player, item);
    }
}
} // namespace

// Grants the bags once per character, on its first login.
//
// The core calls OnPlayerFirstLogin for a character that still carries the
// AT_LOGIN_FIRST flag. The flag is set when the character is created and is
// cleared once it has entered the world; it is saved together with the
// character's items, so the grant and the "already granted" marker reach the
// database in the same save. No extra table or setting is needed.
//
// Consequences:
// - A character gets bags at most once, so selling or deleting them does not
//   bring new ones.
// - Characters that had already logged in before this module was installed
//   (or before this version replaced the old every-login behaviour) get
//   nothing from it.
// - Random bots created by mod-playerbots never have AT_LOGIN_FIRST set, so
//   they never get bags. No playerbots code is needed for that.
class StarterBagsPlayerScript : public PlayerScript
{
public:
    StarterBagsPlayerScript() : PlayerScript("StarterBagsPlayerScript", { PLAYERHOOK_ON_FIRST_LOGIN }) {}

    void OnPlayerFirstLogin(Player* player) override
    {
        if (!player)
            return;

        StarterBagsConfig const config = LoadConfig();
        if (!config.enabled)
            return;

        if (config.unlockBankBagSlots)
            UnlockBankBagSlots(player);

        if (!IsValidBag(config.bagEntry))
        {
            LOG_ERROR("module.starterbags", "StarterBags: StarterBags.BagEntry = {} is not a bag item. No bags granted to player {} ({}).",
                      config.bagEntry, player->GetName(), player->GetGUID().ToString());
            return;
        }

        FillEquippedBagSlots(player, config.bagEntry);
        FillBankBagSlots(player, config.bagEntry);
    }
};

void Addmod_starterbagsScripts()
{
    new StarterBagsPlayerScript();
}
