#ifndef GAME_SERVER_MMO_COMPONENTS_CLAN_MANAGER_H
#define GAME_SERVER_MMO_COMPONENTS_CLAN_MANAGER_H

#include <game/server/mmo/component.h>

#include <engine/server/databases/connection_pool.h>
#include <engine/console.h>

struct SClanCreateResult {};
struct SClanDeleteResult {};

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

#endif // GAME_SERVER_MMO_COMPONENTS_CLAN_MANAGER_H
