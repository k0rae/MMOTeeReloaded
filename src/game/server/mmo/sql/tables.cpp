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
	CREATE_TABLE(Upgrades)
	CREATE_TABLE(Clans)

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
		"  donate INTEGER DEFAULT 0, "
		"  clan_id INTEGER DEFAULT 0, "
		"  create_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
		")",
		MAX_LOGIN_LENGTH, MD5_MAXSTRSIZE);
}

void IDbConnection::FormatCreateInventories(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS u_inv ("
		"  user_id INTEGER NOT NULL, "
		"  item_id INTEGER NOT NULL, "
		"  rarity INTEGER NOT NULL, "
		"  type INTEGER NOT NULL, "
		"  count INTEGER NOT NULL DEFAULT 0, "
		"  quality INTEGER NOT NULL DEFAULT 0, "
		"  data INTEGER NOT NULL DEFAULT 0"
		")");
}

void IDbConnection::FormatCreateWorks(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS u_works ("
		"  user_id INTEGER NOT NULL, "
		"  work_id INTEGER NOT NULL, "
		"  exp INTEGER NOT NULL DEFAULT 0, "
		"  level INTEGER NOT NULL DEFAULT 1"
		")");
}

void IDbConnection::FormatCreateUpgrades(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS u_upgr ("
		"  user_id INTEGER NOT NULL PRIMARY KEY, "
		"  upgrade_point INTEGER NOT NULL DEFAULT 0, "
		"  skill_point INTEGER NOT NULL DEFAULT 0, "
		"  damage INTEGER NOT NULL DEFAULT 0, "
		"  fire_speed INTEGER NOT NULL DEFAULT 0, "
		"  health INTEGER NOT NULL DEFAULT 0, "
		"  health_regen INTEGER NOT NULL DEFAULT 0, "
		"  ammo INTEGER NOT NULL DEFAULT 0, "
		"  ammo_regen INTEGER NOT NULL DEFAULT 0, "
		"  spray INTEGER NOT NULL DEFAULT 0, "
		"  mana INTEGER NOT NULL DEFAULT 0"
		")");
}

void IDbConnection::FormatCreateClans(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS clans ("
		"  id INTEGER NOT NULL PRIMARY KEY, "
		"  name VARCHAR(%d) NOT NULL, "
		"  leader_id INTEGER NOT NULL DEFAULT 0, "
		"  level INTEGER NOT NULL DEFAULT 1, "
		"  exp INTEGER NOT NULL DEFAULT 0, "
		"  max_num INTEGER NOT NULL DEFAULT 2, "
		"  money INTEGER NOT NULL DEFAULT 0, "
		"  money_add INTEGER NOT NULL DEFAULT 0, "
		"  exp_add INTEGER NOT NULL DEFAULT 0, "
		"  spawn_house INTEGER NOT NULL DEFAULT 0, "
		"  chair_house INTEGER NOT NULL DEFAULT 0, "
		"  house_id INTEGER NOT NULL DEFAULT -1, "
		"  create_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
		")",
		16);
}
