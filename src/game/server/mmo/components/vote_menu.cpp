#include "vote_menu.h"

#include <game/server/gamecontext.h>

CVoteMenu::CVoteMenu()
{
	for (int &i : m_aPlayersMenu)
		i = MENU_MAIN;
}

void CVoteMenu::OnMessage(int ClientID, int MsgID, void *pRawMsg, bool InGame)
{
	if (MsgID != NETMSGTYPE_CL_CALLVOTE)
		return;

	CNetMsg_Cl_CallVote *pMsg = (CNetMsg_Cl_CallVote *)pRawMsg;

	char aDesc[VOTE_DESC_LENGTH] = {0};
	char aCmd[VOTE_CMD_LENGTH] = {0};

	if(!str_comp_nocase(pMsg->m_pType, "option"))
	{
		for (auto & i : m_aPlayersVotes[ClientID])
		{
			if(str_comp_nocase(pMsg->m_pValue, i.m_aDescription) == 0)
			{
				str_copy(aDesc, i.m_aDescription);
				str_format(aCmd, sizeof(aCmd), "%s", i.m_aCommand);
			}
		}
	}

	if(!str_comp(aCmd, "null"))
		return;

	// Handle cmds
	int Value1;
	if (sscanf(aCmd, "set%d", &Value1))
	{
		m_aPlayersMenu[ClientID] = Value1;
		RebuildMenu(ClientID);
	}
}

void CVoteMenu::AddMenuVote(int ClientID, const char *pCmd, const char *pDesc)
{
	int Len = str_length(pCmd);

	CVoteOptionServer Vote;
	str_copy(Vote.m_aDescription, pDesc, sizeof(Vote.m_aDescription));
	mem_copy(Vote.m_aCommand, pCmd, Len + 1);
	m_aPlayersVotes[ClientID].push_back(Vote);

	// inform clients about added option
	CNetMsg_Sv_VoteOptionAdd OptionMsg;
	OptionMsg.m_pDescription = Vote.m_aDescription;
	Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
}

void CVoteMenu::ClearVotes(int ClientID)
{
	m_aPlayersVotes[ClientID].clear();

	// send vote options
	CNetMsg_Sv_VoteClearOptions ClearMsg;
	Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);
}

void CVoteMenu::AddMenuChangeVote(int ClientID, int Menu, const char *pDesc)
{
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%d", Menu);
	AddMenuVote(ClientID, aBuf, pDesc);
}

void CVoteMenu::RebuildMenu(int ClientID)
{
	int Menu = m_aPlayersMenu[ClientID];

	ClearVotes(ClientID);

	if (Menu == MENU_MAIN)
	{
		AddMenuVote(ClientID, "null", "------------ Server");
		AddMenuChangeVote(ClientID, MENU_INFO, "☞ Info");
		AddMenuVote(ClientID, "null", "------------ Your stats");

		AddMenuVote(ClientID, "null", "------------ Account menu");
		AddMenuChangeVote(ClientID, MENU_EQUIP, "☞ Armor");
		AddMenuChangeVote(ClientID, MENU_INVENTORY, "☞ Inventory");
		AddMenuChangeVote(ClientID, MENU_UPGRADE, "☞ Upgrade");
	}
}

void CVoteMenu::AddBack(int ClientID, int Menu)
{
	AddMenuVote(ClientID, "null", "");
	char aBuf[16];
	str_format(aBuf, sizeof(aBuf), "set%d", Menu);
	AddMenuVote(ClientID, aBuf, "- Back");
}
