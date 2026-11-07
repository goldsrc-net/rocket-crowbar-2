/***
*
*	Rocket Crowbar 2 client: a driven forklift's headlights (CLTE 0x3f).
*
*	Sent when a player gets in a forklift (server controlforklift.cpp, RC2_CLTE_FORKLIFT_VIEW)
*	and killed with CLTE 0xfe/0x3f when it blows up. With cl_forkliftlights on, one or two
*	white dynamic lights (by cl_detail, two for the driver himself) sit where traces from the
*	forklift's front hit the world. No draw of its own.
*
*	The original's functions 0x8c5c0-0x8d8d0 (and 0x9ad80) of client.dll's run
*	"new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"
#include "pm_defs.h"

//
// CLTE 0x3f: short forklift, byte the driver's index (bit 0x80: a flag, see the constructor).
//
class CForkliftLightsTent : public CClientTent // name ours
{
public:
	CForkliftLightsTent();

	bool Update(float flFrametime) override;
	bool KillEntity(int iEntIndex) override;

	int m_iEntIndex;		  // orig +0x8: the forklift
	int m_iDriver;			  // orig +0xc: the driver's entity index (the byte's low 7 bits)
	bool m_bFlag;			  // orig +0x10: the byte's bit 0x80. VERIFY: what the server means by it (controlforklift.cpp only ever sends an index)
	bool m_bVisible;		  // orig +0x11: the forklift is in this frame's packet
	dlight_t* m_pDlight[2];	  // orig +0x14
	dlight_t* m_pElight[2];	  // orig +0x1c
	Vector m_vecLight[2];	  // orig +0x24: where each light sits (8192,8192,8192 when its trace hit nothing)
	int m_iLights;			  // orig +0x3c: 0, 1 or 2
};

// The factory (the dispatcher's case 0x3f; c3_tents.h). pCopy: a copy of that one instead,
// through the implicit copy constructor, which the original has out of line (
// orig client 0x9ad80). The dispatcher always passes null.
CClientTent* RC2_NewForkliftLightsTent(const CClientTent* pCopy) // orig client 0x8c5c0
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CForkliftLightsTent* pTent = pCopy ? new CForkliftLightsTent(*static_cast<const CForkliftLightsTent*>(pCopy)) : new CForkliftLightsTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x8c5c0): reads the message and makes the lights.
CForkliftLightsTent::CForkliftLightsTent()
{
	m_iEntIndex = READ_SHORT();
	const int iDriver = READ_BYTE();
	m_bFlag = ((iDriver >> 7) & 1) != 0;
	m_iDriver = iDriver & 0x7f; // VERIFY: the original masks only the low byte (and $0x7f, %al); READ_BYTE's upper bytes are 0 anyway
	m_pDlight[1] = nullptr;
	m_pDlight[0] = nullptr;
	m_pElight[1] = nullptr;
	m_pElight[0] = nullptr;
	m_vecLight[1] = m_vecLight[0] = Vector(0, 0, 0);
	m_iLights = 0;

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal)
	{
		m_iEntIndex = 0;
		return;
	}

	cl_entity_t* pForklift = GetEntity(m_iEntIndex);
	if (!pForklift)
	{
		m_iEntIndex = 0;
		return;
	}

	m_vecLight[1] = pForklift->origin;
	m_vecLight[0] = pForklift->origin;

	if (cl_forkliftlights && cl_forkliftlights->value != 0)
	{
		if (m_bFlag)
			m_iLights = g_iClientDetail > 0;
		else if (pLocal->index != m_iDriver)
			m_iLights = g_iClientDetail > 0;
		else if (g_iClientDetail >= 0)
			m_iLights = (g_iClientDetail != 0) + 1;
		else
			m_iLights = 0;
	}
	else
		m_iLights = 0;

	for (int i = 0; i < m_iLights; i++)
	{
		m_pDlight[i] = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		if (m_pDlight[i])
		{
			m_pDlight[i]->origin = m_vecLight[i];
			m_pDlight[i]->radius = 32;
			m_pDlight[i]->color.r = 255;
			m_pDlight[i]->color.g = 255;
			m_pDlight[i]->color.b = 255;
			m_pDlight[i]->die = gEngfuncs.GetClientTime() + 10.0;
		}

		m_pElight[i] = gEngfuncs.pEfxAPI->CL_AllocElight(0);
		if (m_pElight[i])
		{
			m_pElight[i]->origin = m_vecLight[i];
			m_pElight[i]->radius = 18;
			m_pElight[i]->color.r = 255;
			m_pElight[i]->color.g = 255;
			m_pElight[i]->color.b = 255;
			m_pElight[i]->die = gEngfuncs.GetClientTime() + 10.0;
		}
	}
}

// Where a headlight lands: a trace 128 units along vecDir (normalized) from vecSrc, 2 units off
// the surface it hits, (8192,8192,8192) when it hits nothing. Inline at each of its three uses in
// the original. name ours
static Vector ForkliftLightSpot(const Vector& vecSrc, const Vector& vecDir, pmtrace_t* ptr)
{
	const Vector vecEnd = vecDir.Normalize() * 128 + vecSrc;
	gEngfuncs.pEventAPI->EV_PlayerTrace((float*)&vecSrc, (float*)&vecEnd, PM_STUDIO_IGNORE | PM_GLASS_IGNORE, -1, ptr);
	if (ptr->fraction < 1.0)
		return Vector(ptr->plane.normal) * 2 + Vector(ptr->endpos);
	return Vector(8192, 8192, 8192);
}

// Each frame the lights move to where the forklift's front points: two lights aim 0.7 to either
// side, one aims straight ahead, all a little down; their die time is pushed 10 seconds on.
bool CForkliftLightsTent::Update(float flFrametime) // orig client 0x8ca70
{
	if (!m_iEntIndex)
		return true;

	cl_entity_t* pForklift = GetEntity(m_iEntIndex);
	if (!pForklift)
	{
		KillEntity(m_iEntIndex);
		return true;
	}

	m_bVisible = true;
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (pLocal && pLocal->curstate.messagenum > pForklift->curstate.messagenum)
	{
		m_bVisible = false;
		return false;
	}

	if (m_iLights < 1)
		return false;

	Vector vecAngles = pForklift->angles;
	vecAngles.x = -vecAngles.x;
	Vector vecForward, vecRight, vecUp;
	gEngfuncs.pfnAngleVectors(vecAngles, vecForward, vecRight, vecUp);

	Vector vecSrc = CLTE_GetOrigin(m_iEntIndex);
	vecSrc.z += 10.0;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	pmtrace_t tr;
	if (m_iLights == 2)
	{
		m_vecLight[0] = ForkliftLightSpot(vecSrc, vecRight * 0.7 + (vecForward * 4 - vecUp * 0.75), &tr);
		m_vecLight[1] = ForkliftLightSpot(vecSrc, vecForward * 4 - vecUp * 0.75 - vecRight * 0.7, &tr);
	}
	else
	{
		m_vecLight[0] = ForkliftLightSpot(vecSrc, vecForward * 4 - vecUp * 0.75, &tr);
		m_vecLight[1] = Vector(8192, 8192, 8192);
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();

	for (int i = 0; i < 2; i++)
	{
		if (m_pDlight[i])
		{
			m_pDlight[i]->die = gEngfuncs.GetClientTime() + 10.0;
			m_pDlight[i]->origin = m_vecLight[i];
		}
		if (m_pElight[i])
		{
			m_pElight[i]->die = gEngfuncs.GetClientTime() + 10.0;
			m_pElight[i]->origin = m_vecLight[i];
		}
	}
	return false;
}

// The forklift blew up (CLTE 0xfe/0x3f) or is gone: the lights die in a hundredth of a second.
bool CForkliftLightsTent::KillEntity(int iEntIndex) // orig client 0x8d8d0
{
	if (iEntIndex != m_iEntIndex)
		return false;

	for (int i = 0; i < 2; i++)
	{
		// Original bug, kept: the dlight is tested but the elight is the one killed and
		// cleared, so the dlights live on to their die time. A dlight without an elight would
		// write through a null pointer, but both are made together and the engine's
		// CL_AllocDlight/CL_AllocElight never return NULL.
		if (m_pDlight[i])
		{
			m_pElight[i]->die = gEngfuncs.GetClientTime() + 0.01;
			m_pElight[i] = nullptr;
		}
		if (m_pElight[i])
		{
			m_pElight[i]->die = gEngfuncs.GetClientTime() + 0.01;
			m_pElight[i] = nullptr;
		}
	}
	m_iEntIndex = 0;
	return true;
}
