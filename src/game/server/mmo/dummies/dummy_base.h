#ifndef GAME_SERVER_ENTITIES_DUMMIES_DUMMY_BASE_H
#define GAME_SERVER_ENTITIES_DUMMIES_DUMMY_BASE_H

#include <game/server/entity.h>
#include <game/gamecore.h>
#include <game/server/teeinfo.h>

class CDummyBase : public CEntity
{
	CCharacterCore m_Core;
	CTeeInfo m_TeeInfo;

	char m_aName[MAX_NAME_LENGTH];
	char m_aClan[MAX_CLAN_LENGTH];

public:
	CDummyBase(CGameWorld *pWorld, vec2 Pos);

	virtual void Destroy() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;

	// Methods for bots
	virtual void DummyTick() {};

	// Setters
	void SetName(const char *pName) { str_copy(m_aName, pName); }
	void SetClan(const char *pClan) { str_copy(m_aClan, pClan); }
	void SetTeeInfo(CTeeInfo Info) { m_TeeInfo = Info; }
};

#endif // GAME_SERVER_ENTITIES_DUMMIES_DUMMY_BASE_H
