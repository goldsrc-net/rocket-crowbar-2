// view/refresh setup functions

#include "hud.h"
#include "cl_util.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"

#include "entity_state.h"
#include "cl_entity.h"
#include "ref_params.h"
#include "in_defs.h" // PITCH YAW ROLL
#include "pm_movevars.h"
#include "pm_shared.h"
#include "pm_defs.h"
#include "event_api.h"
#include "pmtrace.h"
#include "screenfade.h"
#include "shake.h"
#include "hltv.h"
#include "Exports.h"

int CL_IsThirdPerson();
void CL_CameraOffset(float* ofs);

void DLLEXPORT V_CalcRefdef(struct ref_params_s* pparams);
extern int g_iViewLock; // Rocket Crowbar 2's, below
extern int g_iViewMode;

void PM_ParticleLine(float* start, float* end, int pcolor, float life, float vert);
int PM_GetVisEntInfo(int ent);
int PM_GetPhysEntInfo(int ent);
void InterpolateAngles(float* start, float* end, float* output, float frac);
void NormalizeAngles(float* angles);
float Distance(const float* v1, const float* v2);
float AngleBetweenVectors(const float* v1, const float* v2);

extern float vJumpOrigin[3];
extern float vJumpAngles[3];


void V_DropPunchAngle(float frametime, float* ev_punchangle);
void VectorAngles(const float* forward, float* angles);

#include "r_studioint.h"
#include "com_model.h"
#include "kbutton.h"

extern engine_studio_api_t IEngineStudio;

extern kbutton_t in_mlook;

/*
The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.
*/

extern cvar_t* cl_forwardspeed;
extern cvar_t* chase_active;
extern cvar_t *scr_ofsx, *scr_ofsy, *scr_ofsz;
extern cvar_t* cl_vsmoothing;
extern cvar_t* cl_rollangle;
extern cvar_t* cl_rollspeed;
extern cvar_t* cl_bobtilt;

#define CAM_MODE_RELAX 1
#define CAM_MODE_FOCUS 2

Vector v_origin, v_angles, v_cl_angles, v_sim_org, v_lastAngles;
float v_frametime, v_lastDistance;
float v_cameraRelaxAngle = 5.0f;
float v_cameraFocusAngle = 35.0f;
int v_cameraMode = CAM_MODE_FOCUS;
bool v_resetCamera = true;

Vector v_client_aimangles;
Vector ev_punchangle;

cvar_t* scr_ofsx;
cvar_t* scr_ofsy;
cvar_t* scr_ofsz;

cvar_t* v_centermove;
cvar_t* v_centerspeed;

cvar_t* cl_bobcycle;
cvar_t* cl_bob;
cvar_t* cl_bobup;
cvar_t* cl_waterdist;
cvar_t* cl_chasedist;

// These cvars are not registered (so users can't cheat), so set the ->value field directly
// Register these cvars in V_Init() if needed for easy tweaking
cvar_t v_iyaw_cycle = {"v_iyaw_cycle", "2", 0, 2};
cvar_t v_iroll_cycle = {"v_iroll_cycle", "0.5", 0, 0.5};
cvar_t v_ipitch_cycle = {"v_ipitch_cycle", "1", 0, 1};
cvar_t v_iyaw_level = {"v_iyaw_level", "0.3", 0, 0.3};
cvar_t v_iroll_level = {"v_iroll_level", "0.1", 0, 0.1};
cvar_t v_ipitch_level = {"v_ipitch_level", "0.3", 0, 0.3};

float v_idlescale; // used by TFC for concussion grenade effect

//=============================================================================
/*
void V_NormalizeAngles( Vector& angles )
{
	int i;
	// Normalize angles
	for ( i = 0; i < 3; i++ )
	{
		if ( angles[i] > 180.0 )
		{
			angles[i] -= 360.0;
		}
		else if ( angles[i] < -180.0 )
		{
			angles[i] += 360.0;
		}
	}
}

/*
===================
V_InterpolateAngles

Interpolate Euler angles.
FIXME:  Use Quaternions to avoid discontinuities
Frac is 0.0 to 1.0 ( i.e., should probably be clamped, but doesn't have to be )
===================

void V_InterpolateAngles( float *start, float *end, float *output, float frac )
{
	int i;
	float ang1, ang2;
	float d;
	
	V_NormalizeAngles( start );
	V_NormalizeAngles( end );

	for ( i = 0 ; i < 3 ; i++ )
	{
		ang1 = start[i];
		ang2 = end[i];

		d = ang2 - ang1;
		if ( d > 180 )
		{
			d -= 360;
		}
		else if ( d < -180 )
		{	
			d += 360;
		}

		output[i] = ang1 + d * frac;
	}

	V_NormalizeAngles( output );
} */

// Quakeworld bob code, this fixes jitters in the mutliplayer since the clock (pparams->time) isn't quite linear
// orig client 0xe9400. Rocket Crowbar 2: no bob for spectators or while g_iViewLock is set.
float V_CalcBob(struct ref_params_s* pparams)
{
	static double bobtime = 0;
	static float bob = 0;
	float cycle;
	static float lasttime = 0;
	Vector vel;

	if (0 != pparams->spectator || 0 != g_iViewLock)
		return 0;

	if (pparams->onground == -1 ||
		pparams->time == lasttime)
	{
		// just use old value
		return bob;
	}

	lasttime = pparams->time;

	bobtime += pparams->frametime;
	cycle = bobtime - (int)(bobtime / cl_bobcycle->value) * cl_bobcycle->value;
	cycle /= cl_bobcycle->value;

	if (cycle < cl_bobup->value)
	{
		cycle = M_PI * cycle / cl_bobup->value;
	}
	else
	{
		cycle = M_PI + M_PI * (cycle - cl_bobup->value) / (1.0 - cl_bobup->value);
	}

	// bob is proportional to simulated velocity in the xy plane
	// (don't count Z, or jumping messes it up)
	VectorCopy(pparams->simvel, vel);
	vel[2] = 0;

	bob = sqrt(vel[0] * vel[0] + vel[1] * vel[1]) * cl_bob->value;
	bob = bob * 0.3 + bob * 0.7 * sin(cycle);
	bob = V_min(bob, 4.0f);
	bob = V_max(bob, -7.0f);
	return bob;
}

/*
===============
V_CalcRoll
Used by view and sv_user
===============
*/
float V_CalcRoll(Vector angles, Vector velocity, float rollangle, float rollspeed)
{
	float sign;
	float side;
	float value;
	Vector forward, right, up;

	AngleVectors(angles, forward, right, up);

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

typedef struct pitchdrift_s
{
	float pitchvel;
	int nodrift;
	float driftmove;
	double laststop;
} pitchdrift_t;

static pitchdrift_t pd;

void V_StartPitchDrift()
{
	if (pd.laststop == gEngfuncs.GetClientTime())
	{
		return; // something else is keeping it from drifting
	}

	if (0 != pd.nodrift || 0 == pd.pitchvel)
	{
		pd.pitchvel = v_centerspeed->value;
		pd.nodrift = 0;
		pd.driftmove = 0;
	}
}

void V_StopPitchDrift()
{
	pd.laststop = gEngfuncs.GetClientTime();
	pd.nodrift = 1;
	pd.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.
===============
*/
void V_DriftPitch(struct ref_params_s* pparams)
{
	float delta, move;

	if (0 != gEngfuncs.IsNoClipping() || 0 == pparams->onground || 0 != pparams->demoplayback || 0 != pparams->spectator)
	{
		pd.driftmove = 0;
		pd.pitchvel = 0;
		return;
	}

	// don't count small mouse motion
	if (0 != pd.nodrift)
	{
		if (v_centermove->value > 0 && (in_mlook.state & 1) == 0)
		{
			// this is for lazy players. if they stopped, looked around and then continued
			// to move the view will be centered automatically if they move more than
			// v_centermove units.

			if (fabs(pparams->cmd->forwardmove) < cl_forwardspeed->value)
				pd.driftmove = 0;
			else
				pd.driftmove += pparams->frametime;

			if (pd.driftmove > v_centermove->value)
			{
				V_StartPitchDrift();
			}
			else
			{
				return; // player didn't move enough
			}
		}

		return; // don't drift view
	}

	delta = pparams->idealpitch - pparams->cl_viewangles[PITCH];

	if (0 == delta)
	{
		pd.pitchvel = 0;
		return;
	}

	move = pparams->frametime * pd.pitchvel;

	pd.pitchvel *= (1.0f + (pparams->frametime * 0.25f)); // get faster by time

	if (delta > 0)
	{
		if (move > delta)
		{
			pd.pitchvel = 0;
			move = delta;
		}
		pparams->cl_viewangles[PITCH] += move;
	}
	else if (delta < 0)
	{
		if (move > -delta)
		{
			pd.pitchvel = 0;
			move = -delta;
		}
		pparams->cl_viewangles[PITCH] -= move;
	}
}

/* 
============================================================================== 
						VIEW RENDERING 
============================================================================== 
*/

/*
==================
V_CalcGunAngle
==================
*/
void V_CalcGunAngle(struct ref_params_s* pparams)
{
	cl_entity_t* viewent;

	viewent = gEngfuncs.GetViewModel();
	if (!viewent)
		return;

	viewent->angles[YAW] = pparams->viewangles[YAW] + pparams->crosshairangle[YAW];
	viewent->angles[PITCH] = -pparams->viewangles[PITCH] + pparams->crosshairangle[PITCH] * 0.25;
	viewent->angles[ROLL] -= v_idlescale * sin(pparams->time * v_iroll_cycle.value) * v_iroll_level.value;

	// don't apply all of the v_ipitch to prevent normally unseen parts of viewmodel from coming into view.
	viewent->angles[PITCH] -= v_idlescale * sin(pparams->time * v_ipitch_cycle.value) * (v_ipitch_level.value * 0.5);
	viewent->angles[YAW] -= v_idlescale * sin(pparams->time * v_iyaw_cycle.value) * v_iyaw_level.value;
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void V_AddIdle(struct ref_params_s* pparams)
{
	pparams->viewangles[ROLL] += v_idlescale * sin(pparams->time * v_iroll_cycle.value) * v_iroll_level.value;
	pparams->viewangles[PITCH] += v_idlescale * sin(pparams->time * v_ipitch_cycle.value) * v_ipitch_level.value;
	pparams->viewangles[YAW] += v_idlescale * sin(pparams->time * v_iyaw_cycle.value) * v_iyaw_level.value;
}


/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
// orig client 0xe9ac0. SDK 2.1's: the roll from the movevars; dying resets RC2's view mode.
void V_CalcViewRoll(struct ref_params_s* pparams)
{
	float side;
	cl_entity_t* viewentity;

	viewentity = gEngfuncs.GetEntityByIndex(pparams->viewentity);
	if (!viewentity)
		return;

	side = V_CalcRoll(viewentity->angles, pparams->simvel, pparams->movevars->rollangle, pparams->movevars->rollspeed);

	pparams->viewangles[ROLL] += side;

	if (pparams->health <= 0 && (pparams->viewheight[2] != 0))
	{
		// only roll the view if the player is dead and the viewheight[2] is nonzero
		// this is so deadcam in multiplayer will work.
		pparams->viewangles[ROLL] = 80; // dead view angle
		g_iViewMode = 0;
	}
}


/*
==================
V_CalcIntermissionRefdef

==================
*/
// orig client 0xe9b80. SDK 2.1's (no HLTV camera).
void V_CalcIntermissionRefdef(struct ref_params_s* pparams)
{
	cl_entity_t *ent, *view;
	float old;

	// ent is the player model ( visible when out of body )
	ent = gEngfuncs.GetLocalPlayer();

	// view is the weapon model (only visible from inside body )
	view = gEngfuncs.GetViewModel();

	VectorCopy(pparams->simorg, pparams->vieworg);
	VectorCopy(pparams->cl_viewangles, pparams->viewangles);

	view->model = NULL;

	// allways idle in intermission
	old = v_idlescale;
	v_idlescale = 1;

	V_AddIdle(pparams);

	v_idlescale = old;

	v_origin = pparams->vieworg;
	v_angles = pparams->viewangles;
}

//==========================================================================
// Rocket Crowbar 2's view shake: random wave phases and frequencies on each axis, decaying over
// its duration. Names ours.
static bool g_bViewShake;				  // 0x10227990
static int g_iViewShakePriority = -999;	  // 0x101768b4
static float g_flViewShakeAmplitude;	  // 0x10227978
static float g_flViewShakeFrequency;	  // 0x1022682c
static float g_flViewShakeDuration;		  // 0x1022716c
static float g_flViewShakeWave[3][6];	  // 0x10227100: per output axis, 3 frequencies then 3 phases

// orig client 0xe9c70
void V_ResetViewShake() // name ours
{
	g_iViewShakePriority = -999;
	g_bViewShake = false;
}

// orig client 0xe9ca0. A shake of no lower priority replaces the current one.
void V_StartViewShake(float flAmplitude, float flFrequency, float flDuration, int iPriority) // name ours
{
	if (g_bViewShake && iPriority < g_iViewShakePriority)
		return;

	g_iViewShakePriority = iPriority;
	g_bViewShake = true;
	g_flViewShakeDuration = flDuration;
	g_flViewShakeFrequency = flFrequency;
	g_flViewShakeAmplitude = flAmplitude;

	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			g_flViewShakeWave[i][j] = (j + 1.0) * 0.5 * gEngfuncs.pfnRandomFloat(1.2566371f, 1.0471976f); // low above high, as in the original
			g_flViewShakeWave[i][j + 3] = gEngfuncs.pfnRandomFloat(0, 6.2831855f);
		}
	}
}

// orig client 0xe9d80. Adds the shake to angles (scaled), decaying amplitude and frequency
// with the time left; not in RC2's view modes 1 and 2.
void V_ApplyViewShake(float flFrameTime, float flTime, float* angles, float* origin, float flScale) // name ours
{
	if (!g_bViewShake)
		return;

	g_flViewShakeDuration -= flFrameTime;
	if (g_flViewShakeDuration <= 0)
	{
		g_bViewShake = false;
		return;
	}

	g_flViewShakeAmplitude -= (flFrameTime / g_flViewShakeDuration) * g_flViewShakeAmplitude;
	if (g_flViewShakeAmplitude <= 0)
	{
		g_bViewShake = false;
		return;
	}

	g_flViewShakeFrequency -= (flFrameTime / g_flViewShakeDuration) * g_flViewShakeFrequency * 0.5;
	if (g_flViewShakeFrequency <= 0)
	{
		g_bViewShake = false;
		return;
	}

	if (g_iViewMode == 1 || g_iViewMode == 2)
		return;

	for (int i = 0; i < 3; i++)
	{
		float* w = g_flViewShakeWave[i];
		angles[i] += (cos(g_flViewShakeFrequency * w[0] * flTime + w[3]) + cos(g_flViewShakeFrequency * w[1] * flTime + w[4]) + cos(g_flViewShakeFrequency * w[2] * flTime + w[5])) * g_flViewShakeAmplitude * flScale;
	}
}

#define ORIGIN_BACKUP 64
#define ORIGIN_MASK (ORIGIN_BACKUP - 1)

typedef struct
{
	float Origins[ORIGIN_BACKUP][3];
	float OriginTime[ORIGIN_BACKUP];

	float Angles[ORIGIN_BACKUP][3];
	float AngleTime[ORIGIN_BACKUP];

	int CurrentOrigin;
	int CurrentAngle;
} viewinterp_t;

// Rocket Crowbar 2's view state, set from elsewhere in its client code. Names ours.
int g_iViewLock = 0;		 // 0x10227160: bit 1 smooths the view angles, bit 2 the origin; any bit stops the bob
int g_bLockViewOrigin = 0;	 // 0x10227164
Vector g_vecLockedViewOrigin; // 0x10227084: the origin held while g_bLockViewOrigin
int g_bLockViewAngles = 0;	 // 0x102270f0
Vector g_vecLockedViewAngles; // 0x10226848
bool g_bMainView = true;	 // 0x101768b8: false while the second (scope or overview) view is drawn
static bool g_bViewEntityOverride; // 0x10227991

// RC2's view mode (ViewMode message, hud.cpp): 1 ahead of the view entity, 2 on its up axis, 3 a
// turret on it; 0 its own view
extern int g_iViewMode;				 // name ours
extern int g_bThirdPersonBlocked;	 // name ours: set when the third person camera has no room
extern int g_iPlayerViewState;		 // name ours: 1 scoped (second view), 4 with g_iPlayerViewSubState 2 upside down, 6 fixed angles
extern int g_iPlayerViewSubState;	 // name ours
extern int g_bPlayerViewBusy;		 // name ours
extern bool g_bScopeView;			 // name ours: the scope's second view is drawn this frame
extern Vector g_vecFixedViewAngles;	 // name ours
extern int g_bTiltBodyWithView;		 // name ours: the player model pitches with a third person view
extern int g_bScreenTintBlue;		 // name ours
extern int g_bScreenTintMagenta;	 // name ours
extern int g_bViewOverride;			 // name ours

// VectorAngles of a basis with its roll (RC2's own code at client 0xb0e50, package c3)
extern void RC2_BasisToAngles(const float* forward, const float* right, const float* up, float* angles); // name ours

/*
==================
V_CalcRefdef

==================
*/
// orig client 0xe9ee0. SDK 2.1's with RC2's: a held origin and angles, the angles smoothed as
// the origin is (cl_vsmoothing, by g_iViewLock), its shake, a third person camera that
// backs off walls, a view model hidden by HUD bit 0x10, the view modes on another entity and a
// scope's second view.
void V_CalcNormalRefdef(struct ref_params_s* pparams)
{
	cl_entity_t *ent, *view;
	int i;
	Vector angles;
	float bob, waterOffset;
	static viewinterp_t ViewInterp;
	static viewinterp_t ViewAngleInterp;
	static float oldz = 0;
	static float lasttime;
	static Vector lastangles;
	static Vector lastorg;

	Vector camAngles, camForward, camRight, camUp;
	cl_entity_t* pwater;
	bool bThirdPerson = false;
	Vector delta;

	if (pparams->maxclients > 1)
	{
		scr_ofsx->value = 0;
		scr_ofsy->value = 0;
		scr_ofsz->value = 0;
	}

	V_DriftPitch(pparams);

	// ent is the player model ( visible when out of body )
	ent = gEngfuncs.GetLocalPlayer();

	// view is the weapon model (only visible from inside body )
	view = gEngfuncs.GetViewModel();

	// transform the view offset by the model's matrix to get the offset from
	// model origin for the view
	bob = V_CalcBob(pparams);

	if (0 != g_bLockViewOrigin)
	{
		VectorCopy(g_vecLockedViewOrigin, pparams->vieworg);
		VectorCopy(g_vecLockedViewOrigin, pparams->simorg);
	}

	// refresh position
	VectorCopy(pparams->simorg, pparams->vieworg);
	pparams->vieworg[0] += pparams->viewheight[0];
	pparams->vieworg[1] += pparams->viewheight[1];
	pparams->vieworg[2] = bob + pparams->simorg[2] + pparams->viewheight[2];

	if (0 != g_bLockViewAngles)
		VectorCopy(g_vecLockedViewAngles, pparams->cl_viewangles);

	// keep the client's angles for smoothing
	VectorSubtract(pparams->cl_viewangles, lastangles, delta);
	if (Length(delta) != 0.0)
	{
		VectorCopy(pparams->cl_viewangles, ViewAngleInterp.Origins[ViewAngleInterp.CurrentOrigin & ORIGIN_MASK]);
		ViewAngleInterp.OriginTime[ViewAngleInterp.CurrentOrigin & ORIGIN_MASK] = pparams->time;
		ViewAngleInterp.CurrentOrigin++;
		VectorCopy(pparams->cl_viewangles, lastangles);
	}

	if (0 == pparams->nextView && cl_vsmoothing && 0 != cl_vsmoothing->value && (g_iViewLock & 2) != 0)
	{
		if (cl_vsmoothing->value < 0.0)
			gEngfuncs.Cvar_SetValue("cl_vsmoothing", 0.0);

		float t = pparams->time - cl_vsmoothing->value;
		int foundidx;
		for (i = 1; i < ORIGIN_MASK; i++)
		{
			foundidx = ViewAngleInterp.CurrentOrigin - 1 - i;
			if (ViewAngleInterp.OriginTime[foundidx & ORIGIN_MASK] <= t)
				break;
		}

		if (i < ORIGIN_MASK && ViewAngleInterp.OriginTime[foundidx & ORIGIN_MASK] != 0.0)
		{
			float dt = ViewAngleInterp.OriginTime[(foundidx + 1) & ORIGIN_MASK] - ViewAngleInterp.OriginTime[foundidx & ORIGIN_MASK];
			if (dt > 0.0)
			{
				float frac = (t - ViewAngleInterp.OriginTime[foundidx & ORIGIN_MASK]) / dt;
				frac = V_min(1.0f, frac);
				InterpolateAngles(ViewAngleInterp.Origins[foundidx & ORIGIN_MASK], ViewAngleInterp.Origins[(foundidx + 1) & ORIGIN_MASK], pparams->cl_viewangles, frac);
				VectorCopy(pparams->cl_viewangles, g_vecLockedViewAngles);
			}
		}
	}

	VectorCopy(pparams->cl_viewangles, pparams->viewangles);

	gEngfuncs.V_CalcShake();
	gEngfuncs.V_ApplyShake(pparams->vieworg, pparams->viewangles, 1.0);
	V_ApplyViewShake(pparams->frametime, pparams->time, pparams->viewangles, pparams->vieworg, 1.0);

	// never let view origin sit exactly on a node line, because a water plane can
	// dissapear when viewed with the eye exactly on it.
	// FIXME, we send origin at 1/128 now, change this?
	// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis

	pparams->vieworg[0] += 1.0 / 32;
	pparams->vieworg[1] += 1.0 / 32;
	pparams->vieworg[2] += 1.0 / 32;

	// Check waterlevel
	waterOffset = 0;
	if (pparams->waterlevel >= 2)
	{
		int contents, waterDist, waterEntity;
		Vector point;
		waterDist = cl_waterdist->value;

		if (0 != pparams->hardware)
		{
			waterEntity = gEngfuncs.PM_WaterEntity(pparams->simorg);
			if (waterEntity >= 0 && waterEntity < pparams->max_entities)
			{
				pwater = gEngfuncs.GetEntityByIndex(waterEntity);
				if (pwater && (pwater->model != NULL))
				{
					waterDist += (pwater->curstate.scale * 16); // Add in wave height
				}
			}
		}
		else
		{
			waterEntity = 0; // Don't need this in software
		}

		VectorCopy(pparams->vieworg, point);

		// Eyes are above water, make sure we're above the waves
		if (pparams->waterlevel == 2)
		{
			point[2] -= waterDist;
			for (i = 0; i < waterDist; i++)
			{
				contents = gEngfuncs.PM_PointContents(point, NULL);
				if (contents > CONTENTS_WATER)
					break;
				point[2] += 1;
			}
			waterOffset = (point[2] + waterDist) - pparams->vieworg[2];
		}
		else
		{
			// eyes are under water.  Make sure we're far enough under
			point[2] += waterDist;

			for (i = 0; i < waterDist; i++)
			{
				contents = gEngfuncs.PM_PointContents(point, NULL);
				if (contents <= CONTENTS_WATER)
					break;
				point[2] -= 1;
			}
			waterOffset = (point[2] - waterDist) - pparams->vieworg[2];
		}
	}

	pparams->vieworg[2] += waterOffset;

	V_CalcViewRoll(pparams);

	V_AddIdle(pparams);

	// offsets
	VectorCopy(pparams->cl_viewangles, angles);

	AngleVectors(angles, pparams->forward, pparams->right, pparams->up);

	for (i = 0; i < 3; i++)
	{
		pparams->vieworg[i] += scr_ofsx->value * pparams->forward[i] + scr_ofsy->value * pparams->right[i] + scr_ofsz->value * pparams->up[i];
	}

	// Treating cam_ofs[2] as the distance, backed off walls
	if (0 == pparams->nextView)
	{
		g_bThirdPersonBlocked = 0;

		if (0 != CL_IsThirdPerson())
		{
			Vector ofs;

			ofs[0] = ofs[1] = ofs[2] = 0.0;

			CL_CameraOffset((float*)&ofs);

			camAngles[0] = ofs[0];
			camAngles[1] = ofs[1];
			camAngles[2] = 0;

			AngleVectors(camAngles, camForward, camRight, camUp);

			float flDist = ofs[2];
			if (flDist < 24)
				flDist = 24;
			else if (flDist > 256)
				flDist = 256;

			Vector end;
			for (i = 0; i < 3; i++)
				end[i] = pparams->vieworg[i] - flDist * camForward[i];

			pmtrace_t tr = *gEngfuncs.PM_TraceLine(pparams->vieworg, end, 1, 2, -1);
			if (tr.fraction < 1.0)
				flDist = flDist * tr.fraction - 8;

			if (flDist < 18 || 0 != tr.allsolid || 0 != tr.startsolid)
			{
				bThirdPerson = false;
				g_bThirdPersonBlocked = 1;
			}
			else
			{
				bThirdPerson = true;
				g_bThirdPersonBlocked = 0;
				for (i = 0; i < 3; i++)
					pparams->vieworg[i] -= flDist * camForward[i];
			}
		}
	}

	// VERIFY: HUD bit 0x10 (gHUD.m_iHideHUDDisplay, gHUD+0x54 in client.dll) hides the view model
	if ((gHUD.m_iHideHUDDisplay & 0x10) == 0)
	{
		// Give gun our viewangles
		VectorCopy(pparams->cl_viewangles, view->angles);

		// set up gun position
		V_CalcGunAngle(pparams);

		// Use predicted origin as view origin.
		VectorCopy(pparams->simorg, view->origin);
		view->origin[2] += (waterOffset);
		VectorAdd(view->origin, pparams->viewheight, view->origin);

		// Let the viewmodel shake at about 10% of the amplitude
		gEngfuncs.V_ApplyShake(view->origin, view->angles, 0.9);
		V_ApplyViewShake(pparams->frametime, pparams->time, view->angles, view->origin, 1.5);

		for (i = 0; i < 3; i++)
		{
			view->origin[i] += bob * 0.4 * pparams->forward[i];
		}
		view->origin[2] += bob;

		// throw in a little tilt.
		view->angles[YAW] -= bob * 0.5;
		view->angles[ROLL] -= bob * 1;
		view->angles[PITCH] -= bob * 0.3;

		// pushing the view origin down off of the same X/Y plane as the ent's origin will give the
		// gun a very nice 'shifting' effect when the player looks up/down. If there is a problem
		// with view model distortion, this may be a cause. (SJB).
		view->origin[2] -= 1;

		// fudge position around to keep amount of weapon visible
		// roughly equal with different FOV
		if (pparams->viewsize == 110)
		{
			view->origin[2] += 1;
		}
		else if (pparams->viewsize == 100)
		{
			view->origin[2] += 2;
		}
		else if (pparams->viewsize == 90)
		{
			view->origin[2] += 1;
		}
		else if (pparams->viewsize == 80)
		{
			view->origin[2] += 0.5;
		}
	}
	else
	{
		view->model = NULL;
	}

	// Add in the punchangle, if any (not in RC2's view modes on another entity)
	if (g_iViewMode != 1 && g_iViewMode != 2 && g_iViewMode != 3)
	{
		VectorAdd(pparams->viewangles, pparams->punchangle, pparams->viewangles);

		// Include client side punch, too
		VectorAdd(pparams->viewangles, (float*)&ev_punchangle, pparams->viewangles);
	}

	if (0 == pparams->nextView)
	{
		V_DropPunchAngle(pparams->frametime, (float*)&ev_punchangle);

		// smooth out stair step ups
		if (0 == pparams->smoothing && 0 != pparams->onground && pparams->simorg[2] - oldz > 0)
		{
			float steptime;

			steptime = pparams->time - lasttime;
			if (steptime < 0)
				//FIXME		I_Error ("steptime < 0");
				steptime = 0;

			oldz += steptime * 150;
			if (oldz > pparams->simorg[2])
				oldz = pparams->simorg[2];
			if (pparams->simorg[2] - oldz > 18)
				oldz = pparams->simorg[2] - 18;
			pparams->vieworg[2] += oldz - pparams->simorg[2];
			view->origin[2] += oldz - pparams->simorg[2];
		}
		else
		{
			oldz = pparams->simorg[2];
		}

		VectorSubtract(pparams->simorg, lastorg, delta);

		if (Length(delta) != 0.0)
		{
			VectorCopy(pparams->simorg, ViewInterp.Origins[ViewInterp.CurrentOrigin & ORIGIN_MASK]);
			ViewInterp.OriginTime[ViewInterp.CurrentOrigin & ORIGIN_MASK] = pparams->time;
			ViewInterp.CurrentOrigin++;

			VectorCopy(pparams->simorg, lastorg);
		}

		// Smooth out whole view in multiplayer when on trains, lifts, or when RC2 asks
		if (cl_vsmoothing && 0 != cl_vsmoothing->value &&
			((g_iViewLock & 4) != 0 || (0 != pparams->smoothing && (pparams->maxclients > 1))))
		{
			int foundidx;
			float t;

			if (cl_vsmoothing->value < 0.0)
			{
				gEngfuncs.Cvar_SetValue("cl_vsmoothing", 0.0);
			}

			t = pparams->time - cl_vsmoothing->value;

			for (i = 1; i < ORIGIN_MASK; i++)
			{
				foundidx = ViewInterp.CurrentOrigin - 1 - i;
				if (ViewInterp.OriginTime[foundidx & ORIGIN_MASK] <= t)
					break;
			}

			if (i < ORIGIN_MASK && ViewInterp.OriginTime[foundidx & ORIGIN_MASK] != 0.0)
			{
				// Interpolate
				Vector neworg;
				double dt;
				float frac;

				dt = ViewInterp.OriginTime[(foundidx + 1) & ORIGIN_MASK] - ViewInterp.OriginTime[foundidx & ORIGIN_MASK];
				if (dt > 0.0)
				{
					frac = (t - ViewInterp.OriginTime[foundidx & ORIGIN_MASK]) / dt;
					frac = V_min(1.0f, frac);
					VectorSubtract(ViewInterp.Origins[(foundidx + 1) & ORIGIN_MASK], ViewInterp.Origins[foundidx & ORIGIN_MASK], delta);
					VectorMA(ViewInterp.Origins[foundidx & ORIGIN_MASK], frac, delta, neworg);

					// Dont interpolate large changes
					if (Length(delta) < 64)
					{
						VectorSubtract(neworg, pparams->simorg, delta);

						VectorAdd(pparams->simorg, delta, pparams->simorg);
						VectorAdd(pparams->vieworg, delta, pparams->vieworg);
						VectorAdd(view->origin, delta, view->origin);

						VectorCopy(pparams->simorg, g_vecLockedViewOrigin);
					}
				}
			}
		}

		// Store off v_angles before munging for third person
		v_angles = pparams->viewangles;

		if (bThirdPerson)
		{
			VectorCopy(camAngles, pparams->viewangles);

			float pitch = camAngles[0];
			if (pitch > 180.0)
				pitch -= 360.0;
			else if (pitch < -180.0)
				pitch += 360.0;

			// the player model leans with the camera's pitch
			if (0 != g_bTiltBodyWithView && 0 == g_bScreenTintBlue && 0 == g_bViewOverride && 0 == g_bPlayerViewBusy)
			{
				pitch *= -0.33333334f;
				ent->angles[PITCH] = pitch;
				ent->curstate.angles[PITCH] = pitch;
				ent->prevstate.angles[PITCH] = pitch;
				ent->latched.prevangles[PITCH] = pitch;
			}
		}
	}

	// RC2's player view states: fixed angles, or upside down
	if (g_iPlayerViewState == 6)
	{
		VectorCopy(g_vecFixedViewAngles, pparams->viewangles);
		v_angles = pparams->viewangles;
	}
	else if (g_iPlayerViewState == 4 && g_iPlayerViewSubState == 2)
	{
		pparams->viewangles[ROLL] = 180;
		v_angles = pparams->viewangles;
	}

	// override all previous settings if the viewent isn't the client, in RC2's view mode
	g_bViewEntityOverride = false;
	if (pparams->viewentity > pparams->maxclients)
	{
		cl_entity_t* viewentity;
		viewentity = gEngfuncs.GetEntityByIndex(pparams->viewentity);
		if (viewentity)
		{
			g_bViewEntityOverride = true;

			if (g_iViewMode == 1)
			{
				// ahead of it
				VectorMA(viewentity->origin, 16, pparams->forward, pparams->vieworg);
			}
			else if (g_iViewMode == 2)
			{
				// on its up axis, turned by the view's yaw
				Vector entAngles(-viewentity->angles[0], viewentity->angles[1], viewentity->angles[2]);
				Vector up, a, b;
				AngleVectors(entAngles, NULL, NULL, up);
				VectorMA(viewentity->origin, 8, up, pparams->vieworg);

				if (up[0] == 1.0)
				{
					up = Vector(1, 0, 0);
					a = Vector(0, 0, -1);
					b = Vector(0, 1, 0);
				}
				else if (up[0] == -1.0)
				{
					up = Vector(-1, 0, 0);
					a = Vector(0, 0, 1);
					b = Vector(0, 1, 0);
				}
				else
				{
					// VERIFY: in the two cases above the original goes on with "delta" as it was
					// left (the last origin change); only here is it the cross product
					b = Vector(1, 0, 0);
					CrossProduct(up, b, delta);
					CrossProduct(delta, up, a);
				}

				float c = cos(pparams->viewangles[YAW] * (M_PI / 180));
				float sn = sin(pparams->viewangles[YAW] * (M_PI / 180));

				Vector forward, right, newup, result;
				VectorScale(a, c, forward);
				VectorMA(forward, sn, delta, forward);
				VectorMA(forward, 0, up, forward);
				VectorScale(a, sn, right);
				VectorMA(right, -c, delta, right);
				VectorMA(right, 0, up, right);
				VectorScale(a, 0, newup);
				VectorMA(newup, 0, delta, newup);
				VectorMA(newup, 1, up, newup);

				RC2_BasisToAngles(forward, right, newup, result);
				pparams->viewangles[0] = -result[0];
				pparams->viewangles[1] = result[1];
				pparams->viewangles[2] = result[2];
			}
			else if (g_iViewMode == 3)
			{
				// a turret on it: up to 45 degrees off its facing
				Vector entAngles(-viewentity->angles[0], viewentity->angles[1], viewentity->angles[2]);
				Vector forward, up, offset;
				AngleVectors(entAngles, forward, NULL, up);
				VectorScale(forward, 3, offset);
				VectorMA(offset, 4, up, offset);
				VectorAdd(offset, viewentity->origin, pparams->vieworg);

				Vector diff(pparams->viewangles[0] - entAngles[0], pparams->viewangles[1] - entAngles[1], 0);
				float flLength = Length(diff);
				if (flLength != 0 && flLength > 45)
					VectorScale(diff, 45 / flLength, diff);

				pparams->viewangles[0] = diff[0] + entAngles[0];
				pparams->viewangles[1] = diff[1] + entAngles[1];
				pparams->viewangles[2] = diff[2] + entAngles[2];
				v_angles = pparams->viewangles;
			}
			else
			{
				VectorCopy(viewentity->origin, pparams->vieworg);
				VectorCopy(viewentity->angles, pparams->viewangles);
				pparams->viewangles[0] *= -1.0;

				// Store off overridden viewangles
				v_angles = pparams->viewangles;
			}
		}
	}

	// Update the latched view origin/angles here, this was
	// previously done in V_CalcGunAngle but that happens
	// before a bunch of other stuff happens, which nukes
	// a bunch of the viewbob fx. (halflife-updated's)
	VectorCopy(view->origin, view->curstate.origin);
	VectorCopy(view->origin, view->latched.prevorigin);
	VectorCopy(view->angles, view->curstate.angles);
	VectorCopy(view->angles, view->latched.prevangles);

	lasttime = pparams->time;
	pparams->onlyClientDraw = 0;
	g_bMainView = true;

	// RC2's scope: a second, smaller view in the middle of the screen, moved along the view to
	// what it looks at (in steps that shrink with the distance)
	if (g_iPlayerViewState == 1 && 0 != pparams->nextView)
	{
		pparams->nextView = 0;
		pparams->viewport[2] = XRES(160);
		pparams->viewport[3] = YRES(120);
		pparams->viewport[0] = XRES(320) - pparams->viewport[2] / 2;
		pparams->viewport[1] = YRES(240) - pparams->viewport[3] / 2;

		Vector end;
		VectorMA(pparams->vieworg, 8192, pparams->forward, end);
		pmtrace_t tr = *gEngfuncs.PM_TraceLine(pparams->vieworg, end, 1, 2, -1);

		g_bScopeView = false;
		bool bNoView = true;
		if (tr.fraction < 1.0)
		{
			float flDist = 8192;
			g_bScopeView = true;
			do
			{
				if (flDist * (1.0f / 8192) < tr.fraction)
					break;
				flDist -= 256;
			} while (flDist > 512);

			bNoView = false;
			if (flDist <= 512)
			{
				if (tr.fraction > 0.125)
					flDist = 1024;
				else if (tr.fraction > 0.09375)
					flDist = 768;
				else if (tr.fraction > 0.0625)
					flDist = 512;
				else if (tr.fraction > 0.03125)
					flDist = 256;
				else if (tr.fraction > 0.015625)
					flDist = 128;
				else if (tr.fraction > 0.0078125)
					flDist = 64;
				else
				{
					g_bScopeView = false;
					bNoView = true;
				}
			}

			if (!bNoView)
				VectorMA(pparams->vieworg, flDist, pparams->forward, pparams->vieworg);
		}

		if (bNoView)
		{
			pparams->viewport[0] = pparams->viewport[1] = pparams->viewport[2] = pparams->viewport[3] = 1;
			pparams->onlyClientDraw = 1;
			g_bMainView = false;
		}
	}

	v_origin = pparams->vieworg;
}

void V_SmoothInterpolateAngles(float* startAngle, float* endAngle, float* finalAngle, float degreesPerSec)
{
	float absd, frac, d, threshhold;

	NormalizeAngles(startAngle);
	NormalizeAngles(endAngle);

	for (int i = 0; i < 3; i++)
	{
		d = endAngle[i] - startAngle[i];

		if (d > 180.0f)
		{
			d -= 360.0f;
		}
		else if (d < -180.0f)
		{
			d += 360.0f;
		}

		absd = fabs(d);

		if (absd > 0.01f)
		{
			frac = degreesPerSec * v_frametime;

			threshhold = degreesPerSec / 4;

			if (absd < threshhold)
			{
				float h = absd / threshhold;
				h *= h;
				frac *= h; // slow down last degrees
			}

			if (frac > absd)
			{
				finalAngle[i] = endAngle[i];
			}
			else
			{
				if (d > 0)
					finalAngle[i] = startAngle[i] + frac;
				else
					finalAngle[i] = startAngle[i] - frac;
			}
		}
		else
		{
			finalAngle[i] = endAngle[i];
		}
	}

	NormalizeAngles(finalAngle);
}

// Get the origin of the Observer based around the target's position and angles
void V_GetChaseOrigin(float* angles, float* origin, float distance, float* returnvec)
{
	Vector vecEnd;
	Vector forward;
	Vector vecStart;
	pmtrace_t* trace;
	int maxLoops = 8;

	int ignoreent = -1; // first, ignore no entity

	cl_entity_t* ent = NULL;

	// Trace back from the target using the player's view angles
	AngleVectors(angles, forward, NULL, NULL);

	VectorScale(forward, -1, forward);

	VectorCopy(origin, vecStart);

	VectorMA(vecStart, distance, forward, vecEnd);

	while (maxLoops > 0)
	{
		trace = gEngfuncs.PM_TraceLine(vecStart, vecEnd, PM_TRACELINE_PHYSENTSONLY, 2, ignoreent);

		// WARNING! trace->ent is is the number in physent list not the normal entity number

		if (trace->ent <= 0)
			break; // we hit the world or nothing, stop trace

		ent = gEngfuncs.GetEntityByIndex(PM_GetPhysEntInfo(trace->ent));

		if (ent == NULL)
			break;

		// hit non-player solid BSP , stop here
		if (ent->curstate.solid == SOLID_BSP && 0 == ent->player)
			break;

		// if close enought to end pos, stop, otherwise continue trace
		if (Distance(trace->endpos, vecEnd) < 1.0f)
		{
			break;
		}
		else
		{
			ignoreent = trace->ent; // ignore last hit entity
			VectorCopy(trace->endpos, vecStart);
		}

		maxLoops--;
	}

	/*	if ( ent )
	{
		gEngfuncs.Con_Printf("Trace loops %i , entity %i, model %s, solid %i\n",(8-maxLoops),ent->curstate.number, ent->model->name , ent->curstate.solid ); 
	} */

	VectorMA(trace->endpos, 4, trace->plane.normal, returnvec);

	v_lastDistance = Distance(trace->endpos, origin); // real distance without offset
}

/*void V_GetDeathCam(cl_entity_t * ent1, cl_entity_t * ent2, float * angle, float * origin)
{
	float newAngle[3]; float newOrigin[3]; 

	float distance = 168.0f;

	v_lastDistance+= v_frametime * 96.0f;	// move unit per seconds back

	if ( v_resetCamera )
		v_lastDistance = 64.0f;

	if ( distance > v_lastDistance )
		distance = v_lastDistance;

	VectorCopy(ent1->origin, newOrigin);

	if ( ent1->player )
		newOrigin[2]+= 17; // head level of living player

	// get new angle towards second target
	if ( ent2 )
	{
		VectorSubtract( ent2->origin, ent1->origin, newAngle );
		VectorAngles( newAngle, newAngle );
		newAngle[0] = -newAngle[0];
	}
	else
	{
		// if no second target is given, look down to dead player
		newAngle[0] = 90.0f;
		newAngle[1] = 0.0f;
		newAngle[2] = 0;
	}

	// and smooth view
	V_SmoothInterpolateAngles( v_lastAngles, newAngle, angle, 120.0f );
			
	V_GetChaseOrigin( angle, newOrigin, distance, origin );

	VectorCopy(angle, v_lastAngles);
}*/

void V_GetSingleTargetCam(cl_entity_t* ent1, float* angle, float* origin)
{
	float newAngle[3];
	float newOrigin[3];

	int flags = gHUD.m_Spectator.m_iObserverFlags;

	// see is target is a dead player
	bool deadPlayer = 0 != ent1->player && (ent1->curstate.solid == SOLID_NOT);

	float dfactor = (flags & DRC_FLAG_DRAMATIC) != 0 ? -1.0f : 1.0f;

	float distance = 112.0f + (16.0f * dfactor); // get close if dramatic;

	// go away in final scenes or if player just died
	if ((flags & DRC_FLAG_FINAL) != 0)
		distance *= 2.0f;
	else if (deadPlayer)
		distance *= 1.5f;

	// let v_lastDistance float smoothly away
	v_lastDistance += v_frametime * 32.0f; // move unit per seconds back

	if (distance > v_lastDistance)
		distance = v_lastDistance;

	VectorCopy(ent1->origin, newOrigin);

	if (0 != ent1->player)
	{
		if (deadPlayer)
			newOrigin[2] += 2; //laying on ground
		else
			newOrigin[2] += 17; // head level of living player
	}
	else
		newOrigin[2] += 8; // object, tricky, must be above bomb in CS

	// we have no second target, choose view direction based on
	// show front of primary target
	VectorCopy(ent1->angles, newAngle);

	// show dead players from front, normal players back
	if ((flags & DRC_FLAG_FACEPLAYER) != 0)
		newAngle[1] += 180.0f;


	newAngle[0] += 12.5f * dfactor; // lower angle if dramatic

	// if final scene (bomb), show from real high pos
	if ((flags & DRC_FLAG_FINAL) != 0)
		newAngle[0] = 22.5f;

	// choose side of object/player
	if ((flags & DRC_FLAG_SIDE) != 0)
		newAngle[1] += 22.5f;
	else
		newAngle[1] -= 22.5f;

	V_SmoothInterpolateAngles(v_lastAngles, newAngle, angle, 120.0f);

	// HACK, if player is dead don't clip against his dead body, can't check this
	V_GetChaseOrigin(angle, newOrigin, distance, origin);
}

float MaxAngleBetweenAngles(float* a1, float* a2)
{
	float d, maxd = 0.0f;

	NormalizeAngles(a1);
	NormalizeAngles(a2);

	for (int i = 0; i < 3; i++)
	{
		d = a2[i] - a1[i];
		if (d > 180)
		{
			d -= 360;
		}
		else if (d < -180)
		{
			d += 360;
		}

		d = fabs(d);

		if (d > maxd)
			maxd = d;
	}

	return maxd;
}

void V_GetDoubleTargetsCam(cl_entity_t* ent1, cl_entity_t* ent2, float* angle, float* origin)
{
	float newAngle[3];
	float newOrigin[3];
	float tempVec[3];

	int flags = gHUD.m_Spectator.m_iObserverFlags;

	float dfactor = (flags & DRC_FLAG_DRAMATIC) != 0 ? -1.0f : 1.0f;

	float distance = 112.0f + (16.0f * dfactor); // get close if dramatic;

	// go away in final scenes or if player just died
	if ((flags & DRC_FLAG_FINAL) != 0)
		distance *= 2.0f;

	// let v_lastDistance float smoothly away
	v_lastDistance += v_frametime * 32.0f; // move unit per seconds back

	if (distance > v_lastDistance)
		distance = v_lastDistance;

	VectorCopy(ent1->origin, newOrigin);

	if (0 != ent1->player)
		newOrigin[2] += 17; // head level of living player
	else
		newOrigin[2] += 8; // object, tricky, must be above bomb in CS

	// get new angle towards second target
	VectorSubtract(ent2->origin, ent1->origin, newAngle);

	VectorAngles(newAngle, newAngle);
	newAngle[0] = -newAngle[0];

	// set angle diffrent in Dramtaic scenes
	newAngle[0] += 12.5f * dfactor; // lower angle if dramatic

	if ((flags & DRC_FLAG_SIDE) != 0)
		newAngle[1] += 22.5f;
	else
		newAngle[1] -= 22.5f;

	float d = MaxAngleBetweenAngles(v_lastAngles, newAngle);

	if ((d < v_cameraFocusAngle) && (v_cameraMode == CAM_MODE_RELAX))
	{
		// difference is to small and we are in relax camera mode, keep viewangles
		VectorCopy(v_lastAngles, newAngle);
	}
	else if ((d < v_cameraRelaxAngle) && (v_cameraMode == CAM_MODE_FOCUS))
	{
		// we catched up with our target, relax again
		v_cameraMode = CAM_MODE_RELAX;
	}
	else
	{
		// target move too far away, focus camera again
		v_cameraMode = CAM_MODE_FOCUS;
	}

	// and smooth view, if not a scene cut
	if (v_resetCamera || (v_cameraMode == CAM_MODE_RELAX))
	{
		VectorCopy(newAngle, angle);
	}
	else
	{
		V_SmoothInterpolateAngles(v_lastAngles, newAngle, angle, 180.0f);
	}

	V_GetChaseOrigin(newAngle, newOrigin, distance, origin);

	// move position up, if very close at target
	if (v_lastDistance < 64.0f)
		origin[2] += 16.0f * (1.0f - (v_lastDistance / 64.0f));

	// calculate angle to second target
	VectorSubtract(ent2->origin, origin, tempVec);
	VectorAngles(tempVec, tempVec);
	tempVec[0] = -tempVec[0];

	/* take middle between two viewangles
	InterpolateAngles( newAngle, tempVec, newAngle, 0.5f); */
}

void V_GetDirectedChasePosition(cl_entity_t* ent1, cl_entity_t* ent2, float* angle, float* origin)
{

	if (v_resetCamera)
	{
		v_lastDistance = 4096.0f;
		// v_cameraMode = CAM_MODE_FOCUS;
	}

	if ((ent2 == (cl_entity_t*)0xFFFFFFFF) || (0 != ent1->player && (ent1->curstate.solid == SOLID_NOT)))
	{
		// we have no second target or player just died
		V_GetSingleTargetCam(ent1, angle, origin);
	}
	else if (ent2)
	{
		// keep both target in view
		V_GetDoubleTargetsCam(ent1, ent2, angle, origin);
	}
	else
	{
		// second target disappeard somehow (dead)

		// keep last good viewangle
		float newOrigin[3];

		int flags = gHUD.m_Spectator.m_iObserverFlags;

		float dfactor = (flags & DRC_FLAG_DRAMATIC) != 0 ? -1.0f : 1.0f;

		float distance = 112.0f + (16.0f * dfactor); // get close if dramatic;

		// go away in final scenes or if player just died
		if ((flags & DRC_FLAG_FINAL) != 0)
			distance *= 2.0f;

		// let v_lastDistance float smoothly away
		v_lastDistance += v_frametime * 32.0f; // move unit per seconds back

		if (distance > v_lastDistance)
			distance = v_lastDistance;

		VectorCopy(ent1->origin, newOrigin);

		if (0 != ent1->player)
			newOrigin[2] += 17; // head level of living player
		else
			newOrigin[2] += 8; // object, tricky, must be above bomb in CS

		V_GetChaseOrigin(angle, newOrigin, distance, origin);
	}

	VectorCopy(angle, v_lastAngles);
}

void V_GetChasePos(int target, float* cl_angles, float* origin, float* angles)
{
	cl_entity_t* ent = NULL;

	if (0 != target)
	{
		ent = gEngfuncs.GetEntityByIndex(target);
	}

	if (!ent)
	{
		// just copy a save in-map position
		VectorCopy(vJumpAngles, angles);
		VectorCopy(vJumpOrigin, origin);
		return;
	}



	if (0 != gHUD.m_Spectator.m_autoDirector->value)
	{
		if (0 != g_iUser3)
			V_GetDirectedChasePosition(ent, gEngfuncs.GetEntityByIndex(g_iUser3),
				angles, origin);
		else
			V_GetDirectedChasePosition(ent, (cl_entity_t*)0xFFFFFFFF,
				angles, origin);
	}
	else
	{
		if (cl_angles == NULL) // no mouse angles given, use entity angles ( locked mode )
		{
			VectorCopy(ent->angles, angles);
			angles[0] *= -1;
		}
		else
			VectorCopy(cl_angles, angles);


		VectorCopy(ent->origin, origin);

		VectorAdd(origin, VEC_VIEW, origin); // some offset

		V_GetChaseOrigin(angles, origin, cl_chasedist->value, origin);
	}

	v_resetCamera = false;
}

void V_ResetChaseCam()
{
	v_resetCamera = true;
}


void V_GetInEyePos(int target, float* origin, float* angles)
{
	if (0 == target)
	{
		// just copy a save in-map position
		VectorCopy(vJumpAngles, angles);
		VectorCopy(vJumpOrigin, origin);
		return;
	}


	cl_entity_t* ent = gEngfuncs.GetEntityByIndex(target);

	if (!ent)
		return;

	VectorCopy(ent->origin, origin);
	VectorCopy(ent->angles, angles);

	angles[PITCH] *= -3.0f; // see CL_ProcessEntityUpdate()

	if (ent->curstate.solid == SOLID_NOT)
	{
		angles[ROLL] = 80; // dead view angle
		VectorAdd(origin, VEC_DEAD_VIEW, origin);
	}
	else if (ent->curstate.usehull == 1)
	{
		VectorAdd(origin, VEC_DUCK_VIEW, origin);
	}
	else
		// exacty eye position can't be caluculated since it depends on
		// client values like cl_bobcycle, this offset matches the default values
		VectorAdd(origin, VEC_VIEW, origin);
}

void V_GetMapFreePosition(float* cl_angles, float* origin, float* angles)
{
	Vector forward;
	Vector zScaledTarget;

	VectorCopy(cl_angles, angles);

	// modify angles since we don't wanna see map's bottom
	angles[0] = 51.25f + 38.75f * (angles[0] / 90.0f);

	zScaledTarget[0] = gHUD.m_Spectator.m_mapOrigin[0];
	zScaledTarget[1] = gHUD.m_Spectator.m_mapOrigin[1];
	zScaledTarget[2] = gHUD.m_Spectator.m_mapOrigin[2] * ((90.0f - angles[0]) / 90.0f);


	AngleVectors(angles, forward, NULL, NULL);

	VectorNormalize(forward);

	VectorMA(zScaledTarget, -(4096.0f / gHUD.m_Spectator.m_mapZoom), forward, origin);
}

void V_GetMapChasePosition(int target, float* cl_angles, float* origin, float* angles)
{
	Vector forward;

	if (0 != target)
	{
		cl_entity_t* ent = gEngfuncs.GetEntityByIndex(target);

		if (0 != gHUD.m_Spectator.m_autoDirector->value)
		{
			// this is done to get the angles made by director mode
			V_GetChasePos(target, cl_angles, origin, angles);
			VectorCopy(ent->origin, origin);

			// keep fix chase angle horizontal
			angles[0] = 45.0f;
		}
		else
		{
			VectorCopy(cl_angles, angles);
			VectorCopy(ent->origin, origin);

			// modify angles since we don't wanna see map's bottom
			angles[0] = 51.25f + 38.75f * (angles[0] / 90.0f);
		}
	}
	else
	{
		// keep out roaming position, but modify angles
		VectorCopy(cl_angles, angles);
		angles[0] = 51.25f + 38.75f * (angles[0] / 90.0f);
	}

	origin[2] *= ((90.0f - angles[0]) / 90.0f);
	angles[2] = 0.0f; // don't roll angle (if chased player is dead)

	AngleVectors(angles, forward, NULL, NULL);

	VectorNormalize(forward);

	VectorMA(origin, -1536, forward, origin);
}

int V_FindViewModelByWeaponModel(int weaponindex)
{

	static const char* modelmap[][2] = {

		{"models/p_crossbow.mdl", "models/v_crossbow.mdl"},
		{"models/p_crowbar.mdl", "models/v_crowbar.mdl"},
		{"models/p_egon.mdl", "models/v_egon.mdl"},
		{"models/p_gauss.mdl", "models/v_gauss.mdl"},
		{"models/p_9mmhandgun.mdl", "models/v_9mmhandgun.mdl"},
		{"models/p_grenade.mdl", "models/v_grenade.mdl"},
		{"models/p_hgun.mdl", "models/v_hgun.mdl"},
		{"models/p_9mmAR.mdl", "models/v_9mmAR.mdl"},
		{"models/p_357.mdl", "models/v_357.mdl"},
		{"models/p_rpg.mdl", "models/v_rpg.mdl"},
		{"models/p_shotgun.mdl", "models/v_shotgun.mdl"},
		{"models/p_squeak.mdl", "models/v_squeak.mdl"},
		{"models/p_tripmine.mdl", "models/v_tripmine.mdl"},
		{"models/p_satchel_radio.mdl", "models/v_satchel_radio.mdl"},
		{"models/p_satchel.mdl", "models/v_satchel.mdl"},
		{NULL, NULL}};

	struct model_s* weaponModel = IEngineStudio.GetModelByIndex(weaponindex);

	if (weaponModel)
	{
		int len = strlen(weaponModel->name);
		int i = 0;

		while (modelmap[i] != NULL)
		{
			if (!strnicmp(weaponModel->name, modelmap[i][0], len))
			{
				return gEngfuncs.pEventAPI->EV_FindModelIndex(modelmap[i][1]);
			}
			i++;
		}

		return 0;
	}
	else
		return 0;
}


// orig client 0xeba10. SDK 2.3's V_GetInEyePos, as SDK 2.2 had it: the pitch scaled by -pi (SDK
// 2.3 has -3).
void V_GetInEyePosition(int target, float* origin, float* angles) // name ours
{
	cl_entity_t* ent = gEngfuncs.GetEntityByIndex(target);

	if (!ent || 0 == ent->player || !g_PlayerInfoList[target].name)
		return;

	VectorCopy(ent->origin, origin);
	VectorCopy(ent->angles, angles);

	angles[PITCH] *= -3.1415927f;

	if (ent->curstate.solid == SOLID_NOT)
	{
		angles[ROLL] = 80; // dead view angle
		origin[2] += -8;   // PM_DEAD_VIEWHEIGHT
	}
	else if (ent->curstate.usehull == 1)
		origin[2] += 12; // VEC_DUCK_VIEW;
	else
		// exacty eye position can't be caluculated since it depends on
		// client values like cl_bobcycle, this offset matches the default values
		origin[2] += 28; // DEFAULT_VIEWHEIGHT
}

/*
==================
V_CalcSpectatorRefdef

==================
*/
// orig client 0xebb00. SDK 2.2's: the main view (in eye, the free overview, or free), then the
// inset when there is one; RC2 holds and smooths the angles and origin as in the normal view.
void V_CalcSpectatorRefdef(struct ref_params_s* pparams)
{
	static viewinterp_t ViewAngleInterp;
	static viewinterp_t ViewOriginInterp;
	static Vector lastangles;
	static Vector lastorg;
	static float lasttime;
	int i;
	Vector delta;

	gEngfuncs.GetLocalPlayer();
	cl_entity_t* view = gEngfuncs.GetViewModel();

	VectorCopy(pparams->simorg, pparams->vieworg);
	if (0 == g_bLockViewOrigin)
	{
		VectorCopy(pparams->simorg, g_vecLockedViewOrigin);
	}
	else
	{
		VectorCopy(g_vecLockedViewOrigin, pparams->vieworg);
	}

	VectorCopy(pparams->cl_viewangles, pparams->viewangles);
	if (0 == g_bLockViewAngles)
	{
		VectorCopy(pparams->cl_viewangles, g_vecLockedViewAngles);
	}
	else
	{
		VectorCopy(g_vecLockedViewAngles, pparams->viewangles);
	}

	if (0 == pparams->nextView)
	{
		VectorSubtract(pparams->viewangles, lastangles, delta);
		if (Length(delta) != 0.0)
		{
			VectorCopy(pparams->viewangles, ViewAngleInterp.Origins[ViewAngleInterp.CurrentOrigin & ORIGIN_MASK]);
			ViewAngleInterp.OriginTime[ViewAngleInterp.CurrentOrigin & ORIGIN_MASK] = pparams->time;
			ViewAngleInterp.CurrentOrigin++;
			VectorCopy(pparams->viewangles, lastangles);
		}

		if (cl_vsmoothing && 0 != cl_vsmoothing->value && (g_iViewLock & 2) != 0)
		{
			float t = pparams->time - cl_vsmoothing->value;
			int foundidx;
			for (i = 1; i < ORIGIN_MASK; i++)
			{
				foundidx = ViewAngleInterp.CurrentOrigin - 1 - i;
				if (ViewAngleInterp.OriginTime[foundidx & ORIGIN_MASK] <= t)
					break;
			}

			if (i < ORIGIN_MASK && ViewAngleInterp.OriginTime[foundidx & ORIGIN_MASK] != 0.0)
			{
				Vector f0, f1;
				AngleVectors(ViewAngleInterp.Origins[foundidx & ORIGIN_MASK], f0, NULL, NULL);
				AngleVectors(ViewAngleInterp.Origins[(foundidx + 1) & ORIGIN_MASK], f1, NULL, NULL);
				float flAngle = AngleBetweenVectors(f0, f1);

				float dt = ViewAngleInterp.OriginTime[(foundidx + 1) & ORIGIN_MASK] - ViewAngleInterp.OriginTime[foundidx & ORIGIN_MASK];

				// don't smooth big turns
				if (dt > 0.0 && flAngle < 22.5)
				{
					float frac = (t - ViewAngleInterp.OriginTime[foundidx & ORIGIN_MASK]) / dt;
					frac = V_min(1.0f, frac);
					InterpolateAngles(ViewAngleInterp.Origins[foundidx & ORIGIN_MASK], ViewAngleInterp.Origins[(foundidx + 1) & ORIGIN_MASK], g_vecLockedViewAngles, frac);
					VectorCopy(g_vecLockedViewAngles, pparams->viewangles);
				}
			}
		}

		VectorSubtract(pparams->vieworg, lastorg, delta);
		if (Length(delta) != 0.0)
		{
			VectorCopy(pparams->vieworg, ViewOriginInterp.Origins[ViewOriginInterp.CurrentOrigin & ORIGIN_MASK]);
			ViewOriginInterp.OriginTime[ViewOriginInterp.CurrentOrigin & ORIGIN_MASK] = pparams->time;
			ViewOriginInterp.CurrentOrigin++;
			VectorCopy(pparams->vieworg, lastorg);
		}

		if (cl_vsmoothing && 0 != cl_vsmoothing->value && (g_iViewLock & 4) != 0)
		{
			float t = pparams->time - cl_vsmoothing->value;
			int foundidx;
			for (i = 1; i < ORIGIN_MASK; i++)
			{
				foundidx = ViewOriginInterp.CurrentOrigin - 1 - i;
				if (ViewOriginInterp.OriginTime[foundidx & ORIGIN_MASK] <= t)
					break;
			}

			if (i < ORIGIN_MASK && ViewOriginInterp.OriginTime[foundidx & ORIGIN_MASK] != 0.0)
			{
				float dt = ViewOriginInterp.OriginTime[(foundidx + 1) & ORIGIN_MASK] - ViewOriginInterp.OriginTime[foundidx & ORIGIN_MASK];
				if (dt > 0.0)
				{
					Vector neworg;
					float frac = (t - ViewOriginInterp.OriginTime[foundidx & ORIGIN_MASK]) / dt;
					frac = V_min(1.0f, frac);
					VectorSubtract(ViewOriginInterp.Origins[(foundidx + 1) & ORIGIN_MASK], ViewOriginInterp.Origins[foundidx & ORIGIN_MASK], delta);
					VectorMA(ViewOriginInterp.Origins[foundidx & ORIGIN_MASK], frac, delta, neworg);

					// Dont interpolate large changes
					if (Length(delta) < 64)
					{
						for (i = 0; i < 3; i++)
							pparams->vieworg[i] += neworg[i] - pparams->simorg[i];
						VectorCopy(pparams->vieworg, g_vecLockedViewOrigin);
					}
				}
			}
		}
	}

	lasttime = pparams->time;
	view->model = NULL;
	g_bMainView = true;

	auto MapFree = [pparams]()
	{
		pparams->onlyClientDraw = 1;
		g_bMainView = false;
		gHUD.m_Spectator.m_mapAngles[0] = pparams->cl_viewangles[0] * (1.0f / 90) * 38.75 + 51.25;
		gHUD.m_Spectator.m_mapAngles[1] = pparams->cl_viewangles[1];
		gHUD.m_Spectator.m_mapAngles[2] = pparams->cl_viewangles[2];
		Vector angles = gHUD.m_Spectator.m_mapAngles;
		gHUD.m_Spectator.GetMapPosition(pparams->vieworg);
		VectorCopy(angles, pparams->viewangles);
	};

	if (0 == pparams->nextView)
	{
		// first renderer cycle, full screen
		if (gHUD.m_Spectator.m_iMainMode == 4) // in eye
			V_GetInEyePosition(gHUD.m_Spectator.m_iTrackedPlayer, pparams->vieworg, pparams->viewangles);
		else if (gHUD.m_Spectator.m_iMainMode == 5) // free overview
			MapFree();
		else
			pparams->onlyClientDraw = 0;

		if (0 != gHUD.m_Spectator.m_iInsetMode)
			pparams->nextView = 1; // force a second renderer cycle

		gHUD.m_Spectator.m_iDrawCycle = 0;
	}
	else
	{
		// second renderer cycle, the inset
		pparams->viewport[0] = XRES(gHUD.m_Spectator.m_OverviewData.insetWindowX);
		pparams->viewport[1] = YRES(gHUD.m_Spectator.m_OverviewData.insetWindowY);
		pparams->viewport[2] = XRES(gHUD.m_Spectator.m_OverviewData.insetWindowWidth);
		pparams->viewport[3] = YRES(gHUD.m_Spectator.m_OverviewData.insetWindowHeight);
		pparams->nextView = 0;
		pparams->onlyClientDraw = 0;

		if (gHUD.m_Spectator.m_iInsetMode == 2) // in eye
			V_GetInEyePosition(gHUD.m_Spectator.m_iTrackedPlayer, pparams->vieworg, pparams->viewangles);
		else if (gHUD.m_Spectator.m_iInsetMode == 3) // free overview
			MapFree();

		gHUD.m_Spectator.m_iDrawCycle = 1;
	}

	v_angles = pparams->viewangles;
	v_origin = pparams->vieworg;
}

//==========================================================================
// Rocket Crowbar 2's screen tint, which fades from one colour and alpha to another over a time;
// a flash (black), and two steady tints, take precedence. Names ours.
static bool g_bScreenTint;			  // 0x10227998
static bool g_bScreenTintSet;		  // 0x10227999: a tint or flash was applied last frame
bool g_bClearScreenTint;			  // 0x1022799a: set elsewhere to clear it once
static int g_iScreenTintPriority = -999; // 0x101768bc
static Vector g_vecScreenTint;		  // 0x102270a8
static Vector g_vecScreenTintRate;	  // 0x102270b8
static float g_flScreenTintAlpha;	  // 0x10227154
static float g_flScreenTintAlphaRate; // 0x10226858
static float g_flScreenTintTime;	  // 0x10227070
extern float g_flScreenFlash;		  // name ours: 0 to 1, a black flash

// orig client 0xec560
void V_ResetScreenTint() // name ours
{
	g_iScreenTintPriority = -999;
	g_bScreenTint = false;
}

// orig client 0xec590. Not while the magenta tint is on, unless its priority is above 0.
void V_StartScreenTint(float r1, float g1, float b1, float r2, float g2, float b2, float a1, float a2, float flTime, int iPriority) // name ours
{
	if (g_bScreenTint && iPriority < g_iScreenTintPriority)
		return;
	if (0 != g_bScreenTintMagenta && iPriority <= 0)
		return;

	g_vecScreenTint = Vector(r1, g1, b1);
	g_bScreenTint = true;
	g_iScreenTintPriority = iPriority;
	g_vecScreenTintRate = Vector((r2 - r1) / flTime, (g2 - g1) / flTime, (b2 - b1) / flTime);
	g_flScreenTintAlphaRate = (a2 - a1) / flTime;
	g_flScreenTintAlpha = a1;
	g_flScreenTintTime = flTime;
}

static void V_SetScreenTint(int r, int g, int b, int a) // name ours
{
	screenfade_t sf;
	gEngfuncs.pfnGetScreenFade(&sf);
	sf.fader = r;
	sf.fadeg = g;
	sf.fadeb = b;
	sf.fadealpha = a;
	sf.fadeFlags = FFADE_OUT | FFADE_STAYOUT;
	gEngfuncs.pfnSetScreenFade(&sf);
}

// orig client 0xec6c0. Called after every view; each colour is truncated to a byte (the
// original's clamp to 255 compares the byte, so it never applies).
void V_UpdateScreenTint(float flFrameTime) // name ours
{
	if (g_flScreenFlash > 0)
	{
		float a = g_flScreenFlash;
		if (a < 0)
			a = 0;
		else if (a > 1)
			a = 1;
		V_SetScreenTint(0, 0, 0, (byte)(int)(a * 255.0));
		g_bScreenTintSet = true;
		return;
	}

	if (0 != g_bScreenTintBlue)
	{
		V_SetScreenTint(130, 130, 255, 100);
		g_bScreenTintSet = true;
		return;
	}

	if (!g_bScreenTint)
	{
		if (g_bScreenTintSet)
		{
			V_SetScreenTint(0, 0, 0, 0);
			g_bScreenTintSet = false;
		}

		if (0 != g_bScreenTintMagenta)
		{
			V_SetScreenTint(255, 0, 255, 72);
			g_bScreenTintSet = true;
			return;
		}

		if (g_bClearScreenTint)
		{
			V_SetScreenTint(0, 0, 0, 0);
			g_bClearScreenTint = false;
		}
		return;
	}

	V_SetScreenTint((byte)(int)g_vecScreenTint[0], (byte)(int)g_vecScreenTint[1], (byte)(int)g_vecScreenTint[2], (byte)(int)g_flScreenTintAlpha);

	g_vecScreenTint = g_vecScreenTint + g_vecScreenTintRate * flFrameTime;
	g_flScreenTintAlpha += g_flScreenTintAlphaRate * flFrameTime;
	g_flScreenTintTime -= flFrameTime;

	if (g_flScreenTintTime < 0)
	{
		V_SetScreenTint((byte)(int)g_vecScreenTint[0], (byte)(int)g_vecScreenTint[1], (byte)(int)g_vecScreenTint[2], 0);
		g_bScreenTint = false;
	}
}

// orig client 0xecb10. SDK 2.1's choice (spectators by pparams->spectator only), then RC2's
// screen tint after every view.
void DLLEXPORT V_CalcRefdef(struct ref_params_s* pparams)
{
	//	RecClCalcRefdef(pparams);

	// intermission / finale rendering
	if (0 != pparams->intermission)
	{
		V_CalcIntermissionRefdef(pparams);
	}
	else if (0 != pparams->spectator)
	{
		V_CalcSpectatorRefdef(pparams);
	}
	else if (0 == pparams->paused)
	{
		V_CalcNormalRefdef(pparams);
	}

	V_UpdateScreenTint(pparams->frametime);
}

/*
=============
V_DropPunchAngle

=============
*/
// orig client 0xecb90
void V_DropPunchAngle(float frametime, float* ev_punchangle)
{
	float len;

	len = VectorNormalize(ev_punchangle);
	len -= (10.0 + len * 0.5) * frametime;
	len = V_max(len, 0.0f);
	VectorScale(ev_punchangle, len, ev_punchangle);
}

/*
=============
V_PunchAxis

Client side punch effect
=============
*/
void V_PunchAxis(int axis, float punch)
{
	ev_punchangle[axis] = punch;
}

/*
=============
V_Init
=============
*/
// orig client 0xecc20. SDK 2.1's cvars (no cl_chasedist; cl_bob not archived), after resetting
// RC2's tint and shake.
void V_Init()
{
	V_ResetScreenTint();
	V_ResetViewShake();

	gEngfuncs.pfnAddCommand("centerview", V_StartPitchDrift);

	scr_ofsx = gEngfuncs.pfnRegisterVariable("scr_ofsx", "0", 0);
	scr_ofsy = gEngfuncs.pfnRegisterVariable("scr_ofsy", "0", 0);
	scr_ofsz = gEngfuncs.pfnRegisterVariable("scr_ofsz", "0", 0);

	v_centermove = gEngfuncs.pfnRegisterVariable("v_centermove", "0.15", 0);
	v_centerspeed = gEngfuncs.pfnRegisterVariable("v_centerspeed", "500", 0);

	cl_bobcycle = gEngfuncs.pfnRegisterVariable("cl_bobcycle", "0.8", 0); // best default for my experimental gun wag (sjb)
	cl_bob = gEngfuncs.pfnRegisterVariable("cl_bob", "0.01", 0);		   // best default for my experimental gun wag (sjb)
	cl_bobup = gEngfuncs.pfnRegisterVariable("cl_bobup", "0.5", 0);
	cl_waterdist = gEngfuncs.pfnRegisterVariable("cl_waterdist", "4", 0);
}


//#define TRACE_TEST
#if defined(TRACE_TEST)

extern float in_fov;
/*
====================
CalcFov
====================
*/
float CalcFov(float fov_x, float width, float height)
{
	float a;
	float x;

	if (fov_x < 1 || fov_x > 179)
		fov_x = 90; // error, set to 90

	x = width / tan(fov_x / 360 * M_PI);

	a = atan(height / x);

	a = a * 360 / M_PI;

	return a;
}

int hitent = -1;

void V_Move(int mx, int my)
{
	float fov;
	float fx, fy;
	float dx, dy;
	float c_x, c_y;
	float dX, dY;
	Vector forward, up, right;
	Vector newangles;

	Vector farpoint;
	pmtrace_t tr;

	fov = CalcFov(in_fov, (float)ScreenWidth, (float)ScreenHeight);

	c_x = (float)ScreenWidth / 2.0;
	c_y = (float)ScreenHeight / 2.0;

	dx = (float)mx - c_x;
	dy = (float)my - c_y;

	// Proportion we moved in each direction
	fx = dx / c_x;
	fy = dy / c_y;

	dX = fx * in_fov / 2.0;
	dY = fy * fov / 2.0;

	newangles = v_angles;

	newangles[YAW] -= dX;
	newangles[PITCH] += dY;

	// Now rotate v_forward around that point
	AngleVectors(newangles, forward, right, up);

	farpoint = v_origin + 8192 * forward;

	// Trace
	tr = *(gEngfuncs.PM_TraceLine((float*)&v_origin, (float*)&farpoint, PM_TRACELINE_PHYSENTSONLY, 2 /*point sized hull*/, -1));

	if (tr.fraction != 1.0 && tr.ent != 0)
	{
		hitent = PM_GetPhysEntInfo(tr.ent);
		PM_ParticleLine((float*)&v_origin, (float*)&tr.endpos, 5, 1.0, 0.0);
	}
	else
	{
		hitent = -1;
	}
}

#endif


// TEMP STUB (c5): real ones belong in RC2's client code elsewhere (hud.cpp's ViewMode message,
// the camera and player state code; packages c1 to c4). Names ours. g_iViewMode is hud.cpp's.
int g_bThirdPersonBlocked = 0;
int g_iPlayerViewState = 0;
int g_iPlayerViewSubState = 0;
int g_bPlayerViewBusy = 0;
bool g_bScopeView = false;
Vector g_vecFixedViewAngles;
int g_bTiltBodyWithView = 1;
int g_bScreenTintBlue = 0;
int g_bScreenTintMagenta = 0;
int g_bViewOverride = 0;
float g_flScreenFlash = 0;
void VectorAnglesWithRoll(const float* forward, const float* right, const float* up, float* angles); // pm_math.cpp
void RC2_BasisToAngles(const float* forward, const float* right, const float* up, float* angles)
{
	VectorAnglesWithRoll(forward, right, up, angles);
}

// TEMP STUB (c5): real one belongs in util.cpp (SDK 2.3's, RC2's at client 0xb07d0, package c3)
float AngleBetweenVectors(const float* v1, const float* v2)
{
	float l1 = Length(v1);
	float l2 = Length(v2);
	if (0 == l1 || 0 == l2)
		return 0.0f;
	return (acos(DotProduct(v1, v2) / (l1 * l2)) * 180.0f) / M_PI;
}

