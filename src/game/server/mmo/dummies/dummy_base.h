#ifndef GAME_SERVER_ENTITIES_DUMMIES_DUMMY_BASE_H
#define GAME_SERVER_ENTITIES_DUMMIES_DUMMY_BASE_H

#include <game/server/entity.h>
#include <game/gamecore.h>
#include <game/server/teeinfo.h>

enum
{
	DUMMY_TYPE_STAND,
	DUMMY_TYPE_SLIME
};

class CDummyBase : public CEntity
{
	friend class CDummyController;

	CCharacterCore m_Core;
	CTeeInfo m_TeeInfo;
	CNetObj_PlayerInput m_Input;

	vec2 m_SpawnPos;
	bool m_Alive;
	int m_SpawnTick;

	char m_aName[MAX_NAME_LENGTH];
	char m_aClan[MAX_CLAN_LENGTH];

	int m_Health;
	int m_Armor;
	bool m_NoDamage;

	int m_DummyType;

	int m_DefaultEmote;
	int m_EmoteType;
	int m_EmoteStop;

public:
	CDummyBase(CGameWorld *pWorld, vec2 Pos, int DummyType);

	void Spawn();
	void Die();
	void TakeDamage(vec2 Force, int Damage, int From, int Weapon);

	virtual void Destroy() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;

	// Setters
	void SetName(const char *pName) { str_copy(m_aName, pName); }
	void SetClan(const char *pClan) { str_copy(m_aClan, pClan); }
	void SetTeeInfo(CTeeInfo Info) { m_TeeInfo = Info; }

	// Getters
	bool IsAlive() { return m_Alive; }
	int GetDummyType() { return m_DummyType; }
	CCharacterCore *Core() { return &m_Core; }
};

#endif // GAME_SERVER_ENTITIES_DUMMIES_DUMMY_BASE_H
