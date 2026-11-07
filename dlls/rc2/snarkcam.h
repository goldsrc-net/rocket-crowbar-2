/***
*
*	Rocket Crowbar 2: the snark cam (snark_cam), a camera a player sticks to a wall or to an
*	entity and looks through, and the homing projectile it fires (snark_project).
*	File name ours (rc2.dll keeps no file names).
*
****/

#pragma once

// snark_project: a guided bolt. It steers toward the view of its pev->enemy (the player who
// fired it) and blasts on touch or when its life runs out. Invisible on the server: the
// client draws it from the CLTE message its Spawn sends.
class CSnarkProjectile : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	int GetRCCaps2(int iAsker, CBaseEntity* pAsker) override { return 2; } // orig 0xca920

	void EXPORT HomeThink();
	void EXPORT ExplodeTouch(CBaseEntity* pOther);

	float m_flLastThink;	// orig +0x70, name ours: HomeThink's previous time, for the turn rate
	int m_iUnused74;		// orig +0x74, name ours. VERIFY: never read or written by p7's code
	unsigned short m_usHit; // orig +0x78, name ours: events/snarkprojecthit.sc
};

// snark_cam. pev->dmg_inflictor is the player who owns it, pev->enemy what it is stuck to.
class CSnarkCam : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	int BloodColor() override { return BLOOD_COLOR_YELLOW; } // orig 0xcc530
	int GetRCCaps2(int iAsker, CBaseEntity* pAsker) override { return 0x12; } // orig 0xcc520
	bool IsViewOfPlayer(CBaseEntity* pPlayer) override;

	void EXPORT ControlThink();

	// The snark weapon (CSqueak, 0xd4239) calls TakeControl on a cam it placed.
	void TakeControl();							 // name ours
	void ReleaseControl();						 // name ours: view back to the owner
	void DropControl();							 // name ours: stop controlling, view untouched
	void TakeOverControl();						 // name ours: become the controlled cam, view moves here
	CSnarkCam* FindNextCam(bool fBackward); // name ours: the owner's next cam by entity index

	bool m_fControlling;	 // orig +0x2b4, name ours: the owner is looking through this cam
	bool m_fFixed;			 // orig +0x2b5, name ours: stuck to the world, not to an entity
	bool m_fOnLiving;		 // orig +0x2b6, name ours: stuck to an entity alive and DAMAGE_AIM when placed
	Vector m_vecAngleOfs;	 // orig +0x2b8, name ours: angles, relative to the host's when stuck to one
	Vector m_vecOfs;		 // orig +0x2c4, name ours: origin; forward/right/up offset on the host when stuck to one
	float m_flNextInput;	 // orig +0x2d0, name ours
	float m_flNextFire;		 // orig +0x2d4, name ours
	bool m_fOwnerAway;		 // orig +0x2d8, name ours: the owner has been more than 72 units away
};
