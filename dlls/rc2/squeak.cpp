/***
*
*	Rocket Crowbar 2: the snark weapon's (CSqueak) four attacks and its weapon-sync pair.
*
*	In the original these follow SDK 2.3's squeakgrenade functions (at 0xd3440-0xd44ee), so
*	they are most likely the end of RC2's squeakgrenade.cpp; they are kept here because that
*	file is another work package's. Primary throws a snark, secondary a mini grunt, tertiary
*	sticks a snark cam on what the player aims at and quaternary looks through it. Each mode
*	has its own refire times for all four attacks in rc2.ini [Snark].
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "p8_needs.h"

// Primary: throw a snark, as SDK 2.3 did, but capped by SnarkMaxDeployed and costing
// SnarkAmmoCost, and owned by what the player drives when he drives something.
// orig 0xd3440
void CSqueak::PrimaryAttack()
{
	if (m_iSnarksDeployed > RC2PrefWord(RC2P_Snark_SnarkMaxDeployed))
		return;
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < RC2PrefWord(RC2P_Snark_SnarkAmmoCost))
		return;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	TraceResult tr;
	Vector trace_origin;

	// HACK HACK:  Ugly hacks to handle change in origin based on new physics code for players
	// Move origin up if crouched and start trace a bit outside of body ( 20 units instead of 16 )
	trace_origin = m_pPlayer->pev->origin;
	if ((m_pPlayer->pev->flags & FL_DUCKING) != 0)
	{
		trace_origin = trace_origin - (VEC_HULL_MIN - VEC_DUCK_HULL_MIN);
	}

	// find place to toss monster
	UTIL_TraceLine(trace_origin + gpGlobals->v_forward * 20, trace_origin + gpGlobals->v_forward * 64, dont_ignore_monsters, NULL, &tr);

	if (0 != g_iPlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usSnarkFire, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, 0, 0);

	if (tr.fAllSolid == 0 && tr.fStartSolid == 0 && tr.flFraction > 0.25)
	{
		// player "shoot" animation
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

		CBaseEntity* pSqueak = CBaseEntity::Create("monster_snark", tr.vecEndPos, m_pPlayer->pev->v_angle, m_pPlayer->edict());
		pSqueak->pev->velocity = gpGlobals->v_forward * RC2Pref(RC2P_Snark_SnarkLaunchVel).f + m_pPlayer->pev->velocity;

		if (0 != m_pPlayer->m_iDrivenEntIndex)
		{
			edict_t* pentDriven = INDEXENT(m_pPlayer->m_iDrivenEntIndex);
			if (UTIL_IsValidEntity(pentDriven))
				pSqueak->pev->owner = pentDriven;
		}

		// play hunt sound
		float flRndSound = RANDOM_FLOAT(0, 1);

		if (flRndSound <= 0.5)
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, 105);
		else
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, 105);

		m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= RC2PrefWord(RC2P_Snark_SnarkAmmoCost);

		m_fJustThrown = true;

		UTIL_SetMaxTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_PrimaryRefirePrimary).f);
		UTIL_SetMaxTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_PrimaryRefireSecondary).f);
		UTIL_SetMaxTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_PrimaryRefireTertiary).f);
		UTIL_SetMaxTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_PrimaryRefireQuaternary).f);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
		m_flNextWeaponSync = gpGlobals->time;
	}
}

// Secondary: toss a mini grunt the same way, upright (no pitch), at its own speed.
// orig 0xd38b0
void CSqueak::SecondaryAttack()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < RC2PrefWord(RC2P_Snark_GruntAmmoCost))
		return;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	TraceResult tr;
	Vector trace_origin;

	trace_origin = m_pPlayer->pev->origin;
	if ((m_pPlayer->pev->flags & FL_DUCKING) != 0)
	{
		trace_origin = trace_origin - (VEC_HULL_MIN - VEC_DUCK_HULL_MIN);
	}

	UTIL_TraceLine(trace_origin + gpGlobals->v_forward * 20, trace_origin + gpGlobals->v_forward * 64, dont_ignore_monsters, NULL, &tr);

	if (0 != g_iPlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usSnarkFire, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, 0, 0);

	if (tr.fAllSolid == 0 && tr.fStartSolid == 0 && tr.flFraction > 0.25)
	{
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

		Vector vecAngles(0, m_pPlayer->pev->v_angle.y, m_pPlayer->pev->v_angle.z);
		CBaseEntity* pGrunt = CBaseEntity::Create("mini_grunt", tr.vecEndPos, vecAngles, m_pPlayer->edict());
		pGrunt->pev->velocity = gpGlobals->v_forward * pGrunt->pev->speed;

		if (0 != m_pPlayer->m_iDrivenEntIndex)
		{
			edict_t* pentDriven = INDEXENT(m_pPlayer->m_iDrivenEntIndex);
			if (UTIL_IsValidEntity(pentDriven))
				pGrunt->pev->owner = pentDriven;
		}

		m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= RC2PrefWord(RC2P_Snark_GruntAmmoCost);

		m_fJustThrown = true;

		UTIL_SetMaxTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_SecondaryRefirePrimary).f);
		UTIL_SetMaxTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_SecondaryRefireSecondary).f);
		UTIL_SetMaxTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_SecondaryRefireTertiary).f);
		UTIL_SetMaxTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_SecondaryRefireQuaternary).f);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
	}
}

// Tertiary: trace 128 units from the gun and stick a snark cam on the surface hit, unless an
// HEV shield there soaks up CamDeployShieldDamage. A miss still costs half the refire times.
// orig 0xd3cb0
void CSqueak::TertiaryAttack()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < RC2PrefWord(RC2P_Snark_CamAmmoCost))
		return;
	if (m_iCamsDeployed > RC2PrefWord(RC2P_Snark_CamMaxDeployed))
		return;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = gpGlobals->v_forward;

	if (0 != g_iPlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usSnarkFire2, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, 0, 0);

	UTIL_SetMaxTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefirePrimary).f * 0.5);
	UTIL_SetMaxTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefireSecondary).f * 0.5);
	UTIL_SetMaxTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefireTertiary).f * 0.5);
	UTIL_SetMaxTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefireQuaternary).f * 0.5);

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + vecAiming * 128, dont_ignore_monsters, m_pPlayer->edict(), &tr);
	if (tr.flFraction >= 1.0)
		return;

	CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);

	if (RC2_ShieldFilterDamage(pHit, RC2Pref(RC2P_Snark_CamDeployShieldDamage).f, 1.0, tr.vecEndPos, gpGlobals->v_forward, NULL, NULL, 4) < RC2Pref(RC2P_Snark_CamDeployShieldDamage).f)
		return;
	if (!pHit)
		return;
	if ((pHit->pev->flags & FL_CONVEYOR) != 0) // VERIFY: the original tests bit 4 (FL_CONVEYOR) of the surface hit
		return;

	Vector vecAngles = UTIL_VecToAngles(tr.vecPlaneNormal);
	CBaseEntity::Create("snark_cam", tr.vecEndPos + tr.vecPlaneNormal * 6, vecAngles, m_pPlayer->edict());
	m_flNextWeaponSync = gpGlobals->time;

	if (RANDOM_FLOAT(0, 1) <= 0.5)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 0.6, ATTN_NORM, 0, 105);
	else
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 0.6, ATTN_NORM, 0, 105);

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= RC2PrefWord(RC2P_Snark_CamAmmoCost);
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		RetireWeapon();
		return;
	}

	m_fJustThrown = true;

	UTIL_SetMaxTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefirePrimary).f);
	UTIL_SetMaxTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefireSecondary).f);
	UTIL_SetMaxTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefireTertiary).f);
	UTIL_SetMaxTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_TertiaryRefireQuaternary).f);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
	m_flNextWeaponSync = gpGlobals->time;
}

// Quaternary: look through the first of this player's snark cams (a cam keeps its owner in
// pev->dmg_inflictor); with none deployed, only the refire times move.
// orig 0xd4170
void CSqueak::QuaternaryAttack()
{
	CBaseEntity* pCam = UTIL_FindEntityByClassname(NULL, "snark_cam");
	while (pCam)
	{
		if (pCam->pev->dmg_inflictor == m_pPlayer->edict())
		{
			SnarkCam_TakeView(pCam);
			return;
		}
		pCam = UTIL_FindEntityByClassname(pCam, "snark_cam");
	}

	UTIL_SetMaxTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_QuaternaryRefirePrimary).f);
	UTIL_SetMaxTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_QuaternaryRefireSecondary).f);
	UTIL_SetMaxTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_QuaternaryRefireTertiary).f);
	UTIL_SetMaxTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_QuaternaryRefireQuaternary).f);
}

// Each frame: count this player's live snarks and cams, and every half second send both to
// the client (gmsgWeaponSync: the weapon id, then the two counts).
// orig 0xd4370
void CSqueak::UpdateWeaponSync()
{
	m_iSnarksDeployed = 0;
	CBaseEntity* pEnt = UTIL_FindEntityByClassname(NULL, "monster_snark");
	while (pEnt)
	{
		CBaseEntity* pOwner = SqueakGrenade_Owner(pEnt); // its m_hOwner (orig +0x2dc)
		if (pOwner && pOwner->edict() == m_pPlayer->edict())
			m_iSnarksDeployed++;
		pEnt = UTIL_FindEntityByClassname(pEnt, "monster_snark");
	}

	m_iCamsDeployed = 0;
	pEnt = UTIL_FindEntityByClassname(NULL, "snark_cam");
	while (pEnt)
	{
		if (pEnt->pev->dmg_inflictor == m_pPlayer->edict())
			m_iCamsDeployed++;
		pEnt = UTIL_FindEntityByClassname(pEnt, "snark_cam");
	}

	if (m_flNextWeaponSync < gpGlobals->time)
	{
		m_flNextWeaponSync = gpGlobals->time + 0.5;
		MESSAGE_BEGIN(MSG_ONE, gmsgWeaponSync, NULL, m_pPlayer->edict());
		WRITE_BYTE(WEAPON_SNARK);
		WRITE_BYTE(m_iSnarksDeployed);
		WRITE_BYTE(m_iCamsDeployed);
		MESSAGE_END();
	}
}

// The client's side of the message above, compiled into the server too, where the readers
// return 0.
// orig 0xd44b0
void CSqueak::ReadWeaponSync(void* pbuf, int iSize)
{
	m_iSnarksDeployed = WeaponSync_ReadByte(&pbuf, &iSize);
	m_iCamsDeployed = WeaponSync_ReadByte(&pbuf, &iSize);
}
