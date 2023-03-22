#include "slime.h"

#include <engine/server.h>

CSlimeController::CSlimeController()
{
	m_Dir = -1;
	m_NextChangeDirTick = 0;
}

void CSlimeController::Tick()
{
	if (Server()->Tick() > m_NextChangeDirTick)
	{
		m_Dir *= -1;
		m_NextChangeDirTick = Server()->Tick() + Server()->TickSpeed() * 3 + rand() % 200;
	}

	if (rand() % 30 == 0)
		Jump();

	SetMove(m_Dir);
	SetAimX(m_Dir * 100);
}
