#ifndef GAME_SERVER_MMO_ENTITIES_PICKUP_JOB_H
#define GAME_SERVER_MMO_ENTITIES_PICKUP_JOB_H

#include <game/server/entity.h>

enum
{
	PICKUP_JOB_TYPE_FARM,
	PICKUP_JOB_TYPE_MINE,
	PICKUP_JOB_TYPE_WOOD,

	PICKUP_JOB_STATE_MAGNITUDE = 2,
	PICKUP_JOB_SPAWN_CD = 30
};

class CPickupJob : public CEntity
{
	int m_aIDs[2];
	int m_Type;
	int m_DestroyProgress;
	int m_NextGrowTick;

public:
	CPickupJob(CGameWorld *pWorld, vec2 Pos, int Type);
	~CPickupJob();

	void Damage(int ClientID);

	int m_State;

	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_MMO_ENTITIES_PICKUP_JOB_H
