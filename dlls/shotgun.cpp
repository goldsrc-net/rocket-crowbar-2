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

// Rocket Crowbar 2's shotgun fires no buckshot: each attack launches one of the projectiles in
// dlls/rc2/shotgunprojectiles.cpp, costing clip shells. Reload and idle are SDK 2.3's, with the
// timings from rc2.ini's [Shotgun].

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "UserMessages.h"
#include "rc2prefs.h"
#include "rc2/p7_extern.h"

#ifdef CLIENT_DLL
extern bool g_runfuncs; // cl_dll/com_weapons.cpp
#define WEAPON_RUNFUNCS g_runfuncs
#else
#define WEAPON_RUNFUNCS (0 != g_iWeaponRunFuncs)
#endif

LINK_ENTITY_TO_CLASS(weapon_shotgun, CShotgun); // orig 0xc7300

void CShotgun::Spawn() // orig 0xc7370
{
	Precache();
	m_iId = WEAPON_SHOTGUN;
	SET_MODEL(ENT(pev), "models/w_shotgun.mdl");

	m_iDefaultAmmo = RC2Pref(RC2P_Ammo_ShotgunPickupGive).s;

	FallInit(); // get ready to fall
}

void CShotgun::Precache() // orig 0xc73b0
{
	PRECACHE_MODEL("models/v_shotgun.mdl");
	PRECACHE_MODEL("models/w_shotgun.mdl");
	PRECACHE_MODEL("models/p_shotgun.mdl");

	m_iShell = PRECACHE_MODEL("models/shotgunshell.mdl"); // shotgun shell

	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND("weapons/dbarrel1.wav");
	PRECACHE_SOUND("weapons/sbarrel1.wav");

	PRECACHE_SOUND("weapons/reload1.wav");
	PRECACHE_SOUND("weapons/reload3.wav");

	PRECACHE_SOUND("weapons/357_cock1.wav");
	PRECACHE_SOUND("weapons/scock1.wav");

	PRECACHE_SOUND("ambience/biggun2.wav");
	PRECACHE_SOUND("ambience/biggun3.wav");
	PRECACHE_SOUND("ambience/dronemachine1.wav");
	PRECACHE_SOUND("weapons/rocketfire1.wav");
	PRECACHE_SOUND("weapons/glauncher.wav");

	UTIL_PrecacheOther("shotgun_sci");
	UTIL_PrecacheOther("shotgun_crate");
	UTIL_PrecacheOther("shotgun_hev");
	UTIL_PrecacheOther("shotgun_forklift");

	m_usFireSci = PRECACHE_EVENT(1, "events/shotgunsci.sc");
	m_usFireSuit = PRECACHE_EVENT(1, "events/shotgunsuit.sc");
	m_usFireFL = PRECACHE_EVENT(1, "events/shotgunfl.sc");
	m_usFireCrate = PRECACHE_EVENT(1, "events/firesgcrate.sc");
	m_usCrateCharge = PRECACHE_EVENT(1, "events/sgcratecharge.sc");
}

// The original also sends WeapPickup here (SDK 2.3's AddToPlayer); halflife-updated's
// CBasePlayer::AddPlayerItem sends it instead.
void CShotgun::AddToPlayer(CBasePlayer* pPlayer) // orig 0xc7500
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
}

bool CShotgun::GetItemInfo(ItemInfo* p) // orig 0xc7560
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "buckshot";
	p->iMaxAmmo1 = 36; // a constant, not [Ammo] MaxCarryShells
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = RC2Pref(RC2P_Ammo_ShotgunClipSize).s;
	p->iSlot = 2;
	p->iPosition = 1;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_SHOTGUN;
	p->iWeight = 20;

	return true;
}

bool CShotgun::Deploy() // orig 0xc75d0
{
	m_fCrateCharging = false;
	m_fInAttack = 0;
	return DefaultDeploy("models/v_shotgun.mdl", "models/p_shotgun.mdl", SHOTGUN_DRAW, "shotgun");
}

bool CShotgun::CanHolster() // orig 0xc7600
{
	return 0 == m_fInAttack; // not while charging a crate
}

void CShotgun::Holster() // orig 0xc7610
{
	m_fCrateCharging = false;
	m_fInAttack = 0;
	CBasePlayerWeapon::Holster();
}

// RC2's player flag that lets the shotgun fire under water (player +0x708 bit 0x200, which the
// original reaches by entity index through 0x95860).
static bool CanFireUnderwater(CBasePlayer* pPlayer) // name ours
{
	return (pPlayer->m_iAbilities & 0x200) != 0;
}

// name ours: the head the first three attacks share. False when it can't fire; on the server
// *ppLaunched is the new projectile.
static bool ShotgunLaunch(CShotgun* pGun, const char* szClassname, int iAmmoCost, float flForwardOffset, bool fClampPitch, CBaseEntity** ppLaunched)
{
	CBasePlayer* pPlayer = pGun->m_pPlayer;
	*ppLaunched = NULL;

	if (pPlayer->pev->waterlevel == 3 && !CanFireUnderwater(pPlayer))
	{
		pGun->PlayEmptySound();
		UTIL_ExtendTime(pGun->m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(pGun->m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(pGun->m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(pGun->m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.15);
		return false;
	}

	if (pGun->m_iClip < iAmmoCost)
	{
		pGun->Reload();
		if (pGun->m_iClip == 0)
			pGun->PlayEmptySound();
		return false;
	}

	pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	pGun->m_iClip -= iAmmoCost;
	pPlayer->pev->effects |= EF_MUZZLEFLASH;

#ifndef CLIENT_DLL
	Vector vecSrc = pPlayer->GetGunPosition();
	if (fClampPitch)
	{
		Vector vecAngles = pPlayer->pev->v_angle;
		if (vecAngles.x < -80)
			vecAngles.x = -80;
		else if (vecAngles.x > 80)
			vecAngles.x = 80;
		UTIL_MakeVectors(vecAngles);
	}
	else
		UTIL_MakeVectors(pPlayer->pev->v_angle);

	if (flForwardOffset != 0)
		vecSrc = vecSrc + gpGlobals->v_forward * flForwardOffset;
	*ppLaunched = CBaseEntity::Create(szClassname, vecSrc, g_vecZero, pPlayer->edict());
#endif
	return true;
}

static void ShotgunLaunchOwner(CBasePlayer* pPlayer, CBaseEntity* pLaunched) // name ours
{
#ifndef CLIENT_DLL
	if (0 != pPlayer->m_iRidingIndex)
	{
		edict_t* pentOwner = INDEXENT(pPlayer->m_iRidingIndex);
		if (UTIL_IsValidEntity(pentOwner))
			pLaunched->pev->owner = pentOwner;
	}
#endif
}

// name ours: the shared tail, after the projectile is set up
static void ShotgunLaunchDone(CShotgun* pGun, unsigned short usEvent, float flRefirePrimary, float flRefireSecondary, float flRefireTertiary, float flRefireQuaternary)
{
	CBasePlayer* pPlayer = pGun->m_pPlayer;

	if (WEAPON_RUNFUNCS)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, pPlayer->edict(), usEvent, 0.0, g_vecZero, g_vecZero, 0, 0, 0, 0, false, false);

	if (0 == pGun->m_iClip && pPlayer->m_rgAmmo[pGun->m_iPrimaryAmmoType] <= 0)
		pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0); // HEV suit - indicate out of ammo condition

	if (0 != pGun->m_iClip)
		pGun->m_flPumpTime = gpGlobals->time + 0.5;

	UTIL_ExtendTime(pGun->m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), flRefirePrimary);
	UTIL_ExtendTime(pGun->m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), flRefireSecondary);
	UTIL_ExtendTime(pGun->m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), flRefireTertiary);
	UTIL_ExtendTime(pGun->m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), flRefireQuaternary);

	if (0 != pGun->m_iClip)
		pGun->m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
	else
		pGun->m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.75;
	pGun->m_fInSpecialReload = 0;
}

// The scientist: thrown feet first, with recoil.
void CShotgun::PrimaryAttack() // orig 0xc7630
{
	CBaseEntity* pSci;
	if (!ShotgunLaunch(this, "shotgun_sci", RC2Pref(RC2P_Shotgun_ScientistAmmoCost).s, 0, false, &pSci))
		return;
#ifndef CLIENT_DLL
	pSci->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Shotgun_ScientistLaunchVel).f;
	pSci->pev->velocity.z += RC2Pref(RC2P_Shotgun_ScientistLaunchZBoost).f;
	pSci->pev->owner = m_pPlayer->edict();
	pSci->pev->angles = UTIL_VecToAngles(pSci->pev->velocity);
	pSci->pev->angles.y -= 180;
	pSci->pev->angles.z -= 180;
	pSci->pev->angles.x = -pSci->pev->angles.x;
	ShotgunLaunchOwner(m_pPlayer, pSci);

	m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * RC2Pref(RC2P_Shotgun_ScientistRecoilVelocity).f;
#endif
	ShotgunLaunchDone(this, m_usFireSci, RC2Pref(RC2P_Shotgun_PrimaryRefirePrimary).f, RC2Pref(RC2P_Shotgun_PrimaryRefireSecondary).f,
		RC2Pref(RC2P_Shotgun_PrimaryRefireTertiary).f, RC2Pref(RC2P_Shotgun_PrimaryRefireQuaternary).f);
}

// The HEV suit: flies off under its own engine along the aim (pev->movedir).
void CShotgun::SecondaryAttack() // orig 0xc7a80
{
	CBaseEntity* pSuit;
	if (!ShotgunLaunch(this, "shotgun_hev", RC2Pref(RC2P_Shotgun_HEVAmmoCost).s, 8, false, &pSuit))
		return;
#ifndef CLIENT_DLL
	pSuit->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Shotgun_HEVLaunchVel).f;
	pSuit->pev->movedir = gpGlobals->v_forward;
	pSuit->pev->owner = m_pPlayer->edict();
	const float flRoll = pSuit->pev->angles.z;
	pSuit->pev->angles = UTIL_VecToAngles(pSuit->pev->velocity);
	pSuit->pev->angles.z = flRoll;
	ShotgunLaunchOwner(m_pPlayer, pSuit);

	m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * RC2Pref(RC2P_Shotgun_HEVRecoilVelocity).f;
#endif
	ShotgunLaunchDone(this, m_usFireSuit, RC2Pref(RC2P_Shotgun_SecondaryRefirePrimary).f, RC2Pref(RC2P_Shotgun_SecondaryRefireSecondary).f,
		RC2Pref(RC2P_Shotgun_SecondaryRefireTertiary).f, RC2Pref(RC2P_Shotgun_SecondaryRefireQuaternary).f);
}

// The mini forklift: aimed no steeper than 80 degrees, no recoil.
void CShotgun::TertiaryAttack() // orig 0xc7f20
{
	CBaseEntity* pForklift;
	if (!ShotgunLaunch(this, "shotgun_forklift", RC2Pref(RC2P_Shotgun_ForkliftAmmoCost).s, 0, true, &pForklift))
		return;
#ifndef CLIENT_DLL
	pForklift->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Shotgun_ForkliftLaunchVel).f;
	pForklift->pev->velocity.z += RC2Pref(RC2P_Shotgun_ForkliftLaunchZBoost).f;
	pForklift->pev->owner = m_pPlayer->edict();
	pForklift->pev->angles = UTIL_VecToAngles(pForklift->pev->velocity);
	pForklift->pev->movedir = gpGlobals->v_forward;
	ShotgunLaunchOwner(m_pPlayer, pForklift);
#endif
	ShotgunLaunchDone(this, m_usFireFL, RC2Pref(RC2P_Shotgun_TertiaryRefirePrimary).f, RC2Pref(RC2P_Shotgun_TertiaryRefireSecondary).f,
		RC2Pref(RC2P_Shotgun_TertiaryRefireTertiary).f, RC2Pref(RC2P_Shotgun_TertiaryRefireQuaternary).f);
}

// The crate: starts charging; WeaponFrame burns clip shells while it charges and fires it when
// [Shotgun] CrateChargeTime is up.
void CShotgun::QuaternaryAttack() // orig 0xc8340
{
	if (m_pPlayer->pev->waterlevel == 3 && !CanFireUnderwater(m_pPlayer))
	{
		PlayEmptySound();
		UTIL_ExtendTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.15);
		return;
	}

	if (m_iClip < RC2Pref(RC2P_Shotgun_CrateAmmoCost).s)
	{
		Reload();
		if (m_iClip == 0)
			PlayEmptySound();
	}
	else
	{
		m_fCrateCharging = true;
		m_fInAttack = 1;
		m_flCrateChargeEnd = RC2Pref(RC2P_Shotgun_CrateChargeTime).f + gpGlobals->time;

		if (WEAPON_RUNFUNCS)
			PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usCrateCharge, 0.0, g_vecZero, g_vecZero, 0, 0, 0, 0, false, false);

		m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 3.0;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 4.0;
		m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + 4.0;
		m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 7.0;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.75;
	}
}

void CShotgun::FireCrate() // orig 0xc84f0
{
	if (m_pPlayer->pev->waterlevel == 3 && !CanFireUnderwater(m_pPlayer))
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.15;
		m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + 0.15;
		m_fInSpecialReload = 0;
		m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 0.15;
		if (WEAPON_RUNFUNCS)
		{
			m_fCrateCharging = false;
			m_fInAttack = 0;
		}
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	if (WEAPON_RUNFUNCS)
	{
		m_fCrateCharging = false;
		m_fInAttack = 0;
	}
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

#ifndef CLIENT_DLL
	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	// VERIFY: the source read as GetGunPosition() + v_forward * 6 (the decompile garbles the sum)
	const Vector vecSrc = m_pPlayer->GetGunPosition() + gpGlobals->v_forward * 6;
	CBaseEntity* pCrate = CBaseEntity::Create("shotgun_crate", vecSrc, g_vecZero, m_pPlayer->edict());
	pCrate->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Shotgun_CrateLaunchVel).f;
	pCrate->pev->velocity.z = RC2Pref(RC2P_Shotgun_CrateLaunchZBoost).f + pCrate->pev->velocity.z;
	pCrate->pev->owner = m_pPlayer->edict();

	// tumbling (each RANDOM_FLOAT in the original's order: z, y, x)
	const float flAngZ = RANDOM_FLOAT(-90, 90);
	const float flAngY = RANDOM_FLOAT(0, 360);
	const float flAngX = RANDOM_FLOAT(-90, 90);
	pCrate->pev->angles = Vector(flAngX, flAngY, flAngZ);
	const float flSpinZ = RANDOM_FLOAT(-500, 500);
	const float flSpinY = RANDOM_FLOAT(500, 500); // sic
	const float flSpinX = RANDOM_FLOAT(-500, 500);
	pCrate->pev->avelocity = Vector(flSpinX, flSpinY, flSpinZ);

	ShotgunLaunchOwner(m_pPlayer, pCrate);

	m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * RC2Pref(RC2P_Shotgun_CrateRecoilVelocity).f;
#endif

	if (WEAPON_RUNFUNCS)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usFireCrate, 0.0, g_vecZero, g_vecZero, 0, 0, 0, 0, false, false);

	if (0 == m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 1)
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	m_flPumpTime = gpGlobals->time + 0.5;
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Shotgun_QuaternaryRefirePrimary).f;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Shotgun_QuaternaryRefireSecondary).f;
	m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Shotgun_QuaternaryRefireTertiary).f;
	m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Shotgun_QuaternaryRefireQuaternary).f;
	m_fInSpecialReload = 0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.75;
}

int CShotgun::iItemSlot() { return 3; } // orig 0xc8970

bool CShotgun::UseDecrement() { return true; } // orig 0xc8980

void CShotgun::Reload() // orig 0xc8990
{
	if (0 != m_fInAttack || m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == RC2Pref(RC2P_Ammo_ShotgunClipSize).s)
		return;

	// don't reload until recoil is done
	if (m_flNextPrimaryAttack > UTIL_WeaponTimeBase())
		return;

	// check to see if we're ready to reload
	if (m_fInSpecialReload == 0)
	{
		SendWeaponAnim(SHOTGUN_START_RELOAD);
		m_fInSpecialReload = 1;
		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Shotgun_ReloadStartDelay).f;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Shotgun_ReloadStartDelay).f;
		UTIL_ExtendTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Shotgun_ReloadStartRefirePrimary).f);
		UTIL_ExtendTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Shotgun_ReloadStartRefireSecondary).f);
		UTIL_ExtendTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Shotgun_ReloadStartRefireTertiary).f);
		UTIL_ExtendTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Shotgun_ReloadStartRefireQuaternary).f);
		return;
	}
	else if (m_fInSpecialReload == 1)
	{
		if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
			return;
		// was waiting for gun to move to side
		m_fInSpecialReload = 2;

		if (0 != RANDOM_LONG(0, 1))
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/reload1.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG(0, 0x1f));
		else
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/reload3.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG(0, 0x1f));

		SendWeaponAnim(SHOTGUN_RELOAD);

		m_flNextReload = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Shotgun_ReloadAddDelay).f;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Shotgun_ReloadAddDelay).f;
	}
	else
	{
		// Add them to the clip
		m_iClip += 1;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 1;
		m_fInSpecialReload = 1;
	}
}

void CShotgun::WeaponIdle() // orig 0xc8bc0
{
	if (0 != m_fInAttack)
		return; // charging a crate

	ResetEmptySound();

	if (m_flPumpTime != 0 && m_flPumpTime < gpGlobals->time)
	{
		// play pumping sound
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/scock1.wav", 1, ATTN_NORM, 0, 95 + RANDOM_LONG(0, 0x1f));
		m_flPumpTime = 0;
	}

	if (m_flTimeWeaponIdle >= UTIL_WeaponTimeBase())
		return;

	if (m_iClip == 0 && m_fInSpecialReload == 0 && 0 != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		Reload();
	}
	else if (m_fInSpecialReload != 0)
	{
		if (m_iClip != RC2Pref(RC2P_Ammo_ShotgunClipSize).s && 0 != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		{
			Reload();
		}
		else
		{
			// reload debounce has timed out
			SendWeaponAnim(SHOTGUN_PUMP);

			// play cocking sound
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/scock1.wav", 1, ATTN_NORM, 0, 95 + RANDOM_LONG(0, 0x1f));
			m_fInSpecialReload = 0;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.5;
		}
	}
	else
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 0, 1);
		if (flRand <= 0.8)
		{
			iAnim = SHOTGUN_IDLE_DEEP;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
		}
		else
		{
			iAnim = flRand > 0.95 ? SHOTGUN_IDLE4 : SHOTGUN_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (20.0 / 9.0);
		}
		SendWeaponAnim(iAnim);
	}
}

// While a crate charges: a clip shell burns every CrateChargeTime/CrateAmmoCost seconds, the
// other attacks wait, and the crate fires when the charge time is up.
void CShotgun::WeaponFrame() // orig 0xc8db0
{
	if (!m_fCrateCharging)
		return;

	if (m_fInAttack == 1 && m_pPlayer->m_flNextAmmoBurn <= UTIL_WeaponTimeBase())
	{
		m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Shotgun_CrateChargeTime).f / (int)RC2Pref(RC2P_Shotgun_CrateAmmoCost).s;
		if (--m_iClip < 0)
			m_iClip = 0;
	}

	if (m_flCrateChargeEnd < gpGlobals->time)
	{
		FireCrate();
		return;
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 2.0;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15); // no time base added, as the original
}


class CShotgunAmmo : public CBasePlayerAmmo
{
	void Spawn() override // orig 0xc8f00
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_shotbox.mdl");
		CBasePlayerAmmo::Spawn();
	}
	void Precache() override // orig 0xc8f30
	{
		PRECACHE_MODEL("models/w_shotbox.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	bool AddAmmo(CBaseEntity* pOther) override // orig 0xc8f50
	{
		// VERIFY: RC2's GiveAmmo takes a fourth argument, 0 here (as in CRpgAmmo::AddAmmo)
		if (pOther->GiveAmmo(RC2Pref(RC2P_Ammo_ShellBoxGive).s, "buckshot", RC2Pref(RC2P_Ammo_MaxCarryShells).s) != -1)
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_buckshot, CShotgunAmmo); // orig 0xc8e90
