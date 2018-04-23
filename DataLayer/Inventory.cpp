#include "Inventory.h"

#include "../Models/DataStore.h"
#include "../Models/Player.h"

INT32 Inventory::MoveItem(UINT16 slotIdFrom, UINT16 slotIdTo) noexcept
{
	lock();

	if ((slotIdFrom < 40 || slotIdFrom >= (slotsCount + 40)) || (slotIdTo < 40 || slotIdTo >= (slotsCount + 40))) {
		unlock();
		return 1;
	}

	ISlot * slotFrom = inventorySlots + slotIdFrom;
	ISlot * slotTo = inventorySlots + slotIdTo;

	if (slotFrom->IsEmpty())
	{
		//@TODO log
		unlock();
		return 2;
	}
	else if (slotTo->IsEmpty())
	{
		InterchangeItems(slotFrom, slotTo);
		unlock();
		return 0;
	}

	//if is stackable (nonDB) we process stack stuff
	if (slotFrom->item->iTemplate->id == slotTo->item->iTemplate->id && //same item template
		slotTo->item->iTemplate->HasFlag(EItemTemplateFlags_IsNonDb))
	{
		UINT32 totalStack = slotFrom->item->stackCount + slotTo->item->stackCount;
		if (totalStack > slotTo->item->iTemplate->maxStack)
		{ //if stack is greater than the maxStack admited by the item template we keep (totalStack - maxStack) in slotFrom->item->stackCount
			slotTo->item->stackCount = slotTo->item->iTemplate->maxStack;
			slotFrom->item->stackCount = slotTo->item->iTemplate->maxStack - totalStack;
		}
		else
		{ //if sum of stacks is less than the maxStack we add it to the slotTo->item stack and destory the slotFrom->item
			slotTo->item->stackCount += slotFrom->item->stackCount;
			slotFrom->Clear();
		}
	}
	else
	{
		InterchangeItems(slotFrom, slotTo);
	}


	unlock();
	return 0;
}

INT32 Inventory::MoveItemUnsafe(UINT16 slotIdFrom, UINT16 slotIdTo) noexcept
{
	return INT32();
}

INT32 Inventory::Clear(sql::Connection * sql) noexcept
{


	return 0;
}

INT32 Inventory::ClearUnsafe(sql::Connection * sql) noexcept
{



	return 0;
}

INT32 Inventory::Init()
{



	return 0;
}

INT32 Inventory::InsertNonDb(UINT32 itemId, UINT32 stackCount) {
	Item * item = CreateNonDbItem(itemId, stackCount);
	if (!item) {
		return 1;
	}

	for (UINT16 i = 0; i < slotsCount; i++)
	{
		if (inventorySlots[i].IsEmpty()) {
			inventorySlots[i].item = item;

			return 0;
		}
	}

	delete item;
	return 2;
}

INT32 Inventory::InsertNonDb(Item * item)
{
	if (!item) {
		return 1;
	}

	for (UINT16 i = 0; i < slotsCount; i++)
	{
		if (inventorySlots[i].IsEmpty()) {
			inventorySlots[i].item = item;

			return 0;
		}
	}

	return 2;
}

INT32 Inventory::StackNonDb(UINT32 itemId, INT32 stackCount)
{
	//try to put the stack on existing items
	for (UINT16 i = 0; i < slotsCount; i++)
	{
		if (stackCount > 0) {

			if (!inventorySlots[i].IsEmpty() && inventorySlots[i].item->iTemplate->id == itemId) {
				Item * existingItem = inventorySlots[i].item;

				existingItem->stackCount += stackCount;
				if (existingItem->stackCount > existingItem->iTemplate->maxStack) {
					stackCount = existingItem->stackCount - existingItem->iTemplate->maxStack;

					existingItem->stackCount = existingItem->iTemplate->maxStack;
				}
				else
				{
					stackCount = 0;
					break;
				}
			}
		}
		else {
			break;
		}
	}

	//if still have stack create new [celling(stackCount / maxStack)] items
	if (stackCount > 0) {
		UINT32 iTemplateIndex = GetItemTemplateIndex(itemId);
		if (iTemplateIndex == UINT32_MAX) {
			//@TODO log
			return 1;
		}
		const ItemTemplate * iTemplate = GetItemTemplate(iTemplateIndex);

		for (UINT16 i = 0; i < slotsCount; i++)
		{
			if (inventorySlots[i].IsEmpty()) {

				stackCount -= iTemplate->maxStack;
				if (stackCount > 0)
				{
					inventorySlots[i].item = CreateNonDbItemFromTemplate(iTemplateIndex, iTemplate->maxStack);
					if (!inventorySlots[i].item) {
						//@TODO log
						stackCount += iTemplate->maxStack;
					}
				}
				else
				{
					stackCount += iTemplate->maxStack;
					inventorySlots[i].item = CreateNonDbItemFromTemplate(iTemplateIndex, stackCount);
					if (!inventorySlots[i].item) {
						//@TODO log
					}

					break;
				}

			}
		}

	}

	return stackCount < 0 ? 0 : stackCount;
}

INT32 Inventory::DestroyNonDbStack(UINT32 itemId, INT32 stackCount)
{
	UINT32 total = 0;

	for (UINT16 i = 0; i < slotsCount; i++)
	{
		if (inventorySlots[i].HasItem(itemId))
		{
			if (!inventorySlots[i].item->iTemplate->HasFlag(EItemTemplateFlags_IsNonDb)) {
				//@TODO log
				return 3;
			}

			total += inventorySlots[i].item->stackCount;
		}
	}

	if (total > stackCount) {
		//cant drop more stack than u have
		return 2;
	}

	for (UINT16 i = 0; i < slotsCount; i++)
	{
		if (inventorySlots[i].HasItem(itemId))
		{
			Item * item = inventorySlots[i].item;
			stackCount = item->stackCount - stackCount;

			if (stackCount < 0)
			{
				inventorySlots[i].Clear();
				stackCount = -stackCount;
			}
			else
			{
				item->stackCount = stackCount;
				break;
			}

		}
	}

	return 0;
}

UINT32 Inventory::ExtractNonDbStack(UINT32 itemId, INT32 stackCount)
{
	UINT32 total = 0;

	for (UINT16 i = 0; i < slotsCount; i++)
	{
		if (inventorySlots[i].HasItem(itemId))
		{
			if (!inventorySlots[i].item->iTemplate->HasFlag(EItemTemplateFlags_IsNonDb)) {
				//@TODO log
				return UINT32_MAX;
			}

			total += inventorySlots[i].item->stackCount;
		}
	}

	if (total > stackCount) {
		//cant drop more stack than u have
		return UINT32_MAX;
	}

	for (UINT16 i = 0; i < slotsCount; i++)
	{
		if (inventorySlots[i].HasItem(itemId))
		{
			Item * item = inventorySlots[i].item;

			stackCount = item->stackCount - stackCount;
			if (stackCount < 0) {
				total += item->stackCount;
				inventorySlots[i].Clear();

				stackCount = -stackCount;
			}
			else {
				total += stackCount;
				item->stackCount = stackCount;
				break;
			}
		}
	}

	return (UINT32)stackCount;
}

INT32 Inventory::Insert(Item * item)
{
	for (UINT16 i = 0; i < slotsCount; i++)
	{
		if (inventorySlots[i].IsEmpty()) {
			inventorySlots[i].item = item;
			return 0;
		}
	}

	return 1;
}

Item * Inventory::ExtractInvItem(UID itemUID)
{
	for (UINT16 i = 0; i < slotsCount; i++)
	{
		if (inventorySlots[i].HasItem(itemUID)) {
			return inventorySlots[i].Release();
		}
	}

	return nullptr;
}

Item * Inventory::GetItem(UINT32 itemId)
{
	for (UINT16 i = 0; i < PLAYER_INVENTORY_PROFILE_SLOTS_COUNT; i++)
	{
		if (profileSlots[i].HasItem(itemId)) {
			return profileSlots[i].item;
		}
	}

	for (UINT16 i = 0; i < slotsCount; i++)
	{
		if (inventorySlots[i].HasItem(itemId)) {
			return inventorySlots[i].item;
		}
	}

	return nullptr;
}

Item * Inventory::GetItem(UID itemUID)
{
	for (UINT16 i = 0; i < PLAYER_INVENTORY_PROFILE_SLOTS_COUNT; i++)
	{
		if (profileSlots[i].HasItem(itemUID)) {
			return profileSlots[i].item;
		}
	}

	for (UINT16 i = 0; i < slotsCount; i++)
	{
		if (inventorySlots[i].HasItem(itemUID)) {
			return inventorySlots[i].item;
		}
	}

	return nullptr;
}

INT32 Inventory::EquipeItem(UINT16 slotId, sql::Connection * sql)
{
	if (slotId >= (slotsCount + 40)) {
		return 1;
	}

	ISlot * slot = &inventorySlots[slotId - 40];
	if (slot->IsEmpty()) {
		return 2;
	}

	Item * item = slot->item;
	ItemTemplate * iTemplate = item->iTemplate;
	const ItemTemplateInventoryData * iTemplate2 = GetItemTemplateInventoryData(iTemplate->index);

	register UINT32 playerLevel = a_load_acquire(player->level);
	if (iTemplate2->AcceptsLevel(playerLevel))
	{
		//@TODO send chat message "Your level is to low to equipe that item!"
		return 0;
	}

	if (iTemplate2->AcceptsClass(player->pClass))
	{
		//@TODO send sys-msg "@28"
		return 0;
	}

	if (iTemplate2->AcceptsGender(player->pGender))
	{
		//@TODO send sys-msg [unk]
		return 0;
	}

	if (iTemplate2->AcceptsRace(player->pRace))
	{
		//@TODO send sys-msg [unk]
		return 0;
	}

	if (ArbiterConfig::player.soulbindingEnabled)
	{
		if (item->binderDBId && item->binderDBId != player->dbId) {
			//@TODO send sys-msg "@347"
			return 0;
		}

		//ignore soulbinding logic here??
	}

	EItemCategory iCategory = iTemplate->category;
	EItemType iCombatType = iTemplate->combatItemType;

	//@TODO optimize using array of handlers
	switch (iCombatType)
	{
	case EItemType_EQUIP_ACCESSORY:

		if (iCategory == EItemCategory_necklace)
		{
			InterchangeItems(&profileSlots[EProfileSlotType_NECKLACE], slot);
		}
		else if (iCategory == EItemCategory_earring)
		{
			if (profileSlots[EProfileSlotType_EARRING_L].IsEmpty())
			{
				InterchangeItems(&profileSlots[EProfileSlotType_EARRING_L], slot);
			}
			else if (profileSlots[EProfileSlotType_EARRING_R].IsEmpty())
			{
				InterchangeItems(&profileSlots[EProfileSlotType_EARRING_R], slot);
			}
			else
			{
				if (profileSlots[EProfileSlotType_EARRING_L].item->iTemplate->id == iTemplate->id)
				{
					InterchangeItems(&profileSlots[EProfileSlotType_EARRING_L], slot);
				}
				else
				{
					InterchangeItems(&profileSlots[EProfileSlotType_EARRING_R], slot);
				}
			}
		}
		else if (iCategory == EItemCategory_ring)
		{
			if (profileSlots[EProfileSlotType_RING_L].IsEmpty())
			{
				InterchangeItems(&profileSlots[EProfileSlotType_RING_L], slot);
			}
			else if (profileSlots[EProfileSlotType_RING_R].IsEmpty())
			{
				InterchangeItems(&profileSlots[EProfileSlotType_RING_R], slot);
			}
			else
			{
				if (profileSlots[EProfileSlotType_RING_L].item->iTemplate->id == iTemplate->id) 
				{
					InterchangeItems(&profileSlots[EProfileSlotType_RING_L], slot);
				}
				else
				{
					InterchangeItems(&profileSlots[EProfileSlotType_RING_R], slot);
				}
			}
		}
		else if (iCategory == EItemCategory_brooch)
		{
			InterchangeItems(&profileSlots[EProfileSlotType_BROOCH], slot);
		}
		else if (iCategory == EItemCategory_belt)
		{
			InterchangeItems(&profileSlots[EProfileSlotType_BELT], slot);
		}
		else if (iCategory == EItemCategory_accessoryHair)
		{
			InterchangeItems(&profileSlots[EProfileSlotType_HEAD_ADRONMENT], slot);
		}

		break;
	case EItemType_EQUIP_WEAPON:
		InterchangeItems(&profileSlots[EProfileSlotType_WEAPON], slot);
		break;
	case EItemType_EQUIP_STYLE_ACCESSORY:
		if (iCategory == EItemCategory_style_face) 
		{
			InterchangeItems(&profileSlots[EProfileSlotType_SKIN_FACE], slot);
		}
		else if (iCategory == EItemCategory_style_hair) 
		{
			InterchangeItems(&profileSlots[EProfileSlotType_SKIN_HEAD], slot);
		}
		break;
	case EItemType_EQUIP_ARMOR_BODY:
		InterchangeItems(&profileSlots[EProfileSlotType_ARMOR], slot);
		break;
	case EItemType_EQUIP_ARMOR_ARM:
		InterchangeItems(&profileSlots[EProfileSlotType_GLOVES], slot);
		break;
	case EItemType_EQUIP_ARMOR_LEG:
		break;
	case EItemType_EQUIP_INHERITANCE:
		InterchangeItems(&profileSlots[EProfileSlotType_BOOTS], slot);
		break;
	case EItemType_EQUIP_STYLE_WEAPON:
		InterchangeItems(&profileSlots[EProfileSlotType_SKIN_WEAPON], slot);
		break;
	case EItemType_EQUIP_STYLE_BODY:
		InterchangeItems(&profileSlots[EProfileSlotType_SKIN_BODY], slot);
		break;
	case EItemType_EQUIP_UNDERWEAR:
		InterchangeItems(&profileSlots[EProfileSlotType_INNERWARE], slot);
		break;
	case EItemType_EQUIP_STYLE_BACK:
		InterchangeItems(&profileSlots[EProfileSlotType_SKIN_BACK], slot);
		break;
	case EItemType_EQUIP_STYLE_EFFECT:
		//??
		break;
	case EItemType_CUSTOM:

		//crystals @TODO

		break;
	}

	//@TODO:
	//recalcultate stats if needed [check by item type]
	//send external change async if needed[exclude earring, rings, brooch etc]

	return 0;
}

INT32 Inventory::UnequipeItem(UINT16 slotId, sql::Connection * sql)
{
	if (slotId >= PLAYER_INVENTORY_PROFILE_SLOTS_COUNT) {
		//@TODO log
		return 1;
	}

	if (profileSlots[slotId].IsEmpty()) {
		//@TODO log
		return 2;
	}

	INT32 emptySlot = GetEmptySlotUnsafe();
	if (emptySlot == -1) {
		//@TODO send sys-msg "@39"
		return 0;
	}

	InterchangeItems(&inventorySlots[emptySlot], &profileSlots[slotId]);
	
	//@TODO:
	//recalcultate stats if needed [check by item type]
	//send external change async if needed[exclude earring, rings, brooch etc]

	return 0;
}

INT32 Inventory::EquipeCrystal(UINT16 profileSlotId, UINT16 slotId, sql::Connection * sql)
{
	if (profileSlots[profileSlotId].IsEmpty()) {
		return 1;
	}

	slotId -= 40;

	const ItemTemplateInventoryData * iTempalte = GetItemTemplateInventoryData(profileSlots[profileSlotId].item->iTemplate->index);
	UINT16 countOfSlots = iTempalte->countOfSlot;

	for (UINT16 i = 0; i < countOfSlots; i++)
	{
		if (profileSlots[profileSlotId].item->crystals[i] <= 0) 
		{

		}
	}

	return 0;
}

INT32 Inventory::UnequipeCrystal(UINT16 profileSlotId, UINT64 itemId, sql::Connection * sql)
{
	//@TODO
	return 0;
}

INT32 Inventory::Send(ConnectionNetPartial * net, bool show, bool safe)
{
	return INT32();
}

INT32 Inventory::SendItemLevels()
{
	//@TODO
	return 0;
}

INT32 Inventory::GetProfilePassivities(Passivity ** outPassivities)
{
	//@TODO
	return 0;
}

INT32 Inventory::RefreshEnchantEffect()
{
	//@TODO
	return 0;
}

INT32 Inventory::RefreshItemsModifiers()
{
	//@TODO
	return 0;
}
