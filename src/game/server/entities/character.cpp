/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "character.h"
#include "laser.h"
#include "projectile.h"

#include <engine/shared/config.h>

#include <game/generated/protocol.h>
#include <game/generated/server_data.h>
#include <game/mapitems.h>

#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>
#include <game/server/teams.h>

#include <cstdlib>
#include <game/server/mmo/dummies/dummy_base.h>
#include <game/server/mmo/entities/pickup_job.h>
#include <game/server/mmo/entities/pickup_phys.h>

MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS)

// Character, "physical" player's part
CCharacter::CCharacter(CGameWorld *pWorld, CNetObj_PlayerInput LastInput) :
	CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER, vec2(0, 0), CCharacterCore::PhysicalSize())
{
	m_Health = 0;
	m_Armor = 0;
	m_StrongWeakID = 0;

	m_Input = LastInput;
	// never initialize both to zero
	m_Input.m_TargetX = 0;
	m_Input.m_TargetY = -1;

	m_LatestPrevPrevInput = m_LatestPrevInput = m_LatestInput = m_PrevInput = m_SavedInput = m_Input;

	m_LastTimeCp = -1;
	m_LastTimeCpBroadcasted = -1;
	for(float &CurrentTimeCp : m_aCurrentTimeCp)
	{
		CurrentTimeCp = 0.0f;
	}
}

void CCharacter::Reset()
{
	Destroy();
}

bool CCharacter::Spawn(CPlayer *pPlayer, vec2 Pos)
{
	m_EmoteStop = -1;
	m_LastAction = -1;
	m_LastNoAmmoSound = -1;
	m_LastWeapon = WEAPON_HAMMER;
	m_QueuedWeapon = -1;
	m_LastRefillJumps = false;
	m_LastPenalty = false;
	m_LastBonus = false;

	m_TeleGunTeleport = false;
	m_IsBlueTeleGunTeleport = false;

	m_pPlayer = pPlayer;
	m_Pos = Pos;

	mem_zero(&m_LatestPrevPrevInput, sizeof(m_LatestPrevPrevInput));
	m_LatestPrevPrevInput.m_TargetY = -1;
	m_NumInputs = 0;
	m_SpawnTick = Server()->Tick();
	m_WeaponChangeTick = Server()->Tick();

	m_Core.Reset();
	m_Core.Init(&GameServer()->m_World.m_Core, Collision());
	m_Core.m_ActiveWeapon = WEAPON_GUN;
	m_Core.m_Pos = m_Pos;
	m_Core.m_Id = m_pPlayer->GetCID();
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;

	m_ReckoningTick = 0;
	m_SendCore = CCharacterCore();
	m_ReckoningCore = CCharacterCore();

	GameServer()->m_World.InsertEntity(this);
	m_Alive = true;

	GameServer()->m_pController->OnCharacterSpawn(this);

	DDRaceInit();

	m_TuneZone = Collision()->IsTune(Collision()->GetMapIndex(Pos));
	m_TuneZoneOld = -1; // no zone leave msg on spawn
	m_NeededFaketuning = 0; // reset fake tunings on respawn and send the client
	SendZoneMsgs(); // we want an enter message also on spawn
	GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);

	Server()->StartRecord(m_pPlayer->GetCID());

	// MMOTee stuff
	m_MaxHealth = 10;
	m_MaxArmor = 0;

	m_MaxHealth += pPlayer->m_AccUp.m_Health * 40;
	m_MaxHealth += MMOCore()->ArmorHealth(MMOCore()->GetEquippedItem(m_pPlayer->GetCID(), ITEM_TYPE_ARMOR_BODY));
	m_MaxHealth += MMOCore()->ArmorHealth(MMOCore()->GetEquippedItem(m_pPlayer->GetCID(), ITEM_TYPE_ARMOR_FEET));
	m_MaxArmor += MMOCore()->ArmorDefense(MMOCore()->GetEquippedItem(m_pPlayer->GetCID(), ITEM_TYPE_ARMOR_BODY));
	m_MaxArmor += MMOCore()->ArmorDefense(MMOCore()->GetEquippedItem(m_pPlayer->GetCID(), ITEM_TYPE_ARMOR_FEET));

	m_Health = m_MaxHealth;
	m_Armor = m_MaxArmor;

	return true;
}

void CCharacter::Destroy()
{
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	m_Alive = false;
	SetSolo(false);
}

void CCharacter::SetWeapon(int W)
{
	if(W == m_Core.m_ActiveWeapon)
		return;

	m_LastWeapon = m_Core.m_ActiveWeapon;
	m_QueuedWeapon = -1;
	m_Core.m_ActiveWeapon = W;
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH, TeamMask());

	if(m_Core.m_ActiveWeapon < 0 || m_Core.m_ActiveWeapon >= NUM_WEAPONS)
		m_Core.m_ActiveWeapon = 0;
}

void CCharacter::SetJetpack(bool Active)
{
	m_Core.m_Jetpack = Active;
}

void CCharacter::SetSolo(bool Solo)
{
	m_Core.m_Solo = Solo;
	Teams()->m_Core.SetSolo(m_pPlayer->GetCID(), Solo);

	if(Solo)
		m_NeededFaketuning |= FAKETUNE_SOLO;
	else
		m_NeededFaketuning &= ~FAKETUNE_SOLO;

	GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
}

void CCharacter::SetSuper(bool Super)
{
	m_Core.m_Super = Super;
	if(Super)
	{
		m_TeamBeforeSuper = Team();
		Teams()->SetCharacterTeam(GetPlayer()->GetCID(), TEAM_SUPER);
		m_DDRaceState = DDRACE_CHEAT;
	}
	else
	{
		Teams()->SetForceCharacterTeam(GetPlayer()->GetCID(), m_TeamBeforeSuper);
	}
}

void CCharacter::SetLiveFrozen(bool Active)
{
	m_Core.m_LiveFrozen = Active;
}

void CCharacter::SetDeepFrozen(bool Active)
{
	m_Core.m_DeepFrozen = Active;
}

bool CCharacter::IsGrounded()
{
	if(Collision()->CheckPoint(m_Pos.x + GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;
	if(Collision()->CheckPoint(m_Pos.x - GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;

	int MoveRestrictionsBelow = Collision()->GetMoveRestrictions(m_Pos + vec2(0, GetProximityRadius() / 2 + 4), 0.0f);
	return (MoveRestrictionsBelow & CANTMOVE_DOWN) != 0;
}

void CCharacter::HandleJetpack()
{
	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	bool FullAuto = false;
	if(m_Core.m_ActiveWeapon == WEAPON_GRENADE || m_Core.m_ActiveWeapon == WEAPON_SHOTGUN || m_Core.m_ActiveWeapon == WEAPON_LASER)
		FullAuto = true;
	if(m_Core.m_Jetpack && m_Core.m_ActiveWeapon == WEAPON_GUN)
		FullAuto = true;

	// check if we're going to fire
	bool WillFire = false;
	if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;

	if(FullAuto && (m_LatestInput.m_Fire & 1) && m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
		WillFire = true;

	if(!WillFire)
		return;

	// check for ammo
	if(!m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo || m_FreezeTime)
	{
		return;
	}
}

void CCharacter::HandleNinja()
{
	if(m_Core.m_ActiveWeapon != WEAPON_NINJA)
		return;

	if((Server()->Tick() - m_Core.m_Ninja.m_ActivationTick) > (g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000))
	{
		// time's up, return
		RemoveNinja();
		return;
	}

	int NinjaTime = m_Core.m_Ninja.m_ActivationTick + (g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000) - Server()->Tick();

	if(NinjaTime % Server()->TickSpeed() == 0 && NinjaTime / Server()->TickSpeed() <= 5)
	{
		GameServer()->CreateDamageInd(m_Pos, 0, NinjaTime / Server()->TickSpeed(), TeamMask() & GameServer()->ClientsMaskExcludeClientVersionAndHigher(VERSION_DDNET_NEW_HUD));
	}

	m_Armor = clamp(10 - (NinjaTime / 15), 0, 10);

	// force ninja Weapon
	SetWeapon(WEAPON_NINJA);

	m_Core.m_Ninja.m_CurrentMoveTime--;

	if(m_Core.m_Ninja.m_CurrentMoveTime == 0)
	{
		// reset velocity
		m_Core.m_Vel = m_Core.m_Ninja.m_ActivationDir * m_Core.m_Ninja.m_OldVelAmount;
	}

	if(m_Core.m_Ninja.m_CurrentMoveTime > 0)
	{
		// Set velocity
		m_Core.m_Vel = m_Core.m_Ninja.m_ActivationDir * g_pData->m_Weapons.m_Ninja.m_Velocity;
		vec2 OldPos = m_Pos;
		Collision()->MoveBox(&m_Core.m_Pos, &m_Core.m_Vel, vec2(GetProximityRadius(), GetProximityRadius()), 0.f);

		// reset velocity so the client doesn't predict stuff
		m_Core.m_Vel = vec2(0.f, 0.f);

		// check if we Hit anything along the way
		{
			CEntity *apEnts[MAX_CLIENTS];
			float Radius = GetProximityRadius() * 2.0f;
			int Num = GameServer()->m_World.FindEntities(OldPos, Radius, apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

			// check that we're not in solo part
			if(Teams()->m_Core.GetSolo(m_pPlayer->GetCID()))
				return;

			for(int i = 0; i < Num; ++i)
			{
				auto *pChr = static_cast<CCharacter *>(apEnts[i]);
				if(pChr == this)
					continue;

				// Don't hit players in other teams
				if(Team() != pChr->Team())
					continue;

				// Don't hit players in solo parts
				if(Teams()->m_Core.GetSolo(pChr->m_pPlayer->GetCID()))
					return;

				// make sure we haven't Hit this object before
				bool bAlreadyHit = false;
				for(int j = 0; j < m_NumObjectsHit; j++)
				{
					if(m_apHitObjects[j] == pChr)
						bAlreadyHit = true;
				}
				if(bAlreadyHit)
					continue;

				// check so we are sufficiently close
				if(distance(pChr->m_Pos, m_Pos) > (GetProximityRadius() * 2.0f))
					continue;

				// Hit a player, give them damage and stuffs...
				GameServer()->CreateSound(pChr->m_Pos, SOUND_NINJA_HIT, TeamMask());
				// set his velocity to fast upward (for now)
				if(m_NumObjectsHit < 10)
					m_apHitObjects[m_NumObjectsHit++] = pChr;

				pChr->TakeDamage(vec2(0, -10.0f), g_pData->m_Weapons.m_Ninja.m_pBase->m_Damage, m_pPlayer->GetCID(), WEAPON_NINJA);
			}
		}

		return;
	}
}

void CCharacter::DoWeaponSwitch()
{
	// make sure we can switch
	if(m_ReloadTimer != 0 || m_QueuedWeapon == -1 || m_Core.m_aWeapons[WEAPON_NINJA].m_Got || !m_Core.m_aWeapons[m_QueuedWeapon].m_Got)
		return;

	// switch Weapon
	SetWeapon(m_QueuedWeapon);
}

void CCharacter::HandleWeaponSwitch()
{
	int WantedWeapon = m_Core.m_ActiveWeapon;
	if(m_QueuedWeapon != -1)
		WantedWeapon = m_QueuedWeapon;

	bool Anything = false;
	for(int i = 0; i < NUM_WEAPONS - 1; ++i)
		if(m_Core.m_aWeapons[i].m_Got)
			Anything = true;
	if(!Anything)
		return;
	// select Weapon
	int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
	int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;

	if(Next < 128) // make sure we only try sane stuff
	{
		while(Next) // Next Weapon selection
		{
			WantedWeapon = (WantedWeapon + 1) % NUM_WEAPONS;
			if(m_Core.m_aWeapons[WantedWeapon].m_Got)
				Next--;
		}
	}

	if(Prev < 128) // make sure we only try sane stuff
	{
		while(Prev) // Prev Weapon selection
		{
			WantedWeapon = (WantedWeapon - 1) < 0 ? NUM_WEAPONS - 1 : WantedWeapon - 1;
			if(m_Core.m_aWeapons[WantedWeapon].m_Got)
				Prev--;
		}
	}

	// Direct Weapon selection
	if(m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon - 1;

	// check for insane values
	if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != m_Core.m_ActiveWeapon && m_Core.m_aWeapons[WantedWeapon].m_Got)
		m_QueuedWeapon = WantedWeapon;

	DoWeaponSwitch();
}

void CCharacter::FireWeapon()
{
	if (m_ReloadTimer)
		return;

	DoWeaponSwitch();
	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	bool FullAuto = false;
	if(m_Core.m_ActiveWeapon == WEAPON_GRENADE || m_Core.m_ActiveWeapon == WEAPON_SHOTGUN || m_Core.m_ActiveWeapon == WEAPON_LASER)
		FullAuto = true;
	if(m_Core.m_Jetpack && m_Core.m_ActiveWeapon == WEAPON_GUN)
		FullAuto = true;
	if(m_Core.m_ActiveWeapon == WEAPON_HAMMER && m_pPlayer->m_AccInv.HaveItem(ITEM_AUTO_HAMMER))
		FullAuto = true;
	if(m_Core.m_ActiveWeapon == WEAPON_GUN && m_pPlayer->m_AccInv.HaveItem(ITEM_AUTO_GUN))
		FullAuto = true;
	// allow firing directly after coming out of freeze or being unfrozen
	// by something
	if(m_FrozenLastTick)
		FullAuto = true;

	// don't fire hammer when player is deep and sv_deepfly is disabled
	if(!g_Config.m_SvDeepfly && m_Core.m_ActiveWeapon == WEAPON_HAMMER && m_Core.m_DeepFrozen)
		return;

	// check if we're going to fire
	bool WillFire = false;
	if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;

	if(FullAuto && (m_LatestInput.m_Fire & 1) && m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
		WillFire = true;

	if(!WillFire)
		return;

	if(m_FreezeTime)
	{
		// Timer stuff to avoid shrieking orchestra caused by unfreeze-plasma
		if(m_PainSoundTimer <= 0 && !(m_LatestPrevInput.m_Fire & 1))
		{
			m_PainSoundTimer = 1 * Server()->TickSpeed();
			GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG, TeamMask());
		}
		return;
	}

	// check for ammo
	if(!m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
	{
		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO);

		float FireDelay;
		GameServer()->Tuning()->Get(38 + m_Core.m_ActiveWeapon, &FireDelay);

		float Mul = 30;
		switch(m_Core.m_ActiveWeapon)
		{
		case WEAPON_GRENADE: Mul = 17; break;
		case WEAPON_HAMMER: Mul = 2; break;
		case WEAPON_GUN: Mul = 1; break;
		case WEAPON_SHOTGUN: Mul = 10; break;
		}

		FireDelay -= m_pPlayer->m_AccUp.m_FireSpeed * Mul;
		FireDelay = fmax(FireDelay, 0);
		m_ReloadTimer = FireDelay * Server()->TickSpeed() / 1000;

		return;
	}

	vec2 ProjStartPos = m_Pos + Direction * GetProximityRadius() * 0.75f;

	// Handle weapon fire
	int Weapon = m_Core.m_ActiveWeapon;
	if (Weapon == WEAPON_GUN ||
		Weapon == WEAPON_SHOTGUN ||
		Weapon == WEAPON_GRENADE)
	{
		// Get lifetime
		int LifeTime;
		if (Weapon == WEAPON_GUN)
			LifeTime = Server()->TickSpeed() * GameServer()->Tuning()->m_GunLifetime;
		else if (Weapon == WEAPON_SHOTGUN)
			LifeTime = Server()->TickSpeed() * GameServer()->Tuning()->m_ShotgunLifetime;
		else
			LifeTime = Server()->TickSpeed() * GameServer()->Tuning()->m_GrenadeLifetime;

		const float Fov = 60.f;
		int Spray = m_pPlayer->m_AccUp.m_Spray + 1 + ((Weapon == WEAPON_SHOTGUN) ? 5 : 0);

		// Spread
		const float fovRad = (Fov / 2.f) * pi / 180.f;
		const float aps = fovRad / Spray * 2;
		const float a = angle(Direction);

		for (int i = 1; i <= Spray; i++) {
			float m = (i % 2 == 0) ? -1 : 1;
			float a1 = a + aps * (i / 2) * m;
			vec2 dir = direction(a1);

			new CProjectile(
				GameWorld(),
				Weapon, // Type
				m_pPlayer->GetCID(), // Owner
				ProjStartPos, // Pos
				dir, // Dir
				LifeTime, // Span
				false, // Freeze
				(Weapon == WEAPON_GRENADE), // Explosive
				-1 // SoundImpact
			);

			if (Weapon == WEAPON_SHOTGUN && m_pPlayer->m_AccInv.HaveItem(ITEM_SGUN))
			{
				new CProjectile(
					GameWorld(),
					WEAPON_GUN, // Type
					m_pPlayer->GetCID(), // Owner
					ProjStartPos, // Pos
					dir, // Dir
					LifeTime, // Span
					false, // Freeze
					false, // Explosive
					-1 // SoundImpact
				);
			}
		}

		// Make a sound :O
		if (Weapon == WEAPON_GUN)
			GameServer()->CreateSound(m_Pos, SOUND_GUN_FIRE);
		else if (Weapon == WEAPON_SHOTGUN)
			GameServer()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE);
		else if (Weapon == WEAPON_GRENADE)
			GameServer()->CreateSound(m_Pos, SOUND_GRENADE_FIRE);
	}

	switch(Weapon)
	{
	case WEAPON_HAMMER:
	{
		// Check farming
		CPickupJob *pPickup = (CPickupJob *)GameWorld()->ClosestEntity(m_Pos, 30.f, CGameWorld::ENTTYPE_PICKUP_JOB);
		if (pPickup && pPickup->m_State != 0)
		{
			pPickup->Damage(m_pPlayer->GetCID());
			m_ReloadTimer = Server()->TickSpeed();
			return;
		}

		// Check pickup items
		CPickupPhys *pPickupPhys = (CPickupPhys *)GameWorld()->ClosestEntity(m_Pos, 30.f, CGameWorld::ENTTYPE_PICKUP_PHYS);
		if (pPickupPhys && pPickupPhys->m_Type == PICKUP_PHYS_TYPE_ITEM)
		{
			MMOCore()->GiveItem(GetPlayer()->GetCID(), pPickupPhys->m_ItemID, pPickupPhys->m_Count);
			pPickupPhys->Destroy();

			m_ReloadTimer = Server()->TickSpeed();
			return;
		}

		// reset objects Hit
		m_NumObjectsHit = 0;
		GameServer()->CreateSound(m_Pos, SOUND_HAMMER_FIRE, TeamMask());

		if(m_Core.m_HammerHitDisabled)
			break;

		CEntity *apEnts[MAX_CLIENTS];
		int Hits = 0;
		int Num = GameServer()->m_World.FindEntities(ProjStartPos, GetProximityRadius() * 0.5f, apEnts,
			MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

		for(int i = 0; i < Num; ++i)
		{
			auto *pTarget = static_cast<CCharacter *>(apEnts[i]);

			//if ((pTarget == this) || Collision()->IntersectLine(ProjStartPos, pTarget->m_Pos, NULL, NULL))
			if((pTarget == this || (pTarget->IsAlive() && !CanCollide(pTarget->GetPlayer()->GetCID()))))
				continue;

			// set his velocity to fast upward (for now)
			if(length(pTarget->m_Pos - ProjStartPos) > 0.0f)
				GameServer()->CreateHammerHit(pTarget->m_Pos - normalize(pTarget->m_Pos - ProjStartPos) * GetProximityRadius() * 0.5f, TeamMask());
			else
				GameServer()->CreateHammerHit(ProjStartPos, TeamMask());

			vec2 Dir;
			if(length(pTarget->m_Pos - m_Pos) > 0.0f)
				Dir = normalize(pTarget->m_Pos - m_Pos);
			else
				Dir = vec2(0.f, -1.f);

			float Strength;
			if(!m_TuneZone)
				Strength = GameServer()->Tuning()->m_HammerStrength;
			else
				Strength = GameServer()->TuningList()[m_TuneZone].m_HammerStrength;

			vec2 Temp = pTarget->m_Core.m_Vel + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f;
			Temp = ClampVel(pTarget->m_MoveRestrictions, Temp);
			Temp -= pTarget->m_Core.m_Vel;
			pTarget->TakeDamage((vec2(0.f, -1.0f) + Temp) * Strength, 3,
				m_pPlayer->GetCID(), m_Core.m_ActiveWeapon);

			if(m_FreezeHammer)
				pTarget->Freeze();

			Hits++;
		}

		Num = GameServer()->m_World.FindEntities(ProjStartPos, GetProximityRadius(), apEnts,
			MAX_CLIENTS, CGameWorld::ENTTYPE_DUMMY);

		for(int i = 0; i < Num; ++i)
		{
			auto *pTarget = static_cast<CDummyBase *>(apEnts[i]);

			if(!pTarget->IsAlive())
				continue;

			// set his velocity to fast upward (for now)
			if(length(pTarget->m_Pos - ProjStartPos) > 0.0f)
				GameServer()->CreateHammerHit(pTarget->m_Pos - normalize(pTarget->m_Pos - ProjStartPos) * GetProximityRadius() * 0.5f, TeamMask());
			else
				GameServer()->CreateHammerHit(ProjStartPos, TeamMask());

			vec2 Dir;
			if(length(pTarget->m_Pos - m_Pos) > 0.0f)
				Dir = normalize(pTarget->m_Pos - m_Pos);
			else
				Dir = vec2(0.f, -1.f);

			float Strength;
			if(!m_TuneZone)
				Strength = GameServer()->Tuning()->m_HammerStrength;
			else
				Strength = GameServer()->TuningList()[m_TuneZone].m_HammerStrength;

			vec2 Temp = pTarget->Core()->m_Vel + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f;
			//Temp = ClampVel(pTarget->m_MoveRestrictions, Temp);
			Temp -= pTarget->Core()->m_Vel;
			pTarget->TakeDamage((vec2(0.f, -1.0f) + Temp) * Strength, 3,
				m_pPlayer->GetCID(), m_Core.m_ActiveWeapon);

			Hits++;
		}

		// if we Hit anything, we have to wait for to reload
		if(Hits)
		{
			float FireDelay;
			if(!m_TuneZone)
				FireDelay = GameServer()->Tuning()->m_HammerHitFireDelay;
			else
				FireDelay = GameServer()->TuningList()[m_TuneZone].m_HammerHitFireDelay;
			m_ReloadTimer = FireDelay * Server()->TickSpeed() / 1000;
		}
	}
	break;

	case WEAPON_LASER:
	{
		float LaserReach;
		if(!m_TuneZone)
			LaserReach = GameServer()->Tuning()->m_LaserReach;
		else
			LaserReach = GameServer()->TuningList()[m_TuneZone].m_LaserReach;

		new CLaser(GameWorld(), m_Pos, Direction, LaserReach, m_pPlayer->GetCID(), WEAPON_LASER);
		GameServer()->CreateSound(m_Pos, SOUND_LASER_FIRE, TeamMask());
	}
	break;

	case WEAPON_NINJA:
	{
		// reset Hit objects
		m_NumObjectsHit = 0;

		m_Core.m_Ninja.m_ActivationDir = Direction;
		m_Core.m_Ninja.m_CurrentMoveTime = g_pData->m_Weapons.m_Ninja.m_Movetime * Server()->TickSpeed() / 1000;
		m_Core.m_Ninja.m_OldVelAmount = length(m_Core.m_Vel);

		GameServer()->CreateSound(m_Pos, SOUND_NINJA_FIRE, TeamMask());
	}
	break;
	}

	m_AttackTick = Server()->Tick();

	if (m_Core.m_ActiveWeapon != WEAPON_HAMMER && m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo > 0)
		m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo--;
	m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = Server()->Tick() + Server()->TickSpeed();

	if(!m_ReloadTimer)
	{
		float Mul = 10;
		switch(m_Core.m_ActiveWeapon)
		{
		case WEAPON_GRENADE: Mul = 4; break;
		}

		float FireDelay = 1000 + m_pPlayer->m_AccUp.m_FireSpeed * Mul;
		m_ReloadTimer = g_pData->m_Weapons.m_aId[Weapon].m_Firedelay * Server()->TickSpeed() / FireDelay;
	}
}

void CCharacter::HandleWeapons()
{
	//ninja
	HandleNinja();
	HandleJetpack();

	if(m_PainSoundTimer > 0)
		m_PainSoundTimer--;

	// Check for ammo regen
	int Weapon = m_Core.m_ActiveWeapon;
	int AmmoRegen = m_pPlayer->m_AccUp.m_AmmoRegen;
	float RegenTime = fmax(0.1f, 2 - AmmoRegen / 50.f);
	int Ammo = m_Core.m_aWeapons[Weapon].m_Ammo;
	int MaxAmmo = m_Core.m_aWeapons[Weapon].m_MaxAmmo;
	int Tick = Server()->Tick();
	if(AmmoRegen && Weapon != WEAPON_HAMMER && Tick > m_Core.m_aWeapons[Weapon].m_AmmoRegenStart && Tick % (int)(Server()->TickSpeed() * RegenTime) == 0 && Ammo < MaxAmmo)
		m_Core.m_aWeapons[Weapon].m_Ammo++;

	// check reload timer
	if(m_ReloadTimer)
	{
		m_ReloadTimer--;
		return;
	}

	// fire Weapon, if wanted
	FireWeapon();
}

void CCharacter::GiveNinja()
{
	m_Core.m_Ninja.m_ActivationTick = Server()->Tick();
	m_Core.m_aWeapons[WEAPON_NINJA].m_Got = true;
	m_Core.m_aWeapons[WEAPON_NINJA].m_Ammo = -1;
	if(m_Core.m_ActiveWeapon != WEAPON_NINJA)
		m_LastWeapon = m_Core.m_ActiveWeapon;
	m_Core.m_ActiveWeapon = WEAPON_NINJA;

	if(!m_Core.m_aWeapons[WEAPON_NINJA].m_Got)
		GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA, TeamMask());
}

void CCharacter::RemoveNinja()
{
	m_Core.m_Ninja.m_CurrentMoveTime = 0;
	m_Core.m_aWeapons[WEAPON_NINJA].m_Got = false;
	m_Core.m_ActiveWeapon = m_LastWeapon;

	SetWeapon(m_Core.m_ActiveWeapon);
}

void CCharacter::SetEmote(int Emote, int Tick)
{
	m_EmoteType = Emote;
	m_EmoteStop = Tick;
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	// check for changes
	if(mem_comp(&m_SavedInput, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
		m_LastAction = Server()->Tick();

	// copy new input
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));

	// it is not allowed to aim in the center
	if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;

	mem_copy(&m_SavedInput, &m_Input, sizeof(m_SavedInput));
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));
	m_NumInputs++;

	// it is not allowed to aim in the center
	if(m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
		m_LatestInput.m_TargetY = -1;

	if(m_NumInputs > 1 && m_pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevPrevInput, &m_LatestPrevInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ResetHook()
{
	m_Core.SetHookedPlayer(-1);
	m_Core.m_pHookedDummy = 0x0;
	m_Core.m_HookState = HOOK_RETRACTED;
	m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
	m_Core.m_HookPos = m_Core.m_Pos;
}

void CCharacter::ResetInput()
{
	m_Input.m_Direction = 0;
	// simulate releasing the fire button
	if((m_Input.m_Fire & 1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_Input;
}

void CCharacter::PreTick()
{
	if(m_StartTime > Server()->Tick())
	{
		// Prevent the player from getting a negative time
		// The main reason why this can happen is because of time penalty tiles
		// However, other reasons are hereby also excluded
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), "You died of old age");
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
	}

	if(m_Paused)
		return;

	// set emote
	if(m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = m_pPlayer->GetDefaultEmote();
		m_EmoteStop = -1;
	}

	DDRaceTick();

	m_Core.m_Input = m_Input;
	m_Core.Tick(true, !g_Config.m_SvNoWeakHook);
}

void CCharacter::Tick()
{
	if(g_Config.m_SvNoWeakHook)
	{
		if(m_Paused)
			return;

		m_Core.TickDeferred();
	}
	else
	{
		PreTick();
	}

	// handle Weapons
	HandleWeapons();

	DDRacePostCoreTick();

	if (Server()->Tick() % 10 == 0)
	{
		CPickupPhys *pPickupPhys = (CPickupPhys *)GameWorld()->ClosestEntity(m_Pos, 30.f, CGameWorld::ENTTYPE_PICKUP_PHYS);
		if (pPickupPhys && pPickupPhys->m_Type == PICKUP_PHYS_TYPE_ITEM)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "Item: %s x%d", MMOCore()->GetItemName(pPickupPhys->m_ItemID), pPickupPhys->m_Count);
			GameServer()->SendMMOBroadcast(m_pPlayer->GetCID(), 0.5f, aBuf);
		}
	}

	// Prev input
	m_PrevInput = m_Input;

	m_PrevPos = m_Core.m_Pos;
}

void CCharacter::TickDeferred()
{
	// advance the dummy
	{
		CWorldCore TempWorld;
		m_ReckoningCore.Init(&TempWorld, Collision(), &Teams()->m_Core, m_pTeleOuts);
		m_ReckoningCore.m_Id = m_pPlayer->GetCID();
		m_ReckoningCore.Tick(false);
		m_ReckoningCore.Move();
		m_ReckoningCore.Quantize();
	}

	//lastsentcore
	vec2 StartPos = m_Core.m_Pos;
	vec2 StartVel = m_Core.m_Vel;
	bool StuckBefore = Collision()->TestBox(m_Core.m_Pos, CCharacterCore::PhysicalSizeVec2());

	m_Core.m_Id = m_pPlayer->GetCID();
	m_Core.Move();
	bool StuckAfterMove = Collision()->TestBox(m_Core.m_Pos, CCharacterCore::PhysicalSizeVec2());
	m_Core.Quantize();
	bool StuckAfterQuant = Collision()->TestBox(m_Core.m_Pos, CCharacterCore::PhysicalSizeVec2());
	m_Pos = m_Core.m_Pos;

	if(!StuckBefore && (StuckAfterMove || StuckAfterQuant))
	{
		// Hackish solution to get rid of strict-aliasing warning
		union
		{
			float f;
			unsigned u;
		} StartPosX, StartPosY, StartVelX, StartVelY;

		StartPosX.f = StartPos.x;
		StartPosY.f = StartPos.y;
		StartVelX.f = StartVel.x;
		StartVelY.f = StartVel.y;

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x",
			StuckBefore,
			StuckAfterMove,
			StuckAfterQuant,
			StartPos.x, StartPos.y,
			StartVel.x, StartVel.y,
			StartPosX.u, StartPosY.u,
			StartVelX.u, StartVelY.u);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	{
		int Events = m_Core.m_TriggeredEvents;
		int CID = m_pPlayer->GetCID();

		CClientMask TeamMask = Teams()->TeamMask(Team(), -1, CID);
		// Some sounds are triggered client-side for the acting player
		// so we need to avoid duplicating them
		CClientMask TeamMaskExceptSelf = Teams()->TeamMask(Team(), CID, CID);
		// Some are triggered client-side but only on Sixup
		CClientMask TeamMaskExceptSelfIfSixup = Server()->IsSixup(CID) ? TeamMaskExceptSelf : TeamMask;

		if(Events & COREEVENT_GROUND_JUMP)
			GameServer()->CreateSound(m_Pos, SOUND_PLAYER_JUMP, TeamMaskExceptSelf);

		if(Events & COREEVENT_HOOK_ATTACH_PLAYER)
			GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_PLAYER, TeamMaskExceptSelfIfSixup);

		if(Events & COREEVENT_HOOK_ATTACH_GROUND)
			GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_GROUND, TeamMaskExceptSelf);

		if(Events & COREEVENT_HOOK_HIT_NOHOOK)
			GameServer()->CreateSound(m_Pos, SOUND_HOOK_NOATTACH, TeamMaskExceptSelf);
	}

	if(m_pPlayer->GetTeam() == TEAM_SPECTATORS)
	{
		m_Pos.x = m_Input.m_TargetX;
		m_Pos.y = m_Input.m_TargetY;
	}

	// update the m_SendCore if needed
	{
		CNetObj_Character Predicted;
		CNetObj_Character Current;
		mem_zero(&Predicted, sizeof(Predicted));
		mem_zero(&Current, sizeof(Current));
		m_ReckoningCore.Write(&Predicted);
		m_Core.Write(&Current);

		// only allow dead reackoning for a top of 3 seconds
		if(m_Core.m_Reset || m_ReckoningTick + Server()->TickSpeed() * 3 < Server()->Tick() || mem_comp(&Predicted, &Current, sizeof(CNetObj_Character)) != 0)
		{
			m_ReckoningTick = Server()->Tick();
			m_SendCore = m_Core;
			m_ReckoningCore = m_Core;
			m_Core.m_Reset = false;
		}
	}
}

void CCharacter::TickPaused()
{
	++m_AttackTick;
	++m_DamageTakenTick;
	++m_Core.m_Ninja.m_ActivationTick;
	++m_ReckoningTick;
	if(m_LastAction != -1)
		++m_LastAction;
	if(m_EmoteStop > -1)
		++m_EmoteStop;
}

bool CCharacter::IncreaseHealth(int Amount)
{
	m_Health = clamp(m_Health + Amount, 0, m_MaxHealth);

	return true;
}

bool CCharacter::IncreaseArmor(int Amount)
{
	m_Armor = clamp(m_Armor + Amount, 0, m_MaxArmor);

	return true;
}

void CCharacter::Die(int Killer, int Weapon)
{
	if(Server()->IsRecording(m_pPlayer->GetCID()))
		Server()->StopRecord(m_pPlayer->GetCID());

	int ModeSpecial = GameServer()->m_pController->OnCharacterDeath(this, GameServer()->m_apPlayers[Killer], Weapon);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "kill killer='%d:%s' victim='%d:%s' weapon=%d special=%d",
		Killer, Server()->ClientName(Killer),
		m_pPlayer->GetCID(), Server()->ClientName(m_pPlayer->GetCID()), Weapon, ModeSpecial);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// send the kill message
	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Killer = Killer;
	Msg.m_Victim = m_pPlayer->GetCID();
	Msg.m_Weapon = Weapon;
	Msg.m_ModeSpecial = ModeSpecial;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

	// a nice sound
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE, TeamMask());

	// this is to rate limit respawning to 3 secs
	m_pPlayer->m_PreviousDieTick = m_pPlayer->m_DieTick;
	m_pPlayer->m_DieTick = Server()->Tick();

	m_Alive = false;
	SetSolo(false);

	GameServer()->m_World.RemoveEntity(this);
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), TeamMask());
	Teams()->OnCharacterDeath(GetPlayer()->GetCID(), Weapon);
}

bool CCharacter::TakeDamage(vec2 Force, int Dmg, int From, int Weapon)
{
	if (m_NoDamage)
		return false;

	CPlayer *pFromPly = 0x0;

	if (From >= 0)
	{
		pFromPly = GameServer()->m_apPlayers[From];

		if (m_pPlayer->m_AccData.m_Level < 20 || pFromPly->m_AccData.m_Level < 20)
			return false;

		Dmg += MMOCore()->GetPlusDamage(From);
	}

	if(Dmg)
	{
		// Emote
		m_EmoteType = EMOTE_PAIN;
		m_EmoteStop = Server()->Tick() + 500 * Server()->TickSpeed() / 1000;

		if(m_Armor)
		{
			if(Dmg <= m_Armor)
			{
				m_Armor -= Dmg;
				Dmg = 0;
			}
			else
			{
				Dmg -= m_Armor;
				m_Armor = 0;
			}
		}

		m_Health -= Dmg;

		// do damage Hit sound
		if(From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From])
		{
			GameServer()->CreateSound(GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_HIT, -1);

			//int Steal = (100 - Server()->GetItemCount(From, ACCESSORY_ADD_STEAL_HP) > 30) ? 100 - Server()->GetItemCount(From, ACCESSORY_ADD_STEAL_HP) > 30 : 30;
			//pFrom->m_Health += Steal;
		}
	}

	if (m_Health <= 0)
	{
		Die((From >= 0) ? From : m_pPlayer->GetCID(), Weapon);

		if (From >= 0)
		{
			CPlayer *pFrom = GameServer()->m_apPlayers[From];
			CCharacter *pFromChar = 0x0;
			if (pFrom && pFrom->m_LoggedIn)
			{
				pFromChar = pFrom->GetCharacter();

				// Add EXP for killing
				pFrom->AddEXP(m_pPlayer->m_AccData.m_Level * 2);
			}
		}
	}

	vec2 Temp = m_Core.m_Vel + Force;
	m_Core.m_Vel = ClampVel(m_MoveRestrictions, Temp);

	return true;
}

//TODO: Move the emote stuff to a function
void CCharacter::SnapCharacter(int SnappingClient, int ID)
{
	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	CCharacterCore *pCore;
	int Tick, Emote = m_EmoteType, Weapon = m_Core.m_ActiveWeapon, AmmoCount = m_Core.m_aWeapons[Weapon].m_Ammo, Health = 0, Armor = 0;
	if(!m_ReckoningTick || GameServer()->m_World.m_Paused)
	{
		Tick = 0;
		pCore = &m_Core;
	}
	else
	{
		Tick = m_ReckoningTick;
		pCore = &m_SendCore;
	}

	// change eyes and use ninja graphic if player is frozen
	if(m_Core.m_DeepFrozen || m_FreezeTime > 0 || m_Core.m_LiveFrozen)
	{
		if(Emote == EMOTE_NORMAL)
			Emote = (m_Core.m_DeepFrozen || m_Core.m_LiveFrozen) ? EMOTE_PAIN : EMOTE_BLINK;

		if((m_Core.m_DeepFrozen || m_FreezeTime > 0) && SnappingClientVersion < VERSION_DDNET_NEW_HUD)
			Weapon = WEAPON_NINJA;
	}

	// Change eyes if player in water
	if(m_InWater)
		Emote = EMOTE_BLINK;

	// This could probably happen when m_Jetpack changes instead
	// jetpack and ninjajetpack prediction
	if(m_pPlayer->GetCID() == SnappingClient)
	{
		if(m_Core.m_Jetpack && Weapon != WEAPON_NINJA)
		{
			if(!(m_NeededFaketuning & FAKETUNE_JETPACK))
			{
				m_NeededFaketuning |= FAKETUNE_JETPACK;
				GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);
			}
		}
		else
		{
			if(m_NeededFaketuning & FAKETUNE_JETPACK)
			{
				m_NeededFaketuning &= ~FAKETUNE_JETPACK;
				GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);
			}
		}
	}

	// change eyes, use ninja graphic and set ammo count if player has ninjajetpack
	if(m_pPlayer->m_NinjaJetpack && m_Core.m_Jetpack && m_Core.m_ActiveWeapon == WEAPON_GUN && !m_Core.m_DeepFrozen && m_FreezeTime == 0 && !m_Core.m_HasTelegunGun)
	{
		if(Emote == EMOTE_NORMAL)
			Emote = EMOTE_HAPPY;
		Weapon = WEAPON_NINJA;
		AmmoCount = 10;
	}

	if(m_pPlayer->GetCID() == SnappingClient || SnappingClient == SERVER_DEMO_CLIENT ||
		(!g_Config.m_SvStrictSpectateMode && m_pPlayer->GetCID() == GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID))
	{
		Health = m_Health;
		Armor = m_Armor;
		if(m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo > 0)
			AmmoCount = (m_FreezeTime > 0) ? m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo : 0;
	}

	if(GetPlayer()->IsAfk() || GetPlayer()->IsPaused())
	{
		if(m_FreezeTime > 0 || m_Core.m_DeepFrozen || m_Core.m_LiveFrozen)
			Emote = EMOTE_NORMAL;
		else
			Emote = EMOTE_BLINK;
	}

	if(Emote == EMOTE_NORMAL)
	{
		if(250 - ((Server()->Tick() - m_LastAction) % (250)) < 5)
			Emote = EMOTE_BLINK;
	}

	if(!Server()->IsSixup(SnappingClient))
	{
		CNetObj_Character *pCharacter = Server()->SnapNewItem<CNetObj_Character>(ID);
		if(!pCharacter)
			return;

		pCore->Write(pCharacter);

		pCharacter->m_Tick = Tick;
		pCharacter->m_Emote = Emote;

		if(pCharacter->m_HookedPlayer != -1)
		{
			if(!Server()->Translate(pCharacter->m_HookedPlayer, SnappingClient))
				pCharacter->m_HookedPlayer = -1;
		}

		pCharacter->m_AttackTick = m_AttackTick;
		pCharacter->m_Direction = m_Input.m_Direction;
		pCharacter->m_Weapon = Weapon;
		pCharacter->m_AmmoCount = AmmoCount;
		pCharacter->m_Health = Health;
		pCharacter->m_Armor = Armor;
		pCharacter->m_PlayerFlags = GetPlayer()->m_PlayerFlags;
	}
	else
	{
		protocol7::CNetObj_Character *pCharacter = Server()->SnapNewItem<protocol7::CNetObj_Character>(ID);
		if(!pCharacter)
			return;

		pCore->Write(reinterpret_cast<CNetObj_CharacterCore *>(static_cast<protocol7::CNetObj_CharacterCore *>(pCharacter)));
		if(pCharacter->m_Angle > (int)(pi * 256.0f))
		{
			pCharacter->m_Angle -= (int)(2.0f * pi * 256.0f);
		}

		pCharacter->m_Tick = Tick;
		pCharacter->m_Emote = Emote;
		pCharacter->m_AttackTick = m_AttackTick;
		pCharacter->m_Direction = m_Input.m_Direction;
		pCharacter->m_Weapon = Weapon;
		pCharacter->m_AmmoCount = AmmoCount;

		if(m_FreezeTime > 0 || m_Core.m_DeepFrozen)
			pCharacter->m_AmmoCount = m_Core.m_FreezeStart + g_Config.m_SvFreezeDelay * Server()->TickSpeed();
		else if(Weapon == WEAPON_NINJA)
			pCharacter->m_AmmoCount = m_Core.m_Ninja.m_ActivationTick + g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000;

		pCharacter->m_Health = Health;
		pCharacter->m_Armor = Armor;
		pCharacter->m_TriggeredEvents = 0;
	}
}

bool CCharacter::CanSnapCharacter(int SnappingClient)
{
	if(SnappingClient == SERVER_DEMO_CLIENT)
		return true;

	CCharacter *pSnapChar = GameServer()->GetPlayerChar(SnappingClient);
	CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];

	if(pSnapPlayer->GetTeam() == TEAM_SPECTATORS || pSnapPlayer->IsPaused())
	{
		if(pSnapPlayer->m_SpectatorID != SPEC_FREEVIEW && !CanCollide(pSnapPlayer->m_SpectatorID) && (pSnapPlayer->m_ShowOthers == SHOW_OTHERS_OFF || (pSnapPlayer->m_ShowOthers == SHOW_OTHERS_ONLY_TEAM && !SameTeam(pSnapPlayer->m_SpectatorID))))
			return false;
		else if(pSnapPlayer->m_SpectatorID == SPEC_FREEVIEW && !CanCollide(SnappingClient) && pSnapPlayer->m_SpecTeam && !SameTeam(SnappingClient))
			return false;
	}
	else if(pSnapChar && !pSnapChar->m_Core.m_Super && !CanCollide(SnappingClient) && (pSnapPlayer->m_ShowOthers == SHOW_OTHERS_OFF || (pSnapPlayer->m_ShowOthers == SHOW_OTHERS_ONLY_TEAM && !SameTeam(SnappingClient))))
		return false;

	return true;
}

void CCharacter::Snap(int SnappingClient)
{
	int ID = m_pPlayer->GetCID();

	if(!Server()->Translate(ID, SnappingClient))
		return;

	if(!CanSnapCharacter(SnappingClient))
	{
		return;
	}

	// A player may not be clipped away if his hook or a hook attached to him is in the field of view
	bool PlayerAndHookNotInView = NetworkClippedLine(SnappingClient, m_Pos, m_Core.m_HookPos);
	bool AttachedHookInView = false;
	if(PlayerAndHookNotInView)
	{
		for(const auto &AttachedPlayerID : m_Core.m_AttachedPlayers)
		{
			CCharacter *pOtherPlayer = GameServer()->GetPlayerChar(AttachedPlayerID);
			if(pOtherPlayer && pOtherPlayer->m_Core.m_HookedPlayer == m_pPlayer->GetCID())
			{
				if(!NetworkClippedLine(SnappingClient, m_Pos, pOtherPlayer->m_Pos))
				{
					AttachedHookInView = true;
					break;
				}
			}
		}
	}
	if(PlayerAndHookNotInView && !AttachedHookInView)
	{
		return;
	}

	SnapCharacter(SnappingClient, ID);

	CNetObj_DDNetCharacter *pDDNetCharacter = Server()->SnapNewItem<CNetObj_DDNetCharacter>(ID);
	if(!pDDNetCharacter)
		return;

	pDDNetCharacter->m_Flags = 0;
	if(m_Core.m_Solo)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_SOLO;
	if(m_Core.m_Super)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_SUPER;
	if(m_Core.m_EndlessHook)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_ENDLESS_HOOK;
	if(m_Core.m_CollisionDisabled || !GameServer()->Tuning()->m_PlayerCollision)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_COLLISION_DISABLED;
	if(m_Core.m_HookHitDisabled || !GameServer()->Tuning()->m_PlayerHooking)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_HOOK_HIT_DISABLED;
	if(m_Core.m_EndlessJump)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_ENDLESS_JUMP;
	if(m_Core.m_Jetpack)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_JETPACK;
	if(m_Core.m_HammerHitDisabled)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_HAMMER_HIT_DISABLED;
	if(m_Core.m_ShotgunHitDisabled)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_SHOTGUN_HIT_DISABLED;
	if(m_Core.m_GrenadeHitDisabled)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_GRENADE_HIT_DISABLED;
	if(m_Core.m_LaserHitDisabled)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_LASER_HIT_DISABLED;
	if(m_Core.m_HasTelegunGun)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_TELEGUN_GUN;
	if(m_Core.m_HasTelegunGrenade)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_TELEGUN_GRENADE;
	if(m_Core.m_HasTelegunLaser)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_TELEGUN_LASER;
	if(m_Core.m_aWeapons[WEAPON_HAMMER].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_HAMMER;
	if(m_Core.m_aWeapons[WEAPON_GUN].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_GUN;
	if(m_Core.m_aWeapons[WEAPON_SHOTGUN].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_SHOTGUN;
	if(m_Core.m_aWeapons[WEAPON_GRENADE].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_GRENADE;
	if(m_Core.m_aWeapons[WEAPON_LASER].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_LASER;
	if(m_Core.m_ActiveWeapon == WEAPON_NINJA)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_NINJA;
	if(m_Core.m_LiveFrozen)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_MOVEMENTS_DISABLED;

	pDDNetCharacter->m_FreezeEnd = m_Core.m_DeepFrozen ? -1 : m_FreezeTime == 0 ? 0 : Server()->Tick() + m_FreezeTime;
	pDDNetCharacter->m_Jumps = m_Core.m_Jumps;
	pDDNetCharacter->m_TeleCheckpoint = m_TeleCheckpoint;
	pDDNetCharacter->m_StrongWeakID = m_StrongWeakID;

	// Display Information
	pDDNetCharacter->m_JumpedTotal = m_Core.m_JumpedTotal;
	pDDNetCharacter->m_NinjaActivationTick = m_Core.m_Ninja.m_ActivationTick;
	pDDNetCharacter->m_FreezeStart = m_Core.m_FreezeStart;
	if(m_Core.m_IsInFreeze)
	{
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_IN_FREEZE;
	}
	if(Teams()->IsPractice(Team()))
	{
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_PRACTICE_MODE;
	}
	pDDNetCharacter->m_TargetX = m_Core.m_Input.m_TargetX;
	pDDNetCharacter->m_TargetY = m_Core.m_Input.m_TargetY;
}

// DDRace

bool CCharacter::CanCollide(int ClientID)
{
	return Teams()->m_Core.CanCollide(GetPlayer()->GetCID(), ClientID);
}
bool CCharacter::SameTeam(int ClientID)
{
	return Teams()->m_Core.SameTeam(GetPlayer()->GetCID(), ClientID);
}

int CCharacter::Team()
{
	return Teams()->m_Core.Team(m_pPlayer->GetCID());
}

void CCharacter::SetTeleports(std::map<int, std::vector<vec2>> *pTeleOuts, std::map<int, std::vector<vec2>> *pTeleCheckOuts)
{
	m_pTeleOuts = pTeleOuts;
	m_pTeleCheckOuts = pTeleCheckOuts;
	m_Core.m_pTeleOuts = pTeleOuts;
}

void CCharacter::HandleSkippableTiles(int Index)
{
	// handle death-tiles and leaving gamelayer
	if((Collision()->GetCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
		   Collision()->GetCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH ||
		   Collision()->GetCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
		   Collision()->GetCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH ||
		   Collision()->GetFCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
		   Collision()->GetFCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH ||
		   Collision()->GetFCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
		   Collision()->GetFCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH) &&
		!m_Core.m_Super && !(Team() && Teams()->TeeFinished(m_pPlayer->GetCID())))
	{
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
		return;
	}

	if(GameLayerClipped(m_Pos))
	{
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
		return;
	}

	if(Index < 0)
		return;

	// handle speedup tiles
	if(Collision()->IsSpeedup(Index))
	{
		vec2 Direction, TempVel = m_Core.m_Vel;
		int Force, MaxSpeed = 0;
		float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
		Collision()->GetSpeedup(Index, &Direction, &Force, &MaxSpeed);
		if(Force == 255 && MaxSpeed)
		{
			m_Core.m_Vel = Direction * (MaxSpeed / 5);
		}
		else
		{
			if(MaxSpeed > 0 && MaxSpeed < 5)
				MaxSpeed = 5;
			if(MaxSpeed > 0)
			{
				if(Direction.x > 0.0000001f)
					SpeederAngle = -atan(Direction.y / Direction.x);
				else if(Direction.x < 0.0000001f)
					SpeederAngle = atan(Direction.y / Direction.x) + 2.0f * asin(1.0f);
				else if(Direction.y > 0.0000001f)
					SpeederAngle = asin(1.0f);
				else
					SpeederAngle = asin(-1.0f);

				if(SpeederAngle < 0)
					SpeederAngle = 4.0f * asin(1.0f) + SpeederAngle;

				if(TempVel.x > 0.0000001f)
					TeeAngle = -atan(TempVel.y / TempVel.x);
				else if(TempVel.x < 0.0000001f)
					TeeAngle = atan(TempVel.y / TempVel.x) + 2.0f * asin(1.0f);
				else if(TempVel.y > 0.0000001f)
					TeeAngle = asin(1.0f);
				else
					TeeAngle = asin(-1.0f);

				if(TeeAngle < 0)
					TeeAngle = 4.0f * asin(1.0f) + TeeAngle;

				TeeSpeed = sqrt(pow(TempVel.x, 2) + pow(TempVel.y, 2));

				DiffAngle = SpeederAngle - TeeAngle;
				SpeedLeft = MaxSpeed / 5.0f - cos(DiffAngle) * TeeSpeed;
				if(abs((int)SpeedLeft) > Force && SpeedLeft > 0.0000001f)
					TempVel += Direction * Force;
				else if(abs((int)SpeedLeft) > Force)
					TempVel += Direction * -Force;
				else
					TempVel += Direction * SpeedLeft;
			}
			else
				TempVel += Direction * Force;

			m_Core.m_Vel = ClampVel(m_MoveRestrictions, TempVel);
		}
	}
}

bool CCharacter::IsSwitchActiveCb(int Number, void *pUser)
{
	CCharacter *pThis = (CCharacter *)pUser;
	auto &aSwitchers = pThis->Switchers();
	return !aSwitchers.empty() && pThis->Team() != TEAM_SUPER && aSwitchers[Number].m_aStatus[pThis->Team()];
}

void CCharacter::HandleTiles(int Index)
{
	int MapIndex = Index;
	//int PureMapIndex = Collision()->GetPureMapIndex(m_Pos);
	m_TileIndex = Collision()->GetTileIndex(MapIndex);
	m_TileFIndex = Collision()->GetFTileIndex(MapIndex);
	m_MoveRestrictions = Collision()->GetMoveRestrictions(IsSwitchActiveCb, this, m_Pos, 18.0f, MapIndex);
	if(Index < 0)
	{
		m_LastRefillJumps = false;
		m_LastPenalty = false;
		m_LastBonus = false;
		return;
	}
	int TeleCheckpoint = Collision()->IsTeleCheckpoint(MapIndex);
	if(TeleCheckpoint)
		m_TeleCheckpoint = TeleCheckpoint;

	GameServer()->m_pController->HandleCharacterTiles(this, Index);

	// freeze
	if(((m_TileIndex == TILE_FREEZE) || (m_TileFIndex == TILE_FREEZE)) && !m_Core.m_Super && !m_Core.m_DeepFrozen)
		Freeze();
	else if(((m_TileIndex == TILE_UNFREEZE) || (m_TileFIndex == TILE_UNFREEZE)) && !m_Core.m_DeepFrozen)
		UnFreeze();

	// deep freeze
	if(((m_TileIndex == TILE_DFREEZE) || (m_TileFIndex == TILE_DFREEZE)) && !m_Core.m_Super && !m_Core.m_DeepFrozen)
		m_Core.m_DeepFrozen = true;
	else if(((m_TileIndex == TILE_DUNFREEZE) || (m_TileFIndex == TILE_DUNFREEZE)) && !m_Core.m_Super && m_Core.m_DeepFrozen)
		m_Core.m_DeepFrozen = false;

	// stopper
	if(m_Core.m_Vel.y > 0 && (m_MoveRestrictions & CANTMOVE_DOWN))
	{
		m_Core.m_Jumped = 0;
		m_Core.m_JumpedTotal = 0;
	}
	m_Core.m_Vel = ClampVel(m_MoveRestrictions, m_Core.m_Vel);

	HandleMMOTiles(m_TileIndex);

	int z = Collision()->IsTeleport(MapIndex);
	if(!g_Config.m_SvOldTeleportHook && !g_Config.m_SvOldTeleportWeapons && z && !(*m_pTeleOuts)[z - 1].empty())
	{
		if(m_Core.m_Super)
			return;
		int TeleOut = m_Core.m_pWorld->RandomOr0((*m_pTeleOuts)[z - 1].size());
		m_Core.m_Pos = (*m_pTeleOuts)[z - 1][TeleOut];
		if(!g_Config.m_SvTeleportHoldHook)
		{
			ResetHook();
		}
		if(g_Config.m_SvTeleportLoseWeapons)
			ResetPickups();
		return;
	}
	int evilz = Collision()->IsEvilTeleport(MapIndex);
	if(evilz && !(*m_pTeleOuts)[evilz - 1].empty())
	{
		if(m_Core.m_Super)
			return;
		int TeleOut = m_Core.m_pWorld->RandomOr0((*m_pTeleOuts)[evilz - 1].size());
		m_Core.m_Pos = (*m_pTeleOuts)[evilz - 1][TeleOut];
		if(!g_Config.m_SvOldTeleportHook && !g_Config.m_SvOldTeleportWeapons)
		{
			m_Core.m_Vel = vec2(0, 0);

			if(!g_Config.m_SvTeleportHoldHook)
			{
				ResetHook();
				GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
			}
			if(g_Config.m_SvTeleportLoseWeapons)
			{
				ResetPickups();
			}
		}
		return;
	}
	if(Collision()->IsCheckEvilTeleport(MapIndex))
	{
		if(m_Core.m_Super)
			return;
		// first check if there is a TeleCheckOut for the current recorded checkpoint, if not check previous checkpoints
		for(int k = m_TeleCheckpoint - 1; k >= 0; k--)
		{
			if(!(*m_pTeleCheckOuts)[k].empty())
			{
				int TeleOut = m_Core.m_pWorld->RandomOr0((*m_pTeleCheckOuts)[k].size());
				m_Core.m_Pos = (*m_pTeleCheckOuts)[k][TeleOut];
				m_Core.m_Vel = vec2(0, 0);

				if(!g_Config.m_SvTeleportHoldHook)
				{
					ResetHook();
					GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
				}

				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if(GameServer()->m_pController->CanSpawn(m_pPlayer->GetTeam(), &SpawnPos, GameServer()->GetDDRaceTeam(GetPlayer()->GetCID())))
		{
			m_Core.m_Pos = SpawnPos;
			m_Core.m_Vel = vec2(0, 0);

			if(!g_Config.m_SvTeleportHoldHook)
			{
				ResetHook();
				GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
			}
		}
		return;
	}
	if(Collision()->IsCheckTeleport(MapIndex))
	{
		if(m_Core.m_Super)
			return;
		// first check if there is a TeleCheckOut for the current recorded checkpoint, if not check previous checkpoints
		for(int k = m_TeleCheckpoint - 1; k >= 0; k--)
		{
			if(!(*m_pTeleCheckOuts)[k].empty())
			{
				int TeleOut = m_Core.m_pWorld->RandomOr0((*m_pTeleCheckOuts)[k].size());
				m_Core.m_Pos = (*m_pTeleCheckOuts)[k][TeleOut];

				if(!g_Config.m_SvTeleportHoldHook)
				{
					ResetHook();
				}

				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if(GameServer()->m_pController->CanSpawn(m_pPlayer->GetTeam(), &SpawnPos, GameServer()->GetDDRaceTeam(GetPlayer()->GetCID())))
		{
			m_Core.m_Pos = SpawnPos;

			if(!g_Config.m_SvTeleportHoldHook)
			{
				ResetHook();
			}
		}
		return;
	}
}

void CCharacter::HandleTuneLayer()
{
	m_TuneZoneOld = m_TuneZone;
	int CurrentIndex = Collision()->GetMapIndex(m_Pos);
	m_TuneZone = Collision()->IsTune(CurrentIndex);

	if(m_TuneZone)
		m_Core.m_Tuning = GameServer()->TuningList()[m_TuneZone]; // throw tunings from specific zone into gamecore
	else
		m_Core.m_Tuning = *GameServer()->Tuning();

	if(m_TuneZone != m_TuneZoneOld) // don't send tunigs all the time
	{
		// send zone msgs
		SendZoneMsgs();
	}
}

void CCharacter::SendZoneMsgs()
{
	// send zone leave msg
	// (m_TuneZoneOld >= 0: avoid zone leave msgs on spawn)
	if(m_TuneZoneOld >= 0 && GameServer()->m_aaZoneLeaveMsg[m_TuneZoneOld][0])
	{
		const char *pCur = GameServer()->m_aaZoneLeaveMsg[m_TuneZoneOld];
		const char *pPos;
		while((pPos = str_find(pCur, "\\n")))
		{
			char aBuf[256];
			str_copy(aBuf, pCur, pPos - pCur + 1);
			aBuf[pPos - pCur + 1] = '\0';
			pCur = pPos + 2;
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		}
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), pCur);
	}
	// send zone enter msg
	if(GameServer()->m_aaZoneEnterMsg[m_TuneZone][0])
	{
		const char *pCur = GameServer()->m_aaZoneEnterMsg[m_TuneZone];
		const char *pPos;
		while((pPos = str_find(pCur, "\\n")))
		{
			char aBuf[256];
			str_copy(aBuf, pCur, pPos - pCur + 1);
			aBuf[pPos - pCur + 1] = '\0';
			pCur = pPos + 2;
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		}
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), pCur);
	}
}

void CCharacter::SetTeams(CGameTeams *pTeams)
{
	m_pTeams = pTeams;
	m_Core.SetTeamsCore(&m_pTeams->m_Core);
}

void CCharacter::SetRescue()
{
	m_SetSavePos = true;
}

void CCharacter::DDRaceTick()
{
	mem_copy(&m_Input, &m_SavedInput, sizeof(m_Input));
	if(m_Input.m_Direction != 0 || m_Input.m_Jump != 0)
		m_LastMove = Server()->Tick();

	if(m_Core.m_LiveFrozen && !m_Core.m_Super)
	{
		m_Input.m_Direction = 0;
		m_Input.m_Jump = 0;
		// Hook is possible in live freeze
	}
	if(m_FreezeTime > 0)
	{
		if(m_FreezeTime % Server()->TickSpeed() == Server()->TickSpeed() - 1)
		{
			GameServer()->CreateDamageInd(m_Pos, 0, (m_FreezeTime + 1) / Server()->TickSpeed(), TeamMask() & GameServer()->ClientsMaskExcludeClientVersionAndHigher(VERSION_DDNET_NEW_HUD));
		}
		m_FreezeTime--;
		m_Input.m_Direction = 0;
		m_Input.m_Jump = 0;
		m_Input.m_Hook = 0;
		if(m_FreezeTime == 1)
			UnFreeze();
	}

	HandleTuneLayer(); // need this before coretick

	// check if the tee is in any type of freeze
	int Index = Collision()->GetPureMapIndex(m_Pos);
	const int aTiles[] = {
		Collision()->GetTileIndex(Index),
		Collision()->GetFTileIndex(Index),
		Collision()->GetSwitchType(Index)};
	m_Core.m_IsInFreeze = false;
	for(const int Tile : aTiles)
	{
		if(Tile == TILE_FREEZE || Tile == TILE_DFREEZE)
		{
			m_Core.m_IsInFreeze = true;
			break;
		}
	}

	// look for save position for rescue feature
	if(g_Config.m_SvRescue || ((g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO || Team() > TEAM_FLOCK) && Team() >= TEAM_FLOCK && Team() < TEAM_SUPER))
	{
		if(!m_Core.m_IsInFreeze && IsGrounded() && !m_Core.m_DeepFrozen)
		{
			SetRescue();
		}
	}

	m_Core.m_Id = GetPlayer()->GetCID();
}

void CCharacter::DDRacePostCoreTick()
{
	m_Time = (float)(Server()->Tick() - m_StartTime) / ((float)Server()->TickSpeed());

	if(m_Core.m_EndlessHook || (m_Core.m_Super && g_Config.m_SvEndlessSuperHook))
		m_Core.m_HookTick = 0;

	m_FrozenLastTick = false;

	if(m_Core.m_DeepFrozen && !m_Core.m_Super)
		Freeze();

	// following jump rules can be overridden by tiles, like Refill Jumps, Stopper and Wall Jump
	if(m_Core.m_Jumps == -1)
	{
		// The player has only one ground jump, so his feet are always dark
		m_Core.m_Jumped |= 2;
	}
	else if(m_Core.m_Jumps == 0)
	{
		// The player has no jumps at all, so his feet are always dark
		m_Core.m_Jumped |= 2;
	}
	else if(m_Core.m_Jumps == 1 && m_Core.m_Jumped > 0)
	{
		// If the player has only one jump, each jump is the last one
		m_Core.m_Jumped |= 2;
	}
	else if(m_Core.m_JumpedTotal < m_Core.m_Jumps - 1 && m_Core.m_Jumped > 1)
	{
		// The player has not yet used up all his jumps, so his feet remain light
		m_Core.m_Jumped = 1;
	}

	if((m_Core.m_Super || m_Core.m_EndlessJump) && m_Core.m_Jumped > 1)
	{
		// Super players and players with infinite jumps always have light feet
		m_Core.m_Jumped = 1;
	}

	int CurrentIndex = Collision()->GetMapIndex(m_Pos);
	HandleSkippableTiles(CurrentIndex);
	if(!m_Alive)
		return;

	// handle Anti-Skip tiles
	std::list<int> Indices = Collision()->GetMapIndices(m_PrevPos, m_Pos);
	if(!Indices.empty())
	{
		for(int &Index : Indices)
		{
			HandleTiles(Index);
			if(!m_Alive)
				return;
		}
	}
	else
	{
		HandleTiles(CurrentIndex);
		if(!m_Alive)
			return;
	}

	// teleport gun
	if(m_TeleGunTeleport)
	{
		GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), TeamMask());
		m_Core.m_Pos = m_TeleGunPos;
		if(!m_IsBlueTeleGunTeleport)
			m_Core.m_Vel = vec2(0, 0);
		GameServer()->CreateDeath(m_TeleGunPos, m_pPlayer->GetCID(), TeamMask());
		GameServer()->CreateSound(m_TeleGunPos, SOUND_WEAPON_SPAWN, TeamMask());
		m_TeleGunTeleport = false;
		m_IsBlueTeleGunTeleport = false;
	}
}

bool CCharacter::Freeze(int Seconds)
{
	if(Seconds <= 0 || m_Core.m_Super || m_FreezeTime > Seconds * Server()->TickSpeed())
		return false;
	if(m_Core.m_FreezeStart < Server()->Tick() - Server()->TickSpeed())
	{
		m_Armor = 0;
		m_FreezeTime = Seconds * Server()->TickSpeed();
		m_Core.m_FreezeStart = Server()->Tick();
		return true;
	}
	return false;
}

bool CCharacter::Freeze()
{
	return Freeze(g_Config.m_SvFreezeDelay);
}

bool CCharacter::UnFreeze()
{
	if(m_FreezeTime > 0)
	{
		m_Armor = 10;
		if(!m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Got)
			m_Core.m_ActiveWeapon = WEAPON_GUN;
		m_FreezeTime = 0;
		m_Core.m_FreezeStart = 0;
		m_FrozenLastTick = true;
		return true;
	}
	return false;
}

void CCharacter::GiveWeapon(int Weapon, bool Remove, int Ammo)
{
	if(Weapon == WEAPON_NINJA)
	{
		if(Remove)
			RemoveNinja();
		else
			GiveNinja();
		return;
	}

	if(Remove)
	{
		if(GetActiveWeapon() == Weapon)
			SetActiveWeapon(WEAPON_GUN);
	}
	else
	{
		m_Core.m_aWeapons[Weapon].m_Ammo = -1;
	}

	m_Core.m_aWeapons[Weapon].m_Got = !Remove;
	m_Core.m_aWeapons[Weapon].m_AmmoRegenStart = 0;
	m_Core.m_aWeapons[Weapon].m_MaxAmmo = 10 + (m_pPlayer->m_AccUp.m_Ammo * 5);
	m_Core.m_aWeapons[Weapon].m_Ammo = m_Core.m_aWeapons[Weapon].m_MaxAmmo;
}

void CCharacter::GiveAllWeapons()
{
	for(int i = WEAPON_GUN; i < NUM_WEAPONS - 1; i++)
	{
		GiveWeapon(i);
	}
}

void CCharacter::ResetPickups()
{
	for(int i = WEAPON_SHOTGUN; i < NUM_WEAPONS - 1; i++)
	{
		m_Core.m_aWeapons[i].m_Got = false;
		if(m_Core.m_ActiveWeapon == i)
			m_Core.m_ActiveWeapon = WEAPON_GUN;
	}
}

void CCharacter::SetEndlessHook(bool Enable)
{
	if(m_Core.m_EndlessHook == Enable)
	{
		return;
	}
	GameServer()->SendChatTarget(GetPlayer()->GetCID(), Enable ? "Endless hook has been activated" : "Endless hook has been deactivated");

	m_Core.m_EndlessHook = Enable;
}

void CCharacter::Pause(bool Pause)
{
	m_Paused = Pause;
	if(Pause)
	{
		GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
		GameServer()->m_World.RemoveEntity(this);

		if(m_Core.m_HookedPlayer != -1 || m_Core.m_pHookedDummy) // Keeping hook would allow cheats
		{
			ResetHook();
			GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
		}
	}
	else
	{
		m_Core.m_Vel = vec2(0, 0);
		GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;
		GameServer()->m_World.InsertEntity(this);
	}
}

void CCharacter::DDRaceInit()
{
	m_Paused = false;
	m_DDRaceState = DDRACE_NONE;
	m_PrevPos = m_Pos;
	m_SetSavePos = false;
	m_LastBroadcast = 0;
	m_TeamBeforeSuper = 0;
	m_Core.m_Id = GetPlayer()->GetCID();
	m_TeleCheckpoint = 0;
	m_Core.m_EndlessHook = g_Config.m_SvEndlessDrag;
	if(g_Config.m_SvHit)
	{
		m_Core.m_HammerHitDisabled = false;
		m_Core.m_ShotgunHitDisabled = false;
		m_Core.m_GrenadeHitDisabled = false;
		m_Core.m_LaserHitDisabled = false;
	}
	else
	{
		m_Core.m_HammerHitDisabled = true;
		m_Core.m_ShotgunHitDisabled = true;
		m_Core.m_GrenadeHitDisabled = true;
		m_Core.m_LaserHitDisabled = true;
	}
	m_Core.m_Jumps = 2;
	m_FreezeHammer = false;

	int Team = Teams()->m_Core.Team(m_Core.m_Id);

	if(Teams()->TeamLocked(Team))
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(Teams()->m_Core.Team(i) == Team && i != m_Core.m_Id && GameServer()->m_apPlayers[i])
			{
				CCharacter *pChar = GameServer()->m_apPlayers[i]->GetCharacter();

				if(pChar)
				{
					m_DDRaceState = pChar->m_DDRaceState;
					m_StartTime = pChar->m_StartTime;
				}
			}
		}
	}

	if(g_Config.m_SvTeam == SV_TEAM_MANDATORY && Team == TEAM_FLOCK)
	{
		GameServer()->SendStartWarning(GetPlayer()->GetCID(), "Please join a team before you start");
	}
}

void CCharacter::Rescue()
{
	if(m_SetSavePos && !m_Core.m_Super)
	{
		if(m_LastRescue + (int64_t)g_Config.m_SvRescueDelay * Server()->TickSpeed() > Server()->Tick())
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "You have to wait %d seconds until you can rescue yourself", (int)((m_LastRescue + (int64_t)g_Config.m_SvRescueDelay * Server()->TickSpeed() - Server()->Tick()) / Server()->TickSpeed()));
			GameServer()->SendChatTarget(GetPlayer()->GetCID(), aBuf);
			return;
		}

		float StartTime = m_StartTime;
		// Don't load these from saved tee:
		m_Core.m_Vel = vec2(0, 0);
		m_Core.m_HookState = HOOK_IDLE;
		m_StartTime = StartTime;
		m_SavedInput.m_Direction = 0;
		m_SavedInput.m_Jump = 0;
		// simulate releasing the fire button
		if((m_SavedInput.m_Fire & 1) != 0)
			m_SavedInput.m_Fire++;
		m_SavedInput.m_Fire &= INPUT_STATE_MASK;
		m_SavedInput.m_Hook = 0;
		m_pPlayer->Pause(CPlayer::PAUSE_NONE, true);
	}
}

CClientMask CCharacter::TeamMask()
{
	return Teams()->TeamMask(Team(), -1, GetPlayer()->GetCID());
}

void CCharacter::SwapClients(int Client1, int Client2)
{
	//m_Core.SetHookedPlayer(m_Core.m_HookedPlayer == Client1 ? Client2 : m_Core.m_HookedPlayer == Client2 ? Client1 : m_Core.m_HookedPlayer);
}

void CCharacter::HandleMMOTiles(int Tile)
{
	int ClientID = m_pPlayer->GetCID();

	if (Tile == TILE_OFF_DAMAGE && !m_NoDamage)
	{
		m_NoDamage = true;
		GameServer()->SendMMOBroadcast(ClientID, 1.f, "YOU ENTERED NO DAMAGE ZONE");
	}
	else if (Tile == TILE_ON_DAMAGE && m_NoDamage)
	{
		m_NoDamage = false;
		GameServer()->SendMMOBroadcast(ClientID, 1.f, "YOU EXIT NO DAMAGE ZONE");
	}

	else if (Tile == TILE_CHAIR_10 && Server()->Tick() % Server()->TickSpeed() == 0)
	{
		m_pPlayer->AddEXP(10);
		m_pPlayer->m_AccData.m_Money++;
		GameServer()->SendMMOBroadcast(ClientID, 1.f, "+10 EXP FROM CHAIR");
	}
	else if (Tile == TILE_CHAIR_15 && Server()->Tick() % Server()->TickSpeed() == 0)
	{
		m_pPlayer->AddEXP(15);
		m_pPlayer->m_AccData.m_Money++;
		GameServer()->SendMMOBroadcast(ClientID, 1.f, "+15 EXP FROM CHAIR");
	}

	else if (Tile == TILE_WATER)
	{
		if (!m_InWater)
			GameServer()->CreateDeath(m_Pos, ClientID);

		m_Core.m_Vel.y -= GameServer()->Tuning()->m_Gravity * 1.1f;
		m_InWater = true;
	}
	else
	{
		if (m_InWater)
			GameServer()->CreateDeath(m_Pos, ClientID);

		m_InWater = false;
	}

	if (Tile == TILE_SHOP_ON && !m_InShop)
	{
		m_InShop = true;
		GameServer()->SendMMOBroadcast(ClientID, 1.5f, "YOU'RE IN SHOP NOW");

		GameServer()->m_VoteMenu.SetMenu(ClientID, MENU_MAIN);
		GameServer()->m_VoteMenu.RebuildMenu(ClientID);

		int MaterialCount = m_pPlayer->m_AccInv.ItemCount(ITEM_MATERIAL);
		if (MaterialCount > 0) {
			int Money = MaterialCount / 10;
			m_pPlayer->m_AccInv.RemItem(ITEM_MATERIAL);
			m_pPlayer->m_AccData.m_Money += Money;
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "You have sold %d materials for %d money", MaterialCount, Money);
			GameServer()->SendChatTarget(ClientID, aBuf);
		}
	}
	else if (Tile == TILE_SHOP_OFF && m_InShop)
	{
		m_InShop = false;
		GameServer()->SendMMOBroadcast(ClientID, 1.5f, "YOU'RE NOT IN SHOP NOW");

		GameServer()->m_VoteMenu.SetMenu(ClientID, MENU_MAIN);
		GameServer()->m_VoteMenu.RebuildMenu(ClientID);
	}

	if (Tile == TILE_CRAFT_ON && !m_InCraft)
	{
		m_InCraft = true;
		GameServer()->SendMMOBroadcast(ClientID, 1.5f, "YOU'RE IN CRAFT ROOM NOW");

		GameServer()->m_VoteMenu.SetMenu(ClientID, MENU_MAIN);
		GameServer()->m_VoteMenu.RebuildMenu(ClientID);
	}
	else if (Tile == TILE_CRAFT_OFF && m_InCraft)
	{
		m_InCraft = false;
		GameServer()->SendMMOBroadcast(ClientID, 1.5f, "YOU'RE NOT IN CRAFT ROOM NOW");

		GameServer()->m_VoteMenu.SetMenu(ClientID, MENU_MAIN);
		GameServer()->m_VoteMenu.RebuildMenu(ClientID);
	}
}
