#ifndef GAME_SERVER_MMO_ENTITIES_BALL_EFFECT_H
#define GAME_SERVER_MMO_ENTITIES_BALL_EFFECT_H

#include <game/server/entity.h>

class CBallEffect : public CEntity
{
	constexpr static const float ms_Radius = 10.f;
	int m_ID2;

public:
	CBallEffect(CGameWorld *pWorld, vec2 Pos);
	~CBallEffect();

	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_MMO_ENTITIES_BALL_EFFECT_H
