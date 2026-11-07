/***
*
*	Rocket Crowbar 2 client: freezing (CLTE 0x2f, 0x30 and 0x31).
*
*	CLTE 0x2f marks an entity frozen (server freezetimer.cpp, RC2_CLTE_FROZEN: short entity;
*	ended with CLTE 0xfe/0x2f): it sets c2's per-entity frozen flag, and the local player's
*	when it is him; the drawing is c2's. CLTE 0x30 is a freezing ray: a pale blue beam from a
*	point to a hit, with a frost flare (sprites/freezef.spr) and an expanding shockwave disk at
*	the hit; no server file in this tree sends it (VERIFY: who does). CLTE 0x31 is the freeze
*	grenade's trail (server freezegrenade.cpp: short entity): a pale blue puffing smoke trail
*	that spirals as it flies, or a plain smoke beam below cl_detail 0.
*
*	The original's functions 0x7bff0-0x7ddd0 of client.dll's run "new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"

#include <cmath>

//
// CLTE 0x2f: short entity.
//
class CFrozenTent : public CClientTent // name ours
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEntIndex) override;

	int m_iEntIndex; // orig +0x8
};

// The factory (the dispatcher's case 0x2f; c3_tents.h). Constructs inline; the copy too.
CClientTent* RC2_NewFrozenTent(const CClientTent* pCopy) // orig client 0x7bff0
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	if (pCopy)
	{
		CFrozenTent* pTent = new CFrozenTent(*static_cast<const CFrozenTent*>(pCopy));
		g_pClientTents[iSlot] = pTent;
		return pTent;
	}

	CFrozenTent* pTent = new CFrozenTent();
	pTent->m_iEntIndex = READ_SHORT();
	if (pTent->m_iEntIndex != 0)
	{
		if (gEngfuncs.pEventAPI->EV_IsLocal(pTent->m_iEntIndex - 1))
			g_iLocalPlayerFrozen = 1;
		g_bEntityFrozen[pTent->m_iEntIndex] = 1;
	}
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

bool CFrozenTent::Update(float flFrametime) // orig client 0x7c1d0
{
	return m_iEntIndex == 0;
}

bool CFrozenTent::KillEntity(int iEntIndex) // orig client 0x7c1f0
{
	if (iEntIndex != m_iEntIndex)
		return false;

	if (gEngfuncs.pEventAPI->EV_IsLocal(m_iEntIndex - 1))
		g_iLocalPlayerFrozen = 0;
	g_bEntityFrozen[m_iEntIndex] = 0;
	m_iEntIndex = 0;
	return true;
}

//
// CLTE 0x30: coord x, y, z (the start), coord x, y, z (the hit), short x, y, z (the surface
// normal at the hit, each /32767; zero: along the ray), byte size (hundredths), byte 1: no
// frost flare.
//
class CFreezeRayTent : public CClientTent // name ours
{
public:
	CFreezeRayTent();

	bool Update(float flFrametime) override;
	void DrawTransparent() override;

	void FlareVertex(float flRight, float flUp, float s, float t);

	BEAM* m_pBeam;		 // orig +0x8: start to hit
	BEAM* m_pTail;		 // orig +0xc: a short fading piece behind the start
	Vector m_vecStart;	 // orig +0x10
	Vector m_vecEnd;	 // orig +0x1c
	Vector m_vecNormal;	 // orig +0x28
	Vector m_vecRight;	 // orig +0x34
	Vector m_vecUp;		 // orig +0x40
	float m_flSize;		 // orig +0x4c
	float m_flBeamAlpha; // orig +0x50
	float m_flAlpha;	 // orig +0x54: the flare's; the effect ends when it is gone
	bool m_bNoFlare;	 // orig +0x58
	model_s* m_pFlare;	 // orig +0x5c: sprites/freezef.spr
};

// The factory (the dispatcher's case 0x30; c3_tents.h). The copy is inline here.
CClientTent* RC2_NewFreezeRayTent(const CClientTent* pCopy) // orig client 0x7c240
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CFreezeRayTent* pTent = pCopy ? new CFreezeRayTent(*static_cast<const CFreezeRayTent*>(pCopy)) : new CFreezeRayTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

CFreezeRayTent::CFreezeRayTent() // orig client 0x7cba0
{
	m_vecStart.x = READ_COORD();
	m_vecStart.y = READ_COORD();
	m_vecStart.z = READ_COORD();
	m_vecEnd.x = READ_COORD();
	m_vecEnd.y = READ_COORD();
	m_vecEnd.z = READ_COORD();
	m_vecNormal.x = READ_SHORT() * (1.0f / 32767);
	m_vecNormal.y = READ_SHORT() * (1.0f / 32767);
	m_vecNormal.z = READ_SHORT() * (1.0f / 32767);
	m_flSize = READ_BYTE() * 0.01;
	m_bNoFlare = READ_BYTE() != 0;
	m_flBeamAlpha = 1.0;
	m_flAlpha = 1.0;

	if (m_vecNormal.x == 0 && m_vecNormal.y == 0 && m_vecNormal.z == 0)
		m_vecNormal = (m_vecEnd - m_vecStart).Normalize();
	else
		m_vecNormal = m_vecNormal.Normalize();

	Vector vecAngles;
	VectorAngles(m_vecNormal, vecAngles);
	vecAngles.x = -vecAngles.x;
	vecAngles.z = gEngfuncs.pfnRandomFloat(0, 360);
	gEngfuncs.pfnAngleVectors(vecAngles, nullptr, m_vecRight, m_vecUp);

	m_pBeam = nullptr;
	m_pTail = nullptr;

	if (g_iClientDetail >= 0)
	{
		const float flRadius = m_flSize * 4.0 + 12.0;
		const Vector colDisk(0.8, 0.8, 1.0);
		CLTE_AddDiskRing(m_vecEnd + m_vecNormal, m_vecRight, m_vecUp, m_vecNormal, 1,
			0, 0, 0, 0, 1, flRadius, 6, flRadius,
			colDisk, colDisk, 0.3, 0,
			colDisk, colDisk, 0.9, 0.05,
			m_flSize * 0.05 + 0.2, "sprites/shockwave.spr", 0, 0, 0);
	}

	int iModelIndex;
	m_pFlare = gEngfuncs.CL_LoadModel("sprites/freezef.spr", &iModelIndex);
	if (!gEngfuncs.CL_LoadModel("sprites/smoke.spr", &iModelIndex))
		return;

	const float flStartFrame = gEngfuncs.pfnRandomFloat(0, 4);
	const float flFramerate = gEngfuncs.pfnRandomFloat(8, 10);
	const int iStartFrame = (int)flStartFrame;
	m_pBeam = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecStart, m_vecEnd, iModelIndex, 0.0, m_flSize * 3.0, 0.0, 0.95, 0.0, iStartFrame, flFramerate, 0.8, 0.8, 1.0);

	const Vector vecDir = (m_vecEnd - m_vecStart).Normalize();
	Vector vecTail = m_vecStart - vecDir * 6;
	m_pTail = gEngfuncs.pEfxAPI->R_BeamPoints(vecTail, m_vecStart, iModelIndex, 0.0, m_flSize * 3.0, 0.0, 0.95, 0.0, iStartFrame, flFramerate, 0.8, 0.8, 1.0);
	if (m_pTail)
		m_pTail->flags |= FBEAM_SHADEIN;
}

static void KillRayBeam(BEAM*& pBeam) // name ours; inline in the original
{
	if (pBeam)
	{
		pBeam->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
		pBeam->t = 1.0;
		pBeam->die = 0.1;
		pBeam->endEntity = 0;
		pBeam->startEntity = 0;
		pBeam = nullptr;
	}
}

// The flare fades in 0.65 seconds per unit of size, the beams in 0.2.
bool CFreezeRayTent::Update(float flFrametime) // orig client 0x7c460
{
	m_flAlpha = m_flAlpha - (1.0 / (m_flSize * 0.65)) * flFrametime;
	if (m_flAlpha <= 0)
	{
		KillRayBeam(m_pBeam);
		KillRayBeam(m_pTail);
		return true;
	}

	m_flBeamAlpha = m_flBeamAlpha - (1.0 / (m_flSize * 0.5 * 0.4)) * flFrametime;
	if (m_flBeamAlpha <= 0)
	{
		m_flBeamAlpha = 0;
		KillRayBeam(m_pBeam);
		KillRayBeam(m_pTail);
		m_pBeam = nullptr;
	}

	if (m_pBeam)
	{
		m_pBeam->brightness = m_flBeamAlpha * 0.95;
		m_pBeam->width = m_flBeamAlpha * m_flSize * 3.0;
	}
	if (m_pTail)
	{
		m_pTail->brightness = m_flBeamAlpha * 0.95;
		m_pTail->width = m_flBeamAlpha * m_flSize * 3.0;
	}
	return false;
}

// The frost flare: a quad 12 units a unit of size across, a unit off the surface at the hit.
void CFreezeRayTent::DrawTransparent() // orig client 0x7c660
{
	if (m_bNoFlare)
		return;

	const float flHalf = m_flSize * 6.0;
	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pFlare)
		gEngfuncs.pTriAPI->SpriteTexture(m_pFlare, 0);
	gEngfuncs.pTriAPI->Begin(TRI_QUADS);
	gEngfuncs.pTriAPI->Color4f(0.8, 0.8, 1.0, m_flAlpha * 0.95);
	gEngfuncs.pTriAPI->Brightness(m_flAlpha * 0.95);

	// VERIFY: the corners' signs after the first are read off the disassembly's stack slots
	Vector vecCorner = m_vecEnd + m_vecNormal + m_vecRight * flHalf + m_vecUp * flHalf;
	gEngfuncs.pTriAPI->TexCoord2f(0, 0);
	gEngfuncs.pTriAPI->Vertex3f(vecCorner.x, vecCorner.y, vecCorner.z);

	vecCorner = m_vecEnd + m_vecNormal + m_vecRight * -flHalf + m_vecUp * flHalf;
	gEngfuncs.pTriAPI->TexCoord2f(1, 0);
	gEngfuncs.pTriAPI->Vertex3f(vecCorner.x, vecCorner.y, vecCorner.z);

	vecCorner = m_vecEnd + m_vecNormal + m_vecRight * -flHalf + m_vecUp * -flHalf;
	gEngfuncs.pTriAPI->TexCoord2f(1, 1);
	gEngfuncs.pTriAPI->Vertex3f(vecCorner.x, vecCorner.y, vecCorner.z);

	FlareVertex(flHalf, -flHalf, 0, 1);

	gEngfuncs.pTriAPI->End();
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// One vertex of the flare: flRight and flUp from its centre.
void CFreezeRayTent::FlareVertex(float flRight, float flUp, float s, float t) // orig client 0x7ca90
{
	const float x = m_vecEnd.x + m_vecNormal.x + flRight * m_vecRight.x + flUp * m_vecUp.x;
	const float y = m_vecNormal.y + m_vecEnd.y + flRight * m_vecRight.y + flUp * m_vecUp.y;
	const float z = m_vecNormal.z + m_vecEnd.z + flRight * m_vecRight.z + flUp * m_vecUp.z;
	gEngfuncs.pTriAPI->TexCoord2f(s, t);
	gEngfuncs.pTriAPI->Vertex3f(x, y, z);
}

//
// CLTE 0x31: short entity.
//
class CFreezeTrailTent : public CClientTent // name ours
{
public:
	CFreezeTrailTent();

	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEntIndex) override;

	int m_iEntIndex;	   // orig +0x8
	float m_flPuffWait;	   // orig +0xc: time to the next puff
	float m_flSpin;		   // orig +0x10: the roll the puffs spiral by
	float m_flSpinDir;	   // orig +0x14: 1 or -1
	bool m_bVisible;	   // orig +0x18
	Vector m_vecPuff;	   // orig +0x1c: where puffs start: 2 right of the grenade, 1 back
	Vector m_vecPuffVel;   // orig +0x28: 16 along its heading flat, 72 up
	model_s* m_pSprite;	   // orig +0x34: sprites/gradsmoke.spr
	CTentTrail m_Trail;	   // orig +0x38
	BEAM* m_pBeam;		   // orig +0x60: below cl_detail 0 only
};

// The factory (the dispatcher's case 0x31; c3_tents.h). The copy is inline here (through the
// base class's out-of-line copy constructor, orig 0x9b430).
CClientTent* RC2_NewFreezeTrailTent(const CClientTent* pCopy) // orig client 0x7d2b0
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CFreezeTrailTent* pTent = pCopy ? new CFreezeTrailTent(*static_cast<const CFreezeTrailTent*>(pCopy)) : new CFreezeTrailTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

static float FreezeTrailPuffWait() // name ours; inline in the original
{
	if (g_iClientDetail < 0)
		return 0.15;
	if (g_iClientDetail == 0)
		return 0.1;
	return 0.075;
}

// Inline in the factory (orig 0x7d2b0).
CFreezeTrailTent::CFreezeTrailTent()
{
	m_iEntIndex = READ_SHORT();
	m_bVisible = false;
	m_flPuffWait = FreezeTrailPuffWait();
	m_pSprite = nullptr;
	m_pBeam = nullptr;

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_iEntIndex = 0;
		return;
	}

	int iModelIndex;
	if (g_iClientDetail < 0)
	{
		if (!gEngfuncs.CL_LoadModel("sprites/smoke.spr", &iModelIndex))
			return;
		m_pBeam = gEngfuncs.pEfxAPI->R_BeamFollow(m_iEntIndex, iModelIndex, 0.75, 2.0, 0.8, 0.8, 1.0, 0.6);
		if (m_pBeam)
			m_pBeam->flags |= FBEAM_FOREVER;
		return;
	}

	m_flSpin = gEngfuncs.pfnRandomFloat(0, 360);
	m_vecPuff = CLTE_GetOrigin(m_iEntIndex);
	Vector vecAngles(-pEntity->angles.x, pEntity->angles.y, m_flSpin);
	Vector vecForward, vecRight, vecUp;
	gEngfuncs.pfnAngleVectors(vecAngles, vecForward, vecRight, vecUp);
	m_vecPuff = (m_vecPuff + vecRight * 2.0) - vecForward * 1.0;
	m_vecPuffVel = Vector(0, 0, 72) + Vector(vecForward.x, vecForward.y, 0) * 16;
	m_flSpinDir = (gEngfuncs.pfnRandomLong(0, 1) != 0) ? 1 : -1;
	m_Trail.AddPoint(m_vecPuff, m_vecPuffVel, Vector(0, 0, 0), Vector(0.8, 0.8, 1.0), 0.8, 3.0, 1.0666667, 0, nullptr);
	m_pSprite = gEngfuncs.CL_LoadModel("sprites/gradsmoke.spr", &iModelIndex);
}

bool CFreezeTrailTent::Update(float flFrametime) // orig client 0x7d820
{
	if (!m_iEntIndex)
	{
		if (g_iClientDetail < 0)
			return true;
		// the grenade is gone: what's left of the trail fades out
		return !(m_Trail.Update(flFrametime) > 0 && m_bVisible);
	}

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		KillEntity(m_iEntIndex);
		if (g_iClientDetail < 0)
			return true;
		return !(m_Trail.Update(flFrametime) > 0 && m_bVisible);
	}

	m_bVisible = true;
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (pLocal && pEntity->curstate.messagenum < pLocal->curstate.messagenum)
	{
		if (g_iClientDetail >= 0)
			m_Trail.Update(flFrametime);
		m_bVisible = false;
		return false;
	}

	if (g_iClientDetail < 0)
		return false;

	m_vecPuff = CLTE_GetOrigin(m_iEntIndex);
	Vector vecAngles(-pEntity->angles.x, pEntity->angles.y, m_flSpin);
	Vector vecForward, vecRight, vecUp;
	gEngfuncs.pfnAngleVectors(vecAngles, vecForward, vecRight, vecUp);
	m_vecPuff = (m_vecPuff + vecRight * 2.0) - vecForward * 1.0;
	m_vecPuffVel = Vector(vecForward.x * 16, vecForward.y * 16, 0 + 72);
	m_flSpin = m_flSpinDir * flFrametime * 240.0 + m_flSpin;

	m_flPuffWait = m_flPuffWait - flFrametime;
	if (m_flPuffWait <= 0)
	{
		m_flPuffWait = FreezeTrailPuffWait();
		m_Trail.AddPoint(m_vecPuff, m_vecPuffVel, Vector(0, 0, 0), Vector(0.8, 0.8, 1.0), 0.8, 3.0, 0.53333333, 0, nullptr);
	}
	m_Trail.Update(flFrametime);
	return false;
}

void CFreezeTrailTent::DrawTransparent() // orig client 0x7dcb0
{
	if (!m_bVisible || g_iClientDetail < 0)
		return;
	if (!m_Trail.CanDraw())
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pSprite, 0);

	if (m_iEntIndex)
	{
		// led by the grenade itself
		m_Trail.DrawToHead(m_vecPuff, Vector(0.8, 0.8, 1.0), 0.8, 3.0, true);
		gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
		return;
	}

	m_Trail.Draw(true);
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// Lets the low-detail beam go, puffs one last point. Done at once below cl_detail 0; otherwise
// Update lets the trail fade first.
bool CFreezeTrailTent::KillEntity(int iEntIndex) // orig client 0x7ddd0
{
	if (iEntIndex != m_iEntIndex)
		return false;

	if (m_pBeam)
	{
		m_pBeam->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
		m_pBeam->endEntity = 0;
		m_pBeam->startEntity = 0;
		m_pBeam = nullptr;
	}
	if (g_iClientDetail >= 0)
		m_Trail.AddPoint(m_vecPuff, m_vecPuffVel, Vector(0, 0, 0), Vector(0.8, 0.8, 1.0), 0.8, 3.0, 1.0666667, 0, nullptr);
	m_iEntIndex = 0;
	return g_iClientDetail < 0;
}
