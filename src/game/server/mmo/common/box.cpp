#include "box.h"

#include <game/server/mmo/mmo_core.h>
#include <game/server/gamecontext.h>

void CBox::Init(CMMOCore *pCore)
{
	m_pMMOCore = pCore;
}

void CBox::AddItem(int ItemID, int Count)
{
	SBoxEntry Item;
	Item.m_ID = ItemID;
	Item.m_Count = Count;
	Item.m_Rand = -1;

	m_vItems.push_back(Item);
}

void CBox::AddRareItem(int ItemID, int Rand, int Count)
{
	SBoxEntry Item;
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
	std::vector<SBoxEntry> vGetItems;

	for (int i = 0; i < Count; i++)
	{
		SBoxEntry Item = m_vItems[rand() % m_vItems.size()];
		for (SBoxEntry Rare : m_vRareItems)
		{
			if (rand() % Rare.m_Rand == 0)
				Item = Rare;
		}

		vGetItems.push_back(Item);
	}

	// Build string
	std::vector<SBoxEntry> vClean;

	for (SBoxEntry Item : vGetItems)
	{
		bool Found = false;
		for (auto &i : vClean)
		{
			if (i.m_ID == Item.m_ID)
			{
				Found = true;
				i.m_Count += Item.m_Count;
			}
		}

		if (!Found)
			vClean.push_back(Item);
	}

	// Give items
	for (SBoxEntry &Entry : vClean)
		m_pMMOCore->GiveItem(ClientID, Entry.m_ID, Entry.m_Count);

	std::string Text = std::string(m_pMMOCore->Server()->ClientName(ClientID)) + " opened box and got ";
	for (SBoxEntry Item : vClean)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%s x%d, ", m_pMMOCore->GetItemName(Item.m_ID), Item.m_Count);
		Text += aBuf;
	}

	// Notify players on server
	m_pMMOCore->GameServer()->SendChatTarget(-1, Text.c_str());
}
