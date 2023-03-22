#ifndef GAME_SERVER_MMO_DUMMIES_DUMMY_CONTROLLER_H
#define GAME_SERVER_MMO_DUMMIES_DUMMY_CONTROLLER_H

class CDummyController
{
	class CDummyBase *m_pDummyBase;

	class CGameContext *GameServer();
	class IServer *Server();
	class CGameWorld *GameWorld();
	class CCollision *Collision();
public:
	void MoveLeft();
	void MoveNone();
	void MoveRight();

	void Fire();
	void Hook();
	void Jump();
	void SetWeapon(int Weapon);

	void ResetInput();

	void DummyTick();

	virtual void Tick() {};
};

#endif // GAME_SERVER_MMO_DUMMIES_DUMMY_CONTROLLER_H
