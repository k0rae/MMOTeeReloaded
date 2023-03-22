/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "teams.h"
#include "entities/character.h"
#include "gamecontroller.h"
#include "player.h"
#include "teehistorian.h"

#include <engine/shared/config.h>

#include <game/mapitems.h>

CGameTeams::CGameTeams(CGameContext *pGameContext) :
	m_pGameContext(pGameContext)
{
	Reset();
}

void CGameTeams::Reset()
{
	m_Core.Reset();
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_aTeeStarted[i] = false;
		m_aTeeFinished[i] = false;
		m_aLastChat[i] = 0;
		SendTeamsState(i);
	}

	for(int i = 0; i < NUM_TEAMS; ++i)
	{
		m_aTeamState[i] = TEAMSTATE_EMPTY;
		m_aTeamLocked[i] = false;
		m_apSaveTeamResult[i] = nullptr;
		m_aTeamSentStartWarning[i] = false;
		ResetRoundState(i);
	}
}

void CGameTeams::ResetRoundState(int Team)
{
	ResetInvited(Team);
	if(Team != TEAM_SUPER)
		ResetSwitchers(Team);

	m_aPractice[Team] = false;
	m_aTeamUnfinishableKillTick[Team] = -1;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_VotedForPractice = false;
			GameServer()->m_apPlayers[i]->m_SwapTargetsClientID = -1;
			m_aLastSwap[i] = 0;
		}
	}
}

void CGameTeams::ResetSwitchers(int Team)
{
	for(auto &Switcher : GameServer()->Switchers())
	{
		Switcher.m_aStatus[Team] = Switcher.m_Initial;
		Switcher.m_aEndTick[Team] = 0;
		Switcher.m_aType[Team] = TILE_SWITCHOPEN;
	}
}

void CGameTeams::OnCharacterStart(int ClientID)
{
	int Tick = Server()->Tick();
	CCharacter *pStartingChar = Character(ClientID);
	if(!pStartingChar)
		return;
	if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO && pStartingChar->m_DDRaceState == DDRACE_STARTED)
		return;
	if((g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO || m_Core.Team(ClientID) != TEAM_FLOCK) && pStartingChar->m_DDRaceState == DDRACE_FINISHED)
		return;
	if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO &&
		(m_Core.Team(ClientID) == TEAM_FLOCK || m_Core.Team(ClientID) == TEAM_SUPER))
	{
		m_aTeeStarted[ClientID] = true;
		pStartingChar->m_DDRaceState = DDRACE_STARTED;
		pStartingChar->m_StartTime = Tick;
		return;
	}
	bool Waiting = false;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_Core.Team(ClientID) != m_Core.Team(i))
			continue;
		CPlayer *pPlayer = GetPlayer(i);
		if(!pPlayer || !pPlayer->IsPlaying())
			continue;
		if(GetDDRaceState(pPlayer) != DDRACE_FINISHED)
			continue;

		Waiting = true;
		pStartingChar->m_DDRaceState = DDRACE_NONE;

		if(m_aLastChat[ClientID] + Server()->TickSpeed() + g_Config.m_SvChatDelay < Tick)
		{
			char aBuf[128];
			str_format(
				aBuf,
				sizeof(aBuf),
				"%s has finished and didn't go through start yet, wait for him or join another team.",
				Server()->ClientName(i));
			GameServer()->SendChatTarget(ClientID, aBuf);
			m_aLastChat[ClientID] = Tick;
		}
		if(m_aLastChat[i] + Server()->TickSpeed() + g_Config.m_SvChatDelay < Tick)
		{
			char aBuf[128];
			str_format(
				aBuf,
				sizeof(aBuf),
				"%s wants to start a new round, kill or walk to start.",
				Server()->ClientName(ClientID));
			GameServer()->SendChatTarget(i, aBuf);
			m_aLastChat[i] = Tick;
		}
	}

	if(!Waiting)
	{
		m_aTeeStarted[ClientID] = true;
	}

	if(m_aTeamState[m_Core.Team(ClientID)] < TEAMSTATE_STARTED && !Waiting)
	{
		ChangeTeamState(m_Core.Team(ClientID), TEAMSTATE_STARTED);
		m_aTeamSentStartWarning[m_Core.Team(ClientID)] = false;
		m_aTeamUnfinishableKillTick[m_Core.Team(ClientID)] = -1;

		int NumPlayers = Count(m_Core.Team(ClientID));

		char aBuf[512];
		str_format(
			aBuf,
			sizeof(aBuf),
			"Team %d started with %d player%s: ",
			m_Core.Team(ClientID),
			NumPlayers,
			NumPlayers == 1 ? "" : "s");

		bool First = true;

		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(m_Core.Team(ClientID) == m_Core.Team(i))
			{
				CPlayer *pPlayer = GetPlayer(i);
				// TODO: THE PROBLEM IS THAT THERE IS NO CHARACTER SO START TIME CAN'T BE SET!
				if(pPlayer && (pPlayer->IsPlaying() || TeamLocked(m_Core.Team(ClientID))))
				{
					SetDDRaceState(pPlayer, DDRACE_STARTED);
					SetStartTime(pPlayer, Tick);

					if(First)
						First = false;
					else
						str_append(aBuf, ", ", sizeof(aBuf));

					str_append(aBuf, GameServer()->Server()->ClientName(i), sizeof(aBuf));
				}
			}
		}

		if(g_Config.m_SvTeam < SV_TEAM_FORCED_SOLO && g_Config.m_SvMaxTeamSize != 2 && g_Config.m_SvPauseable)
		{
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				CPlayer *pPlayer = GetPlayer(i);
				if(m_Core.Team(ClientID) == m_Core.Team(i) && pPlayer && (pPlayer->IsPlaying() || TeamLocked(m_Core.Team(ClientID))))
				{
					GameServer()->SendChatTarget(i, aBuf);
				}
			}
		}
	}
}

void CGameTeams::OnCharacterFinish(int ClientID)
{
	if((m_Core.Team(ClientID) == TEAM_FLOCK && g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO) || m_Core.Team(ClientID) == TEAM_SUPER)
	{
		CPlayer *pPlayer = GetPlayer(ClientID);
		if(pPlayer && pPlayer->IsPlaying())
		{
			float Time = (float)(Server()->Tick() - GetStartTime(pPlayer)) / ((float)Server()->TickSpeed());
			if(Time < 0.000001f)
				return;
		}
	}
	else
	{
		if(m_aTeeStarted[ClientID])
		{
			m_aTeeFinished[ClientID] = true;
		}
		CheckTeamFinished(m_Core.Team(ClientID));
	}
}

void CGameTeams::Tick()
{
	int Now = Server()->Tick();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aTeamUnfinishableKillTick[i] == -1 || m_aTeamState[i] != TEAMSTATE_STARTED_UNFINISHABLE)
		{
			continue;
		}
		if(Now >= m_aTeamUnfinishableKillTick[i])
		{
			if(m_aPractice[i])
			{
				m_aTeamUnfinishableKillTick[i] = -1;
				continue;
			}
			GameServer()->SendChatTeam(i, "Your team was killed because it couldn't finish anymore and hasn't entered /practice mode");
			KillTeam(i, -1);
		}
	}

	int Frequency = Server()->TickSpeed() * 60;
	int Remainder = Server()->TickSpeed() * 30;
	uint64_t TeamHasWantedStartTime = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter *pChar = GameServer()->m_apPlayers[i] ? GameServer()->m_apPlayers[i]->GetCharacter() : nullptr;
		int Team = m_Core.Team(i);
		if(!pChar || m_aTeamState[Team] != TEAMSTATE_STARTED || m_aTeeStarted[i] || m_aPractice[m_Core.Team(i)])
		{
			continue;
		}
		if((Now - pChar->m_StartTime) % Frequency == Remainder)
		{
			TeamHasWantedStartTime |= ((uint64_t)1) << m_Core.Team(i);
		}
	}
	TeamHasWantedStartTime &= ~(uint64_t)1;
	if(!TeamHasWantedStartTime)
	{
		return;
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(((TeamHasWantedStartTime >> i) & 1) == 0)
		{
			continue;
		}
		if(Count(i) <= 1)
		{
			continue;
		}
		int NumPlayersNotStarted = 0;
		char aPlayerNames[256];
		aPlayerNames[0] = 0;
		for(int j = 0; j < MAX_CLIENTS; j++)
		{
			if(m_Core.Team(j) == i && !m_aTeeStarted[j])
			{
				if(aPlayerNames[0])
				{
					str_append(aPlayerNames, ", ", sizeof(aPlayerNames));
				}
				str_append(aPlayerNames, Server()->ClientName(j), sizeof(aPlayerNames));
				NumPlayersNotStarted += 1;
			}
		}
		if(!aPlayerNames[0])
		{
			continue;
		}
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf),
			"Your team has %d %s not started yet, they need "
			"to touch the start before this team can finish: %s",
			NumPlayersNotStarted,
			NumPlayersNotStarted == 1 ? "player that has" : "players that have",
			aPlayerNames);
		GameServer()->SendChatTeam(i, aBuf);
	}
}

void CGameTeams::CheckTeamFinished(int Team)
{
	if(TeamFinished(Team))
	{
		CPlayer *apTeamPlayers[MAX_CLIENTS];
		unsigned int PlayersCount = 0;

		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(Team == m_Core.Team(i))
			{
				CPlayer *pPlayer = GetPlayer(i);
				if(pPlayer && pPlayer->IsPlaying())
				{
					m_aTeeStarted[i] = false;
					m_aTeeFinished[i] = false;

					apTeamPlayers[PlayersCount++] = pPlayer;
				}
			}
		}

		if(PlayersCount > 0)
		{
			float Time = (float)(Server()->Tick() - GetStartTime(apTeamPlayers[0])) / ((float)Server()->TickSpeed());
			if(Time < 0.000001f)
			{
				return;
			}

			if(m_aPractice[Team])
			{
				ChangeTeamState(Team, TEAMSTATE_FINISHED);

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf),
					"Your team would've finished in: %d minute(s) %5.2f second(s). Since you had practice mode enabled your rank doesn't count.",
					(int)Time / 60, Time - ((int)Time / 60 * 60));
				GameServer()->SendChatTeam(Team, aBuf);

				for(unsigned int i = 0; i < PlayersCount; ++i)
				{
					SetDDRaceState(apTeamPlayers[i], DDRACE_FINISHED);
				}

				return;
			}
		}
	}
}

const char *CGameTeams::SetCharacterTeam(int ClientID, int Team)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return "Invalid client ID";
	if(Team < 0 || Team >= MAX_CLIENTS + 1)
		return "Invalid team number";
	if(Team != TEAM_SUPER && m_aTeamState[Team] > TEAMSTATE_OPEN)
		return "This team started already";
	if(m_Core.Team(ClientID) == Team)
		return "You are in this team already";
	if(!Character(ClientID))
		return "Your character is not valid";
	if(Team == TEAM_SUPER && !Character(ClientID)->IsSuper())
		return "You can't join super team if you don't have super rights";
	if(Team != TEAM_SUPER && Character(ClientID)->m_DDRaceState != DDRACE_NONE)
		return "You have started racing already";
	// No cheating through noob filter with practice and then leaving team
	if(m_aPractice[m_Core.Team(ClientID)])
		return "You have used practice mode already";

	// you can not join a team which is currently in the process of saving,
	// because the save-process can fail and then the team is reset into the game
	if(Team != TEAM_SUPER && GetSaving(Team))
		return "Your team is currently saving";
	if(m_Core.Team(ClientID) != TEAM_SUPER && GetSaving(m_Core.Team(ClientID)))
		return "This team is currently saving";

	SetForceCharacterTeam(ClientID, Team);
	return nullptr;
}

void CGameTeams::SetForceCharacterTeam(int ClientID, int Team)
{
	m_aTeeStarted[ClientID] = false;
	m_aTeeFinished[ClientID] = false;
	int OldTeam = m_Core.Team(ClientID);

	if(Team != OldTeam && (OldTeam != TEAM_FLOCK || g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO) && OldTeam != TEAM_SUPER && m_aTeamState[OldTeam] != TEAMSTATE_EMPTY)
	{
		bool NoElseInOldTeam = Count(OldTeam) <= 1;
		if(NoElseInOldTeam)
		{
			m_aTeamState[OldTeam] = TEAMSTATE_EMPTY;

			// unlock team when last player leaves
			SetTeamLock(OldTeam, false);
			ResetRoundState(OldTeam);
			// do not reset SaveTeamResult, because it should be logged into teehistorian even if the team leaves
		}
	}

	m_Core.Team(ClientID, Team);

	if(OldTeam != Team)
	{
		for(int LoopClientID = 0; LoopClientID < MAX_CLIENTS; ++LoopClientID)
			if(GetPlayer(LoopClientID))
				SendTeamsState(LoopClientID);

		if(GetPlayer(ClientID))
		{
			GetPlayer(ClientID)->m_VotedForPractice = false;
			GetPlayer(ClientID)->m_SwapTargetsClientID = -1;
		}
		m_pGameContext->m_World.RemoveEntitiesFromPlayer(ClientID);
	}

	if(Team != TEAM_SUPER && (m_aTeamState[Team] == TEAMSTATE_EMPTY || m_aTeamLocked[Team]))
	{
		if(!m_aTeamLocked[Team])
			ChangeTeamState(Team, TEAMSTATE_OPEN);

		ResetSwitchers(Team);
	}
}

int CGameTeams::Count(int Team) const
{
	if(Team == TEAM_SUPER)
		return -1;

	int Count = 0;

	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(m_Core.Team(i) == Team)
			Count++;

	return Count;
}

void CGameTeams::ChangeTeamState(int Team, int State)
{
	m_aTeamState[Team] = State;
}

void CGameTeams::KillTeam(int Team, int NewStrongID, int ExceptID)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_VotedForPractice = false;
			if(i != ExceptID)
			{
				GameServer()->m_apPlayers[i]->KillCharacter(WEAPON_SELF);
				if(NewStrongID != -1 && i != NewStrongID)
				{
					GameServer()->m_apPlayers[i]->Respawn(true); // spawn the rest of team with weak hook on the killer
				}
			}
		}
	}
}

bool CGameTeams::TeamFinished(int Team)
{
	if(m_aTeamState[Team] != TEAMSTATE_STARTED)
	{
		return false;
	}
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(m_Core.Team(i) == Team && !m_aTeeFinished[i])
			return false;
	return true;
}

CClientMask CGameTeams::TeamMask(int Team, int ExceptID, int Asker)
{
	if(Team == TEAM_SUPER)
	{
		if(ExceptID == -1)
			return CClientMask().set();
		return CClientMask().set().reset(ExceptID);
	}

	CClientMask Mask;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i == ExceptID)
			continue; // Explicitly excluded
		if(!GetPlayer(i))
			continue; // Player doesn't exist

		if(!(GetPlayer(i)->GetTeam() == TEAM_SPECTATORS || GetPlayer(i)->IsPaused()))
		{ // Not spectator
			if(i != Asker)
			{ // Actions of other players
				if(!Character(i))
					continue; // Player is currently dead
				if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_ONLY_TEAM)
				{
					if(m_Core.Team(i) != Team && m_Core.Team(i) != TEAM_SUPER)
						continue; // In different teams
				}
				else if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_OFF)
				{
					if(m_Core.GetSolo(Asker))
						continue; // When in solo part don't show others
					if(m_Core.GetSolo(i))
						continue; // When in solo part don't show others
					if(m_Core.Team(i) != Team && m_Core.Team(i) != TEAM_SUPER)
						continue; // In different teams
				}
			} // See everything of yourself
		}
		else if(GetPlayer(i)->m_SpectatorID != SPEC_FREEVIEW)
		{ // Spectating specific player
			if(GetPlayer(i)->m_SpectatorID != Asker)
			{ // Actions of other players
				if(!Character(GetPlayer(i)->m_SpectatorID))
					continue; // Player is currently dead
				if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_ONLY_TEAM)
				{
					if(m_Core.Team(GetPlayer(i)->m_SpectatorID) != Team && m_Core.Team(GetPlayer(i)->m_SpectatorID) != TEAM_SUPER)
						continue; // In different teams
				}
				else if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_OFF)
				{
					if(m_Core.GetSolo(Asker))
						continue; // When in solo part don't show others
					if(m_Core.GetSolo(GetPlayer(i)->m_SpectatorID))
						continue; // When in solo part don't show others
					if(m_Core.Team(GetPlayer(i)->m_SpectatorID) != Team && m_Core.Team(GetPlayer(i)->m_SpectatorID) != TEAM_SUPER)
						continue; // In different teams
				}
			} // See everything of player you're spectating
		}
		else
		{ // Freeview
			if(GetPlayer(i)->m_SpecTeam)
			{ // Show only players in own team when spectating
				if(m_Core.Team(i) != Team && m_Core.Team(i) != TEAM_SUPER)
					continue; // in different teams
			}
		}

		Mask.set(i);
	}
	return Mask;
}

void CGameTeams::SendTeamsState(int ClientID)
{
	if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
		return;

	if(!m_pGameContext->m_apPlayers[ClientID])
		return;

	CMsgPacker Msg(NETMSGTYPE_SV_TEAMSSTATE);
	CMsgPacker MsgLegacy(NETMSGTYPE_SV_TEAMSSTATELEGACY);

	for(unsigned i = 0; i < MAX_CLIENTS; i++)
	{
		Msg.AddInt(m_Core.Team(i));
		MsgLegacy.AddInt(m_Core.Team(i));
	}

	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
	int ClientVersion = m_pGameContext->m_apPlayers[ClientID]->GetClientVersion();
	if(!Server()->IsSixup(ClientID) && VERSION_DDRACE < ClientVersion && ClientVersion < VERSION_DDNET_MSG_LEGACY)
	{
		Server()->SendMsg(&MsgLegacy, MSGFLAG_VITAL, ClientID);
	}
}

int CGameTeams::GetDDRaceState(CPlayer *Player)
{
	if(!Player)
		return DDRACE_NONE;

	CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		return pChar->m_DDRaceState;
	return DDRACE_NONE;
}

void CGameTeams::SetDDRaceState(CPlayer *Player, int DDRaceState)
{
	if(!Player)
		return;

	CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		pChar->m_DDRaceState = DDRaceState;
}

int CGameTeams::GetStartTime(CPlayer *Player)
{
	if(!Player)
		return 0;

	CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		return pChar->m_StartTime;
	return 0;
}

void CGameTeams::SetStartTime(CPlayer *Player, int StartTime)
{
	if(!Player)
		return;

	CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		pChar->m_StartTime = StartTime;
}

void CGameTeams::SetLastTimeCp(CPlayer *Player, int LastTimeCp)
{
	if(!Player)
		return;

	CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		pChar->m_LastTimeCp = LastTimeCp;
}

float *CGameTeams::GetCurrentTimeCp(CPlayer *Player)
{
	if(!Player)
		return NULL;

	CCharacter *pChar = Player->GetCharacter();
	if(pChar)
		return pChar->m_aCurrentTimeCp;
	return NULL;
}

void CGameTeams::OnTeamFinish(CPlayer **Players, unsigned int Size, float Time, const char *pTimestamp)
{
	// Nothing here :P
}

void CGameTeams::OnFinish(CPlayer *Player, float Time, const char *pTimestamp)
{
	// Nothing here :P
}

void CGameTeams::RequestTeamSwap(CPlayer *pPlayer, CPlayer *pTargetPlayer, int Team)
{
	if(!pPlayer || !pTargetPlayer)
		return;

	char aBuf[512];
	if(pPlayer->m_SwapTargetsClientID == pTargetPlayer->GetCID())
	{
		str_format(aBuf, sizeof(aBuf),
			"You have already requested to swap with %s.", Server()->ClientName(pTargetPlayer->GetCID()));

		GameServer()->SendChatTarget(pPlayer->GetCID(), aBuf);
		return;
	}

	// Notification for the swap initiator
	str_format(aBuf, sizeof(aBuf),
		"You have requested to swap with %s.",
		Server()->ClientName(pTargetPlayer->GetCID()));
	GameServer()->SendChatTarget(pPlayer->GetCID(), aBuf);

	// Notification to the target swap player
	str_format(aBuf, sizeof(aBuf),
		"%s has requested to swap with you. To complete the swap process please wait %d seconds and then type /swap %s.",
		Server()->ClientName(pPlayer->GetCID()), g_Config.m_SvSaveSwapGamesDelay, Server()->ClientName(pPlayer->GetCID()));
	GameServer()->SendChatTarget(pTargetPlayer->GetCID(), aBuf);

	// Notification for the remaining team
	str_format(aBuf, sizeof(aBuf),
		"%s has requested to swap with %s.",
		Server()->ClientName(pPlayer->GetCID()), Server()->ClientName(pTargetPlayer->GetCID()));
	// Do not send the team notification for team 0
	if(Team != 0)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_Core.Team(i) == Team && i != pTargetPlayer->GetCID() && i != pPlayer->GetCID())
			{
				GameServer()->SendChatTarget(i, aBuf);
			}
		}
	}

	pPlayer->m_SwapTargetsClientID = pTargetPlayer->GetCID();
	m_aLastSwap[pPlayer->GetCID()] = Server()->Tick();
}

void CGameTeams::SwapTeamCharacters(CPlayer *pPrimaryPlayer, CPlayer *pTargetPlayer, int Team)
{
	if(!pPrimaryPlayer || !pTargetPlayer)
		return;

	char aBuf[128];

	int Since = (Server()->Tick() - m_aLastSwap[pTargetPlayer->GetCID()]) / Server()->TickSpeed();
	if(Since < g_Config.m_SvSaveSwapGamesDelay)
	{
		str_format(aBuf, sizeof(aBuf),
			"You have to wait %d seconds until you can swap.",
			g_Config.m_SvSaveSwapGamesDelay - Since);

		GameServer()->SendChatTarget(pPrimaryPlayer->GetCID(), aBuf);

		return;
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_SwapTargetsClientID = -1;
		}
	}

	int TimeoutAfterDelay = g_Config.m_SvSaveSwapGamesDelay + g_Config.m_SvSwapTimeout;
	if(Since >= TimeoutAfterDelay)
	{
		str_format(aBuf, sizeof(aBuf),
			"Your swap request timed out %d seconds ago. Use /swap again to re-initiate it.",
			Since - g_Config.m_SvSwapTimeout);

		GameServer()->SendChatTarget(pPrimaryPlayer->GetCID(), aBuf);

		return;
	}
}

void CGameTeams::ProcessSaveTeam()
{

}

void CGameTeams::OnCharacterSpawn(int ClientID)
{
	m_Core.SetSolo(ClientID, false);
	int Team = m_Core.Team(ClientID);

	if(GetSaving(Team))
		return;

	if(m_Core.Team(ClientID) >= TEAM_SUPER || !m_aTeamLocked[Team])
	{
		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO)
			SetForceCharacterTeam(ClientID, TEAM_FLOCK);
		else
			SetForceCharacterTeam(ClientID, ClientID); // initialize team
		CheckTeamFinished(Team);
	}
}

void CGameTeams::OnCharacterDeath(int ClientID, int Weapon)
{
	m_Core.SetSolo(ClientID, false);

	int Team = m_Core.Team(ClientID);
	if(GetSaving(Team))
		return;
	bool Locked = TeamLocked(Team) && Weapon != WEAPON_GAME;

	if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO && Team != TEAM_SUPER)
	{
		ChangeTeamState(Team, CGameTeams::TEAMSTATE_OPEN);
		ResetRoundState(Team);
	}
	else if(Locked)
	{
		SetForceCharacterTeam(ClientID, Team);

		if(GetTeamState(Team) != TEAMSTATE_OPEN)
		{
			ChangeTeamState(Team, CGameTeams::TEAMSTATE_OPEN);

			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "Everyone in your locked team was killed because '%s' %s.", Server()->ClientName(ClientID), Weapon == WEAPON_SELF ? "killed" : "died");

			m_aPractice[Team] = false;

			KillTeam(Team, Weapon == WEAPON_SELF ? ClientID : -1, ClientID);
			if(Count(Team) > 1)
			{
				GameServer()->SendChatTeam(Team, aBuf);
			}
		}
	}
	else
	{
		if(m_aTeamState[m_Core.Team(ClientID)] == CGameTeams::TEAMSTATE_STARTED && !m_aTeeStarted[ClientID])
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "This team cannot finish anymore because '%s' left the team before hitting the start", Server()->ClientName(ClientID));
			GameServer()->SendChatTeam(Team, aBuf);
			GameServer()->SendChatTeam(Team, "Enter /practice mode or restart to avoid the entire team being killed in 60 seconds");

			m_aTeamUnfinishableKillTick[Team] = Server()->Tick() + 60 * Server()->TickSpeed();
			ChangeTeamState(Team, CGameTeams::TEAMSTATE_STARTED_UNFINISHABLE);
		}
		SetForceCharacterTeam(ClientID, TEAM_FLOCK);
		CheckTeamFinished(Team);
	}
}

void CGameTeams::SetTeamLock(int Team, bool Lock)
{
	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
		m_aTeamLocked[Team] = Lock;
}

void CGameTeams::ResetInvited(int Team)
{
	m_aInvited[Team].reset();
}

void CGameTeams::SetClientInvited(int Team, int ClientID, bool Invited)
{
	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
	{
		if(Invited)
			m_aInvited[Team].set(ClientID);
		else
			m_aInvited[Team].reset(ClientID);
	}
}

void CGameTeams::KillSavedTeam(int ClientID, int Team)
{
	KillTeam(Team, -1);
}

void CGameTeams::ResetSavedTeam(int ClientID, int Team)
{
	if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
	{
		ChangeTeamState(Team, CGameTeams::TEAMSTATE_OPEN);
		ResetRoundState(Team);
	}
	else
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_Core.Team(i) == Team && GameServer()->m_apPlayers[i])
			{
				SetForceCharacterTeam(i, TEAM_FLOCK);
			}
		}
	}
}

int CGameTeams::GetFirstEmptyTeam() const
{
	for(int i = 1; i < MAX_CLIENTS; i++)
		if(m_aTeamState[i] == TEAMSTATE_EMPTY)
			return i;
	return -1;
}
