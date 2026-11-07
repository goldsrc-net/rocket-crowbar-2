/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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
/*

===== util.cpp ========================================================

  Utility code.  Really not optional after all.

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include <time.h>
#include "shake.h"
#include "decals.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "UserMessages.h"
#include "rc2prefs.h"
#include "rc2/shield.h"

float UTIL_WeaponTimeBase() // orig 0xf0a90 (CLIENT_WEAPONS: 0)
{
#if defined(CLIENT_WEAPONS)
	return 0.0;
#else
	return gpGlobals->time;
#endif
}

CBaseEntity* UTIL_FindEntityForward(CBaseEntity* pMe)
{
	TraceResult tr;

	UTIL_MakeVectors(pMe->pev->v_angle);
	UTIL_TraceLine(pMe->pev->origin + pMe->pev->view_ofs, pMe->pev->origin + pMe->pev->view_ofs + gpGlobals->v_forward * 8192, dont_ignore_monsters, pMe->edict(), &tr);
	if (tr.flFraction != 1.0 && !FNullEnt(tr.pHit))
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		return pHit;
	}
	return NULL;
}

static unsigned int glSeed = 0;

unsigned int seed_table[256] =
	{
		28985, 27138, 26457, 9451, 17764, 10909, 28790, 8716, 6361, 4853, 17798, 21977, 19643, 20662, 10834, 20103,
		27067, 28634, 18623, 25849, 8576, 26234, 23887, 18228, 32587, 4836, 3306, 1811, 3035, 24559, 18399, 315,
		26766, 907, 24102, 12370, 9674, 2972, 10472, 16492, 22683, 11529, 27968, 30406, 13213, 2319, 23620, 16823,
		10013, 23772, 21567, 1251, 19579, 20313, 18241, 30130, 8402, 20807, 27354, 7169, 21211, 17293, 5410, 19223,
		10255, 22480, 27388, 9946, 15628, 24389, 17308, 2370, 9530, 31683, 25927, 23567, 11694, 26397, 32602, 15031,
		18255, 17582, 1422, 28835, 23607, 12597, 20602, 10138, 5212, 1252, 10074, 23166, 19823, 31667, 5902, 24630,
		18948, 14330, 14950, 8939, 23540, 21311, 22428, 22391, 3583, 29004, 30498, 18714, 4278, 2437, 22430, 3439,
		28313, 23161, 25396, 13471, 19324, 15287, 2563, 18901, 13103, 16867, 9714, 14322, 15197, 26889, 19372, 26241,
		31925, 14640, 11497, 8941, 10056, 6451, 28656, 10737, 13874, 17356, 8281, 25937, 1661, 4850, 7448, 12744,
		21826, 5477, 10167, 16705, 26897, 8839, 30947, 27978, 27283, 24685, 32298, 3525, 12398, 28726, 9475, 10208,
		617, 13467, 22287, 2376, 6097, 26312, 2974, 9114, 21787, 28010, 4725, 15387, 3274, 10762, 31695, 17320,
		18324, 12441, 16801, 27376, 22464, 7500, 5666, 18144, 15314, 31914, 31627, 6495, 5226, 31203, 2331, 4668,
		12650, 18275, 351, 7268, 31319, 30119, 7600, 2905, 13826, 11343, 13053, 15583, 30055, 31093, 5067, 761,
		9685, 11070, 21369, 27155, 3663, 26542, 20169, 12161, 15411, 30401, 7580, 31784, 8985, 29367, 20989, 14203,
		29694, 21167, 10337, 1706, 28578, 887, 3373, 19477, 14382, 675, 7033, 15111, 26138, 12252, 30996, 21409,
		25678, 18555, 13256, 23316, 22407, 16727, 991, 9236, 5373, 29402, 6117, 15241, 27715, 19291, 19888, 19847};

unsigned int U_Random()
{
	glSeed *= 69069;
	glSeed += seed_table[glSeed & 0xff];

	return (++glSeed & 0x0fffffff);
}

void U_Srand(unsigned int seed)
{
	glSeed = seed_table[seed & 0xff];
}

/*
=====================
UTIL_SharedRandomLong
=====================
*/
int UTIL_SharedRandomLong(unsigned int seed, int low, int high)
{
	unsigned int range;

	U_Srand((int)seed + low + high);

	range = high - low + 1;
	if (0 == (range - 1))
	{
		return low;
	}
	else
	{
		int offset;
		int rnum;

		rnum = U_Random();

		offset = rnum % range;

		return (low + offset);
	}
}

/*
=====================
UTIL_SharedRandomFloat
=====================
*/
float UTIL_SharedRandomFloat(unsigned int seed, float low, float high)
{
	//
	unsigned int range;

	U_Srand((int)seed + *(int*)&low + *(int*)&high);

	U_Random();
	U_Random();

	range = high - low;
	if (0 == range)
	{
		return low;
	}
	else
	{
		int tensixrand;
		float offset;

		tensixrand = U_Random() & 65535;

		offset = (float)tensixrand / 65536.0;

		return (low + offset * range);
	}
}

void UTIL_ParametricRocket(entvars_t* pev, Vector vecOrigin, Vector vecAngles, edict_t* owner)
{
	pev->startpos = vecOrigin;
	// Trace out line to end pos
	TraceResult tr;
	UTIL_MakeVectors(vecAngles);
	UTIL_TraceLine(pev->startpos, pev->startpos + gpGlobals->v_forward * 8192, ignore_monsters, owner, &tr);
	pev->endpos = tr.vecEndPos;

	// Now compute how long it will take based on current velocity
	Vector vecTravel = pev->endpos - pev->startpos;
	float travelTime = 0.0;
	if (pev->velocity.Length() > 0)
	{
		travelTime = vecTravel.Length() / pev->velocity.Length();
	}
	pev->starttime = gpGlobals->time;
	pev->impacttime = gpGlobals->time + travelTime;
}

// Normal overrides
void UTIL_SetGroupTrace(int groupmask, int op)
{
	g_groupmask = groupmask;
	g_groupop = op;

	ENGINE_SETGROUPMASK(g_groupmask, g_groupop);
}

void UTIL_UnsetGroupTrace()
{
	g_groupmask = 0;
	g_groupop = 0;

	ENGINE_SETGROUPMASK(0, 0);
}

// Smart version, it'll clean itself up when it pops off stack
UTIL_GroupTrace::UTIL_GroupTrace(int groupmask, int op)
{
	m_oldgroupmask = g_groupmask;
	m_oldgroupop = g_groupop;

	g_groupmask = groupmask;
	g_groupop = op;

	ENGINE_SETGROUPMASK(g_groupmask, g_groupop);
}

UTIL_GroupTrace::~UTIL_GroupTrace()
{
	g_groupmask = m_oldgroupmask;
	g_groupop = m_oldgroupop;

	ENGINE_SETGROUPMASK(g_groupmask, g_groupop);
}

TYPEDESCRIPTION gEntvarsDescription[] =
	{
		DEFINE_ENTITY_FIELD(classname, FIELD_STRING),
		DEFINE_ENTITY_GLOBAL_FIELD(globalname, FIELD_STRING),

		DEFINE_ENTITY_FIELD(origin, FIELD_POSITION_VECTOR),
		DEFINE_ENTITY_FIELD(oldorigin, FIELD_POSITION_VECTOR),
		DEFINE_ENTITY_FIELD(velocity, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(basevelocity, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(movedir, FIELD_VECTOR),

		DEFINE_ENTITY_FIELD(angles, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(avelocity, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(punchangle, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(v_angle, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(fixangle, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(idealpitch, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(pitch_speed, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(ideal_yaw, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(yaw_speed, FIELD_FLOAT),

		DEFINE_ENTITY_FIELD(modelindex, FIELD_INTEGER),
		DEFINE_ENTITY_GLOBAL_FIELD(model, FIELD_MODELNAME),

		DEFINE_ENTITY_FIELD(viewmodel, FIELD_MODELNAME),
		DEFINE_ENTITY_FIELD(weaponmodel, FIELD_MODELNAME),

		DEFINE_ENTITY_FIELD(absmin, FIELD_POSITION_VECTOR),
		DEFINE_ENTITY_FIELD(absmax, FIELD_POSITION_VECTOR),
		DEFINE_ENTITY_GLOBAL_FIELD(mins, FIELD_VECTOR),
		DEFINE_ENTITY_GLOBAL_FIELD(maxs, FIELD_VECTOR),
		DEFINE_ENTITY_GLOBAL_FIELD(size, FIELD_VECTOR),

		DEFINE_ENTITY_FIELD(ltime, FIELD_TIME),
		DEFINE_ENTITY_FIELD(nextthink, FIELD_TIME),

		DEFINE_ENTITY_FIELD(solid, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(movetype, FIELD_INTEGER),

		DEFINE_ENTITY_FIELD(skin, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(body, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(effects, FIELD_INTEGER),

		DEFINE_ENTITY_FIELD(gravity, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(friction, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(light_level, FIELD_FLOAT),

		DEFINE_ENTITY_FIELD(frame, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(scale, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(sequence, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(animtime, FIELD_TIME),
		DEFINE_ENTITY_FIELD(framerate, FIELD_FLOAT),
		DEFINE_ENTITY_ARRAY(controller, FIELD_CHARACTER, NUM_ENT_CONTROLLERS),
		DEFINE_ENTITY_ARRAY(blending, FIELD_CHARACTER, NUM_ENT_BLENDERS),

		DEFINE_ENTITY_FIELD(rendermode, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(renderamt, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(rendercolor, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(renderfx, FIELD_INTEGER),

		DEFINE_ENTITY_FIELD(health, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(frags, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(weapons, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(takedamage, FIELD_FLOAT),

		DEFINE_ENTITY_FIELD(deadflag, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(view_ofs, FIELD_VECTOR),
		DEFINE_ENTITY_FIELD(button, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(impulse, FIELD_INTEGER),

		DEFINE_ENTITY_FIELD(chain, FIELD_EDICT),
		DEFINE_ENTITY_FIELD(dmg_inflictor, FIELD_EDICT),
		DEFINE_ENTITY_FIELD(enemy, FIELD_EDICT),
		DEFINE_ENTITY_FIELD(aiment, FIELD_EDICT),
		DEFINE_ENTITY_FIELD(owner, FIELD_EDICT),
		DEFINE_ENTITY_FIELD(groundentity, FIELD_EDICT),

		DEFINE_ENTITY_FIELD(spawnflags, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(flags, FIELD_FLOAT),

		DEFINE_ENTITY_FIELD(colormap, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(team, FIELD_INTEGER),

		DEFINE_ENTITY_FIELD(max_health, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(teleport_time, FIELD_TIME),
		DEFINE_ENTITY_FIELD(armortype, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(armorvalue, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(waterlevel, FIELD_INTEGER),
		DEFINE_ENTITY_FIELD(watertype, FIELD_INTEGER),

		// Having these fields be local to the individual levels makes it easier to test those levels individually.
		DEFINE_ENTITY_GLOBAL_FIELD(target, FIELD_STRING),
		DEFINE_ENTITY_GLOBAL_FIELD(targetname, FIELD_STRING),
		DEFINE_ENTITY_FIELD(netname, FIELD_STRING),
		DEFINE_ENTITY_FIELD(message, FIELD_STRING),

		DEFINE_ENTITY_FIELD(dmg_take, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(dmg_save, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(dmg, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(dmgtime, FIELD_TIME),

		DEFINE_ENTITY_FIELD(noise, FIELD_SOUNDNAME),
		DEFINE_ENTITY_FIELD(noise1, FIELD_SOUNDNAME),
		DEFINE_ENTITY_FIELD(noise2, FIELD_SOUNDNAME),
		DEFINE_ENTITY_FIELD(noise3, FIELD_SOUNDNAME),
		DEFINE_ENTITY_FIELD(speed, FIELD_FLOAT),
		DEFINE_ENTITY_FIELD(air_finished, FIELD_TIME),
		DEFINE_ENTITY_FIELD(pain_finished, FIELD_TIME),
		DEFINE_ENTITY_FIELD(radsuit_finished, FIELD_TIME),
};

#define ENTVARS_COUNT (sizeof(gEntvarsDescription) / sizeof(gEntvarsDescription[0]))

edict_t* UTIL_GetEntityList()
{
	return g_engfuncs.pfnPEntityOfEntOffset(0);
}

CBaseEntity* UTIL_GetLocalPlayer()
{
	return UTIL_PlayerByIndex(1);
}

#ifdef DEBUG
edict_t* DBG_EntOfVars(const entvars_t* pev)
{
	if (pev->pContainingEntity != NULL)
		return pev->pContainingEntity;
	ALERT(at_console, "entvars_t pContainingEntity is NULL, calling into engine");
	edict_t* pent = (*g_engfuncs.pfnFindEntityByVars)((entvars_t*)pev);
	if (pent == NULL)
		ALERT(at_console, "DAMN!  Even the engine couldn't FindEntityByVars!");
	((entvars_t*)pev)->pContainingEntity = pent;
	return pent;
}
#endif //DEBUG


#ifdef DEBUG
void DBG_AssertFunction(
	bool fExpr,
	const char* szExpr,
	const char* szFile,
	int szLine,
	const char* szMessage)
{
	if (fExpr)
		return;
	char szOut[512];
	if (szMessage != NULL)
		sprintf(szOut, "ASSERT FAILED:\n %s \n(%s@%d)\n%s", szExpr, szFile, szLine, szMessage);
	else
		sprintf(szOut, "ASSERT FAILED:\n %s \n(%s@%d)", szExpr, szFile, szLine);
	ALERT(at_console, szOut);
}
#endif // DEBUG

// ripped this out of the engine
float UTIL_AngleMod(float a) // orig 0xf0df0
{
	if (a < 0)
	{
		a = a + 360 * ((int)(a / 360) + 1);
	}
	else if (a >= 360)
	{
		a = a - 360 * ((int)(a / 360));
	}
	// a = (360.0/65536) * ((int)(a*(65536/360.0)) & 65535);
	return a;
}

float UTIL_AngleDiff(float destAngle, float srcAngle)
{
	float delta;

	delta = destAngle - srcAngle;
	if (destAngle > srcAngle)
	{
		if (delta >= 180)
			delta -= 360;
	}
	else
	{
		if (delta <= -180)
			delta += 360;
	}
	return delta;
}

Vector UTIL_VecToAngles(const Vector& vec)
{
	float rgflVecOut[3];
	VEC_TO_ANGLES(vec, rgflVecOut);
	return Vector(rgflVecOut);
}

//	float UTIL_MoveToOrigin( edict_t *pent, const Vector vecGoal, float flDist, int iMoveType )
void UTIL_MoveToOrigin(edict_t* pent, const Vector& vecGoal, float flDist, int iMoveType)
{
	float rgfl[3];
	vecGoal.CopyToArray(rgfl);
	//		return MOVE_TO_ORIGIN ( pent, rgfl, flDist, iMoveType );
	MOVE_TO_ORIGIN(pent, rgfl, flDist, iMoveType);
}


int UTIL_EntitiesInBox(CBaseEntity** pList, int listMax, const Vector& mins, const Vector& maxs, int flagMask)
{
	edict_t* pEdict = UTIL_GetEntityList();
	CBaseEntity* pEntity;
	int count;

	count = 0;

	if (!pEdict)
		return count;

	// Ignore world.
	++pEdict;

	for (int i = 1; i < gpGlobals->maxEntities; i++, pEdict++)
	{
		if (0 != pEdict->free) // Not in use
			continue;

		if (0 != flagMask && (pEdict->v.flags & flagMask) == 0) // Does it meet the criteria?
			continue;

		if (mins.x > pEdict->v.absmax.x ||
			mins.y > pEdict->v.absmax.y ||
			mins.z > pEdict->v.absmax.z ||
			maxs.x < pEdict->v.absmin.x ||
			maxs.y < pEdict->v.absmin.y ||
			maxs.z < pEdict->v.absmin.z)
			continue;

		pEntity = CBaseEntity::Instance(pEdict);
		if (!pEntity)
			continue;

		pList[count] = pEntity;
		count++;

		if (count >= listMax)
			return count;
	}

	return count;
}


int UTIL_MonstersInSphere(CBaseEntity** pList, int listMax, const Vector& center, float radius)
{
	edict_t* pEdict = UTIL_GetEntityList();
	CBaseEntity* pEntity;
	int count;
	float distance, delta;

	count = 0;
	float radiusSquared = radius * radius;

	if (!pEdict)
		return count;

	// Ignore world.
	++pEdict;

	for (int i = 1; i < gpGlobals->maxEntities; i++, pEdict++)
	{
		if (0 != pEdict->free) // Not in use
			continue;

		if ((pEdict->v.flags & (FL_CLIENT | FL_MONSTER)) == 0) // Not a client/monster ?
			continue;

		// Use origin for X & Y since they are centered for all monsters
		// Now X
		delta = center.x - pEdict->v.origin.x; //(pEdict->v.absmin.x + pEdict->v.absmax.x)*0.5;
		delta *= delta;

		if (delta > radiusSquared)
			continue;
		distance = delta;

		// Now Y
		delta = center.y - pEdict->v.origin.y; //(pEdict->v.absmin.y + pEdict->v.absmax.y)*0.5;
		delta *= delta;

		distance += delta;
		if (distance > radiusSquared)
			continue;

		// Now Z
		delta = center.z - (pEdict->v.absmin.z + pEdict->v.absmax.z) * 0.5;
		delta *= delta;

		distance += delta;
		if (distance > radiusSquared)
			continue;

		pEntity = CBaseEntity::Instance(pEdict);
		if (!pEntity)
			continue;

		pList[count] = pEntity;
		count++;

		if (count >= listMax)
			return count;
	}


	return count;
}


CBaseEntity* UTIL_FindEntityInSphere(CBaseEntity* pStartEntity, const Vector& vecCenter, float flRadius) // orig 0xf1140
{
	edict_t* pentEntity;

	if (pStartEntity)
		pentEntity = pStartEntity->edict();
	else
		pentEntity = NULL;

	pentEntity = FIND_ENTITY_IN_SPHERE(pentEntity, vecCenter, flRadius);

	if (!FNullEnt(pentEntity))
		return CBaseEntity::Instance(pentEntity);
	return NULL;
}


CBaseEntity* UTIL_FindEntityByString(CBaseEntity* pStartEntity, const char* szKeyword, const char* szValue) // orig 0xf1190
{
	edict_t* pentEntity;

	if (pStartEntity)
		pentEntity = pStartEntity->edict();
	else
		pentEntity = NULL;

	pentEntity = FIND_ENTITY_BY_STRING(pentEntity, szKeyword, szValue);

	if (!FNullEnt(pentEntity))
		return CBaseEntity::Instance(pentEntity);
	return NULL;
}

CBaseEntity* UTIL_FindEntityByClassname(CBaseEntity* pStartEntity, const char* szName) // orig 0xf11e0
{
	return UTIL_FindEntityByString(pStartEntity, "classname", szName);
}

CBaseEntity* UTIL_FindEntityByTargetname(CBaseEntity* pStartEntity, const char* szName) // orig 0xf1200
{
	return UTIL_FindEntityByString(pStartEntity, "targetname", szName);
}


CBaseEntity* UTIL_FindEntityGeneric(const char* szWhatever, Vector& vecSrc, float flRadius)
{
	CBaseEntity* pEntity = NULL;

	pEntity = UTIL_FindEntityByTargetname(NULL, szWhatever);
	if (pEntity)
		return pEntity;

	CBaseEntity* pSearch = NULL;
	float flMaxDist2 = flRadius * flRadius;
	while ((pSearch = UTIL_FindEntityByClassname(pSearch, szWhatever)) != NULL)
	{
		float flDist2 = (pSearch->pev->origin - vecSrc).Length();
		flDist2 = flDist2 * flDist2;
		if (flMaxDist2 > flDist2)
		{
			pEntity = pSearch;
			flMaxDist2 = flDist2;
		}
	}
	return pEntity;
}


// returns a CBaseEntity pointer to a player by index.  Only returns if the player is spawned and connected
// otherwise returns NULL
// Index is 1 based
CBaseEntity* UTIL_PlayerByIndex(int playerIndex) // orig 0xf12c0, the same but for the extra checks it makes on the edict
{
	CBaseEntity* pPlayer = NULL;

	if (playerIndex > 0 && playerIndex <= gpGlobals->maxClients)
	{
		edict_t* pPlayerEdict = INDEXENT(playerIndex);
		if (pPlayerEdict && 0 == pPlayerEdict->free)
		{
			pPlayer = CBaseEntity::Instance(pPlayerEdict);
		}
	}

	return pPlayer;
}


void UTIL_MakeVectors(const Vector& vecAngles) // orig 0xf1320
{
	MAKE_VECTORS(vecAngles);
}


void UTIL_MakeAimVectors(const Vector& vecAngles)
{
	float rgflVec[3];
	vecAngles.CopyToArray(rgflVec);
	rgflVec[0] = -rgflVec[0];
	MAKE_VECTORS(rgflVec);
}


#define SWAP(a, b, temp) ((temp) = (a), (a) = (b), (b) = (temp))

void UTIL_MakeInvVectors(const Vector& vec, globalvars_t* pgv)
{
	MAKE_VECTORS(vec);

	float tmp;
	pgv->v_right = pgv->v_right * -1;

	SWAP(pgv->v_forward.y, pgv->v_right.x, tmp);
	SWAP(pgv->v_forward.z, pgv->v_up.x, tmp);
	SWAP(pgv->v_right.z, pgv->v_up.y, tmp);
}


void UTIL_EmitAmbientSound(edict_t* entity, const Vector& vecOrigin, const char* samp, float vol, float attenuation, int fFlags, int pitch)
{
	float rgfl[3];
	vecOrigin.CopyToArray(rgfl);

	if (samp && *samp == '!')
	{
		char name[32];
		if (SENTENCEG_Lookup(samp, name) >= 0)
			EMIT_AMBIENT_SOUND(entity, rgfl, name, vol, attenuation, fFlags, pitch);
	}
	else
		EMIT_AMBIENT_SOUND(entity, rgfl, samp, vol, attenuation, fFlags, pitch);
}

static unsigned short FixedUnsigned16(float value, float scale)
{
	int output;

	output = value * scale;
	if (output < 0)
		output = 0;
	if (output > 0xFFFF)
		output = 0xFFFF;

	return (unsigned short)output;
}

static short FixedSigned16(float value, float scale)
{
	int output;

	output = value * scale;

	if (output > 32767)
		output = 32767;

	if (output < -32768)
		output = -32768;

	return (short)output;
}

// Shake the screen of all clients within radius
// radius == 0, shake all clients
// UNDONE: Allow caller to shake clients not ONGROUND?
// UNDONE: Fix falloff model (disabled)?
// UNDONE: Affect user controls?
void UTIL_ScreenShake(const Vector& center, float amplitude, float frequency, float duration, float radius)
{
	int i;
	float localAmplitude;
	ScreenShake shake;

	shake.duration = FixedUnsigned16(duration, 1 << 12);  // 4.12 fixed
	shake.frequency = FixedUnsigned16(frequency, 1 << 8); // 8.8 fixed

	for (i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);

		if (!pPlayer || (pPlayer->pev->flags & FL_ONGROUND) == 0) // Don't shake if not onground
			continue;

		localAmplitude = 0;

		if (radius <= 0)
			localAmplitude = amplitude;
		else
		{
			Vector delta = center - pPlayer->pev->origin;
			float distance = delta.Length();

			// Had to get rid of this falloff - it didn't work well
			if (distance < radius)
				localAmplitude = amplitude; //radius - distance;
		}
		if (0 != localAmplitude)
		{
			shake.amplitude = FixedUnsigned16(localAmplitude, 1 << 12); // 4.12 fixed

			MESSAGE_BEGIN(MSG_ONE, gmsgShake, NULL, pPlayer->edict()); // use the magic #1 for "one client"

			WRITE_SHORT(shake.amplitude); // shake amount
			WRITE_SHORT(shake.duration);  // shake lasts this long
			WRITE_SHORT(shake.frequency); // shake noise frequency

			MESSAGE_END();
		}
	}
}



void UTIL_ScreenShakeAll(const Vector& center, float amplitude, float frequency, float duration)
{
	UTIL_ScreenShake(center, amplitude, frequency, duration, 0);
}


void UTIL_ScreenFadeBuild(ScreenFade& fade, const Vector& color, float fadeTime, float fadeHold, int alpha, int flags)
{
	fade.duration = FixedUnsigned16(fadeTime, 1 << 12); // 4.12 fixed
	fade.holdTime = FixedUnsigned16(fadeHold, 1 << 12); // 4.12 fixed
	fade.r = (int)color.x;
	fade.g = (int)color.y;
	fade.b = (int)color.z;
	fade.a = alpha;
	fade.fadeFlags = flags;
}


void UTIL_ScreenFadeWrite(const ScreenFade& fade, CBaseEntity* pEntity)
{
	if (!pEntity || !pEntity->IsNetClient())
		return;

	MESSAGE_BEGIN(MSG_ONE, gmsgFade, NULL, pEntity->edict()); // use the magic #1 for "one client"

	WRITE_SHORT(fade.duration);	 // fade lasts this long
	WRITE_SHORT(fade.holdTime);	 // fade lasts this long
	WRITE_SHORT(fade.fadeFlags); // fade type (in / out)
	WRITE_BYTE(fade.r);			 // fade red
	WRITE_BYTE(fade.g);			 // fade green
	WRITE_BYTE(fade.b);			 // fade blue
	WRITE_BYTE(fade.a);			 // fade blue

	MESSAGE_END();
}


void UTIL_ScreenFadeAll(const Vector& color, float fadeTime, float fadeHold, int alpha, int flags)
{
	int i;
	ScreenFade fade;


	UTIL_ScreenFadeBuild(fade, color, fadeTime, fadeHold, alpha, flags);

	for (i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);

		UTIL_ScreenFadeWrite(fade, pPlayer);
	}
}


void UTIL_ScreenFade(CBaseEntity* pEntity, const Vector& color, float fadeTime, float fadeHold, int alpha, int flags)
{
	ScreenFade fade;

	UTIL_ScreenFadeBuild(fade, color, fadeTime, fadeHold, alpha, flags);
	UTIL_ScreenFadeWrite(fade, pEntity);
}


void UTIL_HudMessage(CBaseEntity* pEntity, const hudtextparms_t& textparms, const char* pMessage)
{
	if (!pEntity || !pEntity->IsNetClient())
		return;

	MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, NULL, pEntity->edict());
	WRITE_BYTE(TE_TEXTMESSAGE);
	WRITE_BYTE(textparms.channel & 0xFF);

	WRITE_SHORT(FixedSigned16(textparms.x, 1 << 13));
	WRITE_SHORT(FixedSigned16(textparms.y, 1 << 13));
	WRITE_BYTE(textparms.effect);

	WRITE_BYTE(textparms.r1);
	WRITE_BYTE(textparms.g1);
	WRITE_BYTE(textparms.b1);
	WRITE_BYTE(textparms.a1);

	WRITE_BYTE(textparms.r2);
	WRITE_BYTE(textparms.g2);
	WRITE_BYTE(textparms.b2);
	WRITE_BYTE(textparms.a2);

	WRITE_SHORT(FixedUnsigned16(textparms.fadeinTime, 1 << 8));
	WRITE_SHORT(FixedUnsigned16(textparms.fadeoutTime, 1 << 8));
	WRITE_SHORT(FixedUnsigned16(textparms.holdTime, 1 << 8));

	if (textparms.effect == 2)
		WRITE_SHORT(FixedUnsigned16(textparms.fxTime, 1 << 8));

	if (strlen(pMessage) < 512)
	{
		WRITE_STRING(pMessage);
	}
	else
	{
		char tmp[512];
		strncpy(tmp, pMessage, 511);
		tmp[511] = 0;
		WRITE_STRING(tmp);
	}
	MESSAGE_END();
}

void UTIL_HudMessageAll(const hudtextparms_t& textparms, const char* pMessage)
{
	int i;

	for (i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		if (pPlayer)
			UTIL_HudMessage(pPlayer, textparms, pMessage);
	}
}

void UTIL_ClientPrintAll(int msg_dest, const char* msg_name, const char* param1, const char* param2, const char* param3, const char* param4)
{
	MESSAGE_BEGIN(MSG_ALL, gmsgTextMsg);
	WRITE_BYTE(msg_dest);
	WRITE_STRING(msg_name);

	if (param1)
		WRITE_STRING(param1);
	if (param2)
		WRITE_STRING(param2);
	if (param3)
		WRITE_STRING(param3);
	if (param4)
		WRITE_STRING(param4);

	MESSAGE_END();
}

void ClientPrint(entvars_t* client, int msg_dest, const char* msg_name, const char* param1, const char* param2, const char* param3, const char* param4)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgTextMsg, NULL, client);
	WRITE_BYTE(msg_dest);
	WRITE_STRING(msg_name);

	if (param1)
		WRITE_STRING(param1);
	if (param2)
		WRITE_STRING(param2);
	if (param3)
		WRITE_STRING(param3);
	if (param4)
		WRITE_STRING(param4);

	MESSAGE_END();
}

void UTIL_SayText(const char* pText, CBaseEntity* pEntity)
{
	if (!pEntity->IsNetClient())
		return;

	MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, pEntity->edict());
	WRITE_BYTE(pEntity->entindex());
	WRITE_STRING(pText);
	MESSAGE_END();
}

void UTIL_SayTextAll(const char* pText, CBaseEntity* pEntity)
{
	MESSAGE_BEGIN(MSG_ALL, gmsgSayText, NULL);
	WRITE_BYTE(pEntity->entindex());
	WRITE_STRING(pText);
	MESSAGE_END();
}


char* UTIL_dtos1(int d) // orig 0xf1c30. VERIFY: the only one of the four left in rc2.dll; which it is can't be told
{
	static char buf[8];
	sprintf(buf, "%d", d);
	return buf;
}

char* UTIL_dtos2(int d)
{
	static char buf[8];
	sprintf(buf, "%d", d);
	return buf;
}

char* UTIL_dtos3(int d)
{
	static char buf[8];
	sprintf(buf, "%d", d);
	return buf;
}

char* UTIL_dtos4(int d)
{
	static char buf[8];
	sprintf(buf, "%d", d);
	return buf;
}

void UTIL_ShowMessage(const char* pString, CBaseEntity* pEntity)
{
	if (!pEntity || !pEntity->IsNetClient())
		return;

	MESSAGE_BEGIN(MSG_ONE, gmsgHudText, NULL, pEntity->edict());
	WRITE_STRING(pString);
	MESSAGE_END();
}


void UTIL_ShowMessageAll(const char* pString)
{
	int i;

	// loop through all players

	for (i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		if (pPlayer)
			UTIL_ShowMessage(pString, pPlayer);
	}
}

// Rocket Crowbar 2: every trace first changes some entities' solidity, for the entity it
// ignores, and puts it back after (UTIL_RC2PreTrace, UTIL_RC2PostTrace). Each entity keeps its
// own solid meanwhile in CBaseEntity's m_iRC2Base6c (orig +0x6c).
//   - an entity whose IsSeeThroughFor(pentIgnore) is true is traced as SOLID_BBOX;
//   - when pentIgnore is a player, every entity whose IsViewOfPlayer(that player) is true is
//     traced as SOLID_NOT.
// Names ours.
static void UTIL_RC2SetTraceSolid(edict_t* pentIgnore, bool fRestore)
{
	for (int i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent))
			continue;
		CBaseEntity* pEntity = CBaseEntity::Instance(pent);
		if (pEntity && pEntity->IsSeeThroughFor(pentIgnore))
		{
			if (fRestore)
				pEntity->pev->solid = pEntity->m_iRC2Base6c;
			else
			{
				pEntity->m_iRC2Base6c = pEntity->pev->solid;
				pEntity->pev->solid = SOLID_BBOX;
			}
		}
	}

	if (!UTIL_IsValidEntity(pentIgnore))
		return;

	CBaseEntity* pIgnore = CBaseEntity::Instance(pentIgnore);
	if (!pIgnore || !pIgnore->IsPlayer())
		return;

	for (int i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent))
			continue;
		CBaseEntity* pEntity = CBaseEntity::Instance(pent);
		if (pEntity && pEntity->IsViewOfPlayer(pIgnore))
		{
			if (fRestore)
				pEntity->pev->solid = pEntity->m_iRC2Base6c;
			else
			{
				// one that is both keeps its first saved solid
				if (!pEntity->IsSeeThroughFor(pentIgnore))
					pEntity->m_iRC2Base6c = pEntity->pev->solid;
				pEntity->pev->solid = SOLID_NOT;
			}
		}
	}
}

void UTIL_RC2PreTrace(edict_t* pentIgnore) // orig 0xf1d50
{
	UTIL_RC2SetTraceSolid(pentIgnore, false);
}

void UTIL_RC2PostTrace(edict_t* pentIgnore) // orig 0xf1eb0
{
	UTIL_RC2SetTraceSolid(pentIgnore, true);
}

// Overloaded to add IGNORE_GLASS
void UTIL_TraceLine(const Vector& vecStart, const Vector& vecEnd, IGNORE_MONSTERS igmon, IGNORE_GLASS ignoreGlass, edict_t* pentIgnore, TraceResult* ptr) // orig 0xf1fe0
{
	UTIL_RC2PreTrace(pentIgnore);
	//TODO: define constants
	TRACE_LINE(vecStart, vecEnd, (igmon == ignore_monsters ? 1 : 0) | (ignore_glass == ignoreGlass ? 0x100 : 0), pentIgnore, ptr);
	UTIL_RC2PostTrace(pentIgnore);
}


void UTIL_TraceLine(const Vector& vecStart, const Vector& vecEnd, IGNORE_MONSTERS igmon, edict_t* pentIgnore, TraceResult* ptr) // orig 0xf2030
{
	UTIL_RC2PreTrace(pentIgnore);
	TRACE_LINE(vecStart, vecEnd, (igmon == ignore_monsters ? 1 : 0), pentIgnore, ptr);
	UTIL_RC2PostTrace(pentIgnore);
}


void UTIL_TraceHull(const Vector& vecStart, const Vector& vecEnd, IGNORE_MONSTERS igmon, int hullNumber, edict_t* pentIgnore, TraceResult* ptr) // orig 0xf2070
{
	UTIL_RC2PreTrace(pentIgnore);
	TRACE_HULL(vecStart, vecEnd, (igmon == ignore_monsters ? 1 : 0), hullNumber, pentIgnore, ptr);
	UTIL_RC2PostTrace(pentIgnore);
}

void UTIL_TraceModel(const Vector& vecStart, const Vector& vecEnd, int hullNumber, edict_t* pentModel, TraceResult* ptr) // orig 0xf20b0
{
	UTIL_RC2PreTrace(pentModel);
	g_engfuncs.pfnTraceModel(vecStart, vecEnd, hullNumber, pentModel, ptr);
	UTIL_RC2PostTrace(pentModel);
}


TraceResult UTIL_GetGlobalTrace()
{
	TraceResult tr;

	tr.fAllSolid = gpGlobals->trace_allsolid;
	tr.fStartSolid = gpGlobals->trace_startsolid;
	tr.fInOpen = gpGlobals->trace_inopen;
	tr.fInWater = gpGlobals->trace_inwater;
	tr.flFraction = gpGlobals->trace_fraction;
	tr.flPlaneDist = gpGlobals->trace_plane_dist;
	tr.pHit = gpGlobals->trace_ent;
	tr.vecEndPos = gpGlobals->trace_endpos;
	tr.vecPlaneNormal = gpGlobals->trace_plane_normal;
	tr.iHitgroup = gpGlobals->trace_hitgroup;
	return tr;
}


void UTIL_SetSize(entvars_t* pev, const Vector& vecMin, const Vector& vecMax)
{
	SET_SIZE(ENT(pev), vecMin, vecMax);
}


float UTIL_VecToYaw(const Vector& vec) // orig 0xf21e0
{
	return VEC_TO_YAW(vec);
}


void UTIL_SetOrigin(entvars_t* pev, const Vector& vecOrigin)
{
	edict_t* ent = ENT(pev);
	if (ent)
		SET_ORIGIN(ent, vecOrigin);
}

void UTIL_ParticleEffect(const Vector& vecOrigin, const Vector& vecDirection, unsigned int ulColor, unsigned int ulCount)
{
	PARTICLE_EFFECT(vecOrigin, vecDirection, (float)ulColor, (float)ulCount);
}


float UTIL_Approach(float target, float value, float speed)
{
	float delta = target - value;

	if (delta > speed)
		value += speed;
	else if (delta < -speed)
		value -= speed;
	else
		value = target;

	return value;
}


// Rocket Crowbar 2 fixed SDK 2.3's bug here (halflife-updated keeps it): value is wrapped
// from value, not from target.
float UTIL_ApproachAngle(float target, float value, float speed) // orig 0xf22a0
{
	target = UTIL_AngleMod(target);
	value = UTIL_AngleMod(value);

	float delta = target - value;

	// Speed is assumed to be positive
	if (speed < 0)
		speed = -speed;

	if (delta < -180)
		delta += 360;
	else if (delta > 180)
		delta -= 360;

	if (delta > speed)
		value += speed;
	else if (delta < -speed)
		value -= speed;
	else
		value = target;

	return value;
}


float UTIL_AngleDistance(float next, float cur)
{
	float delta = next - cur;

	if (delta < -180)
		delta += 360;
	else if (delta > 180)
		delta -= 360;

	return delta;
}


float UTIL_SplineFraction(float value, float scale) // orig 0xf2370
{
	value = scale * value;
	float valueSquared = value * value;

	// Nice little ease-in, ease-out spline-like curve
	return 3 * valueSquared - 2 * valueSquared * value;
}


char* UTIL_VarArgs(const char* format, ...)
{
	va_list argptr;
	static char string[1024];

	va_start(argptr, format);
	vsprintf(string, format, argptr);
	va_end(argptr);

	return string;
}

Vector UTIL_GetAimVector(edict_t* pent, float flSpeed)
{
	Vector tmp;
	GET_AIM_VECTOR(pent, flSpeed, tmp);
	return tmp;
}

bool UTIL_IsMasterTriggered(string_t sMaster, CBaseEntity* pActivator) // orig 0xf23f0
{
	if (!FStringNull(sMaster))
	{
		edict_t* pentTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(sMaster));

		if (!FNullEnt(pentTarget))
		{
			CBaseEntity* pMaster = CBaseEntity::Instance(pentTarget);
			if (pMaster && (pMaster->ObjectCaps() & FCAP_MASTER) != 0)
				return pMaster->IsTriggered(pActivator);
		}

		ALERT(at_console, "Master was null or not a master!\n");
	}

	// if this isn't a master entity, just say yes.
	return true;
}

// Rocket Crowbar 2: the violence_hblood/violence_ablood cvars aren't asked
bool UTIL_ShouldShowBlood(int color) // orig 0xf2470
{
	return color != DONT_BLEED;
}

int UTIL_PointContents(const Vector& vec) // orig 0xf2480
{
	return POINT_CONTENTS(vec);
}

void UTIL_BloodStream(const Vector& origin, const Vector& direction, int color, int amount)
{
	if (!UTIL_ShouldShowBlood(color))
		return;

	if (g_Language == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED)
		color = 0;


	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, origin);
	WRITE_BYTE(TE_BLOODSTREAM);
	WRITE_COORD(origin.x);
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z);
	WRITE_COORD(direction.x);
	WRITE_COORD(direction.y);
	WRITE_COORD(direction.z);
	WRITE_BYTE(color);
	WRITE_BYTE(V_min(amount, 255));
	MESSAGE_END();
}

// Rocket Crowbar 2: no German low violence, and the blood is always doubled
void UTIL_BloodDrips(const Vector& origin, const Vector& direction, int color, int amount) // orig 0xf2540
{
	if (!UTIL_ShouldShowBlood(color))
		return;

	if (color == DONT_BLEED || amount == 0)
		return;

	// scale up blood effect in multiplayer for better visibility
	amount *= 2;

	if (amount > 255)
		amount = 255;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, origin);
	WRITE_BYTE(TE_BLOODSPRITE);
	WRITE_COORD(origin.x); // pos
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z);
	WRITE_SHORT(g_sModelIndexBloodSpray);		  // initial sprite model
	WRITE_SHORT(g_sModelIndexBloodDrop);		  // droplet sprite models
	WRITE_BYTE(color);							  // color index into host_basepal
	WRITE_BYTE(V_min(V_max(3, amount / 10), 16)); // size
	MESSAGE_END();
}

Vector UTIL_RandomBloodVector()
{
	Vector direction;

	direction.x = RANDOM_FLOAT(-1, 1);
	direction.y = RANDOM_FLOAT(-1, 1);
	direction.z = RANDOM_FLOAT(0, 1);

	return direction;
}


void UTIL_BloodDecalTrace(TraceResult* pTrace, int bloodColor)
{
	if (UTIL_ShouldShowBlood(bloodColor))
	{
		if (bloodColor == BLOOD_COLOR_RED)
			UTIL_DecalTrace(pTrace, DECAL_BLOOD1 + RANDOM_LONG(0, 5));
		else
			UTIL_DecalTrace(pTrace, DECAL_YBLOOD1 + RANDOM_LONG(0, 5));
	}
}


void UTIL_DecalTrace(TraceResult* pTrace, int decalNumber)
{
	short entityIndex;
	int index;
	int message;

	if (decalNumber < 0)
		return;

	index = gDecals[decalNumber].index;

	if (index < 0)
		return;

	if (pTrace->flFraction == 1.0)
		return;

	// Only decal BSP models
	if (pTrace->pHit)
	{
		CBaseEntity* pEntity = CBaseEntity::Instance(pTrace->pHit);
		if (pEntity && !pEntity->IsBSPModel())
			return;
		entityIndex = ENTINDEX(pTrace->pHit);
	}
	else
		entityIndex = 0;

	message = TE_DECAL;
	if (entityIndex != 0)
	{
		if (index > 255)
		{
			message = TE_DECALHIGH;
			index -= 256;
		}
	}
	else
	{
		message = TE_WORLDDECAL;
		if (index > 255)
		{
			message = TE_WORLDDECALHIGH;
			index -= 256;
		}
	}

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(message);
	WRITE_COORD(pTrace->vecEndPos.x);
	WRITE_COORD(pTrace->vecEndPos.y);
	WRITE_COORD(pTrace->vecEndPos.z);
	WRITE_BYTE(index);
	if (0 != entityIndex)
		WRITE_SHORT(entityIndex);
	MESSAGE_END();
}

/*
==============
UTIL_PlayerDecalTrace

A player is trying to apply his custom decal for the spray can.
Tell connected clients to display it, or use the default spray can decal
if the custom can't be loaded.
==============
*/
void UTIL_PlayerDecalTrace(TraceResult* pTrace, int playernum, int decalNumber, bool bIsCustom)
{
	int index;

	if (!bIsCustom)
	{
		if (decalNumber < 0)
			return;

		index = gDecals[decalNumber].index;
		if (index < 0)
			return;
	}
	else
		index = decalNumber;

	if (pTrace->flFraction == 1.0)
		return;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_PLAYERDECAL);
	WRITE_BYTE(playernum);
	WRITE_COORD(pTrace->vecEndPos.x);
	WRITE_COORD(pTrace->vecEndPos.y);
	WRITE_COORD(pTrace->vecEndPos.z);
	WRITE_SHORT((short)ENTINDEX(pTrace->pHit));
	WRITE_BYTE(index);
	MESSAGE_END();
}

void UTIL_GunshotDecalTrace(TraceResult* pTrace, int decalNumber)
{
	if (decalNumber < 0)
		return;

	int index = gDecals[decalNumber].index;
	if (index < 0)
		return;

	if (pTrace->flFraction == 1.0)
		return;

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pTrace->vecEndPos);
	WRITE_BYTE(TE_GUNSHOTDECAL);
	WRITE_COORD(pTrace->vecEndPos.x);
	WRITE_COORD(pTrace->vecEndPos.y);
	WRITE_COORD(pTrace->vecEndPos.z);
	WRITE_SHORT((short)ENTINDEX(pTrace->pHit));
	WRITE_BYTE(index);
	MESSAGE_END();
}


void UTIL_Sparks(const Vector& position)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, position);
	WRITE_BYTE(TE_SPARKS);
	WRITE_COORD(position.x);
	WRITE_COORD(position.y);
	WRITE_COORD(position.z);
	MESSAGE_END();
}


void UTIL_Ricochet(const Vector& position, float scale)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, position);
	WRITE_BYTE(TE_ARMOR_RICOCHET);
	WRITE_COORD(position.x);
	WRITE_COORD(position.y);
	WRITE_COORD(position.z);
	WRITE_BYTE((int)(scale * 10));
	MESSAGE_END();
}


bool UTIL_TeamsMatch(const char* pTeamName1, const char* pTeamName2)
{
	// Everyone matches unless it's teamplay
	if (!g_pGameRules->IsTeamplay())
		return true;

	// Both on a team?
	if (*pTeamName1 != 0 && *pTeamName2 != 0)
	{
		if (!stricmp(pTeamName1, pTeamName2)) // Same Team?
			return true;
	}

	return false;
}


void UTIL_StringToVector(float* pVector, const char* pString)
{
	char *pstr, *pfront, tempString[128];
	int j;

	strncpy(tempString, pString, sizeof(tempString));
	tempString[sizeof(tempString) - 1] = '\0';
	pstr = pfront = tempString;

	for (j = 0; j < 3; j++) // lifted from pr_edict.c
	{
		pVector[j] = atof(pfront);

		while ('\0' != *pstr && *pstr != ' ')
			pstr++;
		if ('\0' == *pstr)
			break;
		pstr++;
		pfront = pstr;
	}
	if (j < 2)
	{
		/*
		ALERT( at_error, "Bad field in entity!! %s:%s == \"%s\"\n",
			pkvd->szClassName, pkvd->szKeyName, pkvd->szValue );
		*/
		for (j = j + 1; j < 3; j++)
			pVector[j] = 0;
	}
}


void UTIL_StringToIntArray(int* pVector, int count, const char* pString)
{
	char *pstr, *pfront, tempString[128];
	int j;

	strncpy(tempString, pString, sizeof(tempString));
	tempString[sizeof(tempString) - 1] = '\0';
	pstr = pfront = tempString;

	for (j = 0; j < count; j++) // lifted from pr_edict.c
	{
		pVector[j] = atoi(pfront);

		while ('\0' != *pstr && *pstr != ' ')
			pstr++;
		if ('\0' == *pstr)
			break;
		pstr++;
		pfront = pstr;
	}

	for (j++; j < count; j++)
	{
		pVector[j] = 0;
	}
}

Vector UTIL_ClampVectorToBox(const Vector& input, const Vector& clampSize)
{
	Vector sourceVector = input;

	if (sourceVector.x > clampSize.x)
		sourceVector.x -= clampSize.x;
	else if (sourceVector.x < -clampSize.x)
		sourceVector.x += clampSize.x;
	else
		sourceVector.x = 0;

	if (sourceVector.y > clampSize.y)
		sourceVector.y -= clampSize.y;
	else if (sourceVector.y < -clampSize.y)
		sourceVector.y += clampSize.y;
	else
		sourceVector.y = 0;

	if (sourceVector.z > clampSize.z)
		sourceVector.z -= clampSize.z;
	else if (sourceVector.z < -clampSize.z)
		sourceVector.z += clampSize.z;
	else
		sourceVector.z = 0;

	return sourceVector.Normalize();
}


float UTIL_WaterLevel(const Vector& position, float minz, float maxz)
{
	Vector midUp = position;
	midUp.z = minz;

	if (UTIL_PointContents(midUp) != CONTENTS_WATER)
		return minz;

	midUp.z = maxz;
	if (UTIL_PointContents(midUp) == CONTENTS_WATER)
		return maxz;

	float diff = maxz - minz;
	while (diff > 1.0)
	{
		midUp.z = minz + diff / 2.0;
		if (UTIL_PointContents(midUp) == CONTENTS_WATER)
		{
			minz = midUp.z;
		}
		else
		{
			maxz = midUp.z;
		}
		diff = maxz - minz;
	}

	return midUp.z;
}

void UTIL_Bubbles(Vector mins, Vector maxs, int count)
{
	Vector mid = (mins + maxs) * 0.5;

	float flHeight = UTIL_WaterLevel(mid, mid.z, mid.z + 1024);
	flHeight = flHeight - mins.z;

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, mid);
	WRITE_BYTE(TE_BUBBLES);
	WRITE_COORD(mins.x); // mins
	WRITE_COORD(mins.y);
	WRITE_COORD(mins.z);
	WRITE_COORD(maxs.x); // maxz
	WRITE_COORD(maxs.y);
	WRITE_COORD(maxs.z);
	WRITE_COORD(flHeight); // height
	WRITE_SHORT(g_sModelIndexBubbles);
	WRITE_BYTE(count); // count
	WRITE_COORD(8);	   // speed
	MESSAGE_END();
}

void UTIL_BubbleTrail(Vector from, Vector to, int count)
{
	float flHeight = UTIL_WaterLevel(from, from.z, from.z + 256);
	flHeight = flHeight - from.z;

	if (flHeight < 8)
	{
		flHeight = UTIL_WaterLevel(to, to.z, to.z + 256);
		flHeight = flHeight - to.z;
		if (flHeight < 8)
			return;

		// UNDONE: do a ploink sound
		flHeight = flHeight + to.z - from.z;
	}

	if (count > 255)
		count = 255;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BUBBLETRAIL);
	WRITE_COORD(from.x); // mins
	WRITE_COORD(from.y);
	WRITE_COORD(from.z);
	WRITE_COORD(to.x); // maxz
	WRITE_COORD(to.y);
	WRITE_COORD(to.z);
	WRITE_COORD(flHeight); // height
	WRITE_SHORT(g_sModelIndexBubbles);
	WRITE_BYTE(count); // count
	WRITE_COORD(8);	   // speed
	MESSAGE_END();
}


void UTIL_Remove(CBaseEntity* pEntity) // orig 0xf2fa0
{
	if (!pEntity)
		return;

	pEntity->UpdateOnRemove();
	pEntity->pev->flags |= FL_KILLME;
	pEntity->pev->targetname = 0;
}


// Rocket Crowbar 2: also a private entity other than the world, and client slots go by
// g_iRC2ClientBits (below)
bool UTIL_IsValidEntity(edict_t* pent) // orig 0xf2fe0
{
	if (!pent || 0 != pent->free || (pent->v.flags & FL_KILLME) != 0)
		return false;

	if (!pent->pvPrivateData || 0 == OFFSET(pent))
		return false;

	const int iIndex = ENTINDEX(pent);
	if (iIndex <= 0 || iIndex > gpGlobals->maxClients)
		return true;

	// Original bug kept: "!" binds to the mask alone, so with no bits set the first client's
	// slot is invalid and otherwise every client slot is valid
	if (((g_iRC2ClientBits == 0) ? 1 : 0) & (1 << (iIndex - 1)))
		return false;

	return true;
}


// Rocket Crowbar 2 (as RC 1.9): once per classname, by the precache list
void UTIL_PrecacheOther(const char* szClassname) // orig 0xf3060
{
	edict_t* pent;

	if (IsClassnamePrecachedNoInvalid(szClassname))
		return;

	pent = CREATE_NAMED_ENTITY(MAKE_STRING(szClassname));
	if (FNullEnt(pent))
	{
		ALERT(at_console, "NULL Ent in UTIL_PrecacheOther\n");
		return;
	}

	CBaseEntity* pEntity = CBaseEntity::Instance(VARS(pent));
	if (pEntity)
		pEntity->Precache();
	REMOVE_ENTITY(pent);
	AddToPrecachedList(szClassname);
}

//=========================================================
// UTIL_LogPrintf - Prints a logged message to console.
// Preceded by LOG: ( timestamp ) < message >
//=========================================================
void UTIL_LogPrintf(const char* fmt, ...)
{
	va_list argptr;
	static char string[1024];

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	// Print to server console
	ALERT(at_logged, "%s", string);
}

//=========================================================
// UTIL_DotPoints - returns the dot product of a line from
// src to check and vecdir.
//=========================================================
float UTIL_DotPoints(const Vector& vecSrc, const Vector& vecCheck, const Vector& vecDir)
{
	Vector2D vec2LOS;

	vec2LOS = (vecCheck - vecSrc).Make2D();
	vec2LOS = vec2LOS.Normalize();

	return DotProduct(vec2LOS, (vecDir.Make2D()));
}


//=========================================================
// UTIL_StripToken - for redundant keynames
//=========================================================
void UTIL_StripToken(const char* pKey, char* pDest, int nLen)
{
	int i = 0;

	while (i < nLen - 1 && '\0' != pKey[i] && pKey[i] != '#')
	{
		pDest[i] = pKey[i];
		i++;
	}
	pDest[i] = 0;
}


// --------------------------------------------------------------
//
// CSave
//
// --------------------------------------------------------------
// The size of one element of each field type IN MEMORY: the stride of an array member (m_rgpPlayerItems, m_hSquadMember,
// m_rgEntities) and the span DataEmpty tests. On a 64-bit build the pointer types (CLASSPTR, EVARS, EDICT, POINTER) are
// 8 bytes and an EHANDLE 16, while the save file still holds one int per element — see gFileSizes.
static int gSizes[FIELD_TYPECOUNT] =
	{
		sizeof(float),		 // FIELD_FLOAT
		sizeof(int),		 // FIELD_STRING
		sizeof(EOFFSET),	 // FIELD_ENTITY
		sizeof(CBaseEntity*), // FIELD_CLASSPTR
		sizeof(EHANDLE),	 // FIELD_EHANDLE
		sizeof(entvars_t*),	 // FIELD_entvars_t
		sizeof(edict_t*),	 // FIELD_EDICT
		sizeof(float) * 3,	 // FIELD_VECTOR
		sizeof(float) * 3,	 // FIELD_POSITION_VECTOR
		sizeof(int*),		 // FIELD_POINTER
		sizeof(int),		 // FIELD_INTEGER
#ifdef GNUC
		sizeof(int*) * 2, // FIELD_FUNCTION
#else
		sizeof(int*), // FIELD_FUNCTION
#endif
		sizeof(byte),		   // FIELD_BOOLEAN
		sizeof(short),		   // FIELD_SHORT
		sizeof(char),		   // FIELD_CHARACTER
		sizeof(float),		   // FIELD_TIME
		sizeof(int),		   // FIELD_MODELNAME
		sizeof(int),		   // FIELD_SOUNDNAME
		sizeof(std::uint64_t), //FIELD_INT64
};

// The size of one element of each field type IN THE SAVE FILE: what CSave::WriteFields writes per element and the stride
// CRestore::ReadField steps the input by. Every entity reference (CLASSPTR, EVARS, EDICT, ENTITY, EHANDLE) is saved as
// one int (an entity table index) whatever its in-memory size, so the two tables differ on 64-bit builds; the original
// SDK used gSizes for both, which restored the second element of a pointer array 4 bytes into the first (a dangling
// m_rgpPlayerItems[1] after any load with two weapons crashed GetWeaponData) and read EHANDLE arrays at 16-byte strides.
// FIELD_STRING and FIELD_FUNCTION are written as strings and stepped by ReadField's own string walk, not by this table.
static int gFileSizes[FIELD_TYPECOUNT] =
	{
		sizeof(float),	   // FIELD_FLOAT
		sizeof(int),	   // FIELD_STRING (unused: strings)
		sizeof(int),	   // FIELD_ENTITY
		sizeof(int),	   // FIELD_CLASSPTR
		sizeof(int),	   // FIELD_EHANDLE
		sizeof(int),	   // FIELD_entvars_t
		sizeof(int),	   // FIELD_EDICT
		sizeof(float) * 3, // FIELD_VECTOR
		sizeof(float) * 3, // FIELD_POSITION_VECTOR
		sizeof(int),	   // FIELD_POINTER (WriteInt: the low int of the address; never restored to anything useful)
		sizeof(int),	   // FIELD_INTEGER
		sizeof(int),	   // FIELD_FUNCTION (unused: a name string)
		sizeof(byte),		   // FIELD_BOOLEAN
		sizeof(short),		   // FIELD_SHORT
		sizeof(char),		   // FIELD_CHARACTER
		sizeof(float),		   // FIELD_TIME
		sizeof(int),		   // FIELD_MODELNAME (unused: strings)
		sizeof(int),		   // FIELD_SOUNDNAME (unused: strings)
		sizeof(std::uint64_t), //FIELD_INT64
};


// Base class includes common SAVERESTOREDATA pointer, and manages the entity table
CSaveRestoreBuffer::CSaveRestoreBuffer(SAVERESTOREDATA& data) // orig 0xf5dd0
	: m_data(data)
{
}


CSaveRestoreBuffer::~CSaveRestoreBuffer() = default; // orig 0xf5de0

int CSaveRestoreBuffer::EntityIndex(CBaseEntity* pEntity)
{
	if (pEntity == NULL)
		return -1;
	return EntityIndex(pEntity->pev);
}


int CSaveRestoreBuffer::EntityIndex(entvars_t* pevLookup)
{
	if (pevLookup == NULL)
		return -1;
	return EntityIndex(ENT(pevLookup));
}

int CSaveRestoreBuffer::EntityIndex(EOFFSET eoLookup)
{
	return EntityIndex(ENT(eoLookup));
}


int CSaveRestoreBuffer::EntityIndex(edict_t* pentLookup)
{
	if (pentLookup == NULL)
		return -1;

	int i;
	ENTITYTABLE* pTable;

	for (i = 0; i < m_data.tableCount; i++)
	{
		pTable = m_data.pTable + i;
		if (pTable->pent == pentLookup)
			return i;
	}
	return -1;
}


edict_t* CSaveRestoreBuffer::EntityFromIndex(int entityIndex)
{
	if (entityIndex < 0)
		return NULL;

	int i;
	ENTITYTABLE* pTable;

	for (i = 0; i < m_data.tableCount; i++)
	{
		pTable = m_data.pTable + i;
		if (pTable->id == entityIndex)
			return pTable->pent;
	}
	return NULL;
}


int CSaveRestoreBuffer::EntityFlagsSet(int entityIndex, int flags)
{
	if (entityIndex < 0)
		return 0;
	if (entityIndex > m_data.tableCount)
		return 0;

	m_data.pTable[entityIndex].flags |= flags;

	return m_data.pTable[entityIndex].flags;
}


void CSaveRestoreBuffer::BufferRewind(int size)
{
	if (m_data.size < size)
		size = m_data.size;

	m_data.pCurrentData -= size;
	m_data.size -= size;
}

#ifndef WIN32
extern "C" {
unsigned _rotr(unsigned val, int shift)
{
	unsigned lobit;	 /* non-zero means lo bit set */
	unsigned num = val; /* number to rotate */

	shift &= 0x1f; /* modulo 32 -- this will also make
										   negative shifts work */

	while (shift--)
	{
		lobit = num & 1; /* get high bit */
		num >>= 1;		 /* shift right one bit */
		if (lobit)
			num |= 0x80000000; /* set hi bit if lo bit was set */
	}

	return num;
}
}
#endif

unsigned int CSaveRestoreBuffer::HashString(const char* pszToken)
{
	unsigned int hash = 0;

	while ('\0' != *pszToken)
		hash = _rotr(hash, 4) ^ *pszToken++;

	return hash;
}

unsigned short CSaveRestoreBuffer::TokenHash(const char* pszToken)
{
#if _DEBUG
	static int tokensparsed = 0;
	tokensparsed++;
#endif
	if (0 == m_data.tokenCount || nullptr == m_data.pTokens)
	{
		//if we're here it means trigger_changelevel is trying to actually save something when it's not supposed to.
		ALERT(at_error, "No token table array in TokenHash()!\n");
		return 0;
	}

	const unsigned short hash = (unsigned short)(HashString(pszToken) % (unsigned)m_data.tokenCount);

	for (int i = 0; i < m_data.tokenCount; i++)
	{
#if _DEBUG
		static bool beentheredonethat = false;
		if (i > 50 && !beentheredonethat)
		{
			beentheredonethat = true;
			ALERT(at_error, "CSaveRestoreBuffer :: TokenHash() is getting too full!\n");
		}
#endif

		int index = hash + i;
		if (index >= m_data.tokenCount)
			index -= m_data.tokenCount;

		if (!m_data.pTokens[index] || strcmp(pszToken, m_data.pTokens[index]) == 0)
		{
			m_data.pTokens[index] = (char*)pszToken;
			return index;
		}
	}

	// Token hash table full!!!
	// [Consider doing overflow table(s) after the main table & limiting linear hash table search]
	ALERT(at_error, "CSaveRestoreBuffer :: TokenHash() is COMPLETELY FULL!\n");
	return 0;
}

void CSave::WriteData(const char* pname, int size, const char* pdata) // orig 0xf6050
{
	BufferField(pname, size, pdata);
}


void CSave::WriteShort(const char* pname, const short* data, int count)
{
	BufferField(pname, sizeof(short) * count, (const char*)data);
}


void CSave::WriteInt(const char* pname, const int* data, int count) // orig 0xf6090
{
	BufferField(pname, sizeof(int) * count, (const char*)data);
}


void CSave::WriteFloat(const char* pname, const float* data, int count) // orig 0xf60b0
{
	BufferField(pname, sizeof(float) * count, (const char*)data);
}


void CSave::WriteTime(const char* pname, const float* data, int count)
{
	int i;
	Vector tmp, input;

	BufferHeader(pname, sizeof(float) * count);
	for (i = 0; i < count; i++)
	{
		float tmp = data[0];

		// Always encode time as a delta from the current time so it can be re-based if loaded in a new level
		// Times of 0 are never written to the file, so they will be restored as 0, not a relative time
		tmp -= m_data.time;

		BufferData((const char*)&tmp, sizeof(float));
		data++;
	}
}


void CSave::WriteString(const char* pname, const char* pdata)
{
#ifdef TOKENIZE
	short token = (short)TokenHash(pdata);
	WriteShort(pname, &token, 1);
#else
	BufferField(pname, strlen(pdata) + 1, pdata);
#endif
}


void CSave::WriteString(const char* pname, const int* stringId, int count)
{
	int i, size;

#ifdef TOKENIZE
	short token = (short)TokenHash(STRING(*stringId));
	WriteShort(pname, &token, 1);
#else
#if 0
	if ( count != 1 )
		ALERT( at_error, "No string arrays!\n" );
	WriteString( pname, (char *)STRING(*stringId) );
#endif

	size = 0;
	for (i = 0; i < count; i++)
		size += strlen(STRING(stringId[i])) + 1;

	BufferHeader(pname, size);
	for (i = 0; i < count; i++)
	{
		const char* pString = STRING(stringId[i]);
		BufferData(pString, strlen(pString) + 1);
	}
#endif
}


void CSave::WriteVector(const char* pname, const Vector& value)
{
	WriteVector(pname, &value.x, 1);
}


void CSave::WriteVector(const char* pname, const float* value, int count)
{
	BufferHeader(pname, sizeof(float) * 3 * count);
	BufferData((const char*)value, sizeof(float) * 3 * count);
}



void CSave::WritePositionVector(const char* pname, const Vector& value)
{
	if (0 != m_data.fUseLandmark)
	{
		Vector tmp = value - m_data.vecLandmarkOffset;
		WriteVector(pname, tmp);
	}

	WriteVector(pname, value);
}


void CSave::WritePositionVector(const char* pname, const float* value, int count)
{
	int i;
	Vector tmp, input;

	BufferHeader(pname, sizeof(float) * 3 * count);
	for (i = 0; i < count; i++)
	{
		Vector tmp(value[0], value[1], value[2]);

		if (0 != m_data.fUseLandmark)
			tmp = tmp - m_data.vecLandmarkOffset;

		BufferData((const char*)&tmp.x, sizeof(float) * 3);
		value += 3;
	}
}


void CSave::WriteFunction(const char* pname, void** data, int count)
{
	const char* functionName = NAME_FOR_FUNCTION((uintp)*data);
	if (functionName)
		BufferField(pname, strlen(functionName) + 1, functionName);
	else
		ALERT(at_error, "Invalid function pointer in entity!\n");
}


void EntvarsKeyvalue(entvars_t* pev, KeyValueData* pkvd)
{
	int i;
	TYPEDESCRIPTION* pField;

	for (i = 0; i < ENTVARS_COUNT; i++)
	{
		pField = &gEntvarsDescription[i];

		if (!stricmp(pField->fieldName, pkvd->szKeyName))
		{
			switch (pField->fieldType)
			{
			case FIELD_MODELNAME:
			case FIELD_SOUNDNAME:
			case FIELD_STRING:
				(*(int*)((char*)pev + pField->fieldOffset)) = ALLOC_STRING(pkvd->szValue);
				break;

			case FIELD_TIME:
			case FIELD_FLOAT:
				(*(float*)((char*)pev + pField->fieldOffset)) = atof(pkvd->szValue);
				break;

			case FIELD_INTEGER:
				(*(int*)((char*)pev + pField->fieldOffset)) = atoi(pkvd->szValue);
				break;

			case FIELD_POSITION_VECTOR:
			case FIELD_VECTOR:
				UTIL_StringToVector((float*)((char*)pev + pField->fieldOffset), pkvd->szValue);
				break;

			default:
			case FIELD_EVARS:
			case FIELD_CLASSPTR:
			case FIELD_EDICT:
			case FIELD_ENTITY:
			case FIELD_POINTER:
				ALERT(at_error, "Bad field in entity!!\n");
				break;
			}
			pkvd->fHandled = 1;
			return;
		}
	}
}



bool CSave::WriteEntVars(const char* pname, entvars_t* pev) // orig 0xf64e0
{
	return WriteFields(pname, pev, gEntvarsDescription, ENTVARS_COUNT);
}



bool CSave::WriteFields(const char* pname, void* pBaseData, TYPEDESCRIPTION* pFields, int fieldCount)
{
	int i, j, actualCount, emptyCount;
	TYPEDESCRIPTION* pTest;
	int entityArray[MAX_ENTITYARRAY];
	byte boolArray[MAX_ENTITYARRAY];

	// Precalculate the number of empty fields
	emptyCount = 0;
	for (i = 0; i < fieldCount; i++)
	{
		void* pOutputData;
		pOutputData = ((char*)pBaseData + pFields[i].fieldOffset);
		if (DataEmpty((const char*)pOutputData, pFields[i].fieldSize * gSizes[pFields[i].fieldType]))
			emptyCount++;
	}

	// Empty fields will not be written, write out the actual number of fields to be written
	actualCount = fieldCount - emptyCount;
	WriteInt(pname, &actualCount, 1);

	for (i = 0; i < fieldCount; i++)
	{
		void* pOutputData;
		pTest = &pFields[i];
		pOutputData = ((char*)pBaseData + pTest->fieldOffset);

		// UNDONE: Must we do this twice?
		if (DataEmpty((const char*)pOutputData, pTest->fieldSize * gSizes[pTest->fieldType]))
			continue;

		switch (pTest->fieldType)
		{
		case FIELD_FLOAT:
			WriteFloat(pTest->fieldName, (float*)pOutputData, pTest->fieldSize);
			break;
		case FIELD_TIME:
			WriteTime(pTest->fieldName, (float*)pOutputData, pTest->fieldSize);
			break;
		case FIELD_MODELNAME:
		case FIELD_SOUNDNAME:
		case FIELD_STRING:
			WriteString(pTest->fieldName, (int*)pOutputData, pTest->fieldSize);
			break;
		case FIELD_CLASSPTR:
		case FIELD_EVARS:
		case FIELD_EDICT:
		case FIELD_ENTITY:
		case FIELD_EHANDLE:
			if (pTest->fieldSize > MAX_ENTITYARRAY)
				ALERT(at_error, "Can't save more than %d entities in an array!!!\n", MAX_ENTITYARRAY);
			for (j = 0; j < pTest->fieldSize; j++)
			{
				switch (pTest->fieldType)
				{
				case FIELD_EVARS:
					entityArray[j] = EntityIndex(((entvars_t**)pOutputData)[j]);
					break;
				case FIELD_CLASSPTR:
					entityArray[j] = EntityIndex(((CBaseEntity**)pOutputData)[j]);
					break;
				case FIELD_EDICT:
					entityArray[j] = EntityIndex(((edict_t**)pOutputData)[j]);
					break;
				case FIELD_ENTITY:
					entityArray[j] = EntityIndex(((EOFFSET*)pOutputData)[j]);
					break;
				case FIELD_EHANDLE:
					entityArray[j] = EntityIndex((CBaseEntity*)(((EHANDLE*)pOutputData)[j]));
					break;
				}
			}
			WriteInt(pTest->fieldName, entityArray, pTest->fieldSize);
			break;
		case FIELD_POSITION_VECTOR:
			WritePositionVector(pTest->fieldName, (float*)pOutputData, pTest->fieldSize);
			break;
		case FIELD_VECTOR:
			WriteVector(pTest->fieldName, (float*)pOutputData, pTest->fieldSize);
			break;

		case FIELD_BOOLEAN:
		{
			//TODO: need to refactor save game stuff to make this cleaner and reusable
			//Convert booleans to bytes
			for (j = 0; j < pTest->fieldSize; j++)
			{
				boolArray[j] = ((bool*)pOutputData)[j] ? 1 : 0;
			}

			WriteData(pTest->fieldName, pTest->fieldSize, (char*)boolArray);
		}
		break;

		case FIELD_INTEGER:
			WriteInt(pTest->fieldName, (int*)pOutputData, pTest->fieldSize);
			break;

		case FIELD_INT64:
			WriteData(pTest->fieldName, sizeof(std::uint64_t) * pTest->fieldSize, ((char*)pOutputData));
			break;

		case FIELD_SHORT:
			WriteData(pTest->fieldName, 2 * pTest->fieldSize, ((char*)pOutputData));
			break;

		case FIELD_CHARACTER:
			WriteData(pTest->fieldName, pTest->fieldSize, ((char*)pOutputData));
			break;

		// For now, just write the address out, we're not going to change memory while doing this yet!
		case FIELD_POINTER:
			WriteInt(pTest->fieldName, (int*)(char*)pOutputData, pTest->fieldSize);
			break;

		case FIELD_FUNCTION:
			WriteFunction(pTest->fieldName, (void**)pOutputData, pTest->fieldSize);
			break;
		default:
			ALERT(at_error, "Bad field type\n");
		}
	}

	return true;
}


void CSave::BufferString(char* pdata, int len)
{
	char c = 0;

	BufferData(pdata, len); // Write the string
	BufferData(&c, 1);		// Write a null terminator
}


bool CSave::DataEmpty(const char* pdata, int size)
{
	for (int i = 0; i < size; i++)
	{
		if (0 != pdata[i])
			return false;
	}
	return true;
}


void CSave::BufferField(const char* pname, int size, const char* pdata)
{
	BufferHeader(pname, size);
	BufferData(pdata, size);
}


void CSave::BufferHeader(const char* pname, int size)
{
	short hashvalue = TokenHash(pname);
	if (size > 1 << (sizeof(short) * 8))
		ALERT(at_error, "CSave :: BufferHeader() size parameter exceeds 'short'!\n");
	BufferData((const char*)&size, sizeof(short));
	BufferData((const char*)&hashvalue, sizeof(short));
}


void CSave::BufferData(const char* pdata, int size)
{
	if (m_data.size + size > m_data.bufferSize)
	{
		ALERT(at_error, "Save/Restore overflow!\n");
		m_data.size = m_data.bufferSize;
		return;
	}

	memcpy(m_data.pCurrentData, pdata, size);
	m_data.pCurrentData += size;
	m_data.size += size;
}



// --------------------------------------------------------------
//
// CRestore
//
// --------------------------------------------------------------

int CRestore::ReadField(void* pBaseData, TYPEDESCRIPTION* pFields, int fieldCount, int startField, int size, char* pName, void* pData) // orig 0xf6920
{
	int i, j, stringCount, fieldNumber, entityIndex;
	TYPEDESCRIPTION* pTest;
	float timeData;
	Vector position;
	edict_t* pent;
	char* pString;

	position = Vector(0, 0, 0);

	if (0 != m_data.fUseLandmark)
		position = m_data.vecLandmarkOffset;

	for (i = 0; i < fieldCount; i++)
	{
		fieldNumber = (i + startField) % fieldCount;
		pTest = &pFields[fieldNumber];
		if (pTest->fieldName && !stricmp(pTest->fieldName, pName))
		{
			if (!m_global || (pTest->flags & FTYPEDESC_GLOBAL) == 0)
			{
				for (j = 0; j < pTest->fieldSize; j++)
				{
					void* pOutputData = ((char*)pBaseData + pTest->fieldOffset + (j * gSizes[pTest->fieldType]));
					void* pInputData = (char*)pData + j * gFileSizes[pTest->fieldType];

					switch (pTest->fieldType)
					{
					case FIELD_TIME:
						timeData = *(float*)pInputData;
						// Re-base time variables
						timeData += m_data.time;
						*((float*)pOutputData) = timeData;
						break;
					case FIELD_FLOAT:
						*((float*)pOutputData) = *(float*)pInputData;
						break;
					case FIELD_MODELNAME:
					case FIELD_SOUNDNAME:
					case FIELD_STRING:
						// Skip over j strings
						pString = (char*)pData;
						for (stringCount = 0; stringCount < j; stringCount++)
						{
							while ('\0' != *pString)
								pString++;
							pString++;
						}
						pInputData = pString;
						if (strlen((char*)pInputData) == 0)
							*((int*)pOutputData) = 0;
						else
						{
							int string;

							string = ALLOC_STRING((char*)pInputData);

							*((int*)pOutputData) = string;

							if (!FStringNull(string) && m_precache)
							{
								if (pTest->fieldType == FIELD_MODELNAME)
									PRECACHE_MODEL((char*)STRING(string));
								else if (pTest->fieldType == FIELD_SOUNDNAME)
									PRECACHE_SOUND((char*)STRING(string));
							}
						}
						break;
					case FIELD_EVARS:
						entityIndex = *(int*)pInputData;
						pent = EntityFromIndex(entityIndex);
						if (pent)
							*((entvars_t**)pOutputData) = VARS(pent);
						else
							*((entvars_t**)pOutputData) = NULL;
						break;
					case FIELD_CLASSPTR:
						entityIndex = *(int*)pInputData;
						pent = EntityFromIndex(entityIndex);
						if (pent)
							*((CBaseEntity**)pOutputData) = CBaseEntity::Instance(pent);
						else
							*((CBaseEntity**)pOutputData) = NULL;
						break;
					case FIELD_EDICT:
						entityIndex = *(int*)pInputData;
						pent = EntityFromIndex(entityIndex);
						*((edict_t**)pOutputData) = pent;
						break;
					case FIELD_EHANDLE:
						// Input and Output sizes are different!
						pInputData = (char*)pData + j * sizeof(int);
						entityIndex = *(int*)pInputData;
						pent = EntityFromIndex(entityIndex);
						if (pent)
							*((EHANDLE*)pOutputData) = CBaseEntity::Instance(pent);
						else
							*((EHANDLE*)pOutputData) = NULL;
						break;
					case FIELD_ENTITY:
						entityIndex = *(int*)pInputData;
						pent = EntityFromIndex(entityIndex);
						if (pent)
							*((EOFFSET*)pOutputData) = OFFSET(pent);
						else
							*((EOFFSET*)pOutputData) = 0;
						break;
					case FIELD_VECTOR:
						((float*)pOutputData)[0] = ((float*)pInputData)[0];
						((float*)pOutputData)[1] = ((float*)pInputData)[1];
						((float*)pOutputData)[2] = ((float*)pInputData)[2];
						break;
					case FIELD_POSITION_VECTOR:
						((float*)pOutputData)[0] = ((float*)pInputData)[0] + position.x;
						((float*)pOutputData)[1] = ((float*)pInputData)[1] + position.y;
						((float*)pOutputData)[2] = ((float*)pInputData)[2] + position.z;
						break;

					case FIELD_BOOLEAN:
					{
						// Input and Output sizes are different!
						pOutputData = (char*)pOutputData + j * (sizeof(bool) - gSizes[pTest->fieldType]);
						const bool value = *((byte*)pInputData) != 0;

						*((bool*)pOutputData) = value;
					}
					break;

					case FIELD_INTEGER:
						*((int*)pOutputData) = *(int*)pInputData;
						break;

					case FIELD_INT64:
						*((std::uint64_t*)pOutputData) = *(std::uint64_t*)pInputData;
						break;

					case FIELD_SHORT:
						*((short*)pOutputData) = *(short*)pInputData;
						break;

					case FIELD_CHARACTER:
						*((char*)pOutputData) = *(char*)pInputData;
						break;

					case FIELD_POINTER:
						*((int*)pOutputData) = *(int*)pInputData;
						break;
					case FIELD_FUNCTION:
						// FIELD_FUNCTION's storage is a member function pointer
						// (gSizes[FIELD_FUNCTION] = sizeof(int*) * 2 on Itanium ABI).
						// Zero the whole slot so the adjustor word is 0 (non-virtual
						// member function), then write the resolved address into the
						// first pointer-sized word.
						memset(pOutputData, 0, gSizes[pTest->fieldType]);
						if (strlen((char*)pInputData) != 0)
							*((uintp*)pOutputData) = FUNCTION_FROM_NAME((char*)pInputData);
						break;

					default:
						ALERT(at_error, "Bad field type\n");
					}
				}
			}
#if 0
			else
			{
				ALERT( at_console, "Skipping global field %s\n", pName );
			}
#endif
			return fieldNumber;
		}
	}

	return -1;
}


bool CRestore::ReadEntVars(const char* pname, entvars_t* pev) // orig 0xf6c70
{
	return ReadFields(pname, pev, gEntvarsDescription, ENTVARS_COUNT);
}


bool CRestore::ReadFields(const char* pname, void* pBaseData, TYPEDESCRIPTION* pFields, int fieldCount)
{
	unsigned short i, token;
	int lastField, fileCount;
	HEADER header;

	i = ReadShort();
	ASSERT(i == sizeof(int)); // First entry should be an int

	token = ReadShort();

	// Check the struct name
	if (token != TokenHash(pname)) // Field Set marker
	{
		//		ALERT( at_error, "Expected %s found %s!\n", pname, BufferPointer() );
		BufferRewind(2 * sizeof(short));
		return false;
	}

	// Skip over the struct name
	fileCount = ReadInt(); // Read field count

	lastField = 0; // Make searches faster, most data is read/written in the same order

	// Clear out base data
	for (i = 0; i < fieldCount; i++)
	{
		// Don't clear global fields
		if (!m_global || (pFields[i].flags & FTYPEDESC_GLOBAL) == 0)
			memset(((char*)pBaseData + pFields[i].fieldOffset), 0, pFields[i].fieldSize * gSizes[pFields[i].fieldType]);
	}

	for (i = 0; i < fileCount; i++)
	{
		BufferReadHeader(&header);
		lastField = ReadField(pBaseData, pFields, fieldCount, lastField, header.size, m_data.pTokens[header.token], header.pData);
		lastField++;
	}

	return true;
}


void CRestore::BufferReadHeader(HEADER* pheader)
{
	ASSERT(pheader != NULL);
	pheader->size = ReadShort();	  // Read field size
	pheader->token = ReadShort();	  // Read field name token
	pheader->pData = BufferPointer(); // Field Data is next
	BufferSkipBytes(pheader->size);	  // Advance to next field
}


short CRestore::ReadShort()
{
	short tmp = 0;

	BufferReadBytes((char*)&tmp, sizeof(short));

	return tmp;
}

int CRestore::ReadInt()
{
	int tmp = 0;

	BufferReadBytes((char*)&tmp, sizeof(int));

	return tmp;
}

int CRestore::ReadNamedInt(const char* pName)
{
	HEADER header;

	BufferReadHeader(&header);
	return ((int*)header.pData)[0];
}

char* CRestore::ReadNamedString(const char* pName)
{
	HEADER header;

	BufferReadHeader(&header);
#ifdef TOKENIZE
	return (char*)(m_pdata->pTokens[*(short*)header.pData]);
#else
	return (char*)header.pData;
#endif
}


char* CRestore::BufferPointer() // orig 0xf6e80
{
	return m_data.pCurrentData;
}

void CRestore::BufferReadBytes(char* pOutput, int size)
{
	if (Empty())
		return;

	if ((m_data.size + size) > m_data.bufferSize)
	{
		ALERT(at_error, "Restore overflow!\n");
		m_data.size = m_data.bufferSize;
		return;
	}

	if (pOutput)
		memcpy(pOutput, m_data.pCurrentData, size);
	m_data.pCurrentData += size;
	m_data.size += size;
}


void CRestore::BufferSkipBytes(int bytes) // orig 0xf6f10
{
	BufferReadBytes(NULL, bytes);
}

int CRestore::BufferSkipZString()
{
	const int maxLen = m_data.bufferSize - m_data.size;

	int len = 0;
	char* pszSearch = m_data.pCurrentData;
	while ('\0' != *pszSearch++ && len < maxLen)
		len++;

	len++;

	BufferSkipBytes(len);

	return len;
}

bool CRestore::BufferCheckZString(const char* string)
{
	const int maxLen = m_data.bufferSize - m_data.size;
	const int len = strlen(string);
	if (len <= maxLen)
	{
		if (0 == strncmp(string, m_data.pCurrentData, len))
			return true;
	}
	return false;
}


//=========================================================
// Rocket Crowbar 2's own utility code (rc2.dll keeps it in util.cpp, between SDK 2.3's
// functions). Names ours unless marked.
//=========================================================

int g_iRC2ClientBits = 0; // orig .bss 0x101af2a8: a bit per client slot (client code sets them; CWorld::Precache clears them). Name p3's

// how many entities (other than the world) have this classname
int UTIL_CountEntitiesByClass(const char* szClassname) // orig 0xf30f0
{
	edict_t* pent = INDEXENT(1);
	int iCount = 0;

	if (!pent)
		return 0;

	for (int i = 1; i < gpGlobals->maxEntities; i++, pent++)
	{
		if (UTIL_IsValidEntity(pent) && 0 == strcmp(STRING(VARS(pent)->classname), szClassname))
			iCount++;
	}

	return iCount;
}

// one of the entities of this classname, picked at random
CBaseEntity* UTIL_RandomEntityByClassname(const char* szClassname) // orig 0xf31a0
{
	int iCount = UTIL_CountEntitiesByClass(szClassname);
	if (iCount < 1)
		return NULL;

	int iPick = RANDOM_LONG(0, iCount);
	CBaseEntity* pEntity = UTIL_FindEntityByClassname(NULL, szClassname);
	// past the end the search starts over from NULL; up to 1000 tries to land on one
	for (int i = 0; i < iPick || (!pEntity && i < 1000); i++)
		pEntity = UTIL_FindEntityByClassname(pEntity, szClassname);

	return pEntity;
}

// a ray (vecDir need not be unit length) against a box: true when it hits; *pflFraction is how
// far along vecDir it enters (0 when vecStart is inside the box)
bool UTIL_RayIntersectsBox(Vector vecStart, Vector vecDir, Vector vecMins, Vector vecMaxs, float* pflFraction) // orig 0xf3200
{
	if (!(vecMaxs.x <= vecStart.x || vecStart.x < vecMins.x || vecMaxs.y <= vecStart.y || vecStart.y < vecMins.y ||
			vecMaxs.z <= vecStart.z || vecStart.z < vecMins.z))
	{
		if (pflFraction)
			*pflFraction = 0;
		return true;
	}

	if (vecDir == g_vecZero)
		return false;

	float flFar = 9999;
	float flNear = -9999;

	for (int i = 0; i < 3; i++)
	{
		if (vecDir[i] == 0)
		{
			if (vecMaxs[i] < vecStart[i] || vecStart[i] < vecMins[i])
				return false;
		}
		else
		{
			float t1 = (vecMins[i] - vecStart[i]) / vecDir[i];
			float t2 = (vecMaxs[i] - vecStart[i]) / vecDir[i];
			if (t2 < t1)
				std::swap(t1, t2);
			if (flNear < t1)
				flNear = t1;
			if (t2 < flFar)
				flFar = t2;
			if (flFar < flNear || flFar < 0)
				return false;
		}
	}

	if (pflFraction)
		*pflFraction = flNear;
	return true;
}

// the next valid entity after pStart (by index) that comes within flRadius of vecCenter: a BSP
// model by its bounds, anything else as an upright cylinder 2 units thinner than its box.
// *pflDist gets the distance to the nearest point, *pvecNearest the point. Name p7's.
CBaseEntity* UTIL_FindEntityNearestInSphere(CBaseEntity* pStart, Vector vecCenter, float flRadius, float* pflDist, Vector* pvecNearest) // orig 0xf33b0
{
	int i = 1;
	if (pStart)
		i = ENTINDEX(pStart->edict()) + 1;

	for (; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent))
			continue;
		CBaseEntity* pEntity = CBaseEntity::Instance(pent);
		if (!pEntity)
			continue;

		entvars_t* pev = pEntity->pev;
		Vector vecNearest;
		float flDistSqr;

		if (!pEntity->IsBSPModel())
		{
			float flX = V_max(fabs(pev->mins.x), fabs(pev->maxs.x)) - 2;
			float flY = V_max(fabs(pev->mins.y), fabs(pev->maxs.y)) - 2;
			if (flX <= 0)
				flX = 0;
			if (flY <= 0)
				flY = 0;
			const float flCylRadius = sqrt(flX * flX + flY * flY);

			if (pev->absmin.z <= vecCenter.z)
			{
				vecNearest.z = vecCenter.z;
				if (pev->absmax.z < vecCenter.z)
					vecNearest.z = pev->absmax.z;
			}
			else
				vecNearest.z = pev->absmin.z;

			if (flRadius + 64 < fabs(vecNearest.z - vecCenter.z) - 32)
				continue;

			Vector vecFlat(vecCenter.x - pev->origin.x, vecCenter.y - pev->origin.y, 0);
			float flLen = sqrt(vecFlat.x * vecFlat.x + vecFlat.y * vecFlat.y);
			if (flLen == 0)
				flLen = 1;
			if (flCylRadius < flLen)
				vecFlat = (vecFlat / flLen) * flCylRadius;

			vecNearest.x = pev->origin.x + vecFlat.x;
			vecNearest.y = pev->origin.y + vecFlat.y;
			vecNearest.z = vecNearest.z + vecFlat.z;
		}
		else
		{
			for (int j = 0; j < 3; j++)
			{
				if (vecCenter[j] <= pev->absmin[j] || pev->absmax[j] <= vecCenter[j])
					vecNearest[j] = (pev->absmin[j] <= vecCenter[j]) ? pev->absmax[j] : pev->absmin[j];
				else
					vecNearest[j] = vecCenter[j];
			}
		}

		const Vector vecDelta = vecNearest - vecCenter;
		flDistSqr = DotProduct(vecDelta, vecDelta);

		if (sqrt(flDistSqr) <= flRadius + 64)
		{
			if (pvecNearest)
				*pvecNearest = vecNearest;
			if (pflDist)
				*pflDist = (vecNearest - vecCenter).Length();
			return pEntity;
		}
	}

	return NULL;
}

// The entity marks: a stack of bit sets, one bit per entity index, so that nested radius
// effects hit each entity once. 128 levels (orig .bss 0x101af084, the level at 0x101af2ac).
static unsigned char* g_rgpRC2EntityMarks[128];
static int g_iRC2EntityMarkLevel = 0;

void UTIL_ClearEntityMarks() // orig 0xf4c50
{
	memset(g_rgpRC2EntityMarks, 0, sizeof(g_rgpRC2EntityMarks)); // Original bug kept: the sets aren't freed
	g_iRC2EntityMarkLevel = 0;
}

// a new, empty set on top (the top one is reused while it isn't in use)
void UTIL_PushEntityMarks() // orig 0xf4c70
{
	if (g_rgpRC2EntityMarks[g_iRC2EntityMarkLevel])
	{
		g_iRC2EntityMarkLevel++;
		if (g_iRC2EntityMarkLevel > 127)
			g_iRC2EntityMarkLevel = 127;
	}

	// VERIFY: the size goes through a C runtime function at 0x1008b3, read here as ceil; and at
	// the top level the original leaks the set it overwrites
	const int iBytes = (int)ceil(gpGlobals->maxEntities * 0.125f);
	g_rgpRC2EntityMarks[g_iRC2EntityMarkLevel] = (unsigned char*)malloc(iBytes);
	for (int i = 0; i < (int)ceil(gpGlobals->maxEntities * 0.125f); i++)
		g_rgpRC2EntityMarks[g_iRC2EntityMarkLevel][i] = 0;
}

void UTIL_PopEntityMarks() // orig 0xf4d40
{
	if (!g_rgpRC2EntityMarks[g_iRC2EntityMarkLevel])
	{
		if (g_iRC2EntityMarkLevel > 0)
			g_iRC2EntityMarkLevel--;
		return;
	}

	free(g_rgpRC2EntityMarks[g_iRC2EntityMarkLevel]);
	g_rgpRC2EntityMarks[g_iRC2EntityMarkLevel] = NULL;
	if (g_iRC2EntityMarkLevel > 0)
		g_iRC2EntityMarkLevel--;
}

void UTIL_MarkEntity(int iEntIndex) // orig 0xf4d90. Name p7's
{
	if (g_rgpRC2EntityMarks[g_iRC2EntityMarkLevel])
		g_rgpRC2EntityMarks[g_iRC2EntityMarkLevel][iEntIndex / 8] |= 1 << (iEntIndex % 8);
}

bool UTIL_IsEntityMarked(int iEntIndex) // orig 0xf4dd0
{
	if (!g_rgpRC2EntityMarks[g_iRC2EntityMarkLevel])
		return false;
	return (g_rgpRC2EntityMarks[g_iRC2EntityMarkLevel][iEntIndex / 8] & (1 << (iEntIndex % 8))) != 0;
}

// Rocket Crowbar 2's radius damage: every entity within flRadius of vecSrc that takes damage and
// can be seen from it takes flDamage less (dist / flRadius) ^ flExponent of it, through its
// shield. With fTrace the damage is dealt as a TraceAttack where the trace ended short. Each
// entity (by GetEffectedEnt) is hit once. pevAttacker defaults to the world, pevInflictor to the
// attacker. Name p3's and p6's.
void RC2RadiusDamage(entvars_t* pevAttacker, entvars_t* pevInflictor, edict_t* pentIgnore, Vector vecSrc, float flRadius,
	float flDamage, float flExponent, int bitsDamageType, int fTrace) // orig 0xf3900
{
	if (!pevAttacker || !UTIL_IsValidEntity(pevAttacker->pContainingEntity))
		pevAttacker = VARS(INDEXENT(0));
	if (!pevInflictor || !UTIL_IsValidEntity(pevInflictor->pContainingEntity))
		pevInflictor = pevAttacker;

	UTIL_PushEntityMarks();

	CBaseEntity* pEntity = NULL;
	float flDist;
	Vector vecNearest;
	TraceResult tr;

	while ((pEntity = UTIL_FindEntityNearestInSphere(pEntity, vecSrc, flRadius, &flDist, &vecNearest)) != NULL)
	{
		if (pEntity->pev->takedamage == DAMAGE_NO)
			continue;
		if (UTIL_IsEntityMarked(ENTINDEX(pEntity->GetEffectedEnt()->edict())))
			continue;
		if (!(flDist < flRadius))
			continue;

		Vector vecDir = (vecNearest - vecSrc).Normalize();

		if (0 == fTrace)
		{
			UTIL_TraceLine(vecSrc, vecNearest, ignore_monsters, pentIgnore, &tr);
			if (tr.flFraction < 1.0 && tr.pHit != pEntity->edict())
				continue;
		}
		else
		{
			if (!pEntity->IsBSPModel())
				vecNearest = vecNearest + vecDir * 32;

			UTIL_TraceLine(vecSrc, vecNearest, dont_ignore_monsters, pentIgnore, &tr);
			if (tr.flFraction < 1.0 && tr.pHit != pEntity->edict())
			{
				UTIL_TraceLine(vecSrc, vecNearest, ignore_monsters, pentIgnore, &tr);
				if (tr.flFraction < 1.0 && tr.pHit != pEntity->edict())
					continue;
			}
		}

		float flAdjusted = flDamage - pow(flDist / flRadius, flExponent) * flDamage;
		if (flAdjusted < 0.0)
			flAdjusted = 0.0;
		else if (flAdjusted > flDamage)
			flAdjusted = flDamage;

		const float flShieldScale = (bitsDamageType & DMG_BLAST) != 0 ? RC2Pref(RC2P_Shield_BlastDamageFraction).f : RC2Pref(RC2P_Shield_GenericRadiusFraction).f;
		flAdjusted = ShieldDeflect(pEntity, flAdjusted, flShieldScale, tr.vecEndPos, vecDir, NULL, NULL, 8);
		if (!(flAdjusted > 0.0))
			continue;

		if (0 != fTrace && tr.flFraction < 1.0)
		{
			ClearMultiDamage();
			pEntity->GetEffectedEnt()->TraceAttack(pevAttacker, flAdjusted, vecDir, &tr, bitsDamageType);
			ApplyMultiDamage(pevInflictor, pevAttacker);
		}
		else
		{
			pEntity->GetEffectedEnt()->TakeDamage(pevInflictor, pevAttacker, flAdjusted, bitsDamageType);
		}

		UTIL_MarkEntity(ENTINDEX(pEntity->GetEffectedEnt()->edict()));
	}

	UTIL_PopEntityMarks();
}

// whether something of vecSize can get from vecStart to vecEnd: straight, or by one of the
// points to the sides of where the straight line is blocked (and, with fTryVertical, above and
// below it), moving those points out up to 8 times. *pvecVia gets the point used.
bool UTIL_RouteExists(Vector vecStart, Vector vecEnd, bool fTryVertical, Vector vecSize, int iHull, edict_t* pentIgnore, edict_t* pentTarget, Vector* pvecVia) // orig 0xf3de0
{
	float flWidth = V_max(vecSize.x, vecSize.y);
	if (flWidth <= 16)
		flWidth = 16;
	const float flHeight = (vecSize.z <= 16) ? 16 : vecSize.z;

	TraceResult tr;
	auto trace = [&](const Vector& a, const Vector& b)
	{
		if (iHull < 1)
			UTIL_TraceLine(a, b, ignore_monsters, pentIgnore, &tr);
		else
			UTIL_TraceHull(a, b, ignore_monsters, iHull, pentIgnore, &tr);
		return tr.flFraction >= 1.0 || (pentTarget && tr.pHit && tr.pHit == pentTarget);
	};

	const Vector vecDir = (vecEnd - vecStart).Normalize();
	const Vector vecRight(vecDir.y, -vecDir.x, 0); // CrossProduct(vecDir, up), not normalized
	const Vector vecUp(0, 0, 1);

	if (trace(vecStart, vecEnd))
		return true;

	const float flHitDist = (vecEnd - vecStart).Length() * tr.flFraction;
	const Vector vecAhead = vecStart + vecDir * (flHitDist + flWidth);
	const Vector vecHit = vecStart + vecDir * flHitDist;

	Vector vecSideA = vecAhead + vecRight * (flWidth * 3);
	Vector vecSideB = vecAhead - vecRight * (flWidth * 3);
	Vector vecUpper = vecHit + vecUp * flHeight * 3;
	Vector vecLower = vecHit - vecUp * flHeight * 3;
	const Vector vecSideStep = vecRight * flWidth * 2;
	const Vector vecUpStep = vecUp * flHeight * 2;

	auto via = [&](const Vector& vecPoint)
	{
		if (trace(vecStart, vecPoint) && trace(vecPoint, vecEnd))
		{
			if (pvecVia)
				*pvecVia = vecPoint;
			return true;
		}
		return false;
	};

	for (int i = 0; i < 8; i++)
	{
		if (via(vecSideA) || via(vecSideB))
			return true;
		if (fTryVertical && (via(vecUpper) || via(vecLower)))
			return true;

		vecSideA = vecSideA + vecSideStep;
		vecSideB = vecSideB - vecSideStep;
		vecUpper = vecUpper + vecUpStep;
		vecLower = vecLower - vecUpStep;
	}

	return false;
}

// an electric discharge into the water at vecSrc: every entity in the water that takes damage
// and that a 16-unit route reaches from vecSrc takes flDamage (DMG_SHOCK), when its shield lets
// any of it through. Each entity once. Name p6's.
void UTIL_RC2WaterDischarge(Vector vecSrc, float flDamage, entvars_t* pevInflictor, entvars_t* pevAttacker) // orig 0xf4e70
{
	UTIL_PushEntityMarks();

	for (int i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent))
			continue;
		CBaseEntity* pEntity = CBaseEntity::Instance(pent);
		if (!pEntity)
			continue;

		CBaseEntity* pEffected = pEntity->GetEffectedEnt();
		if (UTIL_IsEntityMarked(ENTINDEX(pEffected->edict())))
			continue;
		if (pEntity->GetEffectedEnt()->pev->takedamage == DAMAGE_NO)
			continue;
		if (pEntity->GetEffectedEnt()->pev->waterlevel < 1)
			continue;

		Vector vecTarget = pEntity->GetEffectedEnt()->pev->origin;
		vecTarget.z = vecTarget.z + pEntity->GetEffectedEnt()->pev->mins.z + 12;

		if (!UTIL_RouteExists(vecSrc, vecTarget, false, Vector(16, 16, 16), 0, pevInflictor->pContainingEntity, pEntity->edict(), NULL))
			continue;

		// Original bug kept: what the shield lets through is only tested, the full flDamage is dealt
		if (ShieldDeflect(pEntity, flDamage, RC2Pref(RC2P_Shield_GenericDischargeFraction).f, g_vecZero, g_vecZero, NULL, NULL, 5) > 0)
			pEntity->GetEffectedEnt()->TakeDamage(pevInflictor, pevAttacker, flDamage, DMG_SHOCK);

		UTIL_MarkEntity(ENTINDEX(pEntity->GetEffectedEnt()->edict()));
	}

	UTIL_PopEntityMarks();
}

// PM_Accelerate's step: vecVelocity sped up along vecWishDir toward flWishSpeed. Name ours (p's).
Vector UTIL_Accelerate(const Vector& vecVelocity, const Vector& vecWishDir, float flWishSpeed, float flAccel, float flFrameTime) // orig 0xf50a0
{
	const float flAdd = flWishSpeed - DotProduct(vecVelocity, vecWishDir);
	if (flAdd <= 0)
		return vecVelocity;

	float flAccelSpeed = flWishSpeed * flAccel * flFrameTime;
	if (flAdd < flAccelSpeed)
		flAccelSpeed = flAdd;

	return vecVelocity + vecWishDir * flAccelSpeed;
}

// true when a blast from vecSrc reaches vecEnd (or pentTarget on the way): traces on through
// entities that aren't BSP models, and punches through solid world in 8-unit steps while
// *pflPunch lasts (what is left is written back). iMaxSteps limits the recursion. Name p7's.
bool UTIL_BlastReaches(Vector vecSrc, Vector vecEnd, Vector vecDir, float* pflPunch, edict_t* pentIgnore, edict_t* pentTarget, TraceResult* ptr, int iMaxSteps) // orig 0xf5160
{
	if (iMaxSteps < 0)
		return false;
	iMaxSteps--;

	UTIL_TraceLine(vecSrc, vecEnd, ignore_monsters, pentIgnore, ptr);
	if (ptr->flFraction >= 1.0)
		return true;
	if (pentTarget && ptr->pHit == pentTarget)
		return true;

	if (ptr->pHit && CBaseEntity::Instance(ptr->pHit) && !CBaseEntity::Instance(ptr->pHit)->IsBSPModel())
		return UTIL_BlastReaches(ptr->vecEndPos, vecEnd, vecDir, pflPunch, ptr->pHit, pentTarget, ptr, iMaxSteps);

	Vector vecPos = ptr->vecEndPos;
	if (*pflPunch <= 0)
		return false;

	while (true)
	{
		const float flStep = (*pflPunch < 8) ? *pflPunch : 8;
		*pflPunch -= flStep;
		vecPos = vecPos + vecDir * flStep;

		if (UTIL_PointContents(vecPos) != CONTENTS_SOLID)
			return UTIL_BlastReaches(vecPos, vecEnd, vecDir, pflPunch, ptr->pHit, pentTarget, ptr, iMaxSteps);

		if (*pflPunch <= 0)
			return false;
	}
}

// a file in the game directory
FILE* UTIL_OpenGameDirFile(const char* szName, const char* szMode) // orig 0xf54a0
{
	char szPath[512];

	GET_GAME_DIR(szPath);
	strcat(szPath, "//");
	strcat(szPath, szName);
	return fopen(szPath, szMode);
}

// kills the beams attached to an entity, and tells RC2's client effects (gmsgCLTE) to drop
// theirs. Name p3's and p6's.
void UTIL_KillEntityEffects(edict_t* pEdict) // orig 0xf55b0
{
	if (!UTIL_IsValidEntity(pEdict))
		return;

	MESSAGE_BEGIN(MSG_ALL, SVC_TEMPENTITY);
	WRITE_BYTE(TE_KILLBEAM);
	WRITE_SHORT(ENTINDEX(pEdict));
	MESSAGE_END();

	if (0 != gmsgCLTE)
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(255);
		WRITE_SHORT(ENTINDEX(pEdict));
		MESSAGE_END();
	}
}

// vecDir spread at random by up to flSpread along vecRight and vecUp (as the shotgun's spread,
// inside a circle), normalized
Vector UTIL_RandomSpreadDir(Vector vecDir, Vector vecRight, Vector vecUp, float flSpread) // orig 0xf5630
{
	float x, y;
	do
	{
		x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
		y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
	} while (x * x + y * y > 1);

	return (vecDir + vecRight * (x * flSpread) + vecUp * (y * flSpread)).Normalize();
}

// vecDir mirrored about the plane with normal vecNormal
Vector UTIL_ReflectVector(Vector vecDir, Vector vecNormal) // orig 0xf5800
{
	const float flDot = -DotProduct(vecDir, vecNormal);
	return vecNormal * 2 * flDot + vecDir;
}

// The entity RC2 hands out as a named inflictor, so that death notices name the weapon or
// effect (CWorld::Precache makes it; orig .bss 0x101afd30, its name in 0x101aef84).
entvars_t* g_pevRC2DamageInflictor = NULL;
static char g_szRC2DamageInflictorName[256];

// that entity, renamed szClassname; the world when there is none. Name p6's.
entvars_t* UTIL_RC2DamageInflictor(const char* szClassname) // orig 0xf58a0
{
	if (g_pevRC2DamageInflictor)
	{
		strncpy(g_szRC2DamageInflictorName, szClassname, 255);
		g_szRC2DamageInflictorName[255] = 0;
		g_pevRC2DamageInflictor->classname = MAKE_STRING(g_szRC2DamageInflictorName);
		return g_pevRC2DamageInflictor;
	}

	return VARS(INDEXENT(0));
}

// a direction packed into an int for an event's iparam: 9 bits of each normalized component's
// size, 0x200 its sign, x in bits 0-9, y 10-19, z 20-29. Name p3's.
int UTIL_PackNormal(Vector vecDir) // orig 0xf5920
{
	const Vector n = vecDir.Normalize();

	int x = (int)(fabs(n.x) * 511.0);
	if (n.x < 0)
		x |= 0x200;
	int y = (int)(fabs(n.y) * 511.0);
	if (n.y < 0)
		y |= 0x200;
	int z = (int)(fabs(n.z) * 511.0);
	if (n.z < 0)
		z |= 0x200;

	return z << 20 | x | y << 10;
}

// RC 1.9's precache list (orig .bss 0x1019eb84, the count at 0x101af2b4): the classnames every
// factory and UTIL_PrecacheOther have seen. RC 1.9's names (cbase.cpp there).
static char g_pPrecachedStuff[512][128];
static int g_iCntPrecached = 0;

void AddToPrecachedList(const char* pszClassname) // orig 0xf5a20
{
	if (g_iCntPrecached >= 512)
		return;

	// Original bug kept: the duplicate check compares nothing (it tests the name for NULL once
	// per entry), so every call adds the name again until the list is full
	for (int i = 0; i < g_iCntPrecached; i++)
	{
		if (!pszClassname)
			return;
	}

	strcpy(g_pPrecachedStuff[g_iCntPrecached++], pszClassname);
}

bool IsClassnamePrecachedNoInvalid(const char* pszClassname) // orig 0xf5a80
{
	for (int i = 0; i < g_iCntPrecached; i++)
	{
		if (0 == strcmp(g_pPrecachedStuff[i], pszClassname))
			return true;
	}
	return false;
}

// classnames that only a map places (or that only make sense placed by one)
bool IsClassnameMapOnly(const char* pszClassname) // orig 0xf5af0. Name p's
{
	if (0 == strcmp(pszClassname, "monster_generic"))
		return true;
	if (0 == strcmp(pszClassname, "monster_maker"))
		return true;
	if (0 == strncmp(pszClassname, "env_", 4))
		return true;
	if (0 == strncmp(pszClassname, "trigger_", 8))
		return true;
	if (0 == strncmp(pszClassname, "func_", 5))
		return true;
	return 0 == strncmp(pszClassname, "info_", 5);
}

// the precache list's own copy of a classname (a string that stays), or pszClassname itself
const char* GetPrecachedName(const char* pszClassname) // orig 0xf5bf0
{
	for (int i = 0; i < g_iCntPrecached; i++)
	{
		if (0 == strcmp(g_pPrecachedStuff[i], pszClassname))
			return g_pPrecachedStuff[i];
	}
	return pszClassname;
}

void ResetPrecachedList() // orig 0xf5c60. RC 1.9's name
{
	g_iCntPrecached = 0;
}

//=========================================================
// CUTIL_TimedEvent ("timedevent"): calls a function once, after a delay, then removes itself.
//=========================================================
class CUTIL_TimedEvent : public CBaseEntity
{
public:
	void Spawn() override;
	void EXPORT TimeThink();

	void (*m_pfnEvent)(int); // name ours. orig +0x70
	int m_iEventArg;		 // name ours. orig +0x74
};

LINK_ENTITY_TO_CLASS(timedevent, CUTIL_TimedEvent); // orig 0xf5c70

void CUTIL_TimedEvent::Spawn() // orig 0xf5ce0
{
	pev->effects |= EF_NODRAW;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CUTIL_TimedEvent::TimeThink);
	pev->nextthink = gpGlobals->time + 1.0;
}

void CUTIL_TimedEvent::TimeThink() // orig 0xf5d50
{
	if (m_pfnEvent)
		m_pfnEvent(m_iEventArg);
	pev->flags |= FL_KILLME;
}

// pfnEvent(iArg) flDelay seconds from now. Name p3's.
void UTIL_RC2TimedEvent(void (*pfnEvent)(int), int iArg, float flDelay) // orig 0xf5d80
{
	CUTIL_TimedEvent* pEvent = (CUTIL_TimedEvent*)CBaseEntity::Create("timedevent", g_vecZero, g_vecZero, NULL);
	// Original bug kept: no NULL check
	pEvent->pev->nextthink = flDelay + gpGlobals->time;
	pEvent->m_pfnEvent = pfnEvent;
	pEvent->m_iEventArg = iArg;
}
