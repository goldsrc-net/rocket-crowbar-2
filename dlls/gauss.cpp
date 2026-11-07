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
*	Rocket Crowbar 2 replaced the gauss gun with a teleporter driven by a beacon the player
*	throws (CGaussBecon, gauss_becon). With the beacon out the four attacks are: primary, a beam
*	that teleports what it hits to the beacon; secondary, teleports the player to it; tertiary,
*	charges and then teleports everything near the aim point to it; quaternary, brings it back.
*	Without one: primary swaps places with what the beam hits, secondary goes to a random spawn
*	point, tertiary blinks the player to the aim point, quaternary throws the beacon. Ammo is
*	"uranium"; every cost and refire time is in rc2.ini's [Gauss] heading.
*
*	The client compiles this file for its weapon prediction (cl_dll/hl/hl_weapons.cpp). RC2's
*	client is its own library, so the client build gets inert bodies here.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "soundent.h"
#include "shake.h"
#include "gamerules.h"
#include "UserMessages.h"

#ifndef CLIENT_DLL
#include "rc2prefs.h"
#include "gaussbecon.h"
#include "p3b_temp.h" // TEMP STUBs: RC2's messages and helpers from other files
#endif

#define RC2_WEAPON_GAUSS 9 // RC2's weapon id for the gauss (SDK 2.3: 7)

// RC2's bits in the answers of GetRCCaps / GetRCCaps2 the gauss tests. Names ours.
#define RC2CAP_GAUSS_TELEPORTABLE 0x20 // GetRCCaps: the gauss may move it
#define RC2CAP2_NO_TELEFRAG 0x4		   // GetRCCaps2: a teleport onto it fails instead of killing it

#define GAUSS_TELEFRAG_DAMAGE 50000

LINK_ENTITY_TO_CLASS(weapon_gauss, CGauss); // orig 0x4ca70

#ifndef CLIENT_DLL

static int GaussPref(rc2pref_e i) { return RC2Pref(i).s; } // the costs are shorts in rc2.ini
static float GaussPrefF(rc2pref_e i) { return RC2Pref(i).f; }

// GetRCCaps/GetRCCaps2 as the gauss asks them: the original passes (m_pPlayer, this). VERIFY:
// cbase.h declares the first argument an int; the player's index goes there until the merge
// settles the signature.
static int GaussCaps(CBaseEntity* pEnt, CGauss* pGauss)
{
	return pEnt->GetRCCaps(pGauss->m_pPlayer->entindex(), pGauss);
}
static int GaussCaps2(CBaseEntity* pEnt, CGauss* pGauss)
{
	return pEnt->GetRCCaps2(pGauss->m_pPlayer->entindex(), pGauss);
}

#define GAUSS_AMMO (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])

#endif

// orig 0x4cae0
void CGauss::Spawn()
{
	Precache();
	m_iId = RC2_WEAPON_GAUSS;
	SET_MODEL(ENT(pev), "models/w_gauss.mdl");
#ifndef CLIENT_DLL
	m_iDefaultAmmo = RC2Pref(RC2P_Ammo_GaussPickupGive).s;
	m_flNextBeconSync = gpGlobals->time;
#endif
	m_iTertiaryState = 0;
	m_fInAttack = 0;
	m_blHasBecon = false;

	FallInit(); // get ready to fall down.
}

// orig 0x4cb40
void CGauss::Precache()
{
	PRECACHE_MODEL("models/w_gauss.mdl");
	PRECACHE_MODEL("models/v_gauss.mdl");
	PRECACHE_MODEL("models/p_gauss.mdl");
	PRECACHE_MODEL("sprites/xspark2.spr");
	PRECACHE_MODEL("sprites/lgtning.spr");
	PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_MODEL("sprites/xflare1.spr");
	PRECACHE_MODEL("sprites/redflare1.spr");

	PRECACHE_SOUND("items/9mmclip1.wav");
	PRECACHE_SOUND("debris/beamstart4.wav");
	PRECACHE_SOUND("debris/beamstart7.wav");
	PRECACHE_SOUND("ambience/pulsemachine.wav");
	PRECACHE_SOUND("weapons/electro4.wav");
	PRECACHE_SOUND("weapons/electro6.wav");
	PRECACHE_SOUND("weapons/mine_deploy.wav");

	UTIL_PrecacheOther("gauss_becon");

	m_usGaussPrimary = PRECACHE_EVENT(1, "events/gaussprimary.sc");
	m_usGaussBecon = PRECACHE_EVENT(1, "events/gaussbecon.sc");
	m_usGaussPrimaryDeny = PRECACHE_EVENT(1, "events/gaussprimarydeny.sc");
	m_usTeleportFlash = PRECACHE_EVENT(1, "events/teleportflash.sc");
	m_usGaussCharge = PRECACHE_EVENT(1, "events/gausscharge.sc");
	m_usGaussThird = PRECACHE_EVENT(1, "events/gaussthird.sc");
}

// orig 0x4d310. RC2 (SDK 2.3) sends WeapPickup here after the base; halflife-updated sends it
// from CBasePlayer::AddPlayerItem, so this only calls the base (as mod-rockcrowbar does).
void CGauss::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
}

// orig 0x4d370
bool CGauss::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
#ifndef CLIENT_DLL
	p->iMaxAmmo1 = RC2Pref(RC2P_Ammo_MaxCarryCells).s;
#else
	p->iMaxAmmo1 = URANIUM_MAX_CARRY;
#endif
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 1;
	p->iId = m_iId = RC2_WEAPON_GAUSS;
	p->iFlags = 0;
	p->iWeight = 12;

	return true;
}

// orig 0x4d3e0
bool CGauss::Deploy()
{
	m_iTertiaryState = 0;
	m_fInAttack = 0;
	m_pPlayer->m_flPlayAftershock = 0.0;
	return DefaultDeploy("models/v_gauss.mdl", "models/p_gauss.mdl", GAUSS_DRAW, "gauss");
}

// orig 0x4d450: not while charging the tertiary attack
bool CGauss::CanHolster()
{
	return m_fInAttack == 0;
}

// orig 0x4d410
void CGauss::Holster()
{
	m_iTertiaryState = 0;
	m_fInAttack = 0;
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim(GAUSS_HOLSTER);
}

// orig 0x50260. No idle animations in RC2: only the next idle time.
void CGauss::WeaponIdle()
{
	ResetEmptySound();

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	float flRand = RANDOM_FLOAT(0, 1);
	if (flRand <= 0.5)
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	else if (flRand <= 0.75)
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	else
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3;
}

#ifdef CLIENT_DLL

// RC2's client predicts nothing of this weapon here (its client.dll is reconstructed apart).
void CGauss::PrimaryAttack() {}
void CGauss::SecondaryAttack() {}
void CGauss::TertiaryAttack() {}
void CGauss::QuaternaryAttack() {}
void CGauss::WeaponFrame() {}
void CGauss::ReadWeaponState(void* pData, int iSize) {}

#else

// orig 0x50330
void CGauss::ReadWeaponState(void* pData, int iSize)
{
	m_blHasBecon = RC2ReadStateBool(&pData, &iSize);
}

// orig 0x4d460: the beacon this player threw (pev->dmg_inflictor names the thrower), or NULL
CGaussBecon* CGauss::FindBecon()
{
	CBaseEntity* pBecon = NULL;
	while ((pBecon = UTIL_FindEntityByClassname(pBecon, "gauss_becon")) != NULL)
	{
		if (pBecon->pev->dmg_inflictor == m_pPlayer->edict())
			return (CGaussBecon*)pBecon;
	}
	return NULL;
}

// Every attack's refusal: all four timers at least flDelay from now. (Inline in the original.)
void CGauss::RaiseAttackTimes(float flDelay)
{
	RC2RaiseTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), flDelay);
	RC2RaiseTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), flDelay);
	RC2RaiseTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), flDelay);
	RC2RaiseTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), flDelay);
}

// orig 0x4cc80: room for pEnt at vecSpot (RC 1.9's TeleIsValidSpot, here with the gamerules'
// and the target's say)
bool CGauss::CanPlaceAt(CBaseEntity* pEnt, Vector vecSpot)
{
	TraceResult tr;

	if (!RC2GameRulesAllowGaussTeleport(m_pPlayer, pEnt))
		return false;

	if (pEnt->IsPlayer())
	{
		CBasePlayer* pPlayer = (CBasePlayer*)pEnt;
		if (pPlayer->m_bRC2NoTeleport)
			return false;
		int iMe = ENTINDEX(m_pPlayer->edict());
		if (pPlayer->m_iRC2LinkedEnt0 == iMe || pPlayer->m_iRC2LinkedEnt1 == iMe || pPlayer->m_iRC2LinkedEnt2 == iMe)
			return false;
	}

	int iContents = UTIL_PointContents(vecSpot);
	if (iContents == CONTENTS_SOLID || iContents == CONTENTS_SKY)
		return false;

	UTIL_TraceHull(vecSpot, Vector(vecSpot.x, vecSpot.y, vecSpot.z + 1), ignore_monsters, pEnt->GetGuassHull(), pEnt->edict(), &tr);
	if (tr.flFraction < 1.0 || 0 != tr.fAllSolid || 0 != tr.fStartSolid)
		return false;

	// VERIFY: which end of these two lines is the start (moot unless a surface is one-sided)
	UTIL_TraceLine(vecSpot + pEnt->pev->mins, vecSpot + pEnt->pev->maxs, ignore_monsters, pEnt->edict(), &tr);
	if (tr.flFraction < 1.0 || 0 != tr.fAllSolid || 0 != tr.fStartSolid)
		return false;

	UTIL_TraceLine(Vector(vecSpot.x + pEnt->pev->maxs.x, vecSpot.y + pEnt->pev->maxs.y, vecSpot.z + pEnt->pev->mins.z),
		Vector(vecSpot.x + pEnt->pev->mins.x, vecSpot.y + pEnt->pev->mins.y, vecSpot.z + pEnt->pev->maxs.z),
		ignore_monsters, pEnt->edict(), &tr);
	if (tr.flFraction < 1.0 || 0 != tr.fAllSolid || 0 != tr.fStartSolid)
		return false;

	return true;
}

// orig 0x4cf70: what pInBox (found where pMover lands) does about it. True when the landing can
// go ahead: pInBox doesn't count, or the telefrag killed it.
bool CGauss::TeleFragEnt(CBaseEntity* pMover, CBaseEntity* pInBox, CBaseEntity* pSkip)
{
	CBaseEntity* pEnt = pInBox->GetEffectedEnt();

	if (pMover->edict() == pEnt->edict())
		return true;
	if (pSkip && pSkip->edict() == pEnt->edict())
		return true;
	if (pEnt->IsBSPModel())
		return true;
	if (pEnt->pev->solid != SOLID_BBOX && pEnt->pev->solid != SOLID_SLIDEBOX)
		return true;
	if ((GaussCaps(pEnt, this) & RC2CAP_GAUSS_TELEPORTABLE) == 0)
		return true;
	if (pEnt->pev->takedamage == DAMAGE_NO)
		return true;
	if (pEnt->pev->deadflag == DEAD_DYING)
		return true;

	// the one in the way dies, unless it is a teammate or can't be telefragged
	if (g_pGameRules->PlayerRelationship(m_pPlayer, pEnt) != GR_TEAMMATE && (GaussCaps2(pEnt, this) & RC2CAP2_NO_TELEFRAG) == 0)
		return pEnt->TakeDamage(RC2NamedInflictor("gauss_telefrag"), m_pPlayer->pev, GAUSS_TELEFRAG_DAMAGE, DMG_ALWAYSGIB);

	// then the one who moved, under the same two conditions
	if (g_pGameRules->PlayerRelationship(m_pPlayer, pMover) != GR_TEAMMATE && (GaussCaps2(pMover, this) & RC2CAP2_NO_TELEFRAG) == 0)
	{
		if ((GaussCaps2(pEnt, this) & RC2CAP2_NO_TELEFRAG) != 0)
			return false;
		return pMover->TakeDamage(RC2NamedInflictor("gauss_telefrag"), m_pPlayer->pev, GAUSS_TELEFRAG_DAMAGE, DMG_ALWAYSGIB);
	}

	// and failing both, the gauss's player
	m_pPlayer->TakeDamage(RC2NamedInflictor("gauss_telefrag"), m_pPlayer->pev, GAUSS_TELEFRAG_DAMAGE, DMG_ALWAYSGIB);
	return false;
}

// orig 0x4d160: telefrag everything in pMover's box (padded by 8) at vecSpot; false when
// something there stays
bool CGauss::TeleFragBox(CBaseEntity* pMover, Vector vecSpot, CBaseEntity* pSkip)
{
	CBaseEntity* pList[64];
	Vector vecMaxs = vecSpot + pMover->pev->size * 0.5 + Vector(8, 8, 8);
	Vector vecMins = vecSpot - pMover->pev->size * 0.5 - Vector(8, 8, 8);

	int iCount = UTIL_EntitiesInBox(pList, 64, vecMins, vecMaxs, 0);
	for (int i = 0; i < iCount; i++)
	{
		if (!TeleFragEnt(pMover, pList[i], pSkip))
			return false;
	}
	return true;
}

// orig 0x4d4c0: puts pEnt's center at vecDest, facing vecAngles, moving at vecVelocity, and makes
// everything that holds it let go
void CGauss::TeleportEnt(CBaseEntity* pEnt, Vector vecDest, Vector vecAngles, Vector vecVelocity)
{
	Vector vecCenter = pEnt->Center();
	UTIL_SetOrigin(pEnt->pev, vecDest + (pEnt->pev->origin - vecCenter));
	pEnt->pev->angles = vecAngles;
	pEnt->pev->ideal_yaw = pEnt->pev->angles.y;
	pEnt->pev->velocity = vecVelocity;
	pEnt->pev->flags &= ~FL_ONGROUND;

	if (pEnt->IsPlayer())
	{
		pEnt->pev->v_angle = pEnt->pev->angles;
		pEnt->pev->fixangle = 1;
	}

	for (int i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent))
			continue;
		CBaseEntity* pOther = CBaseEntity::Instance(pent);
		if (pOther)
			pOther->ReleasePull(pEnt);
	}
}

// orig 0x4d620: moves pEnt out from the beacon along its surface's normal (or onto it while it
// flies), telefragging what's there. False when there's no room.
bool CGauss::BeconTeleport(CGaussBecon* pBecon, CBaseEntity* pEnt, bool bFlashArrival)
{
	Vector vecDest;
	if (!pBecon->m_blAttached)
		vecDest = pBecon->pev->origin;
	else
		vecDest = pBecon->pev->origin + pBecon->m_vecPlaneNormal * pEnt->GetGuassDistOut();

	if (!CanPlaceAt(pEnt, vecDest))
		return false;

	if (bFlashArrival)
		TeleportFlash(pEnt, vecDest, true);
	TeleportFlash(pEnt, pEnt->Center(), false);

	if (!TeleFragBox(pEnt, vecDest, pBecon))
		return false;

	TeleportEnt(pEnt, vecDest, Vector(0, pBecon->pev->angles.y, 0), g_vecZero);
	return true;
}

// orig 0x4d7e0
void CGauss::PrimaryDenyEvent()
{
	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usGaussPrimaryDeny, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);
}

// orig 0x4d830: the teleport effect at vecOrigin, for pEnt (its index in iparam2)
void CGauss::TeleportFlash(CBaseEntity* pEnt, Vector vecOrigin, bool bArrival)
{
	PLAYBACK_EVENT_FULL(0, NULL, m_usTeleportFlash, 0, vecOrigin, g_vecZero, 0, 0, 0, ENTINDEX(pEnt->edict()), bArrival ? 1 : 0, 0);
}

// orig 0x4d890
void CGauss::PrimaryAttack()
{
	if (m_pPlayer->pev->waterlevel == 3 && !RC2PlayerWaterWeapons(m_pPlayer))
	{
		PlayEmptySound();
		RaiseAttackTimes(0.15);
		return;
	}

	if ((!HasBecon() && GAUSS_AMMO < GaussPref(RC2P_Gauss_PrimaryNoBeaconBaseCost)) || (HasBecon() && GAUSS_AMMO < GaussPref(RC2P_Gauss_PrimaryBaseCost)))
	{
		PlayEmptySound();
		RaiseAttackTimes(0.5);
		return;
	}

	if (!HasBecon() && m_pPlayer->m_iRC2LinkedEnt0 != 0)
	{
		PlayEmptySound();
		RaiseAttackTimes(0.5);
		return;
	}

	TraceResult tr;

	if (HasBecon())
	{
		m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;
		GAUSS_AMMO -= GaussPref(RC2P_Gauss_PrimaryBaseCost);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
		RC2RaiseTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_PrimaryRefirePrimary));
		RC2RaiseTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_PrimaryRefireSecondary));
		RC2RaiseTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_PrimaryRefireTertiary));
		RC2RaiseTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_PrimaryRefireQuaternary));
		m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);
		if (g_fRC2PlayWeaponEvents)
			PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usGaussPrimary, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

		CGaussBecon* pBecon = FindBecon();
		if (!pBecon)
			return;

		Vector vecSrc = m_pPlayer->GetGunPosition();
		UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
		UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);
		if (tr.flFraction >= 1.0)
			return;
		if (!UTIL_IsValidEntity(tr.pHit))
			return;
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (!pHit)
			return;
		CBaseEntity* pTarget = pHit->GetEffectedEnt();
		if (pTarget->IsBSPModel())
			return;

		if ((GaussCaps(pTarget, this) & RC2CAP_GAUSS_TELEPORTABLE) != 0 && GAUSS_AMMO >= GaussPref(RC2P_Gauss_PrimaryTeleportCost) && BeconTeleport(pBecon, pTarget, true))
		{
			m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + GaussPrefF(RC2P_Gauss_PrimaryTeleportRefire);
			// Original bug: checks PrimaryTeleportCost but takes PrimaryNoBeaconTeleportCost
			GAUSS_AMMO -= GaussPref(RC2P_Gauss_PrimaryNoBeaconTeleportCost);
			UTIL_Remove(pBecon);
			return;
		}
	}
	else
	{
		// no beacon: the beam swaps the player with what it hits
		m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;
		GAUSS_AMMO -= GaussPref(RC2P_Gauss_PrimaryNoBeaconBaseCost);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
		RC2RaiseTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_NoBeaconPrimaryRefirePrimary));
		RC2RaiseTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_NoBeaconPrimaryRefireSecondary));
		RC2RaiseTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_NoBeaconPrimaryRefireTertiary));
		RC2RaiseTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_NoBeaconPrimaryRefireQuaternary));
		m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);
		if (g_fRC2PlayWeaponEvents)
			PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usGaussPrimary, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 1, 0);

		Vector vecSrc = m_pPlayer->GetGunPosition();
		UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
		UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);
		if (tr.flFraction >= 1.0)
			return;
		if (!UTIL_IsValidEntity(tr.pHit))
			return;
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (!pHit)
			return;
		CBaseEntity* pTarget = pHit->GetEffectedEnt();
		if (pTarget->IsBSPModel())
			return;

		if ((GaussCaps(pTarget, this) & RC2CAP_GAUSS_TELEPORTABLE) != 0 && GAUSS_AMMO >= GaussPref(RC2P_Gauss_PrimaryNoBeaconTeleportCost))
		{
			EHANDLE hTarget;
			hTarget = pTarget;

			Vector vecTargetSpot = hTarget->Center();
			Vector vecPlayerSpot = m_pPlayer->Center();
			Vector vecTargetAngles = hTarget->pev->angles;
			Vector vecPlayerAngles = m_pPlayer->pev->v_angle;
			Vector vecTargetVel = hTarget->pev->velocity;
			Vector vecPlayerVel = m_pPlayer->pev->velocity;

			if ((hTarget->pev->flags & FL_ONGROUND) != 0)
				vecTargetSpot.z += 16;
			if ((m_pPlayer->pev->flags & FL_ONGROUND) != 0)
				vecPlayerSpot.z += 16;

			if (hTarget->IsPlayer())
			{
				vecTargetAngles = hTarget->pev->v_angle;
			}
			else
			{
				vecTargetAngles.x = -vecTargetAngles.x;
				vecPlayerAngles.x = 0;
			}

			if (CanPlaceAt(m_pPlayer, vecTargetSpot) && CanPlaceAt(hTarget, vecPlayerSpot))
			{
				// each may pass through the other while they move
				edict_t* pentPlayerOwner = m_pPlayer->pev->owner;
				edict_t* pentTargetOwner = hTarget->pev->owner;
				m_pPlayer->pev->owner = hTarget->edict();
				hTarget->pev->owner = m_pPlayer->edict();

				GAUSS_AMMO -= GaussPref(RC2P_Gauss_PrimaryNoBeaconTeleportCost);
				// Original bug: PrimaryTeleportRefire, not PrimaryNoBeaconTeleportRefire
				m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + GaussPrefF(RC2P_Gauss_PrimaryTeleportRefire);

				TeleportFlash(m_pPlayer, vecTargetSpot, true);
				if (TeleFragBox(m_pPlayer, vecTargetSpot, hTarget))
				{
					TeleportFlash(hTarget, vecPlayerSpot, true);
					if (m_pPlayer->IsAlive() && hTarget && TeleFragBox(hTarget, vecPlayerSpot, m_pPlayer))
					{
						if (hTarget)
							hTarget->pev->owner = pentTargetOwner;
						m_pPlayer->pev->owner = pentPlayerOwner;
						TeleportEnt(hTarget, vecPlayerSpot, vecPlayerAngles, vecPlayerVel);
						TeleportEnt(m_pPlayer, vecTargetSpot, vecTargetAngles, vecTargetVel);
						return;
					}
				}

				if (hTarget)
					hTarget->pev->owner = pentTargetOwner;
				m_pPlayer->pev->owner = pentPlayerOwner;
			}
		}
	}

	PrimaryDenyEvent();
}

// orig 0x4e540
void CGauss::SecondaryAttack()
{
	if (m_pPlayer->pev->waterlevel == 3 && !RC2PlayerWaterWeapons(m_pPlayer))
	{
		PlayEmptySound();
		RaiseAttackTimes(0.15);
		return;
	}

	if (m_pPlayer->m_iRC2LinkedEnt0 != 0)
	{
		PlayEmptySound();
		RaiseAttackTimes(0.5);
		return;
	}

	if (HasBecon())
	{
		if (GAUSS_AMMO < GaussPref(RC2P_Gauss_SecondaryCost))
		{
			PlayEmptySound();
			RaiseAttackTimes(0.5);
			return;
		}

		// to the beacon
		m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
		RC2RaiseTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_SecondaryRefirePrimary));
		RC2RaiseTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_SecondaryRefireSecondary));
		RC2RaiseTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_SecondaryRefireTertiary));
		RC2RaiseTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_SecondaryRefireQuaternary));

		CGaussBecon* pBecon = FindBecon();
		if (!pBecon)
			return;

		if (!BeconTeleport(pBecon, m_pPlayer, true))
		{
			PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usGaussPrimaryDeny, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 1, 0);
			GAUSS_AMMO -= GaussPref(RC2P_Gauss_SecondaryDenyCost);
			if (GAUSS_AMMO < 0)
				GAUSS_AMMO = 0;
			return;
		}

		GAUSS_AMMO -= GaussPref(RC2P_Gauss_SecondaryCost);
		UTIL_Remove(pBecon);
		m_flNextBeconSync = gpGlobals->time;
		return;
	}

	if (GAUSS_AMMO < GaussPref(RC2P_Gauss_SecondaryNoBeaconCost))
	{
		PlayEmptySound();
		RaiseAttackTimes(0.5);
		return;
	}

	// to a random spawn point
	m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;
	GAUSS_AMMO -= GaussPref(RC2P_Gauss_SecondaryNoBeaconCost);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
	RC2RaiseTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_NoBeaconSecondaryRefirePrimary));
	RC2RaiseTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_NoBeaconSecondaryRefireSecondary));
	RC2RaiseTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_NoBeaconSecondaryRefireTertiary));
	RC2RaiseTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_NoBeaconSecondaryRefireQuaternary));

	TeleportFlash(m_pPlayer, m_pPlayer->pev->origin, false);

	CBaseEntity* pSpot = UTIL_FindRandomEntityByClassname("info_player_deathmatch");
	if (!pSpot)
		pSpot = UTIL_FindRandomEntityByClassname("info_player_coop");
	if (!pSpot)
		pSpot = UTIL_FindRandomEntityByClassname("info_player_start");
	if (!pSpot)
		return;

	if (!TeleFragBox(m_pPlayer, pSpot->pev->origin, pSpot))
		return;

	TeleportFlash(m_pPlayer, pSpot->pev->origin, false);
	TeleportEnt(m_pPlayer, pSpot->pev->origin, pSpot->pev->angles, g_vecZero);
}

// orig 0x4ea50: starts the charge; WeaponFrame burns ammo while it lasts and then fires it
void CGauss::TertiaryAttack()
{
	if (m_pPlayer->pev->waterlevel == 3 && !RC2PlayerWaterWeapons(m_pPlayer))
	{
		PlayEmptySound();
		RaiseAttackTimes(0.15);
		return;
	}

	if (!HasBecon() && m_pPlayer->m_iRC2LinkedEnt0 != 0)
	{
		PlayEmptySound();
		RaiseAttackTimes(0.5);
		return;
	}

	float flChargeTime;
	if (HasBecon())
	{
		int iNeeded = (int)(GaussPrefF(RC2P_Gauss_TertiaryChargeTime) / GaussPrefF(RC2P_Gauss_TertiaryBurnDelay)) * GaussPref(RC2P_Gauss_TertiaryBurnAmmount) + GaussPref(RC2P_Gauss_TertiaryTeleportCost);
		if (GAUSS_AMMO < iNeeded)
		{
			PlayEmptySound();
			RaiseAttackTimes(0.5);
			return;
		}

		if (g_fRC2PlayWeaponEvents)
			PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usGaussCharge, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 1, 0);

		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 1.0;
		m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
		m_iTertiaryState = 2;
		m_fInAttack = 2;
		flChargeTime = GaussPrefF(RC2P_Gauss_TertiaryChargeTime);
	}
	else
	{
		int iNeeded = (int)(GaussPrefF(RC2P_Gauss_TertiaryNoBeaconChargeTime) / GaussPrefF(RC2P_Gauss_TertiaryNoBeaconBurnDelay)) * GaussPref(RC2P_Gauss_TertiaryNoBeaconBurnAmmount) + GaussPref(RC2P_Gauss_TertiaryNoBeaconTeleportCost);
		if (GAUSS_AMMO < iNeeded)
		{
			PlayEmptySound();
			RaiseAttackTimes(0.5);
			return;
		}

		if (g_fRC2PlayWeaponEvents)
			PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usGaussCharge, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 1.0;
		m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
		m_iTertiaryState = 1;
		m_fInAttack = 1;
		flChargeTime = GaussPrefF(RC2P_Gauss_TertiaryNoBeaconChargeTime);
	}

	m_flTertiaryChargeEnd = flChargeTime + gpGlobals->time;
	m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase();
}

// orig 0x4ee80: throws the beacon, or brings it back (free when the player stands by it)
void CGauss::QuaternaryAttack()
{
	if (HasBecon())
	{
		CGaussBecon* pBecon = FindBecon();
		if (pBecon)
		{
			if ((m_pPlayer->pev->origin - pBecon->pev->origin).Length() < GaussPrefF(RC2P_Gauss_BeaconRetriveFreeDist))
			{
				EMIT_SOUND_DYN(pBecon->edict(), CHAN_WEAPON, "weapons/mine_deploy.wav", 1.0, 0.8, 0, PITCH_NORM);
				UTIL_Remove(pBecon);
				m_flNextBeconSync = gpGlobals->time;
				m_pPlayer->m_iWeaponVolume = 200;
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
				RC2RaiseTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_QuaternaryRefirePrimary));
				RC2RaiseTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_QuaternaryRefireSecondary));
				RC2RaiseTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_QuaternaryRefireTertiary));
				RC2RaiseTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_QuaternaryRefireQuaternary));
				return;
			}

			if (m_pPlayer->pev->waterlevel == 3 && !RC2PlayerWaterWeapons(m_pPlayer))
			{
				PlayEmptySound();
				RaiseAttackTimes(0.15);
				return;
			}

			if (GAUSS_AMMO < GaussPref(RC2P_Gauss_BeaconRetriveCost))
			{
				PlayEmptySound();
				m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 0.5;
				RaiseAttackTimes(0.5);
				return;
			}

			m_pPlayer->m_iWeaponVolume = 200;
			GAUSS_AMMO -= GaussPref(RC2P_Gauss_BeaconRetriveCost);
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
			RC2RaiseTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_QuaternaryRefirePrimary));
			RC2RaiseTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_QuaternaryRefireSecondary));
			RC2RaiseTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_QuaternaryRefireTertiary));
			RC2RaiseTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_QuaternaryRefireQuaternary));
			PLAYBACK_EVENT_FULL(0, NULL, m_usTeleportFlash, 0, pBecon->pev->origin, g_vecZero, 0, 0, 0, ENTINDEX(pBecon->edict()), 3, 0);
			UTIL_Remove(pBecon);
			m_flNextBeconSync = gpGlobals->time;
			return;
		}
	}

	if (m_pPlayer->pev->waterlevel == 3 && !RC2PlayerWaterWeapons(m_pPlayer))
	{
		PlayEmptySound();
		RaiseAttackTimes(0.15);
		return;
	}

	// throw it
	m_blHasBecon = true;
	m_pPlayer->m_iWeaponVolume = 200;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
	RC2RaiseTime(&m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_NoBeaconQuaternaryRefirePrimary));
	RC2RaiseTime(&m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_NoBeaconQuaternaryRefireSecondary));
	RC2RaiseTime(&m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_NoBeaconQuaternaryRefireTertiary));
	RC2RaiseTime(&m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), GaussPrefF(RC2P_Gauss_NoBeaconQuaternaryRefireQuaternary));
	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usGaussBecon, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);

	Vector vecSrc = m_pPlayer->GetGunPosition();
	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	CBaseEntity* pBecon = CBaseEntity::Create("gauss_becon", vecSrc, UTIL_VecToAngles(gpGlobals->v_forward), m_pPlayer->edict());
	pBecon->pev->velocity = gpGlobals->v_forward * GaussPrefF(RC2P_Gauss_BeaconLaunchVel);
	pBecon->pev->velocity.z += GaussPrefF(RC2P_Gauss_BeaconLaunchZBoost);
	pBecon->pev->dmg_inflictor = m_pPlayer->edict();
}

// orig 0x4f4e0: the tertiary charge without a beacon goes off: the player blinks to where the
// beam lands (onto a teleportable thing it hits when there's room above it). A beam into the sky
// or nothing kills the player ("in_space").
void CGauss::TertiaryFireNoBecon()
{
	if (g_fRC2PlayWeaponEvents)
	{
		m_iTertiaryState = 0;
		m_fInAttack = 0;
	}

	if (m_pPlayer->pev->waterlevel == 3 && !RC2PlayerWaterWeapons(m_pPlayer))
	{
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usGaussThird, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 2, 0);
		PlayEmptySound();
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	if (GAUSS_AMMO < GaussPref(RC2P_Gauss_TertiaryNoBeaconBurnAmmount))
	{
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usGaussThird, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 2, 0);
		PlayEmptySound();
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 0.5;
		return;
	}

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + GaussPrefF(RC2P_Gauss_NoBeaconTertiaryRefirePrimary);
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + GaussPrefF(RC2P_Gauss_NoBeaconTertiaryRefireSecondary);
	m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + GaussPrefF(RC2P_Gauss_NoBeaconTertiaryRefireTertiary);
	m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + GaussPrefF(RC2P_Gauss_NoBeaconTertiaryRefireQuaternary);
	m_pPlayer->m_iWeaponVolume = 600;
	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usGaussThird, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 0, 0);
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	TraceResult tr;
	Vector vecSrc = m_pPlayer->GetGunPosition();
	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecDir = gpGlobals->v_forward;
	UTIL_TraceLine(vecSrc, vecSrc + vecDir * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);

	Vector vecDest = tr.vecEndPos + tr.vecPlaneNormal * m_pPlayer->GetGuassDistOut();

	if (tr.flFraction < 1.0 && UTIL_IsValidEntity(tr.pHit))
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (pHit && !pHit->IsBSPModel() && (GaussCaps(pHit->GetEffectedEnt(), this) & RC2CAP_GAUSS_TELEPORTABLE) != 0)
		{
			if (CanPlaceAt(m_pPlayer, pHit->Center() + Vector(0, 0, 8)))
				vecDest = pHit->Center() + Vector(0, 0, 8);
		}
	}

	if (tr.flFraction >= 1.0 || UTIL_PointContents(tr.vecEndPos) == CONTENTS_SKY)
	{
		if (tr.flFraction < 1.0)
		{
			if (tr.flFraction <= 0.03125)
				vecDest = tr.vecEndPos - vecDir * 16;
			else
				vecDest = tr.vecEndPos - vecDir * 256;
			UTIL_SetOrigin(m_pPlayer->pev, vecDest);
		}
		m_pPlayer->TakeDamage(RC2NamedInflictor("in_space"), m_pPlayer->pev, GAUSS_TELEFRAG_DAMAGE, DMG_ALWAYSGIB);
		return;
	}

	if (!CanPlaceAt(m_pPlayer, vecDest))
		return;

	TeleportFlash(m_pPlayer, m_pPlayer->Center(), false);
	TeleportFlash(m_pPlayer, vecDest, false);

	if (!TeleFragBox(m_pPlayer, vecDest, NULL))
		return;

	Vector vecAngles = UTIL_VecToAngles(-vecDir);
	vecAngles.x = -vecAngles.x;
	TeleportEnt(m_pPlayer, vecDest, vecAngles, g_vecZero);
	GAUSS_AMMO -= GaussPref(RC2P_Gauss_TertiaryNoBeaconBurnAmmount);
}

// orig 0x4fca0: the tertiary charge with a beacon goes off: everything teleportable near where
// the beam lands goes to the beacon, the first with a flash; when more than one went, the last
// one is telefragged (they land on each other).
void CGauss::TertiaryFireBecon()
{
	if (g_fRC2PlayWeaponEvents)
	{
		m_iTertiaryState = 0;
		m_fInAttack = 0;
	}

	if (m_pPlayer->pev->waterlevel == 3 && !RC2PlayerWaterWeapons(m_pPlayer))
	{
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usGaussThird, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 2, 0);
		PlayEmptySound();
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	// Original oddity: the no-beacon burn amount is both the test and the cost here
	if (GAUSS_AMMO < GaussPref(RC2P_Gauss_TertiaryNoBeaconBurnAmmount))
	{
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usGaussThird, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 2, 0);
		PlayEmptySound();
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 0.5;
		return;
	}

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + GaussPrefF(RC2P_Gauss_TertiaryRefirePrimary);
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + GaussPrefF(RC2P_Gauss_TertiaryRefireSecondary);
	m_flNextTertiaryAttack = UTIL_WeaponTimeBase() + GaussPrefF(RC2P_Gauss_TertiaryRefireTertiary);
	m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + GaussPrefF(RC2P_Gauss_TertiaryRefireQuaternary);
	m_pPlayer->m_iWeaponVolume = 600;
	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usGaussThird, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 1, 0);
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	GAUSS_AMMO -= GaussPref(RC2P_Gauss_TertiaryNoBeaconBurnAmmount);

	CGaussBecon* pBecon = FindBecon();
	if (!pBecon)
	{
		PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_usGaussPrimaryDeny, 0, g_vecZero, g_vecZero, 0, 0, 0, 0, 2, 0);
		return;
	}

	TraceResult tr;
	Vector vecSrc = m_pPlayer->GetGunPosition();
	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);

	int iCount = 0;
	EHANDLE hLast;
	hLast = NULL;
	Vector vecCenter = tr.vecEndPos + tr.vecPlaneNormal * 8;

	RC2EntMarkPush();

	CBaseEntity* pEnt = NULL;
	Vector vecNear;
	while ((pEnt = RC2FindEntityNear(pEnt, vecCenter, GaussPrefF(RC2P_Gauss_TertiaryTeleportRadius), NULL, &vecNear)) != NULL)
	{
		if ((GaussCaps(pEnt, this) & RC2CAP_GAUSS_TELEPORTABLE) == 0)
			continue;
		if (pEnt->GetEffectedEnt()->edict() == m_pPlayer->edict())
			continue;
		if (!RC2GameRulesAllowGaussTeleport(m_pPlayer, pEnt))
			continue;
		if (pEnt->IsBSPModel())
			continue;
		if (pEnt->IsPlayer())
		{
			if (!pEnt->IsActivePlayer() || !pEnt->IsAlive() || ((CBasePlayer*)pEnt)->m_iRC2LinkedEnt0 != 0)
				continue;
		}
		if (RC2EntMarkTest(ENTINDEX(pEnt->GetEffectedEnt()->edict())))
			continue;
		if (iCount != 0 && pEnt->IsPlayer() && !g_pGameRules->FPlayerCanTakeDamage((CBasePlayer*)pEnt, m_pPlayer))
			continue;

		TraceResult trSee;
		UTIL_TraceLine(vecCenter, vecNear, ignore_monsters, m_pPlayer->edict(), &trSee);
		if (trSee.flFraction < 1.0 && trSee.pHit != pEnt->edict())
			continue;

		if (BeconTeleport(pBecon, pEnt->GetEffectedEnt(), iCount == 0))
		{
			hLast = pEnt->GetEffectedEnt();
			iCount++;
		}
		RC2EntMarkSet(ENTINDEX(pEnt->GetEffectedEnt()->edict()));
	}

	RC2EntMarkPop();

	if (iCount > 1 && hLast)
		hLast->TakeDamage(RC2NamedInflictor("gauss_telefrag"), m_pPlayer->pev, GAUSS_TELEFRAG_DAMAGE, DMG_ALWAYSGIB);
}

// orig 0x50360: tells the client whether the beacon is out, burns ammo while the tertiary charges
// and fires it when the charge is done
void CGauss::WeaponFrame()
{
	if (m_flNextBeconSync < gpGlobals->time)
	{
		m_flNextBeconSync = gpGlobals->time + 0.25;
		m_blHasBecon = HasBecon();
		MESSAGE_BEGIN(MSG_ONE, gmsgWeaponSync, NULL, m_pPlayer->pev);
		WRITE_BYTE(RC2_WEAPON_GAUSS);
		WRITE_BYTE(m_blHasBecon ? 1 : 0);
		MESSAGE_END();
	}

	if (m_iTertiaryState == 0)
		return;

	if (m_iTertiaryState == 1)
	{
		if (UTIL_WeaponTimeBase() >= m_pPlayer->m_flNextAmmoBurn)
		{
			m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + GaussPrefF(RC2P_Gauss_TertiaryNoBeaconBurnDelay);
			GAUSS_AMMO -= GaussPref(RC2P_Gauss_TertiaryNoBeaconBurnAmmount);
			if (GAUSS_AMMO < 0)
				GAUSS_AMMO = 0;
		}
	}
	else if (m_iTertiaryState == 2)
	{
		if (UTIL_WeaponTimeBase() >= m_pPlayer->m_flNextAmmoBurn)
		{
			m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + GaussPrefF(RC2P_Gauss_TertiaryBurnDelay);
			GAUSS_AMMO -= GaussPref(RC2P_Gauss_TertiaryBurnAmmount);
			if (GAUSS_AMMO < 0)
				GAUSS_AMMO = 0;
		}
	}

	if (gpGlobals->time <= m_flTertiaryChargeEnd)
	{
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flNextQuaternaryAttack = UTIL_WeaponTimeBase() + 1.0;
		m_flTimeWeaponIdle = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	}
	else if (m_iTertiaryState == 1)
	{
		TertiaryFireNoBecon();
	}
	else if (m_iTertiaryState == 2)
	{
		TertiaryFireBecon();
	}
}

#endif // CLIENT_DLL


class CGaussAmmo : public CBasePlayerAmmo
{
	// orig 0x505e0
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_gaussammo.mdl");
		CBasePlayerAmmo::Spawn();
	}
	// orig 0x50610
	void Precache() override
	{
		PRECACHE_MODEL("models/w_gaussammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// orig 0x50630. VERIFY: RC2's GiveAmmo takes a fourth argument (0 here); cbase.h has three.
	bool AddAmmo(CBaseEntity* pOther) override
	{
#ifndef CLIENT_DLL
		if (pOther->GiveAmmo(RC2Pref(RC2P_Ammo_CellBoxGive).s, "uranium", RC2Pref(RC2P_Ammo_MaxCarryCells).s) != -1)
#else
		if (pOther->GiveAmmo(AMMO_URANIUMBOX_GIVE, "uranium", URANIUM_MAX_CARRY) != -1)
#endif
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_gaussclip, CGaussAmmo); // orig 0x50570
