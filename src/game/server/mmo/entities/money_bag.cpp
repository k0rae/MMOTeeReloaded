#include "money_bag.h"

#include <engine/server.h>
#include <game/server/entity.h>
#include <game/generated/protocol.h>
#include <game/server/player.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>

CMoneyBag::CMoneyBag(CGameWorld *pWorld, vec2 Pos) :
	CEntity(pWorld, CGameWorld::ENTTYPE_MONEY_BAG, Pos, 20.f)
{
	m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * (rand() % 300 + 60 * 5);

	GameWorld()->InsertEntity(this);
}

void CMoneyBag::Tick()
{
	if (Server()->Tick() < m_RespawnTick)
		return;

	CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, GetProximityRadius(), 0x0);
	if (pChr)
	{
		int Count = rand() % 5 + 1;
		int Bonus = (Server()->Tick() - m_RespawnTick) / (60 * 60);
		int ClientID = pChr->GetPlayer()->GetCID();

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%s found Secret Bag! Got %d + Time %d Money Bag!",
			Server()->ClientName(ClientID), Count, Bonus);
		GameServer()->SendChatTarget(-1, aBuf);

		m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * (rand() % 300 + 60 * 5);

		MMOCore()->GiveItem(ClientID, ITEM_MONEY_BAG, Count + Bonus);
	}
}

void CMoneyBag::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	if (Server()->Tick() < m_RespawnTick)
		return;

	CNetObj_Projectile *pPickup = Server()->SnapNewItem<CNetObj_Projectile>(GetID());
	if(!pPickup)
		return;

	pPickup->m_X = (int)m_Pos.x;
	pPickup->m_Y = (int)m_Pos.y;
	pPickup->m_VelX = 0;
	pPickup->m_VelY = 0;
	pPickup->m_StartTick = Server()->Tick() - 4;
	pPickup->m_Type = WEAPON_LASER;
}
