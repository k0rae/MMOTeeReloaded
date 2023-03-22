#include "slime.h"
#include "game/server/entities/character.h"

#include <engine/server.h>

CSlimeController::CSlimeController()
{
	m_Dir = -1;
	m_NextChangeDirTick = 0;
}

void CSlimeController::Tick()
{
	// Random walk
	if (Server()->Tick() > m_NextChangeDirTick)
	{
		m_Dir *= -1;
		m_NextChangeDirTick = Server()->Tick() + Server()->TickSpeed() * 3 + rand() % 200;
	}

	// Random jump
	if (rand() % 30 == 0)
		Jump();

	// Find characters
	CCharacter *pChr = GameWorld()->ClosestCharacter(m_Pos, 320.f, 0x0);

	if (pChr)
	{
		if (m_Pos.x > pChr->m_Pos.x)
			m_Dir = -1;
		else
			m_Dir = 1;

		if (Server()->Tick() % 5 == 0 && distance(m_Pos, pChr->m_Pos) < 63.f)
			Fire();
	}

	SetMove(m_Dir);
	if (pChr)
	{
		vec2 RelPos = pChr->m_Pos - m_Pos;
		SetAim(RelPos);
	}
	else
		SetAimX(m_Dir * 100);
}
