/***
*
*	Rocket Crowbar 2: balllightning.h (file name ours). The python's ball lightning
*	(ball_lightning) and the chain lightning hurt it hands out (chain_hurter); the python's
*	code (dlls/python.cpp and the run after it) creates both.
*
****/

#pragma once

// The python's secondary shot: a glowing ball that flies until it hits something, then blasts
// everything in [Python] BallBlastRadius it can see. In water it discharges instead.
class CBallLighting : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	int GetRCCaps2(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 2; } // orig 0xaad80

	void EXPORT ImpactTouch(CBaseEntity* pOther);
	void EXPORT BallThink();

	void BlastDamage(Vector vecOrigin); // ours

	unsigned short m_usBallImpact; // orig +0x70, name ours: events/pythballimpact.sc
};

// One link of the chain lightning: on its first think it hurts m_hTarget (when it takes aimed
// damage) with pev->dmg along m_vecDir, then removes itself.
class CChainHurter : public CBaseEntity
{
public:
	void Spawn() override;

	void EXPORT HurtEnt();

	EHANDLE m_hTarget;	// orig +0x70, name ours
	TraceResult m_tr;	// orig +0x78, name ours: passed to TraceAttack
	Vector m_vecDir;	// orig +0xb0, name ours
};
