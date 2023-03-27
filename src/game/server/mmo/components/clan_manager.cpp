#include "clan_manager.h"

#include <engine/server/server.h>

CDbConnectionPool *CClanManager::DBPool() { return ((CServer *)Server())->DbPool(); }

bool CClanManager::CreateClanThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	return false;
}

bool CClanManager::DeleteClanThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	return false;
}

void CClanManager::CreateClan(int ClientID, const char *pClanName)
{

}

void CClanManager::DeleteClan(int ClientID)
{

}

void CClanManager::ChatCreateClan(IConsole::IResult *pResult, void *pUserData)
{
	((CClanManager *)pUserData)->CreateClan(pResult->m_ClientID, pResult->GetString(0));
}

void CClanManager::ChatDeleteClan(IConsole::IResult *pResult, void *pUserData)
{
	((CClanManager *)pUserData)->DeleteClan(pResult->m_ClientID);
}

void CClanManager::OnConsoleInit()
{

}

void CClanManager::OnTick()
{

}
