#ifndef GAME_SERVER_MMO_ACCOUNT_DATA_H
#define GAME_SERVER_MMO_ACCOUNT_DATA_H

#include <vector>

enum
{
	MAX_LOGIN_LENGTH = 64,

	// Works
	WORK_FARMER = 0,
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

struct SAccountUpgrade
{
	int m_UpgradePoints;
	int m_SkillPoints;
	int m_Damage;
	int m_FireSpeed;
	int m_Health;
	int m_HealthRegen;
	int m_Ammo;
	int m_AmmoRegen;
	int m_Spray;
	int m_Mana;

	int &operator[](int Index)
	{
		switch(Index)
		{
		case 0: return m_UpgradePoints;
		case 1: return m_SkillPoints;
		case 2: return m_Damage;
		case 3: return m_FireSpeed;
		case 4: return m_Health;
		case 5: return m_HealthRegen;
		case 6: return m_Ammo;
		case 7: return m_AmmoRegen;
		case 8: return m_Spray;
		case 9: return m_Mana;
		default: return m_UpgradePoints;
		}
	}

	int get_by(int Index) const
	{
		switch(Index)
		{
		case 0: return m_UpgradePoints;
		case 1: return m_SkillPoints;
		case 2: return m_Damage;
		case 3: return m_FireSpeed;
		case 4: return m_Health;
		case 5: return m_HealthRegen;
		case 6: return m_Ammo;
		case 7: return m_AmmoRegen;
		case 8: return m_Spray;
		case 9: return m_Mana;
		default: return m_UpgradePoints;
		}
	}
};

enum
{
	UPGRADE_POINTS,
	UPGRADE_SKILL_POINTS,
	UPGRADE_DAMAGE,
	UPGRADE_FIRE_SPEED,
	UPGRADE_HEALTH,
	UPGRADE_HEALTH_REGEN,
	UPGRADE_AMMO,
	UPGRADE_AMMO_REGEN,
	UPGRADE_SPRAY,
	UPGRADE_MANA,
	UPGRADES_NUM
};

#endif // GAME_SERVER_MMO_ACCOUNT_DATA_H
