/***
*
*	Rocket Crowbar 2: suck_grenade, the hand grenade's secondary throw.
*
*	A glowing grenade that bounces for [Grenade] SharedMaxTimer seconds, rises off the ground in
*	its last 0.75 s, then turns invisible and becomes a pull point (SuckGrenPullRadius,
*	...PlayerConstant, ...MonsterConstant) for SuckGrenDetonateTime seconds, drawn by the client
*	from RC2's CLTE message (type 52). It deals no damage itself.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "soundent.h"
#include "rc2prefs.h"
#include "rc2grenades.h"

LINK_ENTITY_TO_CLASS(suck_grenade, CSuckGrenade); // orig 0x59940

// orig 0x599c0: 2 while it hangs in the air (MOVETYPE_FLY), 0 before
int CSuckGrenade::GetRCCaps2(CBaseEntity* pOther, CBaseEntity* pAsker)
{
	return pev->movetype == MOVETYPE_FLY ? 2 : 0;
}

// orig 0x599e0
void CSuckGrenade::Precache()
{
	PRECACHE_MODEL("models/w_grenade.mdl");
	PRECACHE_MODEL("sprites/animglow01.spr");
	PRECACHE_SOUND("weapons/grenade_hit1.wav");
	PRECACHE_SOUND("weapons/grenade_hit2.wav");
	PRECACHE_SOUND("weapons/grenade_hit3.wav");
	PRECACHE_SOUND("ambience/alien_blipper.wav");
}

// orig 0x59a30
void CSuckGrenade::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	m_iPullPoint = -1;

	SET_MODEL(ENT(pev), "models/w_grenade.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	pev->classname = MAKE_STRING("suck_grenade");
	pev->dmg_inflictor = pev->owner;

	SetTouch(&CGrenade::BounceTouch);
	SetThink(&CSuckGrenade::PreDetonate);
	pev->nextthink = gpGlobals->time;
	pev->dmgtime = RC2Pref(RC2P_Grenade_SharedMaxTimer).f + gpGlobals->time;

	pev->renderfx = kRenderFxGlowShell;
	pev->rendercolor = Vector(255, 255, 255);
	pev->renderamt = 16;

	pev->sequence = RANDOM_LONG(3, 6);
	ResetSequenceInfo();
	pev->framerate = 1.0;
	pev->gravity = RC2Pref(RC2P_Grenade_SuckGrenGravityScaler).f;
	pev->friction = RC2Pref(RC2P_Grenade_SuckGrenFrictionScaler).f;
	m_fRegisteredSound = false;
}

// orig 0x59ba0
void CSuckGrenade::PreDetonate()
{
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance();

	if (pev->dmgtime - 2.0 < gpGlobals->time)
		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin + pev->velocity * (pev->dmgtime - gpGlobals->time), 600, 0.1);

	if (pev->dmgtime - 0.75 < gpGlobals->time && (pev->flags & FL_ONGROUND) != 0)
	{
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = Vector(0, 0, RC2Pref(RC2P_Grenade_SuckGrenArmRiseHeight).f * 1.3333334f);
		pev->flags &= ~FL_ONGROUND;
		pev->origin.z += 1.0;
		UTIL_SetOrigin(pev, pev->origin);
		SetTouch(NULL);
		pev->sequence = RANDOM_LONG(3, 5);
		ResetSequenceInfo();
		pev->framerate = 3.0;
	}

	if (pev->dmgtime <= gpGlobals->time)
	{
		StartDetonate();
		return;
	}

	if (pev->waterlevel != 0 && pev->movetype != MOVETYPE_FLY)
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2;
	}
}

// orig 0x59dc0: vanish and start pulling
void CSuckGrenade::StartDetonate()
{
	pev->velocity = g_vecZero;
	pev->movetype = MOVETYPE_FLY;
	SetTouch(NULL);
	SetThink(&CSuckGrenade::Detonating);
	pev->nextthink = gpGlobals->time;
	pev->dmgtime = RC2Pref(RC2P_Grenade_SuckGrenDetonateTime).f + gpGlobals->time;
	pev->owner = NULL;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	m_iRC2Flags = 2; // VERIFY: what the flags word at orig +0x68 means to its readers

	m_iPullPoint = AddPullPoint(pev->origin, RC2Pref(RC2P_Grenade_SuckGrenPullRadius).f,
		RC2Pref(RC2P_Grenade_SuckGrenPullPlayerConstant).f, RC2Pref(RC2P_Grenade_SuckGrenPullMonsterConstant).f);
	CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, 900, RC2Pref(RC2P_Grenade_SuckGrenDetonateTime).f);

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(52); // the suck grenade's effect
	WRITE_SHORT(ENTINDEX(ENT(pev)));
	MESSAGE_END();
}

// orig 0x59ef0
void CSuckGrenade::Detonating()
{
	pev->nextthink = gpGlobals->time + 0.1;
	if (pev->dmgtime < gpGlobals->time)
		EndDetonate();
}

// orig 0x59f30
void CSuckGrenade::EndDetonate()
{
	RemovePullPoint(m_iPullPoint);

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(254); // end the effect
	WRITE_SHORT(ENTINDEX(ENT(pev)));
	WRITE_BYTE(52);
	MESSAGE_END();

	SetThink(NULL);
	SetTouch(NULL);
	UTIL_Remove(this);
}
