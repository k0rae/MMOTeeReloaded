#ifndef GAME_SERVER_MMO_ENTITIES_BIG_BOOM_PROJ_H
#define GAME_SERVER_MMO_ENTITIES_BIG_BOOM_PROJ_H

#include <game/server/entity.h>

class CBigBoomProjectile : public CEntity
{
	vec2 m_Vel;
	int m_Owner;
	int m_BoomTick;

public:
	CBigBoomProjectile(CGameWorld *pWorld, vec2 Pos, vec2 Vel, int Owner);

	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_MMO_ENTITIES_BIG_BOOM_PROJ_H
