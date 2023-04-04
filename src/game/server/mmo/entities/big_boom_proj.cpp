#include "big_boom_proj.h"

#include <game/server/gamecontext.h>
#include <engine/server.h>
#include <game/collision.h>

CBigBoomProjectile::CBigBoomProjectile(CGameWorld *pWorld, vec2 Pos, vec2 Vel, int Owner) :
	CEntity(pWorld, CGameWorld::ENTTYPE_BIG_BOOM_PROJ, Pos)
{
	m_Vel = Vel;
	m_Owner = Owner;
	m_BoomTick = Server()->Tick() + Server()->TickSpeed() * 3;

	GameWorld()->InsertEntity(this);
}

void CBigBoomProjectile::Tick()
{
	m_Vel.y += GameServer()->Tuning()->m_Gravity;
	m_Vel.x *= 0.98f;
	Collision()->MoveBox(&m_Pos, &m_Vel, vec2(5, 5), 0.5f);

	if (Server()->Tick() > m_BoomTick)
	{
		GameServer()->CreateExplosion(m_Pos, m_Owner, WEAPON_GRENADE, false, 0);
		Destroy();
	}
}

void CBigBoomProjectile::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem<CNetObj_Projectile>(GetID()));
	if(!pObj)
		return;

	pObj->m_X = m_Pos.x;
	pObj->m_Y = m_Pos.y;
	pObj->m_VelX = 0;
	pObj->m_VelY = 0;
	pObj->m_StartTick = Server()->Tick();
	pObj->m_Type = WEAPON_HAMMER;
}
