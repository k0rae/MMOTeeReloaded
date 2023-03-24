#ifndef GAME_SERVER_MMO_MMO_CORE_H
#define GAME_SERVER_MMO_MMO_CORE_H

#include <base/vmath.h>
#include <engine/shared/protocol.h>

#include "components/account_manager.h"

enum
{
	BOT_IDS_OFFSET = 24
};

class CGameContext;

class CMMOCore
{
	CGameContext *m_pGameServer;

	CGameContext *GameServer() { return m_pGameServer; }
	class IServer *Server();
	class CGameWorld *GameWorld();

	int m_aBotSnapIDs[MAX_CLIENTS];

	std::vector<SInvItem> m_vItems;

	SInvItem *GetItem(int ItemID);

public:
	void Init(CGameContext *pGameServer);

	int GetExpForLevelUp(int Level);
	int GetExpForLevelUpWork(int WorkID, int Level);
	const char *GetWorkName(int WorkID);

	void GetProgressBar(char *pStr, int StrSize, char Filler, char Empty, int Num, int MaxNum);

	int GetNextBotSnapID(int ClientID);
	void CreateDummy(vec2 Pos, int DummyType);
	void ClearBotSnapIDs();

	const char *GetItemName(int ItemID);
	int GetItemType(int ItemID);
	int GetItemRarity(int ItemID);
	const char *GetQualityString(int Quality);
	const char *GetRarityString(int Rarity);

	void GiveItem(int ClientID, int ItemID, int Count = 1, int Quality = QUALITY_0, int Data = 0);
	void UseItem(int ClientID, int ItemID, int Count);
};

#endif // GAME_SERVER_MMO_MMO_CORE_H
