#ifndef GAME_SERVER_MMO_ACCOUNT_MANAGER_H
#define GAME_SERVER_MMO_ACCOUNT_MANAGER_H

#include <game/server/mmo/component.h>

class CAccountManager : public CServerComponent
{
public:
	CAccountManager();

	void Register(int ClientID, const char *pName, const char *pPasswordHash);
	void Login(int ClientID, const char *pName, const char *pPasswordHash);
};

#endif // GAME_SERVER_MMO_ACCOUNT_MANAGER_H
