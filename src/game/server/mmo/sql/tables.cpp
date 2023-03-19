#include <engine/server/databases/connection.h>

#include <base/hash.h>

bool IDbConnection::CreateTablesMMO(char *pError, int ErrorSize)
{
	char aBuf[1024];

	FormatCreateAccounts(aBuf, sizeof(aBuf));
	if (Execute(aBuf, pError, ErrorSize))
		return true;

	return false;
}

void IDbConnection::FormatCreateAccounts(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS points ("
		"  id INTEGER NOT NULL PRIMARY KEY, "
		"  name VARCHAR(64) NOT NULL, "
		"  password VARCHAR(%s) NOT NULL, "
		"  level INTEGER DEFAULT 1, "
		"  exp INTEGER DEFAULT 0, "
		"  money INTEGER DEFAULT 0, "
		"  donate INTEGER DEFAULT 0"
		")",
		MD5_MAXSTRSIZE);
}
