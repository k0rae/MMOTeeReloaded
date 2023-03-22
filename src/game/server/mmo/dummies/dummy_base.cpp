#include "dummy_base.h"

#include <game/server/gamecontext.h>

CDummyBase::CDummyBase(CGameWorld *pWorld, vec2 Pos) :
	CEntity(pWorld, CGameWorld::ENTTYPE_DUMMY, Pos)
{
	GameWorld()->InsertEntity(this);

	m_Core.Init(&GameWorld()->m_Core, Collision());

	m_Core.m_Pos = Pos;

	GameWorld()->m_Core.m_vDummies.push_back(&m_Core);
}

void CDummyBase::Destroy()
{
	for (int i = 0; i < GameWorld()->m_Core.m_vDummies.size(); i++)
		if (GameWorld()->m_Core.m_vDummies[i] == &m_Core)
			GameWorld()->m_Core.m_vDummies.erase(GameWorld()->m_Core.m_vDummies.begin() + i);

	delete this;
}

void CDummyBase::Tick()
{
	// Some physic
	m_Core.Tick(false);
	m_Core.Move();

	m_Pos = m_Core.m_Pos;
}

void CDummyBase::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	int SelfID = GameServer()->GetNextBotSnapID(SnappingClient);
	if (SelfID == -1)
	{
		dbg_msg("dummy", "cant get dummy snap ID for %d(%s). cheat?", SnappingClient, Server()->ClientName(SnappingClient));
		return;
	}

	// Snap player
	CNetObj_ClientInfo *pClientInfo = Server()->SnapNewItem<CNetObj_ClientInfo>(SelfID);
	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, m_aName);
	StrToInts(&pClientInfo->m_Clan0, 3, m_aClan);
	StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfo.m_aSkinName);
	pClientInfo->m_Country = 0;
	pClientInfo->m_UseCustomColor = m_TeeInfo.m_UseCustomColor;
	pClientInfo->m_ColorBody = m_TeeInfo.m_ColorBody;
	pClientInfo->m_ColorFeet = m_TeeInfo.m_ColorFeet;

	CNetObj_PlayerInfo *pPlayerInfo = Server()->SnapNewItem<CNetObj_PlayerInfo>(SelfID);
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_Latency = 0;
	pPlayerInfo->m_Score = 0;
	pPlayerInfo->m_Local = 0;
	pPlayerInfo->m_ClientID = SelfID;
	pPlayerInfo->m_Team = 10;

	// Snap character
	CNetObj_Character *pCharacter = Server()->SnapNewItem<CNetObj_Character>(SelfID);
	if(!pCharacter)
		return;

	m_Core.Write(pCharacter);

	pCharacter->m_Tick = Server()->Tick();
	pCharacter->m_Emote = EMOTE_NORMAL;
	pCharacter->m_HookedPlayer = -1;
	pCharacter->m_AttackTick = 0;
	pCharacter->m_Direction = 0;
	pCharacter->m_Weapon = 0;
	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;
	pCharacter->m_PlayerFlags = PLAYERFLAG_PLAYING;
}
