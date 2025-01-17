/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "projectile.h"
#include "character.h"

#include <engine/shared/config.h>

#include <game/generated/protocol.h>
#include <game/mapitems.h>

#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/generated/server_data.h>
#include <game/server/mmo/dummies/dummy_base.h>
#include <game/server/mmo/entities/big_boom_proj.h>

CProjectile::CProjectile(
	CGameWorld *pGameWorld,
	int Type,
	int Owner,
	vec2 Pos,
	vec2 Dir,
	int Span,
	bool Freeze,
	bool Explosive,
	int SoundImpact,
	bool BigBoom) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
	m_Type = Type;
	m_Pos = Pos;
	m_Direction = Dir;
	m_LifeSpan = Span;
	m_Owner = Owner;
	//m_Damage = Damage;
	m_SoundImpact = SoundImpact;
	m_StartTick = Server()->Tick();
	m_Explosive = Explosive;

	m_BigBoom = BigBoom;
	m_Freeze = Freeze;

	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));

	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	m_BelongsToPracticeTeam = pOwnerChar && pOwnerChar->Teams()->IsPractice(pOwnerChar->Team());

	GameWorld()->InsertEntity(this);
}

void CProjectile::Reset()
{
	m_MarkedForDestroy = true;
}

vec2 CProjectile::GetPos(float Time)
{
	float Curvature = 0;
	float Speed = 0;

	switch(m_Type)
	{
	case WEAPON_GRENADE:
		Curvature = GameServer()->Tuning()->m_GrenadeCurvature;
		Speed = GameServer()->Tuning()->m_GrenadeSpeed;

		break;

	case WEAPON_SHOTGUN:
		Curvature = GameServer()->Tuning()->m_ShotgunCurvature;
		Speed = GameServer()->Tuning()->m_ShotgunSpeed;

		break;

	case WEAPON_GUN:
		Curvature = GameServer()->Tuning()->m_GunCurvature;
		Speed = GameServer()->Tuning()->m_GunSpeed;

		break;
	}

	return CalcPos(m_Pos, m_Direction, Curvature, Speed, Time);
}

void CProjectile::Tick()
{
	float Pt = (Server()->Tick() - m_StartTick - 1) / (float)Server()->TickSpeed();
	float Ct = (Server()->Tick() - m_StartTick) / (float)Server()->TickSpeed();
	vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
	vec2 ColPos;
	vec2 NewPos;
	int Collide = GameServer()->Collision()->IntersectLine(PrevPos, CurPos, &ColPos, &NewPos);
	CCharacter *pOwnerChar = 0;

	if(m_Owner >= 0)
		pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

	CCharacter *pTargetChr = 0x0;
	CDummyBase *pTargetDummy = 0x0;

	if(pOwnerChar ? !pOwnerChar->GrenadeHitDisabled() : g_Config.m_SvHit)
		pTargetChr = GameServer()->m_World.IntersectCharacter(PrevPos, ColPos, m_Freeze ? 1.0f : 6.0f, ColPos, pOwnerChar, m_Owner);
	if(!pTargetChr)
		pTargetDummy = GameWorld()->IntersectDummy(PrevPos, ColPos, 6.f, ColPos);

	if(m_LifeSpan > -1)
		m_LifeSpan--;

	CClientMask TeamMask = CClientMask().set();
	bool IsWeaponCollide = false;
	if(
		pOwnerChar &&
		pTargetChr &&
		pOwnerChar->IsAlive() &&
		pTargetChr->IsAlive() &&
		!pTargetChr->CanCollide(m_Owner))
	{
		IsWeaponCollide = true;
	}
	if(pOwnerChar && pOwnerChar->IsAlive())
	{
		TeamMask = pOwnerChar->TeamMask();
	}
	else if(m_Owner >= 0 && (m_Type != WEAPON_GRENADE || g_Config.m_SvDestroyBulletsOnDeath || m_BelongsToPracticeTeam))
	{
		m_MarkedForDestroy = true;
		return;
	}

	if(((pTargetChr && (pOwnerChar ? !pOwnerChar->GrenadeHitDisabled() : g_Config.m_SvHit || m_Owner == -1)) || Collide || GameLayerClipped(CurPos) || pTargetDummy) && !IsWeaponCollide)
	{
		// && (!pTargetChr || (m_Type == WEAPON_SHOTGUN && Collide))
		if(m_Explosive)
		{
			int Number = 1;
			if(GameServer()->EmulateBug(BUG_GRENADE_DOUBLEEXPLOSION) && m_LifeSpan == -1)
			{
				Number = 2;
			}
			for(int i = 0; i < Number; i++)
			{
				GameServer()->CreateExplosion(ColPos, m_Owner, m_Type, m_Owner == -1, (!pTargetChr ? -1 : pTargetChr->Team()),
					(m_Owner != -1) ? TeamMask : CClientMask().set());
				GameServer()->CreateSound(ColPos, m_SoundImpact,
					(m_Owner != -1) ? TeamMask : CClientMask().set());

				if (m_BigBoom)
				{
					for (int j = 0; j < 4; j++)
					{
						vec2 Dir = direction(random_float() * pi * 2) * 5.f;
						new CBigBoomProjectile(GameWorld(), PrevPos, Dir, m_Owner);
					}
				}
			}
		}

		if(Collide && m_Bouncing != 0)
		{
			m_StartTick = Server()->Tick();
			m_Pos = NewPos + (-(m_Direction * 4));
			if(m_Bouncing == 1)
				m_Direction.x = -m_Direction.x;
			else if(m_Bouncing == 2)
				m_Direction.y = -m_Direction.y;
			if(fabs(m_Direction.x) < 1e-6f)
				m_Direction.x = 0;
			if(fabs(m_Direction.y) < 1e-6f)
				m_Direction.y = 0;
			m_Pos += m_Direction;
		}
		else if(m_Type == WEAPON_GUN)
		{
			int Dmg = g_pData->m_Weapons.m_Gun.m_pBase->m_Damage;
			if (pTargetChr)
				pTargetChr->TakeDamage(vec2(0, 0), Dmg, m_Owner, WEAPON_GUN);
			if (pTargetDummy)
				pTargetDummy->TakeDamage(vec2(0, 0), Dmg, m_Owner, WEAPON_GUN);
			m_MarkedForDestroy = true;
			return;
		}
		else if(m_Type == WEAPON_SHOTGUN)
		{
			int Dmg = g_pData->m_Weapons.m_Shotgun.m_pBase->m_Damage;
			if (pTargetChr)
				pTargetChr->TakeDamage(vec2(0, 0), Dmg, m_Owner, WEAPON_SHOTGUN);
			if (pTargetDummy)
				pTargetDummy->TakeDamage(vec2(0, 0), Dmg, m_Owner, WEAPON_SHOTGUN);
			m_MarkedForDestroy = true;
			return;
		}
		else
		{
			if(!m_Freeze)
			{
				m_MarkedForDestroy = true;
				return;
			}
		}
	}
	if(m_LifeSpan == -1)
	{
		if(m_Explosive)
		{
			if(m_Owner >= 0)
				pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

			TeamMask = CClientMask().set();
			if(pOwnerChar && pOwnerChar->IsAlive())
			{
				TeamMask = pOwnerChar->TeamMask();
			}

			GameServer()->CreateExplosion(ColPos, m_Owner, m_Type, m_Owner == -1, (!pOwnerChar ? -1 : pOwnerChar->Team()),
				(m_Owner != -1) ? TeamMask : CClientMask().set());
			GameServer()->CreateSound(ColPos, m_SoundImpact,
				(m_Owner != -1) ? TeamMask : CClientMask().set());
		}
		m_MarkedForDestroy = true;
		return;
	}

	int x = GameServer()->Collision()->GetIndex(PrevPos, CurPos);
	int z;
	if(g_Config.m_SvOldTeleportWeapons)
		z = GameServer()->Collision()->IsTeleport(x);
	else
		z = GameServer()->Collision()->IsTeleportWeapon(x);
	CGameControllerDDRace *pControllerDDRace = (CGameControllerDDRace *)GameServer()->m_pController;
	if(z && !pControllerDDRace->m_TeleOuts[z - 1].empty())
	{
		int TeleOut = GameServer()->m_World.m_Core.RandomOr0(pControllerDDRace->m_TeleOuts[z - 1].size());
		m_Pos = pControllerDDRace->m_TeleOuts[z - 1][TeleOut];
		m_StartTick = Server()->Tick();
	}
}

void CProjectile::TickPaused()
{
	++m_StartTick;
}

void CProjectile::FillInfo(CNetObj_Projectile *pProj)
{
	pProj->m_X = (int)m_Pos.x;
	pProj->m_Y = (int)m_Pos.y;
	pProj->m_VelX = (int)(m_Direction.x * 100.0f);
	pProj->m_VelY = (int)(m_Direction.y * 100.0f);
	pProj->m_StartTick = m_StartTick;
	pProj->m_Type = m_Type;
}

void CProjectile::Snap(int SnappingClient)
{
	float Ct = (Server()->Tick() - m_StartTick) / (float)Server()->TickSpeed();

	if(NetworkClipped(SnappingClient, GetPos(Ct)))
		return;

	if(m_LifeSpan == -2)
	{
		CNetObj_EntityEx *pEntData = Server()->SnapNewItem<CNetObj_EntityEx>(GetID());
		if(!pEntData)
			return;

		pEntData->m_SwitchNumber = m_Number;
		pEntData->m_Layer = m_Layer;
		pEntData->m_EntityClass = ENTITYCLASS_PROJECTILE;
	}

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	if(SnappingClientVersion < VERSION_DDNET_SWITCH)
	{
		CCharacter *pSnapChar = GameServer()->GetPlayerChar(SnappingClient);
		int Tick = (Server()->Tick() % Server()->TickSpeed()) % ((m_Explosive) ? 6 : 20);
		if(pSnapChar && pSnapChar->IsAlive() && (m_Layer == LAYER_SWITCH && m_Number > 0 && !Switchers()[m_Number].m_aStatus[pSnapChar->Team()] && (!Tick)))
			return;
	}

	CCharacter *pOwnerChar = 0;
	CClientMask TeamMask = CClientMask().set();

	if(m_Owner >= 0)
		pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

	if(pOwnerChar && pOwnerChar->IsAlive())
		TeamMask = pOwnerChar->TeamMask();

	if(SnappingClient != SERVER_DEMO_CLIENT && m_Owner != -1 && !TeamMask.test(SnappingClient))
		return;

	CNetObj_DDNetProjectile DDNetProjectile;
	if(SnappingClientVersion >= VERSION_DDNET_ANTIPING_PROJECTILE && FillExtraInfo(&DDNetProjectile))
	{
		int Type = SnappingClientVersion < VERSION_DDNET_MSG_LEGACY ? (int)NETOBJTYPE_PROJECTILE : NETOBJTYPE_DDNETPROJECTILE;
		void *pProj = Server()->SnapNewItem(Type, GetID(), sizeof(DDNetProjectile));
		if(!pProj)
		{
			return;
		}
		mem_copy(pProj, &DDNetProjectile, sizeof(DDNetProjectile));
	}
	else
	{
		CNetObj_Projectile *pProj = Server()->SnapNewItem<CNetObj_Projectile>(GetID());
		if(!pProj)
		{
			return;
		}
		FillInfo(pProj);
	}
}

void CProjectile::SwapClients(int Client1, int Client2)
{
	m_Owner = m_Owner == Client1 ? Client2 : m_Owner == Client2 ? Client1 : m_Owner;
}

// DDRace

void CProjectile::SetBouncing(int Value)
{
	m_Bouncing = Value;
}

bool CProjectile::FillExtraInfo(CNetObj_DDNetProjectile *pProj)
{
	const int MaxPos = 0x7fffffff / 100;
	if(abs((int)m_Pos.y) + 1 >= MaxPos || abs((int)m_Pos.x) + 1 >= MaxPos)
	{
		//If the modified data would be too large to fit in an integer, send normal data instead
		return false;
	}
	//Send additional/modified info, by modifying the fields of the netobj
	float Angle = -atan2f(m_Direction.x, m_Direction.y);

	int Data = 0;
	Data |= (abs(m_Owner) & 255) << 0;
	if(m_Owner < 0)
		Data |= PROJECTILEFLAG_NO_OWNER;
	//This bit tells the client to use the extra info
	Data |= PROJECTILEFLAG_IS_DDNET;
	// PROJECTILEFLAG_BOUNCE_HORIZONTAL, PROJECTILEFLAG_BOUNCE_VERTICAL
	Data |= (m_Bouncing & 3) << 10;
	if(m_Explosive)
		Data |= PROJECTILEFLAG_EXPLOSIVE;
	if(m_Freeze)
		Data |= PROJECTILEFLAG_FREEZE;

	pProj->m_X = (int)(m_Pos.x * 100.0f);
	pProj->m_Y = (int)(m_Pos.y * 100.0f);
	pProj->m_Angle = (int)(Angle * 1000000.0f);
	pProj->m_Data = Data;
	pProj->m_StartTick = m_StartTick;
	pProj->m_Type = m_Type;
	return true;
}
