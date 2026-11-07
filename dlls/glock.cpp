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
*	Rocket Crowbar 2's glock (the original's wpn_shared/hl_wpn_glock.cpp and the run after it).
*	No bullets: primary launches a bouncing gib, secondary is the fart jump, tertiary a sticky gas
*	gib, quaternary charges (burning clip rounds) and fires the transphasic gib, which passes
*	through walls. Every cost, refire and speed is rc2.ini's [Glock] heading.
*
*	This file is also built into the client (cl_dll's predicted weapons). RC2's client is its own
*	library and is reconstructed separately, so the client build gets empty bodies here.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"

#ifndef CLIENT_DLL
#include "rc2prefs.h"
#include "UserMessages.h"

extern bool g_fRC2PlayWeaponEvents; // weapons.cpp

// RC2's CLTE user message: the byte that tells the client which temp entity follows
#define CLTE_STOPEFFECT 0xfe // ours: then the player's index and the effect (13: the charge glow)
#define CLTE_CHARGEGLOW 13	 // ours
#endif

LINK_ENTITY_TO_CLASS(weapon_glock, CGlock);		 // orig 0x54c00
LINK_ENTITY_TO_CLASS(weapon_9mmhandgun, CGlock); // orig 0x54cb0

#ifndef CLIENT_DLL

// orig 0x54d20
void CGlock::Spawn()
{
	pev->classname = MAKE_STRING("weapon_9mmhandgun"); // hack to allow for old names
	Precache();
	m_iId = WEAPON_GLOCK;
	SET_MODEL(ENT(pev), "models/w_9mmhandgun.mdl");

	m_iDefaultAmmo = RC2Pref(RC2P_Ammo_GlockPickupGive).s;

	FallInit(); // get ready to fall down.
	m_fInAttack = 0;
}


// orig 0x54d90
void CGlock::Precache()
{
	PRECACHE_MODEL("models/v_9mmhandgun.mdl");
	PRECACHE_MODEL("models/w_9mmhandgun.mdl");
	PRECACHE_MODEL("models/p_9mmhandgun.mdl");

	m_iShell = PRECACHE_MODEL("models/shell.mdl"); // brass shell

	PRECACHE_SOUND("items/9mmclip1.wav");
	PRECACHE_SOUND("items/9mmclip2.wav");
	PRECACHE_SOUND("debris/beamstart10.wav");
	PRECACHE_SOUND("ambience/biggun2.wav");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	PRECACHE_SOUND("ambience/zapmachine.wav");
	PRECACHE_SOUND("weapons/pl_gun3.wav");

	PRECACHE_MODEL("sprites/blood.spr");
	PRECACHE_MODEL("sprites/bloodspray.spr");
	PRECACHE_MODEL("sprites/plasma.spr");

	UTIL_PrecacheOther("glock_gib");
	UTIL_PrecacheOther("glock_stickygib");
	UTIL_PrecacheOther("transphasic_gib");

	m_usFireGib = PRECACHE_EVENT(1, "events/glockfiregib.sc");
	m_usTPCharge = PRECACHE_EVENT(1, "events/glocktpcharge.sc");
	m_usTPGibFire = PRECACHE_EVENT(1, "events/glocktpgibfire.sc");
	m_usFart = PRECACHE_EVENT(1, "events/glockfart.sc");
}

// orig 0x54eb0
bool CGlock::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = RC2Pref(RC2P_Ammo_MaxCarry9mmBullets).s;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = RC2Pref(RC2P_Ammo_GlockClipSize).s;
	p->iSlot = 1;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_GLOCK;
	p->iWeight = GLOCK_WEIGHT;

	return true;
}

// orig 0x54f20
bool CGlock::Deploy()
{
	m_fInAttack = 0;
	m_fCharging = false;
	return DefaultDeploy("models/v_9mmhandgun.mdl", "models/p_9mmhandgun.mdl", GLOCK_DRAW, "onehanded", 0);
}

// Stop the charge glow the client draws on the player (CLTE 0xfe, effect 13). Inline in the
// original's Holster and FireTransphasic.
static void GlockStopChargeGlow(CBasePlayer* pPlayer) // name ours
{
	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(CLTE_STOPEFFECT);
	WRITE_SHORT(ENTINDEX(pPlayer->edict()));
	WRITE_BYTE(CLTE_CHARGEGLOW);
	MESSAGE_END();
}

// orig 0x54f50
void CGlock::Holster()
{
	if (m_fInAttack != 0)
		GlockStopChargeGlow(m_pPlayer);

	m_fInAttack = 0;
	m_fCharging = false;
	SendWeaponAnim(GLOCK_HOLSTER); // RC2 passes skiplocal 1, body 0
}

// The four timers pushed back by flDelay, the original's response to every failed attack
// (inline there).
static void GlockRaiseAll(CGlock* pGlock, float flDelay) // name ours
{
	UTIL_ExtendTime(&pGlock->m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), flDelay);
	UTIL_ExtendTime(&pGlock->m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), flDelay);
	UTIL_ExtendTime(&pGlock->m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), flDelay);
	UTIL_ExtendTime(&pGlock->m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), flDelay);
}

static void GlockRaiseAll(CGlock* pGlock, float flPrimary, float flSecondary, float flTertiary, float flQuaternary) // name ours
{
	UTIL_ExtendTime(&pGlock->m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), flPrimary);
	UTIL_ExtendTime(&pGlock->m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), flSecondary);
	UTIL_ExtendTime(&pGlock->m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), flTertiary);
	UTIL_ExtendTime(&pGlock->m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), flQuaternary);
}

// A thrown gib is owned by the player in the player's m_iRidingIndex (orig +0x69c) when there
// is one, else by the player (Create's owner). Inline in each attack in the original.
static void GlockGibOwner(CBasePlayer* pPlayer, CBaseEntity* pGib) // name ours
{
	if (pPlayer->m_iRidingIndex != 0)
	{
		edict_t* pent = INDEXENT(pPlayer->m_iRidingIndex);
		if (UTIL_IsValidEntity(pent))
			pGib->pev->owner = pent;
	}
}

// orig 0x54fd0
// Primary: the bouncing gib.
void CGlock::PrimaryAttack()
{
	if (m_iClip < (unsigned short)RC2Pref(RC2P_Glock_GibAmmoCost).s)
	{
		PlayEmptySound();
		GlockRaiseAll(this, 0.2);
		return;
	}

	m_iClip -= (unsigned short)RC2Pref(RC2P_Glock_GibAmmoCost).s;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	Vector vecSrc = m_pPlayer->GetGunPosition();

	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usFireGib, 0.0, g_vecZero, g_vecZero, 0, 0, 0, 0, (m_iClip == 0) ? 1 : 0, 0);

	GlockRaiseAll(this, RC2Pref(RC2P_Glock_PrimaryRefirePrimary).f, RC2Pref(RC2P_Glock_PrimaryRefireSecondary).f,
		RC2Pref(RC2P_Glock_PrimaryRefireTertiary).f, RC2Pref(RC2P_Glock_PrimaryRefireQuaternary).f);

	if (0 == m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	CBaseEntity* pGib = CBaseEntity::Create("glock_gib", vecSrc, g_vecZero, m_pPlayer->edict());
	pGib->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Glock_GibLaunchVel).f;
	pGib->pev->velocity.z += RC2Pref(RC2P_Glock_GibLaunchZBoost).f;
	GlockGibOwner(m_pPlayer, pGib);
}

// orig 0x552e0
// Secondary: the fart, a push along the view (not under water) and an event. The server deals
// no damage here.
void CGlock::SecondaryAttack()
{
	if (m_iClip < (unsigned short)RC2Pref(RC2P_Glock_FartAmmoCost).s)
	{
		Reload();
		PlayEmptySound();
		GlockRaiseAll(this, 0.2);
		return;
	}

	if (m_pPlayer->m_iRidingIndex != 0)
	{
		// not while acting through another entity
		PlayEmptySound();
		GlockRaiseAll(this, 0.2);
		return;
	}

	m_iClip -= (unsigned short)RC2Pref(RC2P_Glock_FartAmmoCost).s;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation(PLAYER_JUMP);
	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_pPlayer->GetGunPosition(); // the original calls it and drops the result

	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usFart, 0.0, g_vecZero, g_vecZero, 0, 0, 0, 0, (m_pPlayer->pev->waterlevel >= 2) ? 1 : 0, 0);

	GlockRaiseAll(this, RC2Pref(RC2P_Glock_SecondaryRefirePrimary).f, RC2Pref(RC2P_Glock_SecondaryRefireSecondary).f,
		RC2Pref(RC2P_Glock_SecondaryRefireTertiary).f, RC2Pref(RC2P_Glock_SecondaryRefireQuaternary).f);

	if (0 == m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

	if (m_pPlayer->pev->waterlevel < 2)
	{
		UTIL_MakeVectors(m_pPlayer->pev->v_angle);
		m_pPlayer->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Glock_FartVelAdd).f + m_pPlayer->pev->velocity;
	}
}

// orig 0x55620
// Tertiary: the sticky gas gib.
void CGlock::TertiaryAttack()
{
	if (m_iClip < (unsigned short)RC2Pref(RC2P_Glock_StickyAmmoCost).s)
	{
		PlayEmptySound();
		Reload();
		GlockRaiseAll(this, 0.2);
		return;
	}

	m_iClip -= (unsigned short)RC2Pref(RC2P_Glock_StickyAmmoCost).s;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	Vector vecSrc = m_pPlayer->GetGunPosition();

	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usFireGib, 0.0, g_vecZero, g_vecZero, 0, 0, 0, 0, (m_iClip == 0) ? 1 : 0, 0);

	GlockRaiseAll(this, RC2Pref(RC2P_Glock_TertiaryRefirePrimary).f, RC2Pref(RC2P_Glock_TertiaryRefireSecondary).f,
		RC2Pref(RC2P_Glock_TertiaryRefireTertiary).f, RC2Pref(RC2P_Glock_TertiaryRefireQuaternary).f);

	if (0 == m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	CBaseEntity* pGib = CBaseEntity::Create("glock_stickygib", vecSrc, g_vecZero, m_pPlayer->edict());
	pGib->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Glock_StickyLaunchVel).f;
	pGib->pev->velocity.z += RC2Pref(RC2P_Glock_StickyLaunchZBoost).f;
	GlockGibOwner(m_pPlayer, pGib);
}

// orig 0x55940
// Quaternary: start the transphasic charge. It costs energy up front; WeaponFrame burns clip
// rounds while it runs and fires when it is done.
void CGlock::QuaternaryAttack()
{
	if (m_fInAttack != 0)
		return;

	if (m_iClip < (unsigned short)RC2Pref(RC2P_Glock_TransphasicAmmoCost).s)
	{
		Reload();
		PlayEmptySound();
		GlockRaiseAll(this, 0.2);
		return;
	}

	if (m_pPlayer->m_flEnergy < RC2Pref(RC2P_Glock_TransphasicEnergyCost).f)
	{
		PlayEmptySound();
		GlockRaiseAll(this, 0.2);
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_flEnergy -= RC2Pref(RC2P_Glock_TransphasicEnergyCost).f;

	// the original takes one round here whatever the cost (the cost only gates the attack)
	if ((unsigned short)RC2Pref(RC2P_Glock_TransphasicAmmoCost).s > 0)
		m_iClip--;

	m_fInAttack = 1;
	m_fCharging = true;
	m_flChargeDone = gpGlobals->time + RC2Pref(RC2P_Glock_TransphasicChargeTime).f;
	m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase();

	m_flNextSecondaryAttack = m_flNextPrimaryAttack = m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + 10.0;
	m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 10.0;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usTPCharge, 0.0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);
}

// orig 0x55b80
// The charge's shot: the transphasic gib along the view, with the shooter's ping as head start
// (TGibThink adds it to its first step), and a recoil that lifts the player off the ground.
void CGlock::FireTransphasic()
{
	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	Vector vecSrc = m_pPlayer->GetGunPosition();

	// The original clears the charge only when weapon events play (always on the server, where
	// ItemPostFrame sets g_fRC2PlayWeaponEvents), and tests the flag twice.
	if (g_fRC2PlayWeaponEvents)
	{
		m_fInAttack = 0;
		m_fCharging = false;
		if (g_fRC2PlayWeaponEvents)
			PLAYBACK_EVENT_FULL(FEV_NOTHOST | FEV_GLOBAL, m_pPlayer->edict(), m_usTPGibFire, 0.0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);
	}

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Glock_QuaternaryRefirePrimary).f;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Glock_QuaternaryRefireSecondary).f;
	m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Glock_QuaternaryRefireTertiary).f;
	m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Glock_QuaternaryRefireQuaternary).f;

	if (0 == m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);

	GlockStopChargeGlow(m_pPlayer);

	CBaseEntity* pGib = CBaseEntity::Create("transphasic_gib", vecSrc, g_vecZero, m_pPlayer->edict());
	pGib->pev->movedir = gpGlobals->v_forward;
	GlockGibOwner(m_pPlayer, pGib);

	int iPing = 0, iLoss = 0;
	g_engfuncs.pfnGetPlayerStats(m_pPlayer->edict(), &iPing, &iLoss);
	pGib->pev->armorvalue = iPing * 0.001;

	m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * RC2Pref(RC2P_Glock_TransphasicRecoilVel).f;
	m_pPlayer->pev->velocity.z += RC2Pref(RC2P_Glock_TransphasicRecoilZBoost).f;
	m_pPlayer->pev->flags &= ~FL_ONGROUND;

	// HLTV: an event on the shooter. VERIFY: SDK 2.3's director messages go to svc 51 and start
	// with a length byte; RC2 writes the command (3, DRC_CMD_EVENT) first, to svc 50
	MESSAGE_BEGIN(MSG_SPEC, 50);
	WRITE_BYTE(3);
	WRITE_SHORT(ENTINDEX(m_pPlayer->edict()));
	WRITE_SHORT(0);
	WRITE_LONG(1);
	MESSAGE_END();
}

// orig 0x56050
// While charging: a clip round every ChargeTime / AmmoCost seconds, then the shot.
void CGlock::WeaponFrame()
{
	if (!m_fCharging)
		return;

	if (m_pPlayer->m_flNextAmmoBurn <= UTIL_WeaponTimeBase())
	{
		m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Glock_TransphasicChargeTime).f / (float)(unsigned short)RC2Pref(RC2P_Glock_TransphasicAmmoCost).s;
		if (--m_iClip < 0)
			m_iClip = 0;
	}

	if (m_flChargeDone < gpGlobals->time)
	{
		FireTransphasic();
		return;
	}

	m_flNextSecondaryAttack = m_flNextPrimaryAttack = m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + 1.5;
	m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 4.0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
}


// orig 0x55ee0
void CGlock::Reload()
{
	if (m_pPlayer->ammo_9mm <= 0)
		return;

	if (m_fInAttack != 0)
		return;

	int iClipSize = RC2Pref(RC2P_Ammo_GlockClipSize).s;
	if (m_iClip >= iClipSize)
		return;

	bool iResult = DefaultReload(iClipSize, m_iClip == 0 ? GLOCK_RELOAD : GLOCK_RELOAD_NOT_EMPTY, RC2Pref(RC2P_Glock_ReloadTime).f, 0);
	if (iResult)
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	}
}


// orig 0x55f70
void CGlock::WeaponIdle()
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector(AUTOAIM_10DEGREES);

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if (m_fInAttack != 0)
		return;

	// only idle if the slid isn't back
	if (m_iClip != 0)
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 0.0, 1.0);

		if (flRand <= 0.3 + 0 * 0.75)
		{
			iAnim = GLOCK_IDLE3;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 49.0 / 16;
		}
		else if (flRand <= 0.6 + 0 * 0.875)
		{
			iAnim = GLOCK_IDLE1;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 60.0 / 16.0;
		}
		else
		{
			iAnim = GLOCK_IDLE2;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 40.0 / 16.0;
		}
		SendWeaponAnim(iAnim);
	}
}

#else // CLIENT_DLL: RC2's client weapons are its own library's

void CGlock::Spawn()
{
	Precache();
	m_iId = WEAPON_GLOCK;
	FallInit();
}
void CGlock::Precache() {}
bool CGlock::GetItemInfo(ItemInfo* p) { return false; }
bool CGlock::Deploy() { return DefaultDeploy("models/v_9mmhandgun.mdl", "models/p_9mmhandgun.mdl", GLOCK_DRAW, "onehanded", 0); }
void CGlock::Holster() { SendWeaponAnim(GLOCK_HOLSTER); }
void CGlock::PrimaryAttack() {}
void CGlock::SecondaryAttack() {}
void CGlock::TertiaryAttack() {}
void CGlock::QuaternaryAttack() {}
void CGlock::FireTransphasic() {}
void CGlock::WeaponFrame() {}
void CGlock::Reload() {}
void CGlock::WeaponIdle() {}

#endif // CLIENT_DLL


#ifndef CLIENT_DLL
class CGlockAmmo : public CBasePlayerAmmo
{
	// orig 0x561d0
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_9mmclip.mdl");
		CBasePlayerAmmo::Spawn();
	}
	// orig 0x56200
	void Precache() override
	{
		PRECACHE_MODEL("models/w_9mmclip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// orig 0x56220
	bool AddAmmo(CBaseEntity* pOther) override
	{
		if (pOther->GiveAmmo(RC2Pref(RC2P_Ammo_GlockSpareClipGive).s, "9mm", RC2Pref(RC2P_Ammo_MaxCarry9mmBullets).s) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_glockclip, CGlockAmmo); // orig 0x56160
LINK_ENTITY_TO_CLASS(ammo_9mmclip, CGlockAmmo);	  // orig 0x56280
#endif
