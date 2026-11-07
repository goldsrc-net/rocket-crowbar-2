/***
*
*	Rocket Crowbar 2: virtual hulls (orig 0xf6fb0-0xf7780, the run after SDK 2.3's util.cpp).
*	A virtual hull is an invisible solid box a player keeps (CBasePlayer::m_rgpVirtualHulls,
*	up to 16): it stands in for the player where the player's own hull is wrong, as when shrunk.
*	What touches, uses or blocks it touches, uses or blocks the player; damage to it goes to the
*	player (GetEffectedEnt). Every name but CVirtualHull's own and its EXPORTs is ours.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

extern void DispatchTouch(edict_t* pentTouched, edict_t* pentOther);
extern void DispatchBlocked(edict_t* pentBlocked, edict_t* pentOther);

#include "virtualhull.h"

// the player's hull iHull, made when it doesn't exist yet
CVirtualHull* GetVirtualHull(CBasePlayer* pPlayer, int iHull) // orig 0xf7550
{
	CVirtualHull* pHull = pPlayer->m_rgpVirtualHulls[iHull];
	if (!pHull)
	{
		pHull = (CVirtualHull*)CBaseEntity::Create("virtual_hull", g_vecZero, g_vecZero, pPlayer->edict());
		pHull->m_hOwner = pPlayer;
		pHull->pev->owner = pPlayer->edict();
		pHull->pev->modelindex = pPlayer->pev->modelindex;
		SET_MODEL(pHull->edict(), STRING(pPlayer->pev->model));
		pPlayer->m_rgpVirtualHulls[iHull] = pHull;
	}
	return pHull;
}

// ten times a second while alive: the hulls the player needs now, and the rest removed. Only
// one is ever needed: hull 0, a small box, while the player is shrunk.
void CBasePlayer::UpdateVirtualHulls() // orig 0xf6fb0
{
	if (m_flNextHullUpdate > gpGlobals->time || pev->deadflag != DEAD_NO)
		return;

	m_flNextHullUpdate = gpGlobals->time + 0.1;

	unsigned char rgbUsed[2] = {0, 0}; // a bit per hull
	UTIL_MakeVectors(pev->angles);

	if (0 != m_iShrunk)
	{
		CVirtualHull* pHull = GetVirtualHull(this, 0);
		pHull->pev->takedamage = DAMAGE_YES;
		UTIL_SetSize(pHull->pev, Vector(-3.2, -3.2, 0), Vector(3.2, 3.2, 14.4));
		UTIL_SetOrigin(pHull->pev, pev->origin);
		pHull->pev->owner = edict();
		rgbUsed[0] |= 1;
	}

	for (int i = 0; i < 16; i++)
	{
		if ((rgbUsed[i / 8] & (1 << (i % 8))) == 0 && m_rgpVirtualHulls[i])
		{
			m_rgpVirtualHulls[i]->pev->solid = SOLID_NOT;
			m_rgpVirtualHulls[i]->pev->flags |= FL_KILLME;
			m_rgpVirtualHulls[i] = NULL;
		}
	}
}

// For the player's movement: each of player iPlayer's hulls made non-solid and handed to
// pfnSetHull out of the way (8192, 8192, 8192), or solid at its own origin. pfnSetHull is
// pm_shared's (orig 0xa8d10): it sets the solid and origin of the matching physents.
void PM_HideOwnedEnts(int iPlayer, PM_SETENTFUNC pfnSetHull) // orig 0xf7150
{
	edict_t* pent = INDEXENT(iPlayer);
	if (!UTIL_IsValidEntity(pent))
		return;
	CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pent);
	if (!pPlayer)
		return;

	Vector vecAway(8192, 8192, 8192);
	for (int i = 0; i < 16; i++)
	{
		CVirtualHull* pHull = pPlayer->m_rgpVirtualHulls[i];
		if (pHull)
		{
			pHull->pev->solid = SOLID_NOT;
			pfnSetHull(ENTINDEX(pHull->edict()), SOLID_NOT, vecAway);
		}
	}
}

void PM_RestoreOwnedEnts(int iPlayer, PM_SETENTFUNC pfnSetHull) // orig 0xf7200
{
	edict_t* pent = INDEXENT(iPlayer);
	if (!UTIL_IsValidEntity(pent))
		return;
	CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pent);
	if (!pPlayer)
		return;

	for (int i = 0; i < 16; i++)
	{
		CVirtualHull* pHull = pPlayer->m_rgpVirtualHulls[i];
		if (pHull)
		{
			pHull->pev->solid = SOLID_BBOX;
			pfnSetHull(ENTINDEX(pHull->edict()), SOLID_BBOX, pHull->pev->origin);
		}
	}
}

LINK_ENTITY_TO_CLASS(virtual_hull, CVirtualHull); // orig 0xf7290

void CVirtualHull::Spawn() // orig 0xf7300
{
	pev->solid = SOLID_BBOX;
	pev->classname = MAKE_STRING("virtual_hull");
	pev->movetype = MOVETYPE_NONE;
	pev->renderamt = 0;
	pev->rendermode = kRenderTransTexture;
	m_iRC2Flags = 4; // VERIFY: what bit 4 of CBaseEntity's m_iRC2Flags (orig +0x68) means
	SetTouch(&CVirtualHull::HullTouch);
	SetUse(&CVirtualHull::HullUse);
	SetBlocked(&CVirtualHull::HullBlocked);
}

// true (and the hull goes) when its player is gone
bool CVirtualHull::OwnerGone() // orig 0xf7370
{
	if (m_hOwner && UTIL_IsValidEntity(pev->owner))
		return false;

	SetThink(NULL);
	SetTouch(NULL);
	SetUse(NULL);
	SetBlocked(NULL);
	pev->solid = SOLID_NOT;
	pev->flags |= FL_KILLME;
	return true;
}

CBaseEntity* CVirtualHull::GetEffectedEnt() // orig 0xf73d0
{
	if (OwnerGone())
		return this;
	return m_hOwner;
}

void CVirtualHull::HullTouch(CBaseEntity* pOther) // orig 0xf73f0
{
	if (!OwnerGone())
		DispatchTouch(m_hOwner->edict(), pOther->edict());
}

void CVirtualHull::HullUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) // orig 0xf7430
{
	if (!OwnerGone())
		m_hOwner->Use(pActivator, pCaller, useType, value);
}

void CVirtualHull::HullBlocked(CBaseEntity* pOther) // orig 0xf7470
{
	if (!OwnerGone())
		DispatchBlocked(m_hOwner->edict(), pOther->edict());
}

// the player sees through its own hulls and those of the players it rides or that ride it
bool CVirtualHull::IsViewOfPlayer(CBaseEntity* pPlayer) // orig 0xf74b0
{
	if (OwnerGone())
		return false;

	if (pPlayer->edict() == m_hOwner->edict())
		return false;

	CBasePlayer* pOwner = (CBasePlayer*)(CBaseEntity*)m_hOwner;
	const int iIndex = ENTINDEX(pPlayer->edict());
	if (0 != iIndex && (pOwner->m_iRidingIndex == iIndex || pOwner->m_iShoulderRider[0] == iIndex || pOwner->m_iShoulderRider[1] == iIndex))
		return true;

	return CBaseEntity::IsViewOfPlayer(pPlayer);
}

// at a level change: the hulls of players who aren't connected any more go
void RemoveOrphanVirtualHulls() // orig 0xf7600
{
	int iConnected = 0;
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		const int iBit = 1 << (i - 1);
		if ((iBit & g_iRC2ClientBits) == 0)
			continue;
		edict_t* pent = INDEXENT(i);
		if (UTIL_IsValidEntity(pent) && CBaseEntity::Instance(pent))
			iConnected |= iBit;
	}

	for (int i = gpGlobals->maxClients + 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t* pent = INDEXENT(i);
		if (!UTIL_IsValidEntity(pent) || 0 != strcmp(STRING(VARS(pent)->classname), "virtual_hull"))
			continue;
		CVirtualHull* pHull = (CVirtualHull*)CBaseEntity::Instance(pent);
		if (!pHull)
			continue;
		if (pHull->m_hOwner && (iConnected & (1 << (ENTINDEX(pHull->m_hOwner->edict()) - 1))) != 0)
			continue;
		pHull->pev->flags |= FL_KILLME;
	}
}

// whether pent stands on one of the player's hulls
bool IsOnVirtualHull(CBasePlayer* pPlayer, edict_t* pent) // orig 0xf7780
{
	for (int i = 0; i < 16; i++)
	{
		if (pPlayer->m_rgpVirtualHulls[i] && pPlayer->m_rgpVirtualHulls[i]->edict() == pent->v.groundentity)
			return true;
	}
	return false;
}
