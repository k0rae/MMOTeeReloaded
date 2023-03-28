#ifndef GAME_SERVER_MMO_COMMON_BOX_H
#define GAME_SERVER_MMO_COMMON_BOX_H

#include <vector>

class CMMOCore;

struct SBoxEntry
{
	int m_ID;
	int m_Count;
	int m_Rand;
};

class CBox
{
	CMMOCore *m_pMMOCore;
	std::vector<SBoxEntry> m_vItems;
	std::vector<SBoxEntry> m_vRareItems;

public:
	void Init(CMMOCore *pCore);

	void AddItem(int ItemID, int Count = 1);
	void AddRareItem(int ItemID, int Rand, int Count = 1);

	void Open(int ClientID, int Count);
};

#endif // GAME_SERVER_MMO_COMMON_BOX_H
