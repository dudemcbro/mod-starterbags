#include "DatabaseEnv.h"
#include "Item.h"
#include "Log.h"
#include "Mail.h"
#include "Player.h"
#include "ScriptMgr.h"

namespace
{
constexpr uint32 STARTER_BAG_ENTRY = 4241; // Green Woolen Bag (8 slots) - change if desired
constexpr uint8 MAX_BANK_BAG_SLOTS = uint8(BANK_SLOT_BAG_END - BANK_SLOT_BAG_START); // 7 on WotLK 3.3.5

void MailItemToPlayer(Player* player, Item* item)
{
    if (!player || !item)
        return;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    item->SaveToDB(trans);

    MailDraft("Starter bag", "Your starter bag could not be placed into your inventory/bank, so it was mailed to you.")
        .AddItem(item)
        .SendMailTo(trans, player, MailSender(player, MAIL_STATIONERY_GM));

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

bool EnsureBankBagSlots(Player* player)
{
    if (player->GetBankBagSlotCount() >= MAX_BANK_BAG_SLOTS)
        return false;

    player->SetBankBagSlotCount(MAX_BANK_BAG_SLOTS);
    return true;
}

void EnsureEquippedBagSlots(Player* player)
{
    for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
    {
        if (player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            continue;

        uint16 dest = 0;
        InventoryResult err = player->CanEquipNewItem(slot, dest, STARTER_BAG_ENTRY, false);
        if (err == EQUIP_ERR_OK)
        {
            if (Item* item = player->EquipNewItem(dest, STARTER_BAG_ENTRY, true))
                item->SetState(ITEM_CHANGED, player);
            continue;
        }

        LOG_ERROR("module.starterbags", "StarterBags: Failed to equip starter bag (entry {}) in bag slot {} for player {} ({}). Error {}.",
                  STARTER_BAG_ENTRY, slot, player->GetName(), player->GetGUID().ToString(), uint32(err));

        if (Item* item = Item::CreateItem(STARTER_BAG_ENTRY, 1, player))
        {
            if (!TryStoreInBank(player, item) && !TryStoreInInventory(player, item))
                MailItemToPlayer(player, item);
        }
    }
}

void EnsureBankBagSlotsFilled(Player* player)
{
    for (uint8 slot = BANK_SLOT_BAG_START; slot < BANK_SLOT_BAG_END; ++slot)
    {
        if (player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            continue;

        Item* item = Item::CreateItem(STARTER_BAG_ENTRY, 1, player);
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
                  STARTER_BAG_ENTRY, slot, player->GetName(), player->GetGUID().ToString(), uint32(err));

        MailItemToPlayer(player, item);
    }
}
} // namespace

class StarterBagsPlayerScript : public PlayerScript
{
public:
    StarterBagsPlayerScript() : PlayerScript("StarterBagsPlayerScript", { PLAYERHOOK_ON_LOGIN }) {}

    void OnPlayerLogin(Player* player) override
    {
        if (!player)
            return;

        EnsureBankBagSlots(player);
        EnsureEquippedBagSlots(player);
        EnsureBankBagSlotsFilled(player);
    }
};

void Addmod_starterbagsScripts()
{
    new StarterBagsPlayerScript();
}
