#include <engine/server/databases/connection.h>

#include <base/hash.h>

#include <game/server/mmo/components/account_manager.h>

bool IDbConnection::CreateTablesMMO(char *pError, int ErrorSize)
{
	char aBuf[1024];

#define CREATE_TABLE(Name) \
	FormatCreate##Name(aBuf, sizeof(aBuf)); \
	if (Execute(aBuf, pError, ErrorSize)) \
		return true;

	CREATE_TABLE(Accounts)
	CREATE_TABLE(Inventories)
	CREATE_TABLE(Works)

#undef CREATE_TABLE

	return false;
}

void IDbConnection::FormatCreateAccounts(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS users ("
		"  id INTEGER NOT NULL PRIMARY KEY, "
		"  name VARCHAR(%d) NOT NULL, "
		"  password VARCHAR(%d) NOT NULL, "
		"  level INTEGER DEFAULT 1, "
		"  exp INTEGER DEFAULT 0, "
		"  money INTEGER DEFAULT 0, "
		"  donate INTEGER DEFAULT 0"
		")",
		MAX_LOGIN_LENGTH, MD5_MAXSTRSIZE);
}

void IDbConnection::FormatCreateInventories(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS u_inv ("
		"  id INTEGER NOT NULL PRIMARY KEY, "
		"  item_id INTEGER NOT NULL, "
		"  count INTEGER NOT NULL DEFAULT 0, "
		"  quality INTEGER NOT NULL DEFAULT 0, "
		"  data INTEGER NOT NULL DEFAULT 0"
		")");
}

void IDbConnection::FormatCreateWorks(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS u_works ("
		"  user_id INTEGER NOT NULL PRIMARY KEY, "
		"  work_id INTEGER NOT NULL, "
		"  exp INTEGER NOT NULL DEFAULT 0, "
		"  level INTEGER NOT NULL DEFAULT 1"
		")");
}
