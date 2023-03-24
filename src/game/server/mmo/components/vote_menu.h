#ifndef GAME_SERVER_MMO_COMPONENTS_VOTE_MENU_H
#define GAME_SERVER_MMO_COMPONENTS_VOTE_MENU_H

#include <game/server/mmo/component.h>
#include <game/voting.h>

#include <engine/shared/protocol.h>

#include <vector>

enum
{
	MENU_NO_AUTH,
	MENU_MAIN,
	MENU_INFO,
	MENU_INVENTORY,
	MENU_EQUIP,
	MENU_UPGRADE,
	MENU_MAIL,
};

class CVoteMenu : public CServerComponent
{
	std::vector<CVoteOptionServer> m_aPlayersVotes[MAX_PLAYERS];
	int m_aPlayersMenu[MAX_PLAYERS];

	void ListInventory(int ClientID, int Type);
	void ItemInfo(int ClientID, int ItemID);

public:
	CVoteMenu();

	virtual void OnMessage(int ClientID, int MsgID, void *pRawMsg, bool InGame) override;
	virtual void OnPlayerLeft(int ClientID) override;

	void SetMenu(int ClientID, int MenuID) { m_aPlayersMenu[ClientID] = MenuID; };

	void AddMenuVote(int ClientID, const char *pCmd, const char *pDesc);
	void AddMenuChangeVote(int ClientID, int Menu, const char *pDesc);
	void ClearVotes(int ClientID);
	void RebuildMenu(int ClientID);
	void AddBack(int ClientID, int Menu);
};

#endif // GAME_SERVER_MMO_COMPONENTS_VOTE_MENU_H
