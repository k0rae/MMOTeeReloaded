#include "admin_commands.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

void CAdminCommands::ConExecuteSQL(IConsole::IResult *pResult, void *pUserData)
{
	CAdminCommands *pSelf = (CAdminCommands *)pUserData;

	pSelf->GameServer()->m_AccountManager.ExecAdminSQL(pResult->m_ClientID, pResult->GetString(0));
}

void CAdminCommands::ConExecuteSQLGet(IConsole::IResult *pResult, void *pUserData)
{
	CAdminCommands *pSelf = (CAdminCommands *)pUserData;

	int RetType = -1;
	const char *pType = pResult->GetString(0);
	if (!str_comp(pType, "int"))
		RetType = 0;
	else if (!str_comp(pType, "str"))
		RetType = 1;

	if (RetType == -1)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "sql", "invalid ret_type. Allowed types: int, str");
		return;
	}

	pSelf->GameServer()->m_AccountManager.ExecAdminSQLGet(pResult->m_ClientID, RetType, pResult->GetString(1));
}

void CAdminCommands::OnConsoleInit()
{
	Console()->Register("exec_sql", "s[query]", CFGFLAG_SERVER, ConExecuteSQL, this, "SQL admin management");
	Console()->Register("exec_sql_get", "s[ret_type] s[query]", CFGFLAG_SERVER, ConExecuteSQLGet, this, "SQL admin management");
}
