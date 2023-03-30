#ifndef GAME_SERVER_MMO_COMPONENTS_CLAN_MANAGER_H
#define GAME_SERVER_MMO_COMPONENTS_CLAN_MANAGER_H

#include <game/server/mmo/component.h>

#include <engine/server/databases/connection_pool.h>
#include <engine/console.h>

struct SClanCreateResult;
struct SClanDeleteResult;

class CClanManager : public CServerComponent
{
	static void ChatCreateClan(IConsole::IResult *pResult, void *pUserData);
	static void ChatDeleteClan(IConsole::IResult *pResult, void *pUserData);

	// DB Threads
	static bool CreateClanThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool DeleteClanThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);

	CDbConnectionPool *DBPool();

public:
	virtual void OnConsoleInit() override;
	virtual void OnTick() override;

	void CreateClan(int ClientID, const char *pClanName);
	void DeleteClan(int ClientID);
};

struct SClanResultBase : ISqlResult
{
	SClanResultBase()
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

struct SClanCreateResult : SClanResultBase
{
	SClanCreateResult()
	{
		m_aClanName[0] = '\0';
	}

	char m_aClanName[32];
};

struct SClanCreateRequest : ISqlData
{
	SClanCreateRequest(std::shared_ptr<SClanCreateResult> pResult) :
		ISqlData(std::move(pResult))
	{
		m_aClanName[0] = '\0';
	}

	char m_aClanName[32];
};

#endif // GAME_SERVER_MMO_COMPONENTS_CLAN_MANAGER_H
