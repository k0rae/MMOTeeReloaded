#include "pickup_job.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>

CPickupJob::CPickupJob(CGameWorld *pWorld, vec2 Pos, int Type) :
	CEntity(pWorld, CGameWorld::ENTTYPE_PICKUP_JOB, Pos)
{
	m_Type = Type;
	m_State = 3; // 3 is max grow for PICKUP_JOB_TYPE_FARM or "is ready for harvest"

	// Alloc IDs for farm type
	if (Type == PICKUP_JOB_TYPE_FARM)
		for (int &i : m_aIDs)
			i = Server()->SnapNewID();

	GameWorld()->InsertEntity(this);
}

CPickupJob::~CPickupJob()
{
	for (int i : m_aIDs)
		Server()->SnapFreeID(i);
}

void CPickupJob::Damage(int ClientID)
{
	if (m_State == 0)
		return;

	m_DestroyProgress += 20;

	if (m_DestroyProgress >= 100)
	{
		m_State = 0;
		m_DestroyProgress = 0;
		m_NextGrowTick = Server()->Tick() + Server()->TickSpeed() * 10;

		GameServer()->CreateSound(m_Pos, SOUND_NINJA_HIT);

		// Give item
		if (m_Type == PICKUP_JOB_TYPE_FARM)
		{
			int Item = ITEM_CARROT;
			switch(rand() % 5)
			{
			case 0: Item = ITEM_TOMATO; break;
			case 1: Item = ITEM_POTATO; break;
			}

			GameServer()->m_MMOCore.GiveItem(ClientID, Item);
		}
	}

	GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP);
}

void CPickupJob::Tick()
{
	if (m_State < 4 && Server()->Tick() > m_NextGrowTick)
	{
		m_State++;

		m_NextGrowTick = Server()->Tick() + Server()->TickSpeed() * 10;
	}
}

void CPickupJob::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;
	if (m_State == 0)
		return;

	CNetObj_Pickup *pPickup = Server()->SnapNewItem<CNetObj_Pickup>(GetID());
	if(!pPickup)
		return;

	pPickup->m_X = (int)m_Pos.x;
	pPickup->m_Y = (int)m_Pos.y;

	if (m_Type == PICKUP_JOB_TYPE_FARM || m_Type == PICKUP_JOB_TYPE_MINE)
		pPickup->m_Type = POWERUP_ARMOR;
	else
		pPickup->m_Type = POWERUP_HEALTH;
	pPickup->m_Subtype = 0;

	// Snap farm grow states
	if (m_Type != PICKUP_JOB_TYPE_FARM)
		return;

	for (int i = 0; i < 2; i++)
	{
		if (i + 2 > m_State)
			continue;

		int Magnitude = PICKUP_JOB_STATE_MAGNITUDE * (i + 1);

		CNetObj_Pickup *pPickup2 = Server()->SnapNewItem<CNetObj_Pickup>(m_aIDs[i]);
		if(!pPickup2)
			return;

		pPickup2->m_X = (int)m_Pos.x + sin(Server()->Tick() / 30.f) * Magnitude + Magnitude * 2;
		pPickup2->m_Y = m_Pos.y - (i + 1) * 16;
		pPickup2->m_Type = POWERUP_ARMOR;
		pPickup2->m_Subtype = 0;
	}
}
