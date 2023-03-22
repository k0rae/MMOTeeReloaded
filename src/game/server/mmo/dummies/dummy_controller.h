#ifndef GAME_SERVER_MMO_DUMMIES_DUMMY_CONTROLLER_H
#define GAME_SERVER_MMO_DUMMIES_DUMMY_CONTROLLER_H

#include <base/vmath.h>

class CDummyController
{
	friend class CDummyBase;

protected:
	class CDummyBase *m_pDummyBase;

	class CGameContext *GameServer();
	class IServer *Server();
	class CGameWorld *GameWorld();
	class CCollision *Collision();

	void MoveLeft();
	void MoveNone();
	void MoveRight();
	void SetMove(int Dir);

	void Fire();
	void Hook();
	void Jump();
	void SetWeapon(int Weapon);

	void SetAimX(int X);
	void SetAimY(int Y);
	void SetAim(int X, int Y);
	void SetAim(vec2 Pos);

	void ResetInput();

	virtual void Tick() {};

public:
	virtual ~CDummyController() = default;

	void DummyTick();
};

#endif // GAME_SERVER_MMO_DUMMIES_DUMMY_CONTROLLER_H
