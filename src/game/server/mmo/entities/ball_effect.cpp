#include "ball_effect.h"

#include <engine/server.h>

CBallEffect::CBallEffect(CGameWorld *pWorld, vec2 Pos) :
	CEntity(pWorld, CGameWorld::ENTTYPE_BALL_EFFECT, Pos)
{
	m_ID2 = Server()->SnapNewID();

	GameWorld()->InsertEntity(this);
}

CBallEffect::~CBallEffect()
{
	Server()->SnapFreeID(m_ID2);
}

void CBallEffect::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	for (int i = 0; i < 2; i++)
	{
		CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem<CNetObj_Projectile>((i == 0) ? GetID() : m_ID2));
		if(!pObj)
			return;

		int Mul = (i == 0) ? 1 : -1;
		vec2 Pos = m_Pos + vec2(cosf(Server()->Tick() / 20.f) * Mul * ms_Radius, sinf(Server()->Tick() / 20.f) * Mul * ms_Radius);

		pObj->m_X = Pos.x;
		pObj->m_Y = Pos.y;
		pObj->m_VelX = 0;
		pObj->m_VelY = 0;
		pObj->m_StartTick = Server()->Tick();
		pObj->m_Type = WEAPON_HAMMER;
	}
}
