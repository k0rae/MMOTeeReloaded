#include "component.h"

#include <game/server/gamecontext.h>

CGameContext *CServerComponent::GameServer() const { return m_pGameServer; }
CConfig *CServerComponent::Config() const { return m_pGameServer->Config(); }
IServer *CServerComponent::Server() const { return m_pGameServer->Server(); }
IStorage *CServerComponent::Storage() const { return m_pGameServer->Storage(); }
IConsole *CServerComponent::Console() const { return m_pGameServer->Console(); }
CCollision *CServerComponent::Collision() const { return m_pGameServer->Collision(); }
