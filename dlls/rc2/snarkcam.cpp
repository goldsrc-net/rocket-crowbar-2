/***
*
*	Rocket Crowbar 2: the snark cam and its projectile (snarkcam.h). File name ours.
*	Reconstructed from rc2.dll 0xca8b0-0xcc530 (files-rc2.tsv "new: skill .. sound").
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "client.h"
#include "soundent.h"
#include "decals.h"
#include "rc2prefs.h"
#include "p7_extern.h"
#include "snarkcam.h"

// The byte at +0x66 of RC2's CBaseEntity block, which the original reads on players as "can't
// act" (also in CBasePlayerWeapon::ItemPostFrame). Inside the int cbase.h calls m_iRC2Base64.
// VERIFY: what sets it (p1/p5), and whether that member is really bytes.
static bool RC2EntityByte66(CBaseEntity* pEntity) // name ours
{
	return reinterpret_cast<const unsigned char*>(&pEntity->m_iRC2Base64)[2] != 0;
}

//=========================================================
// snark_project
//=========================================================

LINK_ENTITY_TO_CLASS(snark_project, CSnarkProjectile); // orig 0xca8b0

// orig 0xca930
void CSnarkProjectile::Precache()
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	PRECACHE_MODEL("sprites/glow04.spr");
	PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_SOUND("debris/beamstart11.wav");
	m_usHit = PRECACHE_EVENT(1, "events/snarkprojecthit.sc");
}

// orig 0xca980
void CSnarkProjectile::Spawn()
{
	Precache();
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_FLY;
	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	UTIL_SetOrigin(pev, pev->origin);
	pev->effects |= EF_NODRAW; // the client draws it (CLTE 0x3e below)
	m_iRC2Flags = 2;		   // VERIFY: meaning of RC2's +0x68 value 2 (cbase.h)
	pev->dmg = RC2Pref(RC2P_Snark_ProjDamage).f;
	SetThink(&CSnarkProjectile::HomeThink);
	SetTouch(&CSnarkProjectile::ExplodeTouch);
	pev->nextthink = gpGlobals->time + 0.1;
	m_flLastThink = gpGlobals->time;
	pev->speed = RANDOM_FLOAT(RC2Pref(RC2P_Snark_ProjMinSpeed).f, RC2Pref(RC2P_Snark_ProjMaxSpeed).f);
	pev->dmgtime = gpGlobals->time + RC2Pref(RC2P_Snark_ProjMaxLife).f;
	pev->dmg_inflictor = pev->owner;
	pev->enemy = pev->dmg_inflictor;

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(0x3e); // VERIFY: CLTE type 62, the projectile's trail (client.dll)
	WRITE_SHORT(entindex());
	MESSAGE_END();
}

// orig 0xcaac0. Turns toward where pev->enemy (whoever fired it) is looking, at most
// [Snark] ProjTurnAvel degrees a second, and flies at pev->speed.
void CSnarkProjectile::HomeThink()
{
	const float flInterval = gpGlobals->time - m_flLastThink;
	m_flLastThink = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.05;

	if (pev->dmgtime < gpGlobals->time)
	{
		ExplodeTouch(this);
		return;
	}

	if (UTIL_IsValidEntity(pev->enemy))
	{
		CBaseEntity* pGuide = CBaseEntity::Instance(pev->enemy);
		if (pGuide == NULL)
		{
			// the original returns here, so the velocity isn't refreshed this frame
			pev->enemy = NULL;
			return;
		}

		float flPitch = pGuide->pev->angles.x;
		float flYaw = pGuide->pev->angles.y;
		if (pGuide->IsPlayer())
		{
			flYaw = pGuide->pev->v_angle.y;
			flPitch = -pGuide->pev->v_angle.x;
		}

		const float flTurn = RC2Pref(RC2P_Snark_ProjTurnAvel).f * flInterval;

		while (flYaw > 180)
			flYaw -= 360;
		while (flYaw < -180)
			flYaw += 360;
		while (pev->angles.y > 180)
			pev->angles.y -= 360;
		while (pev->angles.y < -180)
			pev->angles.y += 360;

		const float flDPitch = flPitch - pev->angles.x;
		const float flDYaw = UTIL_AngleDiff(flYaw, pev->angles.y);
		const float flDist = sqrt(flDPitch * flDPitch + flDYaw * flDYaw);

		if (flTurn <= flDist)
		{
			float flStepPitch = 0, flStepYaw = 0;
			if (flDist != 0)
			{
				flStepPitch = (1.0f / flDist) * flDPitch;
				flStepYaw = flDYaw * (1.0f / flDist);
			}
			pev->angles.x = flStepPitch * flTurn + pev->angles.x;
			pev->angles.y = flStepYaw * flTurn + pev->angles.y;
		}
		else
		{
			pev->angles.x = flPitch;
			pev->angles.y = flYaw;
		}
	}

	UTIL_MakeAimVectors(pev->angles);
	pev->velocity = gpGlobals->v_forward * pev->speed;
}

// orig 0xcadc0. Also called by HomeThink with this as pOther when the life runs out.
void CSnarkProjectile::ExplodeTouch(CBaseEntity* pOther)
{
	entvars_t* pevAttacker = pev;
	if (UTIL_IsValidEntity(pev->dmg_inflictor))
		pevAttacker = pev->dmg_inflictor != NULL ? VARS(pev->dmg_inflictor) : NULL;

	TraceResult tr = UTIL_GetGlobalTrace();
	Vector vecOrigin = pev->origin;

	if (pOther->IsBSPModel())
	{
		vecOrigin = tr.vecEndPos + tr.vecPlaneNormal * 4;
		PLAYBACK_EVENT_FULL(FEV_GLOBAL, edict(), m_usHit, 0.0, vecOrigin, g_vecZero, 0.0, 0.0,
			UTIL_PackDirection(-tr.vecPlaneNormal), 0, 1, 0);
	}
	else
	{
		PLAYBACK_EVENT_FULL(FEV_GLOBAL, edict(), m_usHit, 0.0, vecOrigin, g_vecZero, 0.0, 0.0,
			UTIL_PackDirection(pev->velocity.Normalize()), 0, 0, 0);

		if (pOther->IsAlive())
		{
			// a player whose shield is up doesn't bleed; neither do other snarks
			if (!pOther->IsActivePlayer() || !static_cast<CBasePlayer*>(pOther)->m_fShieldOn)
			{
				if (0 != strcmp(STRING(pOther->pev->classname), "monster_snark"))
					UTIL_BloodDrips(tr.vecEndPos, tr.vecPlaneNormal, pOther->MyMonsterPointer()->BloodColor(), 60);
			}
		}
	}

	RC2RadiusDamage(pevAttacker, pev, edict(), pev->origin, RC2Pref(RC2P_Snark_ProjBlastRadius).f, pev->dmg,
		RC2Pref(RC2P_Snark_ProjBlastFalloffExp).f, DMG_BLAST, 0);

	SetThink(NULL);
	SetTouch(NULL);
	pev->effects |= EF_NODRAW;
	UTIL_Remove(this);
}

//=========================================================
// snark_cam
//=========================================================

LINK_ENTITY_TO_CLASS(snark_cam, CSnarkCam); // orig 0xcb0f0

// orig 0xcb160
bool CSnarkCam::IsViewOfPlayer(CBaseEntity* pPlayer)
{
	if (pPlayer->edict() == pev->enemy)
		return true;
	if (pPlayer->edict() == pev->dmg_inflictor && !m_fOwnerAway)
		return true;
	return CBaseEntity::IsViewOfPlayer(pPlayer);
}

// orig 0xcb1b0
void CSnarkCam::Precache()
{
	PRECACHE_MODEL("models/w_squeak.mdl");
	PRECACHE_SOUND("squeek/sqk_blast1.wav");
	PRECACHE_SOUND("debris/beamstart10.wav");
	UTIL_PrecacheOther("snark_project");
}

// orig 0xcb1e0. Sticks to what is just ahead along pev->angles (or removes itself when nothing
// is within 8 units ahead / 16 behind).
void CSnarkCam::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "models/w_squeak.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));
	UTIL_SetOrigin(pev, pev->origin);
	pev->classname = MAKE_STRING("snark_cam");
	SetThink(&CSnarkCam::ControlThink);
	pev->nextthink = gpGlobals->time + 0.01;
	pev->health = RC2Pref(RC2P_Snark_CamHealth).f;
	pev->takedamage = DAMAGE_YES;
	pev->dmg_inflictor = pev->owner;
	m_fOwnerAway = false;
	m_flNextInput = gpGlobals->time + RC2Pref(RC2P_Snark_CamSpawnInputDelay).f;
	m_flNextFire = gpGlobals->time;

	UTIL_MakeAimVectors(pev->angles);
	const Vector vecBehind = pev->origin - gpGlobals->v_forward * 16;
	const Vector vecAhead = gpGlobals->v_forward * 8 + pev->origin;
	TraceResult tr;
	UTIL_TraceLine(vecAhead, vecBehind, dont_ignore_monsters, edict(), &tr);
	if (tr.flFraction >= 1.0)
	{
		SetThink(&CBaseEntity::SUB_Remove);
		return;
	}

	pev->angles = UTIL_VecToAngles(tr.vecPlaneNormal);

	int iHost = 0;
	if (tr.pHit != NULL && UTIL_IsValidEntity(tr.pHit) && ENTINDEX(tr.pHit) != 0)
	{
		entvars_t* pevHost = VARS(tr.pHit);
		m_vecAngleOfs = pev->angles - pevHost->angles;
		UTIL_MakeVectors(pevHost->angles);
		UTIL_SetOrigin(pev, tr.vecEndPos + tr.vecPlaneNormal);

		const Vector vecDelta = pev->origin - pevHost->origin;
		m_vecOfs.x = DotProduct(vecDelta, gpGlobals->v_forward);
		m_vecOfs.y = DotProduct(vecDelta, gpGlobals->v_right);
		m_vecOfs.z = DotProduct(vecDelta, gpGlobals->v_up);
		m_fFixed = false;
		pev->enemy = tr.pHit;
		iHost = ENTINDEX(tr.pHit);

		CBaseEntity* pHost = CBaseEntity::Instance(tr.pHit);
		if (pHost != NULL && pHost->IsAlive() && pHost->pev->takedamage == DAMAGE_AIM && pHost->pev->deadflag == DEAD_NO)
			m_fOnLiving = true;
		else
			m_fOnLiving = false;
	}
	else
	{
		m_vecAngleOfs = pev->angles;
		m_vecOfs = tr.vecEndPos + tr.vecPlaneNormal;
		UTIL_SetOrigin(pev, m_vecOfs);
		m_fFixed = true;
		m_fOnLiving = false;
	}

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgCLTE);
	WRITE_BYTE(0x19); // VERIFY: CLTE type 25, the cam's attachment (client.dll); Killed ends it with 0xfe
	WRITE_SHORT(entindex());
	WRITE_SHORT(iHost);
	WRITE_COORD(m_vecOfs.x);
	WRITE_COORD(m_vecOfs.y);
	WRITE_COORD(m_vecOfs.z);
	WRITE_COORD(m_vecAngleOfs.x);
	WRITE_COORD(m_vecAngleOfs.y);
	WRITE_COORD(m_vecAngleOfs.z);
	MESSAGE_END();

	pev->owner = NULL;
}

// orig 0xcb7d0
void CSnarkCam::ControlThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (!UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		pev->dmg_inflictor = NULL;
		Killed(pev, GIB_NORMAL);
		return;
	}

	if (!m_fFixed && !UTIL_IsValidEntity(pev->enemy))
	{
		pev->owner = NULL;
		Killed(pev, GIB_NORMAL);
		return;
	}

	if (m_fOnLiving)
	{
		CBaseEntity* pHost = CBaseEntity::Instance(pev->enemy);
		if (pHost == NULL || !pHost->IsAlive() || pHost->pev->takedamage != DAMAGE_AIM || pHost->pev->deadflag != DEAD_NO)
		{
			pev->owner = NULL;
			Killed(pev, GIB_NORMAL);
			return;
		}
	}

	if (!m_fFixed)
	{
		entvars_t* pevHost = VARS(pev->enemy);
		pev->angles = m_vecAngleOfs + pevHost->angles;
		UTIL_MakeVectors(pevHost->angles);
		// VERIFY: the original sums these in a different order (float rounding only)
		const Vector vecOrigin = gpGlobals->v_forward * m_vecOfs.x + pevHost->origin + gpGlobals->v_right * m_vecOfs.y + gpGlobals->v_up * m_vecOfs.z;
		UTIL_SetOrigin(pev, vecOrigin);
	}
	else
	{
		pev->angles = m_vecAngleOfs;
		UTIL_SetOrigin(pev, m_vecOfs);
	}

	if (!m_fOwnerAway)
	{
		entvars_t* pevOwner = VARS(pev->dmg_inflictor);
		if ((pevOwner->origin - pev->origin).Length() > 72)
			m_fOwnerAway = true;
	}

	CBaseEntity* pOwner = CBaseEntity::Instance(pev->dmg_inflictor);
	if (pOwner == NULL || pOwner->pev->deadflag != DEAD_NO || !pOwner->IsAlive() || RC2EntityByte66(pOwner))
	{
		m_fControlling = false;
		m_iRC2Flags &= ~0x10;
		return;
	}

	if (!m_fControlling)
		return;

	// the original treats the owner as a player from here on without checking
	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(pOwner);
	UTIL_ExtendTime(pPlayer->m_flNextAttack, UTIL_WeaponTimeBase(), RC2Pref(RC2P_Snark_CamRefireWeaponTime).f);

	if (gpGlobals->time <= m_flNextInput)
		return;

	const int iButtons = pPlayer->pev->button;
	if ((iButtons & IN_ALT1) != 0)
	{
		ReleaseControl();
		return;
	}

	if ((iButtons & (IN_ATTACK2 | IN_CANCEL)) != 0)
	{
		CSnarkCam* pNext = FindNextCam((iButtons & IN_CANCEL) != 0);
		if (pNext != NULL)
		{
			DropControl();
			pNext->TakeOverControl();
			return;
		}
	}

	if ((pPlayer->pev->button & IN_ATTACK) == 0)
		return;
	if (gpGlobals->time <= m_flNextFire)
		return;
	if (pPlayer->m_flEnergy < 1.0)
		return;

	pPlayer->m_flEnergy = pPlayer->m_flEnergy - 1.0;
	UTIL_ExtendTime(pPlayer->m_flEnergyRegenTime, gpGlobals->time, 1.0);
	m_flNextFire = gpGlobals->time + RC2Pref(RC2P_Snark_CamRefireTime).f;

	const Vector vecAngles(-pPlayer->pev->v_angle.x, pPlayer->pev->v_angle.y, pPlayer->pev->v_angle.z);
	UTIL_MakeAimVectors(vecAngles);
	// the original doesn't check Create's result
	CBaseEntity* pProj = CBaseEntity::Create("snark_project", pev->origin, vecAngles, edict());
	pProj->pev->enemy = pPlayer->edict();
	pProj->pev->dmg_inflictor = pProj->pev->enemy;
	pProj->pev->velocity = gpGlobals->v_forward * pProj->pev->speed;
	pProj->pev->dmg = RC2Pref(RC2P_Snark_CamProjDamage).f;

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "debris/beamstart10.wav", 0.5, 0.9, 0, RANDOM_LONG(120, 130));
}

// orig 0xcbdb0. Walks entity indices from this one (wrapping between maxClients + 2 and
// maxEntities - 1) for another snark_cam of the same owner.
CSnarkCam* CSnarkCam::FindNextCam(bool fBackward)
{
	const int iSelf = ENTINDEX(edict());
	bool fWrapped = false;
	int i = iSelf;

	auto check = [this](int iIndex) -> CSnarkCam*
	{
		edict_t* pent = INDEXENT(iIndex);
		if (!UTIL_IsValidEntity(pent))
			return NULL;
		CBaseEntity* pEntity = CBaseEntity::Instance(pent);
		if (pEntity == NULL)
			return NULL;
		if (0 == strcmp(STRING(pEntity->pev->classname), "snark_cam") && pEntity->pev->dmg_inflictor == pev->dmg_inflictor)
			return static_cast<CSnarkCam*>(pEntity);
		return NULL;
	};

	if (!fBackward)
	{
		// the wrap sets i to maxClients + 1 and the next ++i skips it: kept
		while ((++i < gpGlobals->maxEntities && !fWrapped) || (i < iSelf && fWrapped))
		{
			if (CSnarkCam* pCam = check(i))
				return pCam;
			if (i >= gpGlobals->maxEntities - 1)
			{
				fWrapped = true;
				i = gpGlobals->maxClients + 1;
			}
		}
	}
	else
	{
		while ((--i > gpGlobals->maxClients && !fWrapped) || (i > iSelf && fWrapped))
		{
			if (CSnarkCam* pCam = check(i))
				return pCam;
			if (i <= gpGlobals->maxClients + 1)
			{
				fWrapped = true;
				i = gpGlobals->maxEntities - 1;
			}
		}
	}
	return NULL;
}

// orig 0xcbfe0
void CSnarkCam::TakeControl()
{
	if (!UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		m_fControlling = false;
		return;
	}

	CBaseEntity* pOwner = CBaseEntity::Instance(pev->dmg_inflictor);
	if (pOwner == NULL)
	{
		m_fControlling = false;
		return;
	}

	m_fControlling = true;
	m_iRC2Flags |= 0x10;
	m_flNextInput = gpGlobals->time + RC2Pref(RC2P_Snark_CamCycleInputDelay).f;

	MESSAGE_BEGIN(MSG_ONE, gmsgViewMode, g_vecZero, pev->dmg_inflictor);
	WRITE_BYTE(3); // VERIFY: ViewMode 3, looking through a cam (client.dll)
	MESSAGE_END();

	unsigned char* pvs;
	SetupVisibility(edict(), pev->dmg_inflictor, &pvs, &pvs);
	SET_VIEW(pev->dmg_inflictor, edict());

	pOwner->pev->v_angle = Vector(-pev->angles.x, pev->angles.y, 0);
	pOwner->pev->angles = pOwner->pev->v_angle;
	pOwner->pev->fixangle = 1;
	static_cast<CBaseMonster*>(pOwner)->m_flNextAttack = UTIL_WeaponTimeBase() + RC2Pref(RC2P_Snark_CamRefireWeaponTime).f;
}

// orig 0xcc140
void CSnarkCam::ReleaseControl()
{
	const bool fValid = UTIL_IsValidEntity(pev->dmg_inflictor);
	m_fControlling = false;
	if (!fValid)
		return;

	m_iRC2Flags &= ~0x10;
	unsigned char* pvs;
	SetupVisibility(pev->dmg_inflictor, pev->dmg_inflictor, &pvs, &pvs);
	SET_VIEW(pev->dmg_inflictor, pev->dmg_inflictor);

	entvars_t* pevOwner = VARS(pev->dmg_inflictor);
	pevOwner->angles = pevOwner->v_angle;
	pevOwner->v_angle.z = 0;
	pevOwner->angles.z = 0;
	pevOwner->fixangle = 1;

	MESSAGE_BEGIN(MSG_ONE, gmsgViewMode, g_vecZero, pev->dmg_inflictor);
	WRITE_BYTE(0);
	MESSAGE_END();
}

// orig 0xcc260
void CSnarkCam::DropControl()
{
	const bool fValid = UTIL_IsValidEntity(pev->dmg_inflictor);
	m_fControlling = false;
	if (fValid)
		m_iRC2Flags &= ~0x10;
}

// orig 0xcc290
void CSnarkCam::TakeOverControl()
{
	if (!UTIL_IsValidEntity(pev->dmg_inflictor))
	{
		m_fControlling = false;
		return;
	}

	m_fControlling = true;
	m_iRC2Flags |= 0x10;
	m_flNextInput = gpGlobals->time + 0.5;

	unsigned char* pvs;
	SetupVisibility(edict(), pev->dmg_inflictor, &pvs, &pvs);
	SET_VIEW(pev->dmg_inflictor, edict());

	const float flPitch = pev->angles.x;
	const float flYaw = pev->angles.y;
	entvars_t* pevOwner = VARS(pev->dmg_inflictor);
	pevOwner->v_angle = Vector(-flPitch, flYaw, 0);
	pevOwner->angles = Vector(-flPitch, flYaw, 0);
	pevOwner->fixangle = 1;
}

// orig 0xcc3b0
void CSnarkCam::Killed(entvars_t* pevAttacker, int iGib)
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "squeek/sqk_blast1.wav", 1.0, 0.5, 0, PITCH_NORM);
	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, 512, 3.0);
	UTIL_BloodDrips(pev->origin, g_vecZero, BloodColor(), 80);

	if (m_fControlling)
		ReleaseControl();

	SetThink(NULL);

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(0xfe); // VERIFY: CLTE 254 ends the effect of the type that follows on this entity
	WRITE_SHORT(entindex());
	WRITE_BYTE(0x19);
	MESSAGE_END();

	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DEAD;
	UTIL_Remove(this);
}

// orig 0xcc4a0. The cam's own projectiles don't hurt it.
bool CSnarkCam::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (pevInflictor != NULL && 0 == strcmp(STRING(pevInflictor->classname), "snark_project"))
		return false;
	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}
