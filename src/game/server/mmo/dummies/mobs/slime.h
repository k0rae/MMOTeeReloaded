#ifndef GAME_SERVER_MMO_DUMMIES_MOBS_SLIME_H
#define GAME_SERVER_MMO_DUMMIES_MOBS_SLIME_H

#include <game/server/mmo/dummies/dummy_controller.h>

class CSlimeController : public CDummyController
{
	int m_Dir;
	int m_NextChangeDirTick;

public:
	CSlimeController();

	virtual void Tick() override;
};

#endif // GAME_SERVER_MMO_DUMMIES_MOBS_SLIME_H
