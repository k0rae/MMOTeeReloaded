#include "box.h"

#include <game/server/mmo/mmo_core.h>

void CBox::Init(CMMOCore *pCore)
{
	m_pMMOCore = pCore;
}

void CBox::AddItem(int ItemID, int Count)
{
	SBoxItem Item;
	Item.m_ID = ItemID;
	Item.m_Count = Count;
	Item.m_Rand = -1;

	m_vItems.push_back(Item);
}

void CBox::AddRareItem(int ItemID, int Rand, int Count)
{
	SBoxItem Item;
	Item.m_ID = ItemID;
	Item.m_Count = Count;
	Item.m_Rand = Rand;

	m_vRareItems.push_back(Item);
}

void CBox::Open(int ClientID, int Count)
{
	if (Count > 50)
		Count = 50;

	// Get loot from boxes
	std::vector<SBoxItem> vGetItems;

	for (int i = 0; i < Count; i++)
	{
		SBoxItem Item = m_vItems[rand() % m_vItems.size()];
		for (SBoxItem Rare : m_vRareItems)
			if (rand() % Rare.m_Rand == 0)
				Item = Rare;

		vGetItems.push_back(Item);

		m_pMMOCore->GiveItem(ClientID, Item.m_ID, Item.m_Count);
	}


}
