#ifndef GAME_SERVER_MMO_ACCOUNT_MANAGER_H
#define GAME_SERVER_MMO_ACCOUNT_MANAGER_H

#include <game/server/mmo/component.h>
#include <game/server/mmo/items.h>

#include <engine/server/databases/connection_pool.h>
#include <engine/console.h>
#include <base/hash.h>

struct SAccountRegisterResult;
struct SAccountLoginResult;

enum
{
	MAX_LOGIN_LENGTH = 64
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
	static bool SaveThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);

	CDbConnectionPool *DBPool();

public:
	virtual void OnConsoleInit() override;
	virtual void OnTick() override;

	void Register(int ClientID, const char *pName, const char *pPasswordHash);
	void Login(int ClientID, const char *pName, const char *pPasswordHash);

	void Save(int ClientID);
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

struct SAccountSaveRequest : ISqlData
{
	SAccountSaveRequest() :
		ISqlData(nullptr)
	{}

	SAccountData m_AccData;
	CAccountInventory m_AccInventory;
};

#endif // GAME_SERVER_MMO_ACCOUNT_MANAGER_H
