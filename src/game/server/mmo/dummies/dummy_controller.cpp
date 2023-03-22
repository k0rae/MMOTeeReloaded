#include "dummy_controller.h"

#include "dummy_base.h"

class CGameContext *CDummyController::GameServer() { return m_pDummyBase->GameServer(); }
class IServer *CDummyController::Server() { return m_pDummyBase->Server(); }
class CGameWorld *CDummyController::GameWorld() { return m_pDummyBase->GameWorld(); }
class CCollision *CDummyController::Collision() { return m_pDummyBase->Collision(); }

enum
{
	DIRECTION_LEFT = -1,
	DIRECTION_NONE,
	DIRECTION_RIGHT
};

void CDummyController::MoveLeft() { m_pDummyBase->m_Input.m_Direction = DIRECTION_LEFT; }
void CDummyController::MoveNone() { m_pDummyBase->m_Input.m_Direction = DIRECTION_NONE; }
void CDummyController::MoveRight() { m_pDummyBase->m_Input.m_Direction = DIRECTION_RIGHT; }
void CDummyController::SetMove(int Dir) { m_pDummyBase->m_Input.m_Direction = Dir; }
void CDummyController::Fire() { m_pDummyBase->m_Input.m_Fire = 1; }
void CDummyController::Hook() { m_pDummyBase->m_Input.m_Hook = 1; }
void CDummyController::Jump() { m_pDummyBase->m_Input.m_Jump = 1; }
void CDummyController::SetWeapon(int Weapon) { m_pDummyBase->m_Input.m_WantedWeapon = Weapon; }
void CDummyController::SetAimX(int X){ m_pDummyBase->m_Input.m_TargetX = X; }
void CDummyController::SetAimY(int Y) { m_pDummyBase->m_Input.m_TargetY = Y; }
void CDummyController::SetAim(vec2 Pos) { SetAim(Pos.x, Pos.y); }
void CDummyController::SetAim(int X, int Y)
{
	SetAimX(X);
	SetAimY(Y);
}

void CDummyController::ResetInput()
{
	m_pDummyBase->m_Input.m_Direction = DIRECTION_NONE;
	m_pDummyBase->m_Input.m_Fire = 0;
	m_pDummyBase->m_Input.m_Hook = 0;
	m_pDummyBase->m_Input.m_Jump = 0;
}

void CDummyController::DummyTick()
{
	ResetInput();

	m_Pos = m_pDummyBase->m_Pos;

	Tick();
}
