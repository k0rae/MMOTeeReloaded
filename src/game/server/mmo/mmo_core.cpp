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

	// Load items from mmo/items.xml
	xml_document Document;
	xml_parse_result ParseResult = Document.load_file("mmo/items.xml");

	if (!ParseResult)
	{
		dbg_msg("xml", "source file 'mmo/items.xml' parsed with errors!");
		dbg_msg("xml", "error: %s", ParseResult.description());

		dbg_break();
	}

	xml_node Items = Document.child("Items");

	for (xml_node Node : Items)
	{
		SInvItem Item;
		Item.m_ID = Node.attribute("ID").as_int(-1);
		Item.m_Type = Node.attribute("Type").as_int(-1);
		Item.m_Rarity = Node.attribute("Rarity").as_int(-1);
		str_copy(Item.m_aName, Node.attribute("Name").as_string("[ERROR ITEM]"));

		m_vItems.push_back(Item);
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

void CMMOCore::CreateDummy(vec2 Pos, int DummyType)
{
	CDummyBase *pNewDummy = new CDummyBase(GameWorld(), Pos, DummyType);

	// 0 - name, 1 - clan, 2 - skin
	static const char *s_aapDummyIndentsStrings[][3] = {
		{"ULTRA STANDER", "", "greyfox"}, // None bot
		{"Slime", "MOB", "ghost"} // Slime
	};

	// 0 - use custom color, 1 - color body, 2 - color feet
	static int s_aapDummyIndentsInts[][3] = {
		{1, 0, 0}, // None bot
		{1, 5504798, 5504798} // Slime
	};

	pNewDummy->SetName(s_aapDummyIndentsStrings[DummyType][0]);
	pNewDummy->SetClan(s_aapDummyIndentsStrings[DummyType][1]);
	pNewDummy->SetTeeInfo(CTeeInfo(
		s_aapDummyIndentsStrings[DummyType][2],
		s_aapDummyIndentsInts[DummyType][0],
		s_aapDummyIndentsInts[DummyType][1],
		s_aapDummyIndentsInts[DummyType][2]));
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
