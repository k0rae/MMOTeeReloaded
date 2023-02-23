#include <engine/server/databases/connection.h>

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
		"  Name VARCHAR(%d) COLLATE %s NOT NULL, "
		"  Points INT DEFAULT 0, "
		"  PRIMARY KEY (Name)"
		")",
		MAX_NAME_LENGTH, BinaryCollate());
}
