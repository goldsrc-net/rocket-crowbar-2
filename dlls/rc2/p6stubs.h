/***
*
*	Rocket Crowbar 2: what package p6 uses from other packages' files, declared once so the
*	p6 sources share one set of names. Every body is a TEMP STUB in p6stubs.cpp (not the
*	original's behavior); each comment says which original function it stands for and which
*	file's package owns it. Names are ours. The merge replaces these with the real ones.
*
****/

#pragma once

class CBaseEntity;

// util (rc2.dll 0xf3900): RC2's radius damage, with a falloff exponent. iLast: VERIFY
void RC2RadiusDamage(entvars_t* pevAttacker, entvars_t* pevInflictor, edict_t* pentSelf, Vector vecSrc, float flRadius,
	float flDamage, float flFalloffExponent, int bitsDamageType, int iLast);

// util (rc2.dll 0xf55b0): kills the client-side effects attached to an entity
void UTIL_RC2KillEntityEffects(edict_t* pent);

// util (rc2.dll 0xf5920): a unit vector packed into an int (10 bits per axis, 0x200 the sign),
// for an event's iparam
int UTIL_RC2PackNormal(Vector vecNormal);

// weapons.cpp W_Precache (orig .bss 0x1afd18): PRECACHE_EVENT(1, "events/explode.sc")
extern unsigned short g_usRC2ExplodeEvent;

// util (rc2.dll 0xf4c70, 0xf4d40, 0xf4d90, 0xf4dd0): a stack of per-entity "already hit" bit sets
// (maxEntities bits each, up to 128 deep), so one blast hurts an entity once even when it is
// reached through GetEffectedEnt from several others
void UTIL_RC2PushHitSet();
void UTIL_RC2PopHitSet();
void UTIL_RC2MarkHit(int iEntIndex);
bool UTIL_RC2WasHit(int iEntIndex);

// util (rc2.dll 0xf58a0): a shared dummy entity's pev renamed to szClassname, so the death
// notice names the effect; the world's pev when the dummy doesn't exist
entvars_t* UTIL_RC2DamageInflictor(const char* szClassname);

// util (rc2.dll 0xf4e70): an electric discharge through the water at vecSrc: every entity in
// water that the water connects to takes flDamage (through the shield)
void UTIL_RC2WaterDischarge(Vector vecSrc, float flDamage, entvars_t* pevInflictor, entvars_t* pevAttacker);

// the run between scripted and shotgun (rc2.dll 0xc45b0): what an entity's shield lets through of
// flDamage. pvecOut1/2 get positions/directions back (VERIFY), bitsDamageType as TakeDamage's
float RC2ShieldAbsorb(CBaseEntity* pEnt, float flDamage, float flShieldFraction, Vector vecHitPos, Vector vecDir,
	float* pvecOut1, float* pvecOut2, int bitsDamageType);

// the race controller (dlls/rc2/racecontroller.cpp): the race path point a player in a race
// is heading for. iPlayer is the player's entity index; nothing is written when the player
// isn't racing. "Height" is the controller's track_radius (CBasePlayer::m_flRaceTrackRadius).
void RacePath_GetCurrent(int iPlayer, Vector* pvecPoint, float* pflRadius);				// rc2.dll 0xae570
void RacePath_GetRelative(int iPlayer, int iOffset, Vector* pvecPoint, float* pflRadius); // rc2.dll 0xae660, wraps around
void RacePath_Advance(int iPlayer);														// rc2.dll 0xae760
void RacePath_GetHeight(int iPlayer, float* pflHeight);									// rc2.dll 0xae830: player member +0x1134

// UserMessages.cpp (orig .bss 0x18f4bc): REG_USER_MSG("PullerCtrl", -1)
extern int gmsgPullerCtrl;

// util (rc2.dll 0xf5900): push *pflTime out to flBase + flDelay unless it is already later
void UTIL_RC2ExtendTime(float* pflTime, float flBase, float flDelay);

// weapons.cpp (orig .data 0x181434, initially 1): set by every ItemPostFrame; the weapons
// play their events and clear their charge state only while it is set, as the client's
// prediction code tests its g_runfuncs (the client's own is in cl_dll/com_weapons.cpp)
extern bool g_runfuncs;
