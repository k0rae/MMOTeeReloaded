#include "dummy_base.h"

#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/entities/character.h>
#include <game/server/entities/laser.h>
#include <game/server/entities/projectile.h>

#include <game/server/mmo/entities/pickup_phys.h>

#include <game/mapitems.h>

// Mobs
#include "mobs/slime.h"

CDummyBase::CDummyBase(CGameWorld *pWorld, vec2 Pos, int DummyType) :
	CEntity(pWorld, CGameWorld::ENTTYPE_DUMMY, Pos, 28.f)
{
	GameWorld()->InsertEntity(this);
	GameWorld()->m_Core.m_vDummies.push_back(&m_Core);
	m_Core.Init(&GameWorld()->m_Core, Collision());

	m_SpawnPos = Pos;
	m_NoDamage = false;
	m_DummyType = DummyType;
	m_DefaultEmote = EMOTE_NORMAL;

	str_copy(m_aName, "[NULL BOT]");
	str_copy(m_aClan, "");

	// Create dummy controller
	switch(m_DummyType)
	{
	case DUMMY_TYPE_STAND:
		m_pDummyController = 0x0;
		break;
	case DUMMY_TYPE_SLIME:
		m_pDummyController = new CSlimeController();
		break;
	default:
		m_pDummyController = 0x0;
		dbg_msg("dummy", "invalid dummy type: %d", m_DummyType);
	}

	if (m_pDummyController)
		m_pDummyController->m_pDummyBase = this;

	Spawn();
}

CDummyBase::~CDummyBase()
{
	switch(m_DummyType)
	{
	case DUMMY_TYPE_SLIME:
		delete (CSlimeController *)m_pDummyController;
		break;
	}
}

void CDummyBase::Spawn()
{
	m_Health = 10;
	m_Armor = 0;
	m_Alive = true;
	m_SpawnTick = 0;
	m_EmoteType = EMOTE_NORMAL;
	m_EmoteStop = 0;

	m_Core.m_Pos = m_SpawnPos;
	m_Pos = m_SpawnPos;
	m_Core.m_Vel = vec2(0, 0);

	GameServer()->CreatePlayerSpawn(m_Pos);
}

void CDummyBase::Die(int Killer)
{
	m_Alive = false;
	m_SpawnTick = Server()->Tick() + Server()->TickSpeed();

	if (Killer >= 0 && rand() % 3 == 0)
	{
		int Level = 1;

		CPlayer *pPly = GameServer()->m_apPlayers[Killer];
		if (pPly && pPly->m_LoggedIn)
			Level = pPly->m_AccData.m_Level;

		int RandomForce = 3 - rand() % 7;
		new CPickupPhys(
			GameWorld(),
			m_Pos,
			m_Core.m_Vel + vec2(RandomForce, RandomForce),
			(rand() % 2 == 0) ? PICKUP_PHYS_TYPE_XP : PICKUP_PHYS_TYPE_MONEY,
			10 * sqrt(Level));
	}

	GameServer()->CreateDeath(m_Pos, 0);
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE);
}

void CDummyBase::TakeDamage(vec2 Force, int Damage, int From, int Weapon)
{
	if (!m_Alive)
		return;

	if (From >= 0)
		Damage += MMOCore()->GetPlusDamage(From);

	if (Damage)
	{
		// Emote
		m_EmoteType = EMOTE_PAIN;
		m_EmoteStop = Server()->Tick() + 500 * Server()->TickSpeed() / 1000;

		if(m_Armor)
		{
			if(Damage <= m_Armor)
			{
				m_Armor -= Damage;
				Damage = 0;
			}
			else
			{
				Damage -= m_Armor;
				m_Armor = 0;
			}
		}

		if(From >= 0)
			m_Health -= Damage;

		if(From >= 0 && GameServer()->m_apPlayers[From])
		{
			//GameServer()->CreateSound(m_Pos, SOUND_HIT, -1);

			//int Steal = (100 - Server()->GetItemCount(From, ACCESSORY_ADD_STEAL_HP) > 30) ? 100 - Server()->GetItemCount(From, ACCESSORY_ADD_STEAL_HP) > 30 : 30;
			//pFrom->m_Health += Steal;
		}
	}

	if (m_Health <= 0)
		Die(From);

	vec2 Temp = m_Core.m_Vel + Force;
	m_Core.m_Vel = Temp;
}

void CDummyBase::FireWeapon()
{
	vec2 Direction = normalize(vec2(m_Input.m_TargetX, m_Input.m_TargetY));

	bool FullAuto = false;
	if(m_Core.m_ActiveWeapon == WEAPON_GRENADE || m_Core.m_ActiveWeapon == WEAPON_SHOTGUN || m_Core.m_ActiveWeapon == WEAPON_LASER)
		FullAuto = true;

	bool WillFire = (m_Input.m_Fire & 1) && (FullAuto || !(m_PrevInput.m_Fire & 1));
	if (!WillFire)
		return;

	vec2 ProjStartPos = m_Pos + Direction * 28.f * 0.75f;

	switch(m_Core.m_ActiveWeapon)
	{
	case WEAPON_HAMMER:
	{
		// reset objects Hit
		GameServer()->CreateSound(m_Pos, SOUND_HAMMER_FIRE);

		if(m_Core.m_HammerHitDisabled)
			break;

		CEntity *apEnts[MAX_CLIENTS];
		int Hits = 0;
		int Num = GameServer()->m_World.FindEntities(ProjStartPos, GetProximityRadius() * 0.5f, apEnts,
			MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

		for(int i = 0; i < Num; ++i)
		{
			auto *pTarget = static_cast<CCharacter *>(apEnts[i]);

			if(!pTarget->IsAlive())
				continue;

			// set his velocity to fast upward (for now)
			if(length(pTarget->m_Pos - ProjStartPos) > 0.0f)
				GameServer()->CreateHammerHit(pTarget->m_Pos - normalize(pTarget->m_Pos - ProjStartPos) * GetProximityRadius() * 0.5f);
			else
				GameServer()->CreateHammerHit(ProjStartPos);

			vec2 Dir;
			if(length(pTarget->m_Pos - m_Pos) > 0.0f)
				Dir = normalize(pTarget->m_Pos - m_Pos);
			else
				Dir = vec2(0.f, -1.f);

			float Strength = GameServer()->Tuning()->m_HammerStrength;

			vec2 Temp = pTarget->GetCore().m_Vel + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f;
			Temp = ClampVel(pTarget->m_MoveRestrictions, Temp);
			Temp -= pTarget->GetCore().m_Vel;
			pTarget->TakeDamage((vec2(0.f, -1.0f) + Temp) * Strength, 3,
				-1, m_Core.m_ActiveWeapon);

			Hits++;
		}

		// if we Hit anything, we have to wait for to reload
		if(Hits)
		{
			float FireDelay = GameServer()->Tuning()->m_HammerHitFireDelay;
			m_ReloadTimer = FireDelay * Server()->TickSpeed() / 1000;
		}
	}
	break;

	case WEAPON_GUN:
	{
		int Lifetime = (int)(Server()->TickSpeed() * GameServer()->Tuning()->m_GunLifetime);

		new CProjectile(
			GameWorld(),
			WEAPON_GUN, //Type
			-1, //Owner
			ProjStartPos, //Pos
			Direction, //Dir
			Lifetime, //Span
			false, //Freeze
			false, //Explosive
			-1 //SoundImpact
		);

		GameServer()->CreateSound(m_Pos, SOUND_GUN_FIRE);
	}
	break;

	case WEAPON_SHOTGUN:
	{
		float LaserReach = GameServer()->Tuning()->m_LaserReach;

		new CLaser(&GameServer()->m_World, m_Pos, Direction, LaserReach, -1, WEAPON_SHOTGUN);
		GameServer()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE);
	}
	break;

	case WEAPON_GRENADE:
	{
		int Lifetime = (int)(Server()->TickSpeed() * GameServer()->Tuning()->m_GrenadeLifetime);

		new CProjectile(
			GameWorld(),
			WEAPON_GRENADE, //Type
			-1, //Owner
			ProjStartPos, //Pos
			Direction, //Dir
			Lifetime, //Span
			false, //Freeze
			true, //Explosive
			SOUND_GRENADE_EXPLODE //SoundImpact
		); //SoundImpact

		GameServer()->CreateSound(m_Pos, SOUND_GRENADE_FIRE);
	}
	break;

	case WEAPON_LASER:
	{
		float LaserReach = GameServer()->Tuning()->m_LaserReach;

		new CLaser(GameWorld(), m_Pos, Direction, LaserReach, -1, WEAPON_LASER);
		GameServer()->CreateSound(m_Pos, SOUND_LASER_FIRE);
	}
	break;
	}
}

void CDummyBase::HandleTiles(int Tile)
{
	if (Tile == TILE_OFF_DAMAGE)
		Die(-1);
}

void CDummyBase::Destroy()
{
	for (int i = 0; i < GameWorld()->m_Core.m_vDummies.size(); i++)
		if (GameWorld()->m_Core.m_vDummies[i] == &m_Core)
			GameWorld()->m_Core.m_vDummies.erase(GameWorld()->m_Core.m_vDummies.begin() + i);

	delete this;
}

void CDummyBase::Tick()
{
	m_Core.m_Alive = m_Alive;

	if (Server()->Tick() > m_SpawnTick && !m_Alive)
		Spawn();

	// Don't calc phys if dummy is dead
	if (!m_Alive)
		return;

	if (m_pDummyController)
		m_pDummyController->DummyTick();

	m_Core.m_Input = m_Input;
	m_Core.m_ActiveWeapon = m_Input.m_WantedWeapon;
	m_Core.Tick(true);
	m_Core.Move();
	m_Pos = m_Core.m_Pos;

	FireWeapon();

	int Tile = Collision()->GetTile(m_Pos.x, m_Pos.y);
	HandleTiles(Tile);

	m_PrevInput = m_Input;
}

void CDummyBase::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	int SelfID = GameServer()->m_MMOCore.GetNextBotSnapID(SnappingClient);
	if (SelfID == -1)
	{
		dbg_msg("dummy", "cant get dummy snap ID for %d(%s). cheat?", SnappingClient, Server()->ClientName(SnappingClient));
		return;
	}

	// Snap player
	CNetObj_ClientInfo *pClientInfo = Server()->SnapNewItem<CNetObj_ClientInfo>(SelfID);
	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, m_aName);
	StrToInts(&pClientInfo->m_Clan0, 3, m_aClan);
	StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfo.m_aSkinName);
	pClientInfo->m_Country = 0;
	pClientInfo->m_UseCustomColor = m_TeeInfo.m_UseCustomColor;
	pClientInfo->m_ColorBody = m_TeeInfo.m_ColorBody;
	pClientInfo->m_ColorFeet = m_TeeInfo.m_ColorFeet;

	CNetObj_PlayerInfo *pPlayerInfo = Server()->SnapNewItem<CNetObj_PlayerInfo>(SelfID);
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_Latency = 0;
	pPlayerInfo->m_Score = 0;
	pPlayerInfo->m_Local = 0;
	pPlayerInfo->m_ClientID = SelfID;
	pPlayerInfo->m_Team = 10;

	// Don't snap character if dummy is dead
	if (!m_Alive)
		return;

	// Snap character
	CNetObj_Character *pCharacter = Server()->SnapNewItem<CNetObj_Character>(SelfID);
	if(!pCharacter)
		return;

	m_Core.Write(pCharacter);

	pCharacter->m_Tick = Server()->Tick();
	pCharacter->m_Emote = (m_EmoteStop > Server()->Tick()) ? m_EmoteType : m_DefaultEmote;
	pCharacter->m_HookedPlayer = -1;
	pCharacter->m_AttackTick = 0;
	pCharacter->m_Direction = m_Input.m_Direction;
	pCharacter->m_Weapon = m_Input.m_WantedWeapon;
	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;
	pCharacter->m_PlayerFlags = PLAYERFLAG_PLAYING;
}
