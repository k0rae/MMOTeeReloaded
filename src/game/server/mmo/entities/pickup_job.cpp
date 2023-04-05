#include "pickup_job.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <cstdlib>

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
	if (m_Type == PICKUP_JOB_TYPE_FARM)
		for (int i : m_aIDs)
			Server()->SnapFreeID(i);
}

int CPickupJob::GetDamage(int ClientID)
{
	int Damage = 20;
	if (m_Type == PICKUP_JOB_TYPE_MINE) {
		CPlayer *pPly = GameServer()->m_apPlayers[ClientID];
		Damage = 5;
		for (int i = ITEM_COPPER_PICKAXE; i < ITEM_COPPER_PICKAXE + PICKAXES_COUNT; i++)
		{
			if (pPly->m_AccInv.HaveItem(i))
				Damage = 10 + 5 * (i - ITEM_COPPER_PICKAXE);
		}
	}

	return Damage;
}

void CPickupJob::Damage(int ClientID)
{
	// WARNING: SHIT CODE!

	if (m_State == 0)
		return;
	CPlayer *pPly = GameServer()->m_apPlayers[ClientID];

	int WorkID = -1;
	int GainXP = 1;
	if (m_Type == PICKUP_JOB_TYPE_FARM)
		WorkID = WORK_FARMER;
	else if (m_Type == PICKUP_JOB_TYPE_MINE)
	{
		WorkID = WORK_MINER;
	}
	else if (m_Type == PICKUP_JOB_TYPE_MATERIAL) {
		GainXP = 10;
		WorkID = WORK_MATERIAL;
	}

	m_DestroyProgress = std::min(100, m_DestroyProgress + GetDamage(ClientID));

	int WorkLevel = ((WorkID == -1) ? 0 : pPly->m_AccWorks.m_aWorks[WorkID].m_Level);

	// Send damage progress
	char aProgress[20 + 1];
	MMOCore()->GetProgressBar(aProgress, sizeof(aProgress), ':', ' ', m_DestroyProgress, 100);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "Harvest progress:\n[%s] %d%%\nWork %s:\nLv: %d | EXP: %d/%d",
		aProgress, m_DestroyProgress, MMOCore()->GetWorkName(WorkID),
		WorkLevel,
		(WorkID == -1) ? 0 : pPly->m_AccWorks.m_aWorks[WorkID].m_EXP,
		MMOCore()->GetExpForLevelUpWork(WorkID, WorkLevel));

	GameServer()->SendMMOBroadcast(ClientID, 1.5f, aBuf);

	// If pickup was collected
	if (m_DestroyProgress >= 100)
	{
		m_State = 0;
		m_DestroyProgress = 0;
		m_NextGrowTick = Server()->Tick() + Server()->TickSpeed() * PICKUP_JOB_SPAWN_CD;

		GameServer()->CreateSound(m_Pos, SOUND_NINJA_HIT);

		// Give items
		if (m_Type == PICKUP_JOB_TYPE_FARM)
		{
			// Give vegetables
			int Item = ITEM_CARROT;
			int Count = pPly->m_AccWorks.m_aWorks[WorkID].m_Level;
			Count += floor((double)Count * (m_State / 2 + 0.5f));

			switch(rand() % 5)
			{
			case 0: Item = ITEM_TOMATO; break;
			case 1: Item = ITEM_POTATO; break;
			}

			MMOCore()->GiveItem(ClientID, Item, Count);

			// Give box
			if (rand() % 40 == 0)
				MMOCore()->GiveItem(ClientID, ITEM_FARMER_BOX);
		}
		else if (m_Type == PICKUP_JOB_TYPE_MINE)
		{
			int Level = pPly->m_AccWorks.m_aWorks[WorkID].m_Level;
			int Item = MMOCore()->GetRandomMinerItemByLevel(Level);

			MMOCore()->GiveItem(ClientID, Item, 1 + ((Item <= ITEM_DIAMOND) ? Level / 15 : 0));
		}
		else if (m_Type == PICKUP_JOB_TYPE_WOOD)
		{
			MMOCore()->GiveItem(ClientID, ITEM_WOOD, 1 + rand() % 3);
		}
		else if (m_Type == PICKUP_JOB_TYPE_MATERIAL)
		{
			if (!MMOCore()->GiveItem(ClientID, ITEM_MATERIAL, 25 + pPly->m_AccWorks.m_aWorks[WorkID].m_Level * 3)) {
				str_format(aBuf, sizeof(aBuf), "You have reached the maximum amount of materials. Go sell them in a shop!");
				GameServer()->SendChatTarget(ClientID, aBuf);
				return;
			}
		}

		// Give exp
		if (WorkID != -1)
		{
			pPly->AddWorkEXP(WorkID, GainXP);
			str_format(aBuf, sizeof(aBuf), "+%d %s work exp", GainXP, MMOCore()->GetWorkName(WorkID));
			GameServer()->SendChatTarget(ClientID, aBuf);
		}
	}

	GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP);
}

void CPickupJob::Tick()
{
	if (m_State < 4 && Server()->Tick() > m_NextGrowTick)
	{
		m_State++;

		m_NextGrowTick = Server()->Tick() + Server()->TickSpeed() * PICKUP_JOB_SPAWN_CD;
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

	float s = sin(Server()->Tick() / 30.f);

	for (int i = 0; i < 2; i++)
	{
		if (i + 2 > m_State)
			continue;

		int Magnitude = PICKUP_JOB_STATE_MAGNITUDE * (i + 1);

		CNetObj_Pickup *pPickup2 = Server()->SnapNewItem<CNetObj_Pickup>(m_aIDs[i]);
		if(!pPickup2)
			return;

		pPickup2->m_X = (int)m_Pos.x + s * Magnitude + Magnitude * 2;
		pPickup2->m_Y = m_Pos.y - (i + 1) * 16;
		pPickup2->m_Type = POWERUP_ARMOR;
		pPickup2->m_Subtype = 0;
	}
}
