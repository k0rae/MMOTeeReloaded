#ifndef GAME_SERVER_MMO_ACCOUNT_DATA_H
#define GAME_SERVER_MMO_ACCOUNT_DATA_H

#include <vector>

enum
{
	MAX_LOGIN_LENGTH = 64,

	// Works
	WORK_FARMER,
	WORK_MINER,
	WORK_MATERIAL,
	WORK_FISHER,
	WORK_NUM
};

struct SAccountData
{
	char m_aAccountName[MAX_LOGIN_LENGTH];
	int m_ID;
	int m_Level;
	int m_EXP;
	int m_Money;
	int m_Donate;
};

struct SWorkData
{
	int m_EXP = 0;
	int m_Level = 1;
};

struct SAccountWorksData
{
	SWorkData m_aWorks[WORK_NUM];
};

struct SInvItem
{
	char m_aName[128];
	int m_ID;
	int m_Count;
	int m_Quality;
	int m_Data;
	int m_Type;
	int m_Rarity;
};

class CAccountInventory
{
public:
	std::vector<SInvItem> m_vItems;

	void AddItem(SInvItem Item)
	{
		auto it = std::find_if(m_vItems.begin(), m_vItems.end(), [&](SInvItem i) {
			return (i.m_ID == Item.m_ID);
		});

		if (it == m_vItems.end())
		{
			m_vItems.push_back(Item);
		}
		else
			it->m_Count += Item.m_Count;
	}

	void RemItem(int ItemID, int Count = -1)
	{
		auto it = std::find_if(m_vItems.begin(), m_vItems.end(), [&](SInvItem Item) {
			return (Item.m_ID == ItemID);
		});

		if (it == m_vItems.end())
			return;

		if (Count == -1)
			m_vItems.erase(it);
		else
		{
			it->m_Count -= Count;
			if (it->m_Count <= 0)
				m_vItems.erase(it);
		}
	}

	SInvItem GetItem(int ItemID)
	{
		auto it = std::find_if(m_vItems.begin(), m_vItems.end(), [&](SInvItem Item) {
			return (Item.m_ID == ItemID);
		});

		if (it != m_vItems.end())
			return *it;

		return {{'\0'}, -1, 0};
	}

	int ItemCount(int ItemID)
	{
		return GetItem(ItemID).m_Count;
	}

	bool HaveItem(int ItemID)
	{
		return (GetItem(ItemID).m_Count != 0);
	}
};

#endif // GAME_SERVER_MMO_ACCOUNT_DATA_H
