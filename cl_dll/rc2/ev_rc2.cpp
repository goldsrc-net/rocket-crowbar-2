/***
*
*	Rocket Crowbar 2: ev_rc2.cpp (file name ours). RC2's own event handlers that client.dll links
*	right after SDK 2.3's ev_hldm.obj (the last function of which, EV_TFC_IsAllyTeam, sits at
*	client 0x284c0): gibs, the shared explosion and its smoke (client 0x284e0-0x28f85). Their hooks
*	are in hl/hl_events.cpp.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "r_efx.h"
#include "event_api.h"
#include "event_args.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "eventscripts.h"
#include "ev_hldm.h"
#include "rc2prefs.h"

#include <cstdlib>

// --- c1 events needs (client.dll addresses; names ours unless the SDK has them)
extern int g_iClientDetail;				  // cdll_int.cpp (c2): cl_detail as an int (orig .data 0x101a9598)
extern struct cvar_s* cl_gibcount;		  // hud.cpp: "cl_gibcount" (orig .bss 0x1020b71c)
Vector UTIL_UnpackNormal(int iPacked);	  // orig client 0xde90 (c1)
float EV_WaterLevel(float* position, float minz, float maxz); // orig client 0xdd80 (c1)
struct tempent_s* CL_SpawnGib(Vector vecOrigin, Vector vecVelocity, int bHuman, int bSkull); // orig client 0x9e770 (entity.cpp)

// A gib's launch velocity, as the server's VecVelocityForDamage but spread round vecDir.
// name ours
static Vector EV_GibVelocity(Vector vecDir, float flDamage) // orig client 0x284e0
{
	Vector vec;
	for (int i = 0; i < 3; i++)
	{
		float flSpread = gEngfuncs.pfnRandomFloat(-0.35, 0.35) - vecDir[i];
		vec[i] = gEngfuncs.pfnRandomFloat(200, 450) * flSpread;

		if (flDamage > -50)
			vec[i] = vec[i] * 0.7;
		else if (flDamage > -200)
			vec[i] = vec[i] + vec[i];
		else
			vec[i] = vec[i] * 4;

		// original bug kept: meant to clamp the component to 2000, this scales it by |v| / 2000
		if (fabs(vec[i]) > 2000)
			vec[i] = vec[i] / (2000 / fabs(vec[i]));
	}
	return vec;
}

// A random point in an entity's bounding box. name ours
static Vector EV_RandomPointInEntity(cl_entity_t* pEnt) // orig client 0x28610
{
	Vector vec;
	for (int i = 0; i < 3; i++)
		vec[i] = gEngfuncs.pfnRandomFloat(pEnt->curstate.origin[i] + pEnt->curstate.mins[i], pEnt->curstate.origin[i] + pEnt->curstate.maxs[i]);
	return vec;
}

// gib.sc: a monster or player is gibbed (angles the attack direction, fparam1 its health,
// iparam1 human gibs). A splat and cl_gibcount gibs, a skull first for humans. name ours
void EV_Gib(event_args_t* args) // orig client 0x28690 (Ghidra-missed)
{
	int cGibs = atoi(cl_gibcount->string);

	gEngfuncs.pEventAPI->EV_PlaySound(args->entindex, args->origin, CHAN_BODY, "common/bodysplat.wav", 1.0, 0.8, 0, 100);

	if (cGibs <= 0)
		return;

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(args->entindex);
	if (!pEnt)
		return;

	if (0 != args->iparam1)
	{
		Vector vecHead = pEnt->origin;
		if (0 != pEnt->player)
			vecHead.z += 28;
		else
			vecHead.z += 50;

		CL_SpawnGib(vecHead, EV_GibVelocity(args->angles, args->fparam1), args->iparam1, 1);
	}

	for (int i = cGibs; i > 0; i--)
	{
		Vector vecVelocity = EV_GibVelocity(args->angles, args->fparam1);
		Vector vecOrigin = EV_RandomPointInEntity(pEnt);
		CL_SpawnGib(vecOrigin, vecVelocity, args->iparam1, 0);
	}
}

// explode.sc: RC2's shared explosion (fparam1 the damage, iparam1 the packed direction, bparam1
// the explosion's flags, bparam2 no scorch). A scorch, the explosion pulled off the wall by the
// damage, sparks, debris and an expsmoke event. name ours
void EV_Explode(event_args_t* args) // orig client 0x28850 (Ghidra-missed)
{
	static unsigned short s_usExpSmoke = 0; // name ours (orig .data 0x10179dac)

	Vector origin;
	Vector vecDir;
	Vector vecStart, vecEnd;
	pmtrace_t tr;
	int iModel;
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

	if (tr.fraction < 1.0 && 0 == args->bparam2)
	{
		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);

		const char* pszDecal = "{scorch1";
		if (args->fparam1 <= 70)
		{
			pszDecal = "{smscorch3";
		}
		else
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
		}

		if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP) && gEngfuncs.pfnGetCvarFloat("r_decals") != 0)
		{
			gEngfuncs.pEfxAPI->R_DecalShoot(
				gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)pszDecal)),
				gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), 0, tr.endpos, 0);
		}

		double flPull = (args->fparam1 - 24) * 0.6;
		if (flPull < 8)
			flPull = 8;
		else if (flPull > 200)
			flPull = 200;
		VectorMA(tr.endpos, flPull, tr.plane.normal, origin);
	}
	else
	{
		tr.plane.normal[0] = 0;
		tr.plane.normal[1] = 0;
		tr.plane.normal[2] = 1;
	}

	int iContents = gEngfuncs.PM_PointContents(origin, nullptr);
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

	double flScale = (args->fparam1 - 50) * 0.6;
	if (flScale < 10)
		flScale = 10;
	else if (flScale > 200)
		flScale = 200;
	gEngfuncs.pEfxAPI->R_Explosion(origin, iModel, flScale * 0.1, 15, args->bparam1);

	if (iContents != CONTENTS_WATER)
	{
		for (int i = gEngfuncs.pfnRandomLong(0, 3); i > 0; i--)
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

	gEngfuncs.pfnPlaybackEvent(args->flags, nullptr, s_usExpSmoke, 0.3, origin, args->angles, args->fparam1, 0, 0, 0, 0, 0);
}

// expsmoke.sc: an explosion's smoke, played by EV_Explode and EV_ShotgunCrateHit 0.3 seconds
// after (fparam1 the size): bubbles in water, else a smoke puff. name ours
void EV_ExpSmoke(event_args_t* args) // orig client 0x28df0
{
	int iModel;

	if (gEngfuncs.PM_PointContents(args->origin, nullptr) == CONTENTS_WATER)
	{
		if (!gEngfuncs.CL_LoadModel("sprites/bubble.spr", &iModel))
			return;

		Vector vecOne(1, 1, 1);
		Vector mins, maxs;
		VectorMA(args->origin, -64, vecOne, mins);
		VectorMA(args->origin, 64, vecOne, maxs);

		Vector vecWater = args->origin;
		float flHeight = EV_WaterLevel(vecWater, args->origin[2], args->origin[2] + 1024);
		gEngfuncs.pEfxAPI->R_Bubbles(mins, maxs, flHeight - mins.z, iModel, 100, 8);
		return;
	}

	model_t* pSmoke = gEngfuncs.CL_LoadModel("sprites/steam1.spr", &iModel);
	if (!pSmoke)
		return;

	TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc(args->origin, pSmoke);
	if (!pTemp)
		return;

	pTemp->frameMax = 16;
	pTemp->entity.curstate.framerate = 15;
	pTemp->flags |= FTENT_SPRANIMATE;
	pTemp->die += 1.0;

	double flScale = (args->fparam1 - 50.0) * 0.8;
	if (flScale < 10.0)
		flScale = 10.0;
	else if (flScale > 200.0)
		flScale = 200.0;
	gEngfuncs.pEfxAPI->R_Sprite_Smoke(pTemp, flScale * 0.1);
}

