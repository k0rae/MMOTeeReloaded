#include "admin_commands.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

bool CheckClientID(int ClientID);

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

void CAdminCommands::ConGiveItem(IConsole::IResult *pResult, void *pUserData)
{
	CAdminCommands *pSelf = (CAdminCommands *)pUserData;

	int ClientID = pResult->GetVictim();
	int ItemID = pResult->GetInteger(1);
	int Count = 1;
	int Quality = QUALITY_0;
	int Data = 0;

	if (pResult->NumArguments() > 2)
		Count = pResult->GetInteger(2);
	if (pResult->NumArguments() > 3)
		Quality = pResult->GetInteger(3);
	if (pResult->NumArguments() > 4)
		Data = pResult->GetInteger(4);

	pSelf->MMOCore()->GiveItem(ClientID, ItemID, Count, Quality, Data);
}

void CAdminCommands::ConSetLevel(IConsole::IResult *pResult, void *pUserData)
{
	CAdminCommands *pSelf = (CAdminCommands *)pUserData;
	int ClientID = pResult->GetVictim();
	if(!CheckClientID(ClientID))
		return;
	CPlayer *pPly = pSelf->GameServer()->m_apPlayers[ClientID];
	if (!pPly)
		return;

	pPly->m_AccData.m_Level = pResult->GetInteger(1);
}

void CAdminCommands::OnConsoleInit()
{
	Console()->Register("exec_sql", "s[query]", CFGFLAG_SERVER, ConExecuteSQL, this, "SQL admin management");
	Console()->Register("exec_sql_get", "s[ret_type] s[query]", CFGFLAG_SERVER, ConExecuteSQLGet, this, "SQL admin management");
	Console()->Register("give_item", "v[id] i[item_id] ?i[count] ?i[quality] ?i[data]", CFGFLAG_SERVER, ConGiveItem, this, "Account admin management");
}
