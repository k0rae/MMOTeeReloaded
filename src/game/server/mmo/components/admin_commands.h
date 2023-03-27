#ifndef GAME_SERVER_MMO_COMPONENTS_ADMIN_COMMANDS_H
#define GAME_SERVER_MMO_COMPONENTS_ADMIN_COMMANDS_H

#include <game/server/mmo/component.h>
#include <engine/console.h>

class CAdminCommands : public CServerComponent
{
	static void ConExecuteSQL(IConsole::IResult *pResult, void *pUserData);
	static void ConExecuteSQLGet(IConsole::IResult *pResult, void *pUserData);
	static void ConGiveItem(IConsole::IResult *pResult, void *pUserData);
	static void ConSetLevel(IConsole::IResult *pResult, void *pUserData);

public:
	virtual void OnConsoleInit() override;
};

#endif // GAME_SERVER_MMO_COMPONENTS_ADMIN_COMMANDS_H
