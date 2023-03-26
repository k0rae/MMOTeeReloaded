#ifndef GAME_SERVER_MMO_ACCOUNT_MANAGER_H
#define GAME_SERVER_MMO_ACCOUNT_MANAGER_H

#include <game/server/mmo/component.h>
#include <game/server/mmo/items.h>
#include <game/server/mmo/account_data.h>

#include <engine/server/databases/connection_pool.h>
#include <engine/console.h>
#include <base/hash.h>

struct SAccountRegisterResult;
struct SAccountLoginResult;
struct SAdminExecResult;

class CAccountManager : public CServerComponent
{
	std::vector<std::shared_ptr<SAccountRegisterResult>> m_vpRegisterResults;
	std::vector<std::shared_ptr<SAccountLoginResult>> m_vpLoginResults;
	std::vector<std::shared_ptr<SAdminExecResult>> m_vpAdminExecResults;

	// Chat commands
	static void ChatRegister(IConsole::IResult *pResult, void *pUserData);
	static void ChatLogin(IConsole::IResult *pResult, void *pUserData);

	// Threads
	static bool RegisterThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool LoginThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool SaveThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool AdminExecThread(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);

	CDbConnectionPool *DBPool();

public:
	virtual void OnConsoleInit() override;
	virtual void OnTick() override;
	virtual void OnPlayerLeft(int ClientID) override;

	void Register(int ClientID, const char *pName, const char *pPasswordHash);
	void Login(int ClientID, const char *pName, const char *pPasswordHash);
	void ExecAdminSQL(int ClientID, const char *pQuery);
	void ExecAdminSQLGet(int ClientID, int RetType, const char *pQuery);

	void Save(int ClientID);
};

struct SAccountResultBase : ISqlResult
{
	SAccountResultBase()
	{
		m_aMessage[0] = '\0';
		m_State = STATE_FAILED;
		m_ClientID = -1;
	}

	enum
	{
		STATE_FAILED = -1,
		STATE_SUCCESSFUL
	};

	int m_State;
	char m_aMessage[512];
	int m_ClientID;
};

struct SAccountRegisterResult : SAccountResultBase
{
	SAccountRegisterResult() = default;
};

struct SAccountRegisterRequest : ISqlData
{
	SAccountRegisterRequest(std::shared_ptr<SAccountRegisterResult> pResult) :
		ISqlData(std::move(pResult))
	{
		m_aLogin[0] = '\0';
		m_aPasswordHash[0] = '\0';
	}

	char m_aLogin[MAX_LOGIN_LENGTH];
	char m_aPasswordHash[MD5_MAXSTRSIZE];
};

struct SAccountLoginResult : SAccountResultBase
{
	SAccountLoginResult() = default;

	SAccountData m_AccData;
	CAccountInventory m_AccInv;
	SAccountWorksData m_AccWorks;
	SAccountUpgrade m_AccUp;
};

struct SAccountLoginRequest : ISqlData
{
	SAccountLoginRequest(std::shared_ptr<SAccountLoginResult> pResult) :
		ISqlData(std::move(pResult))
	{
		m_aLogin[0] = '\0';
		m_aPasswordHash[0] = '\0';
	}

	char m_aLogin[MAX_LOGIN_LENGTH];
	char m_aPasswordHash[MD5_MAXSTRSIZE];
};

struct SAccountSaveRequest : ISqlData
{
	SAccountSaveRequest() :
		ISqlData(nullptr)
	{}

	SAccountData m_AccData;
	CAccountInventory m_AccInv;
	SAccountWorksData m_AccWorks;
	SAccountUpgrade m_AccUp;
};

struct SAdminExecResult : SAccountResultBase
{
	SAdminExecResult() = default;

	int m_ClientID;
	char m_aMessage[256];
};

struct SAdminExecRequest : ISqlData
{
	SAdminExecRequest(std::shared_ptr<SAdminExecResult> pResult) :
		ISqlData(std::move(pResult))
	{

	}

	bool m_IsGet;
	int m_RetType;
	char m_aQuery[1024];
};

#endif // GAME_SERVER_MMO_ACCOUNT_MANAGER_H
