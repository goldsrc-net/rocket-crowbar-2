/***
*
*	Rocket Crowbar 2: client effects CLTE 10-12: the glock gibs (see clte.h).
*
****/

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "event_api.h"
#include "r_efx.h"
#include "com_model.h"
#include "triangleapi.h"
#include "pm_defs.h"
#include "clte.h"
#include "clte_types.h"

void VectorAngles(const float* forward, float* angles);

// --- CLTE 10: a glock gib's bleeding (glockgibs.cpp, the sticky and normal gibs) ---

// vtable client 0x1011d9ac, 0x18 bytes
class CTentGibBleed : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;		// +8
	float m_flNextDrip; // +0xc
	int m_iBlood;		// +0x10: sprites/blood.spr
	int m_iBloodSpray;	// +0x14: sprites/bloodspray.spr
};

// orig client 0x601f0. short gib
CClientTent* CLTE_CreateGibBleed(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentGibBleed*>(pCopy));

	CTentGibBleed* p = new CTentGibBleed;
	p->m_iEntity = READ_SHORT();
	p->m_flNextDrip = gEngfuncs.pfnRandomFloat(0.05, 0.2);
	p->m_iBloodSpray = 0;
	p->m_iBlood = 0;
	if (!gEngfuncs.GetEntityByIndex(p->m_iEntity))
	{
		p->m_iEntity = 0;
		g_pClientTents[iSlot] = p;
		return p;
	}
	if (gEngfuncs.CL_LoadModel("sprites/blood.spr", &p->m_iBlood))
		gEngfuncs.CL_LoadModel("sprites/bloodspray.spr", &p->m_iBloodSpray);
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x60460. A blood sprite every 0.2-0.3 s
bool CTentGibBleed::Update(float flFrametime)
{
	if (0 == m_iEntity || 0 == m_iBlood || 0 == m_iBloodSpray)
		return true;

	m_flNextDrip = m_flNextDrip - flFrametime;
	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iEntity);
	if (!pEnt)
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	if (m_flNextDrip < 0)
	{
		m_flNextDrip = gEngfuncs.pfnRandomFloat(0.2, 0.3);
		gEngfuncs.pEfxAPI->R_BloodSprite(pEnt->origin, 247, m_iBloodSpray, m_iBlood, 3.0);
	}
	return false;
}

// orig client 0x60530
bool CTentGibBleed::KillEntity(int iEnt)
{
	return iEnt == m_iEntity;
}

// --- CLTE 11: the sticky gib stuck to something, puffing green gas (glockgibs.cpp) ---

// vtable client 0x1011d9c4, 0x38 bytes
class CTentStickyGib : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;		 // +8: the gib
	int m_iTarget;		 // +0xc: what it is stuck to (0: the world)
	Vector m_vecAngles;	 // +0x10: its angles off the target's
	Vector m_vecOffset;	 // +0x1c: its place in the target's forward, right, up (the world: its origin)
	Vector m_vecTip;	 // +0x28: where the gas comes out, the same way
	float m_flNextPuff;	 // +0x34
};

// orig client 0x60550. short gib, short target, coord x3 offset, coord x3 angles, coord x3 tip
CClientTent* CLTE_CreateStickyGib(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentStickyGib*>(pCopy));

	CTentStickyGib* p = new CTentStickyGib;
	p->m_iEntity = READ_SHORT();
	p->m_iTarget = READ_SHORT();
	p->m_flNextPuff = 0;
	if (!gEngfuncs.GetEntityByIndex(p->m_iEntity))
	{
		p->m_iEntity = 0;
		g_pClientTents[iSlot] = p;
		return p;
	}

	// it bleeds no more
	CLTE_KillByEntity(p->m_iEntity, 10);

	if (0 != p->m_iTarget && !gEngfuncs.GetEntityByIndex(p->m_iTarget))
		p->m_iTarget = 0;

	p->m_vecOffset.x = READ_COORD();
	p->m_vecOffset.y = READ_COORD();
	p->m_vecOffset.z = READ_COORD();
	p->m_vecAngles.x = READ_COORD();
	p->m_vecAngles.y = READ_COORD();
	p->m_vecAngles.z = READ_COORD();
	p->m_vecTip.x = READ_COORD();
	p->m_vecTip.y = READ_COORD();
	p->m_vecTip.z = READ_COORD();
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x60840. Holds the gib on its target (scaled with it; a player's pitch is reversed
// for the axes, anything else's for the angles), and every 0.2-0.3 s a green steam puff near the
// tip unless that is in water
bool CTentStickyGib::Update(float flFrametime)
{
	if (0 == m_iEntity)
		return true;

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iEntity);
	if (!pEnt)
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	Vector vecForward, vecRight, vecUp;
	float flScale = 1;
	if (0 == m_iTarget)
	{
		pEnt->origin = m_vecOffset;
		pEnt->angles = m_vecAngles;
	}
	else
	{
		cl_entity_t* pTarget = gEngfuncs.GetEntityByIndex(m_iTarget);
		if (!pTarget)
		{
			KillEntity(m_iEntity);
			m_iTarget = 0;
			return true;
		}

		flScale = CLTE_GetEntityScale(m_iTarget);
		Vector vecAngles = pTarget->angles;
		if (0 != pTarget->player)
			vecAngles.x = -vecAngles.x;
		gEngfuncs.pfnAngleVectors(vecAngles, vecForward, vecRight, vecUp);

		const Vector vecUpOfs = (vecUp * m_vecOffset.z) * flScale;
		const Vector vecRightOfs = (vecRight * m_vecOffset.y) * flScale;
		const Vector vecForwardOfs = (vecForward * m_vecOffset.x) * flScale;
		const Vector vecOrigin = ((vecForwardOfs + CLTE_GetOrigin(m_iTarget)) + vecRightOfs) + vecUpOfs;

		if (0 == pTarget->player)
			vecAngles.x = -vecAngles.x;

		pEnt->origin = vecOrigin;
		pEnt->angles = vecAngles + m_vecAngles;
	}

	m_flNextPuff = m_flNextPuff - flFrametime;
	if (!(m_flNextPuff < 0))
		return false;

	m_flNextPuff = gEngfuncs.pfnRandomFloat(0.2, 0.3);
	int iModel;
	model_s* pSteam = gEngfuncs.CL_LoadModel("sprites/stmbal1.spr", &iModel);
	if (!pSteam)
		return false;

	Vector vecTip;
	if (0 == m_iTarget)
	{
		vecTip = m_vecTip;
	}
	else
	{
		if (!gEngfuncs.GetEntityByIndex(m_iTarget))
		{
			KillEntity(m_iEntity);
			m_iTarget = 0;
			return true;
		}

		flScale = CLTE_GetEntityScale(m_iTarget);
		const Vector vecUpOfs = (vecUp * m_vecTip.z) * flScale;
		const Vector vecRightOfs = (vecRight * m_vecTip.y) * flScale;
		const Vector vecForwardOfs = (vecForward * m_vecTip.x) * flScale;
		vecTip = ((vecForwardOfs + CLTE_GetOrigin(m_iTarget)) + vecRightOfs) + vecUpOfs;
	}

	if (gEngfuncs.PM_PointContents(vecTip, NULL) == CONTENTS_WATER)
		return false;

	const float x = gEngfuncs.pfnRandomFloat(-12, 12);
	const float y = gEngfuncs.pfnRandomFloat(-12, 12);
	const float z = gEngfuncs.pfnRandomFloat(-12, 12);
	Vector vecPuff(vecTip.x + x, vecTip.y + y, vecTip.z + z);

	TEMPENTITY* pPuff = gEngfuncs.pEfxAPI->CL_TempEntAlloc(vecPuff, pSteam);
	if (pPuff)
	{
		pPuff->entity.curstate.scale = 1.8;
		pPuff->die = pPuff->die + 1.5;
		pPuff->flags |= FTENT_SPRANIMATE;
		pPuff->entity.curstate.renderamt = 180;
		pPuff->entity.curstate.framerate = 20;
		pPuff->entity.curstate.rendermode = kRenderGlow;
		pPuff->entity.curstate.renderfx = kRenderFxNoDissipation;
		pPuff->entity.curstate.rendercolor.r = 0;
		pPuff->entity.curstate.rendercolor.g = 255;
		pPuff->entity.curstate.rendercolor.b = 0;
		pPuff->entity.curstate.frame = 0;
		pPuff->frameMax = 20;
	}
	return false;
}

// orig client 0x61290
bool CTentStickyGib::KillEntity(int iEnt)
{
	return iEnt == m_iEntity;
}

// --- CLTE 12: the transphasic gib, flown on the client (glock quaternary; the glock event sends
// it through the loopback) ---

// vtable client 0x1011d9dc, 0x88 bytes
class CTentTransphasic : public CClientTent
{
public:
	CTentTransphasic();
	CTentTransphasic(const CTentTransphasic&) = default;
	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool TraceStep(Vector vecStart, Vector vecEnd, Vector vecDir, int iDepth, int iIgnore);
	void Decals(pmtrace_t* pTrace);
	void EntryWound(pmtrace_t* pTrace);

	Vector m_vecPos;		// +8
	Vector m_vecTracePos;	// +0x14: where the last trace ended
	Vector m_vecPrevPos;	// +0x20
	Vector m_vecDir;		// +0x2c
	float m_flLife;			// +0x38: [Glock] TransphasicMaxLife
	float m_flSolidDist;	// +0x3c: how far it has gone through solid since it last flew free
	float m_flNextTrace;	// +0x40
	TEMPENTITY* m_pGib;		// +0x44
	TEMPENTITY* m_pFlare;	// +0x48
	dlight_t* m_pLight;		// +0x4c
	dlight_t* m_pElight;	// +0x50
	int m_iBloodSpray;		// +0x54
	int m_iBlood;			// +0x58
	model_s* m_pShockwave;	// +0x5c
	Vector m_vecForward;	// +0x60
	Vector m_vecRight;		// +0x6c
	Vector m_vecUp;			// +0x78
	float m_flHeadStart;	// +0x84: the shooter's latency, caught up at up to twice the frame
};

// orig client 0x612b0
CClientTent* CLTE_CreateTransphasic(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentTransphasic*>(pCopy));

	CTentTransphasic* p = new CTentTransphasic;
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x62780. short latency in ms, coord x3 start, short x3 direction (32767ths). Original
// bug kept: the flare, the lights and the shockwave are left unset when what precedes them fails
// (no gib model, cl_detail below 0), and Update uses them
CTentTransphasic::CTentTransphasic()
{
	m_flHeadStart = (float)READ_SHORT() * 0.001;
	m_vecPos.x = READ_COORD();
	m_vecPos.y = READ_COORD();
	m_vecPos.z = READ_COORD();
	m_vecDir.x = (float)READ_SHORT() * 3.051851e-05;
	m_vecDir.y = (float)READ_SHORT() * 3.051851e-05;
	m_vecDir.z = (float)READ_SHORT() * 3.051851e-05;
	m_vecDir = m_vecDir.Normalize();
	m_vecTracePos = m_vecPos;
	m_vecPrevPos = m_vecPos;
	m_flLife = RC2Pref(RC2P_Glock_TransphasicMaxLife).f;
	m_flSolidDist = 0;
	m_flNextTrace = 0;
	m_pGib = NULL;

	Vector vecAngles;
	VectorAngles(m_vecDir, vecAngles);
	vecAngles.x = -vecAngles.x;
	gEngfuncs.pfnAngleVectors(vecAngles, m_vecForward, m_vecRight, m_vecUp);

	int iModel;
	model_s* pGib = gEngfuncs.CL_LoadModel("models/hgibs.mdl", &iModel);
	if (!pGib)
		return;
	m_pGib = gEngfuncs.pEfxAPI->CL_TempEntAlloc(m_vecPos, pGib);
	if (!m_pGib)
		return;

	m_pGib->flags |= FTENT_PERSIST | FTENT_ROTATE;
	m_pGib->die = m_pGib->die + 10.0;
	// three random vectors drawn, one component kept of each
	m_pGib->entity.baseline.angles.x = CLTE_RandomVector(-1000, 1000).x;
	m_pGib->entity.baseline.angles.y = CLTE_RandomVector(-1000, 1000).y;
	m_pGib->entity.baseline.angles.z = CLTE_RandomVector(-1000, 1000).z;

	if (g_iClientDetail >= 0)
	{
		m_pLight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		if (m_pLight)
		{
			m_pLight->origin = m_vecPos;
			m_pLight->radius = 120;
			m_pLight->color.r = 255;
			m_pLight->color.g = 0;
			m_pLight->color.b = 0;
			m_pLight->die = gEngfuncs.GetClientTime() + 10.0;
		}
		m_pElight = gEngfuncs.pEfxAPI->CL_AllocElight(0);
		if (m_pElight)
		{
			m_pElight->origin = m_vecPos;
			m_pElight->radius = 90;
			m_pElight->color.r = 255;
			m_pElight->color.g = 0;
			m_pElight->color.b = 0;
			m_pElight->die = gEngfuncs.GetClientTime() + 10.0;
		}
	}

	model_s* pFlare = gEngfuncs.CL_LoadModel("sprites/tpgibflare.spr", &iModel);
	if (!pFlare)
		return;
	m_pFlare = gEngfuncs.pEfxAPI->CL_TempEntAlloc(m_vecPos, pFlare);
	if (!m_pFlare)
		return;

	m_pFlare->flags |= FTENT_SPRANIMATELOOP | FTENT_PERSIST | FTENT_SPRANIMATE;
	m_pFlare->entity.curstate.framerate = 30;
	m_pFlare->entity.curstate.scale = 1.4;
	m_pFlare->entity.curstate.renderamt = 240;
	m_pFlare->entity.curstate.rendermode = kRenderGlow;
	m_pFlare->entity.curstate.renderfx = kRenderFxNoDissipation;
	m_pFlare->entity.curstate.rendercolor.r = 255;
	m_pFlare->entity.curstate.rendercolor.b = 0;
	m_pFlare->entity.curstate.rendercolor.g = 0;
	m_pFlare->entity.curstate.frame = gEngfuncs.pfnRandomFloat(0, 7);
	m_pFlare->frameMax = 8;
	m_pFlare->die = m_pFlare->die + 10.0;

	if (gEngfuncs.CL_LoadModel("sprites/blood.spr", &m_iBlood) && gEngfuncs.CL_LoadModel("sprites/bloodspray.spr", &m_iBloodSpray))
		m_pShockwave = gEngfuncs.CL_LoadModel("sprites/shockwave.spr", &iModel);
}

// orig client 0x61520. Flies at [Glock] TransphasicVelocity, tracing every 0.05-0.1 s (each frame
// at cl_detail above 0); done when its life runs out or it has gone through more than
// TransphasicMaxSolidPunch of solid in one go
bool CTentTransphasic::Update(float flFrametime)
{
	float flCatchUp = 0;
	if (m_flHeadStart > 0)
	{
		flCatchUp = m_flHeadStart < 0 ? 0 : (m_flHeadStart > 1 ? 1 : m_flHeadStart);
		if (flFrametime + flFrametime < flCatchUp)
			flCatchUp = flFrametime + flFrametime;
		m_flHeadStart = m_flHeadStart - flCatchUp;
	}

	const Vector vecVel = m_vecDir * RC2Pref(RC2P_Glock_TransphasicVelocity).f;
	m_vecPos = vecVel * (flCatchUp + flFrametime) + m_vecPos;

	m_flNextTrace = m_flNextTrace - flFrametime;
	if (m_flNextTrace < 0 && (m_vecPos - m_vecTracePos).Length() > 8)
	{
		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
		gEngfuncs.pEventAPI->EV_PushPMStates();
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);

		if (TraceStep(m_vecTracePos, m_vecPos, (m_vecPos - m_vecTracePos).Normalize(), 40, -1))
			m_flSolidDist = (m_vecPos - m_vecTracePos).Length() + m_flSolidDist;
		else
			m_flSolidDist = 0;

		gEngfuncs.pEventAPI->EV_PopPMStates();
		m_vecTracePos = m_vecPos;

		if (g_iClientDetail < 0)
			m_flNextTrace = 0.1;
		else if (g_iClientDetail == 0)
			m_flNextTrace = 0.05;
		else
			m_flNextTrace = 0;
	}

	if (m_pGib)
	{
		m_pGib->entity.origin = m_vecPos;
		m_pGib->die = gEngfuncs.GetClientTime() + 10.0;
	}
	if (m_pFlare)
	{
		m_pFlare->entity.origin = m_vecPos;
		m_pFlare->die = gEngfuncs.GetClientTime() + 10.0;
	}
	if (m_pLight)
	{
		m_pLight->origin = m_vecPos;
		m_pLight->die = gEngfuncs.GetClientTime() + 10.0;
	}
	if (m_pElight)
	{
		m_pElight->origin = m_vecPos;
		m_pElight->die = gEngfuncs.GetClientTime() + 10.0;
	}

	m_flLife = m_flLife - flFrametime;
	m_vecPrevPos = m_vecPos;
	if (!(m_flLife < 0) && m_flSolidDist <= RC2Pref(RC2P_Glock_TransphasicMaxSolidPunch).f)
		return false;

	if (m_pGib)
	{
		m_pGib->die = 0;
		m_pGib = NULL;
	}
	if (m_pFlare)
	{
		m_pFlare->die = 0;
		m_pFlare->flags &= ~FTENT_SPRANIMATELOOP;
		m_pFlare = NULL;
	}
	if (m_pLight)
		m_pLight->die = gEngfuncs.GetClientTime() + 0.01;
	if (m_pElight)
		m_pElight->die = gEngfuncs.GetClientTime() + 0.01;
	return true;
}

// orig client 0x61a60. One stretch of flight: while the start is in solid it steps through at a
// sixtieth of the stretch (149 steps at most; through to the end: true), then traces back for
// the exit wound (a shockwave ring and blood on BSP, decals); then traces on to the end, and on a
// hit makes the entry wound and decals and goes on past it. iDepth is only counted down
bool CTentTransphasic::TraceStep(Vector vecStart, Vector vecEnd, Vector vecDir, int iDepth, int iIgnore)
{
	bool bInSolid = false;
	const int iNextDepth = iDepth - 1;
	const Vector vecDelta = vecEnd - vecStart;
	float flStep = vecDelta.Length() * 0.016666668;
	pmtrace_t tr;

	for (int i = 149;; i--)
	{
		if (gEngfuncs.PM_PointContents(vecStart, NULL) != CONTENTS_SOLID)
		{
			if (!bInSolid)
				goto trace_on;
			break;
		}

		vecStart = vecDir * flStep + vecStart;
		bInSolid = true;
		if ((vecEnd - vecStart).Length() < flStep)
			return true;
		if (i <= 0)
			break;
	}

	{
		// out of the solid: find the face it came out of
		flStep = flStep + 8.0;
		Vector vecBack = vecStart - vecDir * flStep;
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecBack, PM_STUDIO_IGNORE, -1, &tr);
		vecStart = tr.endpos;
		if (0 != tr.startsolid || 0 != tr.allsolid)
			return true;

		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
		if (pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP))
		{
			if (g_iClientDetail >= 0)
			{
				CLTE_AddDiskRing(Vector(tr.endpos) + Vector(tr.plane.normal) * 2.0, m_vecUp, m_vecRight, m_vecForward, 3,
					0, 20, 8, 28, 4, 24, 12, 32,
					Vector(1, 0, 0), Vector(1, 0, 0), 0.3, 0,
					Vector(1, 0, 0), Vector(1, 0, 0), 0.9, 0.1,
					0.333, "sprites/shockwave.spr", 0, 0, 0);
			}
			Vector vecBlood = Vector(tr.endpos) + Vector(tr.plane.normal) * 8.0;
			gEngfuncs.pEfxAPI->R_BloodSprite(vecBlood, 247, m_iBloodSpray, m_iBlood, 5.0);
		}
		Decals(&tr);
	}

trace_on:
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_IGNORE, iIgnore, &tr);
	if (tr.fraction < 1.0)
	{
		EntryWound(&tr);
		Decals(&tr);
		vecStart = Vector(tr.endpos) + vecDir * flStep;
		if (0 != tr.ent)
			TraceStep(vecStart, vecEnd, vecDir, iNextDepth, tr.ent);
		else
			TraceStep(vecStart, vecEnd, vecDir, iNextDepth, -1);
	}
	return false;
}

// orig client 0x621c0. Blood and a scorch where it meets BSP, with r_decals on
void CTentTransphasic::Decals(pmtrace_t* pTrace)
{
	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(pTrace->ent);
	if (!pe || (pe->solid != SOLID_BSP && pe->movetype != MOVETYPE_PUSHSTEP))
		return;
	if (gEngfuncs.pfnGetCvarFloat("r_decals") == 0)
		return;

	char szBlood[] = "{bigblood2";
	int iEnt = gEngfuncs.pEventAPI->EV_IndexFromTrace(pTrace);
	gEngfuncs.pEfxAPI->R_DecalShoot(gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(szBlood)),
		iEnt, 0, pTrace->endpos, 0);

	char szDecal[64];
	sprintf(szDecal, "{smscorch%i", gEngfuncs.pfnRandomLong(1, 2));
	iEnt = gEngfuncs.pEventAPI->EV_IndexFromTrace(pTrace);
	gEngfuncs.pEfxAPI->R_DecalShoot(gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(szDecal)),
		iEnt, 0, pTrace->endpos, 0);
}

// orig client 0x62300. Where it goes into BSP: a shockwave ring round the hit and blood
void CTentTransphasic::EntryWound(pmtrace_t* pTrace)
{
	physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(pTrace->ent);
	if (!pe || (pe->solid != SOLID_BSP && pe->movetype != MOVETYPE_PUSHSTEP))
		return;

	Vector vecAngles;
	VectorAngles(pTrace->plane.normal, vecAngles);
	vecAngles.x = -vecAngles.x;
	Vector vecForward, vecRight, vecUp;
	gEngfuncs.pfnAngleVectors(vecAngles, vecForward, vecRight, vecUp);

	if (g_iClientDetail >= 0)
	{
		CLTE_AddDiskRing(Vector(pTrace->endpos) + Vector(pTrace->plane.normal) * 2.0, vecUp, vecRight, vecForward, 3,
			0, 0, 0, 0, 4, 18, 8, 24,
			Vector(1, 0, 0), Vector(1, 0, 0), 0.3, 0,
			Vector(1, 0, 0), Vector(1, 0, 0), 0.9, 0.1,
			0.25, "sprites/shockwave.spr", 0, 0, 0);
	}

	Vector vecBlood(pTrace->endpos[0] + pTrace->plane.normal[0] * 8.0, pTrace->endpos[1] + pTrace->plane.normal[1] * 8.0,
		pTrace->endpos[2] + pTrace->plane.normal[2] * 8.0);
	gEngfuncs.pEfxAPI->R_BloodSprite(vecBlood, 247, m_iBloodSpray, m_iBlood, 5.0);
}

// orig client 0x62610. A red tube 48 long behind it, fading to the back. orig client 0x62740 is the
// unary Vector::operator- this uses, emitted out of line in the original
void CTentTransphasic::DrawTransparent()
{
	if (g_iClientDetail < 0)
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pShockwave)
		gEngfuncs.pTriAPI->SpriteTexture(m_pShockwave, 0);

	CLTE_DrawRing(m_vecPos, m_vecUp, m_vecRight, -m_vecForward, 6, 6, 48, 0, 0, Vector(1, 0, 0), 0, Vector(1, 0, 0), 0.4);

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}
