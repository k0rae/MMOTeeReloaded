#ifndef GAME_SERVER_MMO_ENTITIES_MONEY_BAG_H
#define GAME_SERVER_MMO_ENTITIES_MONEY_BAG_H

#include <game/server/entity.h>

class CMoneyBag : public CEntity
{
	int m_RespawnTick;

public:
	CMoneyBag(CGameWorld *pWorld, vec2 Pos);

	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_MMO_ENTITIES_MONEY_BAG_H
