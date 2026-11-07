/***
*
*	Rocket Crowbar 2: the crossbow's tether bolt (dlls/rc2/crossbowbolts.cpp), which the
*	crossbow (dlls/crossbow.cpp) ends itself through DieThink.
*
****/

#pragma once

#include "weapons.h"

//=========================================================
// CXBowTeather
//=========================================================
class CXBowTeather : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	bool GetPullOrg(CBaseEntity* pPulled, Vector& vecOrigin, float& flForce, float& flOther) override;
	void ReleasePull(CBaseEntity* pPulled) override;
	int GetRCCaps2(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 2; } // orig 0x285f0

	void EXPORT AttachTouch(CBaseEntity* pOther);
	void EXPORT PullingThink();
	void EXPORT DieThink();

	// none saved; names ours. pev->dmg_inflictor is the shooter, pev->owner what it's stuck in.
	int m_iShooterTether;	 // orig +0x2b8: the shooter's tether, -1 for none
	int m_iTargetTether;	 // orig +0x2bc: the struck player's tether back to the shooter, -1
	bool m_fStuckToWorld;	 // orig +0x2c0
	bool m_fInLiving;		 // orig +0x2c1: stuck in a living thing, not a brush
	bool m_fAttached;		 // orig +0x2c2
	Vector m_vecStuckAngles; // orig +0x2c4
	Vector m_vecStuckPos;	 // orig +0x2d0
	float m_flPullDist;		 // orig +0x2dc
	float m_flPullStrength;	 // orig +0x2e0
	float m_flBackPull;		 // orig +0x2e4: the pull on the struck one
	float m_flZOffset;		 // orig +0x2e8: VERIFY: nothing in rc2.dll sets it but Spawn (0)
};
