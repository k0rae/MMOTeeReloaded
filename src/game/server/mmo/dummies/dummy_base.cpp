#include "dummy_base.h"

#include <game/server/gamecontext.h>

CDummyBase::CDummyBase(CGameWorld *pWorld, vec2 Pos, int DummyType) :
	CEntity(pWorld, CGameWorld::ENTTYPE_DUMMY, Pos)
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

	Spawn();
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

void CDummyBase::Die()
{
	m_Alive = false;
	m_SpawnTick = Server()->Tick() + Server()->TickSpeed();

	GameServer()->CreateDeath(m_Pos, 0);
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE);
}

void CDummyBase::TakeDamage(vec2 Force, int Damage, int From, int Weapon)
{
	if(Damage)
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
		Die();

	vec2 Temp = m_Core.m_Vel + Force;
	m_Core.m_Vel = Temp;
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
	{
		Spawn();
	}

	// Don't calc phys if dummy is dead
	if (!m_Alive)
		return;

	m_Core.m_Input = m_Input;
	m_Core.Tick(true);
	m_Core.Move();

	m_Pos = m_Core.m_Pos;
}

void CDummyBase::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	int SelfID = GameServer()->GetNextBotSnapID(SnappingClient);
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
	pCharacter->m_Direction = 0;
	pCharacter->m_Weapon = 0;
	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;
	pCharacter->m_PlayerFlags = PLAYERFLAG_PLAYING;
}
