#ifndef GAME_SERVER_MMO_ENTITIES_PICKUP_PHYS_H
#define GAME_SERVER_MMO_ENTITIES_PICKUP_PHYS_H

#include <game/server/entity.h>

enum
{
	PICKUP_PHYS_TYPE_XP,
	PICKUP_PHYS_TYPE_MONEY,
	PICKUP_PHYS_TYPE_ITEM
};

class CPickupPhys : public CEntity
{
	friend class CCharacter;

	vec2 m_Vel;
	int m_Type;
	int m_ItemID;
	int m_Count;
	int m_DestroyTick;

public:
	CPickupPhys(CGameWorld *pWorld, vec2 Pos, vec2 Vel, int Type, int Count, int ItemID = -1);

	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_MMO_ENTITIES_PICKUP_PHYS_H
