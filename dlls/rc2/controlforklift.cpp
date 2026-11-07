/***
*
*	Rocket Crowbar 2: the remote-control forklift (the satchel's forklift). The player who set it
*	off watches through it and drives it: it sticks to and climbs whatever world surface it
*	reaches, steers by the player's view yaw, speeds up or slows down on the player's buttons,
*	jumps off the surface on the secondary attack and blows up on the primary, when it hits
*	something that can be hurt, or when its driver dies. Every number is rc2.ini [Satchel].
*	Reconstructed from rc2.dll, the run between defaultai.cpp and doors.cpp (0x328e0-0x341e0);
*	reference/dossiers/p2.md. The file name is ours.
*
*	OVERLOADS SOME ENTVARS:
*	  dmg_inflictor  the driver (owner again at the explosion)
*	  speed          the driving speed
*	  impulse        a stuck-check state that is reset every think (see ControlThink)
*	  dmgtime        next AI danger sound
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "client.h"
#include "soundent.h"
#include "rc2prefs.h"
#include "p2shared.h"

extern int gmsgViewMode; // "ViewMode": 2 the player watches through an entity, 0 back to his own view
// orig 0xa0260 (pm_math): angles from a forward, right and up vector
extern void VectorsToAngles(const Vector& vecForward, const Vector& vecRight, const Vector& vecUp, Vector& vecAngles);

#define RC2_CLTE_FORKLIFT_VIEW 0x3f	 // short entity, byte the driver's index (name ours)
#define RC2_CLTE_FORKLIFT_BOOM 0xfe	 // short entity, byte 0x3f (name ours)

// RC2's player byte at +0x66: while set, the player can't act (ItemPostFrame clears his buttons
// and the forklift blows up). It is the third byte of the RC2 CBaseEntity int at +0x64 (name ours).
static bool IsPlayerHeld(CBaseEntity* pPlayer) // name ours
{
	return pPlayer->m_bFrozen;
}

class CControlForklift : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	int GetRCCaps(CBaseEntity* pOther, CBaseEntity* pAsker) override { return 0x480; } // orig 0x32960

	void EXPORT ControlThink();
	void EXPORT DoTouch(CBaseEntity* pOther);
	void EXPORT GoBoom();

	void Stick(TraceResult* ptr); // name ours: drive on the surface ptr hit
	void Unstick();				  // name ours

	Vector m_vecSurfaceNormal; // orig +0x2b8, name ours: the surface it drives on
	bool m_bOnSurface;		   // orig +0x2c4, name ours
	float m_flArmTime;		   // orig +0x2c8, name ours: the driver's buttons count after this
	float m_flUnused2cc;	   // orig +0x2cc, name ours: -1 at spawn, never read here
};

LINK_ENTITY_TO_CLASS(control_forklift, CControlForklift); // orig 0x328e0

// orig 0x33f70
static float ForkliftDot(const Vector& a, const Vector& b) // name ours
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

// The surface's driving frame (inlined twice in ControlThink and Stick): vecSide is
// (0, n.z, -n.y), unnormalized, and vecAhead = vecSide x n; for n = (+-1, 0, 0), where that
// vanishes, (0, 1, 0) and (0, 0, -+1).
static void SurfaceFrame(const Vector& n, Vector& vecSide, Vector& vecAhead) // name ours
{
	if (n.x == 1.0 && n.y == 0 && n.z == 0)
	{
		vecSide = Vector(0, 1, 0);
		vecAhead = Vector(0, 0, -1);
	}
	else if (n.x == -1.0 && n.y == 0 && n.z == 0)
	{
		vecSide = Vector(0, 1, 0);
		vecAhead = Vector(0, 0, 1);
	}
	else
	{
		vecSide = Vector(0, n.z - 0, 0 - n.y);
		vecAhead = Vector(vecSide.y * n.z - vecSide.z * n.y, vecSide.z * n.x - vecSide.x * n.z, vecSide.x * n.y - vecSide.y * n.x);
	}
}

// orig 0x32970
void CControlForklift::Precache()
{
	PRECACHE_MODEL("models/miniforklift.mdl");
	PRECACHE_SOUND("ambience/industrial4.wav");
}

// orig 0x32990
void CControlForklift::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->friction = 0.6;

	SET_MODEL(ENT(pev), "models/miniforklift.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));
	UTIL_SetOrigin(pev, pev->origin);

	pev->classname = MAKE_STRING("control_forklift");

	SetThink(&CControlForklift::ControlThink);
	SetTouch(&CControlForklift::DoTouch);
	pev->nextthink = gpGlobals->time + 0.01;

	pev->dmg = RC2Pref(RC2P_Satchel_ForkliftDamage).f;
	pev->sequence = 0;
	ResetSequenceInfo();
	m_fSequenceLoops = true;

	pev->health = 1.0;
	pev->takedamage = DAMAGE_YES;
	pev->dmg_inflictor = pev->owner;
	pev->dmgtime = gpGlobals->time;

	m_vecSurfaceNormal = Vector(0, 0, 1);
	m_flArmTime = gpGlobals->time + 0.2;
	m_flUnused2cc = -1.0;
	m_bOnSurface = false;
	pev->impulse = 0;
	pev->speed = RC2Pref(RC2P_Satchel_ForkliftInitialSpeed).f;

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/industrial4.wav", 0.6, 1.1, 0, 130);

	m_iRC2Flags = 0x10; // VERIFY: the same bit the snark cam sets: "something views through this"

	// the driver watches through it
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		unsigned char* pvs;
		SetupVisibility(edict(), pev->dmg_inflictor, &pvs, &pvs);
		SET_VIEW(pev->dmg_inflictor, edict());

		MESSAGE_BEGIN(MSG_ONE, gmsgViewMode, g_vecZero, pev->dmg_inflictor);
		WRITE_BYTE(2);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
		WRITE_BYTE(RC2_CLTE_FORKLIFT_VIEW);
		WRITE_SHORT(ENTINDEX(edict()));
		WRITE_BYTE(ENTINDEX(pev->dmg_inflictor));
		MESSAGE_END();
	}
}

// orig 0x32c40
void CControlForklift::ControlThink()
{
	CBaseEntity* pDriver = NULL;
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
		pDriver = CBaseEntity::Instance(pev->dmg_inflictor);

	if (!pDriver || pDriver->pev->deadflag != DEAD_NO || !pDriver->IsAlive() || IsPlayerHeld(pDriver) || (m_flArmTime < gpGlobals->time && (pDriver->pev->button & IN_ATTACK) != 0))
	{
		GoBoom();
		return;
	}

	// stopped dead while it should drive
	if (pev->velocity.Length() < 0.1 && pev->speed > 5.0)
	{
		if (pev->impulse == 1)
		{
			float x = RANDOM_FLOAT(-1, 1);
			float y = RANDOM_FLOAT(-1, 1);
			Vector vecDir = Vector(x, y, 0).Normalize();
			pev->velocity = vecDir * pev->speed;
			pev->impulse = 2;
		}
		else if (pev->impulse == 2)
		{
			GoBoom();
			return;
		}
		else
			pev->impulse = 1;
	}

	// Original oddity, kept: cleared every think, so the stuck handling above only ever goes 0 -> 1
	pev->impulse = 0;
	pev->nextthink = gpGlobals->time + 0.05;

	float flInterval = StudioFrameAdvance(0.0); // VERIFY: 0x9380 (RC2's, in the run before animating.cpp) read as StudioFrameAdvance
	if (ApplyExternalForce(flInterval))
		Unstick();

	if (m_flArmTime < gpGlobals->time)
	{
		int iButtons = pDriver->pev->button;
		if ((iButtons & IN_ATTACK2) != 0)
		{
			// jump off the surface
			if (m_bOnSurface)
			{
				pev->velocity = m_vecSurfaceNormal * RC2Pref(RC2P_Satchel_ForkliftJumpVelocity).f + pev->velocity;
				Unstick();
			}
		}
		else if ((iButtons & IN_CANCEL) != 0) // VERIFY: bit 0x40; RC2's client may send it for its own key
			pev->speed = RC2Pref(RC2P_Satchel_ForkliftAccelRate).f * flInterval + pev->speed;
		else if ((iButtons & IN_ALT1) != 0)
			pev->speed = pev->speed - RC2Pref(RC2P_Satchel_ForkliftDecelRate).f * flInterval;
	}

	if (pev->speed < RC2Pref(RC2P_Satchel_ForkliftMinSpeed).f)
		pev->speed = RC2Pref(RC2P_Satchel_ForkliftMinSpeed).f;
	else if (pev->speed > RC2Pref(RC2P_Satchel_ForkliftMaxSpeed).f)
		pev->speed = RC2Pref(RC2P_Satchel_ForkliftMaxSpeed).f;

	if (m_bOnSurface)
	{
		// a world surface just behind it, or under it, keeps it on; nothing under it, off
		Vector vecDir = pev->velocity.Normalize();
		TraceResult tr;
		UTIL_TraceLine(pev->origin, pev->origin - vecDir * 8, dont_ignore_monsters, edict(), &tr);

		bool bDone = false;
		if (tr.flFraction < 1.0)
		{
			if (UTIL_IsValidEntity(tr.pHit))
			{
				CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
				if (pHit && pHit->IsBSPModel())
				{
					Stick(&tr);
					bDone = true;
				}
			}
			else if (tr.pHit && ENTINDEX(tr.pHit) == 0)
			{
				Stick(&tr);
				bDone = true;
			}
		}

		if (!bDone)
		{
			UTIL_TraceLine(pev->origin, pev->origin - m_vecSurfaceNormal * 12, dont_ignore_monsters, edict(), &tr);
			if (tr.flFraction >= 1.0)
				Unstick();
			else if (UTIL_IsValidEntity(tr.pHit))
			{
				CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
				if (pHit)
				{
					if (pHit->IsBSPModel())
						Stick(&tr);
					else
						Unstick();
				}
			}
			else if (tr.pHit && ENTINDEX(tr.pHit) == 0)
				Stick(&tr);
		}
	}

	if (m_bOnSurface)
	{
		// drive along the surface where the driver looks
		Vector vecSide, vecAhead;
		SurfaceFrame(m_vecSurfaceNormal, vecSide, vecAhead);

		float flYaw = pDriver->pev->v_angle.y * (M_PI / 180.0);
		float c = cos(flYaw);
		float s = sin(flYaw);

		Vector vecForward = vecAhead * c + vecSide * s;
		Vector vecRight = vecAhead * s - vecSide * c;
		VectorsToAngles(vecForward, vecRight, m_vecSurfaceNormal, pev->angles);
		pev->velocity = vecForward * pev->speed;
	}
	else
	{
		pev->angles = UTIL_VecToAngles(pev->velocity);
		if (pev->velocity.Length() < 10)
			pev->angles.x = 0;
	}

	// the driver can't attack while he drives
	UTIL_ExtendTime(&((CBaseMonster*)pDriver)->m_flNextAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Satchel_ForkliftNextAttackTime).f);

	if (pev->dmgtime < gpGlobals->time)
	{
		// Original oddity, kept: bits_SOUND_CARCASS (0x20), not bits_SOUND_DANGER
		CSoundEnt::InsertSound(bits_SOUND_CARCASS, pev->origin + pev->velocity * 0.5, (int)pev->velocity.Length(), 0.2);
		pev->dmgtime = gpGlobals->time + 0.2;
	}
}

// orig 0x33830
void CControlForklift::Stick(TraceResult* ptr)
{
	CBaseEntity* pDriver = NULL;
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
		pDriver = CBaseEntity::Instance(pev->dmg_inflictor);

	if (!pDriver || pDriver->pev->deadflag != DEAD_NO || !pDriver->IsAlive() || IsPlayerHeld(pDriver))
	{
		GoBoom();
		return;
	}

	if (m_bOnSurface)
	{
		// onto a new surface: when its right flips over, so does the driver's view, so his
		// steering keeps its sense
		float flYaw = pDriver->pev->v_angle.y * (M_PI / 180.0);
		float c = cos(flYaw);
		float s = sin(flYaw);

		Vector vecSide, vecAhead;
		SurfaceFrame(m_vecSurfaceNormal, vecSide, vecAhead);
		Vector vecOldRight = vecAhead * s - vecSide * c;

		m_vecSurfaceNormal = ptr->vecPlaneNormal;
		SurfaceFrame(m_vecSurfaceNormal, vecSide, vecAhead);
		Vector vecNewRight = s * vecAhead - vecSide * c; // VERIFY: the scalar*vector operands, from the disassembly's stack offsets

		if (ForkliftDot(vecOldRight, vecNewRight) < 0)
		{
			pDriver->pev->angles = Vector(pDriver->pev->v_angle.x, pDriver->pev->v_angle.y - 180, pDriver->pev->v_angle.z);
			pDriver->pev->v_angle = pDriver->pev->angles;
			pDriver->pev->fixangle = 1;
		}
	}

	m_bOnSurface = true;
	m_vecSurfaceNormal = ptr->vecPlaneNormal;
	UTIL_SetOrigin(pev, ptr->vecEndPos + m_vecSurfaceNormal * 4.5);
	pev->movetype = MOVETYPE_FLY;
}

// orig 0x33f90
void CControlForklift::Unstick()
{
	if (m_bOnSurface)
		pev->velocity = pev->velocity - m_vecSurfaceNormal * 20;
	m_bOnSurface = false;
	pev->movetype = MOVETYPE_BOUNCE;
}

// orig 0x34030
void CControlForklift::DoTouch(CBaseEntity* pOther)
{
	TraceResult tr = UTIL_GetGlobalTrace();

	if (!UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		GoBoom();
		return;
	}

	if (pev->owner && pOther->edict() != pev->owner)
		pev->owner = NULL;

	if (tr.flFraction >= 1.0)
		return;

	if (UTIL_IsValidEntity(tr.pHit))
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		if (pHit)
		{
			// what can be hurt sets it off
			if (pHit->pev->takedamage == DAMAGE_AIM || !pHit->IsBSPModel())
			{
				SetThink(&CControlForklift::GoBoom);
				SetTouch(NULL);
				pev->nextthink = gpGlobals->time + 0.01;
				return;
			}
			if (pHit->IsBSPModel() && !m_bOnSurface)
				Stick(&tr);
		}
	}

	if (tr.pHit && ENTINDEX(tr.pHit) != 0)
		return;
	Stick(&tr);
}

// orig 0x341d0
void CControlForklift::Killed(entvars_t* pevAttacker, int iGib)
{
	GoBoom();
}

// orig 0x341e0
void CControlForklift::GoBoom()
{
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->owner = pev->dmg_inflictor;
	STOP_SOUND(ENT(pev), CHAN_STATIC, "ambience/industrial4.wav");
	SetTouch(NULL);
	SetThink(NULL);

	// find a surface to blow against
	TraceResult tr;
	bool bFound = false;
	if (!m_bOnSurface)
	{
		Vector vecDir = pev->velocity.Normalize();
		UTIL_TraceLine(pev->origin - vecDir * 32, pev->origin + vecDir * 128, dont_ignore_monsters, edict(), &tr);
		if (tr.flFraction >= 1.0)
			UTIL_TraceLine(pev->origin + Vector(0, 0, 32), Vector(pev->origin.x, pev->origin.y, pev->origin.z - 128), dont_ignore_monsters, edict(), &tr); // VERIFY: the start's offset vector
		bFound = tr.flFraction < 1.0;
	}
	else
	{
		UTIL_TraceLine(pev->origin + m_vecSurfaceNormal * 32, pev->origin - m_vecSurfaceNormal * 128, dont_ignore_monsters, edict(), &tr);
		if (tr.flFraction >= 1.0)
		{
			Vector vecDir = pev->velocity.Normalize();
			UTIL_TraceLine(pev->origin - vecDir * 32, pev->origin + vecDir * 128, dont_ignore_monsters, edict(), &tr);
		}
		bFound = tr.flFraction < 1.0;
	}

	if (!bFound)
	{
		tr.vecEndPos = pev->origin;
		tr.vecPlaneNormal = Vector(0, 0, 1);
		tr.flFraction = 1.0;
	}

	// back to the driver's own view
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		unsigned char* pvs;
		SetupVisibility(pev->dmg_inflictor, pev->dmg_inflictor, &pvs, &pvs);
		SET_VIEW(pev->dmg_inflictor, pev->dmg_inflictor);

		MESSAGE_BEGIN(MSG_ONE, gmsgViewMode, g_vecZero, pev->dmg_inflictor);
		WRITE_BYTE(0);
		MESSAGE_END();
	}

	// SDK 2.2's spectator director message (svc 50, event 3), priority 3
	MESSAGE_BEGIN(MSG_SPEC, 50);
	WRITE_BYTE(3);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_SHORT(0);
	WRITE_LONG(3);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(RC2_CLTE_FORKLIFT_BOOM);
	WRITE_SHORT(ENTINDEX(edict()));
	WRITE_BYTE(0x3f);
	MESSAGE_END();

	m_iRC2Flags = 0;
	Explode(&tr, DMG_BLAST);
}
