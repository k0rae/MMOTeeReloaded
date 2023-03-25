#include "mmo_core.h"

#include "dummies/dummy_base.h"

#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <engine/external/xml/pugixml.hpp>

using namespace pugi;

class CGameWorld *CMMOCore::GameWorld() { return &m_pGameServer->m_World; }
class IServer *CMMOCore::Server() { return m_pGameServer->Server(); }

void CMMOCore::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;

	xml_document Document;

	// Load items from mmo/items.xml
	xml_parse_result ParseResult = Document.load_file("mmo/items.xml");

	if (!ParseResult)
	{
		dbg_msg("xml", "source file 'mmo/items.xml' parsed with errors!");
		dbg_msg("xml", "error: %s", ParseResult.description());

		dbg_break();
	}

	xml_node Root = Document.child("Items");

	for (xml_node Node : Root)
	{
		SInvItem Item;
		Item.m_ID = Node.attribute("ID").as_int(-1);
		Item.m_Type = Node.attribute("Type").as_int(-1);
		Item.m_Rarity = Node.attribute("Rarity").as_int(-1);
		str_copy(Item.m_aName, Node.attribute("Name").as_string("[ERROR ITEM]"));

		m_vItems.push_back(Item);
	}

	// Load shop items from mmo/shop.xml
	ParseResult = Document.load_file("mmo/shop.xml");

	if (!ParseResult)
	{
		dbg_msg("xml", "source file 'mmo/shop.xml' parsed with errors!");
		dbg_msg("xml", "error: %s", ParseResult.description());

		dbg_break();
	}

	Root = Document.child("Shop");

	for (xml_node Node : Root)
	{
		SShopEntry Entry;
		Entry.m_ID = Node.attribute("ID").as_int(-1);
		Entry.m_Cost = Node.attribute("Cost").as_int(-1);
		Entry.m_Level = Node.attribute("Level").as_int(-1);

		m_vShopItems.push_back(Entry);
	}

	// Load mobs from mmo/mobs.xml
	ParseResult = Document.load_file("mmo/mobs.xml");

	if (!ParseResult)
	{
		dbg_msg("xml", "source file 'mmo/mobs.xml' parsed with errors!");
		dbg_msg("xml", "error: %s", ParseResult.description());

		dbg_break();
	}

	Root = Document.child("Mobs");

	for (xml_node Node : Root)
	{
		xml_node TeeInfo = Node.child("TeeInfo");
		xml_node Stats = Node.child("Stats");
		xml_node Spawn = Node.child("Spawn");

		SBotData Data;
		Data.m_ID = Node.attribute("ID").as_int();
		Data.m_AIType = Node.attribute("AIType").as_int();
		str_copy(Data.m_aName, TeeInfo.attribute("Name").as_string("UNKNOWN BOT"));
		str_copy(Data.m_TeeInfo.m_aSkinName, TeeInfo.attribute("Skin").as_string());
		Data.m_TeeInfo.m_UseCustomColor = TeeInfo.attribute("UseCustomColors").as_int();
		Data.m_TeeInfo.m_ColorBody = TeeInfo.attribute("ColorBody").as_int();
		Data.m_TeeInfo.m_ColorFeet = TeeInfo.attribute("ColorFeet").as_int();
		Data.m_Level = Stats.attribute("Level").as_int();
		Data.m_HP = Stats.attribute("HP").as_int();
		Data.m_Armor = Stats.attribute("Armor").as_int();
		Data.m_Damage = Stats.attribute("Damage").as_int();
		str_copy(Data.m_aSpawnPointName, Spawn.empty() ? "" : Spawn.attribute("SpawnPoint").as_string());

		m_vBotDatas.push_back(Data);
	}
}

int CMMOCore::GetNextBotSnapID(int ClientID)
{
	int Prev = m_aBotSnapIDs[ClientID];
	m_aBotSnapIDs[ClientID]++;
	return ((Prev >= MAX_CLIENTS) ? -1 : Prev);
}

void CMMOCore::ClearBotSnapIDs()
{
	for (int &i : m_aBotSnapIDs)
		i = BOT_IDS_OFFSET;
}

void CMMOCore::CreateDummy(vec2 Pos, int DummyType, int AIType)
{
	CDummyBase *pNewDummy = new CDummyBase(GameWorld(), Pos, DummyType, AIType);

	pNewDummy->SetName("UNKNOWN BOT");
	pNewDummy->SetClan("MOB");
}

void CMMOCore::CreateDummy(vec2 Pos, SBotData Data)
{
	CDummyBase *pNewDummy = new CDummyBase(GameWorld(), Pos, Data.m_ID, Data.m_AIType);

	pNewDummy->SetName(Data.m_aName);
	pNewDummy->SetClan("MOB");
	pNewDummy->SetTeeInfo(Data.m_TeeInfo);
	pNewDummy->m_Level = Data.m_Level;
	pNewDummy->m_MaxHealth = Data.m_HP;
	pNewDummy->m_MaxArmor = Data.m_Armor;
	pNewDummy->m_Damage = Data.m_Damage;
}

void CMMOCore::OnMapBotPoint(vec2 Pos, const char *pPointName)
{
	SBotData *pBotData = 0x0;

	// Search for bot data
	char aBuf[256];
	for (SBotData &Data : m_vBotDatas)
	{
		str_format(aBuf, sizeof(aBuf), "Bot%s", Data.m_aSpawnPointName);
		if (!str_comp(aBuf, pPointName))
		{
			pBotData = &Data;
			break;
		}
	}

	// Check for bot data
	if (!pBotData)
	{
		dbg_msg("mmo_core", "unknown bot spawn point: %s. Pos: %f %f", pPointName, Pos.x / 32.f, Pos.y / 32.f);
		return;
	}

	// Create dummy
	CreateDummy(Pos, *pBotData);
}

int CMMOCore::GetExpForLevelUp(int Level)
{
	int Exp = 250;

	if(Level > 100) Exp = 700;
	if(Level > 200) Exp = 1000;
	if(Level > 300) Exp = 1500;
	if(Level > 400) Exp = 2500;
	if(Level > 500) Exp = 4000;
	if(Level > 600) Exp = 6000;
	if(Level > 700) Exp = 8000;
	if(Level > 1000) Exp = 12000;
	if(Level > 1100) Exp = 13000;
	if(Level > 1200) Exp = 14000;

	return Level * Exp;
}

int CMMOCore::GetExpForLevelUpWork(int WorkID, int Level)
{
	switch(WorkID)
	{
	case WORK_FARMER: return 10;
	case WORK_MINER: return 100;
	case WORK_MATERIAL: return 100;
	case WORK_FISHER: return 100;
	}

	return 99999999;
}

const char *CMMOCore::GetWorkName(int WorkID)
{
	switch(WorkID)
	{
	case WORK_FARMER: return "Farmer";
	case WORK_MINER: return "Miner";
	case WORK_FISHER: return "Fisher";
	case WORK_MATERIAL: return "Loader";
	}

	return "None";
}

void CMMOCore::GetProgressBar(char *pStr, int StrSize, char Filler, char Empty, int Num, int MaxNum)
{
	int c = StrSize - 1;
	float a = (float)Num / (float)MaxNum;
	int b = (float)c * a;

	pStr[StrSize - 1] = '\0';

	for (int i = 0; i < c; i++)
		pStr[i] = ((i + 1 <= b) ? Filler : Empty);
}

SInvItem *CMMOCore::GetItem(int ItemID)
{
	auto it = std::find_if(m_vItems.begin(), m_vItems.end(), [&](SInvItem Item) {
		return (Item.m_ID == ItemID);
	});

	if (it == m_vItems.end())
		return 0x0;
	return &*it;
}

const char *CMMOCore::GetItemName(int ItemID)
{
	SInvItem *pItem = GetItem(ItemID);
	return pItem ? pItem->m_aName : "[ERROR ITEM]";
}

int CMMOCore::GetItemType(int ItemID)
{
	SInvItem *pItem = GetItem(ItemID);
	return pItem ? pItem->m_Type : -1;
}

int CMMOCore::GetItemRarity(int ItemID)
{
	SInvItem *pItem = GetItem(ItemID);
	return pItem ? pItem->m_Rarity : -1;
}

const char *CMMOCore::GetQualityString(int Quality)
{
	switch(Quality)
	{
	case QUALITY_0: return "☆☆☆☆☆";
	case QUALITY_1: return "★☆☆☆☆";
	case QUALITY_2: return "★★☆☆☆";
	case QUALITY_3: return "★★★☆☆";
	case QUALITY_4: return "★★★★☆";
	case QUALITY_5: return "★★★★★";
	}

	return "[UNKNOWN QUALITY]";
}

const char *CMMOCore::GetRarityString(int Rarity)
{
	switch(Rarity)
	{
	case RARITY_COMMON: return "COMMON";
	case RARITY_UNCOMMON: return "UNCOMMON";
	case RARITY_RARE: return "RARE";
	case RARITY_EPIC: return "EPIC";
	case RARITY_LEGENDARY: return "LEGENDARY";
	}

	return "[UNKNOWN RARITY]";
}

void CMMOCore::GiveItem(int ClientID, int ItemID, int Count, int Quality, int Data)
{
	if (ClientID < 0 || ClientID >= MAX_PLAYERS)
		return;
	CPlayer *pPly = GameServer()->m_apPlayers[ClientID];
	if (!pPly || !pPly->m_LoggedIn)
		return;

	SInvItem Item;
	Item.m_Rarity = GetItemRarity(ItemID);
	Item.m_Type = GetItemType(ItemID);
	Item.m_ID = ItemID;
	Item.m_Count = Count;
	Item.m_Quality = Quality;
	Item.m_Data = Data;

	pPly->m_AccInv.AddItem(Item);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "+%s x%d", GetItemName(ItemID), Count);
	GameServer()->SendChatTarget(ClientID, aBuf);
}

void CMMOCore::UseItem(int ClientID, int ItemID, int Count)
{
	if (ClientID < 0 || ClientID >= MAX_PLAYERS)
		return;
	CPlayer *pPly = GameServer()->m_apPlayers[ClientID];
	if (!pPly || !pPly->m_LoggedIn)
		return;

	int Value = 0;
	SInvItem Item = pPly->m_AccInv.GetItem(ItemID);
	if (Item.m_Count == 0)
		return;
	Count = clamp(Count, 1, Item.m_Count);

	// Handle item use
	if (ItemID == ITEM_CARROT)
	{
		pPly->AddEXP(20 * Count);
		Value += 20 * Count;
	}
	else if (ItemID == ITEM_TOMATO)
	{
		pPly->AddEXP(30 * Count);
		Value += 30 * Count;
	}
	else if (ItemID == ITEM_POTATO)
	{
		pPly->AddEXP(50 * Count);
		Value += 50 * Count;
	}

	// Notify clients
	char aResultText[256] = {'\0'};
	if (ItemID >= ITEM_CARROT && ItemID <= ITEM_TOMATO)
		str_format(aResultText, sizeof(aResultText), "%s used %s x%d and got %d exp", Server()->ClientName(ClientID), GetItemName(ItemID), Count, Value);

	GameServer()->SendChatTarget(-1, aResultText);

	// Delete items from inventory
	pPly->m_AccInv.RemItem(ItemID, Count);
}

void CMMOCore::BuyItem(int ClientID, int ItemID)
{
	if (ClientID < 0 || ClientID >= MAX_PLAYERS)
		return;
	CPlayer *pPly = GameServer()->m_apPlayers[ClientID];
	if (!pPly || !pPly->m_LoggedIn)
		return;

	// Get entry
	auto it = std::find_if(m_vShopItems.begin(), m_vShopItems.end(), [&](const SShopEntry &e) {
		return (e.m_ID == ItemID);
	});

	if (it == m_vShopItems.end())
		return;

	// Check for level
	if (pPly->m_AccData.m_Level < it->m_Level)
	{
		GameServer()->SendChatTarget(ClientID, "You don't have needed level");
		return;
	}

	// Check for money
	if (pPly->m_AccData.m_Money < it->m_Cost)
	{
		GameServer()->SendChatTarget(ClientID, "You don't have needed money");
		return;
	}

	GiveItem(ClientID, ItemID);
	pPly->m_AccData.m_Money -= it->m_Cost;
}

const char *CMMOCore::GetUpgradeName(int UpgradeID)
{
	switch(UpgradeID)
	{
	case UPGRADE_POINTS: return "Upgrade points";
	case UPGRADE_SKILL_POINTS: return "Skill points";
	case UPGRADE_DAMAGE: return "Damage";
	case UPGRADE_FIRE_SPEED: return "Fire speed";
	case UPGRADE_HEALTH: return "Health";
	case UPGRADE_HEALTH_REGEN: return "Health regen";
	case UPGRADE_AMMO: return "Ammo";
	case UPGRADE_AMMO_REGEN: return "Ammo regen";
	case UPGRADE_SPRAY: return "Spray";
	case UPGRADE_MANA: return "Mana";
	}

	return "[UNKNOWN UPGRADE]";
}

int CMMOCore::GetUpgradeCost(int UpgradeID)
{
	switch(UpgradeID)
	{
	case UPGRADE_DAMAGE: return 2;
	case UPGRADE_SPRAY: return 30;
	case UPGRADE_AMMO: return 2;
	case UPGRADE_FIRE_SPEED: return 2;
	}

	return 1;
}

int CMMOCore::_GetMinerLevelID(int Level)
{
	int a = floor(Level / 50);
	int b = ITEM_COPPER;
	const int StartID = ITEM_COPPER - 1;

	switch(a)
	{
	case 0: b = ITEM_COPPER; break; // < 50 lvl
	case 1: b = ITEM_IRON; break; // < 100 lvl
	case 2: b = ITEM_GOLD; break; // < 150 lvl
	case 3: b = ITEM_DIAMOND; break; // < 200 lvl
	case 4: b = ITEM_OBSIDIAN; break; // < 250 lvl
	case 5: b = ITEM_MITHRIL_ORE; break; // < 350 lvl
	case 6: b = ITEM_ORIHALCIUM_ORE; break; // < 400 lvl
	case 7: b = ITEM_ADAMANTITE_ORE; break; // < 450 lvl
	case 8: b = ITEM_TITANIUM_ORE; break; // < 500 lvl
	case 9: b = ITEM_DRAGON_ORE; break; // < 600 lvl
	default: b = ITEM_ASTRALIUM_ORE; break; // All other
	}

	return b - StartID;
}

int CMMOCore::GetRandomMinerItemByLevel(int Level)
{
	int MaxRandom = _GetMinerLevelID(Level);
	switch(rand() % MaxRandom)
	{
	case 0: return ITEM_COPPER;
	case 1: return ITEM_IRON;
	case 2: return ITEM_GOLD;
	case 3: return ITEM_DIAMOND;
	case 4: return ITEM_OBSIDIAN;
	case 5: return ITEM_MITHRIL_ORE;
	case 6: return ITEM_ORIHALCIUM_ORE;
	case 7: return ITEM_ADAMANTITE_ORE;
	case 8: return ITEM_TITANIUM_ORE;
	case 9: return ITEM_DRAGON_ORE;
	default: return ITEM_ASTRALIUM_ORE;
	}
}

int CMMOCore::GetPlusDamage(int ClientID)
{
	CPlayer *pPly = GameServer()->m_apPlayers[ClientID];
	int PlusDamage = pPly->m_AccUp.m_Damage;

	return PlusDamage;
}
