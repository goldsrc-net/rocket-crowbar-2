/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// util.cpp
//
// implementation of class-less helper functions
//

#include <cstdio>
#include <cstdlib>

#include "hud.h"
#include "cl_util.h"
#include <string.h>

// RC2's util.cpp also has SDK 2.3's vector helpers (and Length and VectorScale); halflife-updated
// keeps one copy of each, in pm_shared/pm_math.cpp, which reads the same:
// orig client 0xcbaf0 Length, orig client 0xcbb30 VectorAngles, orig client 0xcbc50 VectorNormalize,
// orig client 0xcbcc0 VectorInverse, orig client 0xcbcf0 VectorScale, orig client 0xcbd30 VectorMA

HSPRITE LoadSprite(const char* pszName)
{
	int iRes;
	char sz[256]; 

	if (ScreenWidth > 2560 && ScreenHeight > 1600)
		iRes = 2560;
	else if (ScreenWidth >= 1280 && ScreenHeight > 720)
		iRes = 1280;
	else if (ScreenWidth >= 640)
		iRes = 640;
	else
		iRes = 320;

	sprintf(sz, pszName, iRes);

	return SPR_Load(sz);
}
