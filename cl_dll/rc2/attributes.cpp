/***
*
*	Rocket Crowbar 2: the client's copy of every player's attributes, and the client side of the
*	game hooks player movement asks (pm_rc2.h).
*
*	The server sends a player's ten attribute points and ability flags in "SetAttr" when they
*	change (dlls/client.cpp); the client turns the points into values with the same formulas,
*	from its own copy of rc2.ini (RCCFG). Movement prediction reads them through the pm hooks,
*	and keeps the local player's ability timers here. Also here: a sequence remap for
*	scientist.mdl that nothing in client.dll calls.
*
*	File name ours (the original's object sits between ammohistory and battery).
*
****/

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "event_api.h"
#include "studio.h"
#include "rc2prefs.h"
#include "pm_rc2.h"

#include <string.h>

// --- the scientist.mdl sequence remap (orig .data 0x10136cf8; nothing references it) ---

struct seqpair_t // name ours
{
	int iFrom;
	int iTo;
};

struct seqremap_t // name ours
{
	const char* szModel; // matched as a substring of the studio header's name
	int iUnused;		 // 22, read by nothing
	int iNumBones;		 // the header must have this many bones
	seqpair_t* pPairs;
	int iNumPairs;
};

static seqpair_t s_ScientistSeqPairs[] = // orig .data 0x101369a8
	{
		{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}, {7, 7}, {8, 8}, {9, 9},
		{10, 10}, {11, 11}, {12, 12}, {13, 13}, {14, 14}, {15, 15}, {16, 16}, {17, 17}, {18, 18}, {19, 19},
		{20, 20}, {21, 21}, {22, 22}, {23, 22}, {24, 24}, {25, 25}, {26, 26}, {27, 27}, {28, 28}, {29, 29},
		{30, 30}, {31, 31}, {32, 33}, {33, 34}, {34, 35}, {35, 36}, {36, 37}, {37, 38}, {38, 39}, {39, 40},
		{40, 41}, {42, 43}, {43, 44}, {44, 45}, {45, 46}, {46, 47}, {47, 48}, {48, 49}, {49, 50}, {50, 0},
		{51, 51}, {52, 52}, {53, 53}, {54, 0}, {55, 54}, {56, 55}, {57, 56}, {58, 57}, {59, 58}, {60, 60},
		{61, 61}, {62, 62}, {63, 63}, {64, 64}, {66, 65}, {67, 0}, {68, 0}, {69, 66}, {70, 67}, {71, 68},
		{72, 0}, {73, 69}, {74, 70}, {75, 71}, {76, 72}, {78, 74}, {79, 75}, {80, 76}, {81, 77}, {82, 0},
		{83, 0}, {85, 79}, {86, 80}, {87, 81}, {88, 82}, {89, 83}, {90, 84}, {91, 85}, {92, 86}, {93, 87},
		{94, 88}, {95, 0}, {96, 89}, {97, 0}, {98, 91}, {99, 92}, {100, 93}, {102, 0}, {103, 0}, {104, 94},
		{105, 0}, {106, 95}, {107, 0}, {108, 96}, {109, 0}, {110, 97},
};

static seqremap_t s_SeqRemaps[] = // orig .data 0x10136cf8
	{
		{"scientist.mdl", 22, 47, s_ScientistSeqPairs, sizeof(s_ScientistSeqPairs) / sizeof(s_ScientistSeqPairs[0])},
};

// orig client 0x4f2c0. The sequence that maps to iSeq in phdr's remap (unchanged when none). name ours
int SeqRemap_ToOriginal(int iSeq, studiohdr_t* phdr)
{
	if (!phdr)
		return iSeq;

	unsigned int i;
	for (i = 0; i < sizeof(s_SeqRemaps) / sizeof(s_SeqRemaps[0]); i++)
	{
		if (strstr(phdr->name, s_SeqRemaps[i].szModel))
			break;
	}
	if (i >= sizeof(s_SeqRemaps) / sizeof(s_SeqRemaps[0]))
		return iSeq;

	if (phdr->numbones != s_SeqRemaps[i].iNumBones)
		return iSeq;

	for (int j = 0; j < s_SeqRemaps[i].iNumPairs; j++)
	{
		if (s_SeqRemaps[i].pPairs[j].iTo == iSeq)
			return s_SeqRemaps[i].pPairs[j].iFrom;
	}
	return iSeq;
}

// orig client 0x4f3a0. What iSeq maps to in phdr's remap (unchanged when none). name ours
int SeqRemap_FromOriginal(int iSeq, studiohdr_t* phdr)
{
	if (!phdr)
		return iSeq;

	unsigned int i;
	for (i = 0; i < sizeof(s_SeqRemaps) / sizeof(s_SeqRemaps[0]); i++)
	{
		if (strstr(phdr->name, s_SeqRemaps[i].szModel))
			break;
	}
	if (i >= sizeof(s_SeqRemaps) / sizeof(s_SeqRemaps[0]))
		return iSeq;

	if (phdr->numbones != s_SeqRemaps[i].iNumBones)
		return iSeq;

	for (int j = 0; j < s_SeqRemaps[i].iNumPairs; j++)
	{
		if (s_SeqRemaps[i].pPairs[j].iFrom == iSeq)
			return s_SeqRemaps[i].pPairs[j].iTo;
	}
	return iSeq;
}

// --- attributes ---

#define RC2_NUM_ATTRIBUTES 10 // name ours

static float g_flAttributes[MAX_PLAYERS + 1][RC2_NUM_ATTRIBUTES]; // orig .bss 0x101a76e0, by entity index. name ours
static float g_flLocalFloats[8];								  // orig .bss 0x101a7c08: the local player's Set/GetPlayerAttrFloat slots. name ours
static int g_iAbilities[MAX_PLAYERS + 1];						  // orig .bss 0x101a7c28: SetAttr's ability flags. name ours
static int g_iLocalTimers[8];									  // orig .bss 0x101a7cac: the local player's PM_SetPlayerTimer slots. name ours

static float Attr_PointsToValue(int i, int iPoints);

// orig client 0x4f480. Attribute iWhich of player iEnt (0 jump, 1 speed, ...); 1.0 for no player
float PM_GetAttributeScale(int iWhich, int iEnt)
{
	if (iEnt > 0 && iEnt <= MAX_PLAYERS)
		return g_flAttributes[iEnt][iWhich];
	return 1.0f;
}

// orig client 0x4f4c0
int PM_GetAbilityFlags(int iEnt)
{
	if (iEnt > 0 && iEnt <= MAX_PLAYERS)
		return g_iAbilities[iEnt];
	return 0;
}

// orig client 0x4f4f0. "SetAttr": byte player, a byte of points per attribute, long ability flags
int __MsgFunc_SetAttr(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const int iPlayer = READ_BYTE();
	if (iPlayer <= 0 || iPlayer > MAX_PLAYERS)
		return 0;

	for (int i = 0; i < RC2_NUM_ATTRIBUTES; i++)
		g_flAttributes[iPlayer][i] = Attr_PointsToValue(i, READ_BYTE());
	g_iAbilities[iPlayer] = READ_LONG();
	return 1;
}

// orig client 0x4f580. The client keeps only the local player's timers
void PM_SetPlayerTimer(int iEnt, int iSlot, int iMsec)
{
	if (0 != gEngfuncs.pEventAPI->EV_IsLocal(iEnt - 1))
		g_iLocalTimers[iSlot] = iMsec;
}

// orig client 0x4f5c0
int PM_GetPlayerTimer(int iEnt, int iSlot)
{
	if (0 == gEngfuncs.pEventAPI->EV_IsLocal(iEnt - 1))
		return 0;
	return g_iLocalTimers[iSlot];
}

// orig client 0x4f600. The client's SetPlayerAttrFloat (rc2.dll 0x95900); nothing in client.dll calls it
void SetPlayerAttrFloat(int iEnt, int iSlot, float flValue)
{
	if (0 != gEngfuncs.pEventAPI->EV_IsLocal(iEnt - 1))
		g_flLocalFloats[iSlot] = flValue;
}

// orig client 0x4f640. The client's GetPlayerAttrFloat (rc2.dll 0x95920); nothing in client.dll calls it
float GetPlayerAttrFloat(int iEnt, int iSlot)
{
	if (0 == gEngfuncs.pEventAPI->EV_IsLocal(iEnt - 1))
		return 0;
	return g_flLocalFloats[iSlot];
}

float HUD_GetEnergy(); // the energy HUD element's value, gHUD.m_Energy +0x18 (orig gHUD +0x204); another package's

// orig client 0x4f680. The local player's energy as the HUD shows it, 100 for anyone else
float PM_GetEnergy(int iEnt)
{
	if (0 != gEngfuncs.pEventAPI->EV_IsLocal(iEnt - 1))
		return HUD_GetEnergy();
	return 100;
}

// orig client 0x4f6c0. The client spends no energy: the server's "Energy" message says what is left
void PM_UseEnergy(int iEnt, float flAmount, float flRechargeDelay)
{
}

// orig client 0x4f6e0. The value iPoints buy of attribute i, as the server's Attr_PointsToValue
// (rc2.dll 0x1cf60), from the client's copy of rc2.ini. client.dll has a second copy at 0xd16d0
static float Attr_PointsToValue(int i, int iPoints)
{
	switch (i)
	{
	case 0: return iPoints / RC2Pref(RC2P_Attributes_JumpCost).f;
	case 1: return iPoints / RC2Pref(RC2P_Attributes_SpeedCost).f;
	case 2: return exp(iPoints * RC2Pref(RC2P_Attributes_AttackConstant).f) / RC2Pref(RC2P_Attributes_AttackScaler).f;
	case 3: return exp(-(iPoints * RC2Pref(RC2P_Attributes_DefendConstant).f)) * RC2Pref(RC2P_Attributes_DefendScaler).f;
	case 4: return iPoints / RC2Pref(RC2P_Attributes_HealthCost).f + RC2Pref(RC2P_Attributes_MinHealth).f;
	case 5: return iPoints / RC2Pref(RC2P_Attributes_ExtraHealthCost).f;
	case 6: return iPoints / RC2Pref(RC2P_Attributes_ArmorCost).f;
	case 7: return iPoints / RC2Pref(RC2P_Attributes_ExtraArmorCost).f;
	case 8: return iPoints / RC2Pref(RC2P_Attributes_EnergyRegenCost).f;
	case 9: return iPoints / RC2Pref(RC2P_Attributes_DamageReflectionCost).f;
	}
	return 0;
}

// TEMP STUB (c2): real one belongs in the energy HUD element's file (client.dll 0x9cdc0 on, CHudEnergy)
float HUD_GetEnergy()
{
	return 100;
}
