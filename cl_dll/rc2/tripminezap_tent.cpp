/***
*
*	Rocket Crowbar 2 client: the tripmine's zap line (CLTE 0x3a).
*
*	Server dlls/tripmine.cpp: short the tripmine, coord x3 the beam's tip, coord x3 where the
*	zap ended. c2's CLTE dispatcher (orig 0x510b0) reads the short, plays debris/beamstart9.wav
*	on that tripmine's charging effects (CLTE 0x39) and ends their charge, then calls the
*	factory here, which reads the six coords. The zap is three beams (red, green and blue) in a
*	small spinning triangle around the line, widening and fading out over 0.7 s; a player who
*	looks toward the zap's start with nothing in between gets a white flash.
*
*	The original's functions 0x98230-0x98a00 of client.dll's run "new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"
#include "pm_defs.h"
#include "pmtrace.h"

// c3 needs:
// orig 0xec590 (view, a changed SDK file): starts a screen colour fade from colFrom/flFrom to
// colTo/flTo over flDuration, unless one of higher iPriority is running. name fork E's
void V_StartColorFade(Vector colFrom, Vector colTo, float flFrom, float flTo, float flDuration, int iPriority);

class CTripmineZapTent : public CClientTent // name ours
{
public:
	CTripmineZapTent();

	bool Think(float flFrametime) override;

	Vector m_vecStart;		  // orig +0x8
	Vector m_vecEnd;		  // orig +0x14
	float m_flLife;			  // orig +0x20: 1 down to 0 over 0.7 s
	Vector m_vecAngles;		  // orig +0x24: the line's, rolled at random
	Vector m_vecAngularVel;	  // orig +0x30: (0, 0, 350-400 either way)
	Vector m_vecOffsets[3];	  // orig +0x3c: the triangle's corners (forward, right, up units)
	BEAM* m_pBeams[3];		  // orig +0x60: red, green, blue
};

CClientTent* RC2_NewTripmineZapTent(const CClientTent* pCopy) // orig client 0x98230
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CTripmineZapTent* pTent = pCopy ? new CTripmineZapTent(*static_cast<const CTripmineZapTent*>(pCopy)) : new CTripmineZapTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

CTripmineZapTent::CTripmineZapTent() // orig client 0x98a00
{
	m_vecStart.x = READ_COORD();
	m_vecStart.y = READ_COORD();
	m_vecStart.z = READ_COORD();
	m_vecEnd.x = READ_COORD();
	m_vecEnd.y = READ_COORD();
	m_vecEnd.z = READ_COORD();
	m_flLife = 1.0;

	const Vector vecLine = (m_vecEnd - m_vecStart).Normalize();
	VectorAngles(vecLine, m_vecAngles);
	m_vecAngles.z = gEngfuncs.pfnRandomFloat(0, 360);
	m_vecAngularVel.y = 0;
	m_vecAngularVel.x = 0;
	const float flSign = gEngfuncs.pfnRandomLong(0, 1) != 0 ? 1.0 : -1.0;
	m_vecAngularVel.z = gEngfuncs.pfnRandomFloat(350, 400) * flSign;

	// an equilateral triangle of radius 1/sqrt(3) square to the line
	m_vecOffsets[0] = Vector(0, 0, 0.57735026);
	m_vecOffsets[1] = Vector(0, 0.5, -0.28867513);
	m_vecOffsets[2] = Vector(0, -0.5, -0.28867513);
	m_pBeams[0] = m_pBeams[1] = m_pBeams[2] = nullptr;

	int iModelIndex;
	if (!gEngfuncs.CL_LoadModel("sprites/zbeam2.spr", &iModelIndex))
		return;
	m_pBeams[0] = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecStart, m_vecEnd, iModelIndex, 0, 3.0, 0, 0.95, 10.0, 0, 0, 1, 0, 0);
	m_pBeams[1] = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecStart, m_vecEnd, iModelIndex, 0, 3.0, 0, 0.95, 10.0, 0, 0, 0, 1, 0);
	m_pBeams[2] = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecStart, m_vecEnd, iModelIndex, 0, 3.0, 0, 0.95, 10.0, 0, 0, 0, 0, 1);

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal)
		return;

	// the flash, for a player looking toward the start with a clear line to it
	Vector vecViewOfs;
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(vecViewOfs);
	Vector vecViewAngles;
	gEngfuncs.GetViewAngles(vecViewAngles);
	Vector vecForward;
	gEngfuncs.pfnAngleVectors(vecViewAngles, vecForward, nullptr, nullptr);
	Vector vecEye = pLocal->origin + vecViewOfs;

	// the original normalizes the direction three times over and takes one component from each
	const Vector vecToStart = (m_vecStart - vecEye).Normalize();
	if (vecToStart.x * vecForward.x + vecToStart.y * vecForward.y + vecToStart.z * vecForward.z <= 0)
		return;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	pmtrace_t tr;
	gEngfuncs.pEventAPI->EV_PlayerTrace(m_vecStart, vecEye, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, &tr);
	if (tr.fraction >= 1.0 || (tr.ent != 0 && gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr) == pLocal->index))
		V_StartColorFade(Vector(255, 255, 255), Vector(255, 255, 255), 200, 0, 0.3, 0);
	gEngfuncs.pEventAPI->EV_PopPMStates();
}

// The triangle spins about the line and spreads (2 to 6 units) as the beams thin and fade.
bool CTripmineZapTent::Think(float flFrametime) // orig client 0x98480
{
	m_vecAngles = m_vecAngles + m_vecAngularVel * flFrametime;
	m_flLife = m_flLife - flFrametime * 1.4285714285714286;

	if (m_flLife <= 0)
	{
		for (int i = 0; i < 3; i++)
		{
			if (m_pBeams[i])
			{
				m_pBeams[i]->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
				m_pBeams[i]->t = 1.0;
				m_pBeams[i]->die = 0.1;
				m_pBeams[i]->endEntity = 0;
				m_pBeams[i]->startEntity = 0;
				m_pBeams[i] = nullptr;
			}
		}
		return true;
	}

	Vector vecForward, vecRight, vecUp;
	gEngfuncs.pfnAngleVectors(m_vecAngles, vecForward, vecRight, vecUp);

	if (m_flLife < 0)
		m_flLife = 0;
	else if (m_flLife > 1)
		m_flLife = 1;

	const float flSpread = m_flLife * 4 + 2;
	const float flBrightness = m_flLife * 0.95;
	const float flWidth = m_flLife + m_flLife + 1.0;

	for (int i = 0; i < 3; i++)
	{
		if (!m_pBeams[i])
			continue;

		const Vector vecUpPart = vecUp * m_vecOffsets[i].z;
		const Vector vecRightPart = vecRight * m_vecOffsets[i].y;
		const Vector vecForwardPart = vecForward * m_vecOffsets[i].x;
		const Vector vecOffset = (vecRightPart + vecForwardPart + vecUpPart) * flSpread;

		m_pBeams[i]->source = vecOffset + m_vecStart;
		m_pBeams[i]->target = vecOffset + m_vecEnd;
		m_pBeams[i]->delta = m_pBeams[i]->target - m_pBeams[i]->source;
		m_pBeams[i]->brightness = flBrightness;
		m_pBeams[i]->width = flWidth;
	}
	return false;
}
