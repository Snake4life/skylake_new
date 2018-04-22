#include "Inventory.h"

#include "../Models/DataStore.h"

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
	//@TODO
	return 0;
}

INT32 Inventory::UnequipeItem(UINT16 slotId, sql::Connection * sql)
{
	//@TODO
	return 0;
}

INT32 Inventory::EquipeCrystal(UINT16 profileSlotId, UINT16 slotId, sql::Connection * sql)
{
	//@TODO
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
