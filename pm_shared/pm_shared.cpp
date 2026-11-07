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

#include "Platform.h"

#include <assert.h>
#include "mathlib.h"
#include "cdll_dll.h"
#include "const.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "pm_shared.h"
#include "pm_materials.h"
#include "pm_movevars.h"
#include "pm_debug.h"
#include "rc2prefs.h"
#include "pm_rc2.h"
#include <stdio.h>	// NULL
#include <string.h> // strcpy
#include <stdlib.h> // atoi
#include <ctype.h>	// isspace

#ifdef CLIENT_DLL
// Spectator Mode
bool iJumpSpectator;
float vJumpOrigin[3];
float vJumpAngles[3];
#endif

static bool pm_shared_initialized = false;

#pragma warning(disable : 4305)

typedef enum
{
	mod_brush,
	mod_sprite,
	mod_alias,
	mod_studio
} modtype_t;

playermove_t* pmove = NULL;

typedef struct
{
	int planenum;
	short children[2]; // negative numbers are contents
} dclipnode_t;

typedef struct mplane_s
{
	Vector normal; // surface normal
	float dist;	   // closest appoach to origin
	byte type;	   // for texture axis selection and fast side tests
	byte signbits; // signx + signy<<1 + signz<<1
	byte pad[2];
} mplane_t;

typedef struct hull_s
{
	dclipnode_t* clipnodes;
	mplane_t* planes;
	int firstclipnode;
	int lastclipnode;
	Vector clip_mins;
	Vector clip_maxs;
} hull_t;

// Ducking time
#define TIME_TO_DUCK 0.4
#define MAX_CLIMB_SPEED 200
#define STUCK_MOVEUP 1
#define STUCK_MOVEDOWN -1
#define STOP_EPSILON 0.1

#define STEP_CONCRETE 0 // default step sound
#define STEP_METAL 1	// metal floor
#define STEP_DIRT 2		// dirt, sand, rock
#define STEP_VENT 3		// ventillation duct
#define STEP_GRATE 4	// metal grating
#define STEP_TILE 5		// floor tiles
#define STEP_SLOSH 6	// shallow liquid puddle
#define STEP_WADE 7		// wading in liquid
#define STEP_LADDER 8	// climbing ladder

#define PLAYER_FATAL_FALL_SPEED 1024															  // approx 60 feet
#define PLAYER_MAX_SAFE_FALL_SPEED 580															  // approx 20 feet
#define DAMAGE_FOR_FALL_SPEED (float)100 / (PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED) // damage per unit per second.
#define PLAYER_MIN_BOUNCE_SPEED 200
#define PLAYER_FALL_PUNCH_THRESHHOLD (float)350 // won't punch player's screen/make scrape noise unless player falling at least this fast.

#define PLAYER_LONGJUMP_SPEED 350 // how fast we longjump

#define PLAYER_DUCKING_MULTIPLIER 0.333

// double to float warning
#pragma warning(disable : 4244)
// up / down
#define PITCH 0
// left / right
#define YAW 1
// fall over
#define ROLL 2

#define CONTENTS_CURRENT_0 -9
#define CONTENTS_CURRENT_90 -10
#define CONTENTS_CURRENT_180 -11
#define CONTENTS_CURRENT_270 -12
#define CONTENTS_CURRENT_UP -13
#define CONTENTS_CURRENT_DOWN -14

#define CONTENTS_TRANSLUCENT -15

static Vector rgv3tStuckTable[54];
static int rgStuckLast[MAX_PLAYERS][2];

// Texture names
static int gcTextures = 0;
static char grgszTextureName[CTEXTURESMAX][CBTEXTURENAMEMAX];
static char grgchTextureType[CTEXTURESMAX];

bool g_onladder = false;

// Rocket Crowbar 2: what PM_Move reads at the start of every move (pm_rc2.h). Names ours.
int g_iPMPlayerEnt = 1;			 // .data 0x1014ddc4: physinfo "eid"
static int g_bPMShrunk;			 // .bss 0x1019159c: physinfo "shr", speeds/step/jump x0.2 and the point hull
static int g_iPMFlags;			 // .bss 0x10191594: physinfo "phf", PMF_*
static int g_iPMMode;			 // .bss 0x10191598: PM_GetPlayerMode, PM_MODE_*
static int g_iPMRideEnt;		 // .bss 0x101915a0: the player this one rides (PM_GetRideInfo & 0x7f)
static int g_bPMRideLeft;		 // .bss 0x101915a4: on the left shoulder (bit 7)
static float g_flPMStopSpeed;	 // .data 0x1014ddc8: friction's stop speed, set per move in PM_Move

#ifdef CLIENT_DLL
// Rocket Crowbar 2, the client's spectator and race views (client.dll; names ours).
Vector g_vecRaceViewAngles;		// client .bss 0x1022fe30: PM_RaceMove's view angles, read by view.cpp (client 0xe9ee0)
int g_iSpecChaseFlags;			// client .bss 0x1022fe3c: in-eye chase side (0x10) and height (0x20); VERIFY: nothing in client.dll writes it
static int s_iAssassinCharge;	// client .bss 0x1020f6bc: PM_AssassinJump's charge, kept from the move that ran the functions
static int s_iHeadcrabCharge;	// client .bss 0x1020f6c0: the same for PM_HeadcrabJump
static Vector s_vecInEyeAngles; // client .bss 0x1020da78: the smoothed in-eye chase angles

// view.cpp's (client 0xe9400-0xebb00, not this package)
extern Vector v_origin, v_angles; // client 0x10227084, 0x10226848. VERIFY: taken for view.cpp's v_origin and v_angles
extern int g_iPMSpecView;	   // client 0x10227160: 1 spectating this move; |2 angles and |4 origin follow a target. Name ours
extern int g_bPMSetViewAngles; // client 0x102270f0: pm_shared set v_angles. Name ours
extern int g_bPMSetViewOrigin; // client 0x10227164: pm_shared set v_origin. Name ours
// the client's predicted origin of entity iEnt (client 0x59d50, not this package). Name ours
void PM_SetPredictedOrigin(int iEnt, const float* vecOrigin);
#endif

void PM_AddGravity();


static void PM_InitTrace(trace_t* trace, const Vector& end)
{
	memset(trace, 0, sizeof(*trace));
	VectorCopy(end, trace->endpos);
	trace->allsolid = 1;
	trace->fraction = 1.0f;
}

static void PM_TraceModel(physent_t* pEnt, const Vector& start, const Vector& end, trace_t* trace)
{
	PM_InitTrace(trace, end);
	pmove->PM_TraceModel(pEnt, start, end, trace);
}

void PM_SwapTextures(int i, int j)
{
	char chTemp;
	char szTemp[CBTEXTURENAMEMAX];

	strcpy(szTemp, grgszTextureName[i]);
	chTemp = grgchTextureType[i];

	strcpy(grgszTextureName[i], grgszTextureName[j]);
	grgchTextureType[i] = grgchTextureType[j];

	strcpy(grgszTextureName[j], szTemp);
	grgchTextureType[j] = chTemp;
}

void PM_SortTextures()
{
	// Bubble sort, yuck, but this only occurs at startup and it's only 512 elements...
	//
	int i, j;

	for (i = 0; i < gcTextures; i++)
	{
		for (j = i + 1; j < gcTextures; j++)
		{
			if (stricmp(grgszTextureName[i], grgszTextureName[j]) > 0)
			{
				// Swap
				//
				PM_SwapTextures(i, j);
			}
		}
	}
}

void PM_InitTextureTypes()
{
	char buffer[512];
	int i, j;
	byte* pMemFile;
	int fileSize, filePos;
	static bool bTextureTypeInit = false;

	if (bTextureTypeInit)
		return;

	memset(&(grgszTextureName[0][0]), 0, CTEXTURESMAX * CBTEXTURENAMEMAX);
	memset(grgchTextureType, 0, CTEXTURESMAX);

	gcTextures = 0;
	memset(buffer, 0, 512);

	fileSize = pmove->COM_FileSize("sound/materials.txt");
	pMemFile = pmove->COM_LoadFile("sound/materials.txt", 5, NULL);
	if (!pMemFile)
		return;

	filePos = 0;
	// for each line in the file...
	while (pmove->memfgets(pMemFile, fileSize, &filePos, buffer, 511) != NULL && (gcTextures < CTEXTURESMAX))
	{
		// skip whitespace
		i = 0;
		while ('\0' != buffer[i] && 0 != isspace(buffer[i]))
			i++;

		if ('\0' == buffer[i])
			continue;

		// skip comment lines
		if (buffer[i] == '/' || 0 == isalpha(buffer[i]))
			continue;

		// get texture type
		grgchTextureType[gcTextures] = toupper(buffer[i++]);

		// skip whitespace
		while ('\0' != buffer[i] && 0 != isspace(buffer[i]))
			i++;

		if ('\0' == buffer[i])
			continue;

		// get sentence name
		j = i;
		while ('\0' != buffer[j] && 0 == isspace(buffer[j]))
			j++;

		if ('\0' == buffer[j])
			continue;

		// null-terminate name and save in sentences array
		j = V_min(j, CBTEXTURENAMEMAX - 1 + i);
		buffer[j] = 0;
		strcpy(&(grgszTextureName[gcTextures++][0]), &(buffer[i]));
	}

	// Must use engine to free since we are in a .dll
	pmove->COM_FreeFile(pMemFile);

	PM_SortTextures();

	bTextureTypeInit = true;
}

char PM_FindTextureType(const char* name)
{
	int left, right, pivot;
	int val;

	assert(pm_shared_initialized);

	left = 0;
	right = gcTextures - 1;

	while (left <= right)
	{
		pivot = (left + right) / 2;

		val = strnicmp(name, grgszTextureName[pivot], CBTEXTURENAMEMAX - 1);
		if (val == 0)
		{
			return grgchTextureType[pivot];
		}
		else if (val > 0)
		{
			left = pivot + 1;
		}
		else if (val < 0)
		{
			right = pivot - 1;
		}
	}

	return CHAR_TEX_CONCRETE;
}

// orig 0xa07a0, orig client 0xb1500. RC2: also plays in multiplayer when walking slowly in a movement mode
void PM_PlayStepSound(int step, float fvol)
{
	static int iSkipStep = 0;
	int irand;
	Vector hvel;

	pmove->iStepLeft = 0 != pmove->iStepLeft ? 0 : 1;

	if (0 == pmove->runfuncs)
	{
		return;
	}

	irand = pmove->RandomLong(0, 1) + (pmove->iStepLeft * 2);

	// FIXME mp_footsteps needs to be a movevar
	if (0 != pmove->multiplayer && 0 == pmove->movevars->footsteps)
		return;

	VectorCopy(pmove->velocity, hvel);
	hvel[2] = 0.0;

	if (0 != pmove->multiplayer && (!g_onladder && Length(hvel) <= 220) && g_iPMMode == PM_MODE_NORMAL)
		return;

	// irand - 0,1 for right foot, 2,3 for left foot
	// used to alternate left and right foot
	// FIXME, move to player state

	switch (step)
	{
	default:
	case STEP_CONCRETE:
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_step1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_step3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_step2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_step4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_METAL:
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_metal1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_metal3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_metal2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_metal4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_DIRT:
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_dirt1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_dirt3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_dirt2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_dirt4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_VENT:
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_duct1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_duct3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_duct2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_duct4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_GRATE:
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_grate1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_grate3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_grate2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_grate4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_TILE:
		if (0 == pmove->RandomLong(0, 4))
			irand = 4;
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_tile1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_tile3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_tile2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_tile4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 4:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_tile5.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_SLOSH:
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_slosh1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_slosh3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_slosh2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_slosh4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_WADE:
		if (iSkipStep == 0)
		{
			iSkipStep++;
			break;
		}

		if (iSkipStep++ == 3)
		{
			iSkipStep = 0;
		}

		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_LADDER:
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_ladder1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_ladder3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_ladder2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_ladder4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	}
}

int PM_MapTextureTypeStepType(char chTextureType)
{
	switch (chTextureType)
	{
	default:
	case CHAR_TEX_CONCRETE:
		return STEP_CONCRETE;
	case CHAR_TEX_METAL:
		return STEP_METAL;
	case CHAR_TEX_DIRT:
		return STEP_DIRT;
	case CHAR_TEX_VENT:
		return STEP_VENT;
	case CHAR_TEX_GRATE:
		return STEP_GRATE;
	case CHAR_TEX_TILE:
		return STEP_TILE;
	case CHAR_TEX_SLOSH:
		return STEP_SLOSH;
	}
}

/*
====================
PM_CatagorizeTextureTypeAt

Rocket Crowbar 2: PM_CatagorizeTextureType along any line (the wall a wallhop pushes off).
====================
*/
void PM_CatagorizeTextureTypeAt(float* start, float* end) // orig 0xa0fb0, orig client 0xb1f20, name ours
{
	const char* pTextureName;

	// Fill in default values, just in case.
	pmove->sztexturename[0] = '\0';
	pmove->chtexturetype = CHAR_TEX_CONCRETE;

	pTextureName = pmove->PM_TraceTexture(pmove->onground, start, end);
	if (!pTextureName)
		return;

	// strip leading '-0' or '+0~' or '{' or '!'
	if (*pTextureName == '-' || *pTextureName == '+')
		pTextureName += 2;

	if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
		pTextureName++;

	strcpy(pmove->sztexturename, pTextureName);
	pmove->sztexturename[CBTEXTURENAMEMAX - 1] = 0;

	// get texture type
	pmove->chtexturetype = PM_FindTextureType(pmove->sztexturename);
}

/*
====================
PM_CatagorizeTextureType

Determine texture info for the texture we are standing on.
====================
*/
void PM_CatagorizeTextureType()
{
	Vector start, end;
	const char* pTextureName;

	VectorCopy(pmove->origin, start);
	VectorCopy(pmove->origin, end);

	// Straight down
	end[2] -= 64;

	// Fill in default values, just in case.
	pmove->sztexturename[0] = '\0';
	pmove->chtexturetype = CHAR_TEX_CONCRETE;

	pTextureName = pmove->PM_TraceTexture(pmove->onground, start, end);
	if (!pTextureName)
		return;

	// strip leading '-0' or '+0~' or '{' or '!'
	if (*pTextureName == '-' || *pTextureName == '+')
		pTextureName += 2;

	if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
		pTextureName++;
	// '}}'

	strcpy(pmove->sztexturename, pTextureName);
	pmove->sztexturename[CBTEXTURENAMEMAX - 1] = 0;

	// get texture type
	pmove->chtexturetype = PM_FindTextureType(pmove->sztexturename);
}

// orig 0xa1180, orig client 0xb2170. RC2: no steps for the controller; quieter and slower steps per movement mode
void PM_UpdateStepSound()
{
	float fvol;
	Vector knee;
	Vector feet;
	Vector center;
	float height;
	float speed;
	float velrun;
	float velwalk;
	float flduck;
	int step;

	if (pmove->flTimeStepSound > 0)
		return;

	if ((pmove->flags & FL_FROZEN) != 0)
		return;

	if (g_iPMMode == PM_MODE_CONTROLLER)
		return;

	PM_CatagorizeTextureType();

	speed = Length(pmove->velocity);

	// determine if we are on a ladder
	const bool fLadder = (pmove->movetype == MOVETYPE_FLY); // IsOnLadder();

	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!
	if ((pmove->flags & FL_DUCKING) != 0 || fLadder)
	{
		velwalk = 60; // These constants should be based on cl_movespeedkey * cl_forwardspeed somehow
		velrun = 80;  // UNDONE: Move walking to server
		flduck = 100;
	}
	else
	{
		velwalk = 120;
		velrun = 210;
		flduck = 0;
	}

	// If we're on a ladder or on the ground, and we're moving fast enough,
	//  play step sound.  Also, if pmove->flTimeStepSound is zero, get the new
	//  sound right away - we just started moving in new level.
	if ((fLadder || (pmove->onground != -1)) &&
		(Length(pmove->velocity) > 0.0) &&
		(speed >= velwalk || 0 == pmove->flTimeStepSound))
	{
		const bool fWalking = speed < velrun;

		VectorCopy(pmove->origin, center);
		VectorCopy(pmove->origin, knee);
		VectorCopy(pmove->origin, feet);

		height = pmove->player_maxs[pmove->usehull][2] - pmove->player_mins[pmove->usehull][2];

		knee[2] = pmove->origin[2] - 0.3 * height;
		feet[2] = pmove->origin[2] - 0.5 * height;

		// find out what we're stepping in or on...
		if (fLadder)
		{
			step = STEP_LADDER;
			fvol = 0.35;
			pmove->flTimeStepSound = 350;
		}
		else if (pmove->PM_PointContents(knee, NULL) == CONTENTS_WATER)
		{
			step = STEP_WADE;
			fvol = 0.65;
			pmove->flTimeStepSound = 600;
		}
		else if (pmove->PM_PointContents(feet, NULL) == CONTENTS_WATER)
		{
			step = STEP_SLOSH;
			fvol = fWalking ? 0.2 : 0.5;
			pmove->flTimeStepSound = fWalking ? 400 : 300;
		}
		else
		{
			// find texture under player, if different from current texture,
			// get material type
			step = PM_MapTextureTypeStepType(pmove->chtexturetype);

			switch (pmove->chtexturetype)
			{
			default:
			case CHAR_TEX_CONCRETE:
				fvol = fWalking ? 0.2 : 0.5;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_METAL:
				fvol = fWalking ? 0.2 : 0.5;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_DIRT:
				fvol = fWalking ? 0.25 : 0.55;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_VENT:
				fvol = fWalking ? 0.4 : 0.7;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_GRATE:
				fvol = fWalking ? 0.2 : 0.5;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_TILE:
				fvol = fWalking ? 0.2 : 0.5;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_SLOSH:
				fvol = fWalking ? 0.2 : 0.5;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;
			}
		}

		// Rocket Crowbar 2: the transformed are quieter (the assassin only on the ground,
		// the headcrab more on the ground than in the air)
		switch (g_iPMMode)
		{
		case PM_MODE_ASSASSIN:
			if (pmove->onground != -1)
				fvol *= 0.5;
			break;
		case PM_MODE_HOUNDEYE:
			fvol *= 0.6;
			break;
		case PM_MODE_BAT:
			fvol *= 0.25;
			break;
		case PM_MODE_HEADCRAB:
			if (pmove->onground != -1)
				fvol *= 0.25;
			else
				fvol *= 0.5;
			break;
		}

		pmove->flTimeStepSound += flduck; // slower step time if ducking

		// play the sound
		// 35% volume if ducking
		if ((pmove->flags & FL_DUCKING) != 0)
		{
			fvol *= 0.35;
		}

		PM_PlayStepSound(step, fvol);

		// Rocket Crowbar 2: and step at their own pace off ladders
		if (!fLadder)
		{
			switch (g_iPMMode)
			{
			case PM_MODE_HOUNDEYE:
			case PM_MODE_BAT:
				pmove->flTimeStepSound = 500;
				break;
			case PM_MODE_ASSASSIN:
			case PM_MODE_HEADCRAB:
				pmove->flTimeStepSound = 300;
				break;
			}
		}
	}
}

/*
================
PM_AddToTouched

Add's the trace result to touch list, if contact is not already in list.
================
*/
bool PM_AddToTouched(pmtrace_t tr, Vector impactvelocity)
{
	int i;

	for (i = 0; i < pmove->numtouch; i++)
	{
		if (pmove->touchindex[i].ent == tr.ent)
			break;
	}
	if (i != pmove->numtouch) // Already in list.
		return false;

	VectorCopy(impactvelocity, tr.deltavelocity);

	if (pmove->numtouch >= MAX_PHYSENTS)
		pmove->Con_DPrintf("Too many entities were touched!\n");

	pmove->touchindex[pmove->numtouch++] = tr;
	return true;
}

/*
================
PM_CheckVelocity

See if the player has a bogus velocity value.
================
*/
void PM_CheckVelocity()
{
	int i;

	//
	// bound velocity
	//
	for (i = 0; i < 3; i++)
	{
		// See if it's bogus.
		if (IS_NAN(pmove->velocity[i]))
		{
			pmove->Con_Printf("PM  Got a NaN velocity %i\n", i);
			pmove->velocity[i] = 0;
		}
		if (IS_NAN(pmove->origin[i]))
		{
			pmove->Con_Printf("PM  Got a NaN origin on %i\n", i);
			pmove->origin[i] = 0;
		}

		// Bound it.
		if (pmove->velocity[i] > pmove->movevars->maxvelocity)
		{
			pmove->Con_DPrintf("PM  Got a velocity too high on %i\n", i);
			pmove->velocity[i] = pmove->movevars->maxvelocity;
		}
		else if (pmove->velocity[i] < -pmove->movevars->maxvelocity)
		{
			pmove->Con_DPrintf("PM  Got a velocity too low on %i\n", i);
			pmove->velocity[i] = -pmove->movevars->maxvelocity;
		}
	}
}

/*
==================
PM_ClipVelocity

Slide off of the impacting object
returns the blocked flags:
0x01 == floor
0x02 == step / wall
==================
*/
// orig 0xa1700. Not byte-identical to SDK 2.3's, but it reads the same
int PM_ClipVelocity(Vector in, Vector normal, Vector& out, float overbounce)
{
	float backoff;
	float change;
	float angle;
	int i, blocked;

	angle = normal[2];

	blocked = 0x00;		 // Assume unblocked.
	if (angle > 0)		 // If the plane that is blocking us has a positive z component, then assume it's a floor.
		blocked |= 0x01; //
	if (0 == angle)		 // If the plane has no Z, it is vertical (wall/step)
		blocked |= 0x02; //

	// Determine how far along plane to slide based on incoming direction.
	// Scale by overbounce factor.
	backoff = DotProduct(in, normal) * overbounce;

	for (i = 0; i < 3; i++)
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;
		// If out velocity is too small, zero it out.
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}

	// Return blocking flags.
	return blocked;
}

// orig 0xa17a0, orig client 0xb2930. RC2: no gravity while PMF_HELD
void PM_AddCorrectGravity()
{
	float ent_gravity;

	if (0 != pmove->waterjumptime || (g_iPMFlags & PMF_HELD) != 0)
		return;

	if (0 != pmove->gravity)
		ent_gravity = pmove->gravity;
	else
		ent_gravity = 1.0;

	// Add gravity so they'll be in the correct position during movement
	// yes, this 0.5 looks wrong, but it's not.
	pmove->velocity[2] -= (ent_gravity * pmove->movevars->gravity * 0.5 * pmove->frametime);
	pmove->velocity[2] += pmove->basevelocity[2] * pmove->frametime;
	pmove->basevelocity[2] = 0;

	PM_CheckVelocity();
}


// orig 0xa1830, orig client 0xb29f0. RC2: as PM_AddCorrectGravity
void PM_FixupGravityVelocity()
{
	float ent_gravity;

	if (0 != pmove->waterjumptime || (g_iPMFlags & PMF_HELD) != 0)
		return;

	if (0 != pmove->gravity)
		ent_gravity = pmove->gravity;
	else
		ent_gravity = 1.0;

	// Get the correct velocity for the end of the dt
	pmove->velocity[2] -= (ent_gravity * pmove->movevars->gravity * pmove->frametime * 0.5);

	PM_CheckVelocity();
}

/*
============
Rocket Crowbar 2: the leaping headcrab flies through what it hits. PM_FlyMove asks the game
(PM_HeadcrabLeapHit) about each entity it bumps; one it may pass is moved out of the way
(origin 8192, not solid) in physents, visents and moveents for the rest of the move, then put
back. Names ours; the arrays are the original's, in its .bss order (0x101b0120-0x101b5ae0),
because the moveent branch writes past its own arrays (see PM_TryPassThrough).
============
*/
#define PM_PASS_MAX_PHYS 600
#define PM_PASS_MAX_MOVE 64

static struct
{
	Vector physOrigin[PM_PASS_MAX_PHYS]; // 0x101b0120
	int visSolid[PM_PASS_MAX_PHYS];		 // 0x101b1d40
	Vector moveOrigin[PM_PASS_MAX_MOVE]; // 0x101b26a0
	int moveIndex[PM_PASS_MAX_MOVE];	 // 0x101b29a0
	int physSolid[PM_PASS_MAX_PHYS];	 // 0x101b2aa0
	int visIndex[PM_PASS_MAX_PHYS];		 // 0x101b3400
	int moveSolid[PM_PASS_MAX_MOVE];	 // 0x101b3d60
	int physIndex[PM_PASS_MAX_PHYS];	 // 0x101b3e60
	Vector visOrigin[PM_PASS_MAX_PHYS];	 // 0x101b47c0
} s_pmPass;							  // name ours

static int s_iPassPhys; // .bss 0x101915b8, name ours
static int s_iPassVis;	// .bss 0x101915bc, name ours
static int s_iPassMove; // .bss 0x101915c0, name ours

// write int slot iSlot of s_pmPass counted from field pField, as the original's flat .bss allows
static void PM_PassWrite(int* pField, int iSlot, int iValue) // name ours, a helper, no original
{
	int* pBase = reinterpret_cast<int*>(&s_pmPass);
	const int iOffset = static_cast<int>(pField - pBase) + iSlot;
	if (iOffset >= 0 && iOffset < static_cast<int>(sizeof(s_pmPass) / sizeof(int)))
		pBase[iOffset] = iValue;
}

void PM_ResetPassThrough() // orig 0xa18a0, orig client 0xb2a80, name ours
{
	s_iPassMove = 0;
	s_iPassVis = 0;
	s_iPassPhys = 0;
}

void PM_RestorePassThrough() // orig 0xa18c0, orig client 0xb2ab0, name ours
{
	int i;

	for (i = 0; i < s_iPassPhys; i++)
	{
		pmove->physents[s_pmPass.physIndex[i]].origin = s_pmPass.physOrigin[i];
		pmove->physents[s_pmPass.physIndex[i]].solid = s_pmPass.physSolid[i];
	}

	for (i = 0; i < s_iPassVis; i++)
	{
		pmove->visents[s_pmPass.visIndex[i]].origin = s_pmPass.visOrigin[i];
		pmove->visents[s_pmPass.visIndex[i]].solid = s_pmPass.visSolid[i];
	}

	for (i = 0; i < s_iPassMove; i++)
	{
		pmove->moveents[s_pmPass.moveIndex[i]].origin = s_pmPass.moveOrigin[i];
		pmove->moveents[s_pmPass.moveIndex[i]].solid = s_pmPass.moveSolid[i];
	}

	s_iPassMove = 0;
	s_iPassVis = 0;
	s_iPassPhys = 0;
}

// physent iPhysEnt was hit: if the game lets the leap through it, move it and its visent and
// moveent copies away and return true
bool PM_TryPassThrough(int iPhysEnt) // orig 0xa1b10, orig client 0xb2da0, name ours
{
	const Vector vecAway(8192, 8192, 8192);
	physent_t* pe = &pmove->physents[iPhysEnt];
	int i;

#ifdef CLIENT_DLL
	// the client's guess at the server's PM_HeadcrabLeapHit: anything that takes damage, when
	// the leap's damage would reach 140
	if (0 == pe->takedamage)
		return false;
	const float flDamage = (Length(pmove->velocity) - RC2Pref(RC2P_TransformHeadcrab_JumpBaseVel).f) / RC2Pref(RC2P_TransformHeadcrab_JumpMaxAddVel).f * (RC2Pref(RC2P_TransformHeadcrab_JumpMaxDamage).f - RC2Pref(RC2P_TransformHeadcrab_JumpMinDamage).f) + RC2Pref(RC2P_TransformHeadcrab_JumpMinDamage).f;
	if (flDamage < 140)
		return false;
#else
	if (0 == PM_HeadcrabLeapHit(g_iPMPlayerEnt, pe->info, Length(pmove->velocity)))
		return false;
#endif

	if (s_iPassPhys < PM_PASS_MAX_PHYS)
	{
		s_pmPass.physOrigin[s_iPassPhys] = pe->origin;
		s_pmPass.physSolid[s_iPassPhys] = pe->solid;
		s_pmPass.physIndex[s_iPassPhys] = iPhysEnt;
		s_iPassPhys++;
	}
	pe->solid = SOLID_NOT;
	pe->origin = vecAway;

	for (i = 0; i < pmove->numvisent; i++)
	{
		if (pmove->visents[i].info == pe->info && s_iPassVis < PM_PASS_MAX_PHYS)
		{
			s_pmPass.visOrigin[s_iPassVis] = pmove->visents[i].origin;
			s_pmPass.visIndex[s_iPassVis] = i;
			s_pmPass.visSolid[s_iPassVis] = pmove->visents[i].solid;
			s_iPassVis++;
			pmove->visents[i].solid = SOLID_NOT;
			pmove->visents[i].origin = vecAway;
		}
	}

	for (i = 0; i < pmove->nummoveent; i++)
	{
		if (pmove->moveents[i].info == pe->info && s_iPassMove < PM_PASS_MAX_MOVE)
		{
			// Original bug, kept: the origin goes into the visent array (at the moveent count)
			// and the solid and index into the moveent arrays at the visent count, which can
			// run past them; PM_RestorePassThrough then restores moveents from slots never
			// written. VERIFY in game; reference/dossiers/p6.md
			s_pmPass.visOrigin[s_iPassMove] = pmove->moveents[i].origin;
			s_iPassMove++;
			PM_PassWrite(s_pmPass.moveSolid, s_iPassVis, pmove->moveents[i].solid);
			PM_PassWrite(s_pmPass.moveIndex, s_iPassVis, i);
			pmove->moveents[i].solid = SOLID_NOT;
			pmove->moveents[i].origin = vecAway;
		}
	}

	return true;
}

/*
============
PM_FlyMove

The basic solid body movement clip that slides along multiple planes
============
*/
// orig 0xa1dc0, orig client 0xb3150. RC2: the leaping headcrab passes through what PM_HeadcrabLeapHit allows;
// PMF_BOUNCE reflects off every plane with some noise and speeds up (the AF grenade); and the
// reflection branch applies to every plane, as in SDK 2.3 (halflife-updated's first-plane-only
// fix dropped)
int PM_FlyMove()
{
	int bumpcount, numbumps;
	Vector dir;
	float d;
	int numplanes;
	Vector planes[MAX_CLIP_PLANES];
	Vector primal_velocity, original_velocity;
	Vector new_velocity;
	int i, j;
	pmtrace_t trace;
	Vector end;
	float time_left, allFraction;
	int blocked;

	numbumps = 4; // Bump up to four times

	blocked = 0;									// Assume not blocked
	numplanes = 0;									//  and not sliding along any planes
	VectorCopy(pmove->velocity, original_velocity); // Store original velocity
	VectorCopy(pmove->velocity, primal_velocity);

	allFraction = 0;
	time_left = pmove->frametime; // Total time for this movement operation.

	if (g_iPMMode == PM_MODE_HEADCRAB)
		PM_ResetPassThrough();

	for (bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		if (pmove->velocity == g_vecZero)
			break;

		// Assume we can move all the way from the current origin to the
		//  end point.
		for (i = 0; i < 3; i++)
			end[i] = pmove->origin[i] + time_left * pmove->velocity[i];

		// See if we can make it from origin to end point.
		trace = pmove->PM_PlayerTrace(pmove->origin, end, PM_NORMAL, -1);

		allFraction += trace.fraction;
		// If we started in a solid object, or we were in solid space
		//  the whole way, zero out our velocity and return that we
		//  are blocked by floor and wall.
		if (0 != trace.allsolid)
		{ // entity is trapped in another solid
			VectorCopy(vec3_origin, pmove->velocity);
			//Con_DPrintf("Trapped 4\n");
			// Original bug, kept: returns without PM_RestorePassThrough
			return 4;
		}

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove->origin and
		//  zero the plane counter.
		if (trace.fraction > 0)
		{ // actually covered some distance
			VectorCopy(trace.endpos, pmove->origin);
			VectorCopy(pmove->velocity, original_velocity);
			numplanes = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
		if (trace.fraction == 1)
			break; // moved the entire distance

		//if (!trace.ent)
		//	Sys_Error ("PM_PlayerTrace: !trace.ent");

		// Save entity that blocked us (since fraction was < 1.0)
		//  for contact
		// Add it if it's not already in the list!!!
		PM_AddToTouched(trace, pmove->velocity);

		// Rocket Crowbar 2: a leaping headcrab goes through it if it may
		if (g_iPMMode == PM_MODE_HEADCRAB && 0 != PM_GetHeadcrabLeap(g_iPMPlayerEnt))
		{
			if (PM_TryPassThrough(trace.ent))
			{
				time_left -= time_left * trace.fraction;
				continue;
			}
			PM_SetHeadcrabLeap(g_iPMPlayerEnt, 0);
		}

		// If the plane we hit has a high z component in the normal, then
		//  it's probably a floor
		if (trace.plane.normal[2] > 0.7)
		{
			blocked |= 1; // floor
		}
		// If the plane has a zero z component in the normal, then it's a
		//  step or wall
		if (0 == trace.plane.normal[2])
		{
			blocked |= 2; // step / wall
						  //Con_DPrintf("Blocked by %i\n", trace.ent);
		}

		// Reduce amount of pmove->frametime left by total time left * fraction
		//  that we covered.
		time_left -= time_left * trace.fraction;

		// Did we run out of planes to clip against?
		if (numplanes >= MAX_CLIP_PLANES)
		{ // this shouldn't really happen
			//  Stop our movement if so.
			VectorCopy(vec3_origin, pmove->velocity);
			//Con_DPrintf("Too many planes 4\n");

			break;
		}

		// Set up next clipping plane
		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;
		//

		if ((g_iPMFlags & PMF_BOUNCE) != 0)
		{
			// Rocket Crowbar 2: bounce off every plane, add some noise, speed up per plane
			Vector vecDir = original_velocity;
			float flSpeed = VectorNormalize(vecDir);

			for (i = 0; i < numplanes; i++)
			{
				d = -DotProduct(vecDir, planes[i]);
				VectorMA(vecDir, d + d, planes[i], vecDir);
			}

			const float flEntropy = RC2Pref(RC2P_Grenade_AFGrenPhysBounceEntropy).f;
			vecDir[0] += pmove->RandomFloat(-flEntropy, flEntropy);
			vecDir[1] += pmove->RandomFloat(-flEntropy, flEntropy);
			vecDir[2] += pmove->RandomFloat(-flEntropy, flEntropy);
			VectorNormalize(vecDir);

			flSpeed *= numplanes * RC2Pref(RC2P_Grenade_AFGrenPhysBounceAccelFactor).f + 1.0;
			if (flSpeed >= pmove->movevars->maxvelocity)
				flSpeed = pmove->movevars->maxvelocity - 1.0;

			VectorScale(vecDir, flSpeed, vecDir);
			VectorCopy(vecDir, pmove->velocity);
			VectorCopy(vecDir, original_velocity);
			continue;
		}

		// modify original_velocity so it parallels all of the clip planes
		//
		if (pmove->movetype == MOVETYPE_WALK && ((pmove->onground == -1) || (pmove->friction != 1))) // relfect player velocity
		{
			for (i = 0; i < numplanes; i++)
			{
				if (planes[i][2] > 0.7)
				{ // floor or slope
					PM_ClipVelocity(original_velocity, planes[i], new_velocity, 1);
					VectorCopy(new_velocity, original_velocity);
				}
				else
					PM_ClipVelocity(original_velocity, planes[i], new_velocity, 1.0 + pmove->movevars->bounce * (1 - pmove->friction));
			}

			VectorCopy(new_velocity, pmove->velocity);
			VectorCopy(new_velocity, original_velocity);
		}
		else
		{
			for (i = 0; i < numplanes; i++)
			{
				PM_ClipVelocity(
					original_velocity,
					planes[i],
					pmove->velocity,
					1);
				for (j = 0; j < numplanes; j++)
					if (j != i)
					{
						// Are we now moving against this plane?
						if (DotProduct(pmove->velocity, planes[j]) < 0)
							break; // not ok
					}
				if (j == numplanes) // Didn't have to clip, so we're ok
					break;
			}

			// Did we go all the way through plane set
			if (i != numplanes)
			{ // go along this plane
				// pmove->velocity is set in clipping call, no need to set again.
				;
			}
			else
			{ // go along the crease
				if (numplanes != 2)
				{
					//Con_Printf ("clip velocity, numplanes == %i\n",numplanes);
					VectorCopy(vec3_origin, pmove->velocity);
					//Con_DPrintf("Trapped 4\n");

					break;
				}
				CrossProduct(planes[0], planes[1], dir);
				d = DotProduct(dir, pmove->velocity);
				VectorScale(dir, d, pmove->velocity);
			}

			//
			// if original velocity is against the original velocity, stop dead
			// to avoid tiny occilations in sloping corners
			//
			if (DotProduct(pmove->velocity, primal_velocity) <= 0)
			{
				//Con_DPrintf("Back\n");
				VectorCopy(vec3_origin, pmove->velocity);
				break;
			}
		}
	}

	if (allFraction == 0)
	{
		VectorCopy(vec3_origin, pmove->velocity);
		//Con_DPrintf( "Don't stick\n" );
	}

	if (g_iPMMode == PM_MODE_HEADCRAB)
		PM_RestorePassThrough();

	return blocked;
}

/*
==============
PM_Accelerate
==============
*/
void PM_Accelerate(Vector wishdir, float wishspeed, float accel)
{
	int i;
	float addspeed, accelspeed, currentspeed;

	// Dead player's don't accelerate
	if (0 != pmove->dead)
		return;

	// If waterjumping, don't accelerate
	if (0 != pmove->waterjumptime)
		return;

	// See if we are changing direction a bit
	currentspeed = DotProduct(pmove->velocity, wishdir);

	// Reduce wishspeed by the amount of veer.
	addspeed = wishspeed - currentspeed;

	// If not going to add any speed, done.
	if (addspeed <= 0)
		return;

	// Determine amount of accleration.
	accelspeed = accel * pmove->frametime * wishspeed * pmove->friction;

	// Cap at addspeed
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	// Adjust velocity.
	for (i = 0; i < 3; i++)
	{
		pmove->velocity[i] += accelspeed * wishdir[i];
	}
}

/*
=====================
PM_WalkMove

Only used by players.  Moves along the ground when player is a MOVETYPE_WALK.
======================
*/
// orig 0xa2540, orig client 0xb3ac0. RC2: the speed attribute scales the wish velocity and the cap, each movement
// mode has its own ground speed, and shrunk players walk and step up at 0.2
void PM_WalkMove()
{
	int clip;
	int oldonground;
	int i;

	Vector wishvel;
	float spd;
	float fmove, smove;
	Vector wishdir;
	float wishspeed;
	float flMaxSpeed;

	Vector dest, start;
	Vector original, originalvel;
	Vector down, downvel;
	float downdist, updist;

	pmtrace_t trace;

	const float flSpeedScale = PM_GetAttributeScale(1, g_iPMPlayerEnt);

	// Copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;

	// Zero out z components of movement vectors
	pmove->forward[2] = 0;
	pmove->right[2] = 0;

	VectorNormalize(pmove->forward); // Normalize remainder of vectors.
	VectorNormalize(pmove->right);	 //

	for (i = 0; i < 2; i++) // Determine x and y parts of velocity
	{
		if (g_iPMMode == PM_MODE_NORMAL)
			wishvel[i] = (pmove->forward[i] * fmove + pmove->right[i] * smove) * flSpeedScale;
		else
			wishvel[i] = pmove->forward[i] * fmove + pmove->right[i] * smove;
	}

	wishvel[2] = 0; // Zero out z part of velocity

	VectorCopy(wishvel, wishdir); // Determine maginitude of speed of move
	wishspeed = VectorNormalize(wishdir);

	// Original bug, kept: in a mode with no ground speed of its own (the controller and race
	// modes, which don't walk here in practice) the cap is the forward move
	flMaxSpeed = fmove;
	switch (g_iPMMode)
	{
	case PM_MODE_NORMAL:
		flMaxSpeed = flSpeedScale * pmove->maxspeed;
		break;
	case PM_MODE_ASSASSIN:
		flMaxSpeed = RC2Pref(RC2P_TransformAssassin_RunSpeed).f;
		break;
	case PM_MODE_HOUNDEYE:
		flMaxSpeed = RC2Pref(RC2P_TransformHoundeye_RunSpeed).f;
		break;
	case PM_MODE_BAT:
		flMaxSpeed = RC2Pref(RC2P_TransformBat_GroundSpeed).f;
		break;
	case PM_MODE_HEADCRAB:
		flMaxSpeed = RC2Pref(RC2P_TransformHeadcrab_RunSpeed).f;
		break;
	}

	if (0 != g_bPMShrunk)
		flMaxSpeed *= 0.2;

	//
	// Clamp to server defined max speed
	//
	if (wishspeed > flMaxSpeed && wishspeed != 0)
	{
		VectorScale(wishvel, flMaxSpeed / wishspeed, wishvel);
		wishspeed = flMaxSpeed;
	}

	// Set pmove velocity
	pmove->velocity[2] = 0;
	PM_Accelerate(wishdir, wishspeed, pmove->movevars->accelerate);
	pmove->velocity[2] = 0;

	// Add in any base velocity to the current velocity.
	VectorAdd(pmove->velocity, pmove->basevelocity, pmove->velocity);

	spd = Length(pmove->velocity);

	if (spd < 1.0f)
	{
		VectorClear(pmove->velocity);
		return;
	}

	// If we are not moving, do nothing
	//if (!pmove->velocity[0] && !pmove->velocity[1] && !pmove->velocity[2])
	//	return;

	oldonground = pmove->onground;

	// first try just moving to the destination
	dest[0] = pmove->origin[0] + pmove->velocity[0] * pmove->frametime;
	dest[1] = pmove->origin[1] + pmove->velocity[1] * pmove->frametime;
	dest[2] = pmove->origin[2];

	// first try moving directly to the next spot
	VectorCopy(dest, start);
	trace = pmove->PM_PlayerTrace(pmove->origin, dest, PM_NORMAL, -1);
	// If we made it all the way, then copy trace end
	//  as new player position.
	if (trace.fraction == 1)
	{
		VectorCopy(trace.endpos, pmove->origin);
		return;
	}

	if (oldonground == -1 && // Don't walk up stairs if not on ground.
		pmove->waterlevel == 0)
		return;

	if (0 != pmove->waterjumptime) // If we are jumping out of water, don't do anything more.
		return;

	const float flStepSize = 0 != g_bPMShrunk ? pmove->movevars->stepsize * 0.2 : pmove->movevars->stepsize;

	// Try sliding forward both on ground and up 16 pixels
	//  take the move that goes farthest
	VectorCopy(pmove->origin, original);	  // Save out original pos &
	VectorCopy(pmove->velocity, originalvel); //  velocity.

	// Slide move
	clip = PM_FlyMove();

	// Copy the results out
	VectorCopy(pmove->origin, down);
	VectorCopy(pmove->velocity, downvel);

	// Reset original values.
	VectorCopy(original, pmove->origin);

	VectorCopy(originalvel, pmove->velocity);

	// Start out up one stair height
	VectorCopy(pmove->origin, dest);
	dest[2] += flStepSize;

	trace = pmove->PM_PlayerTrace(pmove->origin, dest, PM_NORMAL, -1);
	// If we started okay and made it part of the way at least,
	//  copy the results to the movement start position and then
	//  run another move try.
	if (0 == trace.startsolid && 0 == trace.allsolid)
	{
		VectorCopy(trace.endpos, pmove->origin);
	}

	// slide move the rest of the way.
	clip = PM_FlyMove();

	// Now try going back down from the end point
	//  press down the stepheight
	VectorCopy(pmove->origin, dest);
	dest[2] -= flStepSize;

	trace = pmove->PM_PlayerTrace(pmove->origin, dest, PM_NORMAL, -1);

	// If we are not on the ground any more then
	//  use the original movement attempt
	if (trace.plane.normal[2] < 0.7)
		goto usedown;
	// If the trace ended up in empty space, copy the end
	//  over to the origin.
	if (0 == trace.startsolid && 0 == trace.allsolid)
	{
		VectorCopy(trace.endpos, pmove->origin);
	}
	// Copy this origion to up.
	VectorCopy(pmove->origin, pmove->up);

	// decide which one went farther
	downdist = (down[0] - original[0]) * (down[0] - original[0]) + (down[1] - original[1]) * (down[1] - original[1]);
	updist = (pmove->up[0] - original[0]) * (pmove->up[0] - original[0]) + (pmove->up[1] - original[1]) * (pmove->up[1] - original[1]);

	if (downdist > updist)
	{
	usedown:
		VectorCopy(down, pmove->origin);
		VectorCopy(downvel, pmove->velocity);
	}
	else // copy z value from slide move
		pmove->velocity[2] = downvel[2];
}

/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/
// orig 0xa2b60, orig client 0xb4270. RC2: none in PMF_BOUNCE, no edge friction for the bat, and the stop speed is
// g_flPMStopSpeed (PM_Move)
void PM_Friction()
{
	float* vel;
	float speed, newspeed, control;
	float friction;
	float drop;
	Vector newvel;

	// If we are in water jump cycle, don't apply friction
	if (0 != pmove->waterjumptime || (g_iPMFlags & PMF_BOUNCE) != 0)
		return;

	// Get velocity
	vel = pmove->velocity;

	// Calculate speed
	speed = sqrt(vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2]);

	// If too slow, return
	if (speed < 0.1f)
	{
		return;
	}

	drop = 0;

	// apply ground friction
	if (pmove->onground != -1) // On an entity that is the ground
	{
		Vector start, stop;
		pmtrace_t trace;

		start[0] = stop[0] = pmove->origin[0] + vel[0] / speed * 16;
		start[1] = stop[1] = pmove->origin[1] + vel[1] / speed * 16;
		start[2] = pmove->origin[2] + pmove->player_mins[pmove->usehull][2];
		stop[2] = start[2] - 34;

		trace = pmove->PM_PlayerTrace(start, stop, PM_NORMAL, -1);

		if (trace.fraction == 1.0 && g_iPMMode != PM_MODE_BAT)
			friction = pmove->movevars->friction * pmove->movevars->edgefriction;
		else
			friction = pmove->movevars->friction;

		// Grab friction value.
		//friction = pmove->movevars->friction;

		friction *= pmove->friction; // player friction?

		// Bleed off some speed, but if we have less than the bleed
		//  threshhold, bleed the theshold amount.
		control = (speed < g_flPMStopSpeed) ? g_flPMStopSpeed : speed;
		// Add the amount to t'he drop amount.
		drop += control * friction * pmove->frametime;
	}

	// apply water friction
	//	if (pmove->waterlevel)
	//		drop += speed * pmove->movevars->waterfriction * waterlevel * pmove->frametime;

	// scale the velocity
	newspeed = speed - drop;
	if (newspeed < 0)
		newspeed = 0;

	// Determine proportion of old speed we are using.
	newspeed /= speed;

	// Adjust velocity according to proportion.
	newvel[0] = vel[0] * newspeed;
	newvel[1] = vel[1] * newspeed;
	newvel[2] = vel[2] * newspeed;

	VectorCopy(newvel, pmove->velocity);
}

/*
==================
Rocket Crowbar 2: the free-flying moves (the jetpack, the controller, the bat). Each has a
friction and an accelerate of its own, scaled by its rc2.ini FrictionScale (the accelerate
too, as the original has it), then moves as one step-up trace and a fly move. Names ours.
==================
*/
static float PM_FlyFriction(float flFrictionScale) // a helper shared by the three below, no original
{
	float control = 0 != g_bPMShrunk ? 14.0 : 70.0;
	const float speed = sqrt(pmove->velocity[0] * pmove->velocity[0] + pmove->velocity[1] * pmove->velocity[1] + pmove->velocity[2] * pmove->velocity[2]);

	if (speed == 0.0)
		return 0.0;

	if (control <= speed)
		control = speed;

	float newspeed = speed - control * pmove->frametime * (flFrictionScale * pmove->friction);
	if (newspeed < 0.0)
		newspeed = 0.0;

	newspeed /= speed;
	pmove->velocity[0] *= newspeed;
	pmove->velocity[1] *= newspeed;
	pmove->velocity[2] *= newspeed;
	return newspeed;
}

static void PM_FlyAccelerate(const Vector& wishdir, float wishspeed, float accel, float flFrictionScale) // a helper, no original
{
	if (0 != pmove->dead || 0 != pmove->waterjumptime)
		return;

	const float addspeed = wishspeed - DotProduct(pmove->velocity, wishdir);
	if (addspeed <= 0.0)
		return;

	float accelspeed = pmove->friction * pmove->frametime * flFrictionScale * wishspeed * accel;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (int i = 0; i < 3; i++)
		pmove->velocity[i] += accelspeed * wishdir[i];
}

// the end of every fly move: step up from the destination, keep that if clear, then slide
static void PM_FlyStepAndMove() // a helper, no original
{
	Vector dest, start;
	pmtrace_t trace;

	VectorMA(pmove->origin, pmove->frametime, pmove->velocity, dest);
	VectorCopy(dest, start);
	start[2] += (0 != g_bPMShrunk ? pmove->movevars->stepsize * 0.2 : pmove->movevars->stepsize) + 1.0;

	trace = pmove->PM_PlayerTrace(start, dest, PM_NORMAL, -1);
	if (0 == trace.startsolid && 0 == trace.allsolid)
		VectorCopy(trace.endpos, pmove->origin); // and the fly move still runs from there

	VectorAdd(pmove->velocity, pmove->basevelocity, pmove->velocity);
	PM_FlyMove();
	VectorSubtract(pmove->velocity, pmove->basevelocity, pmove->velocity);
}

float PM_JetpackFriction() // orig 0xa2d20, orig client 0xb44a0, name ours
{
	return PM_FlyFriction(RC2Pref(RC2P_Attributes_JetpackPhysFrictionScale).f);
}

void PM_JetpackAccelerate(const Vector& wishdir, float wishspeed, float accel) // orig 0xa2e00, orig client 0xb45c0, name ours
{
	PM_FlyAccelerate(wishdir, wishspeed, accel, RC2Pref(RC2P_Attributes_JetpackPhysFrictionScale).f);
}

void PM_JetpackMove() // orig 0xa2eb0, orig client 0xb46a0, name ours
{
	Vector wishvel, wishdir;
	float wishspeed, flMaxSpeed;
	int i;

	const float flSpeedScale = PM_GetAttributeScale(1, g_iPMPlayerEnt);

	for (i = 0; i < 3; i++)
		wishvel[i] = (pmove->right[i] * pmove->cmd.sidemove + pmove->forward[i] * pmove->cmd.forwardmove) * flSpeedScale;

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	flMaxSpeed = RC2Pref(RC2P_Attributes_JetpackPhysMaxSpeedScale).f * pmove->maxspeed * flSpeedScale;
	if (0 != g_bPMShrunk)
		flMaxSpeed *= 0.2;

	if (wishspeed > flMaxSpeed && wishspeed != 0)
	{
		VectorScale(wishvel, flMaxSpeed / wishspeed, wishvel);
		wishspeed = flMaxSpeed;
	}

	PM_JetpackAccelerate(wishdir, wishspeed, RC2Pref(RC2P_Attributes_JetpackPhysAccelScale).f * pmove->movevars->accelerate);
	PM_JetpackFriction();
	PM_CheckVelocity();
	PM_FlyStepAndMove();
}

float PM_ControllerFriction() // orig 0xa3110, orig client 0xb49a0, name ours
{
	return PM_FlyFriction(RC2Pref(RC2P_TransformController_FrictionScale).f);
}

void PM_ControllerAccelerate(const Vector& wishdir, float wishspeed, float accel) // orig 0xa31f0, orig client 0xb4ac0, name ours
{
	PM_FlyAccelerate(wishdir, wishspeed, accel, RC2Pref(RC2P_TransformController_FrictionScale).f);
}

bool PM_ApplyPullForces();

void PM_ControllerMove() // orig 0xa32a0, orig client 0xb4ba0, name ours
{
	Vector wishvel, wishdir;
	float wishspeed, flMaxSpeed;
	int i;

	for (i = 0; i < 3; i++)
		wishvel[i] = pmove->right[i] * pmove->cmd.sidemove + pmove->forward[i] * pmove->cmd.forwardmove;
	wishvel[2] += pmove->cmd.upmove;

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	if (pmove->waterlevel == 0)
		flMaxSpeed = RC2Pref(RC2P_TransformController_RunSpeed).f;
	else
		flMaxSpeed = RC2Pref(RC2P_TransformController_SwimSpeed).f;
	if (0 != g_bPMShrunk)
		flMaxSpeed *= 0.2;

	if (wishspeed > flMaxSpeed && wishspeed != 0)
	{
		VectorScale(wishvel, flMaxSpeed / wishspeed, wishvel);
		wishspeed = flMaxSpeed;
	}

	PM_ControllerAccelerate(wishdir, wishspeed, RC2Pref(RC2P_TransformController_AccelScale).f * pmove->movevars->accelerate);
	PM_ApplyPullForces();
	PM_ControllerFriction();
	PM_CheckVelocity();
	PM_FlyStepAndMove();
}

float PM_BatFriction() // orig 0xa3500, orig client 0xb4ea0, name ours
{
	return PM_FlyFriction(RC2Pref(RC2P_TransformBat_FrictionScale).f);
}

void PM_BatAccelerate(const Vector& wishdir, float wishspeed, float accel) // orig 0xa35e0, orig client 0xb4fc0, name ours
{
	PM_FlyAccelerate(wishdir, wishspeed, accel, RC2Pref(RC2P_TransformBat_FrictionScale).f);
}

void PM_BatMove() // orig 0xa3690, orig client 0xb50a0, name ours
{
	Vector wishvel, wishdir;
	float wishspeed, flMaxSpeed;
	int i;

	// the bat's keys are capped to its walk speed (and half of it up/down and backwards)
	const float flWalk = RC2Pref(RC2P_TransformBat_WalkSpeed).f;
	if (pmove->cmd.sidemove > flWalk)
		pmove->cmd.sidemove = flWalk;
	else if (pmove->cmd.sidemove < -flWalk)
		pmove->cmd.sidemove = -flWalk;

	const float flHalf = flWalk * 0.5;
	if (pmove->cmd.upmove > flHalf)
		pmove->cmd.upmove = flHalf;
	else if (pmove->cmd.upmove < -flHalf)
		pmove->cmd.upmove = -flHalf;

	if (pmove->cmd.forwardmove < -flHalf)
		pmove->cmd.forwardmove = -flHalf;

	for (i = 0; i < 3; i++)
		wishvel[i] = pmove->right[i] * pmove->cmd.sidemove + pmove->forward[i] * pmove->cmd.forwardmove;
	wishvel[2] += pmove->cmd.upmove;

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	if (pmove->waterlevel == 0)
		flMaxSpeed = RC2Pref(RC2P_TransformBat_RunSpeed).f;
	else
		flMaxSpeed = RC2Pref(RC2P_TransformBat_SwimSpeed).f;
	if (0 != g_bPMShrunk)
		flMaxSpeed *= 0.2;

	if (wishspeed > flMaxSpeed && wishspeed != 0)
	{
		VectorScale(wishvel, flMaxSpeed / wishspeed, wishvel);
		wishspeed = flMaxSpeed;
	}

	PM_BatAccelerate(wishdir, wishspeed, RC2Pref(RC2P_TransformBat_AccelScale).f * pmove->movevars->accelerate);
	PM_ApplyPullForces();
	PM_BatFriction();

	if ((g_iPMFlags & PMF_HELD) != 0)
		PM_AddGravity();

	PM_CheckVelocity();
	PM_FlyStepAndMove();
}

// orig 0xa39a0, orig client 0xb5480. RC2: the air control cap depends on the movement mode
void PM_AirAccelerate(Vector wishdir, float wishspeed, float accel)
{
	int i;
	float addspeed, accelspeed, currentspeed, wishspd = wishspeed;

	if (0 != pmove->dead)
		return;
	if (0 != pmove->waterjumptime)
		return;

	// Cap speed
	//wishspd = VectorNormalize (pmove->wishveloc);

	float flCap;
	switch (g_iPMMode)
	{
	case PM_MODE_ASSASSIN:
		flCap = 80;
		break;
	case PM_MODE_HOUNDEYE:
		flCap = 5;
		break;
	case PM_MODE_HEADCRAB:
		flCap = 50;
		break;
	default:
		flCap = 30;
		break;
	}

	if (wishspd > flCap)
		wishspd = flCap;
	// Determine veer amount
	currentspeed = DotProduct(pmove->velocity, wishdir);
	// See how much to add
	addspeed = wishspd - currentspeed;
	// If not adding any, done.
	if (addspeed <= 0)
		return;
	// Determine acceleration speed after acceleration

	accelspeed = accel * wishspeed * pmove->frametime * pmove->friction;
	// Cap it
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	// Adjust pmove vel.
	for (i = 0; i < 3; i++)
	{
		pmove->velocity[i] += accelspeed * wishdir[i];
	}
}

/*
===================
PM_WaterMove

===================
*/
// orig 0xa3ac0, orig client 0xb55f0. RC2: the speed attribute and the swim speed of each movement mode; shrunk
// players step up at 0.2
void PM_WaterMove()
{
	int i;
	Vector wishvel;
	float wishspeed;
	Vector wishdir;
	Vector start, dest;
	Vector temp;
	pmtrace_t trace;

	float speed, newspeed, addspeed, accelspeed;

	float flSpeedScale = PM_GetAttributeScale(1, g_iPMPlayerEnt);

	//
	// user intentions
	//
	for (i = 0; i < 3; i++)
	{
		if (g_iPMMode == PM_MODE_NORMAL)
			wishvel[i] = (pmove->forward[i] * pmove->cmd.forwardmove + pmove->right[i] * pmove->cmd.sidemove) * flSpeedScale;
		else
			wishvel[i] = pmove->forward[i] * pmove->cmd.forwardmove + pmove->right[i] * pmove->cmd.sidemove;
	}

	// Sinking after no other movement occurs
	if (0 == pmove->cmd.forwardmove && 0 == pmove->cmd.sidemove && 0 == pmove->cmd.upmove)
		wishvel[2] -= 60; // drift towards bottom
	else				  // Go straight up by upmove amount.
		wishvel[2] += flSpeedScale * pmove->cmd.upmove;

	// Copy it over and determine speed
	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	// Original bug, kept: the bat (and any mode without a swim speed) caps at the speed
	// attribute itself, about 1
	float flMaxSpeed = flSpeedScale;
	switch (g_iPMMode)
	{
	case PM_MODE_NORMAL:
		flMaxSpeed = flSpeedScale * pmove->maxspeed;
		break;
	case PM_MODE_ASSASSIN:
		flMaxSpeed = RC2Pref(RC2P_TransformAssassin_SwimSpeed).f;
		break;
	case PM_MODE_HOUNDEYE:
		flMaxSpeed = RC2Pref(RC2P_TransformHoundeye_SwimSpeed).f;
		break;
	case PM_MODE_HEADCRAB:
		flMaxSpeed = RC2Pref(RC2P_TransformHeadcrab_SwimSpeed).f;
		break;
	}
	if (0 != g_bPMShrunk)
		flMaxSpeed *= 0.2;

	// Cap speed.
	if (wishspeed > flMaxSpeed && wishspeed != 0)
	{
		VectorScale(wishvel, flMaxSpeed / wishspeed, wishvel);
		wishspeed = flMaxSpeed;
	}
	// Slow us down a bit.
	wishspeed *= 0.8;

	VectorAdd(pmove->velocity, pmove->basevelocity, pmove->velocity);
	// Water friction
	VectorCopy(pmove->velocity, temp);
	speed = VectorNormalize(temp);
	if (0 != speed)
	{
		newspeed = speed - pmove->frametime * speed * pmove->movevars->friction * pmove->friction;

		if (newspeed < 0)
			newspeed = 0;
		VectorScale(pmove->velocity, newspeed / speed, pmove->velocity);
	}
	else
		newspeed = 0;

	//
	// water acceleration
	//
	if (wishspeed < 0.1f)
	{
		return;
	}

	addspeed = wishspeed - newspeed;
	if (addspeed > 0)
	{

		VectorNormalize(wishvel);
		accelspeed = pmove->movevars->accelerate * wishspeed * pmove->frametime * pmove->friction;
		if (accelspeed > addspeed)
			accelspeed = addspeed;

		for (i = 0; i < 3; i++)
			pmove->velocity[i] += accelspeed * wishvel[i];
	}

	// Now move
	// assume it is a stair or a slope, so press down from stepheight above
	VectorMA(pmove->origin, pmove->frametime, pmove->velocity, dest);
	VectorCopy(dest, start);
	start[2] += (0 != g_bPMShrunk ? pmove->movevars->stepsize * 0.2 : pmove->movevars->stepsize) + 1;
	trace = pmove->PM_PlayerTrace(start, dest, PM_NORMAL, -1);
	if (0 == trace.startsolid && 0 == trace.allsolid) // FIXME: check steep slope?
	{												  // walked up the step, so just keep result and exit
		VectorCopy(trace.endpos, pmove->origin);
		return;
	}

	// Try moving straight along out normal path.
	PM_FlyMove();
}

/*
===================
Rocket Crowbar 2: the wallhop (jump off a wall in the air) and the jetboost (an energy-paid
push where the player looks). Both need their ability (PM_GetAbilityFlags) as a normal
player; the assassin wallhops by nature. Names ours.
===================
*/
void PM_WallhopApply(const Vector& normal) // orig 0xa3ea0, orig client 0xb5ad0, name ours
{
	const float flShrink = 0 != g_bPMShrunk ? 0.2 : 1.0;
	Vector push;

	if (g_iPMMode == PM_MODE_ASSASSIN)
	{
		VectorScale(normal, (Length(pmove->velocity) * RC2Pref(RC2P_TransformAssassin_WallhopVelReboundScale).f + RC2Pref(RC2P_TransformAssassin_WallhopVelBaseAdd).f) * flShrink, push);
		VectorAdd(pmove->velocity, push, pmove->velocity);
		if (normal[2] < 0.3 && normal[2] > -0.1)
			pmove->velocity[2] += RC2Pref(RC2P_TransformAssassin_WallhopWallZAdd).f;
	}
	else if (g_iPMMode == PM_MODE_NORMAL)
	{
		const float flJump = PM_GetAttributeScale(0, g_iPMPlayerEnt);
		VectorScale(normal, (Length(pmove->velocity) * RC2Pref(RC2P_Attributes_WallhopVelReboundScale).f + RC2Pref(RC2P_Attributes_WallhopVelBaseAdd).f) * flJump * flShrink, push);
		VectorAdd(pmove->velocity, push, pmove->velocity);
		if (normal[2] < 0.3 && normal[2] > -0.1)
			pmove->velocity[2] += RC2Pref(RC2P_Attributes_WallhopWallZAdd).f * flJump;
	}
}

// trace from the origin along vecDir; on a hit take its plane as the wall and the trace's
// end as where the wall was looked for
static void PM_WallhopTrace(const Vector& vecDir, Vector& normal, Vector& vecWall) // a helper, no original
{
	Vector end;
	pmtrace_t trace;

	VectorAdd(pmove->origin, vecDir, end);
	trace = pmove->PM_PlayerTrace(pmove->origin, end, PM_STUDIO_IGNORE, -1);
	if (trace.fraction < 1.0)
	{
		normal = trace.plane.normal;
		vecWall = end;
	}
}

void PM_CheckWallhop() // orig 0xa4030, orig client 0xb5cd0, name ours
{
	const float flShrink = 0 != g_bPMShrunk ? 0.2 : 1.0;
	Vector normal = g_vecZero, vecWall, vecDir;
	int i;

	if (g_iPMMode == PM_MODE_NORMAL)
	{
		if ((PM_GetAbilityFlags(g_iPMPlayerEnt) & 1) == 0)
			return;
	}
	else if (g_iPMMode != PM_MODE_ASSASSIN)
	{
		return;
	}

	if ((pmove->cmd.buttons & IN_JUMP) == 0 || PM_GetPlayerTimer(g_iPMPlayerEnt, 0) >= 1)
		return;

	PM_SetPlayerTimer(g_iPMPlayerEnt, 0, 100);

	// the first world or brush entity touched this move is the wall
	bool bFound = false;
	for (i = 0; i < pmove->numtouch; i++)
	{
		const pmtrace_t& touch = pmove->touchindex[i];
		if (touch.ent == 0 || pmove->physents[touch.ent].model != NULL)
		{
			normal = touch.plane.normal;
			VectorMA(touch.endpos, -32, normal, vecWall);
			bFound = normal != g_vecZero;
			break;
		}
	}

	// else look for one ahead, then in front and behind
	if (!bFound)
	{
		vecDir = pmove->velocity;
		VectorNormalize(vecDir);
		VectorScale(vecDir, RC2Pref(RC2P_Attributes_WallhopCheckDistance).f * flShrink, vecDir);
		PM_WallhopTrace(vecDir, normal, vecWall);
	}
	if (normal == g_vecZero)
	{
		VectorScale(pmove->forward, RC2Pref(RC2P_Attributes_WallhopCheckDistance).f * flShrink, vecDir);
		PM_WallhopTrace(vecDir, normal, vecWall);
	}
	if (normal == g_vecZero)
	{
		VectorScale(pmove->forward, -(RC2Pref(RC2P_Attributes_WallhopCheckDistance).f * flShrink), vecDir);
		PM_WallhopTrace(vecDir, normal, vecWall);
	}
	if (normal == g_vecZero)
		return;

	if (normal[2] <= 0.8)
	{
		PM_WallhopApply(normal);
		PM_CatagorizeTextureTypeAt(pmove->origin, vecWall);
		PM_PlayStepSound(PM_MapTextureTypeStepType(pmove->chtexturetype), 1.0);
	}
}

// orig client 0xb6330: empty in the client, the jetboost is the server's alone
void PM_CheckJetboost() // orig 0xa4540, name ours
{
#ifndef CLIENT_DLL
	Vector vecDir;

	if ((PM_GetAbilityFlags(g_iPMPlayerEnt) & 4) == 0 || (pmove->cmd.buttons & IN_JUMP) == 0 || g_iPMMode != PM_MODE_NORMAL)
		return;

	if (PM_GetPlayerTimer(g_iPMPlayerEnt, 1) >= 1)
		return;

	PM_SetPlayerTimer(g_iPMPlayerEnt, 1, 250);

	if (PM_GetEnergy(g_iPMPlayerEnt) < RC2Pref(RC2P_Attributes_JetboostEnergyCost).f)
		return;

	PM_UseEnergy(g_iPMPlayerEnt, RC2Pref(RC2P_Attributes_JetboostEnergyCost).f, RC2Pref(RC2P_Attributes_JetboostEnergyRechargeDelay).f);

	AngleVectors(pmove->angles, &vecDir, NULL, NULL);
	if (vecDir[2] < RC2Pref(RC2P_Attributes_JetboostMinZ).f)
		vecDir[2] = RC2Pref(RC2P_Attributes_JetboostMinZ).f;
	if (vecDir[2] > RC2Pref(RC2P_Attributes_JetboostMaxZ).f)
		vecDir[2] = RC2Pref(RC2P_Attributes_JetboostMaxZ).f;
	VectorNormalize(vecDir);

	VectorMA(pmove->velocity, RC2Pref(RC2P_Attributes_JetboostVelBoost).f, vecDir, pmove->velocity);
	PM_JetboostEffect(g_iPMPlayerEnt, 0, vecDir, pmove->velocity);
	PM_SetPlayerTimer(g_iPMPlayerEnt, 1, (int)(RC2Pref(RC2P_Attributes_JetboostReuseDelay).f * 1000.0));
#endif
}


/*
===================
PM_AirMove

===================
*/
// orig 0xa46a0, orig client 0xb6350. RC2: then the wallhop and the jetboost
void PM_AirMove()
{
	int i;
	Vector wishvel;
	float fmove, smove;
	Vector wishdir;
	float wishspeed;

	// Copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;

	// Zero out z components of movement vectors
	pmove->forward[2] = 0;
	pmove->right[2] = 0;
	// Renormalize
	VectorNormalize(pmove->forward);
	VectorNormalize(pmove->right);

	// Determine x and y parts of velocity
	for (i = 0; i < 2; i++)
	{
		wishvel[i] = pmove->forward[i] * fmove + pmove->right[i] * smove;
	}
	// Zero out z part of velocity
	wishvel[2] = 0;

	// Determine maginitude of speed of move
	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	// Clamp to server defined max speed
	if (wishspeed > pmove->maxspeed)
	{
		VectorScale(wishvel, pmove->maxspeed / wishspeed, wishvel);
		wishspeed = pmove->maxspeed;
	}

	PM_AirAccelerate(wishdir, wishspeed, pmove->movevars->airaccelerate);

	// Add in any base velocity to the current velocity.
	VectorAdd(pmove->velocity, pmove->basevelocity, pmove->velocity);

	PM_FlyMove();

	PM_CheckWallhop();
	PM_CheckJetboost();
}

bool PM_InWater()
{
	return (pmove->waterlevel > 1);
}

/*
=============
PM_CheckWater

Sets pmove->waterlevel and pmove->watertype values.
=============
*/
bool PM_CheckWater()
{
	Vector point;
	int cont;
	int truecont;
	float height;
	float heightover2;

	// Pick a spot just above the players feet.
	point[0] = pmove->origin[0] + (pmove->player_mins[pmove->usehull][0] + pmove->player_maxs[pmove->usehull][0]) * 0.5;
	point[1] = pmove->origin[1] + (pmove->player_mins[pmove->usehull][1] + pmove->player_maxs[pmove->usehull][1]) * 0.5;
	point[2] = pmove->origin[2] + pmove->player_mins[pmove->usehull][2] + 1;

	// Assume that we are not in water at all.
	pmove->waterlevel = 0;
	pmove->watertype = CONTENTS_EMPTY;

	// Grab point contents.
	cont = pmove->PM_PointContents(point, &truecont);
	// Are we under water? (not solid and not empty?)
	if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT)
	{
		// Set water type
		pmove->watertype = cont;

		// We are at least at level one
		pmove->waterlevel = 1;

		height = (pmove->player_mins[pmove->usehull][2] + pmove->player_maxs[pmove->usehull][2]);
		heightover2 = height * 0.5;

		// Now check a point that is at the player hull midpoint.
		point[2] = pmove->origin[2] + heightover2;
		cont = pmove->PM_PointContents(point, NULL);
		// If that point is also under water...
		if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT)
		{
			// Set a higher water level.
			pmove->waterlevel = 2;

			// Now check the eye position.  (view_ofs is relative to the origin)
			point[2] = pmove->origin[2] + pmove->view_ofs[2];

			cont = pmove->PM_PointContents(point, NULL);
			if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT)
				pmove->waterlevel = 3; // In over our eyes
		}

		// Adjust velocity based on water current, if any.
		if ((truecont <= CONTENTS_CURRENT_0) &&
			(truecont >= CONTENTS_CURRENT_DOWN))
		{
			// The deeper we are, the stronger the current.
			static Vector current_table[] =
				{
					{1, 0, 0}, {0, 1, 0}, {-1, 0, 0},
					{0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

			VectorMA(pmove->basevelocity, 50.0 * pmove->waterlevel, current_table[CONTENTS_CURRENT_0 - truecont], pmove->basevelocity);
		}
	}

	return pmove->waterlevel > 1;
}

/*
=============
PM_CatagorizePosition
=============
*/
void PM_CatagorizePosition()
{
	Vector point;
	pmtrace_t tr;

	// if the player hull point one unit down is solid, the player
	// is on ground

	// see if standing on something solid

	// Doing this before we move may introduce a potential latency in water detection, but
	// doing it after can get us stuck on the bottom in water if the amount we move up
	// is less than the 1 pixel 'threshold' we're about to snap to.	Also, we'll call
	// this several times per frame, so we really need to avoid sticking to the bottom of
	// water on each call, and the converse case will correct itself if called twice.
	PM_CheckWater();

	point[0] = pmove->origin[0];
	point[1] = pmove->origin[1];
	point[2] = pmove->origin[2] - 2;

	if (pmove->velocity[2] > 180) // Shooting up really fast.  Definitely not on ground.
	{
		pmove->onground = -1;
	}
	else
	{
		// Try and move down.
		tr = pmove->PM_PlayerTrace(pmove->origin, point, PM_NORMAL, -1);
		// If we hit a steep plane, we are not on ground
		if (tr.plane.normal[2] < 0.7)
			pmove->onground = -1; // too steep
		else
			pmove->onground = tr.ent; // Otherwise, point to index of ent under us.

		// If we are on something...
		if (pmove->onground != -1)
		{
			// Then we are not in water jump sequence
			pmove->waterjumptime = 0;
			// If we could make the move, drop us down that 1 pixel
			if (pmove->waterlevel < 2 && 0 == tr.startsolid && 0 == tr.allsolid)
				VectorCopy(tr.endpos, pmove->origin);
		}

		// Standing on an entity other than the world
		if (tr.ent > 0) // So signal that we are touching something.
		{
			PM_AddToTouched(tr, pmove->velocity);
		}
	}
}

/*
=================
PM_GetRandomStuckOffsets

When a player is stuck, it's costly to try and unstick them
Grab a test offset for the player based on a passed in index
=================
*/
int PM_GetRandomStuckOffsets(int nIndex, int server, Vector& offset)
{
	// Last time we did a full
	int idx;
	idx = rgStuckLast[nIndex][server]++;

	VectorCopy(rgv3tStuckTable[idx % 54], offset);

	return (idx % 54);
}

void PM_ResetStuckOffsets(int nIndex, int server)
{
	rgStuckLast[nIndex][server] = 0;
}

/*
=================
NudgePosition

If pmove->origin is in a solid position,
try nudging slightly on all axis to
allow for the cut precision of the net coordinates
=================
*/
#define PM_CHECKSTUCK_MINTIME 0.05 // Don't check again too quickly.

bool PM_TryToUnstuck(Vector base)
{
	float x, y, z;
	float xystep = 8.0;
	float zstep = 18.0;
	float xyminmax = xystep;
	float zminmax = 4 * zstep;
	Vector test;

	for (z = 0; z <= zminmax; z += zstep)
	{
		for (x = -xyminmax; x <= xyminmax; x += xystep)
		{
			for (y = -xyminmax; y <= xyminmax; y += xystep)
			{
				test = base;
				test[0] += x;
				test[1] += y;
				test[2] += z;

				if (pmove->PM_TestPlayerPosition(test, NULL) == -1)
				{
					VectorCopy(test, pmove->origin);
					return false;
				}
			}
		}
	}

	return true;
}

bool PM_CheckStuck()
{
	Vector base;
	Vector offset;
	Vector test;
	int hitent;
	int i;
	pmtrace_t traceresult;

	static float rgStuckCheckTime[MAX_PLAYERS][2]; // Last time we did a full

	// If position is okay, exit
	hitent = pmove->PM_TestPlayerPosition(pmove->origin, &traceresult);
	if (hitent == -1)
	{
		PM_ResetStuckOffsets(pmove->player_index, pmove->server);
		return false;
	}

	VectorCopy(pmove->origin, base);

	//
	// Deal with precision error in network and cases where the player can get stuck on level transitions in singleplayer.
	//
	if (0 == pmove->server || 0 == pmove->multiplayer)
	{
		// World or BSP model
		if ((hitent == 0) ||
			(pmove->physents[hitent].model != NULL))
		{
			int nReps = 0;
			PM_ResetStuckOffsets(pmove->player_index, pmove->server);
			do
			{
				i = PM_GetRandomStuckOffsets(pmove->player_index, pmove->server, offset);

				VectorAdd(base, offset, test);
				if (pmove->PM_TestPlayerPosition(test, &traceresult) == -1)
				{
					PM_ResetStuckOffsets(pmove->player_index, pmove->server);

					VectorCopy(test, pmove->origin);
					return false;
				}
				nReps++;
			} while (nReps < 54);
		}
	}

	// Always check if we've just changed levels.
	if (!(pmove->server != 0 && g_CheckForPlayerStuck))
	{
		// TODO: not really necessary to have separate arrays for client and server since the code is separate anyway.
		const int idx = 0 != pmove->server ? 0 : 1;

		const float fTime = pmove->Sys_FloatTime();
		// Too soon?
		if (rgStuckCheckTime[pmove->player_index][idx] >=
			(fTime - PM_CHECKSTUCK_MINTIME))
		{
			return true;
		}
		rgStuckCheckTime[pmove->player_index][idx] = fTime;
	}

	pmove->PM_StuckTouch(hitent, &traceresult);

	i = PM_GetRandomStuckOffsets(pmove->player_index, pmove->server, offset);

	VectorAdd(base, offset, test);
	if ((hitent = pmove->PM_TestPlayerPosition(test, NULL)) == -1)
	{
		//Con_DPrintf("Nudged\n");

		PM_ResetStuckOffsets(pmove->player_index, pmove->server);

		if (i >= 27)
			VectorCopy(test, pmove->origin);

		return false;
	}

	// Try to unstuck the player after a level change.
	// This only works in singleplayer. In multiplayer there it's too unreliable to try, so only the first player gets unstuck.
	if (pmove->server != 0 && g_CheckForPlayerStuck)
	{
		g_CheckForPlayerStuck = false;

		// Are we stuck inside the world?
		if (hitent == 0)
		{
			if (!PM_TryToUnstuck(base))
			{
				return false;
			}
		}
	}

	// If player is flailing while stuck in another player ( should never happen ), then see
	//  if we can't "unstick" them forceably.
	if ((pmove->cmd.buttons & (IN_JUMP | IN_DUCK | IN_ATTACK)) != 0 && (pmove->physents[hitent].player != 0))
	{
		if (!PM_TryToUnstuck(base))
		{
			return false;
		}
	}

	//VectorCopy (base, pmove->origin);

	return true;
}

// Rocket Crowbar 2: where a chase camera behind physent iTarget sits: 112 units back along
// angles from 28 above its origin, 4 off whatever the point hull hits on the way
void PM_GetChaseOrigin(const Vector& angles, int iTarget, const Vector& offset, Vector& vecOut) // orig 0xa4ef0, orig client 0xb6de0, name ours
{
	physent_t* pe = &pmove->physents[iTarget];
	Vector forward, start, end;

	AngleVectors(angles, &forward, NULL, NULL);

	start = pe->origin;
	start[2] += 28.0;

	VectorMA(offset, 112.0, forward, end);
	end = start - end;

	pmtrace_t* trace = pmove->PM_TraceLine(start, end, PM_NORMAL, 2, iTarget);
	VectorMA(trace->endpos, 4.0, trace->plane.normal, vecOut);
}

/*
===============
PM_SpectatorMove
===============
*/
// orig 0xa4fc0, orig client 0xb6ef0. RC2's is an older shape than SDK 2.3's: roaming also when there is no
// target, the stop speed is g_flPMStopSpeed, and a tracking spectator is put behind the
// target (PM_GetChaseOrigin) instead of on it
void PM_SpectatorMove()
{
	float speed, drop, friction, control, newspeed;
	//float   accel;
	float currentspeed, addspeed, accelspeed;
	int i;
	Vector wishvel;
	float fmove, smove;
	Vector wishdir;
	float wishspeed;
	// this routine keeps track of the spectators psoition
	// there a two different main move types : track player or moce freely (OBS_ROAMING)
	// doesn't need excate track position, only to generate PVS, so just copy
	// targets position and real view position is calculated on client (saves server CPU)

#ifdef CLIENT_DLL
	if (0 != pmove->runfuncs)
		g_iPMSpecView = 1;
#endif

	if (pmove->iuser1 == OBS_ROAMING || pmove->iuser2 == 0)
	{

#ifdef CLIENT_DLL
		// jump only in roaming mode
		if (iJumpSpectator)
		{
			VectorCopy(vJumpOrigin, pmove->origin);
			VectorCopy(vJumpAngles, pmove->angles);
			VectorCopy(vec3_origin, pmove->velocity);
			iJumpSpectator = false;
			return;
		}
#endif
		// Move around in normal spectator method

		speed = Length(pmove->velocity);
		if (speed < 1)
		{
			VectorCopy(vec3_origin, pmove->velocity)
		}
		else
		{
			drop = 0;

			friction = pmove->movevars->friction * 1.5; // extra friction
			control = speed < g_flPMStopSpeed ? g_flPMStopSpeed : speed;
			drop += control * friction * pmove->frametime;

			// scale the velocity
			newspeed = speed - drop;
			if (newspeed < 0)
				newspeed = 0;
			newspeed /= speed;

			VectorScale(pmove->velocity, newspeed, pmove->velocity);
		}

		// accelerate
		fmove = pmove->cmd.forwardmove;
		smove = pmove->cmd.sidemove;

		VectorNormalize(pmove->forward);
		VectorNormalize(pmove->right);

		for (i = 0; i < 3; i++)
		{
			wishvel[i] = pmove->forward[i] * fmove + pmove->right[i] * smove;
		}
		wishvel[2] += pmove->cmd.upmove;

		VectorCopy(wishvel, wishdir);
		wishspeed = VectorNormalize(wishdir);

		//
		// clamp to server defined max speed
		//
		if (wishspeed > pmove->movevars->spectatormaxspeed && wishspeed != 0)
		{
			VectorScale(wishvel, pmove->movevars->spectatormaxspeed / wishspeed, wishvel);
			wishspeed = pmove->movevars->spectatormaxspeed;
		}

		currentspeed = DotProduct(pmove->velocity, wishdir);
		addspeed = wishspeed - currentspeed;
		if (addspeed <= 0)
			return;

		accelspeed = pmove->movevars->accelerate * pmove->frametime * wishspeed;
		if (accelspeed > addspeed)
			accelspeed = addspeed;

		for (i = 0; i < 3; i++)
			pmove->velocity[i] += accelspeed * wishdir[i];

		// move
		VectorMA(pmove->origin, pmove->frametime, pmove->velocity, pmove->origin);
	}
	else
	{
		// all other modes just track some kind of target, so spectator PVS = target PVS

		int target;
		Vector angles, vecOrigin;

		// Find the client this player's targeting
		for (target = 0; target < pmove->numphysent; target++)
		{
			if (pmove->physents[target].info == pmove->iuser2)
				break;
		}

#ifdef CLIENT_DLL
		// Rocket Crowbar 2's client places the view itself (client 0xb6ef0): no target keeps
		// the last view; in-eye looks from behind the target at the second target, smoothed
		if (target == pmove->numphysent)
		{
			if (0 != pmove->runfuncs)
			{
				pmove->origin = v_origin;
				pmove->angles = v_angles;
				pmove->velocity = vec3_origin;
				g_bPMSetViewAngles = 1;
				g_bPMSetViewOrigin = 1;
			}
			return;
		}

		int target2 = 0;
		if (pmove->iuser1 == OBS_IN_EYE && 0 != pmove->iuser3)
		{
			for (target2 = 0; target2 < pmove->numphysent; target2++)
			{
				if (pmove->physents[target2].info == pmove->iuser3)
					break;
			}
			if (target2 == pmove->numphysent)
				target2 = -1;
		}

		if (pmove->iuser1 == OBS_CHASE_LOCKED)
		{
			angles[0] = 0;
			angles[1] = pmove->physents[target].angles[1];
			angles[2] = pmove->physents[target].angles[2];
			if (0 != pmove->runfuncs)
				g_iPMSpecView |= 6;
		}
		else if (pmove->iuser1 == OBS_IN_EYE)
		{
			if (target2 < 1)
			{
				// the original has a branch each for -1 and 0, the same
				angles = pmove->angles;
			}
			else
			{
				angles = pmove->physents[target2].origin - pmove->physents[target].origin;
				VectorAngles(angles, angles);

				if ((g_iSpecChaseFlags & 0x20) == 0)
					angles[0] = -angles[0] + 12.5;
				else
					angles[0] = -angles[0] - 12.5;

				if ((g_iSpecChaseFlags & 0x10) == 0)
					angles[1] = angles[1] - 22.5;
				else
					angles[1] = angles[1] + 22.5;

				// swing to the other side when the second target is hidden from there
				Vector vecChase;
				PM_GetChaseOrigin(angles, target, g_vecZero, vecChase);
				pmtrace_t trace = *pmove->PM_TraceLine(vecChase, pmove->physents[target2].origin, PM_STUDIO_IGNORE, 2, -1);
				if (trace.fraction != 1.0)
				{
					if ((g_iSpecChaseFlags & 0x10) == 0)
						angles[1] = angles[1] + 45.0;
					else
						angles[1] = angles[1] - 45.0;
				}
			}

			if (0 != pmove->runfuncs)
			{
				g_iPMSpecView |= 6;
				InterpolateAngles(s_vecInEyeAngles, angles, angles, 0.1f);
				s_vecInEyeAngles = angles;
			}
		}
		else
		{
			angles = pmove->angles;
		}

		PM_GetChaseOrigin(angles, target, g_vecZero, vecOrigin);

		pmove->origin = vecOrigin;
		pmove->angles = angles;
		pmove->velocity = vec3_origin;

		if (0 != pmove->runfuncs)
		{
			v_angles = pmove->angles;
			g_bPMSetViewAngles = 1;
			v_origin = pmove->origin;
			g_bPMSetViewOrigin = 1;
		}
	}
}
#else
		if (target == pmove->numphysent)
			return;

		// In-eye looks up the second target too, and uses nothing of it
		if (pmove->iuser1 == OBS_IN_EYE && 0 != pmove->iuser3)
		{
			for (int target2 = 0; target2 < pmove->numphysent; target2++)
			{
				if (pmove->physents[target2].info == pmove->iuser3)
					break;
			}
		}

		if (pmove->iuser1 == OBS_CHASE_LOCKED)
		{
			angles[0] = 0;
			angles[1] = pmove->physents[target].angles[1];
			angles[2] = pmove->physents[target].angles[2];
		}
		else if (pmove->iuser1 != OBS_IN_EYE)
		{
			angles = pmove->angles;
		}
		else
		{
			// Original bug: in-eye leaves these angles uninitialised (whatever was on the
			// stack). VERIFY: zero here, the original's value is undefined
			angles = g_vecZero;
		}

		PM_GetChaseOrigin(angles, target, g_vecZero, vecOrigin);

		pmove->origin = vecOrigin;
		pmove->angles = angles;

		// no velocity
		VectorCopy(vec3_origin, pmove->velocity);
	}
}
#endif

/*
==================
PM_SplineFraction

Use for ease-in, ease-out style interpolation (accel/decel)
Used by ducking code.
==================
*/
// orig 0xa5380. Not byte-identical to SDK 2.3's, but it reads the same
float PM_SplineFraction(float value, float scale)
{
	float valueSquared;

	value = scale * value;
	valueSquared = value * value;

	// Nice little ease-in, ease-out spline-like curve
	return 3 * valueSquared - 2 * valueSquared * value;
}

void PM_FixPlayerCrouchStuck(int direction)
{
	int hitent;
	int i;
	Vector test;

	hitent = pmove->PM_TestPlayerPosition(pmove->origin, NULL);
	if (hitent == -1)
		return;

	VectorCopy(pmove->origin, test);
	for (i = 0; i < 36; i++)
	{
		pmove->origin[2] += direction;
		hitent = pmove->PM_TestPlayerPosition(pmove->origin, NULL);
		if (hitent == -1)
			return;
	}

	VectorCopy(test, pmove->origin); // Failed
}

// orig 0xa5440, orig client 0xb7920. RC2: a shrunk player is never ducked and stays in the point hull
void PM_UnDuck()
{
	int i;
	pmtrace_t trace;
	Vector newOrigin;

	VectorCopy(pmove->origin, newOrigin);

	if (0 != g_bPMShrunk)
	{
		pmove->flags &= ~FL_DUCKING;
		pmove->bInDuck = 0;
		pmove->flDuckTime = 0;
		pmove->usehull = 2;
		return;
	}

	if (pmove->onground != -1)
	{
		for (i = 0; i < 3; i++)
		{
			newOrigin[i] += (pmove->player_mins[1][i] - pmove->player_mins[0][i]);
		}
	}

	trace = pmove->PM_PlayerTrace(newOrigin, newOrigin, PM_NORMAL, -1);

	if (0 == trace.startsolid)
	{
		pmove->usehull = 0;

		// Oh, no, changing hulls stuck us into something, try unsticking downward first.
		trace = pmove->PM_PlayerTrace(newOrigin, newOrigin, PM_NORMAL, -1);
		if (0 != trace.startsolid)
		{
			// See if we are stuck?  If so, stay ducked with the duck hull until we have a clear spot
			//Con_Printf( "unstick got stuck\n" );
			pmove->usehull = 1;
			return;
		}

		pmove->flags &= ~FL_DUCKING;
		pmove->bInDuck = 0;
		pmove->view_ofs[2] = VEC_VIEW[2];
		pmove->flDuckTime = 0;

		VectorCopy(newOrigin, pmove->origin);

		// Recatagorize position since ducking can change origin
		PM_CatagorizePosition();
	}
}

// orig 0xa55e0, orig client 0xb7b30. RC2: no ducking when shrunk or in a movement mode
void PM_Duck()
{
	int i;
	float time;
	float duckFraction;

	int buttonsChanged = (pmove->oldbuttons ^ pmove->cmd.buttons); // These buttons have changed this frame
	int nButtonPressed = buttonsChanged & pmove->cmd.buttons;	   // The changed ones still down are "pressed"

	if ((pmove->cmd.buttons & IN_DUCK) != 0)
	{
		pmove->oldbuttons |= IN_DUCK;
	}
	else
	{
		pmove->oldbuttons &= ~IN_DUCK;
	}

	// Prevent ducking if the iuser3 variable is set
	if (0 != pmove->iuser3 || 0 != pmove->dead || 0 != g_bPMShrunk || g_iPMMode != PM_MODE_NORMAL)
	{
		// Try to unduck
		if ((pmove->flags & FL_DUCKING) != 0)
		{
			PM_UnDuck();
		}
		return;
	}

	if ((pmove->flags & FL_DUCKING) != 0)
	{
		pmove->cmd.forwardmove *= PLAYER_DUCKING_MULTIPLIER;
		pmove->cmd.sidemove *= PLAYER_DUCKING_MULTIPLIER;
		pmove->cmd.upmove *= PLAYER_DUCKING_MULTIPLIER;
	}

	if ((pmove->cmd.buttons & IN_DUCK) != 0 || (0 != pmove->bInDuck) || (pmove->flags & FL_DUCKING) != 0)
	{
		if ((pmove->cmd.buttons & IN_DUCK) != 0)
		{
			if ((nButtonPressed & IN_DUCK) != 0 && (pmove->flags & FL_DUCKING) == 0)
			{
				// Use 1 second so super long jump will work
				pmove->flDuckTime = 1000;
				pmove->bInDuck = 1;
			}

			time = V_max(0.0, (1.0 - (float)pmove->flDuckTime / 1000.0));

			if (0 != pmove->bInDuck)
			{
				// Finish ducking immediately if duck time is over or not on ground
				if (((float)pmove->flDuckTime / 1000.0 <= (1.0 - TIME_TO_DUCK)) ||
					(pmove->onground == -1))
				{
					pmove->usehull = 1;
					pmove->view_ofs[2] = VEC_DUCK_VIEW[2];
					pmove->flags |= FL_DUCKING;
					pmove->bInDuck = 0;

					// HACKHACK - Fudge for collision bug - no time to fix this properly
					if (pmove->onground != -1)
					{
						for (i = 0; i < 3; i++)
						{
							pmove->origin[i] -= (pmove->player_mins[1][i] - pmove->player_mins[0][i]);
						}
						// See if we are stuck?
						PM_FixPlayerCrouchStuck(STUCK_MOVEUP);

						// Recatagorize position since ducking can change origin
						PM_CatagorizePosition();
					}
				}
				else
				{
					float fMore = (VEC_DUCK_HULL_MIN[2] - VEC_HULL_MIN[2]);

					// Calc parametric time
					duckFraction = PM_SplineFraction(time, (1.0 / TIME_TO_DUCK));
					pmove->view_ofs[2] = ((VEC_DUCK_VIEW[2] - fMore) * duckFraction) + (VEC_VIEW[2] * (1 - duckFraction));
				}
			}
		}
		else
		{
			// Try to unduck
			PM_UnDuck();
		}
	}
}

// orig 0xa5840, orig client 0xb7e30. RC2: climb speed from the speed attribute or the movement mode, jump-off from
// the jump attribute. halflife-updated's maxspeed cap and ducked climb speed dropped (SDK 2.3
// and RC2 have neither)
void PM_LadderMove(physent_t* pLadder)
{
	Vector ladderCenter;
	trace_t trace;
	Vector floor;
	Vector modelmins, modelmaxs;

	if (pmove->movetype == MOVETYPE_NOCLIP)
		return;

	const float flSpeedScale = PM_GetAttributeScale(1, g_iPMPlayerEnt);
	const float flJumpScale = PM_GetAttributeScale(0, g_iPMPlayerEnt);

	pmove->PM_GetModelBounds(pLadder->model, modelmins, modelmaxs);

	VectorAdd(modelmins, modelmaxs, ladderCenter);
	VectorScale(ladderCenter, 0.5, ladderCenter);

	pmove->movetype = MOVETYPE_FLY;

	// On ladder, convert movement to be relative to the ladder

	VectorCopy(pmove->origin, floor);
	floor[2] += pmove->player_mins[pmove->usehull][2] - 1;

	const bool onFloor = pmove->PM_PointContents(floor, NULL) == CONTENTS_SOLID;

	pmove->gravity = 0;
	PM_TraceModel(pLadder, pmove->origin, ladderCenter, &trace);
	if (trace.fraction != 1.0)
	{
		float forward = 0, right = 0;
		Vector vpn, v_right;
		float flSpeed;

		AngleVectors(pmove->angles, &vpn, &v_right, NULL);

		if (g_iPMMode == PM_MODE_NORMAL)
		{
			flSpeed = flSpeedScale * MAX_CLIMB_SPEED;
		}
		else if (g_iPMMode == PM_MODE_ASSASSIN)
		{
			flSpeed = RC2Pref(RC2P_TransformAssassin_LadderSpeed).f;
		}
		else if (g_iPMMode == PM_MODE_HOUNDEYE)
		{
			flSpeed = RC2Pref(RC2P_TransformHoundeye_LadderSpeed).f;
		}
		else if (g_iPMMode == PM_MODE_HEADCRAB)
		{
			flSpeed = RC2Pref(RC2P_TransformHeadcrab_LadderSpeed).f;
		}
		else
		{
			// Original bug, kept: other modes climb at the jump attribute (about 1 unit/s)
			flSpeed = flJumpScale;
		}

		if ((pmove->cmd.buttons & IN_BACK) != 0)
		{
			forward -= flSpeed;
		}
		if ((pmove->cmd.buttons & IN_FORWARD) != 0)
		{
			forward += flSpeed;
		}
		if ((pmove->cmd.buttons & IN_MOVELEFT) != 0)
		{
			right -= flSpeed;
		}
		if ((pmove->cmd.buttons & IN_MOVERIGHT) != 0)
		{
			right += flSpeed;
		}

		if ((pmove->cmd.buttons & IN_JUMP) != 0)
		{
			pmove->movetype = MOVETYPE_WALK;
			VectorScale(trace.plane.normal, flJumpScale * 270, pmove->velocity);
		}
		else
		{
			if (forward != 0 || right != 0)
			{
				Vector velocity, perp, cross, lateral, tmp;
				float normal;

				//ALERT(at_console, "pev %.2f %.2f %.2f - ",
				//	pev->velocity.x, pev->velocity.y, pev->velocity.z);
				// Calculate player's intended velocity
				//Vector velocity = (forward * gpGlobals->v_forward) + (right * gpGlobals->v_right);
				VectorScale(vpn, forward, velocity);
				VectorMA(velocity, right, v_right, velocity);


				// Perpendicular in the ladder plane
				//					Vector perp = CrossProduct( Vector(0,0,1), trace.vecPlaneNormal );
				//					perp = perp.Normalize();
				VectorClear(tmp);
				tmp[2] = 1;
				CrossProduct(tmp, trace.plane.normal, perp);
				VectorNormalize(perp);


				// decompose velocity into ladder plane
				normal = DotProduct(velocity, trace.plane.normal);
				// This is the velocity into the face of the ladder
				VectorScale(trace.plane.normal, normal, cross);


				// This is the player's additional velocity
				VectorSubtract(velocity, cross, lateral);

				// This turns the velocity into the face of the ladder into velocity that
				// is roughly vertically perpendicular to the face of the ladder.
				// NOTE: It IS possible to face up and move down or face down and move up
				// because the velocity is a sum of the directional velocity and the converted
				// velocity through the face of the ladder -- by design.
				CrossProduct(trace.plane.normal, perp, tmp);
				VectorMA(lateral, -normal, tmp, pmove->velocity);
				if (onFloor && normal > 0) // On ground moving away from the ladder
				{
					VectorMA(pmove->velocity, flSpeedScale * MAX_CLIMB_SPEED, trace.plane.normal, pmove->velocity);
				}
				//pev->velocity = lateral - (CrossProduct( trace.vecPlaneNormal, perp ) * normal);
			}
			else
			{
				VectorClear(pmove->velocity);
			}
		}
	}
}

physent_t* PM_Ladder()
{
	int i;
	physent_t* pe;
	hull_t* hull;
	int num;
	Vector test;

	for (i = 0; i < pmove->nummoveent; i++)
	{
		pe = &pmove->moveents[i];

		if (pe->model && (modtype_t)pmove->PM_GetModelType(pe->model) == mod_brush && pe->skin == CONTENTS_LADDER)
		{

			hull = (hull_t*)pmove->PM_HullForBsp(pe, test);
			num = hull->firstclipnode;

			// Offset the test point appropriately for this hull.
			VectorSubtract(pmove->origin, test, test);

			// Test the player's hull for intersection with this model
			if (pmove->PM_HullPointContents(hull, num, test) == CONTENTS_EMPTY)
				continue;

			return pe;
		}
	}

	return NULL;
}



void PM_WaterJump()
{
	if (pmove->waterjumptime > 10000)
	{
		pmove->waterjumptime = 10000;
	}

	if (0 == pmove->waterjumptime)
		return;

	pmove->waterjumptime -= pmove->cmd.msec;
	if (pmove->waterjumptime < 0 ||
		0 == pmove->waterlevel)
	{
		pmove->waterjumptime = 0;
		pmove->flags &= ~FL_WATERJUMP;
	}

	pmove->velocity[0] = pmove->movedir[0];
	pmove->velocity[1] = pmove->movedir[1];
}

/*
============
PM_AddGravity

============
*/
void PM_AddGravity()
{
	float ent_gravity;

	if (0 != pmove->gravity)
		ent_gravity = pmove->gravity;
	else
		ent_gravity = 1.0;

	// Add gravity incorrectly
	pmove->velocity[2] -= (ent_gravity * pmove->movevars->gravity * pmove->frametime);
	pmove->velocity[2] += pmove->basevelocity[2] * pmove->frametime;
	pmove->basevelocity[2] = 0;
	PM_CheckVelocity();
}
/*
============
PM_PushEntity

Does not change the entities velocity at all
============
*/
pmtrace_t PM_PushEntity(Vector push)
{
	pmtrace_t trace;
	Vector end;

	VectorAdd(pmove->origin, push, end);

	trace = pmove->PM_PlayerTrace(pmove->origin, end, PM_NORMAL, -1);

	VectorCopy(trace.endpos, pmove->origin);

	// So we can run impact function afterwards.
	if (trace.fraction < 1.0 &&
		0 == trace.allsolid)
	{
		PM_AddToTouched(trace, pmove->velocity);
	}

	return trace;
}

/*
============
PM_Physics_Toss()

Dead player flying through air., e.g.
============
*/
// orig 0xa5f50. Not byte-identical to SDK 2.3's, but it reads the same
void PM_Physics_Toss()
{
	pmtrace_t trace;
	Vector move;
	float backoff;

	PM_CheckWater();

	if (pmove->velocity[2] > 0)
		pmove->onground = -1;

	// If on ground and not moving, return.
	if (pmove->onground != -1)
	{
		if (VectorCompare(pmove->basevelocity, vec3_origin) &&
			VectorCompare(pmove->velocity, vec3_origin))
			return;
	}

	PM_CheckVelocity();

	// add gravity
	if (pmove->movetype != MOVETYPE_FLY &&
		pmove->movetype != MOVETYPE_BOUNCEMISSILE &&
		pmove->movetype != MOVETYPE_FLYMISSILE)
		PM_AddGravity();

	// move origin
	// Base velocity is not properly accounted for since this entity will move again after the bounce without
	// taking it into account
	VectorAdd(pmove->velocity, pmove->basevelocity, pmove->velocity);

	PM_CheckVelocity();
	VectorScale(pmove->velocity, pmove->frametime, move);
	VectorSubtract(pmove->velocity, pmove->basevelocity, pmove->velocity);

	trace = PM_PushEntity(move); // Should this clear basevelocity

	PM_CheckVelocity();

	if (0 != trace.allsolid)
	{
		// entity is trapped in another solid
		pmove->onground = trace.ent;
		VectorCopy(vec3_origin, pmove->velocity);
		return;
	}

	if (trace.fraction == 1)
	{
		PM_CheckWater();
		return;
	}


	if (pmove->movetype == MOVETYPE_BOUNCE)
		backoff = 2.0 - pmove->friction;
	else if (pmove->movetype == MOVETYPE_BOUNCEMISSILE)
		backoff = 2.0;
	else
		backoff = 1;

	PM_ClipVelocity(pmove->velocity, trace.plane.normal, pmove->velocity, backoff);

	// stop if on ground
	if (trace.plane.normal[2] > 0.7)
	{
		float vel;
		Vector base;

		VectorClear(base);
		if (pmove->velocity[2] < pmove->movevars->gravity * pmove->frametime)
		{
			// we're rolling on the ground, add static friction.
			pmove->onground = trace.ent;
			pmove->velocity[2] = 0;
		}

		vel = DotProduct(pmove->velocity, pmove->velocity);

		// Con_DPrintf("%f %f: %.0f %.0f %.0f\n", vel, trace.fraction, ent->velocity[0], ent->velocity[1], ent->velocity[2] );

		if (vel < (30 * 30) || (pmove->movetype != MOVETYPE_BOUNCE && pmove->movetype != MOVETYPE_BOUNCEMISSILE))
		{
			pmove->onground = trace.ent;
			VectorCopy(vec3_origin, pmove->velocity);
		}
		else
		{
			VectorScale(pmove->velocity, (1.0 - trace.fraction) * pmove->frametime * 0.9, move);
			trace = PM_PushEntity(move);
		}
		VectorSubtract(pmove->velocity, base, pmove->velocity)
	}

	// check for in water
	PM_CheckWater();
}

/*
====================
PM_NoClip

====================
*/
void PM_NoClip()
{
	int i;
	Vector wishvel;
	float fmove, smove;
	//	float		currentspeed, addspeed, accelspeed;

	// Copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;

	VectorNormalize(pmove->forward);
	VectorNormalize(pmove->right);

	for (i = 0; i < 3; i++) // Determine x and y parts of velocity
	{
		wishvel[i] = pmove->forward[i] * fmove + pmove->right[i] * smove;
	}
	wishvel[2] += pmove->cmd.upmove;

	VectorMA(pmove->origin, pmove->frametime, wishvel, pmove->origin);

	// Zero out the velocity so that we don't accumulate a huge downward velocity from
	//  gravity, etc.
	VectorClear(pmove->velocity);
}

// Only allow bunny jumping up to 1.7x server / player maxspeed setting
#define BUNNYJUMP_MAX_SPEED_FACTOR 1.7f

//-----------------------------------------------------------------------------
// Purpose: Corrects bunny jumping ( where player initiates a bunny jump before other
//  movement logic runs, thus making onground == -1 thus making PM_Friction get skipped and
//  running PM_AirMove, which doesn't crop velocity to maxspeed like the ground / other
//  movement logic does.
//-----------------------------------------------------------------------------
void PM_PreventMegaBunnyJumping()
{
	const bool allowBunnyHopping = atoi(pmove->PM_Info_ValueForKey(pmove->physinfo, "bj")) == 1;

	if (allowBunnyHopping)
	{
		return;
	}

	// Current player speed
	float spd;
	// If we have to crop, apply this cropping fraction to velocity
	float fraction;
	// Speed at which bunny jumping is limited
	float maxscaledspeed;

	maxscaledspeed = BUNNYJUMP_MAX_SPEED_FACTOR * pmove->maxspeed;

	// Don't divide by zero
	if (maxscaledspeed <= 0.0f)
		return;

	spd = Length(pmove->velocity);

	if (spd <= maxscaledspeed)
		return;

	fraction = (maxscaledspeed / spd) * 0.65; //Returns the modifier for the velocity

	VectorScale(pmove->velocity, fraction, pmove->velocity); //Crop it down!.
}

/*
=============
PM_Jump
=============
*/
// orig 0xa6380, orig client 0xb8c80. RC2: the bat doesn't jump here; heights from rc2.ini and the jump attribute
// (0.2 when shrunk), the houndeye's own height, the other modes jump in PM_AssassinJump and
// PM_HeadcrabJump; sets the wallhop and jetboost timers. No tfc sounds, and any feigning
// player (deadflag 5) can't jump.
void PM_Jump()
{
	int i;

	if (g_iPMMode == PM_MODE_BAT)
		return;

	float flJumpScale = PM_GetAttributeScale(0, g_iPMPlayerEnt);
	PM_GetAttributeScale(1, g_iPMPlayerEnt); // read and unused
	if (g_iPMMode != PM_MODE_NORMAL)
		flJumpScale = 1.0;

	const float flShrink = 0 != g_bPMShrunk ? 0.2 : 1.0;

	if (0 != pmove->dead)
	{
		pmove->oldbuttons |= IN_JUMP; // don't jump again until released
		return;
	}

	// Spy that's feigning death cannot jump
	if (pmove->deadflag == (DEAD_DISCARDBODY + 1))
	{
		return;
	}

	// See if we are waterjumping.  If so, decrement count and return.
	if (0 != pmove->waterjumptime)
	{
		pmove->waterjumptime -= pmove->cmd.msec;
		if (pmove->waterjumptime < 0)
		{
			pmove->waterjumptime = 0;
		}
		return;
	}

	// If we are in the water most of the way...
	if (pmove->waterlevel >= 2)
	{ // swimming, not jumping
		pmove->onground = -1;

		if (pmove->watertype == CONTENTS_WATER) // We move up a certain amount
			pmove->velocity[2] = flJumpScale * 100;
		else if (pmove->watertype == CONTENTS_SLIME)
			pmove->velocity[2] = flJumpScale * 80;
		else // LAVA
			pmove->velocity[2] = flJumpScale * 50;

		// play swiming sound
		if (pmove->flSwimTime <= 0)
		{
			// Don't play sound again for 1 second
			pmove->flSwimTime = 1000;
			switch (pmove->RandomLong(0, 3))
			{
			case 0:
				pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM, 0, PITCH_NORM);
				break;
			case 1:
				pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade2.wav", 1, ATTN_NORM, 0, PITCH_NORM);
				break;
			case 2:
				pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade3.wav", 1, ATTN_NORM, 0, PITCH_NORM);
				break;
			case 3:
				pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade4.wav", 1, ATTN_NORM, 0, PITCH_NORM);
				break;
			}
		}

		return;
	}

	// No more effect
	if (pmove->onground == -1)
	{
		// Flag that we jumped.
		// HACK HACK HACK
		// Remove this when the game .dll no longer does physics code!!!!
		pmove->oldbuttons |= IN_JUMP; // don't jump again until released
		return;						  // in air, so no effect
	}

	if ((pmove->oldbuttons & IN_JUMP) != 0)
		return; // don't pogo stick

	// In the air now.
	pmove->onground = -1;

	PM_PreventMegaBunnyJumping();

	PM_PlayStepSound(PM_MapTextureTypeStepType(pmove->chtexturetype), 1.0);

	if (g_iPMMode == PM_MODE_NORMAL)
	{
		// See if user can super long jump?
		const bool cansuperjump = atoi(pmove->PM_Info_ValueForKey(pmove->physinfo, "slj")) == 1;
		int iWallhopDelay = 50;

		// Acclerate upward
		// If we are ducking...
		if ((0 != pmove->bInDuck) || (pmove->flags & FL_DUCKING) != 0)
		{
			// Adjust for super long jump module
			// UNDONE -- note this should be based on forward angles, not current velocity.
			if (cansuperjump &&
				(pmove->cmd.buttons & IN_DUCK) != 0 &&
				(pmove->flDuckTime > 0) &&
				Length(pmove->velocity) > 50)
			{
				pmove->punchangle[0] = -5;

				for (i = 0; i < 2; i++)
				{
					pmove->velocity[i] = RC2Pref(RC2P_Attributes_BasePhysicsLongJumpForwardVel).f * pmove->forward[i] * flJumpScale;
				}

				iWallhopDelay = 250;
				pmove->velocity[2] = sqrt(RC2Pref(RC2P_Attributes_BasePhysicsLongJumpHeight).f * flShrink * 1600.0) * flJumpScale;
			}
			else
			{
				pmove->velocity[2] = sqrt(RC2Pref(RC2P_Attributes_BasePhysicsJumpHeight).f * flShrink * 1600.0) * flJumpScale;
			}
		}
		else
		{
			pmove->velocity[2] = sqrt(RC2Pref(RC2P_Attributes_BasePhysicsJumpHeight).f * flShrink * 1600.0) * flJumpScale;
		}

		PM_SetPlayerTimer(g_iPMPlayerEnt, 0, iWallhopDelay);
	}
	else if (g_iPMMode == PM_MODE_HOUNDEYE)
	{
		pmove->velocity[2] = sqrt(RC2Pref(RC2P_TransformHoundeye_JumpHeight).f * flShrink * 1600.0);
	}

	if (PM_GetPlayerTimer(g_iPMPlayerEnt, 1) < 50)
		PM_SetPlayerTimer(g_iPMPlayerEnt, 1, 50);

	// Decay it for simulation
	PM_FixupGravityVelocity();

	// Flag that we jumped.
	pmove->oldbuttons |= IN_JUMP; // don't jump again until released
}

/*
=============
Rocket Crowbar 2: the assassin's and the headcrab's charged jumps, on releasing jump. The
charge is player timer 2 (counted up in PM_PlayerMove while jump is held, negative while
the re-jump delay runs). Names ours.
=============
*/
// the water "jump" both have: up by the water type, with the swim sound
static void PM_ChargedJumpSwim(float flWater, float flSlime, float flLava) // a helper, no original
{
	pmove->onground = -1;

	if (pmove->watertype == CONTENTS_WATER)
		pmove->velocity[2] = flWater;
	else if (pmove->watertype == CONTENTS_SLIME)
		pmove->velocity[2] = flSlime;
	else
		pmove->velocity[2] = flLava;

	if (pmove->flSwimTime <= 0)
	{
		pmove->flSwimTime = 1000;
		switch (pmove->RandomLong(0, 3))
		{
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade2.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade3.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade4.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
	}
}

void PM_AssassinJump() // orig 0xa67b0, orig client 0xb91c0, name ours
{
#ifdef CLIENT_DLL
	// the client keeps the charge of the move that ran the functions for the replays
	int iCharge = PM_GetPlayerTimer(g_iPMPlayerEnt, 2);
	if (0 != pmove->runfuncs)
		s_iAssassinCharge = iCharge;
	else
		iCharge = s_iAssassinCharge;
#else
	const int iCharge = PM_GetPlayerTimer(g_iPMPlayerEnt, 2);
#endif
	if (iCharge < 0)
		return;

	const float flShrink = 0 != g_bPMShrunk ? 0.2 : 1.0;

	if (0 != pmove->dead)
		return;

	if (pmove->waterlevel >= 2)
	{
		PM_ChargedJumpSwim(150, 100, 80);
		return;
	}

	if (pmove->onground == -1)
		return;

	pmove->onground = -1;

	float flCharge = (float)iCharge / (RC2Pref(RC2P_TransformAssassin_JumpFullChargeTime).f * 1000.0);
	if (flCharge < 0)
		flCharge = 0;
	if (flCharge > 1)
		flCharge = 1;
	pmove->velocity[2] = flCharge;
	pmove->velocity[2] = RC2Pref(RC2P_TransformAssassin_JumpMaxAddVel).f * pmove->velocity[2];
	pmove->velocity[2] = RC2Pref(RC2P_TransformAssassin_JumpBaseVel).f + pmove->velocity[2];
	pmove->velocity[2] = flShrink * pmove->velocity[2];

	PM_PlayStepSound(PM_MapTextureTypeStepType(pmove->chtexturetype), 1.0);

	if (pmove->cmd.forwardmove > 0)
	{
		Vector vecFlat(pmove->forward[0], pmove->forward[1], 0);
		VectorNormalize(vecFlat);
		VectorMA(pmove->velocity, RC2Pref(RC2P_TransformAssassin_JumpForwardVelBoost).f * flShrink, vecFlat, pmove->velocity);
	}

	PM_SetPlayerTimer(g_iPMPlayerEnt, 2, (int)(RC2Pref(RC2P_TransformAssassin_JumpMinRejumpTime).f * -1000.0));
	PM_SetPlayerTimer(g_iPMPlayerEnt, 0, 100);
	PM_FixupGravityVelocity();
}

void PM_HeadcrabJump() // orig 0xa6a90, orig client 0xb9570, name ours
{
#ifdef CLIENT_DLL
	// the client keeps the charge of the move that ran the functions for the replays
	int iCharge = PM_GetPlayerTimer(g_iPMPlayerEnt, 2);
	if (0 != pmove->runfuncs)
		s_iHeadcrabCharge = iCharge;
	else
		iCharge = s_iHeadcrabCharge;
#else
	const int iCharge = PM_GetPlayerTimer(g_iPMPlayerEnt, 2);
#endif
	if (iCharge < 0)
		return;

	const float flShrink = 0 != g_bPMShrunk ? 0.2 : 1.0;

	if (0 != pmove->dead)
		return;

	if (pmove->waterlevel > 1)
	{
		pmove->onground = -1;
		return;
	}

	if (pmove->onground == -1)
		return;

	switch (pmove->RandomLong(0, 1))
	{
	case 0:
		pmove->PM_PlaySound(CHAN_VOICE, "headcrab/hc_attack1.wav", 1, ATTN_NORM, 0, pmove->RandomLong(95, 105));
		break;
	case 1:
		pmove->PM_PlaySound(CHAN_VOICE, "headcrab/hc_attack2.wav", 1, ATTN_NORM, 0, pmove->RandomLong(95, 105));
		break;
	}

	PM_SetHeadcrabLeap(g_iPMPlayerEnt, 1);
	pmove->onground = -1;

	// not clamped, unlike the assassin's
	const float flVel = ((float)iCharge / (RC2Pref(RC2P_TransformHeadcrab_JumpFullChargeTime).f * 1000.0)) * RC2Pref(RC2P_TransformHeadcrab_JumpMaxAddVel).f + RC2Pref(RC2P_TransformHeadcrab_JumpBaseVel).f;

	PM_PlayStepSound(PM_MapTextureTypeStepType(pmove->chtexturetype), 1.0);

	Vector vecDir = pmove->forward;
	if (vecDir[2] < 0.2)
	{
		vecDir[2] = 0.2;
		VectorNormalize(vecDir);
	}
	VectorMA(pmove->velocity, flVel * flShrink, vecDir, pmove->velocity);

	PM_SetPlayerTimer(g_iPMPlayerEnt, 2, (int)(RC2Pref(RC2P_TransformHeadcrab_JumpMinRejumpTime).f * -1000.0));
	PM_SetPlayerTimer(g_iPMPlayerEnt, 0, 100);

	// Original oddity, kept: past maxvelocity it is scaled down to the movevars' maxspeed
	const float flSpeed = Length(pmove->velocity);
	if (flSpeed != 0 && flSpeed > pmove->movevars->maxvelocity)
		VectorScale(pmove->velocity, pmove->movevars->maxspeed / flSpeed, pmove->velocity);

	PM_FixupGravityVelocity();
}

/*
=============
PM_CheckWaterJump
=============
*/
#define WJ_HEIGHT 8
void PM_CheckWaterJump()
{
	Vector vecStart, vecEnd;
	Vector flatforward;
	Vector flatvelocity;
	float curspeed;
	pmtrace_t tr;
	int savehull;

	// Already water jumping.
	if (0 != pmove->waterjumptime)
		return;

	// Don't hop out if we just jumped in
	if (pmove->velocity[2] < -180)
		return; // only hop out if we are moving up

	// See if we are backing up
	flatvelocity[0] = pmove->velocity[0];
	flatvelocity[1] = pmove->velocity[1];
	flatvelocity[2] = 0;

	// Must be moving
	curspeed = VectorNormalize(flatvelocity);

	// see if near an edge
	flatforward[0] = pmove->forward[0];
	flatforward[1] = pmove->forward[1];
	flatforward[2] = 0;
	VectorNormalize(flatforward);

	// Are we backing into water from steps or something?  If so, don't pop forward
	if (curspeed != 0.0 && (DotProduct(flatvelocity, flatforward) < 0.0))
		return;

	VectorCopy(pmove->origin, vecStart);
	vecStart[2] += WJ_HEIGHT;

	VectorMA(vecStart, 24, flatforward, vecEnd);

	// Trace, this trace should use the point sized collision hull
	savehull = pmove->usehull;
	pmove->usehull = 2;
	tr = pmove->PM_PlayerTrace(vecStart, vecEnd, PM_NORMAL, -1);
	if (tr.fraction < 1.0 && fabs(tr.plane.normal[2]) < 0.1f) // Facing a near vertical wall?
	{
		vecStart[2] += pmove->player_maxs[savehull][2] - WJ_HEIGHT;
		VectorMA(vecStart, 24, flatforward, vecEnd);
		VectorMA(vec3_origin, -50, tr.plane.normal, pmove->movedir);

		tr = pmove->PM_PlayerTrace(vecStart, vecEnd, PM_NORMAL, -1);
		if (tr.fraction == 1.0)
		{
			pmove->waterjumptime = 2000;
			pmove->velocity[2] = 225;
			pmove->oldbuttons |= IN_JUMP;
			pmove->flags |= FL_WATERJUMP;
		}
	}

	// Reset the collision hull
	pmove->usehull = savehull;
}

// orig 0xa6f30, orig client 0xb9b70. RC2: no fall pain sounds, and no screen punch for the assassin and headcrab
void PM_CheckFalling()
{
	if (pmove->onground != -1 &&
		0 == pmove->dead &&
		pmove->flFallVelocity >= PLAYER_FALL_PUNCH_THRESHHOLD)
	{
		float fvol = 0.5;

		if (pmove->waterlevel > 0)
		{
		}
		else if (pmove->flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED)
		{
			fvol = 1.0;
		}
		else if (pmove->flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED / 2)
		{
			fvol = 0.85;
		}
		else if (pmove->flFallVelocity < PLAYER_MIN_BOUNCE_SPEED)
		{
			fvol = 0;
		}

		if (fvol > 0.0)
		{
			// Play landing step right away
			pmove->flTimeStepSound = 0;

			PM_UpdateStepSound();

			// play step sound for current texture
			PM_PlayStepSound(PM_MapTextureTypeStepType(pmove->chtexturetype), fvol);

			if (g_iPMMode != PM_MODE_ASSASSIN && g_iPMMode != PM_MODE_HEADCRAB)
			{
				// Knock the screen around a little bit, temporary effect
				pmove->punchangle[2] = pmove->flFallVelocity * 0.013; // punch z axis

				if (pmove->punchangle[0] > 8)
				{
					pmove->punchangle[0] = 8;
				}
			}
		}
	}

	if (pmove->onground != -1)
	{
		pmove->flFallVelocity = 0;
	}
}

/*
=================
Rocket Crowbar 2: what pulls the player: its 64 tethers, a tractor pull point (the egon's
tractor), and 64 global pull points. Returns whether anything pulled. Name ours.
=================
*/
static void PM_PullLiftOff() // a helper, no original
{
	if (pmove->onground != -1)
	{
		pmove->onground = -1;
		pmove->origin[2] += 2.0;
	}
}

bool PM_ApplyPullForces() // orig 0xa7080, orig client 0xb9d20, name ours
{
	bool bPulled = false;
	Vector vecPull = g_vecZero;
	Vector vecOrg = pmove->origin;
	Vector vecPoint, vecDir;
	float flStrength, flDist, flLen;
	int i;

	if (0 != g_bPMShrunk)
		vecOrg[2] += 7.2;

	for (i = 0; i < 64; i++)
	{
		if (PM_GetPlayerTether(g_iPMPlayerEnt, i, vecPoint, &flStrength, &flDist) && flStrength != 0)
		{
			vecDir = vecPoint - vecOrg;
			flLen = VectorNormalize(vecDir);
			if (flLen > flDist && flLen != 0)
				VectorMA(vecPull, (flLen - flDist) * pmove->frametime * flStrength, vecDir, vecPull);
		}
	}

	if (vecPull != g_vecZero)
	{
		bPulled = true;
		VectorAdd(pmove->velocity, vecPull, pmove->velocity);

		if (vecPull[2] > pmove->frametime * 150.0)
			PM_PullLiftOff();

		float flDrag;
		if (pmove->waterlevel == 2)
			flDrag = 0.99;
		else if (pmove->onground == -1)
			flDrag = 0.33;
		else
			flDrag = 1.0;

		if (flDrag != 1.0)
		{
			Vector vecDrag = pmove->velocity;
			flLen = VectorNormalize(vecDrag);
			VectorScale(vecDrag, flDrag * pmove->frametime * flLen, vecDrag);
			VectorSubtract(pmove->velocity, vecDrag, pmove->velocity);
		}
	}

	Vector vecTractor;
	if (PM_GetPlayerPullVec(g_iPMPlayerEnt, vecTractor) && vecTractor[0] != 0 && vecTractor[1] != 0 && vecTractor[2] != 0 && pmove->frametime > 0)
	{
		vecDir = vecTractor - pmove->origin;
		flLen = VectorNormalize(vecDir);
		const float flDropDist = RC2Pref(RC2P_Egon_TractorDropDist).f;
		if (flLen <= flDropDist)
		{
			bPulled = true;
			VectorMA(pmove->velocity, ((flLen / flDropDist) * 0.95 + 0.05) * pmove->frametime * RC2Pref(RC2P_Egon_TractorMaxAccel).f, vecDir, pmove->velocity);
			if (pmove->velocity[2] > pmove->frametime * 800.0)
				PM_PullLiftOff();
		}
		else
		{
			PM_ClearPlayerPull(g_iPMPlayerEnt);
		}
	}

	for (i = 0; i < 64; i++)
	{
		// writes into vecOrg as the original does (its player-origin copy is done with)
		PM_GetGlobalPuller(i, vecOrg, &flDist, &flStrength);
		if (vecOrg != g_vecZero)
		{
			vecDir = vecOrg - pmove->origin;
			flLen = Length(vecDir);
			if (flLen < flDist)
			{
				bPulled = true;
				if (flLen >= 32.0)
				{
					VectorScale(vecDir, (flStrength * pmove->frametime) / (flLen * flLen), vecDir);
					VectorAdd(pmove->velocity, vecDir, pmove->velocity);
					if (vecDir[2] > pmove->frametime * 500.0)
						PM_PullLiftOff();
				}
				else
				{
					if (pmove->frametime != 0)
						VectorScale(vecDir, 1.0 / pmove->frametime, pmove->velocity);
					if (pmove->velocity[2] > 0)
						PM_PullLiftOff();
				}
			}
		}
	}

	return bPulled;
}

/*
=================
PM_PlayWaterSounds

=================
*/
void PM_PlayWaterSounds()
{
	// Did we enter or leave water?
	if ((pmove->oldwaterlevel == 0 && pmove->waterlevel != 0) ||
		(pmove->oldwaterlevel != 0 && pmove->waterlevel == 0))
	{
		switch (pmove->RandomLong(0, 3))
		{
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade2.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade3.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade4.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
	}
}

/*
===============
PM_CalcRoll

===============
*/
float PM_CalcRoll(Vector angles, Vector velocity, float rollangle, float rollspeed)
{
	float sign;
	float side;
	float value;
	Vector forward, right, up;

	AngleVectors(angles, &forward, &right, &up);

	side = DotProduct(velocity, right);

	sign = side < 0 ? -1 : 1;

	side = fabs(side);

	value = rollangle;

	if (side < rollspeed)
	{
		side = side * value / rollspeed;
	}
	else
	{
		side = value;
	}

	return side * sign;
}

/*
=============
PM_DropPunchAngle

=============
*/
// orig 0xa7760, orig client 0xba5d0. Not byte-identical to SDK 2.3's, but it reads the same
void PM_DropPunchAngle(Vector& punchangle)
{
	float len;

	len = VectorNormalize(punchangle);
	len -= (10.0 + len * 0.5) * pmove->frametime;
	len = V_max(len, 0.0f);
	VectorScale(punchangle, len, punchangle);
}

/*
==============
PM_CheckParamters

==============
*/
// orig 0xa77b0, orig client 0xba640. RC2: the assassin, houndeye and headcrab run at their own speed; dead view
// height set as SDK 2.3 does (z only). halflife-updated's slow-down while +use on the ground
// is not RC2's and is dropped
void PM_CheckParamters()
{
	float spd;
	float maxspeed;
	Vector v_angle;

	spd = (pmove->cmd.forwardmove * pmove->cmd.forwardmove) +
		  (pmove->cmd.sidemove * pmove->cmd.sidemove) +
		  (pmove->cmd.upmove * pmove->cmd.upmove);
	spd = sqrt(spd);

	maxspeed = pmove->clientmaxspeed; //atof( pmove->PM_Info_ValueForKey( pmove->physinfo, "maxspd" ) );
	if (maxspeed != 0.0)
	{
		pmove->maxspeed = V_min(maxspeed, pmove->maxspeed);
	}

	switch (g_iPMMode)
	{
	case PM_MODE_ASSASSIN:
		pmove->maxspeed = RC2Pref(RC2P_TransformAssassin_RunSpeed).f;
		break;
	case PM_MODE_HOUNDEYE:
		pmove->maxspeed = RC2Pref(RC2P_TransformHoundeye_RunSpeed).f;
		break;
	case PM_MODE_HEADCRAB:
		pmove->maxspeed = RC2Pref(RC2P_TransformHeadcrab_RunSpeed).f;
		break;
	}

	if ((spd != 0.0) &&
		(spd > pmove->maxspeed))
	{
		float fRatio = pmove->maxspeed / spd;
		pmove->cmd.forwardmove *= fRatio;
		pmove->cmd.sidemove *= fRatio;
		pmove->cmd.upmove *= fRatio;
	}

	if ((pmove->flags & FL_FROZEN) != 0 ||
		(pmove->flags & FL_ONTRAIN) != 0 ||
		0 != pmove->dead)
	{
		pmove->cmd.forwardmove = 0;
		pmove->cmd.sidemove = 0;
		pmove->cmd.upmove = 0;
	}


	PM_DropPunchAngle(pmove->punchangle);

	// Take angles from command.
	if (0 == pmove->dead)
	{
		VectorCopy(pmove->cmd.viewangles, v_angle);
		VectorAdd(v_angle, pmove->punchangle, v_angle);

		// Set up view angles.
		pmove->angles[ROLL] = PM_CalcRoll(v_angle, pmove->velocity, pmove->movevars->rollangle, pmove->movevars->rollspeed) * 4;
		pmove->angles[PITCH] = v_angle[PITCH];
		pmove->angles[YAW] = v_angle[YAW];
	}
	else
	{
		VectorCopy(pmove->oldangles, pmove->angles);
	}

	// Set dead player view_offset
	if (0 != pmove->dead)
	{
		pmove->view_ofs[2] = VEC_DEAD_VIEW[2];
	}

	// Adjust client view angles to match values used on server.
	if (pmove->angles[YAW] > 180.0f)
	{
		pmove->angles[YAW] -= 360.0f;
	}
}

// orig 0xa7a20, orig client 0xba950. RC2: a negative duck time (the transforms' refire wait) counts up to 0, and
// player timers 0 and 1 count down while jump isn't held
void PM_ReduceTimers()
{
	if (pmove->flTimeStepSound > 0)
	{
		pmove->flTimeStepSound -= pmove->cmd.msec;
		if (pmove->flTimeStepSound < 0)
		{
			pmove->flTimeStepSound = 0;
		}
	}
	if (pmove->flDuckTime > 0)
	{
		pmove->flDuckTime -= pmove->cmd.msec;
		if (pmove->flDuckTime < 0)
		{
			pmove->flDuckTime = 0;
		}
	}
	else if (g_iPMMode != PM_MODE_NORMAL && pmove->flDuckTime < 0)
	{
		pmove->flDuckTime += pmove->cmd.msec;
		if (pmove->flDuckTime > 0)
		{
			pmove->flDuckTime = 0;
		}
	}
	if (pmove->flSwimTime > 0)
	{
		pmove->flSwimTime -= pmove->cmd.msec;
		if (pmove->flSwimTime < 0)
		{
			pmove->flSwimTime = 0;
		}
	}

	for (int iSlot = 0; iSlot < 2; iSlot++)
	{
		int iTimer = PM_GetPlayerTimer(g_iPMPlayerEnt, iSlot);
		if (iTimer > 0 && (pmove->cmd.buttons & IN_JUMP) == 0)
		{
			iTimer -= pmove->cmd.msec;
			if (iTimer < 1)
				iTimer = 0;
			PM_SetPlayerTimer(g_iPMPlayerEnt, iSlot, iTimer);
		}
	}
}

/*
=============
Rocket Crowbar 2: the bat's own move. In flight it flies (PM_BatMove); perched it sits at its
perch (a point, or an offset in a physent's frame) until something pulls it off. Returns
whether the move is done. Name ours.
=============
*/
bool PM_BatPerchMove() // orig 0xa7c00, orig client 0xbabb0, name ours
{
	int iAttach;
	Vector vecOffset;

	if ((g_iPMFlags & (PMF_NOINPUT | PMF_HELD)) != 0)
		return false;

	const int iState = PM_GetBatPerch(g_iPMPlayerEnt, &iAttach, vecOffset);
	if (iState == 0)
	{
		PM_BatMove();
		return true;
	}

	if (iState != 2)
		return false;

	if (PM_ApplyPullForces())
	{
#ifndef CLIENT_DLL
		PM_BatUnperch(g_iPMPlayerEnt); // the client only stops predicting the perch
#endif
		return false;
	}

	if (iAttach == 0)
	{
		pmove->origin = vecOffset;
		pmove->velocity = g_vecZero;
		return true;
	}

	int i;
	for (i = 0; i < pmove->numphysent; i++)
	{
		if (pmove->physents[i].info == iAttach)
			break;
	}

	if (i < pmove->numphysent)
	{
		Vector forward, right, up;
		AngleVectors(pmove->physents[i].angles, &forward, &right, &up);

		Vector vecPos = pmove->physents[i].origin;
		VectorMA(vecPos, vecOffset[0], forward, vecPos);
		VectorMA(vecPos, vecOffset[1], right, vecPos);
		VectorMA(vecPos, vecOffset[2], up, vecPos);
		pmove->origin = vecPos;
		pmove->velocity = g_vecZero;
	}

	return true;
}

// Rocket Crowbar 2: the assassin's and the headcrab's charge (player timer 2) and refire
// waits, once per move before the movetype switch. Name ours; inline in the original.
static void PM_ChargeTimers(float flFullChargeTime, bool bAssassin) // a helper, no original
{
	const int iMsec = pmove->cmd.msec;
	const int iFull = (int)(flFullChargeTime * 1000.0);
	int iCharge = PM_GetPlayerTimer(g_iPMPlayerEnt, 2);

	if (pmove->onground == -1)
	{
		// in the air
		if (iCharge > 0)
		{
			if ((pmove->cmd.buttons & IN_JUMP) != 0)
			{
				iCharge += iMsec;
				if (iCharge >= iFull)
					iCharge = iFull;
				PM_SetPlayerTimer(g_iPMPlayerEnt, 2, iCharge);
			}
			else
			{
				PM_SetPlayerTimer(g_iPMPlayerEnt, 2, 0);
			}
		}
		else
		{
			iCharge += iMsec;
			PM_SetPlayerTimer(g_iPMPlayerEnt, 2, iCharge < 0 ? iCharge : 0);
		}
	}
	else
	{
		if (!bAssassin)
			PM_SetHeadcrabLeap(g_iPMPlayerEnt, 0);

		// on the ground
		if (iCharge < 0)
		{
			iCharge += iMsec;
			PM_SetPlayerTimer(g_iPMPlayerEnt, 2, iCharge < 0 ? iCharge : 0);
		}
		else if ((pmove->cmd.buttons & IN_JUMP) != 0)
		{
			iCharge += iMsec;
			if (iCharge >= iFull)
				iCharge = iFull;
			PM_SetPlayerTimer(g_iPMPlayerEnt, 2, iCharge);
		}
		else if ((pmove->oldbuttons & IN_JUMP) == 0 || (g_iPMFlags & PMF_HELD) != 0)
		{
			PM_SetPlayerTimer(g_iPMPlayerEnt, 2, 0);
		}
		// else keep the charge for the jump on release

		if ((pmove->cmd.buttons & IN_JUMP) != 0)
			PM_SetPlayerTimer(g_iPMPlayerEnt, 0, 300);

		if (bAssassin && (pmove->cmd.buttons & IN_ATTACK) != 0)
		{
			// the gun: stand still for the refire time
			pmove->flDuckTime = RC2Pref(RC2P_TransformAssassin_GunRefireOnGround).f * 1000.0 + iMsec + 10.0;
			pmove->cmd.upmove = 0;
			pmove->cmd.sidemove = 0;
			pmove->cmd.forwardmove = 0;
		}
	}

	if (bAssassin && (pmove->cmd.buttons & IN_ATTACK2) != 0)
	{
		// the kick: a negative wait, which also blocks jumping
		pmove->flDuckTime = RC2Pref(RC2P_TransformAssassin_KickRefireKick).f * 1000.0 + iMsec + 10.0;
		pmove->flDuckTime *= -1.0;
		pmove->cmd.upmove = 0;
		pmove->cmd.sidemove = 0;
		pmove->cmd.forwardmove = 0;
		pmove->cmd.buttons &= ~IN_JUMP;
		pmove->oldbuttons &= ~IN_JUMP;
		if (iCharge > 0)
		{
			PM_SetPlayerTimer(g_iPMPlayerEnt, 2, 0);
			iCharge = 0;
		}
	}

	if (pmove->flDuckTime != 0)
	{
		pmove->cmd.upmove = 0;
		pmove->cmd.sidemove = 0;
		pmove->cmd.forwardmove = 0;
		if (pmove->flDuckTime < 0)
		{
			pmove->cmd.buttons &= ~IN_JUMP;
			pmove->oldbuttons &= ~IN_JUMP;
			if (iCharge > 0)
				PM_SetPlayerTimer(g_iPMPlayerEnt, 2, 0);
		}
	}
}

/*
=============
PlayerMove

Returns with origin, angles, and velocity modified in place.

Numtouch and touchindex[] will be set if any of the physents
were contacted during the move.
=============
*/
// orig 0xa7dd0, orig client 0xbae00. RC2: the race mode hands the move over; a rider is carried on the ridden
// player's shoulder; PMF_FROZEN stops everything; the controller and the bat have moves of
// their own; ladders, step sounds and the jump differ per mode; pull forces; jetpack and
// AF grenade physics. halflife-updated's "duck to get unstuck" is not RC2's and is dropped.
void PM_PlayerMove(qboolean server)
{
	physent_t* pLadder = NULL;

	// Are we running server code?
	pmove->server = server;

	// Adjust speeds etc.
	PM_CheckParamters();

	// Assume we don't touch anything
	pmove->numtouch = 0;

	// # of msec to apply movement
	pmove->frametime = pmove->cmd.msec * 0.001;

	if (g_iPMMode == PM_MODE_RACE)
	{
		PM_RaceMove();
		return;
	}

	PM_ReduceTimers();

	// Convert view angles to vectors
	AngleVectors(pmove->angles, &pmove->forward, &pmove->right, &pmove->up);

#ifdef CLIENT_DLL
	// the spectator move sets these again (PM_SpectatorMove); view.cpp reads them
	if (0 != pmove->runfuncs)
	{
		g_iPMSpecView = 0;
		g_bPMSetViewAngles = 0;
		g_bPMSetViewOrigin = 0;
	}
#endif

	// PM_ShowClipBox();

	// Special handling for spectator and observers. (iuser1 is set if the player's in observer mode)
	if (0 != pmove->spectator || pmove->iuser1 > 0)
	{
		PM_SpectatorMove();
		PM_CatagorizePosition();
		return;
	}

	// Rocket Crowbar 2: riding another player's shoulder
	if (0 != g_iPMRideEnt)
	{
		Vector vecOrigin, vecVAngle, vecMins, vecMaxs;
		Vector forward, right, up;

		PM_GetEntityBox(g_iPMRideEnt, vecOrigin, vecVAngle, vecMins, vecMaxs);
		vecVAngle[0] = 0;
		AngleVectors(vecVAngle, &forward, &right, &up);

		VectorMA(vecOrigin, (0 != g_bPMRideLeft ? -1.0 : 1.0) * 7.0, right, pmove->origin);
		pmove->origin[2] += (vecMaxs[2] - vecMins[2] >= 70.0) ? 24.0 : 16.0;

		if (pmove->PM_PointContents(pmove->origin, NULL) == CONTENTS_WATER)
			pmove->waterlevel = 2;
		else
			pmove->waterlevel = 0;

		pmove->flags &= ~FL_DUCKING;
		pmove->bInDuck = 0;
		pmove->flDuckTime = 0;
		pmove->usehull = 2;
		pmove->oldwaterlevel = pmove->waterlevel;
		pmove->onground = -1;
		pmove->velocity = g_vecZero;
		return;
	}

	if ((g_iPMFlags & PMF_FROZEN) != 0)
	{
		pmove->flags &= ~FL_DUCKING;
		pmove->bInDuck = 0;
		pmove->flDuckTime = 0;
		pmove->oldwaterlevel = pmove->waterlevel;
		pmove->velocity = g_vecZero;
		return;
	}

	// Always try and unstick us unless we are in NOCLIP mode
	if (pmove->movetype != MOVETYPE_NOCLIP && pmove->movetype != MOVETYPE_NONE)
	{
		if (PM_CheckStuck())
		{
			return; // Can't move, we're stuck
		}
	}

	if ((g_iPMFlags & (PMF_NOINPUT | PMF_HELD)) != 0)
	{
		// everything but attack
		pmove->cmd.buttons &= ~(IN_JUMP | IN_DUCK | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT);
		pmove->cmd.upmove = 0;
		pmove->cmd.sidemove = 0;
		pmove->cmd.forwardmove = 0;
	}

	// Now that we are "unstuck", see where we are ( waterlevel and type, pmove->onground ).
	PM_CatagorizePosition();

	if (g_iPMMode == PM_MODE_CONTROLLER)
	{
		if (0 == pmove->dead)
		{
			PM_ControllerMove();
			return;
		}
	}
	else if (g_iPMMode == PM_MODE_BAT && 0 == pmove->dead && PM_BatPerchMove())
	{
		return;
	}

	// Store off the starting water level
	pmove->oldwaterlevel = pmove->waterlevel;

	// If we are not on ground, store off how fast we are moving down
	if (pmove->onground == -1)
	{
		pmove->flFallVelocity = -pmove->velocity[2];
	}

	g_onladder = false;
	// Don't run ladder code if dead or on a train
	if (0 == pmove->dead && (pmove->flags & FL_ONTRAIN) == 0 && 0 == g_bPMShrunk &&
		(g_iPMFlags & (PMF_JETPACK | PMF_BOUNCE | PMF_NOLADDER)) == 0 &&
		g_iPMMode != PM_MODE_HOUNDEYE && g_iPMMode != PM_MODE_BAT && g_iPMMode != PM_MODE_HEADCRAB)
	{
		pLadder = PM_Ladder();
		if (pLadder)
		{
			g_onladder = true;
		}
	}

	if ((g_iPMFlags & PMF_JETPACK) == 0)
		PM_UpdateStepSound();

	PM_Duck();

	// Don't run ladder code if dead or on a train
	if (0 == pmove->dead && (pmove->flags & FL_ONTRAIN) == 0)
	{
		if (pLadder)
		{
			PM_LadderMove(pLadder);
		}
		else if (pmove->movetype != MOVETYPE_WALK &&
				 pmove->movetype != MOVETYPE_NOCLIP)
		{
			// Clear ladder stuff unless player is noclipping
			//  it will be set immediately again next frame if necessary
			pmove->movetype = MOVETYPE_WALK;
		}
	}

	// Rocket Crowbar 2: pulled players (and some modes and physics) never climb
	if (PM_ApplyPullForces() && pmove->movetype == MOVETYPE_FLY)
		pmove->movetype = MOVETYPE_WALK;

	if (((g_iPMFlags & (PMF_BOUNCE | PMF_NOLADDER)) != 0 || g_iPMMode == PM_MODE_HOUNDEYE || g_iPMMode == PM_MODE_CONTROLLER ||
			g_iPMMode == PM_MODE_BAT || g_iPMMode == PM_MODE_HEADCRAB) &&
		pmove->movetype == MOVETYPE_FLY)
	{
		pmove->movetype = MOVETYPE_WALK;
	}

	if ((g_iPMFlags & PMF_JETPACK) != 0 && pmove->movetype == MOVETYPE_WALK && pmove->waterlevel < 2)
	{
		PM_JetpackMove();
		return;
	}

	if (g_iPMMode == PM_MODE_ASSASSIN)
		PM_ChargeTimers(RC2Pref(RC2P_TransformAssassin_JumpFullChargeTime).f, true);
	else if (g_iPMMode == PM_MODE_HEADCRAB)
		PM_ChargeTimers(RC2Pref(RC2P_TransformHeadcrab_JumpFullChargeTime).f, false);

	// Handle movement
	switch (pmove->movetype)
	{
	default:
		pmove->Con_DPrintf("Bogus pmove player movetype %i on (%i) 0=cl 1=sv\n", pmove->movetype, pmove->server);
		break;

	case MOVETYPE_NONE:
		break;

	case MOVETYPE_NOCLIP:
		PM_NoClip();
		break;

	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		PM_Physics_Toss();
		break;

	case MOVETYPE_FLY:

		PM_CheckWater();

		// Was jump button pressed?
		// If so, set velocity to 270 away from ladder.  This is currently wrong.
		// Also, set MOVE_TYPE to walk, too.
		if ((pmove->cmd.buttons & IN_JUMP) != 0)
		{
			if (!pLadder)
			{
				PM_Jump();
			}
		}
		else
		{
			pmove->oldbuttons &= ~IN_JUMP;
		}

		// Perform the move accounting for any base velocity.
		VectorAdd(pmove->velocity, pmove->basevelocity, pmove->velocity);
		PM_FlyMove();
		VectorSubtract(pmove->velocity, pmove->basevelocity, pmove->velocity);
		break;

	case MOVETYPE_WALK:
		if (!PM_InWater())
		{
			PM_AddCorrectGravity();
		}

		// Rocket Crowbar 2: the AF grenade's physics: speed up to AFGrenPhysAccVel, kick off
		// in a random direction when (almost) still
		if ((g_iPMFlags & PMF_BOUNCE) != 0)
		{
			const float flSpeed = Length(pmove->velocity);
			if (flSpeed < RC2Pref(RC2P_Grenade_AFGrenPhysAccVel).f)
			{
				if (flSpeed <= RC2Pref(RC2P_Grenade_AFGrenPhysAccAddVel).f)
				{
					pmove->velocity[0] = pmove->RandomFloat(-1.0, 1.0);
					pmove->velocity[1] = pmove->RandomFloat(-1.0, 1.0);
					pmove->velocity[2] = 0;
					VectorScale(pmove->velocity, RC2Pref(RC2P_Grenade_AFGrenPhysAccAddVel).f, pmove->velocity);
				}
				else
				{
					VectorScale(pmove->velocity, RC2Pref(RC2P_Grenade_AFGrenPhysAccRate).f * pmove->frametime + 1.0, pmove->velocity);
				}
			}

			VectorAdd(pmove->velocity, pmove->basevelocity, pmove->velocity);
			PM_FlyMove();
			VectorSubtract(pmove->velocity, pmove->basevelocity, pmove->velocity);

			if (!PM_InWater())
				PM_FixupGravityVelocity();
			PM_CatagorizePosition();
			return;
		}

		// If we are leaping out of the water, just update the counters.
		if (0 != pmove->waterjumptime)
		{
			PM_WaterJump();
			PM_FlyMove();

			// Make sure waterlevel is set correctly
			PM_CheckWater();
			return;
		}

		// If we are swimming in the water, see if we are nudging against a place we can jump up out
		//  of, and, if so, start out jump.  Otherwise, if we are not moving up, then reset jump timer to 0
		if (pmove->waterlevel >= 2)
		{
			if (pmove->waterlevel == 2)
			{
				PM_CheckWaterJump();
			}

			// If we are falling again, then we must not trying to jump out of water any more.
			if (pmove->velocity[2] < 0 && 0 != pmove->waterjumptime)
			{
				pmove->waterjumptime = 0;
			}

			// Was jump button pressed?
			if ((pmove->cmd.buttons & IN_JUMP) != 0)
			{
				PM_Jump();
			}
			else
			{
				pmove->oldbuttons &= ~IN_JUMP;
			}

			// Perform regular water movement
			PM_WaterMove();

			VectorSubtract(pmove->velocity, pmove->basevelocity, pmove->velocity);

			// Get a final position
			PM_CatagorizePosition();
		}
		else

		// Not underwater
		{
			// Was jump button pressed?
			if (g_iPMMode == PM_MODE_ASSASSIN || g_iPMMode == PM_MODE_HEADCRAB)
			{
				// Rocket Crowbar 2: they charge while jump is held and leap on release
				if ((pmove->cmd.buttons & IN_JUMP) != 0)
				{
					pmove->oldbuttons |= IN_JUMP;
				}
				else
				{
					if ((pmove->oldbuttons & IN_JUMP) != 0 && (g_iPMFlags & PMF_HELD) == 0)
					{
						if (g_iPMMode == PM_MODE_ASSASSIN)
							PM_AssassinJump();
						else
							PM_HeadcrabJump();
					}
					pmove->oldbuttons &= ~IN_JUMP;
				}
			}
			else if ((pmove->cmd.buttons & IN_JUMP) != 0)
			{
				if (!pLadder)
				{
					PM_Jump();
				}
			}
			else
			{
				pmove->oldbuttons &= ~IN_JUMP;
			}

			// Fricion is handled before we add in any base velocity. That way, if we are on a conveyor,
			//  we don't slow when standing still, relative to the conveyor.
			if (pmove->onground != -1)
			{
				pmove->velocity[2] = 0.0;
				PM_Friction();
			}

			// Make sure velocity is valid.
			PM_CheckVelocity();

			// Are we on ground now
			if (pmove->onground != -1)
			{
				PM_WalkMove();
			}
			else
			{
				PM_AirMove(); // Take into account movement when in air.
			}

			// Set final flags.
			PM_CatagorizePosition();

			// Now pull the base velocity back out.
			// Base velocity is set if you are on a moving object, like
			//  a conveyor (or maybe another monster?)
			VectorSubtract(pmove->velocity, pmove->basevelocity, pmove->velocity);

			// Make sure velocity is valid.
			PM_CheckVelocity();

			// Add any remaining gravitational component.
			if (!PM_InWater())
			{
				PM_FixupGravityVelocity();
			}

			// If we are on ground, no downward velocity.
			if (pmove->onground != -1)
			{
				pmove->velocity[2] = 0;
			}

			// See if we landed on the ground with enough force to play
			//  a landing sound.
			PM_CheckFalling();
		}

		// Did we enter or leave the water?
		PM_PlayWaterSounds();
		break;
	}
}

// orig 0xa8af0. Not byte-identical to SDK 2.3's, but it reads the same
void PM_CreateStuckTable()
{
	float x, y, z;
	int idx;
	int i;
	float zi[3];

	memset(rgv3tStuckTable, 0, 54 * sizeof(Vector));

	idx = 0;
	// Little Moves.
	x = y = 0;
	// Z moves
	for (z = -0.125; z <= 0.125; z += 0.125)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}
	x = z = 0;
	// Y moves
	for (y = -0.125; y <= 0.125; y += 0.125)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}
	y = z = 0;
	// X moves
	for (x = -0.125; x <= 0.125; x += 0.125)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}

	// Remaining multi axis nudges.
	for (x = -0.125; x <= 0.125; x += 0.250)
	{
		for (y = -0.125; y <= 0.125; y += 0.250)
		{
			for (z = -0.125; z <= 0.125; z += 0.250)
			{
				rgv3tStuckTable[idx][0] = x;
				rgv3tStuckTable[idx][1] = y;
				rgv3tStuckTable[idx][2] = z;
				idx++;
			}
		}
	}

	// Big Moves.
	x = y = 0;
	zi[0] = 0.0f;
	zi[1] = 1.0f;
	zi[2] = 6.0f;

	for (i = 0; i < 3; i++)
	{
		// Z moves
		z = zi[i];
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}

	x = z = 0;

	// Y moves
	for (y = -2.0f; y <= 2.0f; y += 2.0)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}
	y = z = 0;
	// X moves
	for (x = -2.0f; x <= 2.0f; x += 2.0f)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}

	// Remaining multi axis nudges.
	for (i = 0; i < 3; i++)
	{
		z = zi[i];

		for (x = -2.0f; x <= 2.0f; x += 2.0f)
		{
			for (y = -2.0f; y <= 2.0f; y += 2.0)
			{
				rgv3tStuckTable[idx][0] = x;
				rgv3tStuckTable[idx][1] = y;
				rgv3tStuckTable[idx][2] = z;
				idx++;
			}
		}
	}
}



/*
This modume implements the shared player physics code between any particular game and 
the engine.  The same PM_Move routine is built into the game .dll and the client .dll and is
invoked by each side as appropriate.  There should be no distinction, internally, between server
and client.  This will ensure that prediction behaves appropriately.
*/

// Rocket Crowbar 2: the callback PM_Move gives the game's hide/restore hooks (PM_SETENTFUNC)
static void PM_SetEntsByInfo(int iInfo, int iSolid, float* vecOrigin) // orig 0xa8d10, orig client 0xbc140, name ours (in neither files-rc2.tsv nor files-client.tsv: Ghidra missed it); unused by the client
{
	int i;

	for (i = 0; i < pmove->numphysent; i++)
	{
		if (pmove->physents[i].info == iInfo)
		{
			pmove->physents[i].solid = iSolid;
			VectorCopy(vecOrigin, pmove->physents[i].origin);
		}
	}

	for (i = 0; i < pmove->numvisent; i++)
	{
		if (pmove->visents[i].info == iInfo)
		{
			pmove->visents[i].solid = iSolid;
			VectorCopy(vecOrigin, pmove->visents[i].origin);
		}
	}

	for (i = 0; i < pmove->nummoveent; i++)
	{
		if (pmove->moveents[i].info == iInfo)
		{
			pmove->moveents[i].solid = iSolid;
			VectorCopy(vecOrigin, pmove->moveents[i].origin);
		}
	}
}

/*
This modume implements the shared player physics code between any particular game and
the engine.  The same PM_Move routine is built into the game .dll and the client .dll and is
invoked by each side as appropriate.  There should be no distinction, internally, between server
and client.  This will ensure that prediction behaves appropriately.
*/

// orig 0xa8e60, orig client 0xbc2f0 (not in files-rc2.tsv: Ghidra missed it). RC2: reads the player's index,
// shrink and movement flags from physinfo and its mode and ride from the game, hides the
// player's own entities and those it sees through, sets the stop speed, and always resets
// friction (halflife-updated only for MOVETYPE_WALK; SDK 2.3 only in single player)
void PM_Move(struct playermove_s* ppmove, qboolean server)
{
	assert(pm_shared_initialized);

	pmove = ppmove;

	g_iPMPlayerEnt = atoi(pmove->PM_Info_ValueForKey(pmove->physinfo, "eid"));
	g_bPMShrunk = atoi(pmove->PM_Info_ValueForKey(pmove->physinfo, "shr"));
	// a missing key leaves the previous move's flags, as in the original
	sscanf(pmove->PM_Info_ValueForKey(pmove->physinfo, "phf"), "%02X", &g_iPMFlags);
	g_iPMMode = PM_GetPlayerMode(g_iPMPlayerEnt);

	const unsigned char* pRide = PM_GetRideInfo(g_iPMPlayerEnt);
	g_iPMRideEnt = *pRide & 0x7f;
	g_bPMRideLeft = (*pRide >> 7) & 1;

#ifndef CLIENT_DLL
	PM_HideOwnedEnts(g_iPMPlayerEnt, PM_SetEntsByInfo);
	PM_HideViewers(g_iPMPlayerEnt, PM_SetEntsByInfo);
#endif

	g_flPMStopSpeed = pmove->movevars->stopspeed;
	if (0 != g_bPMShrunk)
	{
		pmove->usehull = 2;
	}
	else
	{
		switch (g_iPMMode)
		{
		case PM_MODE_ASSASSIN:
			pmove->flags &= ~FL_DUCKING;
			pmove->usehull = 0;
			g_flPMStopSpeed = RC2Pref(RC2P_TransformAssassin_WalkSpeed).f - 10.0;
			break;
		case PM_MODE_HOUNDEYE:
			pmove->flags &= ~FL_DUCKING;
			pmove->usehull = 1;
			g_flPMStopSpeed = RC2Pref(RC2P_TransformHoundeye_WalkSpeed).f - 10.0;
			break;
		case PM_MODE_CONTROLLER:
			pmove->flags &= ~FL_DUCKING;
			pmove->usehull = 0;
			g_flPMStopSpeed = RC2Pref(RC2P_TransformController_WalkSpeed).f - 10.0;
			break;
		case PM_MODE_BAT:
			pmove->flags &= ~FL_DUCKING;
			pmove->usehull = 1;
			g_flPMStopSpeed = RC2Pref(RC2P_TransformBat_WalkSpeed).f - 10.0;
			break;
		case PM_MODE_HEADCRAB:
			pmove->flags &= ~FL_DUCKING;
			pmove->usehull = 1;
			g_flPMStopSpeed = RC2Pref(RC2P_TransformHeadcrab_WalkSpeed).f - 10.0;
			break;
		}
	}

	if (g_iPMMode == PM_MODE_NORMAL)
		g_flPMStopSpeed *= PM_GetAttributeScale(1, g_iPMPlayerEnt);
	if (0 != g_bPMShrunk)
		g_flPMStopSpeed *= 0.2;
	if (g_flPMStopSpeed < 1.0)
		g_flPMStopSpeed = 1.0;

	PM_PlayerMove((server != 0) ? true : false);

	if (pmove->onground != -1)
	{
		pmove->flags |= FL_ONGROUND;
	}
	else
	{
		pmove->flags &= ~FL_ONGROUND;
	}

#ifdef CLIENT_DLL
	// the client hides nothing; it records where the move left this player
	PM_SetPredictedOrigin(g_iPMPlayerEnt, pmove->origin);
#else
	PM_RestoreOwnedEnts(g_iPMPlayerEnt, PM_SetEntsByInfo);
	PM_RestoreViewers(g_iPMPlayerEnt, PM_SetEntsByInfo);
#endif

	// Reset friction after each movement so FrictionModifier Triggers work still.
	pmove->friction = 1.0f;
}

int PM_GetVisEntInfo(int ent) // orig client 0xbc610 (not in files-client.tsv: Ghidra missed it). Not byte-identical to SDK 2.3's, but it reads the same
{
	if (ent >= 0 && ent <= pmove->numvisent)
	{
		return pmove->visents[ent].info;
	}
	return -1;
}

int PM_GetPhysEntInfo(int ent)
{
	if (ent >= 0 && ent <= pmove->numphysent)
	{
		return pmove->physents[ent].info;
	}
	return -1;
}

void PM_Init(struct playermove_s* ppmove)
{
	assert(!pm_shared_initialized);

	pmove = ppmove;

	PM_CreateStuckTable();
	PM_InitTextureTypes();

	//The engine copies the hull sizes initialized by PM_GetHullBounds *before* PM_GetHullBounds is actually called, so manually initialize these.
	for (int i = 0; i < NUM_HULLS; ++i)
	{
		if (!PM_GetHullBounds(i, pmove->player_mins[i], pmove->player_maxs[i]))
		{
			//Matches the engine's behavior in ignoring the remaining hull sizes if any hull isn't provided.
			break;
		}
	}

	pm_shared_initialized = true;
}

bool PM_GetHullBounds(int hullnumber, float* mins, float* maxs)
{
	switch (hullnumber)
	{
	case 0: // Normal player
		VEC_HULL_MIN.CopyToArray(mins);
		VEC_HULL_MAX.CopyToArray(maxs);
		return true;
	case 1: // Crouched player
		VEC_DUCK_HULL_MIN.CopyToArray(mins);
		VEC_DUCK_HULL_MAX.CopyToArray(maxs);
		return true;
	case 2: // Point based hull
		g_vecZero.CopyToArray(mins);
		g_vecZero.CopyToArray(maxs);
		return true;
	}

	return false;
}

#ifdef CLIENT_DLL
/*
Rocket Crowbar 2: the client's side of the game hooks pm_rc2.h declares. client.dll has its own
versions: the rider and entity box in cl_dll/rc2/pm_client.cpp, the race move and pullers in
cl_dll/rc2/pullers_client.cpp, rc2.ini's values in cl_dll/rc2/rc2prefs_client.cpp, the
transforms' (PM_GetPlayerMode, PM_GetBatPerch, the headcrab leap) in cl_dll/rc2/transform_hud.cpp.
The client's move doesn't call the hide/restore hooks, PM_BatUnperch, PM_HeadcrabLeapHit or the
jetboost's energy and effect.
*/
// TEMP STUB (c4-P): real ones belong in the client's player-state code (client 0x4f480-0x4f5c0,
// not this package). Weak, so the real ones replace them; each answers as for "no such player".
__attribute__((weak)) float PM_GetAttributeScale(int iWhich, int iEnt) { return 1.0f; }
__attribute__((weak)) int PM_GetAbilityFlags(int iEnt) { return 0; }
__attribute__((weak)) void PM_SetPlayerTimer(int iEnt, int iSlot, int iMsec) {}
__attribute__((weak)) int PM_GetPlayerTimer(int iEnt, int iSlot) { return 0; }
#else
// TEMP STUB (p6): real ones belong in player.cpp (rc2.dll 0x95890-0x95ab0, p5), the transforms'
// files (0xd9a60, 0xd9b00, 0xdf260-0xdf320, p8), triggers (0xe3400, p8) and util (0xf7150,
// 0xf7200, p8). Each answers as the original does when there is no such player.
float PM_GetAttributeScale(int iWhich, int iEnt) { return 1.0f; }
int PM_GetAbilityFlags(int iEnt) { return 0; }
void PM_SetPlayerTimer(int iEnt, int iSlot, int iMsec) {}
int PM_GetPlayerTimer(int iEnt, int iSlot) { return 0; }
float PM_GetEnergy(int iEnt) { return 100.0f; }
void PM_UseEnergy(int iEnt, float flAmount, float flRechargeDelay) {}
void PM_JetboostEffect(int iEnt, int iFlag, float* vecDir, float* vecVelocity) {}
void PM_HideViewers(int iEnt, PM_SETENTFUNC pfnSet) {}
void PM_RestoreViewers(int iEnt, PM_SETENTFUNC pfnSet) {}
int PM_GetPlayerMode(int iEnt) { return 0; }
void PM_HideOwnedEnts(int iEnt, PM_SETENTFUNC pfnSet) {}
void PM_RestoreOwnedEnts(int iEnt, PM_SETENTFUNC pfnSet) {}
int PM_GetBatPerch(int iEnt, int* piAttachInfo, float* vecOffset) { return 0; }
void PM_BatUnperch(int iEnt) {}
void PM_SetHeadcrabLeap(int iEnt, int bLeaping) {}
int PM_GetHeadcrabLeap(int iEnt) { return 0; }
int PM_HeadcrabLeapHit(int iEnt, int iHitInfo, float flSpeed) { return 0; }
#endif
