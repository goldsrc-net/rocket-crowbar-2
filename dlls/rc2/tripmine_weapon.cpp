/***
*
*	Rocket Crowbar 2: the tripmine weapon's own code, the run of RC2's functions in rc2.dll right
*	after SDK 2.3's tripmine.cpp (orig 0xece60-0xee0a0). The rest of CTripmine, and the mines
*	themselves, are in dlls/tripmine.cpp.
*
*	Two of the original's functions here are CTripmine virtuals halflife-updated's tripmine.cpp
*	already defines, so they can't be defined here as well (reference/dossiers/p9.md, "Merge"):
*	- CTripmine::PrimaryAttack (orig 0xecee0) is MineAttack(0); tripmine.cpp still has SDK 2.3's.
*	- CTripmine::WeaponIdle (orig 0xedee0) is SDK 2.3's, as tripmine.cpp has it.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "rc2prefs.h"
#include "rc2/p9_needs.h"

// what tells the four mines apart: everything else in the original's four copies is the same
struct tripmineattack_t
{
	const char* pszClassname;
	rc2pref_e iAmmoCost;	   // short: ammo used per mine
	rc2pref_e iMaxOut;		   // char: how many may be out at once (one more is let through)
	rc2pref_e iDeployDistance; // float: how far ahead the mine can be stuck on
	rc2pref_e iRefire[4];	   // float: delays this attack puts on the primary..quaternary attacks
};

static const tripmineattack_t kMineAttacks[4] = {
	{"monster_tripmine", RC2P_Tripmine_PrimaryAmmoCost, RC2P_Tripmine_PrimaryMaxOut, RC2P_Tripmine_PrimaryDeployDistance,
		{RC2P_Tripmine_PrimaryRefirePrimary, RC2P_Tripmine_PrimaryRefireSecondary, RC2P_Tripmine_PrimaryRefireTertiary, RC2P_Tripmine_PrimaryRefireQuaternary}},
	{"bouncy_tripmine", RC2P_Tripmine_BouncyAmmoCost, RC2P_Tripmine_BouncyMaxOut, RC2P_Tripmine_BouncyDeployDistance,
		{RC2P_Tripmine_SecondaryRefirePrimary, RC2P_Tripmine_SecondaryRefireSecondary, RC2P_Tripmine_SecondaryRefireTertiary, RC2P_Tripmine_SecondaryRefireQuaternary}},
	{"punch_tripmine", RC2P_Tripmine_PunchAmmoCost, RC2P_Tripmine_PunchMaxOut, RC2P_Tripmine_PunchDeployDistance,
		{RC2P_Tripmine_TertiaryRefirePrimary, RC2P_Tripmine_TertiaryRefireSecondary, RC2P_Tripmine_TertiaryRefireTertiary, RC2P_Tripmine_TertiaryRefireQuaternary}},
	{"target_tripmine", RC2P_Tripmine_TargetAmmoCost, RC2P_Tripmine_TargetMaxOut, RC2P_Tripmine_TargetDeployDistance,
		{RC2P_Tripmine_QuaternaryRefirePrimary, RC2P_Tripmine_QuaternaryRefireSecondary, RC2P_Tripmine_QuaternaryRefireTertiary, RC2P_Tripmine_QuaternaryRefireQuaternary}},
};

// how many mines of this kind the player who has this weapon has out
int CTripmine::CountMines(const char* szClassname) // orig 0xece60
{
	int iCount = 0;
	CBaseEntity* pMine = NULL;

	while ((pMine = UTIL_FindEntityByClassname(pMine, szClassname)) != NULL)
	{
		CBaseEntity* pPlacer = TripmineGrenade_GetPlacer(pMine);
		if (pPlacer && pPlacer->edict() == m_pPlayer->edict())
			iCount++;
	}

	return iCount;
}

// CTripmine::PrimaryAttack (orig 0xecee0) is MineAttack(0); see the top of this file.
void CTripmine::MineAttack(int iMine)
{
	const tripmineattack_t& attack = kMineAttacks[iMine];
	const int iAmmoCost = (unsigned short)RC2Pref(attack.iAmmoCost).s;

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < iAmmoCost)
		return;

	// Original bug kept: "greater than", so one mine more than the limit goes out
	if (m_iMinesOut[iMine] > (unsigned char)RC2Pref(attack.iMaxOut).c)
		return;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = gpGlobals->v_forward;

	TraceResult tr;

	UTIL_TraceLine(vecSrc, vecSrc + vecAiming * RC2Pref(attack.iDeployDistance).f, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr);

	if (g_fRC2PlayWeaponEvents)
		PLAYBACK_EVENT_FULL(FEV_NOTHOST, m_pPlayer->edict(), m_usTripFire, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, 0, 0);

	if (tr.flFraction < 1.0)
	{
		CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
		// Rocket Crowbar 2: a shielded player's shield can stop the mine going on
		if (pEntity && (pEntity->pev->flags & FL_CONVEYOR) == 0 &&
			ShieldDeflect(pEntity, RC2Pref(RC2P_Tripmine_GlobalDeployShieldDamage).f, 1.0f, tr.vecEndPos, gpGlobals->v_forward, NULL, NULL, 4) >= RC2Pref(RC2P_Tripmine_GlobalDeployShieldDamage).f)
		{
			Vector angles = UTIL_VecToAngles(tr.vecPlaneNormal);

			CBaseEntity::Create(attack.pszClassname, tr.vecEndPos + tr.vecPlaneNormal * 8, angles, m_pPlayer->edict());

			m_flNextMinesOutSend = gpGlobals->time; // send the new count next frame

			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= iAmmoCost;

			// player "shoot" animation
			m_pPlayer->SetAnimation(PLAYER_ATTACK1);

			if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
			{
				// no more mines!
				RetireWeapon();
				return;
			}
		}
	}

	UTIL_ExtendTime(m_flNextPrimaryAttack, UTIL_WeaponTimeBase(), RC2Pref(attack.iRefire[0]).f);
	UTIL_ExtendTime(m_flNextSecondaryAttack, UTIL_WeaponTimeBase(), RC2Pref(attack.iRefire[1]).f);
	UTIL_ExtendTime(m_flNextTertiaryAttack, UTIL_WeaponTimeBase(), RC2Pref(attack.iRefire[2]).f);
	UTIL_ExtendTime(m_flNextQuaternaryAttack, UTIL_WeaponTimeBase(), RC2Pref(attack.iRefire[3]).f);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
}

void CTripmine::SecondaryAttack() // orig 0xed2e0
{
	MineAttack(1);
}

void CTripmine::TertiaryAttack() // orig 0xed6e0
{
	MineAttack(2);
}

void CTripmine::QuaternaryAttack() // orig 0xedae0
{
	MineAttack(3);
}

// CTripmine::WeaponIdle (orig 0xedee0) is SDK 2.3's; see the top of this file.

// counts the mines out and, twice a second, tells the client
void CTripmine::WeaponFrame() // orig 0xedfc0
{
	for (int i = 0; i < 4; i++)
		m_iMinesOut[i] = CountMines(kMineAttacks[i].pszClassname);

	if (m_flNextMinesOutSend < gpGlobals->time)
	{
		m_flNextMinesOutSend = gpGlobals->time + 0.5;

		MESSAGE_BEGIN(MSG_ONE, gmsgWeaponSync, NULL, m_pPlayer->edict());
		WRITE_BYTE(WEAPON_TRIPMINE);
		WRITE_BYTE(m_iMinesOut[0]);
		WRITE_BYTE(m_iMinesOut[1]);
		WRITE_BYTE(m_iMinesOut[2]);
		WRITE_BYTE(m_iMinesOut[3]);
		MESSAGE_END();
	}
}

// reads the counts back (only the client's readers read anything)
void CTripmine::ReadWeaponState(void* pData, int iSize) // orig 0xee0a0
{
	m_iMinesOut[0] = RC2ReadStateByte(&pData, &iSize);
	m_iMinesOut[1] = RC2ReadStateByte(&pData, &iSize);
	m_iMinesOut[2] = RC2ReadStateByte(&pData, &iSize);
	m_iMinesOut[3] = RC2ReadStateByte(&pData, &iSize);
}
