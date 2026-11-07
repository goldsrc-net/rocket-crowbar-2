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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "customentity.h"
#include "gamerules.h"
#include "UserMessages.h"

#ifdef CLIENT_DLL
#include "hud.h"
#include "com_weapons.h"
#else
extern bool IsBustingGame();
#endif


//=========================================================
// Rocket Crowbar 2's egon. Primary: a tractor beam that grabs what it hits and holds it at a
// distance the secondary and tertiary attacks lengthen and shorten, and the quaternary throws
// it. With the beam off: secondary fires the shrinker, tertiary charges the imploder (the
// secondary during the charge shrinks yourself instead), quaternary fires the nuke. Every number
// is rc2.ini [Egon]/[Ammo]. Reconstructed from rc2.dll (reference/dossiers/p2.md); the client's
// prediction compiles the same code (client.dll, reference/dossiers/c1.md): it switches the beam
// on and off and spends ammo, but never grabs, and plays the beam's event itself every frame.
//=========================================================

#include "rc2prefs.h"
#ifndef CLIENT_DLL
#include "p2shared.h"
#endif

// from other files (TEMP STUBs in dlls/rc2/p2stubs_forkC.cpp; on the client, hl_weapons.cpp's
// PlayerHasAttrFlag200, orig client 0x3ba60)
#ifndef CLIENT_DLL
extern int RC2PlayerWaterWeapons(CBasePlayer* pPlayer);		  // orig 0x95860 (player): bit 9 of RC2PlayerButtons, the player may fire under water
#else
bool PlayerHasAttrFlag200(CBaseEntity* pPlayer);
static int RC2PlayerWaterWeapons(CBasePlayer* pPlayer) { return PlayerHasAttrFlag200(pPlayer) ? 1 : 0; }
#endif
extern int UTIL_PackNormal(Vector vecNormal);					  // orig 0xf5920 (util): a unit vector in an int, 10 bits and a sign per axis

// can't fire: fully under water without the RC2 under-water bit
static bool EgonUnderwater(CBasePlayer* pPlayer) // name ours
{
	return pPlayer->pev->waterlevel == 3 && RC2PlayerWaterWeapons(pPlayer) == 0;
}

// the bytes RC2 keeps in CBaseEntity +0x64 (cbase.h's m_ucTractorHeld, m_ucTractorKnocked, m_bFrozen, m_bAntiFriction): +0x64 an egon holds it,
// +0x65 another egon knocked it out of this one's beam
static byte& TractorHeld(CBaseEntity* pEnt) { return pEnt->m_ucTractorHeld; }	   // name ours
static byte& TractorKnocked(CBaseEntity* pEnt) { return pEnt->m_ucTractorKnocked; } // name ours

#define EGON_EXTEND4(flDelay)                                                     \
	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), flDelay);      \
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), flDelay);    \
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), flDelay);     \
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), flDelay)

#define EGON_EXTEND4_KEYS(Keys)                                                                                             \
	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Egon_##Keys##RefirePrimary).f);       \
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Egon_##Keys##RefireSecondary).f);   \
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Egon_##Keys##RefireTertiary).f);     \
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Egon_##Keys##RefireQuaternary).f)

#define EGON_AMMO (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])

// orig client 0xbbd0 (and its GetClassPtr<CEgon>, orig client 0xdb10)
LINK_ENTITY_TO_CLASS(weapon_egon, CEgon); // orig 0x3d290

// orig 0x3d320, orig client 0xbc00
void CEgon::Spawn()
{
	Precache();
	m_iId = WEAPON_EGON;
	SET_MODEL(ENT(pev), "models/w_egon.mdl");
	m_iDefaultAmmo = RC2Pref(RC2P_Ammo_EgonPickupGive).s;
	FallInit(); // get ready to fall down.

	m_bHolding = false;
	m_flNextSync = gpGlobals->time;
	m_bAdjusting = false;
	m_flLastFrame = gpGlobals->time;
}

// orig 0x3d390, orig client 0xbc90
void CEgon::Precache()
{
	PRECACHE_MODEL("models/w_egon.mdl");
	PRECACHE_MODEL("models/v_egon.mdl");
	PRECACHE_MODEL("models/p_egon.mdl");
	PRECACHE_MODEL("models/w_9mmclip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND("weapons/egon_windup2.wav");
	PRECACHE_SOUND("weapons/egon_run3.wav");
	PRECACHE_SOUND("weapons/egon_off1.wav");
	PRECACHE_SOUND("weapons/357_cock1.wav");
	PRECACHE_SOUND("debris/beamstart8.wav");
	PRECACHE_SOUND("weapons/rocketfire1.wav");
	PRECACHE_SOUND("ambience/biggun3.wav");

	UTIL_PrecacheOther("shrinker_project");
	UTIL_PrecacheOther("nuclear_missile");
	UTIL_PrecacheOther("implode_timer");

	m_usFireShrinker = PRECACHE_EVENT(1, "events/egonfireshrinker.sc");
	m_usImploderCharge = PRECACHE_EVENT(1, "events/egonimplodercharge.sc");
	m_usImploderFire = PRECACHE_EVENT(1, "events/egonimploderfire.sc");
	m_usFireNuke = PRECACHE_EVENT(1, "events/egonfirenuke.sc");
	m_usTractor = PRECACHE_EVENT(1, "events/egontractor.sc");
	m_usShrinkerImpact = PRECACHE_EVENT(1, "events/egonshrinkerimpact.sc");
}

// orig 0x3d4c0, orig client 0xbe10
bool CEgon::Deploy()
{
	m_bTractorOn = false;
	m_bHolding = false;
	m_bAdjusting = false;
	m_iChargeType = 0;
	m_fInAttack = 0;
#ifndef CLIENT_DLL
	m_hTractor = NULL;
	m_flNextSync = gpGlobals->time;
#endif
	m_flLastFrame = gpGlobals->time;
	return DefaultDeploy("models/v_egon.mdl", "models/p_egon.mdl", EGON_DRAW, "egon");
}

// orig 0x3d530, orig client 0xbe80. RC2 sends gmsgWeapPickup here once the base has added the weapon;
// halflife-updated sends that same message from CBasePlayer::AddPlayerItem (one message either way).
void CEgon::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
}

// orig 0x3d590, orig client 0xbf00 (the client's ends nothing: no messages, no sound)
void CEgon::Holster()
{
	TractorStop();
	m_flLastFrame = gpGlobals->time;
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim(EGON_HOLSTER);

	m_iChargeType = 0;
	m_fInAttack = 0;

#ifndef CLIENT_DLL
	// end the client's beam effects
	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(RC2_CLTE_KILL);
	WRITE_SHORT(ENTINDEX(m_pPlayer->edict()));
	WRITE_BYTE(0x29);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(RC2_CLTE_KILL);
	WRITE_SHORT(ENTINDEX(m_pPlayer->edict()));
	WRITE_BYTE(0x2c);
	MESSAGE_END();

	STOP_SOUND(ENT(m_pPlayer->pev), CHAN_STATIC, "weapons/egon_run3.wav");
#endif
}

// orig 0x3d6a0, orig client 0xbf80: not while the beam is on
bool CEgon::CanHolster()
{
	return !m_bTractorOn;
}

// orig 0x3d6b0, orig client 0xbfa0. Original oddity, kept: it also sets m_iId.
bool CEgon::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = RC2Pref(RC2P_Ammo_MaxCarryCells).s;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 2;
	m_iId = WEAPON_EGON;
	p->iId = WEAPON_EGON;
	p->iFlags = 0;
	p->iWeight = 25;
	return true;
}

// orig 0x3d720, orig client 0xc030: the tractor beam, every frame the button is held
void CEgon::PrimaryAttack()
{
	if (EgonUnderwater(m_pPlayer))
	{
		PlayEmptySound();
		TractorStop();
		EGON_EXTEND4(0.2);
		return;
	}

	if (EGON_AMMO < 1)
	{
		PlayEmptySound();
		TractorStop();
		EGON_EXTEND4(0.5);
		return;
	}

	if (!m_bTractorOn)
	{
		// switch it on
		m_bTractorOn = true;
		m_bHolding = false;
#ifndef CLIENT_DLL
		m_hTractor = NULL;
		m_flNextSync = gpGlobals->time;
#endif
		EGON_AMMO--;
		m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + 0.5;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1;
		SendWeaponAnim(EGON_FIRE1 + RANDOM_LONG(0, 3));
		if (g_runfuncs)
			PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usTractor, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 1);
		return;
	}

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.1;
#ifndef CLIENT_DLL
	Vector vecSrc = m_pPlayer->GetGunPosition();
	TraceResult tr;

	if (m_bHolding && m_hTractor)
	{
		// lose it when something else comes between
		UTIL_TraceLine(vecSrc, m_hTractor->Center(), dont_ignore_monsters, m_pPlayer->edict(), &tr);
		if (tr.flFraction < 1.0)
		{
			CBaseEntity* pHit = tr.pHit ? (CBaseEntity*)GET_PRIVATE(tr.pHit) : NULL;
			if (!pHit || pHit->GetEffectedEnt()->edict() != m_hTractor->edict())
			{
				TractorHeld(m_hTractor) = 0;
				TractorKnocked(m_hTractor) = 0;
				m_bHolding = false;
				m_hTractor = NULL;
				m_flNextSync = gpGlobals->time;
			}
		}
	}
	else
	{
		// look for something to grab
		m_bHolding = false;
		m_hTractor = NULL;

		UTIL_MakeVectors(m_pPlayer->pev->punchangle + m_pPlayer->pev->v_angle);
		UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);

		CBaseEntity* pEnt = NULL;
		if (tr.flFraction < 1.0 && UTIL_IsValidEntity(tr.pHit))
		{
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			if (pHit)
				pEnt = pHit->GetEffectedEnt();
		}

		// VERIFY: slot 75's first argument is the player here (cbase.h has an int)
		if (pEnt && !pEnt->IsBSPModel() && (pEnt->GetRCCaps(m_pPlayer, this) & 0x80) != 0 && g_pGameRules->PlayerCanAffect(m_pPlayer, pEnt))
		{
			if (TractorHeld(pEnt))
			{
				// someone else's beam has it: knock it out, and stop
				TractorStop();
				TractorHeld(pEnt) = 0;
				TractorKnocked(pEnt) = 1;
				EGON_EXTEND4_KEYS(TractorDrop);
				return;
			}

			m_bHolding = true;
			m_hTractor = pEnt;
			m_flHoldDist = (m_hTractor->Center() - vecSrc).Length();
			m_bTargetAlive = m_hTractor->IsAlive();
			m_hTractor->m_vecRC2Base58 = vecSrc + gpGlobals->v_forward * m_flHoldDist; // where ApplyExternalForce pulls it
			TractorHeld(m_hTractor) = 1;
			TractorKnocked(m_hTractor) = 0;
			m_flNextSync = gpGlobals->time;
		}
	}

	if (m_bHolding && m_hTractor)
		PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usTractor, 0, g_vecZero, g_vecZero, m_flHoldDist, 0, 0, ENTINDEX(m_hTractor->edict()), 0, 0);
	else
		PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usTractor, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);
#endif
}

// orig 0x3ddd0, orig client 0xc2b0: push what the beam holds away; with the beam off, the shrinker
void CEgon::SecondaryAttack()
{
	if (m_bTractorOn)
	{
		if (m_bHolding)
		{
			m_bAdjusting = true;
			m_flHoldDist += RC2Pref(RC2P_Egon_TractorExtendRate).f * m_flFrameInterval;
		}
		return;
	}

	if (EgonUnderwater(m_pPlayer))
	{
		PlayEmptySound();
		EGON_EXTEND4(0.2);
		return;
	}

	if (EGON_AMMO < (RC2Pref(RC2P_Egon_ShrinkerAmmoCost).s & 0xffff))
	{
		PlayEmptySound();
		EGON_EXTEND4(0.5);
		return;
	}

	EGON_EXTEND4_KEYS(Secondary);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

	EGON_AMMO -= RC2Pref(RC2P_Egon_ShrinkerAmmoCost).s & 0xffff;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->m_iWeaponVolume = 600;
	m_pPlayer->m_iWeaponFlash = 256;
	(void)m_pPlayer->GetGunPosition(); // the original calls it here and drops the result

	if (g_runfuncs)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usFireShrinker, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

	if (EGON_AMMO <= 0)
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

#ifndef CLIENT_DLL
	UTIL_MakeVectors(m_pPlayer->pev->punchangle + m_pPlayer->pev->v_angle);
	CBaseEntity* pShrinker = CBaseEntity::Create("shrinker_project", m_pPlayer->GetGunPosition(), g_vecZero, m_pPlayer->edict());
	pShrinker->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Egon_ShrinkerLaunchVelocity).f;

	if (m_pPlayer->m_iRidingIndex)
	{
		edict_t* pControlled = INDEXENT(m_pPlayer->m_iRidingIndex);
		if (UTIL_IsValidEntity(pControlled))
			pShrinker->pev->owner = pControlled;
	}
#endif
}

// orig 0x3e1e0, orig client 0xc690: pull what the beam holds in; with the beam off, start charging the imploder
void CEgon::TertiaryAttack()
{
	if (m_bTractorOn)
	{
		if (!m_bHolding)
			return;
		m_bAdjusting = true;
		m_flHoldDist -= RC2Pref(RC2P_Egon_TractorContractRate).f * m_flFrameInterval;
		return;
	}

	if (EgonUnderwater(m_pPlayer))
	{
		PlayEmptySound();
		EGON_EXTEND4(0.15);
		return;
	}

	// enough for the whole charge and the shot
	int iNeed = (int)(RC2Pref(RC2P_Egon_ImploderChargeTime).f / RC2Pref(RC2P_Egon_ImploderChargeAmmoBurnDelay).f) * (RC2Pref(RC2P_Egon_ImploderChargeAmmoPerBurn).s & 0xffff) + (RC2Pref(RC2P_Egon_ImploderFireUseAmmo).s & 0xffff);
	if (iNeed <= EGON_AMMO && RC2Pref(RC2P_Egon_ImploderFireUseEnergy).f <= m_pPlayer->m_flEnergy)
	{
		m_iChargeType = 1;
		m_fInAttack = 1;
		m_flChargeDone = RC2Pref(RC2P_Egon_ImploderChargeTime).f + gpGlobals->time;
		m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase();
		if (g_runfuncs)
			PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usImploderCharge, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);
		return;
	}

	PlayEmptySound();
	EGON_EXTEND4(0.5);
}

// orig 0x3e400, orig client 0xc950: throw what the beam holds; with the beam off, the nuke
void CEgon::QuaternaryAttack()
{
	if (m_bTractorOn)
	{
		if (!m_bHolding)
			return;

		if (EGON_AMMO < (RC2Pref(RC2P_Egon_TractorLaunchCost).s & 0xffff))
		{
			PlayEmptySound();
			// Original oddity, kept: only the fourth attack's timer
			UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.2);
			return;
		}

#ifndef CLIENT_DLL
		if (m_hTractor)
		{
			UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
			m_hTractor->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Egon_TractorLaunchVel).f + m_hTractor->pev->velocity;

			// the original reads sv_maxvelocity through the pointer world.cpp keeps (orig .data 0x18c500)
			float flMax = CVAR_GET_FLOAT("sv_maxvelocity");
			if (m_hTractor->pev->velocity.Length() > flMax)
				m_hTractor->pev->velocity = m_hTractor->pev->velocity.Normalize() * flMax;
		}
#endif

		EGON_AMMO -= RC2Pref(RC2P_Egon_TractorLaunchCost).s & 0xffff;
		TractorStop();
		UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), 0.5);
		UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), 0.6);
		UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), 0.9);
		UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.9);
		return;
	}

	if (EgonUnderwater(m_pPlayer) || m_pPlayer->m_flJoinTime > gpGlobals->time)
	{
		PlayEmptySound();
		EGON_EXTEND4(0.5);
		return;
	}

	if (EGON_AMMO < (RC2Pref(RC2P_Egon_NukeAmmoCost).s & 0xffff) || m_pPlayer->m_flEnergy < RC2Pref(RC2P_Egon_NukeEnergyCost).f)
	{
		PlayEmptySound();
		EGON_EXTEND4(0.75);
		return;
	}

	EGON_AMMO -= RC2Pref(RC2P_Egon_NukeAmmoCost).s & 0xffff;
	m_pPlayer->m_flEnergy -= RC2Pref(RC2P_Egon_NukeEnergyCost).f;
	UTIL_ExtendTime(&m_pPlayer->m_flJoinTime, gpGlobals->time, RC2Pref(RC2P_Egon_QuaternaryRefireSuperWeapon).f);
	EGON_EXTEND4_KEYS(Quaternary);
	UTIL_ExtendTime(&m_pPlayer->m_flNextAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Egon_QuaternaryRefireAnything).f);
	UTIL_ExtendTime(&m_pPlayer->m_flEnergyRegenTime, gpGlobals->time, RC2Pref(RC2P_Egon_QuaternaryEnergyRegenDelay).f);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->m_iWeaponVolume = 1000;
	m_pPlayer->m_iWeaponFlash = 512;
	(void)m_pPlayer->GetGunPosition(); // the original calls it here and drops the result

	if (g_runfuncs)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usFireNuke, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

#ifndef CLIENT_DLL
	UTIL_MakeVectors(m_pPlayer->pev->punchangle + m_pPlayer->pev->v_angle);
	CBaseEntity* pNuke = CBaseEntity::Create("nuclear_missile", m_pPlayer->GetGunPosition(), g_vecZero, m_pPlayer->edict());
	pNuke->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Egon_NukeLaunchVel).f;
	pNuke->pev->angles.x = -m_pPlayer->pev->v_angle.x;
	pNuke->pev->angles.y = m_pPlayer->pev->v_angle.y;

	if (m_pPlayer->m_iRidingIndex)
	{
		edict_t* pControlled = INDEXENT(m_pPlayer->m_iRidingIndex);
		if (UTIL_IsValidEntity(pControlled))
			pNuke->pev->owner = pControlled;
	}
#endif
}

// orig 0x3eb40, orig client 0xce70: switch the beam off and drop what it holds
void CEgon::TractorStop()
{
	if (m_bTractorOn)
	{
#ifdef CLIENT_DLL
		// the client's own beam off: one event from here, bparam1 1
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usTractor, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 1, 2);
#else
		if (m_hTractor)
		{
			m_hTractor->m_vecRC2Base58 = g_vecZero;
			TractorHeld(m_hTractor) = 0;
			TractorKnocked(m_hTractor) = 0;
		}
		PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usTractor, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 2);
		PLAYBACK_EVENT_FULL(FEV_RELIABLE | FEV_GLOBAL, m_pPlayer->edict(), m_usTractor, 0.01, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 2);
#endif
		SendWeaponAnim(EGON_IDLE1);
	}

	m_bTractorOn = false;
	m_bHolding = false;
	m_bAdjusting = false;
#ifndef CLIENT_DLL
	m_hTractor = NULL;
	m_flNextSync = gpGlobals->time;
#endif
}

// orig 0x3ec50, orig client 0xcf00: the charged imploder: an implode timer homes on the best target ahead
void CEgon::ImploderFire()
{
	CBaseEntity* pBest = NULL;

	if (g_runfuncs)
	{
		m_iChargeType = 0;
		m_fInAttack = 0;
	}

	if (EgonUnderwater(m_pPlayer))
	{
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usImploderFire, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 1, 0);
		PlayEmptySound();
	}
	else if ((RC2Pref(RC2P_Egon_ImploderFireUseAmmo).s & 0xffff) <= EGON_AMMO && RC2Pref(RC2P_Egon_ImploderFireUseEnergy).f <= m_pPlayer->m_flEnergy)
	{
		m_pPlayer->m_flEnergy -= RC2Pref(RC2P_Egon_ImploderFireUseEnergy).f;
		UTIL_ExtendTime(&m_pPlayer->m_flEnergyRegenTime, gpGlobals->time, 1.5);
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefirePrimary).f;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefireSecondary).f;
		m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefireTertiary).f;
		m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefireQuaternary).f;
		m_pPlayer->m_iWeaponVolume = 600;

		if (g_runfuncs)
			PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usImploderFire, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

		m_pPlayer->SetAnimation(PLAYER_ATTACK1);
		EGON_AMMO -= RC2Pref(RC2P_Egon_ImploderFireUseAmmo).s & 0xffff;

#ifndef CLIENT_DLL
		float flBest = -9999;
		float flMaxDist = RC2Pref(RC2P_Egon_ImploderTargetMaxDist).f;
		float flMinDot = RC2Pref(RC2P_Egon_ImploderTargetMinDot).f;

		UTIL_MakeVectors(m_pPlayer->pev->punchangle + m_pPlayer->pev->v_angle);
		Vector vecForward = gpGlobals->v_forward;
		Vector vecSrc = m_pPlayer->GetGunPosition();

		// every entity, not just players
		for (int i = 1; i < gpGlobals->maxEntities; i++)
		{
			edict_t* pent = INDEXENT(i);
			if (pent == m_pPlayer->edict() || !UTIL_IsValidEntity(pent))
				continue;
			CBaseEntity* pEnt = CBaseEntity::Instance(pent);
			if (!pEnt || !pEnt->IsAlive() || pEnt->pev->takedamage != DAMAGE_AIM || pEnt->pev->deadflag != DEAD_NO || pEnt->IsBSPModel())
				continue;
			// not one already shrunk below half
			if (pEnt->pev->scale > 0 && pEnt->pev->scale < 0.5)
				continue;
			if (g_pGameRules->PlayerRelationship(m_pPlayer, pEnt) == GR_TEAMMATE)
				continue;
			// VERIFY: slot 76's first argument is the player here (cbase.h has an int)
			if ((pEnt->GetRCCaps2(m_pPlayer, this) & 0x20) != 0)
				continue;

			Vector vecTarget = pEnt->Center();
			Vector vecTo = vecTarget - vecSrc;
			float flDist = vecTo.Length();
			if (flDist > flMaxDist)
				continue;

			Vector vecDir = flDist > 0 ? vecTo / flDist : vecForward;
			float flDot = DotProduct(vecDir, vecForward);
			if (flDot < flMinDot)
				continue;

			float flNear = 1.0 - flDist / flMaxDist;
			if (flNear < 0)
				flNear = 0;
			else if (flNear > 1)
				flNear = 1;
			float flAim = flDot / flMinDot;
			if (flAim < 0)
				flAim = 0;
			else if (flAim > 1)
				flAim = 1;
			float flScore = flAim * 3.0 + flNear + flNear;
			if (flScore < flBest)
				continue;

			TraceResult tr;
			UTIL_TraceLine(vecSrc, vecTarget, ignore_monsters, m_pPlayer->edict(), &tr);
			if (tr.flFraction >= 1.0 || tr.pHit == pEnt->edict())
			{
				flBest = flScore;
				pBest = pEnt->GetEffectedEnt();
			}
		}

		// VERIFY: slot 75's first argument is the player here (cbase.h has an int)
		if (pBest && (pBest->GetRCCaps(m_pPlayer, this) & 0x100) != 0)
		{
			CBaseEntity* pTimer = CBaseEntity::Create("implode_timer", vecSrc, g_vecZero, NULL);
			pTimer->pev->origin = vecSrc;
			pTimer->pev->velocity = vecForward * RC2Pref(RC2P_Egon_ImploderTrackSpeed).f;
			pTimer->pev->owner = pBest->edict();
			pTimer->pev->dmg_inflictor = m_pPlayer->edict();
		}

		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(RC2_CLTE_KILL);
		WRITE_SHORT(ENTINDEX(m_pPlayer->edict()));
		WRITE_BYTE(0x2c);
		MESSAGE_END();
#endif
		return;
	}
	else
	{
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usImploderFire, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 1, 0);
		PlayEmptySound();
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 0.5;
}

// orig 0x3f370, orig client 0xd170: the charge turned on yourself: a shrink timer that never ends
void CEgon::ShrinkSelf()
{
	if (g_runfuncs)
	{
		m_iChargeType = 0;
		m_fInAttack = 0;
	}

	if (EgonUnderwater(m_pPlayer))
	{
		PlayEmptySound();
	}
	// Original oddity, kept: the check is against the imploder shot's costs, the charge against
	// ShrinkSelf's
	else if (m_pPlayer->m_iRidingIndex == 0 && (RC2Pref(RC2P_Egon_ImploderFireUseAmmo).s & 0xffff) <= EGON_AMMO && RC2Pref(RC2P_Egon_ImploderFireUseEnergy).f <= m_pPlayer->m_flEnergy)
	{
		m_pPlayer->m_flEnergy -= RC2Pref(RC2P_Egon_ImploderShrinkSelfEnergyCost).f;
		UTIL_ExtendTime(&m_pPlayer->m_flEnergyRegenTime, gpGlobals->time, 1.5);
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefirePrimary).f;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefireSecondary).f;
		m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefireTertiary).f;
		m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TertiaryRefireQuaternary).f;
		m_pPlayer->m_iWeaponVolume = 600;
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);
		EGON_AMMO -= RC2Pref(RC2P_Egon_ImploderShrinkSelfAmmoCost).s & 0xffff;

#ifndef CLIENT_DLL
		PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usShrinkerImpact, 0, g_vecZero, g_vecZero, 0, 0, UTIL_PackNormal(Vector(0, 0, -1)), ENTINDEX(m_pPlayer->edict()), 0, 0);

		CBaseEntity* pTimer = NULL;
		for (pTimer = UTIL_FindEntityByClassname(NULL, "shrink_timer"); pTimer; pTimer = UTIL_FindEntityByClassname(pTimer, "shrink_timer"))
		{
			if (pTimer->pev->owner == m_pPlayer->edict())
				break;
		}
		if (!pTimer)
			pTimer = CBaseEntity::Create("shrink_timer", g_vecZero, g_vecZero, m_pPlayer->edict());
		if (!pTimer)
			return;

		pTimer->pev->owner = m_pPlayer->edict();
		pTimer->pev->dmg = -1;
		pTimer->pev->dmgtime = gpGlobals->time + 1e6;
#endif
		return;
	}
	else
	{
		PlayEmptySound();
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 0.5;
}

// orig 0x3f620, orig client 0xd370
void CEgon::WeaponIdle()
{
	ResetEmptySound();

	if (UTIL_WeaponTimeBase() < m_flTimeWeaponIdle)
		return;
	if (m_bTractorOn)
		return;

	int iAnim;
	if (RANDOM_FLOAT(0, 1) <= 0.5)
	{
		iAnim = EGON_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	}
	else
	{
		iAnim = EGON_FIDGET1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3;
	}
	SendWeaponAnim(iAnim);
}

// orig 0x3f6c0, orig client 0xd440: the client's half of WeaponSync (on the server the readers return 0)
void CEgon::ReadWeaponState(void* pData, int iSize)
{
	bool bWasHolding = m_bHolding;

	int iFlags = RC2ReadStateByte(&pData, &iSize);
	m_bHolding = ((iFlags >> 1) & 1) != 0;
	m_bTractorOn = (iFlags & 1) != 0;
	m_flHoldDist = RC2ReadStateShort(&pData, &iSize) * 0.125;

	if (bWasHolding && !m_bTractorOn)
	{
		if ((m_pPlayer->pev->button & IN_ATTACK) != 0)
			EGON_EXTEND4_KEYS(TractorDrop);
		TractorStop();
	}
}

// orig 0x3f7d0, orig client 0xd5a0 (the client's without the sync, and with its own beam event): sync, the imploder's charge, and the beam, every frame
void CEgon::WeaponFrame()
{
#ifndef CLIENT_DLL
	if (m_flNextSync < gpGlobals->time)
	{
		m_flNextSync = gpGlobals->time + 0.5;
		MESSAGE_BEGIN(MSG_ONE, gmsgWeaponSync, NULL, m_pPlayer->pev);
		WRITE_BYTE(WEAPON_EGON);
		WRITE_BYTE((m_bTractorOn ? 1 : 0) | (m_bHolding ? 2 : 0));
		WRITE_SHORT((int)(m_flHoldDist * 8));
		MESSAGE_END();
	}
#endif

	if (m_flHoldDist < 32)
		m_flHoldDist = 32;
	else if (m_flHoldDist > 8192)
		m_flHoldDist = 8192;

	m_flFrameInterval = gpGlobals->time - m_flLastFrame;
	m_flLastFrame = gpGlobals->time;

	if (m_iChargeType != 0)
	{
		if (m_iChargeType == 1)
		{
			// the secondary attack during the charge shrinks yourself
			if ((m_pPlayer->m_afButtonPressed & IN_ATTACK2) != 0)
			{
				if ((RC2Pref(RC2P_Egon_ImploderShrinkSelfAmmoCost).s & 0xffff) <= EGON_AMMO && RC2Pref(RC2P_Egon_ImploderShrinkSelfEnergyCost).f <= m_pPlayer->m_flEnergy)
				{
					PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usImploderFire, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 1);
					ShrinkSelf();
					return;
				}
				PlayEmptySound();
			}

			if (UTIL_WeaponTimeBase() >= m_pPlayer->m_flNextAmmoBurn)
			{
				m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_ImploderChargeAmmoBurnDelay).f;
				EGON_AMMO -= RC2Pref(RC2P_Egon_ImploderChargeAmmoPerBurn).s & 0xffff;
				if (EGON_AMMO < 0)
					EGON_AMMO = 0;
			}
		}

		if (m_flChargeDone < gpGlobals->time)
		{
			if (m_iChargeType == 1)
				ImploderFire();
			return;
		}

		// still charging: hold every attack
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 1.0;
		m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
		return;
	}

	if (!m_bTractorOn)
		return;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1;

	// the beam burns ammo, faster when it holds something
	if (UTIL_WeaponTimeBase() > m_pPlayer->m_flNextAmmoBurn)
	{
		if (m_bHolding)
			m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TractorBurnTimeHasTarget).f;
		else
			m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Egon_TractorBurnTimeNoTarget).f;

		if (EGON_AMMO < (RC2Pref(RC2P_Egon_TractorAmmoUsePerBurn).s & 0xffff))
		{
			EGON_EXTEND4_KEYS(TractorNoAmmo);
			TractorStop();
			return;
		}
		EGON_AMMO -= RC2Pref(RC2P_Egon_TractorAmmoUsePerBurn).s & 0xffff;
	}

	// let go of the button
	if ((m_pPlayer->pev->button & IN_ATTACK) == 0)
	{
		EGON_EXTEND4_KEYS(TractorOff);
		TractorStop();
		return;
	}

	if (m_bAdjusting)
	{
		PrimaryAttack();
		m_bAdjusting = false;
	}

#ifdef CLIENT_DLL
	// the client's beam, every frame: the hold distance, and 1 off target or 2 holding
	PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usTractor, 0, g_vecZero, g_vecZero, m_flHoldDist, 0, 0, 0, (m_bHolding ? 1 : 0) + 1, 0);
#else
	if (!m_bHolding || !m_hTractor)
	{
		m_bHolding = false;
		m_hTractor = NULL;
		return;
	}

	if (!TractorHeld(m_hTractor))
	{
		// knocked out by another beam
		if (TractorKnocked(m_hTractor))
		{
			EGON_EXTEND4_KEYS(TractorDrop);
			TractorStop();
			return;
		}
		TractorHeld(m_hTractor) = 0;
		TractorKnocked(m_hTractor) = 0;
		m_bHolding = false;
		m_hTractor = NULL;
		m_flNextSync = gpGlobals->time;
		return;
	}

	// VERIFY: slot 75's first argument is the player here (cbase.h has an int)
	if ((m_hTractor->GetRCCaps(m_pPlayer, this) & 0x80) != 0 && (!m_bTargetAlive || m_hTractor->IsAlive()))
	{
		// hold it where the beam points
		Vector vecSrc = m_pPlayer->GetGunPosition();
		UTIL_MakeVectors(m_pPlayer->pev->punchangle + m_pPlayer->pev->v_angle);
		m_hTractor->m_vecRC2Base58 = vecSrc + gpGlobals->v_forward * m_flHoldDist;
		return;
	}

	TractorHeld(m_hTractor) = 0;
	m_bHolding = false;
	m_hTractor = NULL;
	m_flNextSync = gpGlobals->time;
#endif
}

#undef EGON_EXTEND4
#undef EGON_EXTEND4_KEYS
#undef EGON_AMMO

class CEgonAmmo : public CBasePlayerAmmo
{
	void Spawn() override // orig 0x3fef0, orig client 0xdc10
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_chainammo.mdl");
		CBasePlayerAmmo::Spawn();
	}
	void Precache() override // orig 0x3ff20, orig client 0xdc50
	{
		PRECACHE_MODEL("models/w_chainammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	bool AddAmmo(CBaseEntity* pOther) override // orig 0x3ff40, orig client 0xdc80
	{
		// VERIFY: RC2's GiveAmmo takes a fourth argument, 0 here (cbase.h has the SDK's three)
		if (pOther->GiveAmmo(RC2Pref(RC2P_Ammo_CellBoxGive).s, "uranium", RC2Pref(RC2P_Ammo_MaxCarryCells).s) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
// orig client 0xdae0 (and its GetClassPtr<CEgonAmmo>, orig client 0xdb90); the inline iItemSlot and
// UseDecrement are orig client 0xdd00 and orig client 0xdd20
LINK_ENTITY_TO_CLASS(ammo_egonclip, CEgonAmmo); // orig 0x3fe80

