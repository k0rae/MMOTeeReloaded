#ifndef GAME_SERVER_MMO_ACCOUNT_MANAGER_H
#define GAME_SERVER_MMO_ACCOUNT_MANAGER_H

#include <game/server/mmo/component.h>
#include <game/server/mmo/items.h>

#include <engine/server/databases/connection_pool.h>
#include <engine/console.h>
#include <base/hash.h>

struct SAccountRegisterResult;
struct SAccountLoginResult;

struct SAccountData
{
	int m_ID;
	int m_Level;
	int m_EXP;
	int m_Money;
	int m_Donate;
};

struct SInvItem
{
	int m_ID;
	int m_Count;
	int m_Quality;
	int m_Data;
};

class CAccountInventory
{
public:
	std::vector<SInvItem> m_vItems;

	void AddItem(int ItemID, int Count = 1, int Quality = QUALITY_0, int Data = 0)
	{
		auto it = std::find_if(m_vItems.begin(), m_vItems.end(), [&](SInvItem Item) {
			return (Item.m_ID == ItemID);
		});

		if (it == m_vItems.end())
		{
			SInvItem NewItem = { ItemID, Count, Quality, Data };
			m_vItems.push_back(NewItem);
		}
		else
			it->m_Count += Count;
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

		return {0, 0, 0, 0};
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

enum
{
	MAX_LOGIN_LENGTH = 64
};

class CAccountManager : public CServerComponent
{
	std::vector<std::shared_ptr<SAccountRegisterResult>> m_vpRegisterResults;
	std::vector<std::shared_ptr<SAccountLoginResult>> m_vpLoginResults;

	// Chat commands
	static void ChatRegister(IConsole::IResult *pResult, void *pUserData);
	static void ChatLogin(IConsole::IResult *pResult, void *pUserData);

	// Threads
	static bool RegisterThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool LoginThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);

	CDbConnectionPool *DBPool();

public:
	virtual void OnConsoleInit() override;
	virtual void OnTick() override;

	void Register(int ClientID, const char *pName, const char *pPasswordHash);
	void Login(int ClientID, const char *pName, const char *pPasswordHash);
};

struct SAccountResultBase : ISqlResult
{
	SAccountResultBase()
	{
		m_aMessage[0] = '\0';
		m_State = STATE_FAILED;
		m_ClientID = -1;
	}

	enum
	{
		STATE_FAILED = -1,
		STATE_SUCCESSFUL
	};

	int m_State;
	char m_aMessage[512];
	int m_ClientID;
};

struct SAccountRegisterResult : SAccountResultBase
{
	SAccountRegisterResult() = default;
};

struct SAccountRegisterRequest : ISqlData
{
	SAccountRegisterRequest(std::shared_ptr<SAccountRegisterResult> pResult) :
		ISqlData(std::move(pResult))
	{
		m_aLogin[0] = '\0';
		m_aPasswordHash[0] = '\0';
	}

	char m_aLogin[MAX_LOGIN_LENGTH];
	char m_aPasswordHash[MD5_MAXSTRSIZE];
};

struct SAccountLoginResult : SAccountResultBase
{
	SAccountLoginResult() = default;

	SAccountData m_AccData;
	CAccountInventory m_AccInv;
};

struct SAccountLoginRequest : ISqlData
{
	SAccountLoginRequest(std::shared_ptr<SAccountLoginResult> pResult) :
		ISqlData(std::move(pResult))
	{
		m_aLogin[0] = '\0';
		m_aPasswordHash[0] = '\0';
	}

	char m_aLogin[MAX_LOGIN_LENGTH];
	char m_aPasswordHash[MD5_MAXSTRSIZE];
};

#endif // GAME_SERVER_MMO_ACCOUNT_MANAGER_H
