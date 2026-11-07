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
#include "hud.h"
#include "cl_util.h"

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"

#include "com_weapons.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "entity_types.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "pm_materials.h"

#include "eventscripts.h"
#include "ev_hldm.h"

#include "r_efx.h"
#include "event_api.h"
#include "event_args.h"
#include "in_defs.h"

#include <string.h>

#include "r_studioint.h"
#include "com_model.h"

extern engine_studio_api_t IEngineStudio;


#include "pm_shared.h"

void V_PunchAxis(int axis, float punch);
void VectorAngles(const float* forward, float* angles);

extern cvar_t* cl_lw;
extern cvar_t* r_decals;

static inline bool EV_HLDM_IsBSPModel(physent_t* pe)
{
	return pe != nullptr && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP);
}

// play a strike sound based on the texture that was hit by the attack traceline.  VecSrc/VecEnd are the
// original traceline endpoints used by the attacker, iBulletType is the type of bullet that hit the texture.
// returns volume of strike instrument (crowbar) to play
float EV_HLDM_PlayTextureSound(int idx, pmtrace_t* ptr, float* vecSrc, float* vecEnd, int iBulletType)
{
	// hit the world, try to play sound based on texture material type
	char chTextureType = CHAR_TEX_CONCRETE;
	cl_entity_t* cl_entity = NULL;
	float fvol;
	float fvolbar;
	const char* rgsz[4];
	int cnt;
	float fattn = ATTN_NORM;
	int entity;
	char* pTextureName;
	char texname[64];
	char szbuffer[64];

	entity = gEngfuncs.pEventAPI->EV_IndexFromTrace(ptr);

	// FIXME check if playtexture sounds movevar is set
	//

	chTextureType = 0;

	// Player
	if (entity == 0)
	{
		// get texture from entity or world (world is ent(0))
		pTextureName = (char*)gEngfuncs.pEventAPI->EV_TraceTexture(ptr->ent, vecSrc, vecEnd);

		if (pTextureName)
		{
			strcpy(texname, pTextureName);
			pTextureName = texname;

			// strip leading '-0' or '+0~' or '{' or '!'
			if (*pTextureName == '-' || *pTextureName == '+')
			{
				pTextureName += 2;
			}

			if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
			{
				pTextureName++;
			}

			// '}}'
			strcpy(szbuffer, pTextureName);
			szbuffer[CBTEXTURENAMEMAX - 1] = 0;

			// get texture type
			chTextureType = PM_FindTextureType(szbuffer);
		}
	}
	else
	{
		// JoshA: Look up the entity and find the EFLAG_FLESH_SOUND flag.
		// This broke at some point then TF:C added prediction.
		//
		// It used to use Classify of pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE
		// to determine what sound to play, but that's server side and isn't available on the client
		// and got lost in the translation to that.
		// Now the server will replicate that state via an eflag.
		cl_entity = gEngfuncs.GetEntityByIndex(entity);

		if (cl_entity && !!(cl_entity->curstate.eflags & EFLAG_FLESH_SOUND))
			chTextureType = CHAR_TEX_FLESH;
	}

	switch (chTextureType)
	{
	default:
	case CHAR_TEX_CONCRETE:
		fvol = 0.9;
		fvolbar = 0.6;
		rgsz[0] = "player/pl_step1.wav";
		rgsz[1] = "player/pl_step2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_METAL:
		fvol = 0.9;
		fvolbar = 0.3;
		rgsz[0] = "player/pl_metal1.wav";
		rgsz[1] = "player/pl_metal2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_DIRT:
		fvol = 0.9;
		fvolbar = 0.1;
		rgsz[0] = "player/pl_dirt1.wav";
		rgsz[1] = "player/pl_dirt2.wav";
		rgsz[2] = "player/pl_dirt3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_VENT:
		fvol = 0.5;
		fvolbar = 0.3;
		rgsz[0] = "player/pl_duct1.wav";
		rgsz[1] = "player/pl_duct1.wav";
		cnt = 2;
		break;
	case CHAR_TEX_GRATE:
		fvol = 0.9;
		fvolbar = 0.5;
		rgsz[0] = "player/pl_grate1.wav";
		rgsz[1] = "player/pl_grate4.wav";
		cnt = 2;
		break;
	case CHAR_TEX_TILE:
		fvol = 0.8;
		fvolbar = 0.2;
		rgsz[0] = "player/pl_tile1.wav";
		rgsz[1] = "player/pl_tile3.wav";
		rgsz[2] = "player/pl_tile2.wav";
		rgsz[3] = "player/pl_tile4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_SLOSH:
		fvol = 0.9;
		fvolbar = 0.0;
		rgsz[0] = "player/pl_slosh1.wav";
		rgsz[1] = "player/pl_slosh3.wav";
		rgsz[2] = "player/pl_slosh2.wav";
		rgsz[3] = "player/pl_slosh4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_WOOD:
		fvol = 0.9;
		fvolbar = 0.2;
		rgsz[0] = "debris/wood1.wav";
		rgsz[1] = "debris/wood2.wav";
		rgsz[2] = "debris/wood3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_GLASS:
	case CHAR_TEX_COMPUTER:
		fvol = 0.8;
		fvolbar = 0.2;
		rgsz[0] = "debris/glass1.wav";
		rgsz[1] = "debris/glass2.wav";
		rgsz[2] = "debris/glass3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_FLESH:
		if (iBulletType == BULLET_PLAYER_CROWBAR)
			return 0.0; // crowbar already makes this sound
		fvol = 1.0;
		fvolbar = 0.2;
		rgsz[0] = "weapons/bullet_hit1.wav";
		rgsz[1] = "weapons/bullet_hit2.wav";
		fattn = 1.0;
		cnt = 2;
		break;
	}

	// play material hit sound
	gEngfuncs.pEventAPI->EV_PlaySound(0, ptr->endpos, CHAN_STATIC, rgsz[gEngfuncs.pfnRandomLong(0, cnt - 1)], fvol, fattn, 0, 96 + gEngfuncs.pfnRandomLong(0, 0xf));
	return fvolbar;
}

char* EV_HLDM_DamageDecal(physent_t* pe)
{
	static char decalname[32];
	int idx;

	if (pe->rendermode == kRenderTransAlpha)
		return nullptr;

	if (pe->classnumber == 1)
	{
		idx = gEngfuncs.pfnRandomLong(0, 2);
		sprintf(decalname, "{break%i", idx + 1);
	}
	else if (pe->rendermode != kRenderNormal)
	{
		sprintf(decalname, "{bproof1");
	}
	else
	{
		idx = gEngfuncs.pfnRandomLong(0, 4);
		sprintf(decalname, "{shot%i", idx + 1);
	}
	return decalname;
}

void EV_HLDM_GunshotDecalTrace(pmtrace_t* pTrace, char* decalName)
{
	int iRand;

	gEngfuncs.pEfxAPI->R_BulletImpactParticles(pTrace->endpos);

	iRand = gEngfuncs.pfnRandomLong(0, 0x7FFF);
	if (iRand < (0x7fff / 2)) // not every bullet makes a sound.
	{
		switch (iRand % 5)
		{
		case 0:
			gEngfuncs.pEventAPI->EV_PlaySound(-1, pTrace->endpos, 0, "weapons/ric1.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			gEngfuncs.pEventAPI->EV_PlaySound(-1, pTrace->endpos, 0, "weapons/ric2.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 2:
			gEngfuncs.pEventAPI->EV_PlaySound(-1, pTrace->endpos, 0, "weapons/ric3.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			gEngfuncs.pEventAPI->EV_PlaySound(-1, pTrace->endpos, 0, "weapons/ric4.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 4:
			gEngfuncs.pEventAPI->EV_PlaySound(-1, pTrace->endpos, 0, "weapons/ric5.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
	}

	// Only decal brush models such as the world etc.
	if (decalName && '\0' != decalName[0] && r_decals->value > 0)
	{
		gEngfuncs.pEfxAPI->R_DecalShoot(
			gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(decalName)),
			gEngfuncs.pEventAPI->EV_IndexFromTrace(pTrace), 0, pTrace->endpos, 0);
	}
}

void EV_HLDM_DecalGunshot(pmtrace_t* pTrace, int iBulletType)
{
	physent_t* pe;

	pe = gEngfuncs.pEventAPI->EV_GetPhysent(pTrace->ent);

	if (EV_HLDM_IsBSPModel(pe))
	{
		switch (iBulletType)
		{
		case BULLET_PLAYER_9MM:
		case BULLET_MONSTER_9MM:
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_PLAYER_BUCKSHOT:
		case BULLET_PLAYER_357:
		default:
			// smoke and decal
			EV_HLDM_GunshotDecalTrace(pTrace, EV_HLDM_DamageDecal(pe));
			break;
		}
	}
}

void EV_HLDM_CheckTracer(int idx, float* vecSrc, float* end, float* forward, float* right, int iBulletType, int iTracerFreq, int* tracerCount)
{
	int i;
	bool player = idx >= 1 && idx <= gEngfuncs.GetMaxClients();

	if (iTracerFreq != 0 && ((*tracerCount)++ % iTracerFreq) == 0)
	{
		Vector vecTracerSrc;

		if (player)
		{
			Vector offset(0, 0, -4);

			// adjust tracer position for player
			for (i = 0; i < 3; i++)
			{
				vecTracerSrc[i] = vecSrc[i] + offset[i] + right[i] * 2 + forward[i] * 16;
			}
		}
		else
		{
			VectorCopy(vecSrc, vecTracerSrc);
		}

		switch (iBulletType)
		{
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_MONSTER_9MM:
		case BULLET_MONSTER_12MM:
		default:
			EV_CreateTracer(vecTracerSrc, end);
			break;
		}
	}
}

// RC2's rc2.ini table (the server sends the client its NETWORK_SYNC keys, RCCFG)
#include "rc2prefs.h"

#include <cmath>
#include <cstdlib>

// --- c1 events needs: what the event code uses from other client files (client.dll addresses;
// names ours unless SDK 2.3 has them). The ones not reconstructed in this worktree have weak TEMP
// STUBs at the bottom of this file.

// cdll_int.cpp (c2): (int)cl_detail->value, refreshed every HUD_Frame (orig .data 0x101a9598)
extern int g_iClientDetail;
// entity.cpp's HUD_AddEntity: one past the highest player or normal entity index seen (orig .bss
// 0x10206918)
extern int g_iHighestEntIndex;
// hud.cpp: the FOV while transformed, 0 when not zoomed (orig .bss 0x1020bb44)
extern float g_flTransformFOV;
// in_camera.cpp
// VERIFY: extern "C" linkage assumed (SDK in_camera.cpp exports it C-style); orig client 0xa9fd0
extern "C" int CL_IsThirdPerson();

// cl_dll/rc2/ev_helpers.cpp (c1)
int EV_PlayerForEntity(int idx);							  // orig client 0xdd40
float EV_WaterLevel(float* position, float minz, float maxz); // orig client 0xdd80
Vector UTIL_UnpackNormal(int iPacked);						  // orig client 0xde90

// cl_dll/rc2/clte*.cpp (c2): RC2's client temp entities
// VERIFY: 0x532f0 and 0x53780 argument names are ours, read off the call sites; c2 to confirm
// they match its CLTE definitions (0x532f0 is the 42-dword ring, not the cone)
void CLTE_KillByEntity(int iEnt, int iType); // orig client 0x52280
void CLTE_AddDiskRing(Vector vecOrigin, Vector vecX, Vector vecY, Vector vecZ, int iRing,
	float flHeight1, float flHeight1End, float flHeight2, float flHeight2End,
	float flRadius1, float flRadius1End, float flRadius2, float flRadius2End,
	Vector vecColor1, Vector vecColor1End, float flAlpha1, float flAlpha1End,
	Vector vecColor2, Vector vecColor2End, float flAlpha2, float flAlpha2End,
	float flLife, const char* pszModel, float flFrame, float flNumFrames, float flFramerate); // orig client 0x532f0
void CLTE_SetPlayerOverride(int iPlayer, float x, float y, float z, float flValue); // orig client 0x59860
void CLTE_ClearPlayerOverride(int iPlayer);		// orig client 0x598b0
void CLTE_AddFadeQuads(int iCount, Vector* pV0, Vector* pV1, Vector* pV2, Vector* pV3, Vector* pColors,
	float* pAlphas, float* pFades, model_s* pModel); // orig client 0x53780
void CLTE_LoopbackBegin();				 // orig client 0x59940
void CLTE_LoopbackSend();				 // orig client 0x59970
void CLTE_LoopbackByte(int iValue);		 // orig client 0x599a0
void CLTE_LoopbackShort(int iValue);	 // orig client 0x599d0
void CLTE_LoopbackCoord(float flValue);	 // orig client 0x59a30
Vector CLTE_ShieldPoint(int iPlayer, Vector vecPoint, Vector vecDir, float* vecNormal, int* pbHit); // orig client 0x59ee0 (writes the flag as a dword)

// view.cpp (c4): a full screen color fade from colorStart/alphaStart to colorEnd/alphaEnd over
// flDuration; a new one replaces the running one only at the same or a higher priority. name ours
// orig client 0xec590 is view.cpp's V_StartScreenTint; the events pass its two colours as three
// floats each, which this spells as Vectors (the same ten arguments)
void V_StartScreenTint(float r1, float g1, float b1, float r2, float g2, float b2, float a1, float a2, float flTime, int iPriority);
static inline void V_ScreenTint(Vector vecColorStart, Vector vecColorEnd, float flAlphaStart, float flAlphaEnd, float flDuration, int iPriority)
{
	V_StartScreenTint(vecColorStart.x, vecColorStart.y, vecColorStart.z, vecColorEnd.x, vecColorEnd.y, vecColorEnd.z, flAlphaStart, flAlphaEnd, flDuration, iPriority);
}



/*
================
FireBullets

Go to the trouble of combining multiple pellets into a single damage call.
================
*/
// orig client 0xe820 (Ghidra-missed). RC2: the players come from EV_PlayerForEntity, and a shot
// that hits a physent ends on that player's hit ellipsoid (CLTE_ShieldPoint) instead of the trace end
void EV_HLDM_FireBullets(int idx, float* forward, float* right, float* up, int cShots, float* vecSrc, float* vecDirShooting, float flDistance, int iBulletType, int iTracerFreq, int* tracerCount, float flSpreadX, float flSpreadY)
{
	int i;
	pmtrace_t tr;
	int iShot;

	for (iShot = 1; iShot <= cShots; iShot++)
	{
		Vector vecDir, vecEnd;

		float x, y, z;
		//We randomize for the Shotgun.
		if (iBulletType == BULLET_PLAYER_BUCKSHOT)
		{
			do
			{
				x = gEngfuncs.pfnRandomFloat(-0.5, 0.5) + gEngfuncs.pfnRandomFloat(-0.5, 0.5);
				y = gEngfuncs.pfnRandomFloat(-0.5, 0.5) + gEngfuncs.pfnRandomFloat(-0.5, 0.5);
				z = x * x + y * y;
			} while (z > 1);

			for (i = 0; i < 3; i++)
			{
				vecDir[i] = vecDirShooting[i] + x * flSpreadX * right[i] + y * flSpreadY * up[i];
				vecEnd[i] = vecSrc[i] + flDistance * vecDir[i];
			}
		} //But other guns already have their spread randomized in the synched spread.
		else
		{

			for (i = 0; i < 3; i++)
			{
				vecDir[i] = vecDirShooting[i] + flSpreadX * right[i] + flSpreadY * up[i];
				vecEnd[i] = vecSrc[i] + flDistance * vecDir[i];
			}
		}

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);

		// Store off the old count
		gEngfuncs.pEventAPI->EV_PushPMStates();

		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(EV_PlayerForEntity(idx) - 1);

		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		// JoshA: Changed from PM_STUDIO_BOX to PM_NORMAL in prediction code as otherwise if you hit an NPC or player's
		// bounding box but not one of their hitboxes, the shot won't hit on the server but it will
		// play a hit sound on the client and not make a decal (as if it hit the NPC/player).
		// We should mirror the way the server does the test here as close as possible.
		//
		// I initially thought I was just fixing some stupid Half-Life bug but no,
		// this is *the* root cause of all the ghost shot bad prediction bugs in Half-Life Deathmatch!
		//
		// Also... CStrike was always using PM_NORMAL for all of these so it didn't have the problem.
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_NORMAL, -1, &tr);

		// RC2: a hit on a player ends on its hit ellipsoid
		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
		if (pe)
		{
			Vector vecHit = CLTE_ShieldPoint(pe->info, tr.endpos, vecDir, nullptr, nullptr);
			VectorCopy(vecHit, tr.endpos);
		}

		EV_HLDM_CheckTracer(idx, vecSrc, tr.endpos, forward, right, iBulletType, iTracerFreq, tracerCount);

		// do damage, paint decals
		if (tr.fraction != 1.0)
		{
			switch (iBulletType)
			{
			default:
			case BULLET_PLAYER_9MM:

				EV_HLDM_PlayTextureSound(idx, &tr, vecSrc, vecEnd, iBulletType);
				EV_HLDM_DecalGunshot(&tr, iBulletType);

				break;
			case BULLET_PLAYER_MP5:

				EV_HLDM_PlayTextureSound(idx, &tr, vecSrc, vecEnd, iBulletType);
				EV_HLDM_DecalGunshot(&tr, iBulletType);
				break;
			case BULLET_PLAYER_BUCKSHOT:

				EV_HLDM_DecalGunshot(&tr, iBulletType);

				break;
			case BULLET_PLAYER_357:

				EV_HLDM_PlayTextureSound(idx, &tr, vecSrc, vecEnd, iBulletType);
				EV_HLDM_DecalGunshot(&tr, iBulletType);

				break;
			}
		}

		gEngfuncs.pEventAPI->EV_PopPMStates();
	}
}

// Can a shot get from vecStart to vecEnd, or to the entity *pTarget, through at most flThickness
// units of solid? Traces; on a hit that isn't the target it walks on along vecDir in 8 unit steps
// until it leaves the solid and tries again, at most iDepth + 1 traces. name ours
bool EV_CanPenetrate(Vector vecStart, Vector vecEnd, Vector vecDir, float flThickness, int* pTarget, pmtrace_t* tr, int iDepth) // orig client 0xec60
{
	if (iDepth < 0)
		return false;

	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, tr);

	if (tr->fraction >= 1.0)
		return true;

	if (0 != tr->ent && gEngfuncs.pEventAPI->EV_IndexFromTrace(tr) == *pTarget)
		return true;

	vecStart = tr->endpos;

	if (flThickness <= 0)
		return false;

	while (true)
	{
		float flStep = 8;
		if (flThickness < 8)
			flStep = flThickness;
		flThickness -= flStep;

		vecStart = vecStart + vecDir * flStep;

		if (gEngfuncs.PM_PointContents(vecStart, nullptr) != CONTENTS_SOLID)
			break;

		if (flThickness <= 0)
			return false;
	}

	return EV_CanPenetrate(vecStart, vecEnd, vecDir, flThickness, pTarget, tr, iDepth - 1);
}

// EV_CanPenetrate returning the thickness left over when the shot gets through, 0 when it
// doesn't. name ours
float EV_PenetrateThickness(Vector vecStart, Vector vecEnd, Vector vecDir, float flThickness, int* pTarget, pmtrace_t* tr, int iDepth) // orig client 0xee70
{
	if (iDepth < 0)
		return 0;

	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, tr);

	if (tr->fraction >= 1.0)
		return flThickness;

	if (0 != tr->ent && gEngfuncs.pEventAPI->EV_IndexFromTrace(tr) == *pTarget)
		return flThickness;

	vecStart = tr->endpos;

	if (flThickness <= 0)
		return 0;

	while (true)
	{
		float flStep = 8;
		if (flThickness < 8)
			flStep = flThickness;
		flThickness -= flStep;

		vecStart = vecStart + vecDir * flStep;

		if (gEngfuncs.PM_PointContents(vecStart, nullptr) != CONTENTS_SOLID)
			break;

		if (flThickness <= 0)
			return 0;
	}

	return EV_PenetrateThickness(vecStart, vecEnd, vecDir, flThickness, pTarget, tr, iDepth - 1);
}

//======================
//	    GLOCK START
//======================

// glockfiregib.sc: the glock's primary and tertiary fire (a gib round). name ours
void EV_GlockFireGib(event_args_t* args) // orig client 0xf1b0
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;
	bool empty;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	empty = 0 != args->bparam1;
	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4);

	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/pl_gun3.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong(0, 3));

	if (EV_IsLocal(idx))
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(empty ? GLOCK_SHOOT_EMPTY : GLOCK_SHOOT, 2);

		V_PunchAxis(0, -2.0);

		if (g_iClientDetail >= 0)
		{
			int iBlood, iBloodSpray;
			if (gEngfuncs.CL_LoadModel("sprites/blood.spr", &iBlood) && gEngfuncs.CL_LoadModel("sprites/bloodspray.spr", &iBloodSpray))
			{
				Vector vecSrc;
				EV_GetGunPosition(args, vecSrc, origin);
				VectorMA(vecSrc, 26, forward, vecSrc);
				VectorMA(vecSrc, 8, right, vecSrc);
				VectorMA(vecSrc, -8, up, vecSrc);
				gEngfuncs.pEfxAPI->R_BloodSprite(vecSrc, BLOOD_COLOR_RED, iBloodSpray, iBlood, 3);
			}
		}
	}
}

// glocktpcharge.sc: the glock's quaternary attack starts charging; the loopback message 13 starts
// the owner's charge effect. name ours
void EV_GlockTPCharge(event_args_t* args) // orig client 0xfa30 (Ghidra-missed)
{
	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(13);
	CLTE_LoopbackByte(args->entindex);
	CLTE_LoopbackSend();

	if (EV_IsLocal(args->entindex))
		gEngfuncs.pEventAPI->EV_WeaponAnimation(2, 2);
}

// The loopback messages' lag field: 0 for the local player's own shot, else the shooter's ping plus
// ours. name ours
static int EV_ShotLag(int idx)
{
	if (EV_IsLocal(idx))
		return 0;

	hud_player_info_t info;
	gEngfuncs.pfnGetPlayerInfo(idx, &info);
	int iPing = info.ping;
	gEngfuncs.pfnGetPlayerInfo(gEngfuncs.GetLocalPlayer()->index, &info);
	return info.ping + iPing;
}

// glocktpgibfire.sc: the charged quaternary shot. Ends the charge effect, sends the shot to the
// client-side projectile code (loopback message 12) and, for the shooter, a shockwave disk and a
// red flash. name ours
void EV_GlockTPGibFire(event_args_t* args) // orig client 0xfaa0 (Ghidra-missed)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;
	bool empty;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	empty = 0 != args->bparam1;
	AngleVectors(angles, forward, right, up);

	CLTE_KillByEntity(idx, 13);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4);

	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "debris/beamstart10.wav", 1.0, 0.8, 0, gEngfuncs.pfnRandomLong(120, 130));

	EV_GetGunPosition(args, vecSrc, origin);

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(12);
	CLTE_LoopbackShort(EV_ShotLag(idx));
	CLTE_LoopbackCoord(vecSrc.x);
	CLTE_LoopbackCoord(vecSrc.y);
	CLTE_LoopbackCoord(vecSrc.z);
	CLTE_LoopbackShort((int)(forward.x * 32767.0));
	CLTE_LoopbackShort((int)(forward.y * 32767.0));
	CLTE_LoopbackShort((int)(forward.z * 32767.0));
	CLTE_LoopbackSend();

	if (EV_IsLocal(idx))
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(empty ? GLOCK_SHOOT_EMPTY : GLOCK_SHOOT, 2);

		V_PunchAxis(0, -12.0);

		if (g_iClientDetail >= 0)
		{
			Vector vecDisk;
			EV_GetGunPosition(args, vecDisk, origin);
			VectorMA(vecDisk, 26, forward, vecDisk);
			VectorMA(vecDisk, 8, right, vecDisk);
			VectorMA(vecDisk, -8, up, vecDisk);

			CLTE_AddDiskRing(vecDisk, up, right, forward, 1,
				0, 8, 2, 10, 1, 15, 4, 18,
				Vector(1, 0, 0), Vector(1, 0, 0), 0.3, 0,
				Vector(1, 0, 0), Vector(1, 0, 0), 0.9, 0.1,
				0.2, "sprites/shockwave.spr", 0, 0, 0);
		}

		V_ScreenTint(Vector(255, 0, 0), Vector(255, 0, 0), 128, 0, 0.5, 0);
	}
}

// glockfart.sc: the glock's secondary attack. Under water (bparam1) bubbles and a gurgle, else a
// small explosion in front of the gun. name ours
void EV_GlockFart(event_args_t* args) // orig client 0x10a40 (Ghidra-missed)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4);

	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	if (0 != args->bparam1)
	{
		char sz[256];

		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "ambience/biggun2.wav", 0.4, 0.8, 0, gEngfuncs.pfnRandomLong(80, 90));
		sprintf(sz, "player/pl_swim%i.wav", gEngfuncs.pfnRandomLong(1, 4));
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_BODY, sz, 1.0, 0.8, 0, gEngfuncs.pfnRandomLong(100, 105));

		int iBubble;
		if (gEngfuncs.CL_LoadModel("sprites/bubble.spr", &iBubble))
		{
			Vector vecPos = origin - forward * 16 - up * 8;
			Vector vecWater = vecPos;
			Vector vecOne(1, 1, 1);
			Vector mins, maxs;
			VectorMA(args->origin, -10, vecOne, mins);
			VectorMA(args->origin, 10, vecOne, maxs);

			float flHeight = EV_WaterLevel(vecWater, origin.z, origin.z + 1024);
			gEngfuncs.pEfxAPI->R_Bubbles(mins, maxs, flHeight - mins.z, iBubble, 10, 8);
		}
	}
	else
	{
		Vector vecPos = origin - forward * 16 - up * 8;

		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "ambience/biggun2.wav", 1.0, 0.9, 0, gEngfuncs.pfnRandomLong(100, 105));

		int iExplode;
		if (gEngfuncs.CL_LoadModel("sprites/zerogxplode.spr", &iExplode))
			gEngfuncs.pEfxAPI->R_Explosion(vecPos, iExplode, 0.5, 15, TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOPARTICLES);
	}

	if (EV_IsLocal(idx))
		V_PunchAxis(0, -6.0);
}

// glockgibimpact.sc: a glock gib hits (iparam1 the packed direction, bparam1 on the world: a blood
// decal there, bparam2 the gib is gone: a bigger splash). name ours
void EV_GlockGibImpact(event_args_t* args) // orig client 0x12010 (Ghidra-missed)
{
	int idx = args->entindex;
	Vector vecDir = UTIL_UnpackNormal(args->iparam1);
	Vector origin;
	Vector vecNormal;
	Vector vecStart, vecEnd;
	pmtrace_t tr;

	VectorCopy(args->origin, origin);

	VectorScale(vecDir, -1, vecNormal);
	VectorMA(origin, -8, vecDir, vecStart);
	VectorMA(origin, 32, vecDir, vecEnd);

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_BOX, args->entindex, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.fraction < 1.0)
	{
		if (0 != args->bparam1)
		{
			physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
			if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
			{
				char szDecal[32];
				sprintf(szDecal, "{blood%i", gEngfuncs.pfnRandomLong(1, 6));
				gEngfuncs.pEfxAPI->R_DecalShoot(
					gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(szDecal)),
					gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
			}
		}

		VectorMA(tr.endpos, 8, tr.plane.normal, origin);
		vecNormal = tr.plane.normal;
	}

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_BODY, "squeek/sqk_blast1.wav", 1.0, 0.8, 0, 100);

	int iBlood, iBloodSpray;
	if (gEngfuncs.CL_LoadModel("sprites/blood.spr", &iBlood) && gEngfuncs.CL_LoadModel("sprites/bloodspray.spr", &iBloodSpray))
	{
		if (0 != args->bparam2)
			gEngfuncs.pEfxAPI->R_BloodSprite(origin, BLOOD_COLOR_RED, iBloodSpray, iBlood, 10);
		else
			gEngfuncs.pEfxAPI->R_BloodSprite(origin, BLOOD_COLOR_RED, iBloodSpray, iBlood, 5);
	}
}

// stickygibattach.sc: a sticky glock gib sticks to something (bparam1 on the world: a big blood
// decal). name ours
void EV_StickyGibAttach(event_args_t* args) // orig client 0x12370 (Ghidra-missed)
{
	int idx = args->entindex;
	Vector vecDir = UTIL_UnpackNormal(args->iparam1);
	Vector origin;
	Vector vecNormal;
	Vector vecStart, vecEnd;
	pmtrace_t tr;

	VectorCopy(args->origin, origin);

	VectorScale(vecDir, -1, vecNormal);
	VectorMA(origin, -8, vecDir, vecStart);
	VectorMA(origin, 32, vecDir, vecEnd);

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_BOX, args->entindex, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.fraction < 1.0)
	{
		if (0 != args->bparam1)
		{
			physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
			if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
			{
				char szDecal[32];
				sprintf(szDecal, "{bigblood2");
				gEngfuncs.pEfxAPI->R_DecalShoot(
					gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(szDecal)),
					gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
			}
		}

		VectorMA(tr.endpos, 8, tr.plane.normal, origin);
		vecNormal = tr.plane.normal;
	}

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_BODY, "weapons/pwach.wav", 0.8, 0.8, 0, 130);

	int iBlood, iBloodSpray;
	if (gEngfuncs.CL_LoadModel("sprites/blood.spr", &iBlood) && gEngfuncs.CL_LoadModel("sprites/bloodspray.spr", &iBloodSpray))
		gEngfuncs.pEfxAPI->R_BloodSprite(origin, BLOOD_COLOR_RED, iBloodSpray, iBlood, 12);
}


//======================
//	   SHOTGUN START
//======================

// shotgunsci.sc: the shotgun fires a shotgun scientist. name ours
void EV_FireShotgunSci(event_args_t* args) // orig client 0x12680
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shotgunshell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(1, 2);

		V_PunchAxis(0, -6.0);
	}

	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 6);

	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHOTSHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "ambience/biggun2.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0x1f));

	EV_GetGunPosition(args, vecSrc, origin); // result unused, as in the original
}

// shotgunsuit.sc: the shotgun fires an HEV suit. name ours
void EV_FireShotgunSuit(event_args_t* args) // orig client 0x12840
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shotgunshell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(1, 2);

		V_PunchAxis(0, -5.0);
	}

	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 6);

	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHOTSHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "ambience/biggun2.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0x1f));
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_VOICE, "weapons/rocketfire1.wav", 0.9, ATTN_NORM, 0, PITCH_NORM);

	EV_GetGunPosition(args, vecSrc, origin); // result unused, as in the original
}

// shotgunfl.sc: the shotgun fires a forklift. name ours
void EV_FireShotgunFL(event_args_t* args) // orig client 0x12a40
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shotgunshell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(1, 2);

		V_PunchAxis(0, -6.0);
	}

	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 6);

	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHOTSHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "ambience/biggun2.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0x1f));

	EV_GetGunPosition(args, vecSrc, origin); // result unused, as in the original
}

// firesgcrate.sc: the shotgun fires its charged crate; ends the charge effect (19). name ours
void EV_FireShotgunCrate(event_args_t* args) // orig client 0x12c00
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	CLTE_KillByEntity(idx, 19);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shotgunshell.mdl"); // brass shell

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(2, 2);

		V_PunchAxis(0, -10.0);
	}

	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 6);

	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHOTSHELL);
	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHOTSHELL);
	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHOTSHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "ambience/biggun3.wav", 1.0, ATTN_NORM, 0, 90);
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_VOICE, "weapons/dbarrel1.wav", gEngfuncs.pfnRandomFloat(0.98, 1.0), ATTN_NORM, 0, gEngfuncs.pfnRandomLong(90, 100));

	EV_GetGunPosition(args, vecSrc, origin); // result unused, as in the original
}

// sgcratecharge.sc: the shotgun's crate starts charging; loopback message 19 starts the owner's
// charge effect. name ours
void EV_ShotgunCrateCharge(event_args_t* args) // orig client 0x12e70 (Ghidra-missed)
{
	if (EV_IsLocal(args->entindex))
		gEngfuncs.pEventAPI->EV_WeaponAnimation(8, 2);

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(19);
	CLTE_LoopbackByte(args->entindex);
	CLTE_LoopbackSend();
}

// sgcratehit.sc: the shotgun crate blows up (iparam1 the packed direction, fparam1 the charge,
// bparam1 the explosion's flags): scorch, explosion, sparks, an expsmoke event and wood gibs. name ours
void EV_ShotgunCrateHit(event_args_t* args) // orig client 0x12ee0 (Ghidra-missed)
{
	static unsigned short s_usExpSmoke = 0; // name ours (orig .data 0x10179da0)

	Vector origin;
	Vector vecDir;
	Vector vecStart, vecEnd;
	pmtrace_t tr;
	int iModel;
	int iContents;
	const char* pszDecal;
	const char* pszSound;

	VectorCopy(args->origin, origin);
	vecDir = UTIL_UnpackNormal(args->iparam1);

	VectorMA(origin, -8, vecDir, vecStart);
	VectorMA(origin, 64, vecDir, vecEnd);

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_IGNORE, args->entindex, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.fraction < 1.0)
	{
		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);

		pszDecal = "{scorch1";
		switch (gEngfuncs.pfnRandomLong(0, 2))
		{
		case 0:
			pszDecal = "{scorch1";
			break;
		case 1:
			pszDecal = "{scorch2";
			break;
		case 2:
			pszDecal = "{scorch3";
			break;
		}

		if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
		{
			gEngfuncs.pEfxAPI->R_DecalShoot(
				gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)pszDecal)),
				gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
		}

		VectorMA(tr.endpos, args->fparam1 * 140, tr.plane.normal, origin);
	}
	else
	{
		tr.plane.normal[0] = 0;
		tr.plane.normal[1] = 0;
		tr.plane.normal[2] = 1;
	}

	iContents = gEngfuncs.PM_PointContents(origin, nullptr);
	if (iContents != CONTENTS_WATER)
	{
		if (!gEngfuncs.CL_LoadModel("sprites/zerogxplode.spr", &iModel))
			return;
	}
	else
	{
		if (!gEngfuncs.CL_LoadModel("sprites/WXplo1.spr", &iModel))
			return;
	}

	gEngfuncs.pEfxAPI->R_Explosion(origin, iModel, RC2Pref(RC2P_Shotgun_CrateMaxRadius).f * 0.02 * args->fparam1, 15, args->bparam1);

	if (iContents != CONTENTS_WATER)
	{
		for (int i = gEngfuncs.pfnRandomLong(1, 3); i > 0; i--)
		{
			TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAllocNoModel(origin);
			if (!pTemp)
				continue;

			pTemp->flags |= FTENT_SLOWGRAVITY | FTENT_COLLIDEWORLD | FTENT_SPARKSHOWER;
			pTemp->entity.baseline.framerate = gEngfuncs.pfnRandomFloat(0.5, 1.5);
			pTemp->entity.baseline.scale = 0.1;
			VectorScale(tr.plane.normal, gEngfuncs.pfnRandomFloat(200, 300), pTemp->entity.baseline.origin);
			pTemp->entity.baseline.origin[0] += gEngfuncs.pfnRandomFloat(-100, 100);
			pTemp->entity.baseline.origin[1] += gEngfuncs.pfnRandomFloat(-100, 100);
			if (pTemp->entity.baseline.origin[2] > 0)
				pTemp->entity.baseline.origin[2] += 200;
			else
				pTemp->entity.baseline.origin[2] -= 200;
		}
	}

	switch (gEngfuncs.pfnRandomLong(0, 2))
	{
	case 0:
		pszSound = "weapons/debris1.wav";
		break;
	case 1:
		pszSound = "weapons/debris2.wav";
		break;
	case 2:
	default: // can't happen; the original leaves the sound unset there
		pszSound = "weapons/debris3.wav";
		break;
	}
	gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, origin, CHAN_VOICE, pszSound, 0.55, 0.8, 0, 100);

	if (0 == s_usExpSmoke)
		s_usExpSmoke = gEngfuncs.pfnPrecacheEvent(1, "events/expsmoke.sc");

	gEngfuncs.pfnPlaybackEvent(args->flags, nullptr, s_usExpSmoke, 0.3, origin, args->angles,
		RC2Pref(RC2P_Shotgun_CrateMaxRadius).f * 0.4 * args->fparam1, 0, 0, 0, 0, 0);

	if (g_iClientDetail < 0)
		return;

	if (!gEngfuncs.CL_LoadModel("models/woodgibs.mdl", &iModel))
		return;

	int cGibs = gEngfuncs.pfnRandomLong(30, 40);

	VectorMA(tr.endpos, 32, tr.plane.normal, origin);

	for (int i = cGibs; i > 0; i--)
	{
		Vector vecPos;
		vecPos[0] = origin[0] + gEngfuncs.pfnRandomFloat(-12, 12);
		vecPos[1] = origin[1] + gEngfuncs.pfnRandomFloat(-12, 12);
		vecPos[2] = origin[2] + gEngfuncs.pfnRandomFloat(-12, 12);

		Vector vecVel;
		vecVel[0] = gEngfuncs.pfnRandomFloat(-1, 1);
		vecVel[1] = gEngfuncs.pfnRandomFloat(-1, 1);
		vecVel[2] = gEngfuncs.pfnRandomFloat(-1, 1);
		vecVel = vecVel.Normalize() * gEngfuncs.pfnRandomFloat(500, 600);

		Vector vecAngles;
		vecAngles[0] = gEngfuncs.pfnRandomFloat(-90, 90);
		vecAngles[1] = gEngfuncs.pfnRandomFloat(0, 360);
		vecAngles[2] = gEngfuncs.pfnRandomFloat(-90, 90);

		if (iContents == CONTENTS_WATER)
			vecVel = vecVel * 0.25;

		TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->R_TempModel(vecPos, vecVel, vecAngles, 6, iModel, BOUNCE_WOOD);
		if (pTemp)
		{
			pTemp->entity.curstate.body = gEngfuncs.pfnRandomLong(0, 2);
			pTemp->flags |= FTENT_ROTATE | FTENT_FADEOUT;
			pTemp->fadeSpeed = 2;
			pTemp->entity.curstate.renderamt = 255;
			pTemp->entity.baseline.renderamt = 255;
			pTemp->entity.baseline.angles[0] = gEngfuncs.pfnRandomFloat(-200, 200);
			pTemp->entity.baseline.angles[1] = gEngfuncs.pfnRandomFloat(-200, 200);
			pTemp->entity.baseline.angles[2] = gEngfuncs.pfnRandomFloat(-200, 200);
			pTemp->die += gEngfuncs.pfnRandomFloat(-1, 1);
		}
	}
}


//======================
//	    MP5 START
//======================

// mp5primary.sc: the MP5's blaster bolt. The client-side bolt is loopback message 16 (owner,
// iparam2 the bolt's id, lag, start, direction). name ours
void EV_MP5Primary(event_args_t* args) // orig client 0x13830 (Ghidra-missed)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecDir;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	EV_GetGunPosition(args, vecSrc, origin);

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(MP5_FIRE1 + gEngfuncs.pfnRandomLong(0, 2), 2);

		V_PunchAxis(0, gEngfuncs.pfnRandomFloat(-1, 1));
		V_PunchAxis(1, gEngfuncs.pfnRandomFloat(-1, 1));
	}

	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4);

	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "debris/beamstart4.wav", 1.0, 0.8, 0, gEngfuncs.pfnRandomLong(140, 150));

	vecDir = forward;
	vecSrc = vecSrc + right - up;

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(16);
	CLTE_LoopbackByte(idx);
	CLTE_LoopbackShort(args->iparam2);
	CLTE_LoopbackShort(EV_ShotLag(idx));
	CLTE_LoopbackCoord(vecSrc.x);
	CLTE_LoopbackCoord(vecSrc.y);
	CLTE_LoopbackCoord(vecSrc.z);
	CLTE_LoopbackShort((int)(vecDir.x * 32767.0));
	CLTE_LoopbackShort((int)(vecDir.y * 32767.0));
	CLTE_LoopbackShort((int)(vecDir.z * 32767.0));
	CLTE_LoopbackSend();
}

// mp5fart.sc: the MP5's secondary attack. Under water (bparam1) bubbles and a gurgle, else a
// small explosion in front of the gun. name ours
void EV_MP5Fart(event_args_t* args) // orig client 0x13c10
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	if (0 == args->bparam1)
	{
		Vector vecPos = origin - forward * 16;

		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "ambience/biggun3.wav", 1.0, 0.9, 0, gEngfuncs.pfnRandomLong(100, 105));

		int iExplode;
		if (gEngfuncs.CL_LoadModel("sprites/zerogxplode.spr", &iExplode))
			gEngfuncs.pEfxAPI->R_Explosion(vecPos, iExplode, 1.0, 18, TE_EXPLFLAG_NOPARTICLES);
	}
	else
	{
		char sz[256];

		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "ambience/biggun3.wav", 0.2, 0.8, 0, gEngfuncs.pfnRandomLong(80, 90));
		sprintf(sz, "player/pl_swim%i.wav", gEngfuncs.pfnRandomLong(1, 4));
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_BODY, sz, 1.0, 0.8, 0, gEngfuncs.pfnRandomLong(100, 105));

		int iBubble;
		if (gEngfuncs.CL_LoadModel("sprites/bubble.spr", &iBubble))
		{
			Vector vecPos = origin - up * 8 - forward * 16;
			Vector vecWater = vecPos;
			Vector vecOne(1, 1, 1);
			Vector mins, maxs;
			VectorMA(args->origin, -18, vecOne, mins);
			VectorMA(args->origin, 18, vecOne, maxs);

			float flHeight = EV_WaterLevel(vecWater, origin.z, origin.z + 1024);
			gEngfuncs.pEfxAPI->R_Bubbles(mins, maxs, flHeight - mins.z, iBubble, 12, 8);
		}
	}

	if (EV_IsLocal(idx))
		V_PunchAxis(0, -10.0);
}

// mp5bouncy.sc: the MP5's bouncing bolt (loopback message 18, as mp5primary). name ours
void EV_MP5Bouncy(event_args_t* args) // orig client 0x140b0 (Ghidra-missed)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecDir;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	EV_GetGunPosition(args, vecSrc, origin);

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(MP5_FIRE1 + gEngfuncs.pfnRandomLong(0, 2), 2);

		V_PunchAxis(0, gEngfuncs.pfnRandomFloat(-1.2, 1.2));
		V_PunchAxis(1, gEngfuncs.pfnRandomFloat(-1.2, 1.2));
	}

	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4);

	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "debris/beamstart4.wav", 1.0, 0.8, 0, gEngfuncs.pfnRandomLong(120, 130));

	vecDir = forward;
	vecSrc = vecSrc + right - up;

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(18);
	CLTE_LoopbackByte(idx);
	CLTE_LoopbackShort(args->iparam2);
	CLTE_LoopbackShort(EV_ShotLag(idx));
	CLTE_LoopbackCoord(vecSrc.x);
	CLTE_LoopbackCoord(vecSrc.y);
	CLTE_LoopbackCoord(vecSrc.z);
	CLTE_LoopbackShort((int)(vecDir.x * 32767.0));
	CLTE_LoopbackShort((int)(vecDir.y * 32767.0));
	CLTE_LoopbackShort((int)(vecDir.z * 32767.0));
	CLTE_LoopbackSend();
}

// mp5pulse.sc: the MP5's charged pulse (loopback message 17); the shooter gets a green shockwave
// disk and a green flash. name ours
void EV_MP5Pulse(event_args_t* args) // orig client 0x14490 (Ghidra-missed)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector vecSrc, vecDir;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, right, up);

	EV_GetGunPosition(args, vecSrc, origin);

	if (EV_IsLocal(idx))
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(2, 2);

		V_PunchAxis(0, -10.0);

		Vector vecDisk = vecSrc + forward * 18 + right * 4 - up * 6;

		CLTE_AddDiskRing(vecDisk, up, right, forward, 2,
			0, 0, 0, 0, 0, 8, 4, 12,
			Vector(0, 1, 0), Vector(0, 1, 0), 0.3, 0,
			Vector(0, 1, 0), Vector(0, 1, 0), 0.9, 0,
			0.2, "sprites/shockwave.spr", 0, 0, 0);

		V_ScreenTint(Vector(0, 255, 0), Vector(0, 255, 0), 160, 0, 0.1, 0);
	}

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "debris/beamstart10.wav", 1.0, 0.8, 0, gEngfuncs.pfnRandomLong(110, 120));

	vecDir = forward;
	vecSrc = vecSrc + forward * 12;

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(17);
	CLTE_LoopbackByte(idx);
	CLTE_LoopbackShort(args->iparam2);
	CLTE_LoopbackShort(EV_ShotLag(idx));
	CLTE_LoopbackCoord(vecSrc.x);
	CLTE_LoopbackCoord(vecSrc.y);
	CLTE_LoopbackCoord(vecSrc.z);
	CLTE_LoopbackShort((int)(vecDir.x * 32767.0));
	CLTE_LoopbackShort((int)(vecDir.y * 32767.0));
	CLTE_LoopbackShort((int)(vecDir.z * 32767.0));
	CLTE_LoopbackSend();
}

// The bolts' impacts first tell the client-side projectile code (loopback message 253) to drop
// bolt iparam2 of type iType owned by bparam2. name ours
static void EV_MP5BoltKill(event_args_t* args, int iType)
{
	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(253);
	CLTE_LoopbackByte(iType);
	CLTE_LoopbackByte(args->bparam2);
	CLTE_LoopbackShort(args->iparam2);
	CLTE_LoopbackSend();
}

// mp5blasterhit.sc: a blaster bolt hits (iparam1 the packed direction, bparam1 it hit something:
// a bullet hole and a red shockwave disk, bparam2 the owner). name ours
void EV_MP5BlasterHit(event_args_t* args) // orig client 0x14b40 (Ghidra-missed)
{
	Vector origin;
	Vector vecDir;
	Vector vecStart, vecEnd;
	pmtrace_t tr;
	const char* pszDecal = nullptr;

	EV_MP5BoltKill(args, 16);

	VectorCopy(args->origin, origin);
	vecDir = UTIL_UnpackNormal(args->iparam1);

	VectorMA(origin, -8, vecDir, vecStart);
	VectorMA(origin, 64, vecDir, vecEnd);

	if (0 == args->bparam1)
		return;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(args->bparam2 - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_IGNORE, args->entindex, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.fraction < 1.0)
	{
		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
		if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
		{
			switch (gEngfuncs.pfnRandomLong(0, 4))
			{
			case 0:
				pszDecal = "{bigshot1";
				break;
			case 1:
				pszDecal = "{bigshot2";
				break;
			case 2:
				pszDecal = "{bigshot3";
				break;
			case 3:
				pszDecal = "{bigshot4";
				break;
			case 4:
				pszDecal = "{bigshot5";
				break;
			}
			gEngfuncs.pEfxAPI->R_DecalShoot(
				gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)pszDecal)),
				gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
		}
	}
	else
	{
		tr.plane.normal[0] = 0;
		tr.plane.normal[1] = 0;
		tr.plane.normal[2] = 1;
	}

	if (g_iClientDetail < 0)
		return;

	Vector angles, forward, right, up;
	VectorAngles(tr.plane.normal, angles);
	angles[0] = -angles[0];
	angles[2] = gEngfuncs.pfnRandomFloat(0, 360);
	AngleVectors(angles, forward, right, up);

	CLTE_AddDiskRing(Vector(tr.endpos) + Vector(tr.plane.normal) * 2, up, right, forward, 0,
		0, 0, 0, 0, 1, 9, 3, 11,
		Vector(1, 0, 0), Vector(1, 0, 0), 0.3, 0,
		Vector(1, 0, 0), Vector(1, 0, 0), 0.9, 0.1,
		0.15, "sprites/shockwave.spr", 0, 0, 0);
}

// mp5pulsehit.sc: a pulse hits. A green flash for the local player when he can see the impact in
// front of him, then a scorch and a green shockwave disk. name ours
void EV_MP5PulseHit(event_args_t* args) // orig client 0x150b0 (Ghidra-missed)
{
	Vector origin;
	Vector vecDir;
	Vector vecStart, vecEnd;
	pmtrace_t tr;
	const char* pszDecal = nullptr;

	EV_MP5BoltKill(args, 17);

	VectorCopy(args->origin, origin);
	vecDir = UTIL_UnpackNormal(args->iparam1);

	VectorMA(origin, -8, vecDir, vecStart);
	VectorMA(origin, 64, vecDir, vecEnd);

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal)
		return;

	gEngfuncs.pEventAPI->EV_PlaySound(-1, origin, CHAN_AUTO, "debris/beamstart9.wav", 1.0, 0.5, 0, 130);

	Vector view_ofs, viewangles, forward;
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);
	gEngfuncs.GetViewAngles(viewangles);
	Vector vecEye = Vector(pLocal->origin[0] + view_ofs[0], pLocal->origin[1] + view_ofs[1], pLocal->origin[2] + view_ofs[2]);
	AngleVectors(viewangles, forward, nullptr, nullptr);

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(args->bparam2 - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_IGNORE, args->entindex, &tr);

	// the macro evaluates the normalized vector once per component, as the original did
	if (DotProduct(forward, (origin - vecEye).Normalize()) > 0)
	{
		pmtrace_t trView;
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecEye, origin, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, &trView);
		if (trView.fraction >= 1.0 || (0 != trView.ent && gEngfuncs.pEventAPI->EV_IndexFromTrace(&trView) == pLocal->index))
			V_ScreenTint(Vector(0, 255, 0), Vector(0, 255, 0), 220, 0, 0.5, -1);
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();

	Vector angles, right, up;

	if (0 != args->bparam1)
	{
		if (tr.fraction < 1.0)
		{
			physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
			if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
			{
				switch (gEngfuncs.pfnRandomLong(0, 2))
				{
				case 0:
					pszDecal = "{scorch1";
					break;
				case 1:
					pszDecal = "{scorch2";
					break;
				case 2:
					pszDecal = "{scorch3";
					break;
				}
				gEngfuncs.pEfxAPI->R_DecalShoot(
					gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)pszDecal)),
					gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
			}
		}
		else
		{
			tr.plane.normal[0] = 0;
			tr.plane.normal[1] = 0;
			tr.plane.normal[2] = 1;
		}

		if (g_iClientDetail < 0)
			return;

		VectorAngles(tr.plane.normal, angles);
	}
	else
	{
		if (g_iClientDetail < 0)
			return;

		VectorAngles(vecDir, angles);
	}

	angles[0] = -angles[0];
	angles[2] = gEngfuncs.pfnRandomFloat(0, 360);
	AngleVectors(angles, forward, right, up);

	CLTE_AddDiskRing(Vector(tr.endpos) + Vector(tr.plane.normal) * 2, up, right, forward, 3,
		0, 0, 0, 0, 4, 60, 16, 60,
		Vector(0, 1, 0), Vector(0, 1, 0), 0.3, 0,
		Vector(0, 1, 0), Vector(0, 1, 0), 0.9, 0,
		0.25, "sprites/shockwave.spr", 0, 0, 0);
}

// mp5bouncyhit.sc: a bouncing bolt hits: a zap, a bullet hole and a blue shockwave disk. name ours
void EV_MP5BouncyHit(event_args_t* args) // orig client 0x15bd0 (Ghidra-missed)
{
	Vector origin;
	Vector vecDir;
	Vector vecStart, vecEnd;
	pmtrace_t tr;
	const char* pszDecal = nullptr;

	EV_MP5BoltKill(args, 18);

	VectorCopy(args->origin, origin);
	vecDir = UTIL_UnpackNormal(args->iparam1);

	VectorMA(origin, -8, vecDir, vecStart);
	VectorMA(origin, 64, vecDir, vecEnd);

	if (0 == args->bparam1)
		return;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(args->bparam2 - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_IGNORE, args->entindex, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (g_iClientDetail >= 0)
		gEngfuncs.pEventAPI->EV_PlaySound(-1, tr.endpos, CHAN_AUTO, "debris/beamstart11.wav", 0.3, 1.0, 0, gEngfuncs.pfnRandomLong(130, 140));

	if (tr.fraction < 1.0)
	{
		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
		if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
		{
			switch (gEngfuncs.pfnRandomLong(0, 4))
			{
			case 0:
				pszDecal = "{bigshot1";
				break;
			case 1:
				pszDecal = "{bigshot2";
				break;
			case 2:
				pszDecal = "{bigshot3";
				break;
			case 3:
				pszDecal = "{bigshot4";
				break;
			case 4:
				pszDecal = "{bigshot5";
				break;
			}
			gEngfuncs.pEfxAPI->R_DecalShoot(
				gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)pszDecal)),
				gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
		}
	}
	else
	{
		tr.plane.normal[0] = 0;
		tr.plane.normal[1] = 0;
		tr.plane.normal[2] = 1;
	}

	if (g_iClientDetail < 0)
		return;

	Vector angles, forward, right, up;
	VectorAngles(tr.plane.normal, angles);
	angles[0] = -angles[0];
	angles[2] = gEngfuncs.pfnRandomFloat(0, 360);
	AngleVectors(angles, forward, right, up);

	CLTE_AddDiskRing(Vector(tr.endpos) + Vector(tr.plane.normal) * 2, up, right, forward, 0,
		0, 0, 0, 0, 1, 12, 5, 12,
		Vector(0.5, 0.5, 1), Vector(0.5, 0.5, 1), 0.3, 0,
		Vector(0.5, 0.5, 1), Vector(0.5, 0.5, 1), 0.9, 0.1,
		0.15, "sprites/shockwave.spr", 0, 0, 0);
}

// mp5charge.sc: the MP5's pulse starts charging. name ours
void EV_MP5Charge(event_args_t* args) // orig client 0x16100 (Ghidra-missed)
{
	gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, args->origin, CHAN_WEAPON, "weapons/mp5charge.wav", 1.0, 0.8, 0, 100);

	if (EV_IsLocal(args->entindex))
		gEngfuncs.pEventAPI->EV_WeaponAnimation(0, 2);
}


//======================
//	   PHYTON START
//	     ( .357 )
//======================

// A lightning beam from vecStart to vecEnd (bAttach: from attachment 1 of entity idx to vecEnd)
// and, unless bNoFlash or idx is the local player, a white flash for the local player when he can
// see a point of the bolt in front of him. Walks the bolt in 16 steps; true when it flashed or
// had nothing to check. name ours
bool EV_DrawLightning(Vector vecStart, Vector vecEnd, float flWidth, int bNoFlash, int bAttach, int idx) // orig client 0x16170
{
	int iBeam;
	if (!gEngfuncs.CL_LoadModel("sprites/lgtning.spr", &iBeam))
		return true;

	if (0 != bAttach)
	{
		gEngfuncs.pEfxAPI->R_BeamEntPoint(idx | 0x1000, vecEnd, iBeam, 0.05, flWidth, 0.3, 1.0, 0,
			(int)gEngfuncs.pfnRandomFloat(0, 2), 20, 0.95, 0.95, 1.0);
	}
	else
	{
		BEAM* pBeam = gEngfuncs.pEfxAPI->R_BeamPoints(vecStart, vecEnd, iBeam, 0.05, flWidth, 0.3, 1.0, 0,
			(int)gEngfuncs.pfnRandomFloat(0, 2), 20, 0.95, 0.95, 1.0);
		if (pBeam)
			pBeam->flags |= FBEAM_SHADEOUT;
	}

	if (0 != bNoFlash)
		return true;

	if (EV_IsLocal(idx))
		return true;

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal)
		return true;

	Vector view_ofs, viewangles, forward;
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);
	gEngfuncs.GetViewAngles(viewangles);
	AngleVectors(viewangles, forward, nullptr, nullptr);

	Vector vecEye = Vector(pLocal->origin[0] + view_ofs[0], pLocal->origin[1] + view_ofs[1], pLocal->origin[2] + view_ofs[2]);
	Vector vecPoint = vecStart;
	Vector vecStep = (vecEnd - vecStart) * 0.0625;

	int i = 15;
	do
	{
		// the macro evaluates the normalized vector once per component, as the original did
		if (DotProduct(forward, (vecPoint - vecEye).Normalize()) < 0)
		{
			vecPoint = vecPoint + vecStep;
		}
		else
		{
			// original bug kept: a point in front of the viewer but hidden doesn't advance, so the
			// remaining steps trace the same point again
			pmtrace_t tr;
			gEngfuncs.pEventAPI->EV_PlayerTrace(vecPoint, vecEye, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, &tr);
			if (tr.fraction >= 1.0 || (0 != tr.ent && gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr) == pLocal->index))
			{
				V_ScreenTint(Vector(230, 230, 255), Vector(230, 230, 255), 160, 0, 0.25, -1);
				return true;
			}
		}
	} while (i-- > 0);

	return false;
}

// The python's chain lightning jumping on from vecFrom: to every current entity flagged as a chain
// target (curstate.team 2; VERIFY: settle against the server's AddToFullPack) not yet in the bit mask, in reach (Python ChainMaxChainDistance) and in
// sight, a bolt (drawn from pSource's attachment), and on from there while the hit isn't a
// player's ellipsoid. name ours
void EV_ChainLightning(int iDepth, Vector vecFrom, int iIgnore, unsigned char* pMask, cl_entity_t* pSource, int* pFlashed) // orig client 0x17690
{
	int bHitPlayer = false;

	if (iDepth <= 0)
		return;

	for (int i = 1; i < g_iHighestEntIndex; i++)
	{
		cl_entity_t* pEnt = GetEntity(i);
		if (!pEnt)
			continue;
		if (pEnt->curstate.messagenum < pSource->curstate.messagenum)
			continue;
		if (pEnt->index == iIgnore)
			continue;
		if (pEnt->curstate.team != 2)
			continue;
		if ((pMask[pEnt->index / 8] & (1 << (pEnt->index % 8))) != 0)
			continue;

		Vector vecTarget = pEnt->origin + (Vector(pEnt->curstate.mins) + Vector(pEnt->curstate.maxs)) * 0.5;

		if ((vecTarget - vecFrom).Length() > RC2Pref(RC2P_Python_ChainMaxChainDistance).f)
			continue;

		pmtrace_t tr;
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecFrom, vecTarget, PM_STUDIO_IGNORE, iIgnore, &tr);
		if (tr.fraction < 1.0 && gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr) != pEnt->index)
			continue;

		pMask[pEnt->index / 8] |= 1 << (pEnt->index % 8);

		vecTarget = CLTE_ShieldPoint(pEnt->index, vecTarget, (vecTarget - vecFrom).Normalize(), nullptr, &bHitPlayer);

		*pFlashed = EV_DrawLightning(vecFrom, vecTarget, 2.5, *pFlashed, 1, pSource->index);

		// original: the depth is passed on unchanged; only the mask ends the chain
		if (!bHitPlayer)
			EV_ChainLightning(iDepth, vecTarget, pEnt->index, pMask, pSource, pFlashed);
	}
}

// pythonlightning.sc: the python's lightning bolt. Under water (bparam1) only a fizzle; a bolt
// ending in water sprays six small bolts at the surface, one ending on the world leaves a scorch
// and a disk. name ours
void EV_PythonLightning(event_args_t* args) // orig client 0x168f0 (Ghidra-missed)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector vecSrc, vecEnd;
	Vector up, right, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, right, up);

	CLTE_KillByEntity(idx, 14);

	if (EV_IsLocal(idx))
	{
		V_ScreenTint(Vector(230, 230, 255), Vector(230, 230, 255), 160, 0, 0.33, 0);
		gEngfuncs.pEventAPI->EV_WeaponAnimation(PYTHON_FIRE1, gEngfuncs.GetMaxClients() != 1 ? 1 : 0);
		V_PunchAxis(0, -10.0);
	}

	if (0 != args->bparam1)
	{
		switch (gEngfuncs.pfnRandomLong(0, 2))
		{
		case 0:
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/electro4.wav", gEngfuncs.pfnRandomFloat(0.8, 0.6), 0.8, 0, 100);
			break;
		case 1:
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/electro5.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), 0.8, 0, 100);
			break;
		case 2:
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/electro6.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), 0.8, 0, 100);
			break;
		}
		return;
	}

	switch (gEngfuncs.pfnRandomLong(0, 1))
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/357_shot1.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), 0.8, 0, 100);
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/357_shot2.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), 0.8, 0, 100);
		break;
	}

	EV_GetGunPosition(args, vecSrc, origin);
	vecEnd = forward * 8192 + vecSrc;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(EV_PlayerForEntity(idx) - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);

	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
	if (pe)
	{
		Vector vecHit = CLTE_ShieldPoint(pe->info, tr.endpos, forward, nullptr, nullptr);
		VectorCopy(vecHit, tr.endpos);
	}

	if (gEngfuncs.PM_PointContents(tr.endpos, nullptr) == CONTENTS_WATER)
	{
		// back along the bolt to the water's surface
		Vector vecDir = forward;
		vecEnd = tr.endpos;

		int i = 127;
		bool bSurface = false;
		while (true)
		{
			if (gEngfuncs.PM_PointContents(vecEnd, nullptr) != CONTENTS_WATER)
			{
				bSurface = true;
				break;
			}
			vecEnd = vecEnd - vecDir * 8;
			if (i-- <= 0)
				break;
		}

		if (bSurface && i > 0)
		{
			vecEnd = vecEnd + vecDir * 8;
			vecEnd.z = EV_WaterLevel(vecEnd, vecEnd.z, vecEnd.z + 16.0) + 2.0;

			if (g_iClientDetail >= 0)
			{
				for (int j = 6; j > 0; j--)
				{
					Vector vecA = Vector(0, gEngfuncs.pfnRandomFloat(-1, 1), 0);
					Vector vecB = Vector(gEngfuncs.pfnRandomFloat(-1, 1), 0, 0);
					Vector vecSpray = (vecA + vecB).Normalize() * gEngfuncs.pfnRandomFloat(16, 32);
					EV_DrawLightning(vecEnd, vecSpray + vecEnd, 1.0, 0, 0, idx);
				}
			}
		}
		else
		{
			vecEnd = tr.endpos;
		}
	}
	else
	{
		pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
		if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP))
		{
			if (gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
			{
				char szDecal[32];
				sprintf(szDecal, "{smscorch%i", gEngfuncs.pfnRandomLong(1, 2));
				gEngfuncs.pEfxAPI->R_DecalShoot(
					gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(szDecal)),
					gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
			}

			if (g_iClientDetail >= 0)
			{
				Vector vecAngles, vecForward, vecRight, vecUp;
				VectorAngles(tr.plane.normal, vecAngles);
				vecAngles[0] = -vecAngles[0];
				vecAngles[2] = gEngfuncs.pfnRandomFloat(0, 360);
				AngleVectors(vecAngles, vecForward, vecRight, vecUp);

				CLTE_AddDiskRing(Vector(tr.endpos) + Vector(tr.plane.normal) * 2, vecUp, vecRight, vecForward, 1,
					0, 0, 0, 0, 1, 15, 6, 20,
					Vector(0.95, 0.95, 1), Vector(0.95, 0.95, 1), 0.3, 0,
					Vector(0.95, 0.95, 1), Vector(0.95, 0.95, 1), 0.9, 0.1,
					0.2, "sprites/shockwave.spr", 0, 0, 0);
			}
		}

		vecEnd = tr.endpos;
	}

	EV_DrawLightning(vecSrc, vecEnd, 3.0, 0, 1, idx);

	gEngfuncs.pEventAPI->EV_PopPMStates();
}

// pythonchain.sc: the python's chain lightning: the bolt, and when it hits a chain target (not a
// player's ellipsoid) the chain on from there, at most 8 deep. name ours
void EV_PythonChain(event_args_t* args) // orig client 0x17c80 (Ghidra-missed)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector vecSrc, vecEnd;
	Vector up, right, forward;
	pmtrace_t tr;
	int bHitPlayer = false;
	int bFlashed;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, right, up);

	CLTE_KillByEntity(idx, 14);

	if (EV_IsLocal(idx))
	{
		V_ScreenTint(Vector(230, 230, 255), Vector(230, 230, 255), 160, 0, 0.4, 0);
		gEngfuncs.pEventAPI->EV_WeaponAnimation(PYTHON_FIRE1, gEngfuncs.GetMaxClients() != 1 ? 1 : 0);
		V_PunchAxis(0, -10.0);
	}

	if (0 != args->bparam1)
	{
		switch (gEngfuncs.pfnRandomLong(0, 2))
		{
		case 0:
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/electro4.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), 0.8, 0, 100);
			break;
		case 1:
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/electro5.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), 0.8, 0, 100);
			break;
		case 2:
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/electro6.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), 0.8, 0, 100);
			break;
		}
		return;
	}

	switch (gEngfuncs.pfnRandomLong(0, 1))
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/357_shot1.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), 0.8, 0, 100);
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/357_shot2.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), 0.8, 0, 100);
		break;
	}

	EV_GetGunPosition(args, vecSrc, origin);
	vecEnd = forward * 8192 + vecSrc;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(EV_PlayerForEntity(idx) - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);

	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
	if (pe)
	{
		Vector vecHit = CLTE_ShieldPoint(pe->info, tr.endpos, forward, nullptr, &bHitPlayer);
		VectorCopy(vecHit, tr.endpos);
	}

	bFlashed = 0;

	if (gEngfuncs.PM_PointContents(tr.endpos, nullptr) == CONTENTS_WATER)
	{
		// back along the bolt to the water's surface
		Vector vecDir = forward;
		vecEnd = tr.endpos;

		int i = 127;
		bool bSurface = false;
		while (true)
		{
			if (gEngfuncs.PM_PointContents(vecEnd, nullptr) != CONTENTS_WATER)
			{
				bSurface = true;
				break;
			}
			vecEnd = vecEnd - vecDir * 8;
			if (i-- <= 0)
				break;
		}

		if (bSurface && i > 0)
		{
			vecEnd = vecEnd + vecDir * 8;
			vecEnd.z = EV_WaterLevel(vecEnd, vecEnd.z, vecEnd.z + 16.0) + 2.0;

			if (g_iClientDetail >= 0)
			{
				for (int j = 6; j > 0; j--)
				{
					Vector vecA = Vector(0, gEngfuncs.pfnRandomFloat(-1, 1), 0);
					Vector vecB = Vector(gEngfuncs.pfnRandomFloat(-1, 1), 0, 0);
					Vector vecSpray = (vecA + vecB).Normalize() * gEngfuncs.pfnRandomFloat(16, 32);
					EV_DrawLightning(vecEnd, vecSpray + vecEnd, 1.0, 0, 0, idx);
				}
			}
		}
		else
		{
			vecEnd = tr.endpos;
		}
	}
	else
	{
		pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
		if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP))
		{
			if (gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
			{
				char szDecal[32];
				sprintf(szDecal, "{smscorch%i", gEngfuncs.pfnRandomLong(1, 2));
				gEngfuncs.pEfxAPI->R_DecalShoot(
					gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(szDecal)),
					gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
			}

			if (g_iClientDetail >= 0)
			{
				Vector vecAngles, vecForward, vecRight, vecUp;
				VectorAngles(tr.plane.normal, vecAngles);
				vecAngles[0] = -vecAngles[0];
				AngleVectors(vecAngles, vecForward, vecRight, vecUp);

				CLTE_AddDiskRing(Vector(tr.endpos) + Vector(tr.plane.normal) * 2, vecUp, vecRight, vecForward, 1,
					0, 0, 0, 0, 1, 15, 6, 20,
					Vector(0.95, 0.95, 1), Vector(0.95, 0.95, 1), 0.3, 0,
					Vector(0.95, 0.95, 1), Vector(0.95, 0.95, 1), 0.9, 0.1,
					0.2, "sprites/shockwave.spr", 0, 0, 0);
			}
		}
		else
		{
			cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
			cl_entity_t* pEnt = GetEntity(gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr));
			if (pEnt && pEnt->curstate.team == 2 && !bHitPlayer)
			{
				Vector vecTarget = pEnt->origin + (Vector(pEnt->curstate.mins) + Vector(pEnt->curstate.maxs)) * 0.5;

				unsigned char* pMask = (unsigned char*)malloc((int)ceil(g_iHighestEntIndex * 0.125));
				for (int i = 0; i < (int)ceil(g_iHighestEntIndex * 0.125); i++)
					pMask[i] = 0;

				pMask[pEnt->index / 8] |= 1 << (pEnt->index % 8);
				pMask[idx / 8] |= 1 << (idx % 8);

				// original: the chain's bolts are drawn from the local player's entity, whoever fired
				EV_ChainLightning(8, vecTarget, pEnt->index, pMask, pLocal, &bFlashed);

				free(pMask);
			}
		}

		vecEnd = tr.endpos;
	}

	EV_DrawLightning(vecSrc, vecEnd, 3.0, bFlashed, 1, idx);

	gEngfuncs.pEventAPI->EV_PopPMStates();
}

// pythonprefire.sc: the python's secondary and tertiary attacks start charging (charge effect 14).
// name ours
void EV_PythonPrefire(event_args_t* args) // orig client 0x18c90 (Ghidra-missed)
{
	gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, args->origin, CHAN_WEAPON, "weapons/pyth_prefire.wav", 1.0, 0.8, 0, 100);

	if (EV_IsLocal(args->entindex))
		gEngfuncs.pEventAPI->EV_WeaponAnimation(PYTHON_IDLE1, gEngfuncs.GetMaxClients() != 1 ? 1 : 0);

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(14);
	CLTE_LoopbackByte(args->entindex);
	CLTE_LoopbackSend();
}

// pythonfireball.sc: the python fires its ball lightning (bparam1 under water: a fizzle). name ours
void EV_PythonFireball(event_args_t* args) // orig client 0x18d50
{
	int idx;
	Vector origin;
	Vector angles;
	Vector vecSrc;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, right, up);

	CLTE_KillByEntity(idx, 14);

	EV_GetGunPosition(args, vecSrc, origin);

	if (EV_IsLocal(idx))
	{
		V_ScreenTint(Vector(230, 230, 255), Vector(230, 230, 255), 160, 0, 0.33, 0);
		gEngfuncs.pEventAPI->EV_WeaponAnimation(PYTHON_FIRE1, gEngfuncs.GetMaxClients() != 1 ? 1 : 0);
		V_PunchAxis(0, -10.0);

		if (g_iClientDetail >= 0)
		{
			VectorMA(vecSrc, 26, forward, vecSrc);
			VectorMA(vecSrc, 6, right, vecSrc);
			VectorMA(vecSrc, -6, up, vecSrc);

			CLTE_AddDiskRing(vecSrc, up, right, forward, 1,
				0, 8, 2, 10, 1, 15, 4, 18,
				Vector(0.95, 0.95, 1), Vector(0.95, 0.95, 1), 0.3, 0,
				Vector(0.95, 0.95, 1), Vector(0.95, 0.95, 1), 0.9, 0.1,
				0.33, "sprites/shockwave.spr", 0, 0, 0);
		}
	}

	if (0 == args->bparam1)
	{
		switch (gEngfuncs.pfnRandomLong(0, 1))
		{
		case 0:
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/357_shot1.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), 0.8, 0, 100);
			break;
		case 1:
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/357_shot2.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), 0.8, 0, 100);
			break;
		}
	}
	else
	{
		switch (gEngfuncs.pfnRandomLong(0, 2))
		{
		case 0:
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/electro4.wav", 1.0, 0.8, 0, 100);
			break;
		case 1:
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/electro5.wav", 1.0, 0.8, 0, 100);
			break;
		case 2:
			gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/electro6.wav", 1.0, 0.8, 0, 100);
			break;
		}
	}
}

// The ball lightning impact's glow sprite: grows and fades. name ours
void EV_BallDetCallback(struct tempent_s* ent, float frametime, float currenttime) // orig client 0x19230 (Ghidra-missed)
{
	ent->entity.curstate.scale += frametime * 4.0;
	ent->entity.curstate.renderamt = (int)(ent->entity.curstate.renderamt - frametime * 320.0);
}

// pythballimpact.sc: the ball lightning bursts (iparam1 the packed direction it came from,
// bparam1 in water: no trace, the burst at the water's surface). name ours
void EV_PythBallImpact(event_args_t* args) // orig client 0x19280
{
	Vector vecDir = UTIL_UnpackNormal(args->iparam1).Normalize();
	Vector vecStart = Vector(args->origin) - vecDir * 16;
	Vector vecEnd = vecDir * 128 + vecStart;
	pmtrace_t tr;

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal)
		return;

	if (0 != args->bparam1)
	{
		vecStart.z = EV_WaterLevel(vecStart, vecStart.z, vecStart.z + 1024);

		const char* pszSound = nullptr;
		switch (gEngfuncs.pfnRandomLong(0, 2))
		{
		case 0:
			pszSound = "weapons/electro4.wav";
			break;
		case 1:
			pszSound = "weapons/electro5.wav";
			break;
		case 2:
			pszSound = "weapons/electro6.wav";
			break;
		}
		if (pszSound)
			gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, vecStart, CHAN_BODY, pszSound, 0.5, 0.8, 0, 100);
	}

	Vector view_ofs, viewangles, forward;
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);
	gEngfuncs.GetViewAngles(viewangles);
	AngleVectors(viewangles, forward, nullptr, nullptr);
	Vector vecEye = Vector(pLocal->origin[0] + view_ofs[0], pLocal->origin[1] + view_ofs[1], pLocal->origin[2] + view_ofs[2]);

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	if (0 == args->bparam1)
	{
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_BOX, -1, &tr);
	}
	else
	{
		VectorCopy(vecStart, tr.endpos);
		vecDir = Vector(0, 0, 1);
		VectorCopy(vecDir, tr.plane.normal);
		tr.fraction = 1.0;
	}

	// the macro evaluates the normalized vector once per component, as the original did
	if (DotProduct(forward, (vecStart - vecEye).Normalize()) > 0)
	{
		pmtrace_t trView;
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEye, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, &trView);
		if (trView.fraction >= 1.0 || (0 != trView.ent && gEngfuncs.pEventAPI->EV_IndexFromTrace(&trView) == pLocal->index))
			V_ScreenTint(Vector(230, 230, 255), Vector(230, 230, 255), 190, 0, 0.4, -1);
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();

	int iGlow;
	model_t* pGlow = gEngfuncs.CL_LoadModel("sprites/bldet.spr", &iGlow);
	if (pGlow)
	{
		Vector vecPos = Vector(tr.endpos) + Vector(tr.plane.normal) * 8;
		TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TentEntAllocCustom(vecPos, pGlow, 0, EV_BallDetCallback);
		if (pTemp)
		{
			pTemp->entity.curstate.framerate = 18;
			pTemp->entity.curstate.scale = 1.0;
			pTemp->flags |= FTENT_SPRANIMATE;
			pTemp->entity.curstate.renderamt = 250;
			pTemp->die += 1.0;
			pTemp->entity.curstate.rendermode = kRenderGlow;
			pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
			pTemp->entity.curstate.rendercolor.g = 230;
			pTemp->entity.curstate.rendercolor.r = 230;
			pTemp->entity.curstate.rendercolor.b = 255;
			pTemp->entity.curstate.frame = 0;
			pTemp->frameMax = 9;
		}
	}

	gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, vecStart, CHAN_WEAPON, "debris/beamstart10.wav", 1.0, 0.5, 0, 80);

	if (tr.fraction >= 1.0)
		return;

	if (g_iClientDetail >= 0)
	{
		Vector vecAngles, vecForward, vecRight, vecUp;
		VectorAngles(tr.plane.normal, vecAngles);
		vecAngles[0] = -vecAngles[0];
		AngleVectors(vecAngles, vecForward, vecRight, vecUp);

		CLTE_AddDiskRing(Vector(tr.endpos) + Vector(tr.plane.normal) * 2, vecUp, vecRight, vecForward, 3,
			0, 0, 0, 0, 4, 100, 16, 100,
			Vector(0.95, 0.95, 1), Vector(0.95, 0.95, 1), 0.3, 0,
			Vector(0.95, 0.95, 1), Vector(0.95, 0.95, 1), 0.9, 0.1,
			0.4, "sprites/shockwave.spr", 0, 0, 0);

		CLTE_AddDiskRing(Vector(tr.endpos) + Vector(tr.plane.normal) * 2, vecUp, vecRight, vecForward, 3,
			32, 0, 0, 0, 16, 100, 16, 100,
			Vector(0.95, 0.95, 1), Vector(0.95, 0.95, 1), 0.2, 0,
			Vector(0.95, 0.95, 1), Vector(0.95, 0.95, 1), 0.7, 0.1,
			0.4, "sprites/shockwave.spr", 0, 0, 0);
	}

	gEngfuncs.pfnRandomLong(4, 6); // result unused, as in the original

	Vector vecLight = Vector(tr.plane.normal) * 2 + Vector(tr.endpos);

	if (g_iClientDetail >= 0)
	{
		dlight_t* dl = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		if (dl)
		{
			VectorCopy(vecLight, dl->origin);
			dl->radius = 220;
			dl->color.r = 230;
			dl->color.g = 230;
			dl->color.b = 255;
			dl->decay = 550;
			dl->die = gEngfuncs.GetClientTime() + 0.5;
		}

		dl = gEngfuncs.pEfxAPI->CL_AllocElight(0);
		if (dl)
		{
			VectorCopy(vecLight, dl->origin);
			dl->radius = 150;
			dl->color.r = 230;
			dl->color.g = 230;
			dl->color.b = 255;
			dl->decay = 375;
			dl->die = gEngfuncs.GetClientTime() + 0.5;
		}
	}

	if (0 == args->bparam1)
	{
		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
		if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
		{
			char szDecal[32];
			sprintf(szDecal, "{scorch%i", gEngfuncs.pfnRandomLong(1, 3));
			gEngfuncs.pEfxAPI->R_DecalShoot(
				gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(szDecal)),
				gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
		}
	}
}


//======================
//	   GAUSS START
//======================

// gaussprimary.sc: the gauss's primary beam; the beam itself is the client effect of loopback
// message 33 (start, end). A spark where it hits the world. name ours
void EV_GaussPrimary(event_args_t* args) // orig client 0x1a2c0 (Ghidra-missed)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector vecSrc, vecBeam, vecEnd;
	Vector up, right, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	EV_GetGunPosition(args, vecSrc, origin);
	AngleVectors(angles, forward, right, up);

	vecBeam = vecSrc;
	VectorMA(vecBeam, 8, right, vecBeam);
	VectorMA(vecBeam, -6, up, vecBeam);
	VectorMA(vecBeam, 8, forward, vecBeam);

	VectorMA(vecSrc, 8192, forward, vecEnd);

	if (EV_IsLocal(idx))
	{
		V_PunchAxis(0, -2.0);
		gEngfuncs.pEventAPI->EV_WeaponAnimation(GAUSS_FIRE2, 2);
	}

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "debris/beamstart4.wav", gEngfuncs.pfnRandomFloat(0.9, 1.0), 0.8, 0, gEngfuncs.pfnRandomLong(95, 105));

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(EV_PlayerForEntity(idx) - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(33);
	CLTE_LoopbackCoord(vecBeam.x);
	CLTE_LoopbackCoord(vecBeam.y);
	CLTE_LoopbackCoord(vecBeam.z);
	CLTE_LoopbackCoord(tr.endpos[0]);
	CLTE_LoopbackCoord(tr.endpos[1]);
	CLTE_LoopbackCoord(tr.endpos[2]);
	CLTE_LoopbackSend();

	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
	if (!pe || (pe->solid != SOLID_BSP && pe->movetype != MOVETYPE_PUSHSTEP))
		return;

	int iSpark;
	model_t* pSpark = gEngfuncs.CL_LoadModel("sprites/xspark2.spr", &iSpark);
	if (!pSpark)
		return;

	Vector vecPos = Vector(tr.endpos) + Vector(tr.plane.normal);
	TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc(vecPos, pSpark);
	if (pTemp)
	{
		pTemp->entity.curstate.framerate = 22;
		pTemp->entity.curstate.scale = 1.0;
		pTemp->flags |= FTENT_SPRANIMATE;
		pTemp->entity.curstate.renderamt = 250;
		pTemp->entity.curstate.rendermode = kRenderGlow;
		pTemp->die += 1.0;
		pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		pTemp->entity.curstate.rendercolor.g = 230;
		pTemp->entity.curstate.rendercolor.r = 230;
		pTemp->entity.curstate.rendercolor.b = 255;
		pTemp->entity.curstate.frame = 0;
		pTemp->frameMax = 11;
	}
}

// gaussthird.sc: the gauss's tertiary beam (loopback message 36). Ends the charge effect (35);
// bparam1 2: the charge was cancelled, 1: the beam hit and leaves two rings, 0: a spark. name ours
void EV_GaussThird(event_args_t* args) // orig client 0x1a6d0
{
	int idx;
	Vector origin;
	Vector angles;
	Vector vecSrc, vecBeam, vecEnd;
	Vector up, right, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	CLTE_KillByEntity(idx, 35);

	if (args->bparam1 == 2)
		return;

	EV_GetGunPosition(args, vecSrc, origin);
	AngleVectors(angles, forward, right, up);

	vecBeam = vecSrc;
	VectorMA(vecBeam, 8, right, vecBeam);
	VectorMA(vecBeam, -6, up, vecBeam);
	VectorMA(vecBeam, 8, forward, vecBeam);

	VectorMA(vecSrc, 8192, forward, vecEnd);

	if (EV_IsLocal(idx))
	{
		V_PunchAxis(0, -2.0);
		gEngfuncs.pEventAPI->EV_WeaponAnimation(GAUSS_FIRE2, 2);
	}

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "debris/beamstart4.wav", gEngfuncs.pfnRandomFloat(0.9, 1.0), 0.8, 0, gEngfuncs.pfnRandomLong(95, 105));

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(EV_PlayerForEntity(idx) - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(36);
	CLTE_LoopbackCoord(vecBeam.x);
	CLTE_LoopbackCoord(vecBeam.y);
	CLTE_LoopbackCoord(vecBeam.z);
	CLTE_LoopbackCoord(tr.endpos[0]);
	CLTE_LoopbackCoord(tr.endpos[1]);
	CLTE_LoopbackCoord(tr.endpos[2]);
	CLTE_LoopbackSend();

	if (0 == args->bparam1)
	{
		int iSpark;
		model_t* pSpark = gEngfuncs.CL_LoadModel("sprites/xspark2.spr", &iSpark);
		if (!pSpark)
			return;

		Vector vecPos = Vector(tr.endpos) + Vector(tr.plane.normal);
		TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc(vecPos, pSpark);
		if (pTemp)
		{
			pTemp->entity.curstate.framerate = 35;
			pTemp->entity.curstate.scale = 1.2;
			pTemp->flags |= FTENT_SPRANIMATE;
			pTemp->entity.curstate.renderamt = 200;
			pTemp->entity.curstate.rendermode = kRenderGlow;
			pTemp->die += 1.0;
			pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
			pTemp->entity.curstate.rendercolor.g = 230;
			pTemp->entity.curstate.rendercolor.r = 230;
			pTemp->entity.curstate.rendercolor.b = 255;
			pTemp->entity.curstate.frame = 0;
			pTemp->frameMax = 11;
		}
		return;
	}

	Vector vecAngles, vecForward, vecRight, vecUp;
	VectorAngles(tr.plane.normal, vecAngles);
	vecAngles[0] = -vecAngles[0];
	AngleVectors(vecAngles, vecForward, vecRight, vecUp);

	// original: the first ring ignores cl_detail
	CLTE_AddDiskRing(Vector(tr.endpos) + Vector(tr.plane.normal) * 2, vecUp, vecRight, vecForward, 3,
		0, 0, 0, 0, 4, 180, 16, 184,
		Vector(0.8, 0.8, 1), Vector(0.8, 0.8, 1), 0.3, 0,
		Vector(0.8, 0.8, 1), Vector(0.8, 0.8, 1), 0.9, 0.1,
		0.4, "sprites/shockwave.spr", 0, 0, 0);

	if (g_iClientDetail < 0)
		return;

	CLTE_AddDiskRing(Vector(tr.endpos) + Vector(tr.plane.normal) * 2, vecUp, vecRight, vecForward, 3,
		16, 0, 0, 0, 16, 184, 16, 184,
		Vector(0.8, 0.8, 1), Vector(0.8, 0.8, 1), 0.3, 0,
		Vector(0.8, 0.8, 1), Vector(0.8, 0.8, 1), 0.9, 0.1,
		0.4, "sprites/shockwave.spr", 0, 0, 0);
}

// gaussbecon.sc: the gauss throws its teleport beacon. name ours
void EV_GaussBecon(event_args_t* args) // orig client 0x1aec0
{
	int idx = args->entindex;
	Vector origin;
	VectorCopy(args->origin, origin);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/rocketfire1.wav", 0.75, 0.8, 0, gEngfuncs.pfnRandomLong(95, 105));

	if (EV_IsLocal(idx))
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation(GAUSS_FIRE2, 2);
		V_PunchAxis(0, -2.0);
	}
}

// gaussprimarydeny.sc: not enough charge. name ours
void EV_GaussPrimaryDeny(event_args_t* args) // orig client 0x1af60
{
	int idx = args->entindex;
	Vector origin;
	VectorCopy(args->origin, origin);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/electro4.wav", gEngfuncs.pfnRandomFloat(0.9, 1.0), 0.8, 0, gEngfuncs.pfnRandomLong(120, 130));
}

// gausscharge.sc: the gauss's tertiary attack charges (loopback message 35: owner, bparam1).
// name ours
void EV_GaussCharge(event_args_t* args) // orig client 0x1b000 (Ghidra-missed)
{
	if (EV_IsLocal(args->entindex))
		gEngfuncs.pEventAPI->EV_WeaponAnimation(GAUSS_SPIN, 2);

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(35);
	CLTE_LoopbackByte(args->entindex);
	CLTE_LoopbackByte(args->bparam1);
	CLTE_LoopbackSend();
}

// teleportflash.sc: a gauss teleport (iparam2 the teleported entity, bparam1 3: a beacon is
// destroyed, only a small flare). Flares, a white flash for the one teleported, sparks and, in
// water, bubbles. name ours
void EV_TeleportFlash(event_args_t* args) // orig client 0x1b070 (Ghidra-missed)
{
	Vector origin;
	int iModel;
	VectorCopy(args->origin, origin);

	if (args->bparam1 == 3)
	{
		model_t* pFlare = gEngfuncs.CL_LoadModel("sprites/xflare1.spr", &iModel);
		if (pFlare)
		{
			TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc(origin, pFlare);
			if (pTemp)
			{
				pTemp->die += 2.0;
				pTemp->entity.curstate.framerate = 18;
				pTemp->flags |= FTENT_SPRANIMATE;
				pTemp->entity.curstate.scale = 0.8;
				pTemp->entity.curstate.renderamt = 250;
				pTemp->entity.curstate.rendermode = kRenderGlow;
				pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
				pTemp->entity.curstate.rendercolor.g = 255;
				pTemp->entity.curstate.rendercolor.r = 255;
				pTemp->entity.curstate.rendercolor.b = 255;
				pTemp->entity.curstate.frame = 0;
				pTemp->frameMax = 20;
			}
		}

		gEngfuncs.pEventAPI->EV_PlaySound(-1, origin, CHAN_AUTO, "weapons/electro4.wav", gEngfuncs.pfnRandomFloat(0.9, 1.0), 0.8, 0, gEngfuncs.pfnRandomLong(105, 110));
		return;
	}

	gEngfuncs.pEventAPI->EV_PlaySound(-1, origin, CHAN_AUTO, "debris/beamstart7.wav", gEngfuncs.pfnRandomFloat(0.9, 1.0), 0.8, 0, gEngfuncs.pfnRandomLong(105, 110));

	if (EV_IsLocal(args->iparam2))
		V_ScreenTint(Vector(255, 255, 255), Vector(255, 255, 255), 250, 0, 0.5, 1);

	model_t* pFlare = gEngfuncs.CL_LoadModel("sprites/xflare1.spr", &iModel);
	if (pFlare)
	{
		TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc(origin, pFlare);
		if (pTemp)
		{
			pTemp->entity.curstate.framerate = 15;
			pTemp->flags |= FTENT_SPRANIMATE;
			pTemp->entity.curstate.scale = 3.0;
			pTemp->entity.curstate.renderamt = 250;
			pTemp->die += 2.0;
			pTemp->entity.curstate.rendermode = kRenderGlow;
			pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
			pTemp->entity.curstate.rendercolor.g = 255;
			pTemp->entity.curstate.rendercolor.r = 255;
			pTemp->entity.curstate.rendercolor.b = 255;
			pTemp->entity.curstate.frame = 0;
			pTemp->frameMax = 20;
		}
	}

	if (g_iClientDetail < 0)
		return;

	if (!gEngfuncs.CL_LoadModel("sprites/redflare1.spr", &iModel))
		return;

	int cSparks = gEngfuncs.pfnRandomLong(8, 12);
	if (g_iClientDetail >= 1)
		cSparks = gEngfuncs.pfnRandomLong(12, 15);

	Vector vecCenter = args->origin;

	for (int i = cSparks; i > 0; i--)
	{
		Vector vecPos = vecCenter;
		vecPos[0] += gEngfuncs.pfnRandomFloat(-12, 12);
		vecPos[1] += gEngfuncs.pfnRandomFloat(-12, 12);
		vecPos[2] += gEngfuncs.pfnRandomFloat(-20, 20);

		Vector vecVel;
		vecVel[0] = gEngfuncs.pfnRandomFloat(-1, 1);
		vecVel[1] = gEngfuncs.pfnRandomFloat(-1, 1);
		vecVel[2] = gEngfuncs.pfnRandomFloat(-1, 1);
		vecVel = vecVel.Normalize() * gEngfuncs.pfnRandomFloat(350, 450);

		// original bug: the angles passed are never set (stack garbage); a sprite ignores them, so
		// zero here
		Vector vecAngles = g_vecZero;

		TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->R_TempModel(vecPos, vecVel, vecAngles, 3.0, iModel, 0);
		if (pTemp)
		{
			pTemp->flags |= FTENT_FADEOUT;
			pTemp->fadeSpeed = 1.0;
			pTemp->entity.curstate.scale = 0.2;
			pTemp->entity.curstate.renderamt = 250;
			pTemp->entity.baseline.renderamt = 250;
			pTemp->entity.curstate.rendermode = kRenderGlow;
			pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
			pTemp->entity.curstate.rendercolor.g = 255;
			pTemp->entity.curstate.rendercolor.r = 255;
			pTemp->entity.curstate.rendercolor.b = 255;
			pTemp->entity.curstate.frame = 0;
			pTemp->die += gEngfuncs.pfnRandomFloat(-1, 1);
		}
	}

	if (gEngfuncs.PM_PointContents(args->origin, nullptr) == CONTENTS_WATER && gEngfuncs.CL_LoadModel("sprites/bubble.spr", &iModel))
	{
		Vector vecOne(1, 1, 1);
		Vector mins, maxs;
		VectorMA(args->origin, -32, vecOne, mins);
		VectorMA(args->origin, 32, vecOne, maxs);

		Vector vecWater = args->origin;
		float flHeight = EV_WaterLevel(vecWater, args->origin[2], args->origin[2] + 1024);
		gEngfuncs.pEfxAPI->R_Bubbles(mins, maxs, flHeight - mins.z, iModel, 30, 8);
	}
}


//======================
//	   CROWBAR START
//======================
int g_iSwing;

//Only predict the miss sounds, hit sounds are still played
//server side, so players don't get the wrong idea.
// SDK 2.3's, byte for byte in RC2 (client 0x1b7f0): the first swing animation twice and the
// view model's body 1, as RC2's models need (halflife-updated drops the first call and uses body 0)
void EV_Crowbar(event_args_t* args)
{
	int idx;
	Vector origin;

	idx = args->entindex;
	VectorCopy(args->origin, origin);

	//Play Swing sound
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, PITCH_NORM);

	if (EV_IsLocal(idx))
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation(CROWBAR_ATTACK1MISS, 1);

		switch ((g_iSwing++) % 3)
		{
		case 0:
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROWBAR_ATTACK1MISS, 1);
			break;
		case 1:
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROWBAR_ATTACK2MISS, 1);
			break;
		case 2:
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROWBAR_ATTACK3MISS, 1);
			break;
		}
	}
}


//======================
//	   CROWBAR START
//======================

// crowbarswing.sc: a crowbar swing with the animation in bparam1. name ours
void EV_CrowbarSwing(event_args_t* args) // orig client 0x1b900
{
	int idx;
	Vector origin;

	idx = args->entindex;
	VectorCopy(args->origin, origin);

	//Play Swing sound
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, PITCH_NORM);

	if (EV_IsLocal(idx))
		gEngfuncs.pEventAPI->EV_WeaponAnimation(args->bparam1, 1);
}

// cbarcharge.sc: the crowbar's charge (iparam1 the player when valid, else the invoker; fparam1
// the charge; bparam1 stop; bparam2 a fresh start of the sound). A rising hum, the player's
// glow (CLTE_SetPlayerOverride) and for the local player a glow shell on the view model. name ours
void EV_CbarCharge(event_args_t* args) // orig client 0x1b990
{
	int idx = args->iparam1;
	Vector origin;
	VectorCopy(args->origin, origin);

	if (idx < 1 || idx >= gEngfuncs.GetMaxClients())
		idx = args->entindex;

	// original: a player whose index is maxclients is left out
	if (idx < 1 || idx >= gEngfuncs.GetMaxClients())
		return;

	float flCharge;
	if (args->fparam1 < 0.0)
		flCharge = 0.0;
	else if (args->fparam1 > RC2Pref(RC2P_Crowbar_MaxCharge).f)
		flCharge = RC2Pref(RC2P_Crowbar_MaxCharge).f;
	else
		flCharge = args->fparam1;
	args->fparam1 = flCharge;

	if (0 != args->bparam1 || flCharge <= 0.0)
	{
		if (EV_IsLocal(idx))
		{
			cl_entity_t* view = gEngfuncs.GetViewModel();
			if (view)
			{
				view->curstate.renderfx = kRenderFxNone;
				view->curstate.renderamt = 0;
				view->curstate.rendercolor.b = 0;
				view->curstate.rendercolor.g = 0;
				view->curstate.rendercolor.r = 0;
			}
		}

		CLTE_ClearPlayerOverride(idx);
		gEngfuncs.pEventAPI->EV_StopSound(idx, CHAN_WEAPON, "ambience/alien_cycletone.wav");
		return;
	}

	float flFrac = flCharge / RC2Pref(RC2P_Crowbar_MaxCharge).f;

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "ambience/alien_cycletone.wav", flFrac * 0.3 + 0.7, 0.8,
		0 != args->bparam2 ? SND_SPAWNING : (SND_CHANGE_VOL | SND_CHANGE_PITCH), (int)(flFrac * 80 + 120));

	Vector vecColor(flFrac * 190.0, flFrac * 190.0, flFrac * 255.0);
	CLTE_SetPlayerOverride(idx, vecColor.x, vecColor.y, vecColor.z, flFrac * 32.0);

	if (EV_IsLocal(idx))
	{
		cl_entity_t* view = gEngfuncs.GetViewModel();
		if (view)
		{
			view->curstate.renderfx = kRenderFxGlowShell;
			view->curstate.renderamt = (int)(flFrac * 24.0);
			view->curstate.rendercolor.b = (int)(flFrac * 255.0);
			view->curstate.rendercolor.r = view->curstate.rendercolor.g = (int)(flFrac * 140.0);
		}
	}
}

//======================
//	  CROSSBOW START
//======================

// xbowzoom1.sc: the crossbow's zoomed shot, an instant hit (bparam1 clip left, bparam2 ammo
// left: the reload animations). The bolt's trail is loopback message 22 (start, end, hit flesh).
// name ours
void EV_XbowZoom1(event_args_t* args) // orig client 0x1bca0
{
	int idx;
	Vector origin;
	Vector angles;
	Vector vecSrc, vecBeam, vecEnd;
	Vector up, right, forward;
	pmtrace_t tr;
	int iHit = 0;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, right, up);
	EV_GetGunPosition(args, vecSrc, origin);
	VectorMA(vecSrc, 8192, forward, vecEnd);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/xbow_fire1.wav", 1, ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0xF));
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_ITEM, "weapons/xbow_reload1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0xF));

	vecBeam = vecSrc;

	if (EV_IsLocal(idx))
	{
		if (0 != args->bparam1)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE1, 1);
		else if (0 != args->bparam2)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE3, 1);

		VectorMA(vecBeam, 6, right, vecBeam);
		VectorMA(vecBeam, -8, up, vecBeam);
	}

	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(EV_PlayerForEntity(idx) - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.fraction < 1.0)
	{
		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
		int bShield = false;
		if (pe)
		{
			Vector vecHit = CLTE_ShieldPoint(pe->info, tr.endpos, forward, nullptr, &bShield);
			VectorCopy(vecHit, tr.endpos);
		}

		cl_entity_t* pEnt = GetEntity(gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr));
		if (pEnt && 0 != pEnt->curstate.team)
		{
			if (!bShield)
			{
				switch (gEngfuncs.pfnRandomLong(0, 1))
				{
				case 0:
					gEngfuncs.pEventAPI->EV_PlaySound(gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), tr.endpos, CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM, 0, PITCH_NORM);
					break;
				case 1:
					gEngfuncs.pEventAPI->EV_PlaySound(gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), tr.endpos, CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM, 0, PITCH_NORM);
					break;
				}
			}
			iHit = 1;
		}
	}

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(22);
	CLTE_LoopbackCoord(vecBeam.x);
	CLTE_LoopbackCoord(vecBeam.y);
	CLTE_LoopbackCoord(vecBeam.z);
	CLTE_LoopbackCoord(tr.endpos[0]);
	CLTE_LoopbackCoord(tr.endpos[1]);
	CLTE_LoopbackCoord(tr.endpos[2]);
	CLTE_LoopbackByte(iHit);
	CLTE_LoopbackSend();
}

// xbowsuperzoomed.sc: the crossbow's super zoomed shot (loopback message 27, the last byte 1 for
// a flesh hit, 0 otherwise; anything else hit gets a big bullet hole). name ours
void EV_XbowSuperZoomed(event_args_t* args) // orig client 0x1c0f0 (Ghidra-missed)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector vecSrc, vecBeam, vecEnd;
	Vector up, right, forward;
	pmtrace_t tr;
	int iHit = 0;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, right, up);
	EV_GetGunPosition(args, vecSrc, origin);
	VectorMA(vecSrc, 8192, forward, vecEnd);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/xbow_fire1.wav", 1, ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0xF));
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_ITEM, "weapons/xbow_reload1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0xF));

	vecBeam = vecSrc;

	if (EV_IsLocal(idx))
	{
		if (0 != args->bparam1)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE1, 1);
		else if (0 != args->bparam2)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE3, 1);

		VectorMA(vecBeam, 6, right, vecBeam);
		VectorMA(vecBeam, -8, up, vecBeam);
		V_PunchAxis(0, -2.0);
	}

	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(EV_PlayerForEntity(idx) - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.fraction < 1.0)
	{
		cl_entity_t* pEnt = GetEntity(gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr));
		if (pEnt && 0 != pEnt->curstate.team)
		{
			switch (gEngfuncs.pfnRandomLong(0, 1))
			{
			case 0:
				gEngfuncs.pEventAPI->EV_PlaySound(gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), tr.endpos, CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM, 0, PITCH_NORM);
				break;
			case 1:
				gEngfuncs.pEventAPI->EV_PlaySound(gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), tr.endpos, CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM, 0, PITCH_NORM);
				break;
			}
			iHit = 1;
		}
		else if (gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
		{
			char szDecal[32];
			sprintf(szDecal, "{bigshot%i", gEngfuncs.pfnRandomLong(1, 5));
			gEngfuncs.pEfxAPI->R_DecalShoot(
				gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(szDecal)),
				gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
		}
	}

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(27);
	CLTE_LoopbackCoord(vecBeam.x);
	CLTE_LoopbackCoord(vecBeam.y);
	CLTE_LoopbackCoord(vecBeam.z);
	CLTE_LoopbackCoord(tr.endpos[0]);
	CLTE_LoopbackCoord(tr.endpos[1]);
	CLTE_LoopbackCoord(tr.endpos[2]);
	CLTE_LoopbackByte(iHit);
	CLTE_LoopbackSend();
}

// xbowexplozoomed.sc: the crossbow's exploding super zoomed shot (loopback message 27, the last
// byte 3 for a flesh hit, 2 otherwise). name ours
void EV_XbowExploZoomed(event_args_t* args) // orig client 0x1c570 (Ghidra-missed)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector vecSrc, vecBeam, vecEnd;
	Vector up, right, forward;
	pmtrace_t tr;
	int iHit = 2;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, right, up);
	EV_GetGunPosition(args, vecSrc, origin);
	VectorMA(vecSrc, 8192, forward, vecEnd);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/xbow_fire1.wav", 1, ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0xF));
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_ITEM, "weapons/xbow_reload1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0, 0xF));

	vecBeam = vecSrc;

	if (EV_IsLocal(idx))
	{
		if (0 != args->bparam1)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE1, 1);
		else if (0 != args->bparam2)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE3, 1);

		VectorMA(vecBeam, 6, right, vecBeam);
		VectorMA(vecBeam, -8, up, vecBeam);
		V_PunchAxis(0, -2.0);
	}

	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(EV_PlayerForEntity(idx) - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.fraction < 1.0)
	{
		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
		int bShield = false;
		if (pe)
		{
			Vector vecHit = CLTE_ShieldPoint(pe->info, tr.endpos, forward, nullptr, &bShield);
			VectorCopy(vecHit, tr.endpos);
		}

		cl_entity_t* pEnt = GetEntity(gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr));
		if (pEnt && 0 != pEnt->curstate.team)
		{
			if (!bShield)
			{
				switch (gEngfuncs.pfnRandomLong(0, 1))
				{
				case 0:
					gEngfuncs.pEventAPI->EV_PlaySound(gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), tr.endpos, CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM, 0, PITCH_NORM);
					break;
				case 1:
					gEngfuncs.pEventAPI->EV_PlaySound(gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), tr.endpos, CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM, 0, PITCH_NORM);
					break;
				}
			}
			iHit = 3;
		}
		else if (gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
		{
			char szDecal[32];
			sprintf(szDecal, "{bigshot%i", gEngfuncs.pfnRandomLong(1, 5));
			gEngfuncs.pEfxAPI->R_DecalShoot(
				gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(szDecal)),
				gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
		}
	}

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(27);
	CLTE_LoopbackCoord(vecBeam.x);
	CLTE_LoopbackCoord(vecBeam.y);
	CLTE_LoopbackCoord(vecBeam.z);
	CLTE_LoopbackCoord(tr.endpos[0]);
	CLTE_LoopbackCoord(tr.endpos[1]);
	CLTE_LoopbackCoord(tr.endpos[2]);
	CLTE_LoopbackByte(iHit);
	CLTE_LoopbackSend();
}

// xbowimpailer.sc: the crossbow fires an impaler bolt (also its exploding bolt). name ours
void EV_XbowImpailer(event_args_t* args) // orig client 0x1ca90
{
	int idx;
	Vector origin;

	idx = args->entindex;
	VectorCopy(args->origin, origin);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/xbow_fire1.wav", 1, ATTN_NORM, 0, gEngfuncs.pfnRandomLong(95, 105));
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_ITEM, "weapons/glauncher.wav", 0.7, ATTN_NORM, 0, gEngfuncs.pfnRandomLong(95, 105));

	if (EV_IsLocal(idx))
	{
		if (0 != args->bparam1)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE1, 1);
		else if (0 != args->bparam2)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE3, 1);

		V_PunchAxis(0, -9.0);
	}
}

// xbowteather.sc: the crossbow fires its tether bolt. name ours
void EV_XbowTeather(event_args_t* args) // orig client 0x1cbb0
{
	int idx;
	Vector origin;

	idx = args->entindex;
	VectorCopy(args->origin, origin);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/xbow_fire1.wav", 1, ATTN_NORM, 0, gEngfuncs.pfnRandomLong(95, 105));

	if (EV_IsLocal(idx))
	{
		if (0 != args->bparam1)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE1, 1);
		else if (0 != args->bparam2)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(CROSSBOW_FIRE3, 1);

		V_PunchAxis(0, -5.0);
	}
}

// xbowcharge.sc: the crossbow's charge (loopback message 28: owner, bparam1). name ours
void EV_XbowCharge(event_args_t* args) // orig client 0x1cc80
{
	EV_IsLocal(args->entindex); // result unused, as in the original

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(28);
	CLTE_LoopbackByte(args->entindex);
	CLTE_LoopbackByte(args->bparam1);
	CLTE_LoopbackSend();
}

// xbowcharge2.sc: the crossbow's second charge (loopback message 28 with 2). name ours
void EV_XbowCharge2(event_args_t* args) // orig client 0x1ccd0
{
	EV_IsLocal(args->entindex); // result unused, as in the original

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(28);
	CLTE_LoopbackByte(args->entindex);
	CLTE_LoopbackByte(2);
	CLTE_LoopbackSend();
}


//======================
//	    RPG START
//======================
// SDK 2.3's, byte for byte in RC2 (client 0x1cd20): body 1 (halflife-updated uses body 0)
void EV_FireRpg(event_args_t* args)
{
	int idx;
	Vector origin;

	idx = args->entindex;
	VectorCopy(args->origin, origin);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/rocketfire1.wav", 0.9, ATTN_NORM, 0, PITCH_NORM);
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_ITEM, "weapons/glauncher.wav", 0.7, ATTN_NORM, 0, PITCH_NORM);

	//Only play the weapon anims if I shot it.
	if (EV_IsLocal(idx))
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation(RPG_FIRE2, 1);

		V_PunchAxis(0, -5.0);
	}
}

// halflife-updated's egon beams; RC2's client has no egon beam code, but hud_msg.cpp still clears
// these on InitHUD
BEAM* pBeam;
BEAM* pBeam2;


//======================
//	    RPG START
//======================

// rpgsmimpact.sc: a sphere mine hits (iparam1 the packed direction, bparam1 it hit the world: a
// small scorch, bparam2 the owner). Ends its trail (32) and leaves a yellow ring. name ours
void EV_RpgSphereMineImpact(event_args_t* args) // orig client 0x1cdf0 (Ghidra-missed)
{
	Vector vecDir;
	Vector vecStart, vecEnd;
	pmtrace_t tr;
	const char* pszDecal = nullptr;

	// original bug: without bparam1 the trace isn't run and the ring sits at whatever tr.endpos held
	// on the stack; zeroed here
	memset(&tr, 0, sizeof(tr));

	CLTE_KillByEntity(args->entindex, 32);

	Vector origin = args->origin;
	vecDir = UTIL_UnpackNormal(args->iparam1);

	VectorMA(origin, -8, vecDir, vecStart);
	VectorMA(origin, 64, vecDir, vecEnd);

	gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, args->origin, CHAN_WEAPON, "debris/beamstart4.wav", 0.6, 0.8, 0, gEngfuncs.pfnRandomLong(130, 140));

	if (0 != args->bparam1)
	{
		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
		gEngfuncs.pEventAPI->EV_PushPMStates();
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(args->bparam2 - 1);
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_IGNORE, args->entindex, &tr);
		gEngfuncs.pEventAPI->EV_PopPMStates();

		if (tr.fraction < 1.0)
		{
			physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
			if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
			{
				switch (gEngfuncs.pfnRandomLong(0, 1))
				{
				case 0:
					pszDecal = "{smscorch1";
					break;
				case 1:
					pszDecal = "{smscorch2";
					break;
				}
				gEngfuncs.pEfxAPI->R_DecalShoot(
					gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)pszDecal)),
					gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
			}
		}
		else
		{
			tr.plane.normal[0] = 0;
			tr.plane.normal[1] = 0;
			tr.plane.normal[2] = 1;
		}
	}
	else
	{
		VectorCopy(vecDir, tr.plane.normal);
	}

	if (g_iClientDetail < 0)
		return;

	// original: the ring's axes come from the direction, its position from the trace
	Vector vecAngles, vecForward, vecRight, vecUp;
	VectorAngles(vecDir, vecAngles);
	vecAngles[0] = -vecAngles[0];
	AngleVectors(vecAngles, vecForward, vecRight, vecUp);

	CLTE_AddDiskRing(Vector(tr.endpos) + Vector(tr.plane.normal) * 2, vecUp, vecRight, vecForward, 0,
		0, 0, 0, 0, 1, 10, 3, 14,
		Vector(1, 1, 0.2), Vector(1, 1, 0.2), 0.3, 0,
		Vector(1, 1, 0.2), Vector(1, 1, 0.2), 0.9, 0.1,
		0.2, "sprites/shockwave.spr", 0, 0, 0);
}

//======================
//	    EGON START
//======================

// The egon tractor's state on the local client, across events. names ours
static int s_iTractorTarget = -1;		 // orig .data 0x101344cc: the entity last held (iparam2)
static float s_flTractorRefire = -1;	 // orig .data 0x101344d0
static float s_flTractorDropTime = -1;	 // orig .data 0x101344d4
static bool s_bTractorDropped = false;	 // orig .data 0x10179da2

// egontractor.sc: the egon's tractor beam (bparam1 the tertiary/drop variant, bparam2 1 start,
// 2 stop, 0 running; iparam2 the entity held, fparam1 its distance). The beam is loopback message
// 41 (owner, the target or 0x4000, the trace end). name ours
void EV_EgonTractor(event_args_t* args) // orig client 0x1d2c0 (Ghidra-missed)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector vecSrc, vecEnd;
	Vector up, right, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	EV_GetGunPosition(args, vecSrc, origin);
	AngleVectors(angles, forward, right, up);

	if (0 == args->bparam1)
	{
		if (0 == args->iparam2)
			VectorMA(vecSrc, 8192, forward, vecEnd);
		else
			VectorMA(vecSrc, args->fparam1, forward, vecEnd);
	}
	else
	{
		if (args->bparam1 != 2)
			VectorMA(vecSrc, 8192, forward, vecEnd);
		else
			VectorMA(vecSrc, args->fparam1, forward, vecEnd);
	}

	if (args->bparam2 == 2)
	{
		CLTE_KillByEntity(idx, 41);
		gEngfuncs.pEventAPI->EV_StopSound(idx, CHAN_STATIC, "weapons/egon_run3.wav");
	}

	if (EV_IsLocal(idx))
	{
		if (0 == args->bparam1)
		{
			if (s_iTractorTarget == args->iparam2 && args->bparam2 != 2)
				return;
			if (!s_bTractorDropped && args->bparam2 != 1)
				return;
			s_iTractorTarget = args->iparam2;
		}
		else if (args->bparam2 != 2)
		{
			// original: the return here needs the refire time to be both ahead of now and behind it
			const float flTime = gEngfuncs.GetClientTime();
			if (s_flTractorRefire != -1 && s_flTractorRefire > flTime && flTime > RC2Pref(RC2P_Egon_TractorDropRefirePrimary).f + s_flTractorRefire + 0.5)
				return;
			s_flTractorRefire = -1;
		}

		s_bTractorDropped = args->bparam2 != 2;

		if (0 == args->bparam1)
		{
			if (args->bparam2 == 2)
				s_flTractorDropTime = gEngfuncs.GetClientTime() + RC2Pref(RC2P_Egon_TractorDropRefirePrimary).f + 0.5;
			else
				s_flTractorDropTime = -1;
		}
		else if (s_flTractorDropTime != -1 && gEngfuncs.GetClientTime() < s_flTractorDropTime)
		{
			return;
		}
	}

	if (args->bparam2 == 1)
	{
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/egon_windup2.wav", 0.6, 0.8, 0, 125);

		if (EV_IsLocal(idx))
		{
			if (0 == args->bparam1)
				s_flTractorRefire = -1;
			gEngfuncs.pEventAPI->EV_WeaponAnimation(EGON_FIRE1 + gEngfuncs.pfnRandomLong(0, 3), 1);
		}
	}
	else if (args->bparam2 == 2)
	{
		if (EV_IsLocal(idx) && 0 == args->bparam1)
			s_flTractorRefire = gEngfuncs.GetClientTime() + RC2Pref(RC2P_Egon_TractorDropRefirePrimary).f;

		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/egon_off1.wav", 0.6, 0.8, 0, 100);
		return;
	}
	else
	{
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_STATIC, "weapons/egon_run3.wav", 0.6, 0.8, SND_CHANGE_PITCH, 125);
	}

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(EV_PlayerForEntity(idx) - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(41);
	CLTE_LoopbackByte(idx);
	CLTE_LoopbackShort(0 != args->bparam1 ? 0x4000 : args->iparam2);
	CLTE_LoopbackCoord(tr.endpos[0]);
	CLTE_LoopbackCoord(tr.endpos[1]);
	CLTE_LoopbackCoord(tr.endpos[2]);
	CLTE_LoopbackSend();
}

// egonfireshrinker.sc: the egon fires a shrinker ball. name ours
void EV_EgonFireShrinker(event_args_t* args) // orig client 0x1d760
{
	int idx;
	Vector origin;
	Vector angles;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, right, up);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "debris/beamstart8.wav", 1.0, ATTN_NORM, 0, gEngfuncs.pfnRandomLong(130, 140));

	if (EV_IsLocal(idx))
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation(EGON_IDLE1, 1);
		V_PunchAxis(0, -8.0);
	}
}

// egonimplodercharge.sc: the egon's imploder charges (charge effect 44). name ours
void EV_EgonImploderCharge(event_args_t* args) // orig client 0x1d850 (Ghidra-missed)
{
	if (EV_IsLocal(args->entindex))
		gEngfuncs.pEventAPI->EV_WeaponAnimation(EGON_FIRE1, 1);

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(44);
	CLTE_LoopbackByte(args->entindex);
	CLTE_LoopbackSend();
}

// egonimploderfire.sc: the egon fires its imploder (bparam1: out of ammo, only the charge ends).
// The shot is loopback message 45 (start, direction, the target: the chain target in front of the
// gun with the best mix of closeness and aim, in sight). name ours
void EV_EgonImploderFire(event_args_t* args) // orig client 0x1d8c0 (Ghidra-missed)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector vecSrc;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, right, up);

	CLTE_KillByEntity(idx, 44);

	if (EV_IsLocal(idx))
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation(EGON_IDLE1, 1);
		if (0 != args->bparam1)
			return;
		V_PunchAxis(0, -4.0);
		V_ScreenTint(Vector(255, 180, 0), Vector(255, 180, 0), 100, 0, 0.2, -1);
	}

	if (0 != args->bparam1)
		return;

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "debris/beamstart8.wav", 1.0, ATTN_NORM, 0, gEngfuncs.pfnRandomLong(85, 90));

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal)
		return;

	const float flMaxDist = RC2Pref(RC2P_Egon_ImploderTargetMaxDist).f;
	const float flMinDot = RC2Pref(RC2P_Egon_ImploderTargetMinDot).f;
	float flBest = -9999;

	EV_GetGunPosition(args, vecSrc, origin);

	cl_entity_t* pBest = nullptr;
	for (int i = 1; i < g_iHighestEntIndex; i++)
	{
		cl_entity_t* pEnt = GetEntity(i);
		if (!pEnt)
			continue;
		if (pEnt->curstate.messagenum < pLocal->curstate.messagenum)
			continue;
		if (pEnt->index == idx)
			continue;
		if (pEnt->curstate.team != 2)
			continue;

		Vector vecTarget = (Vector(pEnt->curstate.mins) + Vector(pEnt->curstate.maxs)) * 0.5 + pEnt->origin;
		Vector vecDelta = vecTarget - vecSrc;
		float flDist = vecDelta.Length();
		if (flDist > flMaxDist)
			continue;

		Vector vecDir;
		if (flDist > 0)
			vecDir = Vector(vecDelta.x / flDist, vecDelta.y / flDist, vecDelta.z / flDist);
		else
			vecDir = forward;

		float flDot = DotProduct(vecDir, forward);
		if (flDot < flMinDot)
			continue;

		float flNear = 1.0 - flDist / flMaxDist;
		float flAim = flDot / flMinDot;
		if (flNear < 0.0)
			flNear = 0.0;
		else if (flNear > 1.0)
			flNear = 1.0;
		if (flAim < 0.0)
			flAim = 0.0;
		else if (flAim > 1.0)
			flAim = 1.0;

		float flScore = flAim * 3.0 + flNear + flNear;
		if (flScore < flBest)
			continue;

		pmtrace_t tr;
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecTarget, PM_STUDIO_IGNORE, idx, &tr);
		if (tr.fraction < 1.0 && gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr) != pEnt->index)
			continue;

		flBest = flScore;
		pBest = pEnt;
	}

	int iTarget = 0;
	if (pBest)
		iTarget = pBest->index;

	Vector vecShot = forward * 16 + vecSrc + right * 3 - up * 6;

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(45);
	CLTE_LoopbackCoord(vecShot.x);
	CLTE_LoopbackCoord(vecShot.y);
	CLTE_LoopbackCoord(vecShot.z);
	CLTE_LoopbackShort((int)(forward.x * 32767.0f));
	CLTE_LoopbackShort((int)(forward.y * 32767.0f));
	CLTE_LoopbackShort((int)(forward.z * 32767.0f));
	CLTE_LoopbackShort(iTarget);
	CLTE_LoopbackSend();
}

// egonfirenuke.sc: the egon fires its nuke. name ours
void EV_EgonFireNuke(event_args_t* args) // orig client 0x1e0e0
{
	int idx;
	Vector origin;
	Vector angles;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, right, up);

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/rocketfire1.wav", 1.0, ATTN_NORM, 0, gEngfuncs.pfnRandomLong(95, 105));
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_BODY, "ambience/biggun3.wav", 0.4, ATTN_NORM, 0, gEngfuncs.pfnRandomLong(110, 120));

	if (EV_IsLocal(idx))
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation(EGON_IDLE1, 1);
		V_ScreenTint(Vector(255, 255, 255), Vector(255, 200, 180), 64, 0, 0.2, -5);
	}
}

// egonshrinkerimpact.sc: a shrinker ball bursts (iparam1 the packed direction it came from,
// iparam2 the entity it hit, 0 for the world). An orange flash for the one hit and for a local
// player who sees it, two rings and a light where it hit the world. name ours
void EV_EgonShrinkerImpact(event_args_t* args) // orig client 0x1e260
{
	CLTE_KillByEntity(args->entindex, 39);

	Vector vecDir = UTIL_UnpackNormal(args->iparam1).Normalize();
	Vector vecStart = Vector(args->origin) - vecDir * 16;
	Vector vecEnd = vecDir * 128 + vecStart;
	pmtrace_t tr;

	gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, vecStart, CHAN_WEAPON, "debris/beamstart10.wav", 1.0, 0.5, 0, gEngfuncs.pfnRandomLong(130, 140));

	if (args->iparam2 > 0 && EV_IsPlayer(args->iparam2) && EV_IsLocal(args->iparam2))
		V_ScreenTint(Vector(255, 180, 0), Vector(255, 180, 0), 220, 0, 0.6, 2);

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal)
		return;

	Vector view_ofs, viewangles, forward;
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);
	gEngfuncs.GetViewAngles(viewangles);
	AngleVectors(viewangles, forward, nullptr, nullptr);
	Vector vecEye = Vector(pLocal->origin[0] + view_ofs[0], pLocal->origin[1] + view_ofs[1], pLocal->origin[2] + view_ofs[2]);

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	if (0 == args->iparam2)
	{
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_BOX, -1, &tr);
	}
	else
	{
		VectorCopy(vecStart, tr.endpos);
		Vector vecUp(0, 0, 1);
		VectorCopy(vecUp, tr.plane.normal);
		tr.fraction = 1.0;
	}

	if (pLocal->index != args->iparam2)
	{
		// the macro evaluates the normalized vector once per component, as the original did
		if (DotProduct(forward, (vecStart - vecEye).Normalize()) > 0)
		{
			pmtrace_t trView;
			gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEye, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, &trView);
			if (trView.fraction >= 1.0 || (0 != trView.ent && gEngfuncs.pEventAPI->EV_IndexFromTrace(&trView) == pLocal->index))
				V_ScreenTint(Vector(255, 180, 0), Vector(255, 180, 0), 120, 0, 0.2, -2);
		}
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (0 == args->iparam2 && tr.fraction < 1.0)
	{
		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
		if (pe)
		{
			Vector vecHit = CLTE_ShieldPoint(pe->info, tr.endpos, vecDir, nullptr, nullptr);
			VectorCopy(vecHit, tr.endpos);
		}

		if (g_iClientDetail < 0)
			return;

		Vector vecAngles, vecForward, vecRight, vecUp;
		VectorAngles(tr.plane.normal, vecAngles);
		vecAngles[0] = -vecAngles[0];
		AngleVectors(vecAngles, vecForward, vecRight, vecUp);

		CLTE_AddDiskRing(Vector(tr.endpos) + Vector(tr.plane.normal) * 2, vecUp, vecRight, vecForward, 3,
			0, 0, 0, 0, 4, 72, 16, 72,
			Vector(1, 0.5, 0), Vector(1, 0.5, 0), 0.3, 0,
			Vector(1, 0.5, 0), Vector(1, 0.5, 0), 0.9, 0.1,
			0.2, "sprites/shockwave.spr", 0, 0, 0);

		CLTE_AddDiskRing(Vector(tr.endpos) + Vector(tr.plane.normal) * 2, vecUp, vecRight, vecForward, 3,
			32, 0, 0, 0, 16, 72, 16, 72,
			Vector(1, 0.5, 0), Vector(1, 0.5, 0), 0.2, 0,
			Vector(1, 0.5, 0), Vector(1, 0.5, 0), 0.7, 0.1,
			0.2, "sprites/shockwave.spr", 0, 0, 0);
	}

	if (g_iClientDetail >= 0)
	{
		dlight_t* dl = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		Vector vecLight = Vector(tr.plane.normal) * 16 + Vector(tr.endpos);
		if (dl)
		{
			VectorCopy(vecLight, dl->origin);
			dl->radius = 150;
			dl->color.r = 255;
			dl->color.g = 200;
			dl->color.b = 0;
			dl->decay = 750;
			dl->die = gEngfuncs.GetClientTime() + 0.3;
		}

		dl = gEngfuncs.pEfxAPI->CL_AllocElight(0);
		if (dl)
		{
			VectorCopy(vecLight, dl->origin);
			dl->radius = 100;
			dl->color.r = 255;
			dl->color.g = 200;
			dl->color.b = 0;
			dl->decay = 250;
			dl->die = gEngfuncs.GetClientTime() + 0.3;
		}
	}
}

// egonnukeexplode.sc: the egon's nuke goes off (iparam1 the packed direction of the surface hit,
// bparam1 in the air; iparam2 the player who steered it). Scorches, a white flash by distance and
// sight (the steering player's own the longest), loopback message 43 (the blast's position), a
// big explosion and, in water, bubbles. name ours
void EV_EgonNukeExplode(event_args_t* args) // orig client 0x1f0c0 (Ghidra-missed)
{
	Vector origin = args->origin;
	Vector vecPos = origin;
	pmtrace_t tr;
	const char* pszDecal = nullptr;

	gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, origin, CHAN_VOICE, "weapons/mortarhit.wav", 1.0, 0.33, 0, gEngfuncs.pfnRandomLong(85, 90));

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	if (0 != args->iparam1 && 0 == args->bparam1)
	{
		Vector vecDir = UTIL_UnpackNormal(args->iparam1).Normalize();
		origin = Vector(args->origin) - vecDir * 16;
		Vector vecEnd = vecDir * 128 + origin;

		gEngfuncs.pEventAPI->EV_PlayerTrace(origin, vecEnd, PM_STUDIO_BOX, -1, &tr);
		if (tr.fraction < 1.0)
			vecPos = Vector(tr.endpos) + Vector(tr.plane.normal) * 160;

		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
		switch (gEngfuncs.pfnRandomLong(0, 2))
		{
		case 0:
			pszDecal = "{scorch1";
			break;
		case 1:
			pszDecal = "{scorch2";
			break;
		case 2:
			pszDecal = "{scorch3";
			break;
		}
		if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
		{
			gEngfuncs.pEfxAPI->R_DecalShoot(
				gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)pszDecal)),
				gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
		}

		// four more scorches round the hit, along directions scattered off the surface's
		Vector vecBack = Vector(args->origin) - vecDir * 128;
		Vector vecAngles, vecForward, vecRight, vecUp;
		VectorAngles(vecDir, vecAngles);
		vecAngles[0] = -vecAngles[0];
		AngleVectors(vecAngles, vecForward, vecRight, vecUp);

		for (int i = 4; i > 0; i--)
		{
			Vector vecSpreadUp = vecUp * gEngfuncs.pfnRandomFloat(-0.9, 0.9);
			Vector vecSpreadRight = vecRight * gEngfuncs.pfnRandomFloat(-0.9, 0.9);
			Vector vecScatter = (vecDir + vecSpreadRight + vecSpreadUp).Normalize();
			vecEnd = vecScatter * 384 + vecBack;

			gEngfuncs.pEventAPI->EV_PlayerTrace(origin, vecEnd, PM_STUDIO_BOX, -1, &tr);
			if (tr.fraction < 1.0)
			{
				pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
				switch (gEngfuncs.pfnRandomLong(0, 2))
				{
				case 0:
					pszDecal = "{scorch1";
					break;
				case 1:
					pszDecal = "{scorch2";
					break;
				case 2:
					pszDecal = "{scorch3";
					break;
				}
				if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
				{
					gEngfuncs.pEfxAPI->R_DecalShoot(
						gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)pszDecal)),
						gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
				}
			}
		}
	}
	else
	{
		VectorCopy(origin, tr.endpos);
		Vector vecUp(0, 0, 1);
		VectorCopy(vecUp, tr.plane.normal);
		tr.fraction = 1.0;
	}

	// the flash and the message use the last trace: off the surface it ended on
	if (tr.fraction < 1.0)
		origin = Vector(tr.plane.normal) * 16 + Vector(tr.endpos);

	Vector view_ofs, viewangles, forward;
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);
	gEngfuncs.GetViewAngles(viewangles);
	AngleVectors(viewangles, forward, nullptr, nullptr);

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	Vector vecEye(0, 0, 0);
	if (pLocal)
		vecEye = Vector(pLocal->origin[0] + view_ofs[0], pLocal->origin[1] + view_ofs[1], pLocal->origin[2] + view_ofs[2]);

	if (EV_IsLocal(args->iparam2))
	{
		V_ScreenTint(Vector(255, 255, 255), Vector(255, 220, 200), 255, 0, 1.5, 6);
	}
	else
	{
		Vector vecDelta = vecEye - origin;
		Vector vecToEye = vecDelta;
		// original bug kept: without a local player the far branch reads pLocal->index of a null
		if (vecDelta.Length() > 1600)
		{
			pmtrace_t trView;
			gEngfuncs.pEventAPI->EV_PlayerTrace(origin, vecEye, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, &trView);
			if (trView.fraction >= 1.0 || (0 != trView.ent && gEngfuncs.pEventAPI->EV_IndexFromTrace(&trView) == pLocal->index))
			{
				// the macro evaluates the normalized vector once per component, as the original did
				if (DotProduct(forward, (origin - vecEye).Normalize()) > 0)
					V_ScreenTint(Vector(255, 255, 255), Vector(255, 220, 200), 255, 0, 1.0, 4);
				else
					V_ScreenTint(Vector(255, 255, 255), Vector(255, 220, 200), 255, 0, 0.8, 3);
			}
		}
		else if (pLocal)
		{
			pmtrace_t trView;
			if (EV_CanPenetrate(origin, vecEye, vecToEye.Normalize(), 256, &pLocal->index, &trView, 32))
				V_ScreenTint(Vector(255, 255, 255), Vector(255, 220, 200), 255, 0, 1.2, 5);
		}
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(43);
	CLTE_LoopbackCoord(origin.x);
	CLTE_LoopbackCoord(origin.y);
	CLTE_LoopbackCoord(origin.z);
	CLTE_LoopbackSend();

	int iExplode;
	if (gEngfuncs.PM_PointContents(origin, nullptr) != CONTENTS_WATER)
	{
		if (!gEngfuncs.CL_LoadModel("sprites/zerogxplode.spr", &iExplode))
			return;
	}
	else
	{
		if (!gEngfuncs.CL_LoadModel("sprites/WXplo1.spr", &iExplode))
			return;

		int iBubble;
		if (!gEngfuncs.CL_LoadModel("sprites/bubble.spr", &iBubble))
			return;

		Vector vecOne(1, 1, 1);
		Vector mins, maxs;
		VectorMA(args->origin, -64, vecOne, mins);
		VectorMA(args->origin, 64, vecOne, maxs);

		Vector vecWater = args->origin;
		float flHeight = EV_WaterLevel(vecWater, args->origin[2], args->origin[2] + 1024);
		gEngfuncs.pEfxAPI->R_Bubbles(mins, maxs, flHeight - mins.z, iBubble, 150, 8);
	}

	gEngfuncs.pEfxAPI->R_Explosion(vecPos, iExplode, 15, 20, TE_EXPLFLAG_NOPARTICLES);

	if (gEngfuncs.CL_LoadModel("sprites/fexplo.spr", &iExplode))
		gEngfuncs.pEfxAPI->R_Explosion(vecPos, iExplode, 10, 18, TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOPARTICLES);
}


//======================
//	   HORNET START
//======================

// hgunfire.sc: the hornet gun fires (fparam1 the charge, bparam1 1 the charged stream, 2 a
// stronger punch). A charge over 1 ends the charge effects; the shot is loopback message 48
// (start, end, the wall's normal, the charge, 1 when it hit no wall). name ours
void EV_HornetGunFire(event_args_t* args) // orig client 0x205d0 (Ghidra-missed)
{
	int idx;
	Vector origin;
	Vector angles;
	Vector vecSrc, vecStart, vecEnd;
	Vector up, right, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	EV_GetGunPosition(args, vecSrc, origin);
	AngleVectors(angles, forward, right, up);

	vecStart = vecSrc + forward * 4 + right * 5 + up * -8;
	vecEnd = forward * 8192 + vecSrc;

	if (args->fparam1 > 1.0)
	{
		if (EV_IsLocal(idx))
		{
			cl_entity_t* view = gEngfuncs.GetViewModel();
			if (view)
			{
				view->curstate.renderfx = kRenderFxNone;
				view->curstate.renderamt = 0;
				view->curstate.rendercolor.b = 0;
				view->curstate.rendercolor.g = 0;
				view->curstate.rendercolor.r = 0;
			}
		}

		CLTE_ClearPlayerOverride(idx);
		gEngfuncs.pEventAPI->EV_StopSound(idx, CHAN_WEAPON, "ambience/alien_purrmachine.wav");
	}

	if (EV_IsLocal(idx))
	{
		if (args->bparam1 == 2)
			V_PunchAxis(0, -3.0);
		else
			V_PunchAxis(0, (float)-gEngfuncs.pfnRandomLong(0, 2));

		gEngfuncs.pEventAPI->EV_WeaponAnimation(HGUN_SHOOT, 1);
	}

	switch (gEngfuncs.pfnRandomLong(0, 2))
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "agrunt/ag_fire1.wav", 1, ATTN_NORM, 0, 100);
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "agrunt/ag_fire2.wav", 1, ATTN_NORM, 0, 100);
		break;
	case 2:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "agrunt/ag_fire3.wav", 1, ATTN_NORM, 0, 100);
		break;
	}

	if (0 != args->bparam1)
		return;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(EV_PlayerForEntity(idx) - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	physent_t* pe = nullptr;
	if (tr.fraction < 1.0)
		pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);

	int bShield = false;
	if (pe)
	{
		Vector vecHit = CLTE_ShieldPoint(pe->info, tr.endpos, forward, tr.plane.normal, &bShield);
		VectorCopy(vecHit, tr.endpos);
	}

	const bool bWall = pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP);

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(48);
	CLTE_LoopbackCoord(vecStart.x);
	CLTE_LoopbackCoord(vecStart.y);
	CLTE_LoopbackCoord(vecStart.z);
	CLTE_LoopbackCoord(tr.endpos[0]);
	CLTE_LoopbackCoord(tr.endpos[1]);
	CLTE_LoopbackCoord(tr.endpos[2]);
	if (bWall || bShield)
	{
		CLTE_LoopbackShort((int)(tr.plane.normal[0] * 32767.0f));
		CLTE_LoopbackShort((int)(tr.plane.normal[1] * 32767.0f));
		CLTE_LoopbackShort((int)(tr.plane.normal[2] * 32767.0f));
	}
	else
	{
		CLTE_LoopbackShort(0);
		CLTE_LoopbackShort(0);
		CLTE_LoopbackShort(0);
	}

	float flCharge;
	if (args->fparam1 < 1.0)
		flCharge = 1.0;
	else if (args->fparam1 > 2.55)
		flCharge = 2.55;
	else
		flCharge = args->fparam1;
	CLTE_LoopbackByte((int)(flCharge * 100));

	CLTE_LoopbackByte(bWall ? 0 : 1);
	CLTE_LoopbackSend();
}

// hgunsetcharge.sc: the hornet gun's charge (iparam1 the player when valid, else the invoker;
// fparam1 the charge 0-1; bparam1 stop; bparam2 deploy: the draw animation). A purr, the player's
// glow and the view model's glow shell. name ours
void EV_HornetGunSetCharge(event_args_t* args) // orig client 0x20d00
{
	int idx = args->iparam1;
	Vector origin;
	VectorCopy(args->origin, origin);

	if (idx < 1 || idx >= gEngfuncs.GetMaxClients())
		idx = args->entindex;

	// original: a player whose index is maxclients is left out
	if (idx < 1 || idx >= gEngfuncs.GetMaxClients())
		return;

	if (0 != args->bparam2)
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation(HGUN_IDLE1, 1);
	}
	else if (0 != args->bparam1 || args->fparam1 <= 0.0)
	{
		if (EV_IsLocal(idx))
		{
			cl_entity_t* view = gEngfuncs.GetViewModel();
			if (view)
			{
				view->curstate.renderfx = kRenderFxNone;
				view->curstate.renderamt = 0;
				view->curstate.rendercolor.b = 0;
				view->curstate.rendercolor.g = 0;
				view->curstate.rendercolor.r = 0;
			}
		}

		CLTE_ClearPlayerOverride(idx);
		gEngfuncs.pEventAPI->EV_StopSound(idx, CHAN_WEAPON, "ambience/alien_purrmachine.wav");
		return;
	}

	const float flCharge = args->fparam1;

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "ambience/alien_purrmachine.wav", flCharge * 0.3 + 0.7, 0.8,
		SND_CHANGE_VOL | SND_CHANGE_PITCH, (int)(flCharge * 60 + 120));

	Vector vecColor(flCharge * 220.0, flCharge * 220.0, flCharge * 255.0);
	CLTE_SetPlayerOverride(idx, vecColor.x, vecColor.y, vecColor.z, flCharge * 16.0);

	if (EV_IsLocal(idx))
	{
		cl_entity_t* view = gEngfuncs.GetViewModel();
		if (view)
		{
			view->curstate.renderfx = kRenderFxGlowShell;
			view->curstate.renderamt = (int)(flCharge * 12.0);
			view->curstate.rendercolor.b = (int)(flCharge * 255.0);
			view->curstate.rendercolor.r = view->curstate.rendercolor.g = (int)(flCharge * 220.0);
		}
	}
}

// hgungrenadedet.sc: the hornet gun's freeze grenade goes off (iparam1 the packed direction):
// two icy rings, a flare and a zap. name ours
void EV_HornetGrenadeDet(event_args_t* args) // orig client 0x20f70
{
	Vector origin;
	VectorCopy(args->origin, origin);

	// original bug: the effects let go of an entity numbered by the packed direction
	CLTE_KillByEntity(args->iparam1, 49);

	Vector vecDir = UTIL_UnpackNormal(args->iparam1);
	VectorMA(origin, 24, vecDir, origin);

	CLTE_AddDiskRing(origin, Vector(1, 0, 0), Vector(0, 1, 0), Vector(0, 0, 1), 3,
		18, 8, 0, 0, 1, 220, 16, 256,
		Vector(0.8, 0.8, 1), Vector(0.8, 0.8, 1), 0.3, 0,
		Vector(0.8, 0.8, 1), Vector(0.8, 0.8, 1), 0.9, 0.1,
		0.5, "sprites/shockwave.spr", 0, 0, 0);

	CLTE_AddDiskRing(origin, Vector(1, 0, 0), Vector(0, 1, 0), Vector(0, 0, 1), 3,
		-18, -8, 0, 0, 1, 220, 16, 256,
		Vector(0.8, 0.8, 1), Vector(0.8, 0.8, 1), 0.3, 0,
		Vector(0.8, 0.8, 1), Vector(0.8, 0.8, 1), 0.9, 0.1,
		0.5, "sprites/shockwave.spr", 0, 0, 0);

	int iFlare;
	model_t* pFlare = gEngfuncs.CL_LoadModel("sprites/freezedet.spr", &iFlare);
	if (pFlare)
	{
		TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc(origin, pFlare);
		if (pTemp)
		{
			pTemp->entity.curstate.framerate = 18;
			pTemp->entity.curstate.scale = 0.8;
			pTemp->flags |= FTENT_SPRANIMATE;
			pTemp->entity.curstate.renderamt = 250;
			pTemp->entity.curstate.rendermode = kRenderGlow;
			pTemp->die += 2.0;
			pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
			pTemp->entity.curstate.rendercolor.g = 200;
			pTemp->entity.curstate.rendercolor.r = 200;
			pTemp->entity.curstate.rendercolor.b = 255;
			pTemp->entity.curstate.frame = 0;
			pTemp->frameMax = 9;
		}
	}

	gEngfuncs.pEventAPI->EV_PlaySound(-1, origin, CHAN_AUTO, "weapons/gauss2.wav", 1.0, 0.8, 0, gEngfuncs.pfnRandomLong(110, 120));
}

//======================
//	   TRIPMINE START
//======================
//We only check if it's possible to put a trip mine
//and if it is, then we play the animation. Server still places it.
// orig client 0x21460 (Ghidra-missed). RC2: the players from EV_PlayerForEntity, the animation
// after the PM states are popped
void EV_TripmineFire(event_args_t* args)
{
	int idx;
	Vector vecSrc, angles, view_ofs, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy(args->origin, vecSrc);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, NULL, NULL);

	if (!EV_IsLocal(idx))
		return;

	// Grab predicted result for local player
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);

	vecSrc = vecSrc + view_ofs;

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(EV_PlayerForEntity(idx) - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecSrc + forward * 128, PM_NORMAL, -1, &tr);

	gEngfuncs.pEventAPI->EV_PopPMStates();

	//Hit something solid
	if (tr.fraction < 1.0)
		gEngfuncs.pEventAPI->EV_WeaponAnimation(TRIPMINE_DRAW, 0);
}
//======================
//	   TRIPMINE END
//======================

//======================
//	   SQUEAK START
//======================
// orig client 0x21640 (Ghidra-missed). RC2: the players from EV_PlayerForEntity
void EV_SnarkFire(event_args_t* args)
{
	int idx;
	Vector vecSrc, angles, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy(args->origin, vecSrc);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, NULL, NULL);

	if (!EV_IsLocal(idx))
		return;

	if (0 != args->ducking)
		vecSrc = vecSrc - (VEC_HULL_MIN - VEC_DUCK_HULL_MIN);

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(EV_PlayerForEntity(idx) - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc + forward * 20, vecSrc + forward * 64, PM_NORMAL, -1, &tr);

	//Find space to drop the thing.
	if (tr.allsolid == 0 && tr.startsolid == 0 && tr.fraction > 0.25)
		gEngfuncs.pEventAPI->EV_WeaponAnimation(SQUEAK_THROW, 0);

	gEngfuncs.pEventAPI->EV_PopPMStates();
}

// snarkcamplace.sc: the snark's camera is placed; as EV_TripmineFire with the throw animation.
// name ours
void EV_SnarkCamPlace(event_args_t* args) // orig client 0x21930 (Ghidra-missed)
{
	int idx;
	Vector vecSrc, angles, view_ofs, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy(args->origin, vecSrc);
	VectorCopy(args->angles, angles);

	AngleVectors(angles, forward, NULL, NULL);

	if (!EV_IsLocal(idx))
		return;

	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);

	vecSrc = vecSrc + view_ofs;

	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(EV_PlayerForEntity(idx) - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecSrc + forward * 128, PM_NORMAL, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.fraction < 1.0)
		gEngfuncs.pEventAPI->EV_WeaponAnimation(SQUEAK_THROW, 0);
}

// snarkprojecthit.sc: a snark camera's projectile hits (iparam1 the packed direction, bparam1 it
// hit the world: a bullet hole, bparam2 the owner). Ends its trail (62) and leaves a red ring.
// name ours
void EV_SnarkProjectHit(event_args_t* args) // orig client 0x21b10 (Ghidra-missed)
{
	Vector vecDir;
	Vector vecStart, vecEnd;
	pmtrace_t tr;
	const char* pszDecal = nullptr;

	// original bug: without bparam1 the trace isn't run and the ring sits at whatever tr.endpos held
	// on the stack; zeroed here
	memset(&tr, 0, sizeof(tr));

	CLTE_KillByEntity(args->entindex, 62);

	Vector origin = args->origin;
	vecDir = UTIL_UnpackNormal(args->iparam1);

	VectorMA(origin, -8, vecDir, vecStart);
	VectorMA(origin, 64, vecDir, vecEnd);

	gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, args->origin, CHAN_WEAPON, "debris/beamstart11.wav", 0.6, 0.8, 0, gEngfuncs.pfnRandomLong(130, 140));

	if (0 != args->bparam1)
	{
		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
		gEngfuncs.pEventAPI->EV_PushPMStates();
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(args->bparam2 - 1);
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_IGNORE, args->entindex, &tr);
		gEngfuncs.pEventAPI->EV_PopPMStates();

		if (tr.fraction < 1.0)
		{
			physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
			if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
			{
				switch (gEngfuncs.pfnRandomLong(0, 4))
				{
				case 0:
					pszDecal = "{bigshot1";
					break;
				case 1:
					pszDecal = "{bigshot2";
					break;
				case 2:
					pszDecal = "{bigshot3";
					break;
				case 3:
					pszDecal = "{bigshot4";
					break;
				case 4:
					pszDecal = "{bigshot5";
					break;
				}
				gEngfuncs.pEfxAPI->R_DecalShoot(
					gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)pszDecal)),
					gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
			}
		}
		else
		{
			tr.plane.normal[0] = 0;
			tr.plane.normal[1] = 0;
			tr.plane.normal[2] = 1;
		}
	}
	else
	{
		VectorCopy(vecDir, tr.plane.normal);
	}

	if (g_iClientDetail < 0)
		return;

	// original: the ring's axes come from the direction, its position from the trace
	Vector vecAngles, vecForward, vecRight, vecUp;
	VectorAngles(vecDir, vecAngles);
	vecAngles[0] = -vecAngles[0];
	AngleVectors(vecAngles, vecForward, vecRight, vecUp);

	CLTE_AddDiskRing(Vector(tr.endpos) + Vector(tr.plane.normal) * 2, vecUp, vecRight, vecForward, 0,
		0, 0, 0, 0, 1, 16, 4, 16,
		Vector(1, 0, 0), Vector(1, 0, 0), 0.3, 0,
		Vector(1, 0, 0), Vector(1, 0, 0), 0.9, 0.1,
		0.3, "sprites/shockwave.spr", 0, 0, 0);
}

// minigruntblast.sc: a mini grunt (snark) pops (iparam1 the packed direction, bparam2 the owner):
// blood, gibs flying on along it and a blood decal behind. name ours
void EV_MiniGruntBlast(event_args_t* args) // orig client 0x21ff0 (Ghidra-missed)
{
	static model_t* s_pGibModel = nullptr; // name ours (orig .data 0x10179da4)

	Vector vecDir = UTIL_UnpackNormal(args->iparam1);
	int iBlood, iBloodSpray, iGibs;

	s_pGibModel = gEngfuncs.CL_LoadModel("models/hgibs.mdl", &iGibs);
	if (!s_pGibModel)
		return;

	if (!gEngfuncs.CL_LoadModel("sprites/blood.spr", &iBlood))
		return;
	if (!gEngfuncs.CL_LoadModel("sprites/bloodspray.spr", &iBloodSpray))
		return;

	gEngfuncs.pEfxAPI->R_BloodSprite(args->origin, BLOOD_COLOR_RED, iBloodSpray, iBlood, 15);
	gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, args->origin, CHAN_WEAPON, "squeek/sqk_blast1.wav", 0.7, 0.8, 0, 100);

	if (!gEngfuncs.GetEntityByIndex(args->entindex))
		return;

	Vector vecNormal = vecDir.Normalize();

	if (g_iClientDetail >= 0)
	{
		for (int i = 4; i > 0; i--)
		{
			Vector vecOrigin = args->origin;

			Vector vecGib;
			vecGib.x = gEngfuncs.pfnRandomFloat(-0.4, 0.4) + vecNormal.x;
			vecGib.y = gEngfuncs.pfnRandomFloat(-0.4, 0.4) + vecNormal.y;
			vecGib.z = gEngfuncs.pfnRandomFloat(-0.4, 0.4) + vecNormal.z;
			vecGib = vecGib.Normalize();

			Vector vecUnused = vecGib * 4 + vecOrigin; // computed and never used, as in the original
			(void)vecUnused;

			Vector vecVel = vecGib * gEngfuncs.pfnRandomFloat(200, 300);

			TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc(args->origin, s_pGibModel);
			if (pTemp)
			{
				VectorCopy(args->origin, pTemp->entity.curstate.origin);
				VectorCopy(vecVel, pTemp->entity.baseline.origin);
				pTemp->entity.curstate.body = gEngfuncs.pfnRandomLong(1, 5);
				pTemp->flags |= FTENT_GRAVITY | FTENT_ROTATE | FTENT_COLLIDEWORLD | FTENT_FADEOUT | FTENT_COLLIDEKILL;
				pTemp->entity.curstate.scale = 0.4;
				pTemp->entity.baseline.angles[0] = gEngfuncs.pfnRandomFloat(-300, 300);
				pTemp->entity.baseline.angles[1] = gEngfuncs.pfnRandomFloat(-300, 300);
				pTemp->entity.baseline.angles[2] = gEngfuncs.pfnRandomFloat(-300, 300);
				pTemp->die += 4.0;
				pTemp->entity.baseline.renderamt = 255;
				pTemp->entity.curstate.rendermode = kRenderNormal;
				pTemp->fadeSpeed = 1.0;
			}
		}
	}

	// a blood decal on the wall behind
	Vector vecBack = -vecNormal;
	Vector vecSplat;
	vecSplat.x = gEngfuncs.pfnRandomFloat(-0.3, 0.3) + vecBack.x;
	vecSplat.y = gEngfuncs.pfnRandomFloat(-0.3, 0.3) + vecBack.y;
	vecSplat.z = gEngfuncs.pfnRandomFloat(-0.3, 0.3) + vecBack.z;
	vecSplat = vecSplat.Normalize();

	Vector vecStart = args->origin;
	Vector vecEnd = vecSplat * 16 + vecStart;
	pmtrace_t tr;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(args->bparam2 - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_IGNORE, args->entindex, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (tr.fraction >= 1.0)
		return;

	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
	if (!pe || (pe->solid != SOLID_BSP && pe->movetype != MOVETYPE_PUSHSTEP))
		return;
	if (gEngfuncs.pfnGetCvarFloat("r_decals") == 0)
		return;

	const char* pszDecal = nullptr;
	switch (gEngfuncs.pfnRandomLong(0, 5))
	{
	case 0:
		pszDecal = "{blood1";
		break;
	case 1:
		pszDecal = "{blood2";
		break;
	case 2:
		pszDecal = "{blood3";
		break;
	case 3:
		pszDecal = "{blood4";
		break;
	case 4:
		pszDecal = "{blood5";
		break;
	case 5:
		pszDecal = "{blood6";
		break;
	}
	gEngfuncs.pEfxAPI->R_DecalShoot(
		gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)pszDecal)),
		gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
}


//======================
//	   GRENADES START
//======================

// gravgrenadeglow.sc: the gravity grenade's flare while it pulls. name ours
void EV_GravGrenadeGlow(event_args_t* args) // orig client 0x228b0
{
	Vector origin;
	VectorCopy(args->origin, origin);

	int iFlare;
	model_t* pFlare = gEngfuncs.CL_LoadModel("sprites/flare1.spr", &iFlare);
	if (pFlare)
	{
		TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc(origin, pFlare);
		if (pTemp)
		{
			pTemp->fadeSpeed = 4.5;
			pTemp->entity.curstate.scale = 13;
			pTemp->flags |= FTENT_FADEOUT;
			pTemp->entity.curstate.renderamt = 133;
			pTemp->entity.curstate.rendermode = kRenderGlow;
			pTemp->die += 4.5;
			pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
			pTemp->entity.curstate.rendercolor.r = 255;
			pTemp->entity.curstate.rendercolor.g = 255;
			pTemp->entity.curstate.rendercolor.b = 255;
			pTemp->entity.curstate.frame = 0;
		}
	}

	// original: what follows has no effect (a flash test left without its test)
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (pLocal)
	{
		Vector view_ofs, viewangles, forward;
		gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);
		gEngfuncs.GetViewAngles(viewangles);
		AngleVectors(viewangles, forward, nullptr, nullptr);

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
		gEngfuncs.pEventAPI->EV_PushPMStates();
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PopPMStates();
	}
}

// gravgrenadedet.sc: the gravity grenade (bparam1 0: it starts pulling, a spinning sprite and its
// hum; 1: the final blast, a flare, sparks and loopback message 51, a scorch below). Either way a
// white flash for a local player who sees it, and bubbles in water. name ours
void EV_GravGrenadeDet(event_args_t* args) // orig client 0x22a20 (Ghidra-missed)
{
	Vector origin;
	int iModel;
	VectorCopy(args->origin, origin);

	if (0 == args->bparam1)
	{
		model_t* pModel = gEngfuncs.CL_LoadModel("sprites/gravgren.spr", &iModel);
		if (pModel)
		{
			TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc(origin, pModel);
			if (pTemp)
			{
				pTemp->entity.curstate.framerate = 50;
				pTemp->die += 4.5;
				pTemp->flags |= FTENT_SPRCYCLE;
				pTemp->entity.curstate.scale = 2;
				pTemp->entity.curstate.renderamt = 250;
				pTemp->entity.curstate.rendermode = kRenderGlow;
				pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
				pTemp->entity.curstate.rendercolor.r = 150;
				pTemp->entity.curstate.rendercolor.g = 150;
				pTemp->entity.curstate.rendercolor.b = 150;
				pTemp->entity.curstate.frame = 0;
				pTemp->frameMax = 25;
			}

			gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, origin, CHAN_VOICE, "weapons/gravgren.wav", 1.0, 0.75, 0, 100);
		}
	}
	else
	{
		if (gEngfuncs.CL_LoadModel("sprites/xflare1.spr", &iModel))
			gEngfuncs.pEfxAPI->R_Explosion(origin, iModel, 2.0, 15, TE_EXPLFLAG_NOPARTICLES);

		if (g_iClientDetail >= 0 && gEngfuncs.CL_LoadModel("sprites/blueflare1.spr", &iModel))
		{
			int cSparks = gEngfuncs.pfnRandomLong(6, 8);
			if (g_iClientDetail >= 1)
				cSparks = gEngfuncs.pfnRandomLong(10, 12);

			Vector vecCenter = args->origin;

			for (int i = cSparks; i > 0; i--)
			{
				Vector vecPos = vecCenter;
				vecPos[0] += gEngfuncs.pfnRandomFloat(-12, 12);
				vecPos[1] += gEngfuncs.pfnRandomFloat(-12, 12);
				vecPos[2] += gEngfuncs.pfnRandomFloat(-20, 20);

				Vector vecVel;
				vecVel[0] = gEngfuncs.pfnRandomFloat(-1, 1);
				vecVel[1] = gEngfuncs.pfnRandomFloat(-1, 1);
				vecVel[2] = gEngfuncs.pfnRandomFloat(-1, 1);
				vecVel = vecVel.Normalize() * gEngfuncs.pfnRandomFloat(350, 450);

				// original bug: the angles passed are never set (stack garbage); a sprite ignores
				// them, so zero here
				Vector vecAngles = g_vecZero;

				TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->R_TempModel(vecPos, vecVel, vecAngles, 3.0, iModel, 0);
				if (pTemp)
				{
					pTemp->flags |= FTENT_FADEOUT;
					pTemp->fadeSpeed = 1.0;
					pTemp->entity.curstate.scale = 0.2;
					pTemp->entity.curstate.renderamt = 250;
					pTemp->entity.baseline.renderamt = 250;
					pTemp->entity.curstate.rendermode = kRenderGlow;
					pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
					pTemp->entity.curstate.rendercolor.g = 255;
					pTemp->entity.curstate.rendercolor.r = 255;
					pTemp->entity.curstate.rendercolor.b = 255;
					pTemp->entity.curstate.frame = 0;
					pTemp->die += gEngfuncs.pfnRandomFloat(-1, 1);
				}
			}
		}

		CLTE_LoopbackBegin();
		CLTE_LoopbackByte(51);
		CLTE_LoopbackCoord(args->origin[0]);
		CLTE_LoopbackCoord(args->origin[1]);
		CLTE_LoopbackCoord(args->origin[2]);
		CLTE_LoopbackSend();
	}

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal)
		return;

	Vector view_ofs, viewangles, forward;
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);
	gEngfuncs.GetViewAngles(viewangles);
	AngleVectors(viewangles, forward, nullptr, nullptr);
	Vector vecEye = Vector(pLocal->origin[0] + view_ofs[0], pLocal->origin[1] + view_ofs[1], pLocal->origin[2] + view_ofs[2]);

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	pmtrace_t tr;

	// the macro evaluates the normalized vector once per component, as the original did
	if (DotProduct(forward, (origin - vecEye).Normalize()) > 0)
	{
		gEngfuncs.pEventAPI->EV_PlayerTrace(origin, vecEye, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, &tr);
		if (tr.fraction >= 1.0 || (0 != tr.ent && gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr) == pLocal->index))
		{
			if (0 != args->bparam1)
				V_ScreenTint(Vector(255, 255, 255), Vector(255, 255, 255), 200, 0, 0.3, 0);
			else
				V_ScreenTint(Vector(255, 255, 255), Vector(255, 255, 255), 200, 0, 0.25, 0);
		}
	}

	if (0 != args->bparam1)
	{
		Vector vecDown = Vector(origin.x, origin.y, origin.z - 64);
		gEngfuncs.pEventAPI->EV_PlayerTrace(origin, vecDown, PM_STUDIO_IGNORE, args->entindex, &tr);
		if (tr.fraction < 1.0)
		{
			physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
			const char* pszDecal = "{scorch1";
			switch (gEngfuncs.pfnRandomLong(0, 2))
			{
			case 0:
				pszDecal = "{scorch1";
				break;
			case 1:
				pszDecal = "{scorch2";
				break;
			case 2:
				pszDecal = "{scorch3";
				break;
			}
			if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
			{
				gEngfuncs.pEfxAPI->R_DecalShoot(
					gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)pszDecal)),
					gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
			}
		}
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (gEngfuncs.PM_PointContents(args->origin, nullptr) == CONTENTS_WATER && gEngfuncs.CL_LoadModel("sprites/bubble.spr", &iModel))
	{
		Vector vecOne(1, 1, 1);
		Vector mins, maxs;
		VectorMA(args->origin, -32, vecOne, mins);
		VectorMA(args->origin, 32, vecOne, maxs);

		float flHeight = EV_WaterLevel(origin, origin.z, origin.z + 1024);
		gEngfuncs.pEfxAPI->R_Bubbles(mins, maxs, flHeight - mins.z, iModel, 20, 8);
	}
}

// afgrendet.sc: the antiforce grenade goes off (bparam1: 32 units higher). Loopback message 54
// and a teleport sprite. name ours
void EV_AFGrenadeDet(event_args_t* args) // orig client 0x23710
{
	int idx = args->entindex;
	Vector origin;
	VectorCopy(args->origin, origin);

	if (0 != args->bparam1)
		origin.z += 32;

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "debris/beamstart10.wav", 1.0, 0.6, 0, gEngfuncs.pfnRandomLong(110, 120));
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_VOICE, "debris/beamstart4.wav", 1.0, 0.6, 0, gEngfuncs.pfnRandomLong(80, 90));

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(54);
	CLTE_LoopbackCoord(origin.x);
	CLTE_LoopbackCoord(origin.y);
	CLTE_LoopbackCoord(origin.z + 1.0);
	CLTE_LoopbackSend();

	int iSprite;
	model_t* pSprite = gEngfuncs.CL_LoadModel("sprites/c-tele1.spr", &iSprite);
	if (pSprite)
	{
		TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc(origin, pSprite);
		if (pTemp)
		{
			pTemp->entity.curstate.framerate = 40;
			pTemp->entity.curstate.scale = 1.7;
			pTemp->flags |= FTENT_SPRANIMATE;
			pTemp->entity.curstate.renderamt = 250;
			pTemp->entity.curstate.rendermode = kRenderGlow;
			pTemp->die += 5.0;
			pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
			pTemp->entity.curstate.rendercolor.r = 255;
			pTemp->entity.curstate.rendercolor.g = 200;
			pTemp->entity.curstate.rendercolor.b = 255;
			pTemp->entity.curstate.frame = 0;
			pTemp->frameMax = 25;
		}
	}
}

// ultgrendet.sc: the ultima grenade goes off: scorches thrown all round (more with cl_detail), a
// white flash for a local player the blast reaches through at most UltimaGrenBlastMaxPunch of
// wall, loopback message 56 and the explosion. Ends its effects (55). name ours
void EV_UltimaGrenadeDet(event_args_t* args) // orig client 0x238f0 (Ghidra-missed)
{
	Vector origin = args->origin;
	Vector vecBlast = origin;
	pmtrace_t tr;
	const char* pszDecal = nullptr;

	CLTE_KillByEntity(args->entindex, 55);

	gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, origin, CHAN_VOICE, "weapons/mortarhit.wav", 1.0, 0.33, 0, gEngfuncs.pfnRandomLong(85, 90));

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	int cScorches;
	if (g_iClientDetail < 0)
		cScorches = gEngfuncs.pfnRandomLong(8, 10);
	else if (g_iClientDetail == 0)
		cScorches = gEngfuncs.pfnRandomLong(12, 16);
	else
		cScorches = gEngfuncs.pfnRandomLong(18, 24);

	for (int i = cScorches; i > 0; i--)
	{
		// up to five tries for a direction that hits something within 512; after four, downwards
		int j;
		for (j = 0; j < 5; j++)
		{
			Vector vecDir;
			if (j < 4)
			{
				vecDir.z = gEngfuncs.pfnRandomFloat(-1, 1);
				vecDir.y = gEngfuncs.pfnRandomFloat(-1, 1);
				vecDir.x = gEngfuncs.pfnRandomFloat(-1, 1);
			}
			else
			{
				vecDir.z = gEngfuncs.pfnRandomFloat(-1, -0.25);
				vecDir.y = gEngfuncs.pfnRandomFloat(-1, 1);
				vecDir.x = gEngfuncs.pfnRandomFloat(-1, 1);
			}
			vecDir = vecDir.Normalize();

			Vector vecEnd = vecDir * 512 + origin;
			gEngfuncs.pEventAPI->EV_PlayerTrace(origin, vecEnd, PM_STUDIO_BOX, -1, &tr);
			if (tr.fraction < 1.0)
				break;
		}
		if (j >= 5)
			continue;

		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
		switch (gEngfuncs.pfnRandomLong(0, 2))
		{
		case 0:
			pszDecal = "{scorch1";
			break;
		case 1:
			pszDecal = "{scorch2";
			break;
		case 2:
			pszDecal = "{scorch3";
			break;
		}
		if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
		{
			gEngfuncs.pEfxAPI->R_DecalShoot(
				gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)pszDecal)),
				gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
		}
	}

	Vector view_ofs, viewangles, forward;
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);
	gEngfuncs.GetViewAngles(viewangles);
	AngleVectors(viewangles, forward, nullptr, nullptr);

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	Vector vecEye(0, 0, 0);
	if (pLocal)
		vecEye = Vector(pLocal->origin[0] + view_ofs[0], pLocal->origin[1] + view_ofs[1], pLocal->origin[2] + view_ofs[2]);

	Vector vecToEye = vecEye - origin;
	if (pLocal)
	{
		pmtrace_t trView;
		if (EV_CanPenetrate(origin, vecEye, vecToEye.Normalize(), RC2Pref(RC2P_Grenade_UltimaGrenBlastMaxPunch).f, &pLocal->index, &trView, 32))
			V_ScreenTint(Vector(255, 255, 255), Vector(220, 220, 255), 255, 0, 1.25, 10);
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(56);
	CLTE_LoopbackCoord(origin.x);
	CLTE_LoopbackCoord(origin.y);
	CLTE_LoopbackCoord(origin.z);
	CLTE_LoopbackSend();

	int iExplode;
	if (gEngfuncs.CL_LoadModel("sprites/zerogxplode.spr", &iExplode))
	{
		gEngfuncs.pEfxAPI->R_Explosion(vecBlast, iExplode, 15, 20, TE_EXPLFLAG_NOPARTICLES);

		if (gEngfuncs.CL_LoadModel("sprites/fexplo.spr", &iExplode))
			gEngfuncs.pEfxAPI->R_Explosion(vecBlast, iExplode, 10, 18, TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOPARTICLES);
	}
}

// concchgexp.sc: the concussion charge (satchel) goes off (iparam1 the packed surface normal,
// iparam2 the player who set it off): a scorch, the flash by distance and sight, loopback message
// 61 (the blast's position and direction) and a big flare. name ours
void EV_ConcChargeExplode(event_args_t* args) // orig client 0x24110 (Ghidra-missed)
{
	Vector origin = args->origin;
	Vector vecPos = origin;
	pmtrace_t tr;

	gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, origin, CHAN_VOICE, "weapons/mortarhit.wav", 0.8, 0.33, 0, gEngfuncs.pfnRandomLong(130, 140));
	gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, origin, CHAN_BODY, "ambience/biggun3.wav", 1.0, 0.33, 0, gEngfuncs.pfnRandomLong(110, 115));

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	Vector vecDir = UTIL_UnpackNormal(args->iparam1).Normalize();
	Vector vecMsgDir = vecDir;

	origin = Vector(args->origin) + vecDir * 16;
	Vector vecEnd = origin - vecDir * 128;
	gEngfuncs.pEventAPI->EV_PlayerTrace(origin, vecEnd, PM_STUDIO_BOX, -1, &tr);

	// original: the scorch goes on the trace's end even when it hit nothing (ent 0, the world)
	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
	if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
	{
		gEngfuncs.pEfxAPI->R_DecalShoot(
			gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)"{smscorch3")),
			gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
	}

	if (tr.fraction < 1.0)
	{
		vecMsgDir = tr.plane.normal;
		origin = Vector(tr.endpos) + vecMsgDir * 16;
	}
	else
	{
		origin = vecMsgDir * 16 + origin;
	}
	vecPos = origin;

	Vector view_ofs, viewangles, forward;
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);
	gEngfuncs.GetViewAngles(viewangles);
	AngleVectors(viewangles, forward, nullptr, nullptr);

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	Vector vecEye(0, 0, 0);
	if (pLocal)
		vecEye = Vector(pLocal->origin[0] + view_ofs[0], pLocal->origin[1] + view_ofs[1], pLocal->origin[2] + view_ofs[2]);

	if (EV_IsLocal(args->iparam2))
	{
		V_ScreenTint(Vector(255, 255, 255), Vector(255, 220, 200), 255, 0, 1.5, 6);
	}
	else
	{
		Vector vecDelta = vecEye - origin;

		// original bug kept: the message's direction becomes this unnormalized offset to the viewer
		vecMsgDir = vecDelta;

		// original bug kept: without a local player the far branch reads pLocal->index of a null
		if (vecDelta.Length() > 1280)
		{
			pmtrace_t trView;
			gEngfuncs.pEventAPI->EV_PlayerTrace(origin, vecEye, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, &trView);
			if (trView.fraction >= 1.0 || (0 != trView.ent && gEngfuncs.pEventAPI->EV_IndexFromTrace(&trView) == pLocal->index))
			{
				// the macro evaluates the normalized vector once per component, as the original did
				if (DotProduct(forward, (origin - vecEye).Normalize()) > 0)
					V_ScreenTint(Vector(255, 255, 255), Vector(200, 220, 255), 255, 0, 1.0, 4);
				else
					V_ScreenTint(Vector(255, 255, 255), Vector(200, 220, 255), 255, 0, 0.8, 3);
			}
		}
		else if (pLocal)
		{
			pmtrace_t trView;
			if (EV_CanPenetrate(origin, vecEye, vecDelta.Normalize(), 256, &pLocal->index, &trView, 32))
				V_ScreenTint(Vector(255, 255, 255), Vector(200, 220, 255), 255, 0, 1.2, 5);
		}
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(61);
	CLTE_LoopbackCoord(origin.x);
	CLTE_LoopbackCoord(origin.y);
	CLTE_LoopbackCoord(origin.z);
	CLTE_LoopbackShort((int)(vecMsgDir.x * 32767.0));
	CLTE_LoopbackShort((int)(vecMsgDir.y * 32767.0));
	CLTE_LoopbackShort((int)(vecMsgDir.z * 32767.0));
	CLTE_LoopbackSend();

	int iFlare;
	model_t* pFlare = gEngfuncs.CL_LoadModel("sprites/xflare1.spr", &iFlare);
	if (pFlare)
	{
		TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc(vecPos, pFlare);
		if (pTemp)
		{
			pTemp->die += 2.0;
			pTemp->flags |= FTENT_SPRANIMATE;
			pTemp->entity.curstate.framerate = 20;
			pTemp->entity.curstate.scale = 6;
			pTemp->entity.curstate.renderamt = 250;
			pTemp->entity.curstate.rendermode = kRenderGlow;
			pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
			pTemp->entity.curstate.rendercolor.g = 255;
			pTemp->entity.curstate.rendercolor.r = 255;
			pTemp->entity.curstate.rendercolor.b = 255;
			pTemp->entity.curstate.frame = 0;
			pTemp->frameMax = 20;
		}
	}
}


//======================
//	  TRANSFORMS START
//======================

// tassassingun.sc: a player transformed into a female assassin fires (fparam1/fparam2 the
// spread along right/up). A tracer from the gun, the shell, the bullet's hit sound and decal.
// name ours
void EV_TAssassinGun(event_args_t* args) // orig client 0x24f50
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecTracerSrc, vecDir, vecEnd;
	Vector up, right, forward;
	pmtrace_t tr;
	bool bFirstPerson;

	idx = args->entindex;
	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);
	VectorCopy(args->velocity, velocity);

	AngleVectors(angles, forward, right, up);

	if (0 != gEngfuncs.pfnRandomLong(0, 1))
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/pl_gun1.wav", gEngfuncs.pfnRandomFloat(0.6, 0.8), 0.8, 0, gEngfuncs.pfnRandomLong(95, 105));
	else
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/pl_gun2.wav", gEngfuncs.pfnRandomFloat(0.6, 0.8), 0.8, 0, gEngfuncs.pfnRandomLong(95, 105));

	EV_GetGunPosition(args, vecTracerSrc, origin);

	vecDir = forward;
	vecSrc = vecTracerSrc;
	VectorMA(vecDir, args->fparam1, right, vecDir);
	VectorMA(vecDir, args->fparam2, up, vecDir);
	VectorMA(vecSrc, 8192, vecDir, vecEnd);

	bFirstPerson = false;
	if (EV_IsLocal(idx))
	{
		EV_MuzzleFlash();
		V_PunchAxis(0, g_flTransformFOV == 0 ? -1.0 : -0.25);

		if (0 == CL_IsThirdPerson())
		{
			VectorMA(vecTracerSrc, -4, up, vecTracerSrc);
			VectorMA(vecTracerSrc, 3, right, vecTracerSrc);
			bFirstPerson = true;
		}
	}

	if (!bFirstPerson)
	{
		VectorMA(vecTracerSrc, -24, up, vecTracerSrc);
		VectorMA(vecTracerSrc, 6, right, vecTracerSrc);
	}

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl"); // brass shell

	if (bFirstPerson)
		EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 16, -12, 8);
	else
		EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -18, 8);

	EV_EjectBrass(ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL);

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr);

	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
	if (pe)
	{
		Vector vecHit = CLTE_ShieldPoint(pe->info, tr.endpos, vecDir, nullptr, nullptr);
		VectorCopy(vecHit, tr.endpos);
	}

	EV_CreateTracer(vecTracerSrc, tr.endpos);

	if (tr.fraction < 1.0 && gEngfuncs.PM_PointContents(tr.endpos, nullptr) != CONTENTS_SKY)
	{
		EV_HLDM_PlayTextureSound(idx, &tr, vecSrc, vecEnd, BULLET_PLAYER_9MM);
		EV_HLDM_DecalGunshot(&tr, BULLET_PLAYER_9MM);
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();
}

// tassassinkick.sc: the transformed assassin's kick (bparam1 a stronger one, bparam2 it hit).
// name ours
void EV_TAssassinKick(event_args_t* args) // orig client 0x254a0
{
	int idx = args->entindex;
	Vector origin;
	VectorCopy(args->origin, origin);

	float flVolume;
	int iPitch;
	if (0 == args->bparam1)
	{
		flVolume = gEngfuncs.pfnRandomFloat(0.9, 1.0);
		iPitch = gEngfuncs.pfnRandomLong(105, 110);
	}
	else
	{
		flVolume = gEngfuncs.pfnRandomFloat(0.7, 0.8);
		iPitch = gEngfuncs.pfnRandomLong(95, 105);
	}

	// original: the sample is left as the volume's bits when the random pick falls through (it can't)
	const char* pszSound = nullptr;
	if (0 == args->bparam2)
	{
		switch (gEngfuncs.pfnRandomLong(0, 1))
		{
		case 0:
			pszSound = "zombie/claw_miss1.wav";
			break;
		case 1:
			pszSound = "zombie/claw_miss2.wav";
			break;
		}
	}
	else
	{
		switch (gEngfuncs.pfnRandomLong(0, 2))
		{
		case 0:
			pszSound = "zombie/claw_strike1.wav";
			break;
		case 1:
			pszSound = "zombie/claw_strike2.wav";
			break;
		case 2:
			pszSound = "zombie/claw_strike3.wav";
			break;
		}
	}

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, pszSound, flVolume, 0.8, 0, iPitch);
}

// The transformed houndeye's beam: the quads its impacts leave, drawn as one fade-quad batch by
// EV_THoundeyeBeam. names ours
#define HOUND_MAX_QUADS 64
static Vector s_vecHoundQuad0[HOUND_MAX_QUADS]; // orig .bss 0x101791a0
static Vector s_vecHoundQuad1[HOUND_MAX_QUADS]; // orig .bss 0x101794a0
static Vector s_vecHoundQuad2[HOUND_MAX_QUADS]; // orig .bss 0x101797a0
static Vector s_vecHoundQuad3[HOUND_MAX_QUADS]; // orig .bss 0x10179aa0
static int s_cHoundQuads = 0;					// orig .bss 0x10179da8

// Where the houndeye beam enters or leaves a surface (tr the trace that found it): scorches on
// the world, a green ring, a quad for the batch and a green glow. iSide 1 entering, 0 leaving
// (unused). name ours
void EV_HoundBeamImpact(Vector vecPos, Vector vecNormal, int iSide, pmtrace_t* tr) // orig client 0x25610
{
	if (tr->fraction >= 1.0)
		return;

	Vector vecTest = vecPos - vecNormal;
	if (gEngfuncs.PM_PointContents(vecTest, nullptr) == CONTENTS_SKY)
		return;

	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr->ent);
	const char* pszDecal = "{smscorch3";
	switch (gEngfuncs.pfnRandomLong(0, 2))
	{
	case 0:
		pszDecal = "{scorch1";
		break;
	case 1:
		pszDecal = "{scorch2";
		break;
	case 2:
		pszDecal = "{scorch3";
		break;
	}

	if (pe)
	{
		if (pe->solid != SOLID_BSP && pe->movetype != MOVETYPE_PUSHSTEP)
			return;

		if (gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
		{
			gEngfuncs.pEfxAPI->R_DecalShoot(
				gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)pszDecal)),
				gEngfuncs.pEventAPI->EV_IndexFromTrace(tr), 0, tr->endpos, 0);
			gEngfuncs.pEfxAPI->R_DecalShoot(
				gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)"{smscorch3")),
				gEngfuncs.pEventAPI->EV_IndexFromTrace(tr), 0, tr->endpos, 0);
		}
	}

	// two axes across the surface from a random direction
	Vector vecRandom;
	do
	{
		vecRandom.x = gEngfuncs.pfnRandomFloat(-1, 1);
		vecRandom.y = gEngfuncs.pfnRandomFloat(-1, 1);
		vecRandom.z = gEngfuncs.pfnRandomFloat(-1, 1);
	} while (vecRandom.x * vecRandom.x + vecRandom.y * vecRandom.y + vecRandom.z * vecRandom.z == 0);

	Vector vecA = CrossProduct(vecRandom, vecNormal).Normalize();
	Vector vecB = CrossProduct(vecA, vecNormal).Normalize();

	CLTE_AddDiskRing(vecPos + vecNormal * 2, vecA, vecB, vecNormal, 2,
		0, 0, 0, 0, 1, 18, 4, 24,
		Vector(0.2, 1, 2), Vector(0, 1, 0), 0.3, 0,
		Vector(0.2, 1, 2), Vector(0, 1, 0), 0.9, 0.05,
		0.5, "sprites/shockwave.spr", 0, 0, 0);

	if (g_iClientDetail >= 0 && s_cHoundQuads < HOUND_MAX_QUADS)
	{
		Vector vecCenter = vecNormal + vecPos;
		Vector vecA32 = vecA * 32;
		Vector vecB32 = vecB * 32;
		s_vecHoundQuad0[s_cHoundQuads] = vecA32 + (vecCenter + vecB32);
		s_vecHoundQuad1[s_cHoundQuads] = vecA32 + (vecCenter - vecB32);
		s_vecHoundQuad2[s_cHoundQuads] = (vecCenter - vecA32) - vecB32;
		s_vecHoundQuad3[s_cHoundQuads] = (vecCenter + vecB32) - vecA32;
		s_cHoundQuads++;
	}

	int iGlow;
	model_t* pGlow = gEngfuncs.CL_LoadModel("sprites/animglow01.spr", &iGlow);
	if (!pGlow)
		return;

	TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc(vecPos + vecNormal * 2, pGlow);
	if (pTemp)
	{
		pTemp->entity.curstate.scale = 1.0;
		pTemp->entity.curstate.rendermode = kRenderGlow;
		pTemp->flags |= FTENT_FADEOUT;
		pTemp->entity.baseline.renderamt = 250;
		pTemp->entity.curstate.renderamt = 250;
		pTemp->die += 1.0;
		pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		pTemp->entity.curstate.rendercolor.r = 20;
		pTemp->entity.curstate.rendercolor.g = 255;
		pTemp->entity.curstate.rendercolor.b = 20;
		pTemp->entity.curstate.frame = 0;
		pTemp->fadeSpeed = 0.5;
	}
}

// The houndeye beam through walls: as EV_CanPenetrate, marking where it enters and leaves each
// solid (EV_HoundBeamImpact); it also stops walking a solid that turns away from vecEnd. name ours
bool EV_HoundBeamPenetrate(Vector vecStart, Vector vecEnd, Vector vecDir, float flThickness, pmtrace_t* tr, pmtrace_t* trExit, int* pTarget, int iDepth) // orig client 0x26350
{
	if (iDepth < 0)
		return false;

	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, tr);

	if (tr->fraction >= 1.0)
		return true;

	if (pTarget && 0 != tr->ent && gEngfuncs.pEventAPI->EV_IndexFromTrace(tr) == *pTarget)
		return true;

	EV_HoundBeamImpact(tr->endpos, tr->plane.normal, 1, tr);

	vecStart = tr->endpos;

	if (flThickness <= 0)
		return false;

	float flStep;
	while (true)
	{
		flStep = 8;
		if (flThickness < 8)
			flStep = flThickness;
		flThickness -= flStep;

		vecStart = vecStart + vecDir * flStep;

		if (gEngfuncs.PM_PointContents(vecStart, nullptr) != CONTENTS_SOLID)
			break;

		// the macro evaluates the difference once per component, as the original did
		if (DotProduct(vecDir, (vecEnd - vecStart)) < 0)
			break;

		if (flThickness <= 0)
			return false;
	}

	Vector vecBack = vecStart - vecDir * flStep;
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecBack, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, trExit);
	EV_HoundBeamImpact(trExit->endpos, trExit->plane.normal, 0, trExit);

	return EV_HoundBeamPenetrate(vecStart, vecEnd, vecDir, flThickness, tr, trExit, pTarget, iDepth - 1);
}

// thoundeyebeam.sc: the transformed houndeye's beam: a short level blast, then the beam through
// up to BeamPunchDistance of wall, a green flash for whoever it nearly hits (or that sees it, or
// the shooter), loopback message 66 (start, direction, length) and the impacts' quad batch.
// Ends its charge effect (65). name ours
void EV_THoundeyeBeam(event_args_t* args) // orig client 0x26880 (Ghidra-missed)
{
	int idx = args->entindex;
	Vector origin;
	Vector angles;
	Vector up, right, forward;
	pmtrace_t tr, trExit;
	float flFlash = -1;

	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	cl_entity_t* pEnt = GetEntity(idx);
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	s_cHoundQuads = 0;

	CLTE_KillByEntity(idx, 65);

	AngleVectors(angles, forward, right, up);

	Vector vecFlat(forward.x, forward.y, 0);
	VectorNormalize(vecFlat);

	Vector vecStart, vecEnd;
	VectorMA(origin, 12, vecFlat, vecStart);
	vecStart.z += 8;
	Vector vecBeamStart = vecStart;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	VectorMA(vecStart, 80, vecFlat, vecEnd);
	tr.fraction = 1.0;
	trExit.fraction = 1.0;
	EV_HoundBeamPenetrate(vecStart, vecEnd, vecFlat, 32, &tr, &trExit, pEnt ? &pEnt->index : nullptr, 16);
	vecStart = vecEnd;

	if (EV_IsLocal(idx))
	{
		flFlash = 1.2;
	}
	else if (pLocal)
	{
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(pLocal->index - 1);

		Vector vecToLocal = Vector(pLocal->origin) - vecStart;
		float flDot = DotProduct(vecToLocal, forward);
		if (flDot > 0)
		{
			// original bug kept: the beam's closest point to the player is measured from the
			// world's origin, not from the player
			Vector vecClosest = forward * flDot + vecStart;
			float flMiss = vecClosest.Length();
			if (flMiss < RC2Pref(RC2P_TransformHoundeye_BeamCoreWidth).f)
				flFlash = 0.9;
			else if (flMiss < RC2Pref(RC2P_TransformHoundeye_BeamRadialWidth).f)
				flFlash = 0.8;
		}

		if (vecToLocal.Length() < RC2Pref(RC2P_TransformHoundeye_BeamStartBlastRadius).f)
			flFlash = 1.0;
	}

	VectorMA(vecStart, 8192, forward, vecEnd);
	EV_HoundBeamPenetrate(vecStart, vecEnd, forward, RC2Pref(RC2P_TransformHoundeye_BeamPunchDistance).f, &tr, &trExit, pEnt ? &pEnt->index : nullptr, 64);

	float flLength = (vecStart - Vector(tr.endpos)).Length();

	if (flFlash < 0.4 && !EV_IsLocal(idx) && pLocal && 0 <= flLength)
	{
		// a point of the beam in sight of the player, every 64 units
		float flAlong = 0;
		do
		{
			Vector vecPoint = forward * flAlong + vecStart;
			gEngfuncs.pEventAPI->EV_PlayerTrace(vecPoint, pLocal->origin, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, &tr);
			if (tr.fraction >= 1.0 || (0 != tr.ent && gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr) == pLocal->index))
			{
				flFlash = 0.4;
				break;
			}
			flAlong += 64;
		} while (flAlong <= flLength);
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(66);
	CLTE_LoopbackCoord(vecBeamStart.x);
	CLTE_LoopbackCoord(vecBeamStart.y);
	CLTE_LoopbackCoord(vecBeamStart.z);
	CLTE_LoopbackShort((int)(forward.x * 32767.0));
	CLTE_LoopbackShort((int)(forward.y * 32767.0));
	CLTE_LoopbackShort((int)(forward.z * 32767.0));
	CLTE_LoopbackShort((int)flLength);
	CLTE_LoopbackSend();

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "debris/beamstart10.wav", 1.0, 0.6, 0, gEngfuncs.pfnRandomLong(109, 111));
	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_ITEM, "debris/beamstart9.wav", 1.0, 0.6, 0, gEngfuncs.pfnRandomLong(109, 111));

	if (flFlash > 0)
		V_ScreenTint(Vector(128, 255, 128), Vector(0, 255, 0), 250, 0, flFlash, flFlash > 1.0 ? 5 : 3);

	if (s_cHoundQuads <= 0)
		return;

	if (s_cHoundQuads > HOUND_MAX_QUADS)
		s_cHoundQuads = HOUND_MAX_QUADS;

	int iFlare;
	model_t* pFlare = gEngfuncs.CL_LoadModel("sprites/heflare3.spr", &iFlare);
	if (!pFlare)
		return;

	Vector vecColors[HOUND_MAX_QUADS];
	float flAlphas[HOUND_MAX_QUADS];
	float flFades[HOUND_MAX_QUADS];
	for (int i = 0; i < s_cHoundQuads; i++)
	{
		vecColors[i].z = vecColors[i].x = gEngfuncs.pfnRandomFloat(0, 0.2);
		vecColors[i].y = 1.0;
		flAlphas[i] = gEngfuncs.pfnRandomFloat(0.9, 0.95);
		flFades[i] = flAlphas[i] / gEngfuncs.pfnRandomFloat(0.4, 0.5);
	}

	CLTE_AddFadeQuads(s_cHoundQuads, s_vecHoundQuad0, s_vecHoundQuad1, s_vecHoundQuad2, s_vecHoundQuad3, vecColors, flAlphas, flFades, pFlare);
}

// thoundeyesonic.sc: the transformed houndeye's sonic blast; loopback message 67 (origin and the
// direction, its pitch held within SonicMaxXAngDev). name ours
void EV_THoundeyeSonic(event_args_t* args) // orig client 0x271b0
{
	int idx = args->entindex;
	Vector origin;
	Vector angles;
	Vector up, right, forward;

	VectorCopy(args->origin, origin);
	VectorCopy(args->angles, angles);

	switch (gEngfuncs.pfnRandomLong(0, 2))
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "houndeye/he_blast1.wav", 1.0, ATTN_NORM, 0, gEngfuncs.pfnRandomLong(95, 105));
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "houndeye/he_blast2.wav", 1.0, ATTN_NORM, 0, gEngfuncs.pfnRandomLong(95, 105));
		break;
	case 2:
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "houndeye/he_blast3.wav", 1.0, ATTN_NORM, 0, gEngfuncs.pfnRandomLong(95, 105));
		break;
	}

	const float flMax = RC2Pref(RC2P_TransformHoundeye_SonicMaxXAngDev).f;
	if (angles[0] < -flMax)
		angles[0] = -flMax;
	else if (angles[0] > flMax)
		angles[0] = flMax;

	AngleVectors(angles, forward, right, up);

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(67);
	CLTE_LoopbackCoord(origin.x);
	CLTE_LoopbackCoord(origin.y);
	CLTE_LoopbackCoord(origin.z);
	CLTE_LoopbackShort((int)(forward.x * 32767.0));
	CLTE_LoopbackShort((int)(forward.y * 32767.0));
	CLTE_LoopbackShort((int)(forward.z * 32767.0));
	CLTE_LoopbackSend();
}

// tcontrollerthrow.sc: the transformed controller throws its energy ball (ends its charge, 69).
// name ours
void EV_TControllerThrow(event_args_t* args) // orig client 0x273d0
{
	CLTE_KillByEntity(args->entindex, 69);

	int idx = args->entindex;
	Vector origin;
	VectorCopy(args->origin, origin);

	float flVolume = gEngfuncs.pfnRandomFloat(0.7, 0.8);
	int iPitch = gEngfuncs.pfnRandomLong(110, 120);

	// original: the sample is left as the volume's bits when the random pick falls through (it can't)
	const char* pszSound = nullptr;
	switch (gEngfuncs.pfnRandomLong(0, 1))
	{
	case 0:
		pszSound = "zombie/claw_miss1.wav";
		break;
	case 1:
		pszSound = "zombie/claw_miss2.wav";
		break;
	}

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_BODY, pszSound, flVolume, 0.8, 0, iPitch);
}

// tcontrollertballhit.sc: the controller's energy ball hits (iparam1 the packed direction it
// came from, iparam2 the one it hit). A scorch, an orange ring, loopback message 71 (where and the
// surface's normal), an orange flash for a local player who sees it, and a light. Nothing at all
// without a local player. name ours
void EV_TControllerBallHit(event_args_t* args) // orig client 0x274a0
{
	CLTE_KillByEntity(args->entindex, 70);

	Vector vecDir = UTIL_UnpackNormal(args->iparam1).Normalize();
	Vector vecStart = Vector(args->origin) - vecDir * 16;
	Vector vecEnd = vecDir * 128 + vecStart;
	pmtrace_t tr;

	gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, vecStart, CHAN_WEAPON, "debris/beamstart10.wav", 1.0, 0.5, 0, gEngfuncs.pfnRandomLong(120, 130));

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal)
		return;

	Vector view_ofs, viewangles, forward;
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);
	gEngfuncs.GetViewAngles(viewangles);
	AngleVectors(viewangles, forward, nullptr, nullptr);
	Vector vecEye = Vector(pLocal->origin[0] + view_ofs[0], pLocal->origin[1] + view_ofs[1], pLocal->origin[2] + view_ofs[2]);

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_BOX, -1, &tr);

	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
	if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
	{
		const char* pszDecal = nullptr;
		switch (gEngfuncs.pfnRandomLong(0, 2))
		{
		case 0:
			pszDecal = "{scorch1";
			break;
		case 1:
			pszDecal = "{scorch2";
			break;
		case 2:
			pszDecal = "{scorch3";
			break;
		}
		gEngfuncs.pEfxAPI->R_DecalShoot(
			gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)pszDecal)),
			gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
	}

	if (g_iClientDetail >= 0)
	{
		// original: the ring's axes come from the direction, its position from the trace
		Vector vecAngles, vecForward, vecRight, vecUp;
		VectorAngles(vecDir, vecAngles);
		vecAngles[0] = -vecAngles[0];
		AngleVectors(vecAngles, vecForward, vecRight, vecUp);

		CLTE_AddDiskRing(Vector(tr.endpos) + Vector(tr.plane.normal) * 2, vecUp, vecRight, vecForward, 0,
			0, 0, 0, 0, 1, 32, 12, 32,
			Vector(1, 0.75, 0.25), Vector(1, 0.75, 0.25), 0.3, 0,
			Vector(1, 0.75, 0.25), Vector(1, 0.75, 0.25), 0.9, 0.1,
			0.4, "sprites/shockwave.spr", 0, 0, 0);
	}

	CLTE_LoopbackBegin();
	CLTE_LoopbackByte(71);
	CLTE_LoopbackCoord(tr.endpos[0]);
	CLTE_LoopbackCoord(tr.endpos[1]);
	CLTE_LoopbackCoord(tr.endpos[2]);
	CLTE_LoopbackShort((int)(tr.plane.normal[0] * 32767.0));
	CLTE_LoopbackShort((int)(tr.plane.normal[1] * 32767.0));
	CLTE_LoopbackShort((int)(tr.plane.normal[2] * 32767.0));
	CLTE_LoopbackSend();

	if (pLocal->index != args->iparam2)
	{
		// the macro evaluates the normalized vector once per component, as the original did
		if (DotProduct(forward, (vecStart - vecEye).Normalize()) > 0)
		{
			pmtrace_t trView;
			gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEye, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, &trView);
			if (trView.fraction >= 1.0 || (0 != trView.ent && gEngfuncs.pEventAPI->EV_IndexFromTrace(&trView) == pLocal->index))
				V_ScreenTint(Vector(255, 192, 64), Vector(255, 192, 64), 150, 0, 0.25, -2);
		}
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (g_iClientDetail < 0)
		return;

	dlight_t* dl = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
	Vector vecLight = Vector(tr.plane.normal) * 16 + Vector(tr.endpos);
	if (dl)
	{
		VectorCopy(vecLight, dl->origin);
		dl->radius = 120;
		dl->color.r = 255;
		dl->color.g = 192;
		dl->color.b = 64;
		dl->decay = 600;
		dl->die = gEngfuncs.GetClientTime() + 0.3;
	}

	dl = gEngfuncs.pEfxAPI->CL_AllocElight(0);
	if (dl)
	{
		VectorCopy(vecLight, dl->origin);
		dl->radius = 110;
		dl->color.r = 255;
		dl->color.g = 192;
		dl->color.b = 64;
		dl->decay = 550;
		dl->die = gEngfuncs.GetClientTime() + 0.3;
	}
}

// tbatbite.sc: the transformed bat bites (bparam1 it hit). name ours
void EV_TBatBite(event_args_t* args) // orig client 0x281b0
{
	int idx = args->entindex;
	Vector origin;
	VectorCopy(args->origin, origin);

	float flVolume = gEngfuncs.pfnRandomFloat(0.8, 0.9);
	int iPitch = gEngfuncs.pfnRandomLong(110, 120);

	// original: the sample is left as the volume's bits when the random pick falls through (it can't)
	const char* pszSound = nullptr;
	if (0 == args->bparam1)
	{
		switch (gEngfuncs.pfnRandomLong(0, 1))
		{
		case 0:
			pszSound = "zombie/claw_miss1.wav";
			break;
		case 1:
			pszSound = "zombie/claw_miss2.wav";
			break;
		}
	}
	else
	{
		switch (gEngfuncs.pfnRandomLong(0, 2))
		{
		case 0:
			pszSound = "zombie/claw_strike1.wav";
			break;
		case 1:
			pszSound = "zombie/claw_strike2.wav";
			break;
		case 2:
			pszSound = "zombie/claw_strike3.wav";
			break;
		}
	}

	gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, pszSound, flVolume, 0.8, 0, iPitch);
}

// orig client 0x282c0 (Ghidra-missed): SDK 2.3's, the same apart from addresses
void EV_TrainPitchAdjust(event_args_t* args)
{
	int idx;
	Vector origin;

	unsigned short us_params;
	int noise;
	float m_flVolume;
	int pitch;
	bool stop;

	char sz[256];

	idx = args->entindex;

	VectorCopy(args->origin, origin);

	us_params = (unsigned short)args->iparam1;
	stop = 0 != args->bparam1;

	m_flVolume = (float)(us_params & 0x003f) / 40.0;
	noise = (int)(((us_params) >> 12) & 0x0007);
	pitch = (int)(10.0 * (float)((us_params >> 6) & 0x003f));

	switch (noise)
	{
	case 1:
		strcpy(sz, "plats/ttrain1.wav");
		break;
	case 2:
		strcpy(sz, "plats/ttrain2.wav");
		break;
	case 3:
		strcpy(sz, "plats/ttrain3.wav");
		break;
	case 4:
		strcpy(sz, "plats/ttrain4.wav");
		break;
	case 5:
		strcpy(sz, "plats/ttrain6.wav");
		break;
	case 6:
		strcpy(sz, "plats/ttrain7.wav");
		break;
	default:
		// no sound
		strcpy(sz, "");
		return;
	}

	if (stop)
	{
		gEngfuncs.pEventAPI->EV_StopSound(idx, CHAN_STATIC, sz);
	}
	else
	{
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_STATIC, sz, m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, pitch);
	}
}

// halflife-updated's (func_vehicle); RC2 has neither the entity nor the hook
void EV_VehiclePitchAdjust(event_args_t* args)
{
	int idx;
	Vector origin;

	unsigned short us_params;
	int noise;
	float m_flVolume;
	int pitch;
	bool stop;

	char sz[256];

	idx = args->entindex;

	VectorCopy(args->origin, origin);

	us_params = (unsigned short)args->iparam1;
	stop = 0 != args->bparam1;

	m_flVolume = (float)(us_params & 0x003f) / 40.0;
	noise = (int)(((us_params) >> 12) & 0x0007);
	pitch = (int)(10.0 * (float)((us_params >> 6) & 0x003f));

	switch (noise)
	{
	case 1:
		strcpy(sz, "plats/vehicle1.wav");
		break;
	case 2:
		strcpy(sz, "plats/vehicle2.wav");
		break;
	case 3:
		strcpy(sz, "plats/vehicle3.wav");
		break;
	case 4:
		strcpy(sz, "plats/vehicle4.wav");
		break;
	case 5:
		strcpy(sz, "plats/vehicle6.wav");
		break;
	case 6:
		strcpy(sz, "plats/vehicle7.wav");
		break;
	default:
		// no sound
		strcpy(sz, "");
		return;
	}

	if (stop)
	{
		gEngfuncs.pEventAPI->EV_StopSound(idx, CHAN_STATIC, sz);
	}
	else
	{
		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_STATIC, sz, m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, pitch);
	}
}

// orig client 0x284c0 (Ghidra-missed): SDK 2.3's
bool EV_TFC_IsAllyTeam(int iTeam1, int iTeam2)
{
	return false;
}


