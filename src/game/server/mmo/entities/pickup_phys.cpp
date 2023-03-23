#include "pickup_phys.h"

#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/entities/character.h>

#include <game/generated/protocol.h>

CPickupPhys::CPickupPhys(CGameWorld *pWorld, vec2 Pos, vec2 Vel, int Type, int Count, int ItemID) :
	CEntity(pWorld, CGameWorld::ENTTYPE_PICKUP_PHYS, Pos, 20.f)
{
	m_Vel = Vel;
	m_Type = Type;
	m_Count = Count;
	m_ItemID = ItemID;
	m_DestroyTick = Server()->Tick() + Server()->TickSpeed() * 15;

	GameWorld()->InsertEntity(this);
}

void CPickupPhys::Tick()
{
	// Physic
	m_Vel.x *= 0.98f;
	m_Vel.y += GameServer()->Tuning()->m_Gravity;

	Collision()->MoveBox(&m_Pos, &m_Vel, vec2(GetProximityRadius(), GetProximityRadius()), 0.3f);

	// Pickup handle
	CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, GetProximityRadius(), 0x0);

	if (pChr)
	{
		CPlayer *pPly = pChr->GetPlayer();
		if (m_Type == PICKUP_PHYS_TYPE_XP)
		{
			pPly->AddEXP(m_Count);
			GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR);
			Destroy();

			return;
		}
		else if (m_Type == PICKUP_PHYS_TYPE_MONEY)
		{
			pPly->m_AccData.m_Money += m_Count;
			GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
			Destroy();

			return;
		}
	}

	if (m_DestroyTick < Server()->Tick())
		Destroy();
}

void CPickupPhys::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	int DeadTick = m_DestroyTick - Server()->Tick();

	if (DeadTick < 150 && DeadTick % 50 < 25)
		return;

	CNetObj_Pickup *pPickup = Server()->SnapNewItem<CNetObj_Pickup>(GetID());
	if(!pPickup)
		return;

	pPickup->m_X = (int)m_Pos.x;
	pPickup->m_Y = (int)m_Pos.y;

	if (m_Type == PICKUP_PHYS_TYPE_XP)
		pPickup->m_Type = POWERUP_ARMOR;
	else if (m_Type == PICKUP_PHYS_TYPE_MONEY)
		pPickup->m_Type = POWERUP_HEALTH;
	else if (m_Type == PICKUP_PHYS_TYPE_ITEM)
		pPickup->m_Type = POWERUP_WEAPON;
	pPickup->m_Subtype = 0;
}
