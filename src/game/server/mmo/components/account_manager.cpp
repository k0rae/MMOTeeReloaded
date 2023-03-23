#include "account_manager.h"

#include <engine/server/server.h>
#include <game/server/gamecontext.h>
#include <engine/server/databases/connection.h>
#include <engine/shared/config.h>
#include <game/server/player.h>

bool CheckClientID(int ClientID);

CDbConnectionPool *CAccountManager::DBPool() { return ((CServer *)Server())->DbPool(); }

bool CAccountManager::RegisterThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const SAccountRegisterRequest *pData = dynamic_cast<const SAccountRegisterRequest *>(pGameData);
	SAccountRegisterResult *pResult = dynamic_cast<SAccountRegisterResult *>(pGameData->m_pResult.get());

	char aBuf[1024];

	str_copy(aBuf, "SELECT COUNT(*) AS logins FROM users WHERE name = ?");
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		return true;
	pSqlServer->BindString(1, pData->m_aLogin);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
		return true;
	if(pSqlServer->GetInt(1) != 0)
	{
		str_format(pResult->m_aMessage, sizeof(pResult->m_aMessage), "This name is already taken.");
		return false;
	}

	str_copy(aBuf, "INSERT INTO users(name, password) VALUES(?, ?);");

	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		return true;
	pSqlServer->BindString(1, pData->m_aLogin);
	pSqlServer->BindString(2, pData->m_aPasswordHash);
	pSqlServer->Print();

	int NumInserted;
	if(pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
		return true;

	str_format(pResult->m_aMessage, sizeof(pResult->m_aMessage), "Register successful. Now you can login to your new account using /login command.");
	pResult->m_State = SAccountResultBase::STATE_SUCCESSFUL;

	return false;
}

void CAccountManager::Register(int ClientID, const char *pName, const char *pPasswordHash)
{
	auto pResult = std::make_shared<SAccountRegisterResult>();
	pResult->m_ClientID = ClientID;
	m_vpRegisterResults.push_back(pResult);

	auto Request = std::make_unique<SAccountRegisterRequest>(pResult);
	str_copy(Request->m_aLogin, pName);
	str_copy(Request->m_aPasswordHash, pPasswordHash);
	DBPool()->Execute(RegisterThread, std::move(Request), "Register");
}

bool CAccountManager::LoginThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const SAccountLoginRequest *pData = dynamic_cast<const SAccountLoginRequest *>(pGameData);
	SAccountLoginResult *pResult = dynamic_cast<SAccountLoginResult *>(pGameData->m_pResult.get());

	char aBuf[1024];

	str_copy(aBuf, "SELECT id, level, exp, money, donate FROM users WHERE name = ? AND password = ?");
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		return true;
	pSqlServer->BindString(1, pData->m_aLogin);
	pSqlServer->BindString(2, pData->m_aPasswordHash);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
		return true;
	if (End)
	{
		str_copy(pResult->m_aMessage, "User with given username and password is not registered.");
		return false;
	}

	// Load user vars
	pResult->m_AccData.m_ID = pSqlServer->GetInt(1);
	pResult->m_AccData.m_Level = pSqlServer->GetInt(2);
	pResult->m_AccData.m_EXP = pSqlServer->GetInt(3);
	pResult->m_AccData.m_Money = pSqlServer->GetInt(4);
	pResult->m_AccData.m_Donate = pSqlServer->GetInt(5);
	str_copy(pResult->m_AccData.m_aAccountName, pData->m_aLogin);

	// Load inventory
	str_copy(aBuf, "SELECT item_id, count, quality, data FROM u_inv WHERE id = ?");
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		return true;
	pSqlServer->BindInt(1, pResult->m_AccData.m_ID);

	CAccountInventory Inventory;

	End = false;
	while(!pSqlServer->Step(&End, pError, ErrorSize) && !End)
	{
		Inventory.AddItem(
			pSqlServer->GetInt(1),
			pSqlServer->GetInt(2),
			pSqlServer->GetInt(3),
			pSqlServer->GetInt(4));
	}

	pResult->m_AccInv = Inventory;
	pResult->m_State = SAccountResultBase::STATE_SUCCESSFUL;
	str_copy(pResult->m_aMessage, "You successfully logged to account.");

	return false;
}

void CAccountManager::Login(int ClientID, const char *pName, const char *pPasswordHash)
{
	for (CPlayer *pPly : GameServer()->m_apPlayers)
	{
		if (pPly && pPly->m_LoggedIn && !str_comp(pPly->m_AccData.m_aAccountName, pName))
		{
			GameServer()->SendChatTarget(ClientID, "Account is already in use.");
			return;
		}
	}

	auto pResult = std::make_shared<SAccountLoginResult>();
	pResult->m_ClientID = ClientID;
	m_vpLoginResults.push_back(pResult);

	auto Request = std::make_unique<SAccountLoginRequest>(pResult);
	str_copy(Request->m_aLogin, pName);
	str_copy(Request->m_aPasswordHash, pPasswordHash);
	DBPool()->Execute(LoginThread, std::move(Request), "Login");
}

bool CAccountManager::SaveThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const SAccountSaveRequest *>(pGameData);

	char aBuf[1024];
	int NumInserted;

	// Update 'users' table
	str_copy(aBuf, "UPDATE users SET level = ?, exp = ?, money = ?, donate = ? WHERE id = ?");

	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		return true;
	pSqlServer->BindInt(1, pData->m_AccData.m_Level);
	pSqlServer->BindInt(2, pData->m_AccData.m_EXP);
	pSqlServer->BindInt(3, pData->m_AccData.m_Money);
	pSqlServer->BindInt(4, pData->m_AccData.m_Donate);
	pSqlServer->BindInt(5, pData->m_AccData.m_ID);

	if(pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
		return true;

	// Clean old inventory
	str_copy(aBuf, "DELETE FROM u_inv WHERE id = ?");

	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		return true;
	pSqlServer->BindInt(1, pData->m_AccData.m_ID);

	if(pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
		return true;

	// Insert items
	str_copy(aBuf, "INSERT INTO u_inv(id, item_id, count, quality, data) VALUES (?, ?, ?, ?, ?)");
	for (SInvItem Item : pData->m_AccInventory.m_vItems)
	{
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
			return true;
		pSqlServer->BindInt(1, pData->m_AccData.m_ID);
		pSqlServer->BindInt(2, Item.m_ID);
		pSqlServer->BindInt(3, Item.m_Count);
		pSqlServer->BindInt(4, Item.m_Quality);
		pSqlServer->BindInt(5, Item.m_Data);

		if(pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
			return true;
	}

	return false;
}

void CAccountManager::Save(int ClientID)
{
	CPlayer *pPly = GameServer()->m_apPlayers[ClientID];
	if (!pPly)
		return;

	auto Request = std::make_unique<SAccountSaveRequest>();
	Request->m_AccData = pPly->m_AccData;
	Request->m_AccInventory = pPly->m_AccInv;
	DBPool()->Execute(SaveThread, std::move(Request), "Save");
}

void CAccountManager::ChatRegister(IConsole::IResult *pResult, void *pUserData)
{
	CAccountManager *pSelf = (CAccountManager *)pUserData;

	int ClientID = pResult->m_ClientID;
	if(!CheckClientID(ClientID))
		return;

	CPlayer *pPlayer = pSelf->GameServer()->m_apPlayers[ClientID];
	if(!pPlayer)
		return;

	if(pPlayer->m_LoggedIn)
	{
		pSelf->GameServer()->SendChatTarget(ClientID, "You already logged in!");
		return;
	}

	if(pSelf->GameServer()->ProcessSpamProtection(ClientID))
	{
		pSelf->GameServer()->SendChatTarget(ClientID, "Woah, take a rest. o.O");
		return;
	}

	const char *pLogin = pResult->GetString(0);
	const char *pPassword = pResult->GetString(1);
	const char *pPassword2 = pResult->GetString(2);

	if (str_comp(pPassword, pPassword2))
	{
		pSelf->GameServer()->SendChatTarget(ClientID, "Passwords not matching.");
		return;
	}

	if(str_length(pLogin) > MAX_LOGIN_LENGTH)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Login is to long. Max len: %d", MAX_LOGIN_LENGTH);
		pSelf->GameServer()->SendChatTarget(ClientID, aBuf);
		return;
	}

	char aPasswordHash[MD5_MAXSTRSIZE];
	md5_str(md5(pPassword, str_length(pPassword)), aPasswordHash, sizeof(aPasswordHash));

	pSelf->Register(ClientID, pLogin, aPasswordHash);
}

void CAccountManager::ChatLogin(IConsole::IResult *pResult, void *pUserData)
{
	CAccountManager *pSelf = (CAccountManager *)pUserData;

	int ClientID = pResult->m_ClientID;
	if(!CheckClientID(ClientID))
		return;

	CPlayer *pPlayer = pSelf->GameServer()->m_apPlayers[ClientID];
	if(!pPlayer)
		return;

	if(pPlayer->m_LoggedIn)
	{
		pSelf->GameServer()->SendChatTarget(ClientID, "You already logged in!");
		return;
	}

	if(pSelf->GameServer()->ProcessSpamProtection(ClientID))
	{
		pSelf->GameServer()->SendChatTarget(ClientID, "Woah, take a rest. o.O");
		return;
	}

	const char *pLogin = pResult->GetString(0);
	const char *pPassword = pResult->GetString(1);

	char aPasswordHash[MD5_MAXSTRSIZE];
	md5_str(md5(pPassword, str_length(pPassword)), aPasswordHash, sizeof(aPasswordHash));

	pSelf->Login(ClientID, pLogin, aPasswordHash);
}

void CAccountManager::OnConsoleInit()
{
	Console()->Register("register", "s[name] s[password] s[password2]", CFGFLAG_SERVER | CFGFLAG_CHAT, ChatRegister, this, "Register new account");
	Console()->Register("login", "s[name] s[password]", CFGFLAG_SERVER | CFGFLAG_CHAT, ChatLogin, this, "Login to existing account");
}

void CAccountManager::OnTick()
{
	for (int i = 0; i < m_vpRegisterResults.size(); i++)
	{
		auto &pResult = m_vpRegisterResults[i];

		if (!pResult->m_Completed)
			continue;

		if (pResult->m_aMessage[0] != '\0')
			GameServer()->SendChatTarget(pResult->m_ClientID, pResult->m_aMessage);

		m_vpRegisterResults.erase(m_vpRegisterResults.begin() + i);
	}

	for (int i = 0; i < m_vpLoginResults.size(); i++)
	{
		auto &pResult = m_vpLoginResults[i];

		if (!pResult->m_Completed)
			continue;
		CPlayer *pPly = GameServer()->m_apPlayers[pResult->m_ClientID];
		if (!pPly)
			continue;

		if (pResult->m_aMessage[0] != '\0')
			GameServer()->SendChatTarget(pResult->m_ClientID, pResult->m_aMessage);

		if (pResult->m_State == SAccountResultBase::STATE_SUCCESSFUL)
		{
			pPly->m_LoggedIn = true;
			pPly->m_AccData = pResult->m_AccData;
			pPly->m_AccInv = pResult->m_AccInv;
		}

		m_vpLoginResults.erase(m_vpLoginResults.begin() + i);
	}
}
