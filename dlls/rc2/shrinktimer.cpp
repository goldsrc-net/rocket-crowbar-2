/***
*
*	Rocket Crowbar 2: shrink_timer, the helper entity that shrinks its pev->owner to a fifth of
*	its size for pev->dmg seconds (10 by default), lets bigger players crush it meanwhile, and
*	grows it back. File name ours. Reconstructed from rc2.dll 0xc8fb0-0xca493.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "p7_extern.h"

class CShrinkTimer : public CBaseEntity
{
public:
	void Spawn() override;

	void EXPORT ShrinkInit();
	void EXPORT ShrinkEnt();
	void EXPORT ShrunkThink();
	void EXPORT UnShrinkEnt();

	bool RemoveIfOwnerGone();																   // name ours
	bool TrySquash(CBaseEntity* pShrunk, CBaseEntity* pSquasher, Vector vecTop, Vector vecHit);   // name ours
	bool CheckSquash(CBaseEntity* pShrunk, CBaseEntity* pOther, Vector vecTop, Vector vecHit); // name ours

	Vector m_vecMins;	  // orig +0x70, name ours: the owner's size, view height and health before shrinking
	Vector m_vecMaxs;	  // orig +0x7c, name ours
	Vector m_vecViewOfs;  // orig +0x88, name ours
	float m_flHealth;	  // orig +0x94, name ours
	float m_flMaxHealth;  // orig +0x98, name ours
	float m_flLastTime;	  // orig +0x9c, name ours: the previous think, for the rates
};

LINK_ENTITY_TO_CLASS(shrink_timer, CShrinkTimer); // orig 0xc8fb0

// orig 0xc9020
void CShrinkTimer::Spawn()
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	pev->dmg = 10.0;	// how long the owner stays shrunk
	pev->dmgtime = -1.0;
	SetThink(&CShrinkTimer::ShrinkInit);
	pev->nextthink = gpGlobals->time + 0.01;
}

// orig 0xc9080. Removes the timer unless its owner is a valid entity, not dead, with health left.
bool CShrinkTimer::RemoveIfOwnerGone()
{
	if (UTIL_IsValidEntity(pev->owner))
	{
		entvars_t* pevOwner = pev->owner != NULL ? VARS(pev->owner) : NULL;
		if (pevOwner->deadflag == DEAD_NO && pevOwner->health > 0)
			return false;
	}
	UTIL_Remove(this);
	return true;
}

// orig 0xc90f0
void CShrinkTimer::ShrinkInit()
{
	if (RemoveIfOwnerGone())
		return;

	CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner);
	if (pOwner->IsPlayer())
	{
		CBasePlayer* pPlayer = static_cast<CBasePlayer*>(CBaseEntity::Instance(pev->owner));
		if (pPlayer->m_iRidingIndex != 0)
			pPlayer->StopRiding(0);
	}

	entvars_t* pevOwner = VARS(pev->owner);
	m_vecMins = pevOwner->mins;
	m_vecMaxs = pevOwner->maxs;
	m_vecViewOfs = pevOwner->view_ofs;
	m_flHealth = pevOwner->health;
	m_flMaxHealth = pevOwner->max_health;
	pevOwner->scale = 1.0;

	if (CBaseEntity::Instance(pev->owner)->IsPlayer())
	{
		g_engfuncs.pfnSetPhysicsKeyValue(pev->owner, "shr", "1");
		CBasePlayer* pPlayer = static_cast<CBasePlayer*>(CBaseEntity::Instance(pev->owner));
		pPlayer->m_fShieldOn = false;
		pPlayer->m_iShrunk = 1;
		if (pPlayer->m_iTransformType == 0)
			m_vecViewOfs.z = 28.0;

		if (!pPlayer->m_bTransforming && pPlayer->m_iTransformType == 0)
		{
			pPlayer->m_iHideHUD |= 0x10; // VERIFY: RC2's HIDEHUD bit 0x10 (client.dll)
			if (pPlayer->m_pActiveItem != NULL)
			{
				pPlayer->m_pActiveItem->Holster(); // SDK 2.3's Holster(0)
				pPlayer->pev->weaponmodel = 0;
				pPlayer->pev->viewmodel = 0;
			}
			if (pPlayer->m_pTank != NULL)
			{
				pPlayer->m_pTank->Use(pPlayer, pPlayer, USE_OFF, 0);
				pPlayer->m_pTank = NULL;
			}
		}

		if (pPlayer->m_iShoulderRider[0] != 0)
		{
			CBasePlayer* pRider = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(pPlayer->m_iShoulderRider[0]));
			if (pRider != NULL)
				pRider->StopRiding(0);
		}
		if (pPlayer->m_iShoulderRider[1] != 0)
		{
			CBasePlayer* pRider = static_cast<CBasePlayer*>(UTIL_PlayerByIndex(pPlayer->m_iShoulderRider[1]));
			if (pRider != NULL)
				pRider->StopRiding(0);
		}
	}

	if ((VARS(pev->owner)->flags & FL_MONSTER) != 0)
	{
		// a shrunk monster forgets its enemies
		CBaseMonster* pMonster = static_cast<CBaseMonster*>(CBaseEntity::Instance(pev->owner));
		pMonster->m_hEnemy = NULL;
		for (int i = 0; i < 4; i++)
			pMonster->m_hOldEnemy[i] = NULL;
	}

	const Vector vecOrigin = VARS(pev->owner)->origin + Vector(0, 0, 1);
	MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, vecOrigin);
	WRITE_BYTE(0x28); // VERIFY: CLTE type 40, the shrink effect (client.dll); byte 0 shrinking, 1 growing
	WRITE_SHORT(ENTINDEX(pev->owner));
	WRITE_BYTE(0);
	MESSAGE_END();

	SetThink(&CShrinkTimer::ShrinkEnt);
	m_flLastTime = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.01;
}

// orig 0xc94b0. Scale falls 16/15 a second to 0.2; size, view height and health follow.
void CShrinkTimer::ShrinkEnt()
{
	if (RemoveIfOwnerGone())
		return;

	const float flInterval = gpGlobals->time - m_flLastTime;
	m_flLastTime = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.1;

	entvars_t* pevOwner = pev->owner != NULL ? VARS(pev->owner) : NULL;
	pevOwner->scale = pevOwner->scale - flInterval * 1.0666667f;
	const bool fDone = pevOwner->scale < 0.2;
	if (fDone)
		pevOwner->scale = 0.2;

	pevOwner->maxs = m_vecMaxs * pevOwner->scale;
	pevOwner->mins = m_vecMins * pevOwner->scale;
	pevOwner->view_ofs = m_vecViewOfs * pevOwner->scale;

	if ((pevOwner->flags & FL_ONGROUND) != 0)
	{
		// keep the feet on the ground as the hull shrinks
		const Vector vecFeet(pevOwner->origin.x, pevOwner->origin.y, pevOwner->mins.z + pevOwner->origin.z);
		const Vector vecBelow(pevOwner->origin.x, pevOwner->origin.y, (pevOwner->mins.z - 8) + pevOwner->origin.z);
		TraceResult tr;
		UTIL_TraceLine(vecFeet, vecBelow, ignore_monsters, pevOwner->pContainingEntity, &tr);
		if (tr.flFraction < 1.0)
			UTIL_SetOrigin(pevOwner, Vector(tr.vecEndPos.x, tr.vecEndPos.y, tr.vecEndPos.z + -pevOwner->mins.z));
	}

	if (CBaseEntity::Instance(pev->owner)->IsPlayer())
		pevOwner->view_ofs.z = pevOwner->scale * 36 + pevOwner->view_ofs.z;

	UTIL_SetSize(pevOwner, pevOwner->mins, pevOwner->maxs);

	pevOwner->health = pevOwner->health - (m_flHealth - 1.0f) * 1.3333334f * flInterval;
	if ((int)pevOwner->health < 1)
		pevOwner->health = 1.0;
	pevOwner->max_health = pevOwner->health;

	if (fDone)
	{
		if ((pevOwner->flags & (FL_CLIENT | FL_MONSTER)) != 0)
			static_cast<CBaseAnimating*>(CBaseEntity::Instance(pev->owner))->ResetSequenceInfo();

		MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
		WRITE_BYTE(0xfe);
		WRITE_SHORT(ENTINDEX(pev->owner));
		WRITE_BYTE(0x28);
		MESSAGE_END();

		SetThink(&CShrinkTimer::ShrunkThink);
		pev->nextthink = gpGlobals->time + 0.1;
		pev->dmgtime = pev->dmg + gpGlobals->time;
	}

	// the original stops all four channels on the timer itself, which plays nothing (kept)
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "common/null.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "common/null.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/null.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);
}

// orig 0xc9910. Crushes the shrunk owner when the line from its head to the squasher is clear.
bool CShrinkTimer::TrySquash(CBaseEntity* pShrunk, CBaseEntity* pSquasher, Vector vecTop, Vector vecHit)
{
	if (pShrunk->IsPlayer() && !g_pGameRules->FPlayerCanTakeDamage(static_cast<CBasePlayer*>(pShrunk), pSquasher))
		return false;

	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	TraceResult tr;
	UTIL_TraceLine(vecTop, vecHit, dont_ignore_monsters, pev->owner, &tr);
	if (tr.pHit != pSquasher->edict() && tr.flFraction < 1.0)
		return false;

	pShrunk->pev->armorvalue = 0;
	pShrunk->TakeDamage(UTIL_NamedInflictor("squash"), pSquasher->pev, 50000, DMG_CRUSH);
	return true;
}

// orig 0xc99e0. pOther squashes the shrunk owner when it stands on it, on one of a player's
// parts, or has its feet within 16 up/down and 8 across of the owner's head.
bool CShrinkTimer::CheckSquash(CBaseEntity* pShrunk, CBaseEntity* pOther, Vector vecTop, Vector vecHit)
{
	if (pOther->pev->groundentity != pShrunk->edict())
	{
		if (!pShrunk->IsPlayer() || !UTIL_PlayerPartIsGround(pShrunk, pOther->edict()))
		{
			const Vector vecFeet(pOther->pev->origin.x, pOther->pev->origin.y, pOther->pev->mins.z + pOther->pev->origin.z);
			const Vector vecHead(pShrunk->pev->origin.x, pShrunk->pev->origin.y, pShrunk->pev->maxs.z + 1 + pShrunk->pev->origin.z);
			const Vector vecDelta = vecHead - vecFeet;
			if (fabs(vecDelta.z) > 16)
				return false;
			if (sqrt(vecDelta.y * vecDelta.y + vecDelta.x * vecDelta.x) > 8)
				return false;
		}
	}
	return TrySquash(pShrunk, pOther, vecTop, vecHit);
}

// orig 0xc9b70. Waits pev->dmg seconds, crushing the owner under any full-size live player
// that comes within 16 units of its head.
void CShrinkTimer::ShrunkThink()
{
	if (RemoveIfOwnerGone())
		return;

	pev->nextthink = gpGlobals->time + 0.05;

	if (pev->dmgtime < gpGlobals->time && pev->dmg > 0)
	{
		SetThink(&CShrinkTimer::UnShrinkEnt);
		m_flLastTime = gpGlobals->time;
		pev->nextthink = gpGlobals->time + 0.01;

		const Vector vecOrigin = VARS(pev->owner)->origin + Vector(0, 0, 1);
		MESSAGE_BEGIN(MSG_PVS, gmsgCLTE, vecOrigin);
		WRITE_BYTE(0x28);
		WRITE_SHORT(ENTINDEX(pev->owner));
		WRITE_BYTE(1);
		MESSAGE_END();
		return;
	}

	VARS(pev->owner)->scale = 0.2;

	CBaseEntity* pShrunk = CBaseEntity::Instance(pev->owner);
	const Vector vecTop(pShrunk->pev->origin.x, pShrunk->pev->origin.y, pShrunk->pev->maxs.z + 2 + pShrunk->pev->origin.z);

	CBaseEntity* pEntity = NULL;
	Vector vecHit;
	while ((pEntity = UTIL_FindEntityNearestInSphere(pEntity, vecTop, 16, NULL, &vecHit)) != NULL)
	{
		if (!pEntity->IsActivePlayer() || pEntity->IsBSPModel() || !pEntity->IsAlive())
			continue;
		if (pEntity->pev->scale < 1.0 && pEntity->pev->scale != 0.0)
			continue;

		if (CheckSquash(pShrunk, pEntity, vecTop, vecHit))
		{
			SetThink(NULL);
			SetTouch(NULL);
			pev->flags |= FL_KILLME;
			pev->solid = SOLID_NOT;
			return;
		}
	}
}

// orig 0xc9df0. Grows the owner back at 16/15 a second, crushing it when the space above is
// blocked, then restores it and removes the timer once a free hull is checked.
void CShrinkTimer::UnShrinkEnt()
{
	if (RemoveIfOwnerGone())
		return;

	const float flInterval = gpGlobals->time - m_flLastTime;
	m_flLastTime = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.1;

	entvars_t* pevOwner = pev->owner != NULL ? VARS(pev->owner) : NULL;
	const float flScale = flInterval * 1.0666667f + pevOwner->scale;
	pevOwner->scale = flScale;
	if (flScale > 1.0)
		pevOwner->scale = 1.0;

	CBaseEntity* pShrunk = CBaseEntity::Instance(pev->owner);
	const Vector vecOldTop(pevOwner->origin.x, pevOwner->origin.y, pevOwner->maxs.z + pevOwner->origin.z);
	const float flOldMinsZ = pevOwner->mins.z;

	pevOwner->maxs = m_vecMaxs * pevOwner->scale;
	pevOwner->mins = m_vecMins * pevOwner->scale;
	pevOwner->view_ofs = m_vecViewOfs * pevOwner->scale;
	if (pShrunk->IsPlayer())
		pevOwner->view_ofs.z = pevOwner->scale * 36 + pevOwner->view_ofs.z;

	// VERIFY: this only runs when mins.z rises, which growing never does for a hull with
	// mins.z < 0; read as the original has it
	if (pevOwner->mins.z - flOldMinsZ > 0)
	{
		if ((pevOwner->flags & FL_ONGROUND) != 0)
		{
			const Vector vecBelow(pevOwner->origin.x, pevOwner->origin.y, pevOwner->origin.z - (pevOwner->mins.z - 8));
			TraceResult tr;
			UTIL_TraceLine(pevOwner->origin, vecBelow, ignore_monsters, pevOwner->pContainingEntity, &tr);
			if (tr.flFraction < 1.0)
				UTIL_SetOrigin(pevOwner, Vector(tr.vecEndPos.x, tr.vecEndPos.y, tr.vecEndPos.z + pevOwner->mins.z));
		}
		else
		{
			UTIL_SetOrigin(pevOwner, Vector(pevOwner->origin.x, pevOwner->origin.y, (-pevOwner->mins.z - flOldMinsZ) + pevOwner->origin.z));
		}
	}

	const Vector vecNewTop(pevOwner->origin.x, pevOwner->origin.y, pevOwner->maxs.z + pevOwner->origin.z);
	TraceResult tr;
	UTIL_TraceLine(vecOldTop, vecNewTop, dont_ignore_monsters, pev->owner, &tr);
	if (tr.flFraction < 1.0 || 0 != tr.fStartSolid || 0 != tr.fAllSolid)
	{
		// no room to grow: crushed by whatever is there
		pShrunk->TakeDamage(UTIL_NamedInflictor("squash"), tr.pHit != NULL ? VARS(tr.pHit) : NULL, 50000, DMG_CRUSH);
		SetThink(NULL);
		SetTouch(NULL);
		UTIL_Remove(this);
		return;
	}

	UTIL_SetSize(pevOwner, pevOwner->mins, pevOwner->maxs);

	const float flHealth = (m_flHealth - 1.0f) * 1.3333334f * flInterval + pevOwner->health;
	pevOwner->health = flHealth;
	if (flHealth > m_flMaxHealth)
		pevOwner->health = m_flMaxHealth;
	if (pevOwner->health < 1.0)
		pevOwner->health = 1.0;
	pevOwner->max_health = pevOwner->health;

	if (flScale <= 1.0)
		return;

	// grown back
	pevOwner->scale = 0;
	if ((pevOwner->flags & (FL_CLIENT | FL_MONSTER)) != 0)
		static_cast<CBaseAnimating*>(CBaseEntity::Instance(pev->owner))->ResetSequenceInfo();

	MESSAGE_BEGIN(MSG_ALL, gmsgCLTE);
	WRITE_BYTE(0xfe);
	WRITE_SHORT(ENTINDEX(pev->owner));
	WRITE_BYTE(0x28);
	MESSAGE_END();

	if (pShrunk->IsPlayer())
	{
		CBasePlayer* pPlayer = static_cast<CBasePlayer*>(pShrunk);
		g_engfuncs.pfnSetPhysicsKeyValue(pev->owner, "shr", "0");
		pPlayer->m_iShrunk = 0;
		if (!pPlayer->m_bTransforming && pPlayer->m_iTransformType == 0)
		{
			pPlayer->m_iHideHUD &= ~0x10;
			if (pPlayer->m_pActiveItem != NULL)
				pPlayer->m_pActiveItem->Deploy();
		}
		pevOwner->view_ofs = m_vecViewOfs;
		UTIL_SetOrigin(pevOwner, Vector(pevOwner->origin.x, pevOwner->origin.y, pevOwner->origin.z + 36));
	}

	pevOwner->max_health = m_flMaxHealth;

	UTIL_TraceHull(pShrunk->Center(), pShrunk->Center(), dont_ignore_monsters, human_hull, pev->owner, &tr);
	if (tr.flFraction < 1.0 || 0 != tr.fAllSolid || 0 != tr.fStartSolid)
	{
		// stuck inside something: a player there gets the kill, otherwise the owner kills itself
		entvars_t* pevSquasher;
		if (UTIL_IsValidEntity(tr.pHit) && CBaseEntity::Instance(tr.pHit)->IsPlayer())
			pevSquasher = tr.pHit != NULL ? VARS(tr.pHit) : NULL;
		else
			pevSquasher = pShrunk->pev;
		pShrunk->TakeDamage(UTIL_NamedInflictor("squash"), pevSquasher, 50000, DMG_CRUSH);
	}

	SetThink(NULL);
	SetTouch(NULL);
	UTIL_Remove(this);
}
