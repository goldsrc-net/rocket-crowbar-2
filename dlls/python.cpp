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
#include "weapons.h"
#include "monsters.h"
#include "player.h"
#include "gamerules.h"
#include "UserMessages.h"


//=========================================================
// Rocket Crowbar 2's python. Primary is a lightning bolt: a hitscan hurt one think later
// (DelayedHurt). Secondary charges chain lightning, which jumps from what it hits on to other
// players in reach (ChainTo); tertiary charges a ball lightning (ball_lightning). Under water
// each of them discharges into the water instead. Quaternary toggles the zoom. Every number is
// rc2.ini [Python]/[Ammo]. Reconstructed from rc2.dll 0xab930-0xadac0.
//=========================================================

#include <cmath>
#include <cstdlib>

#include "rc2prefs.h"
#ifndef CLIENT_DLL
#include "rc2/shield.h"
#include "rc2/balllightning.h"
#define PYTHON_MULTIPLAYER() (g_pGameRules->IsMultiplayer())
#else
// The client's prediction (client.dll, reference/dossiers/c1.md) compiles the same code without
// the hurts, the chain, the ball, the water discharges and the client effect messages; it asks
// bIsMultiplayer() where the server asks the game rules.
bool bIsMultiplayer();	// hl_weapons.cpp
#define PYTHON_MULTIPLAYER() (bIsMultiplayer())
#endif

#ifndef CLIENT_DLL
extern int gmsgCLTE; // RC2's "CLTE" user message (orig .bss 0x18f4b8), UserMessages.cpp

#define CLTE_KILL 0xfe // ours: kills a client effect of the type in the next byte

#endif

// orig 0xac2e0 is util.h's inline VARS(edict_t*), emitted out of line in this file; the SDK's is used

// CPython::iItemSlot (orig 0xadaa0) returns 2 and CPython::UseDecrement (orig 0xadab0) returns
// true: the inline ones in weapons.h (CLIENT_WEAPONS is always defined in this tree).

// VERIFY: the original's factories also call RC 1.9's AddToPrecachedList("weapon_python", "weapon_357") (0xf5a20); the shared macro doesn't yet
// orig client 0x3f2f0 and orig client 0x3f320 (and their GetClassPtr<CPython>, orig client 0x40ac0); the
// inline iItemSlot, UseDecrement and CanHolster are orig client 0x3bb30, orig client 0x3bb50 and
// orig client 0x3bb70
LINK_ENTITY_TO_CLASS(weapon_python, CPython); // orig 0xab930
LINK_ENTITY_TO_CLASS(weapon_357, CPython);	  // orig 0xab9a0

bool CPython::GetItemInfo(ItemInfo* p) // orig 0xaba10, orig client 0x3f350
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "357";
	p->iMaxAmmo1 = RC2Pref(RC2P_Ammo_MaxCarry357).s;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = RC2Pref(RC2P_Ammo_PythonClipSize).s;
	p->iFlags = 0;
	p->iSlot = 1;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_PYTHON;
	p->iWeight = PYTHON_WEIGHT;

	return true;
}

// orig 0xaba80, orig client 0x3f3e0. RC2 sends gmsgWeapPickup here once the base has added the weapon;
// halflife-updated sends that same message from CBasePlayer::AddPlayerItem, so it isn't
// repeated here (one message either way).
void CPython::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
}

void CPython::Spawn() // orig 0xabae0, orig client 0x3f460
{
	pev->classname = MAKE_STRING("weapon_357"); // hack to allow for old names
	Precache();
	m_iId = WEAPON_PYTHON;
	SET_MODEL(ENT(pev), "models/w_357.mdl");

	m_iDefaultAmmo = RC2Pref(RC2P_Ammo_PythonPickupGive).s;

	FallInit(); // get ready to fall down.
}

void CPython::Precache() // orig 0xabb40, orig client 0x3f4e0
{
	PRECACHE_MODEL("models/v_357.mdl");
	PRECACHE_MODEL("models/w_357.mdl");
	PRECACHE_MODEL("models/p_357.mdl");

	PRECACHE_MODEL("models/w_357ammobox.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND("weapons/357_reload1.wav");
	PRECACHE_SOUND("weapons/357_cock1.wav");
	PRECACHE_SOUND("weapons/357_shot1.wav");
	PRECACHE_SOUND("weapons/357_shot2.wav");
	PRECACHE_SOUND("weapons/pyth_prefire.wav");
	PRECACHE_MODEL("sprites/lgtning.spr");

	UTIL_PrecacheOther("ball_lightning");

	m_usPythonLightning = PRECACHE_EVENT(1, "events/pythonlightning.sc");
	m_usPythonFireball = PRECACHE_EVENT(1, "events/pythonfireball.sc");
	m_usPythonChain = PRECACHE_EVENT(1, "events/pythonchain.sc");
	m_usPythonPrefire = PRECACHE_EVENT(1, "events/pythonprefire.sc");
}

bool CPython::Deploy() // orig 0xabc20, orig client 0x3f600
{
	m_iChargeState = 0;
	m_fInAttack = 0;

	if (PYTHON_MULTIPLAYER())
	{
		// enable laser sight geometry.
		pev->body = 1;
	}
	else
	{
		pev->body = 0;
	}

	return DefaultDeploy("models/v_357.mdl", "models/p_357.mdl", PYTHON_DRAW, "python", pev->body);
}

void CPython::Holster() // orig 0xabc90, orig client 0x3f690
{
	m_fInReload = false; // cancel any reload in progress.
	m_iChargeState = 0;
	m_fInAttack = 0;

	if (0 != m_fInZoom)
	{
		QuaternaryAttack(); // the zoom toggle
	}

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	SendWeaponAnim(PYTHON_HOLSTER);
}

// The zoom toggle
void CPython::QuaternaryAttack() // orig 0xabd10, orig client 0x3f750
{
	if (m_pPlayer->pev->fov != 0)
	{
		m_fInZoom = 0;
		m_pPlayer->m_iFOV = 0; // 0 means reset to default fov
		m_pPlayer->pev->fov = 0;
	}
	else if (m_pPlayer->pev->fov != RC2Pref(RC2P_Python_ZoomedFOV).f)
	{
		m_fInZoom = 1;
		m_pPlayer->m_iFOV = (int)RC2Pref(RC2P_Python_ZoomedFOV).f;
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV;
	}

	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Python_QuaternaryRefirePrimary).f);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Python_QuaternaryRefireSecondary).f);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Python_QuaternaryRefireTertiary).f);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Python_QuaternaryRefireQuaternary).f);
}

// The lightning bolt: traced now, hurt on the next think (DelayedHurt)
void CPython::PrimaryAttack() // orig 0xabe10, orig client 0x3f890
{
	if (m_pPlayer->pev->waterlevel == 3 && !PlayerHasAttrFlag200(m_pPlayer))
	{
		PlayEmptySound();
		UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.15);
		return;
	}

	// the ammo costs are read zero-extended (movzx) in the original
	if (m_iClip < RC2Pref(RC2P_Python_PrimaryAmmoCost).us)
	{
		if (!m_fFireOnEmpty)
		{
			Reload();
			return;
		}

		EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "weapons/357_cock1.wav", 0.8, ATTN_NORM, 0, PITCH_NORM);
		UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.15);
		return;
	}

	m_iClip -= RC2Pref(RC2P_Python_PrimaryAmmoCost).us;

	if (0 == m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	Vector vecSrc = m_pPlayer->GetGunPosition();

	UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Python_PrimaryRefirePrimary).f);
	UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Python_PrimaryRefireSecondary).f);
	UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Python_PrimaryRefireTertiary).f);
	UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Python_PrimaryRefireQuaternary).f);
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

	// bparam1: fired under water
	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usPythonLightning, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, m_pPlayer->pev->waterlevel == 3 ? 1 : 0, 0);

#ifndef CLIENT_DLL
	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);

	if (UTIL_PointContents(tr.vecEndPos) == CONTENTS_WATER || m_pPlayer->pev->waterlevel == 3)
	{
		UTIL_RC2WaterDischarge(tr.vecEndPos + tr.vecPlaneNormal * 2.0, RC2Pref(RC2P_Python_PrimaryDischargeDamage).f,
			UTIL_RC2DamageInflictor("357_discharge"), m_pPlayer->pev);
		return;
	}

	if (!tr.pHit || !UTIL_IsValidEntity(tr.pHit))
		return;

	CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
	if (!pEntity || pEntity->pev->takedamage == DAMAGE_NO)
		return;

	m_hTarget = pEntity;
	m_trHurt = tr;
	m_vecHurtDir = gpGlobals->v_forward;
	SetThink(&CPython::DelayedHurt);
	pev->nextthink = gpGlobals->time + 0.001;
#endif
}

// Starts charging the chain lightning: it fires from WeaponFrame when the charge is done
void CPython::SecondaryAttack() // orig 0xac2f0, orig client 0x3fc50
{
	if (m_pPlayer->pev->waterlevel == 3 && !PlayerHasAttrFlag200(m_pPlayer))
	{
		PlayEmptySound();
		UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.15);
		return;
	}

	if (m_iClip < RC2Pref(RC2P_Python_ChainAmmoCost).us)
	{
		if (!m_fFireOnEmpty)
		{
			Reload();
			return;
		}

		EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "weapons/357_cock1.wav", 0.8, ATTN_NORM, 0, PITCH_NORM);
		UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.15);
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	// the whole cost up front (the ball's is taken over its charge)
	m_iClip -= RC2Pref(RC2P_Python_ChainAmmoCost).us;

	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usPythonPrefire, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, 0, 0);

	if (0 == m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	const float flNext = UTIL_WeaponTimeBase() + 1.0;
	m_flNextPrimaryAttack = flNext;
	m_flNextSecondaryAttack = flNext;
	m_flNextTertiaryAttack = flNext;
	m_flNextQuaternaryAttack = flNext;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

	m_iChargeState = 3;
	m_fInAttack = 3;
	m_flChargeDone = gpGlobals->time + RC2Pref(RC2P_Python_SharedChargeTime).f;

	SendWeaponAnim(PYTHON_IDLE1, PYTHON_MULTIPLAYER() ? 1 : 0);
}

// Starts charging the ball lightning: one round now, the rest of [Python] BallAmmoCost over
// the charge (WeaponFrame), and it fires from WeaponFrame when the charge is done
void CPython::TertiaryAttack() // orig 0xac5b0, orig client 0x3ff90
{
	if (m_pPlayer->pev->waterlevel == 3 && !PlayerHasAttrFlag200(m_pPlayer))
	{
		PlayEmptySound();
		UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.15);
		return;
	}

	if (m_iClip < RC2Pref(RC2P_Python_BallAmmoCost).us)
	{
		if (!m_fFireOnEmpty)
		{
			Reload();
			return;
		}

		EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "weapons/357_cock1.wav", 0.8, ATTN_NORM, 0, PITCH_NORM);
		UTIL_ExtendTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), 0.15);
		UTIL_ExtendTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), 0.15);
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	// VERIFY: player +0x660 read as halflife-updated's m_flNextAmmoBurn (several weapons share it, the gauss's run among them)
	m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Python_SharedChargeTime).f / RC2Pref(RC2P_Python_BallAmmoCost).us;
	m_iClip--;

	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usPythonPrefire, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, 0, 0);

	const float flNext = UTIL_WeaponTimeBase() + 1.0;
	m_flNextPrimaryAttack = flNext;
	m_flNextSecondaryAttack = flNext;
	m_flNextTertiaryAttack = flNext;
	m_flNextQuaternaryAttack = flNext;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

	m_iChargeState = 2;
	m_fInAttack = 2;
	m_flChargeDone = gpGlobals->time + RC2Pref(RC2P_Python_SharedChargeTime).f;

	SendWeaponAnim(PYTHON_IDLE1, PYTHON_MULTIPLAYER() ? 1 : 0);
}

// The lightning bolt's hurt, one think after PrimaryAttack traced it
void CPython::DelayedHurt() // orig 0xac860, orig client 0x402b0 (empty on the client)
{
#ifndef CLIENT_DLL
	if (!m_hTarget)
		return;

	if (m_hTarget->pev->takedamage == DAMAGE_NO)
		return;

	// VERIFY: the last argument, 6, as RC2ShieldAbsorb reads it (the chain passes 8)
	const float flDamage = ShieldDeflect(m_hTarget, RC2Pref(RC2P_Python_PrimaryDamage).f, RC2Pref(RC2P_Python_PrimaryShieldFraction).f,
		m_trHurt.vecEndPos, m_vecHurtDir, NULL, NULL, 6);

	if (flDamage > 0)
	{
		ClearMultiDamage();
		m_hTarget->TraceAttack(m_pPlayer->pev, flDamage, m_vecHurtDir, &m_trHurt, DMG_SHOCK | DMG_ENERGYBEAM);
		ApplyMultiDamage(pev, m_pPlayer->pev);
	}

	m_hTarget = NULL;
	SetThink(NULL);
#endif
}

#ifndef CLIENT_DLL

// One jump of the chain lightning: from vecFrom (pentFrom, which the traces skip) to every
// player within [Python] ChainMaxChainDistance it can see that pHitBits (one bit per entity
// index) doesn't have yet. A link the shield lets through in full jumps on, iDepth times at
// most. The first link standing in water discharges into it instead (bDischarged after that).
void CPython::ChainTo(int iDepth, Vector vecFrom, edict_t* pentFrom, unsigned char* pHitBits, bool bDischarged) // orig 0xac970
{
	if (iDepth <= 0)
		return;

	// VERIFY: i < maxClients, as the original: the last player slot is never chained to
	for (int i = 1; i < gpGlobals->maxClients; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent))
			continue;

		CBaseEntity* pEntity = CBaseEntity::Instance(pent);
		if (!pEntity)
			continue;

		if (pEntity->pev->takedamage != DAMAGE_AIM)
			continue;

		if (pEntity->edict() == pentFrom || pEntity->edict() == m_pPlayer->edict())
			continue;

		const int iEnt = ENTINDEX(pEntity->edict());
		if ((pHitBits[iEnt / 8] & (1 << (iEnt % 8))) != 0)
			continue;

		if ((pEntity->Center() - vecFrom).Length() > RC2Pref(RC2P_Python_ChainMaxChainDistance).f)
			continue;

		TraceResult tr;
		UTIL_TraceLine(vecFrom, pEntity->Center(), ignore_monsters, pentFrom, &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEntity->edict())
			continue;

		const float flDamage = ShieldDeflect(pEntity, RC2Pref(RC2P_Python_ChainDamage).f, RC2Pref(RC2P_Python_ChainShieldFraction).f,
			tr.vecEndPos, (pEntity->Center() - vecFrom).Normalize(), NULL, NULL, 8);

		if (flDamage > 0)
		{
			if (!bDischarged && pEntity->pev->waterlevel >= 1 && flDamage == RC2Pref(RC2P_Python_ChainDamage).f)
			{
				Vector vecDischarge;
				if (pEntity->pev->waterlevel > 1)
					vecDischarge = pEntity->Center();
				else
					vecDischarge = pEntity->pev->origin + Vector(0, 0, pEntity->pev->mins.z + 12);

				UTIL_RC2WaterDischarge(vecDischarge, RC2Pref(RC2P_Python_ChainSecondaryDischargeDamage).f,
					UTIL_RC2DamageInflictor("357_discharge"), m_pPlayer->pev);
				bDischarged = true;
			}
			else
			{
				// no check that Create worked, as the original
				CChainHurter* pHurter = static_cast<CChainHurter*>(CBaseEntity::Create("chain_hurter", tr.vecEndPos, g_vecZero, m_pPlayer->edict()));
				pHurter->m_hTarget = pEntity;
				pHurter->m_tr = tr;
				pHurter->m_vecDir = (pEntity->Center() - vecFrom).Normalize();
				pHurter->pev->dmg = flDamage;
			}
		}

		pHitBits[iEnt / 8] |= 1 << (iEnt % 8);

		if (flDamage == RC2Pref(RC2P_Python_ChainDamage).f)
			ChainTo(iDepth - 1, pEntity->Center(), pEntity->edict(), pHitBits, bDischarged);
	}
}

#endif

// The chain lightning, when its charge is done (WeaponFrame)
void CPython::FireChain() // orig 0xacea0, orig client 0x402f0
{
	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	Vector vecSrc = m_pPlayer->GetGunPosition();

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Python_SecondaryRefirePrimary).f;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Python_SecondaryRefireSecondary).f;
	m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Python_SecondaryRefireTertiary).f;
	m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Python_SecondaryRefireQuaternary).f;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

	if (g_fRC2PlayWeaponEvents)
	{
		m_iChargeState = 0;
		m_fInAttack = 0;
	}

	if (m_pPlayer->pev->waterlevel == 3)
	{
		// bparam1: fired under water
		if (g_fRC2PlayWeaponEvents)
			PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usPythonChain, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, 1, 0);

#ifndef CLIENT_DLL
		UTIL_RC2WaterDischarge(m_pPlayer->Center(), RC2Pref(RC2P_Python_ChainSelfDischargeDamage).f,
			UTIL_RC2DamageInflictor("357_discharge"), m_pPlayer->pev);
#endif
		return;
	}

	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usPythonChain, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, 0, 0);

#ifndef CLIENT_DLL
	// VERIFY: which client effect type 0x0e is (the charge's, presumably)
	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(CLTE_KILL);
	WRITE_SHORT(ENTINDEX(m_pPlayer->edict()));
	WRITE_BYTE(0x0e);
	MESSAGE_END();

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);

	if (UTIL_PointContents(tr.vecEndPos) == CONTENTS_WATER)
	{
		UTIL_RC2WaterDischarge(tr.vecEndPos + tr.vecPlaneNormal * 2.0, RC2Pref(RC2P_Python_ChainDischargeDamage).f,
			UTIL_RC2DamageInflictor("357_discharge"), m_pPlayer->pev);
		return;
	}

	if (!tr.pHit || !UTIL_IsValidEntity(tr.pHit))
		return;

	CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
	if (!pEntity || pEntity->pev->takedamage != DAMAGE_AIM)
		return;

	const float flDamage = ShieldDeflect(pEntity, RC2Pref(RC2P_Python_ChainDamage).f, RC2Pref(RC2P_Python_ChainShieldFraction).f,
		tr.vecEndPos, gpGlobals->v_forward, NULL, NULL, 8);
	if (flDamage <= 0)
		return;

	// no check that Create worked, as the original
	CChainHurter* pHurter = static_cast<CChainHurter*>(CBaseEntity::Create("chain_hurter", tr.vecEndPos, g_vecZero, m_pPlayer->edict()));
	pHurter->m_hTarget = pEntity;
	pHurter->m_tr = tr;
	pHurter->m_vecDir = gpGlobals->v_forward;
	pHurter->pev->dmg = flDamage;

	// One bit per entity index, but sized for maxClients only: the first target can be any
	// entity (a monster), and its bit can land past the end, as in the original
	const int iBytes = (int)ceil(gpGlobals->maxClients * 0.125);
	unsigned char* pHitBits = (unsigned char*)malloc(iBytes);
	for (int i = 0; i < (int)ceil(gpGlobals->maxClients * 0.125); i++)
		pHitBits[i] = 0;

	const int iEnt = ENTINDEX(pEntity->edict());
	pHitBits[iEnt / 8] |= 1 << (iEnt % 8);

	ChainTo(8, pEntity->Center(), pEntity->edict(), pHitBits, false);

	free(pHitBits);
#endif
}

// The ball lightning, when its charge is done (WeaponFrame)
void CPython::FireBall() // orig 0xad3c0, orig client 0x404f0
{
	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	Vector vecSrc = m_pPlayer->GetGunPosition();

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Python_TertiaryRefirePrimary).f;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Python_TertiaryRefireSecondary).f;
	m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Python_TertiaryRefireTertiary).f;
	m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Python_TertiaryRefireQuaternary).f;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);

	if (g_fRC2PlayWeaponEvents)
	{
		m_iChargeState = 0;
		m_fInAttack = 0;
	}

	if (m_pPlayer->pev->waterlevel == 3)
	{
		// bparam1: fired under water
		if (g_fRC2PlayWeaponEvents)
			PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usPythonFireball, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, 1, 0);

#ifndef CLIENT_DLL
		UTIL_RC2WaterDischarge(m_pPlayer->Center(), RC2Pref(RC2P_Python_BallSelfDischargeDamage).f,
			UTIL_RC2DamageInflictor("357_discharge"), m_pPlayer->pev);
#endif
		return;
	}

	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usPythonFireball, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, 0, 0);

#ifndef CLIENT_DLL
	// VERIFY: which client effect type 0x0e is (the charge's, presumably)
	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(CLTE_KILL);
	WRITE_SHORT(ENTINDEX(m_pPlayer->edict()));
	WRITE_BYTE(0x0e);
	MESSAGE_END();

	// no check that Create worked, as the original
	CBaseEntity* pBall = CBaseEntity::Create("ball_lightning", vecSrc, g_vecZero, m_pPlayer->edict());
	pBall->pev->movedir = gpGlobals->v_forward;
	pBall->pev->velocity = pBall->pev->movedir * RC2Pref(RC2P_Python_BallLaunchVelocity).f;

	// riding on someone's shoulders: the ball is the ridden player's, so it doesn't hit them
	// (ball_lightning's Spawn already took the shooter as its dmg_inflictor)
	if (0 != m_pPlayer->m_iRidingIndex)
	{
		edict_t* pentRidden = INDEXENT(m_pPlayer->m_iRidingIndex);
		if (UTIL_IsValidEntity(pentRidden))
			pBall->pev->owner = pentRidden;
	}
#endif
}

void CPython::Reload() // orig 0xad6b0, orig client 0x406f0
{
	if (m_pPlayer->ammo_357 <= 0 || 0 != m_fInAttack)
		return;

	if (m_pPlayer->pev->fov != 0)
	{
		m_fInZoom = 0;
		m_pPlayer->m_iFOV = 0; // 0 means reset to default fov
		m_pPlayer->pev->fov = 0;
	}

	if (DefaultReload(RC2Pref(RC2P_Ammo_PythonClipSize).s, PYTHON_RELOAD, RC2Pref(RC2P_Python_ReloadTime).f, PYTHON_MULTIPLAYER() ? 1 : 0))
		m_flReloadSoundTime = 1.5;
}

void CPython::WeaponIdle() // orig 0xad730, orig client 0x40790
{
	if (0 != m_fInAttack)
		return;

	ResetEmptySound();

	// VERIFY: nothing on the server counts m_flReloadSoundTime down (UTIL_WeaponTimeBase() is 0
	// there and SDK 2.3 has no DecrementTimers), so this plays only in client prediction
	if (m_flReloadSoundTime != 0 && UTIL_WeaponTimeBase() >= m_flReloadSoundTime)
	{
		EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "weapons/357_reload1.wav", RANDOM_FLOAT(0.8, 0.9), ATTN_NORM, 0, PITCH_NORM);
		m_flReloadSoundTime = 0;
	}

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	int iAnim;
	// 10..15 where SDK 2.3 draws 0..1, as the original: always the fidget
	float flRand = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	if (flRand <= 0.5)
	{
		iAnim = PYTHON_IDLE1;
		m_flTimeWeaponIdle = (70.0 / 30.0);
	}
	else if (flRand <= 0.7)
	{
		iAnim = PYTHON_IDLE2;
		m_flTimeWeaponIdle = (60.0 / 30.0);
	}
	else if (flRand <= 0.9)
	{
		iAnim = PYTHON_IDLE3;
		m_flTimeWeaponIdle = (88.0 / 30.0);
	}
	else
	{
		iAnim = PYTHON_FIDGET;
		m_flTimeWeaponIdle = (170.0 / 30.0);
	}

	SendWeaponAnim(iAnim, PYTHON_MULTIPLAYER() ? 1 : 0);
}

// First thing in every ItemPostFrame: takes the ball's rounds over its charge, and fires
// the charge when it is done
void CPython::WeaponFrame() // orig 0xad880, orig client 0x40950
{
	if (0 == m_iChargeState)
		return;

	if (m_fInAttack == 2 && UTIL_WeaponTimeBase() >= m_pPlayer->m_flNextAmmoBurn)
	{
		m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Python_SharedChargeTime).f / RC2Pref(RC2P_Python_BallAmmoCost).us;
		m_iClip--;
		if (m_iClip < 0)
			m_iClip = 0;
	}

	if (m_flChargeDone < gpGlobals->time)
	{
		if (m_iChargeState == 3)
			FireChain();
		else if (m_iChargeState == 2)
			FireBall();
		return;
	}

	const float flNext = UTIL_WeaponTimeBase() + 1.0;
	m_flNextPrimaryAttack = flNext;
	m_flNextSecondaryAttack = flNext;
	m_flNextTertiaryAttack = flNext;
	m_flNextQuaternaryAttack = flNext;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
}

bool CPython::CanHolster() // orig 0xadac0
{
	return 0 == m_fInAttack;
}


class CPythonAmmo : public CBasePlayerAmmo
{
	void Spawn() override // orig 0xad9f0, orig client 0x40bc0
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_357ammobox.mdl");
		CBasePlayerAmmo::Spawn();
	}
	void Precache() override // orig 0xada20, orig client 0x40c00
	{
		PRECACHE_MODEL("models/w_357ammobox.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// VERIFY: RC2's GiveAmmo takes a fourth argument (0 here); halflife-updated's has three
	bool AddAmmo(CBaseEntity* pOther) override // orig 0xada40, orig client 0x40c30
	{
		if (pOther->GiveAmmo(RC2Pref(RC2P_Ammo_PythonSpareClipGive).s, "357", RC2Pref(RC2P_Ammo_MaxCarry357).s) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
// VERIFY: the original's factory also calls RC 1.9's AddToPrecachedList("ammo_357") (0xf5a20); the shared macro doesn't yet
// orig client 0x40a90 (and its GetClassPtr<CPythonAmmo>, orig client 0x40b40)
LINK_ENTITY_TO_CLASS(ammo_357, CPythonAmmo); // orig 0xad980



