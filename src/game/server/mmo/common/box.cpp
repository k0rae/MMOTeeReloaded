#include "box.h"

void CBox::Init(CMMOCore *pCore)
{
	m_pMMOCore = pCore;
}

void CBox::AddItem(int ItemID, int Rand, int Count)
{
	SBoxItem Item;
	Item.m_ID = ItemID;
	Item.m_Count = Count;
	Item.m_Rand = Rand;

	m_vItems.push_back(Item);
}

void CBox::Open(int ClientID, int Count)
{
	if (Count > 50)
		Count = 50;

	for (SBoxItem Item : m_vItems)
	{

	}
}
