/***
*
*	Rocket Crowbar 2 (client): the client's copy of rc2.ini (dlls/rc2prefs.h). client.dll has
*	the server's key table (client .data 0x13e6e0, the server's 0x14e568 byte for byte) and its
*	value array (client .bss 0x210aa8), and keeps the NETWORK_SYNC keys the server sends in
*	RCCFG, so its prediction of pm_shared and the weapons reads the server's values. client.dll
*	0xbd490-0xbea0d, after the pullers. File name and every name here are ours.
*
*	ParseValue, FormatValue, ResetToDefaults and ValuesEqual are the server's (dlls/rc2prefs.cpp,
*	rc2.dll 0xb06a0-0xb1180) instruction for instruction; they are static there in a server-only
*	file, so they are repeated here. The merge can move them to a file both libraries build.
*	The table is built through this file because cl_dll/CMakeLists.txt isn't a work package's
*	to edit.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "rc2prefs.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>


// client .bss 0x210aa8: one 8-byte slot per key of g_rc2Prefs
rc2prefvalue_t g_rc2PrefValues[RC2P_COUNT];

// The clamps a key's flags ask for (as dlls/rc2prefs.cpp's).
template <typename T>
static void ApplyFlags(unsigned int iFlags, T& value)
{
	if ((iFlags & RC2PF_POS_ONLY) != 0 && value < 0)
		value = 0;
	if ((iFlags & RC2PF_THREE_DIGITS) != 0)
		value = (value < 0) ? 0 : ((value > 999) ? 999 : value);
	if ((iFlags & RC2PF_ZERO_TO_ONE) != 0)
		value = (value < 0) ? 0 : ((value > 1) ? 1 : value);
	if ((iFlags & RC2PF_NEGATIVE_ONE_TO_ONE) != 0)
		value = (value < -1) ? -1 : ((value > 1) ? 1 : value);
	if ((iFlags & RC2PF_FOUR_DIGITS) != 0)
		value = (value < 0) ? 0 : ((value > 9999) ? 9999 : value);
	if ((iFlags & RC2PF_GREATER_THEN_ONE) != 0 && value < 1)
		value = 1;
	if ((iFlags & RC2PF_NOT_ZERO) != 0 && value == 0)
		value = 1;
}

static void RC2Prefs_ParseValue(int iIndex, const char* pszText, rc2prefvalue_t* pValue) // orig client 0xbd650
{
	const unsigned int iFlags = g_rc2Prefs[iIndex].iFlags;

	switch (g_rc2Prefs[iIndex].iType)
	{
	case RC2PT_FLOAT:
		pValue->f = (float)atof(pszText);
		ApplyFlags(iFlags, pValue->f);
		break;
	case RC2PT_SMALLFLOAT:
	{
		double flValue = atof(pszText);
		if (flValue < 0.0)
			flValue = 0.0;
		else if (flValue > 655.35)
			flValue = 655.35;
		pValue->f = (float)flValue;
		ApplyFlags(iFlags, pValue->f);
		break;
	}
	case RC2PT_DOUBLE:
		pValue->d = atof(pszText);
		ApplyFlags(iFlags, pValue->d);
		break;
	case RC2PT_INT:
		pValue->i = atoi(pszText);
		ApplyFlags(iFlags, pValue->i);
		break;
	case RC2PT_SHORT:
	{
		int iValue = atoi(pszText);
		pValue->s = (short)((iValue < -0x7fff) ? -0x7fff : ((iValue > 0x7fff) ? 0x7fff : iValue));
		ApplyFlags(iFlags, pValue->s);
		break;
	}
	case RC2PT_USHORT:
	{
		int iValue = atoi(pszText);
		pValue->s = (short)((iValue < 0) ? 0 : ((iValue > 0xffff) ? 0xffff : iValue));
		ApplyFlags(iFlags, pValue->s); // signed, as the original
		break;
	}
	case RC2PT_BYTE:
	{
		int iValue = atoi(pszText);
		pValue->uc = (unsigned char)((iValue < 0) ? 0 : ((iValue > 0xff) ? 0xff : iValue));
		ApplyFlags(iFlags, pValue->uc);
		break;
	}
	case RC2PT_BOOL:
		pValue->uc = (stricmp(pszText, "true") == 0 || stricmp(pszText, "yes") == 0 || atoi(pszText) != 0) ? 1 : 0;
		ApplyFlags(iFlags, pValue->uc);
		break;
	}
}

// Unused in the client, as in the original.
[[maybe_unused]] static void RC2Prefs_FormatValue(int iIndex, const rc2prefvalue_t* pValue, char* pszOut) // orig client 0xbe250 (not in files-client.tsv: Ghidra missed it)
{
	switch (g_rc2Prefs[iIndex].iType)
	{
	case RC2PT_FLOAT:
	case RC2PT_SMALLFLOAT:
		sprintf(pszOut, "%f", (double)pValue->f);
		break;
	case RC2PT_DOUBLE:
		sprintf(pszOut, "%f", pValue->d);
		break;
	case RC2PT_INT:
		sprintf(pszOut, "%d", pValue->i);
		break;
	case RC2PT_SHORT:
		sprintf(pszOut, "%d", pValue->s);
		break;
	case RC2PT_USHORT:
		sprintf(pszOut, "%d", pValue->us);
		break;
	case RC2PT_BYTE:
		sprintf(pszOut, "%d", pValue->uc);
		break;
	case RC2PT_BOOL:
		sprintf(pszOut, "%s", pValue->c != 0 ? "true" : "false");
		break;
	}
}

// Every key back to its default: at the client's start (client 0x4fee0) and RCCFG 0x7fff.
void RC2Prefs_ResetToDefaults() // orig client 0xbe3c0
{
	for (int i = 0; i < RC2P_COUNT; i++)
		RC2Prefs_ParseValue(i, g_rc2Prefs[i].szDefault, &g_rc2PrefValues[i]);
}

static bool RC2Prefs_ValuesEqual(int iIndex, const rc2prefvalue_t* pA, const rc2prefvalue_t* pB) // orig client 0xbe400
{
	switch (g_rc2Prefs[iIndex].iType)
	{
	case RC2PT_FLOAT:
	case RC2PT_SMALLFLOAT:
		return pA->f == pB->f;
	case RC2PT_DOUBLE:
		return pA->d == pB->d;
	case RC2PT_INT:
		return pA->i == pB->i;
	case RC2PT_SHORT:
	case RC2PT_USHORT:
		return pA->s == pB->s;
	case RC2PT_BYTE:
		return pA->c == pB->c;
	case RC2PT_BOOL:
		return (pA->c != 0) == (pB->c != 0);
	}
	return false;
}

// One key's value from an RCCFG message, as the server writes it (RC2Prefs_WriteValue).
static void RC2Prefs_ReadValue(int iIndex) // orig client 0xbd490
{
	rc2prefvalue_t* pValue = &g_rc2PrefValues[iIndex];

	switch (g_rc2Prefs[iIndex].iType)
	{
	case RC2PT_FLOAT:
	{
		const int iBits = READ_LONG(); // the float's bits
		memcpy(&pValue->f, &iBits, sizeof(float));
		break;
	}
	case RC2PT_SMALLFLOAT:
		pValue->f = (unsigned short)READ_SHORT() * 0.01f;
		break;
	case RC2PT_DOUBLE:
	{
		// original bug: the first long (the low half, which the server sends twice) is dropped
		// and the second becomes the low half; the high half is whatever was on the stack.
		// VERIFY: zero here. No key is a DOUBLE, so it never runs.
		READ_LONG();
		int iHalves[2] = {READ_LONG(), 0};
		memcpy(&pValue->d, iHalves, sizeof(double));
		break;
	}
	case RC2PT_INT:
		pValue->i = READ_LONG();
		break;
	case RC2PT_SHORT:
	case RC2PT_USHORT:
		pValue->s = (short)READ_SHORT();
		break;
	case RC2PT_BYTE:
		pValue->c = (char)READ_BYTE();
		break;
	case RC2PT_BOOL:
		pValue->b = READ_BYTE() != 0;
		break;
	}
}

// The bytes a block of the NETWORK_SYNC keys takes (RC2Prefs_BuildSyncBlock): a bit per key,
// then the value of each that isn't its default.
static int RC2Prefs_GetSyncSize(int* piCount) // orig client 0xbe520
{
	int iCount = 0;
	int iBytes = 0;
	rc2prefvalue_t defaultValue;

	for (int i = 0; i < RC2P_COUNT; i++)
	{
		if ((g_rc2Prefs[i].iFlags & RC2PF_NETWORK_SYNC) == 0)
			continue;
		iCount++;
		RC2Prefs_ParseValue(i, g_rc2Prefs[i].szDefault, &defaultValue);
		if (RC2Prefs_ValuesEqual(i, &g_rc2PrefValues[i], &defaultValue))
			continue;
		switch (g_rc2Prefs[i].iType)
		{
		case RC2PT_FLOAT:
		case RC2PT_INT:
			iBytes += 4;
			break;
		case RC2PT_SMALLFLOAT:
		case RC2PT_SHORT:
		case RC2PT_USHORT:
			iBytes += 2;
			break;
		case RC2PT_DOUBLE:
			iBytes += 8;
			break;
		case RC2PT_BYTE:
		case RC2PT_BOOL:
			iBytes += 1;
			break;
		}
	}

	if (piCount)
		*piCount = iCount;
	return (int)(ceil(iCount * 0.125) + iBytes);
}

// A block of every NETWORK_SYNC key that isn't its default, for RCCFG 0x7ffe. Nothing in
// client.dll calls it; the server doesn't build one (it sends one key per message).
[[maybe_unused]] static void RC2Prefs_BuildSyncBlock(unsigned char** ppBlock, int* piSize) // orig client 0xbe640
{
	*ppBlock = NULL;
	*piSize = 0;

	int iCount = 0;
	const int iSize = RC2Prefs_GetSyncSize(&iCount);
	if (iSize <= 0 || iCount <= 0)
		return;

	const int iBitBytes = (int)ceil(iCount * 0.125);
	if (iBitBytes == iSize)
		return;

	unsigned char* pBlock = (unsigned char*)malloc(iSize);
	if (!pBlock)
		return;
	memset(pBlock, 0, iSize);

	unsigned char* p = pBlock + iBitBytes;
	int iBit = 0;
	rc2prefvalue_t defaultValue;
	for (int i = 0; i < RC2P_COUNT && p - pBlock < iSize; i++)
	{
		if ((g_rc2Prefs[i].iFlags & RC2PF_NETWORK_SYNC) == 0)
			continue;

		RC2Prefs_ParseValue(i, g_rc2Prefs[i].szDefault, &defaultValue);
		if (RC2Prefs_ValuesEqual(i, &g_rc2PrefValues[i], &defaultValue))
		{
			iBit++;
			continue;
		}

		pBlock[iBit / 8] |= 1 << (iBit % 8);
		iBit++;

		const rc2prefvalue_t* pValue = &g_rc2PrefValues[i];
		switch (g_rc2Prefs[i].iType)
		{
		case RC2PT_FLOAT:
		case RC2PT_INT:
			memcpy(p, &pValue->i, 4);
			p += 4;
			break;
		case RC2PT_SMALLFLOAT:
		{
			const short sValue = (short)(int)(pValue->f * 100.0);
			memcpy(p, &sValue, 2);
			p += 2;
			break;
		}
		case RC2PT_DOUBLE:
			memcpy(p, &pValue->d, 8);
			p += 8;
			break;
		case RC2PT_SHORT:
		case RC2PT_USHORT:
			memcpy(p, &pValue->s, 2);
			p += 2;
			break;
		case RC2PT_BYTE:
			*p++ = pValue->uc;
			break;
		case RC2PT_BOOL:
			*p++ = pValue->c != 0;
			break;
		}
	}

	*ppBlock = pBlock;
	*piSize = iSize;
}

// RCCFG 0x7ffe: the defaults, then the block's keys. 0 when the block is too short for its
// bits, or ends before the table does.
static int RC2Prefs_ReadSyncBlock(const unsigned char* pBlock, int iSize) // orig client 0xbe8a0
{
	RC2Prefs_ResetToDefaults();

	int iCount = 0;
	for (int i = 0; i < RC2P_COUNT; i++)
	{
		if ((g_rc2Prefs[i].iFlags & RC2PF_NETWORK_SYNC) != 0)
			iCount++;
	}

	const int iBitBytes = (int)ceil(iCount * 0.125);
	if (iCount <= 0 || iSize < iBitBytes)
		return 0;
	if (iSize == iBitBytes)
		return 1;

	const unsigned char* p = pBlock + iBitBytes;
	int iBit = 0;
	for (int i = 0; i < RC2P_COUNT; i++)
	{
		if (p - pBlock >= iSize)
			return 0;

		if ((g_rc2Prefs[i].iFlags & RC2PF_NETWORK_SYNC) == 0)
			continue;

		if ((pBlock[iBit / 8] & (1 << (iBit % 8))) == 0)
		{
			iBit++;
			continue;
		}
		iBit++;

		rc2prefvalue_t* pValue = &g_rc2PrefValues[i];
		switch (g_rc2Prefs[i].iType)
		{
		case RC2PT_FLOAT:
		case RC2PT_INT:
			memcpy(&pValue->i, p, 4);
			p += 4;
			break;
		case RC2PT_SMALLFLOAT:
		{
			unsigned short usValue;
			memcpy(&usValue, p, 2);
			p += 2;
			// original bug: times 100 again where it should divide (the block's writer multiplies)
			pValue->f = usValue * 100.0f;
			break;
		}
		case RC2PT_DOUBLE:
			memcpy(&pValue->d, p, 8);
			p += 8;
			break;
		case RC2PT_SHORT:
		case RC2PT_USHORT:
			memcpy(&pValue->s, p, 2);
			p += 2;
			break;
		case RC2PT_BYTE:
			pValue->c = (char)*p++;
			break;
		case RC2PT_BOOL:
			pValue->b = *p++ != 0;
			break;
		}
	}
	return 1;
}

// RCCFG (dlls/rc2prefs.cpp sends it): short 0x7fff all defaults, 0x7ffe a block of keys, or a
// key's index and its value.
int __MsgFunc_RCCFG(const char* pszName, int iSize, void* pbuf) // orig client 0xbd5c0 (not in files-client.tsv: Ghidra missed it)
{
	BEGIN_READ(pbuf, iSize);
	const int iIndex = READ_SHORT();

	if (iIndex == 0x7fff)
	{
		RC2Prefs_ResetToDefaults();
		return 1;
	}

	if (iIndex == 0x7ffe)
		return RC2Prefs_ReadSyncBlock((const unsigned char*)pbuf + 2, iSize - 2);

	if (iIndex < 0 || iIndex >= RC2P_COUNT)
		return 0;

	RC2Prefs_ReadValue(iIndex);
	return 1;
}
