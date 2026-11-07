/***
*
*	Rocket Crowbar 2 client: an expanding blast sphere (CLTE 0x2b).
*
*	coord x, y, z. For one second a sphere of sprites/noise2.spr (orange) grows from the point
*	at 1000 units a second and fades, a laserbeam ring stretches across it at 4000 units a
*	second, and the local player's view wobbles once when the 6000-units-a-second shock front
*	passes him. No server file in this tree sends type 0x2b. VERIFY: who sends it (by its
*	size and look, likely the nuclear missile's blast).
*
*	The original's functions 0x772d0-0x778c0 of client.dll's run "new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"

#include <cmath>

// c3 needs:
// orig 0xe9ca0 (view.cpp, changed stock code): starts a view wobble of flAmplitude, flSpeed
// and flTime (RC2's own, kept in view.cpp's globals 0x10227978, 0x1022682c, 0x1022716c with
// random phases for each axis) unless one of higher iPriority is running. Names ours.
// VERIFY: what each float means is read off the setter only.
void RC2_ViewShake(float flAmplitude, float flSpeed, float flTime, int iPriority);

class CBlastSphereTent : public CClientTent // name ours
{
public:
	CBlastSphereTent();

	bool Think(float flFrametime) override;
	void DrawTransparent() override;

	Vector m_vecOrigin;	 // orig +0x8
	float m_flRing;		 // orig +0x14: the ring beam's half length
	float m_flShock;	 // orig +0x18: the shock front's radius (the view wobble)
	float m_flRadius;	 // orig +0x1c: the sphere's
	float m_flLife;		 // orig +0x20
	float m_flMaxLife;	 // orig +0x24
	float m_flAlpha;	 // orig +0x28
	bool m_bShaken;		 // orig +0x2c
	BEAM* m_pRing;		 // orig +0x30
	model_s* m_pSprite;	 // orig +0x34: sprites/noise2.spr
};

// The factory (the dispatcher's case 0x2b; c3_tents.h). The copy is inline here.
CClientTent* RC2_NewBlastSphereTent(const CClientTent* pCopy) // orig client 0x772d0
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CBlastSphereTent* pTent = pCopy ? new CBlastSphereTent(*static_cast<const CBlastSphereTent*>(pCopy)) : new CBlastSphereTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x772d0).
CBlastSphereTent::CBlastSphereTent()
{
	m_vecOrigin.x = READ_COORD();
	m_vecOrigin.y = READ_COORD();
	m_vecOrigin.z = READ_COORD();
	m_bShaken = false;
	m_flRing = 0;
	m_flShock = 0;
	m_flRadius = 0;
	m_flAlpha = 0.1;
	m_pRing = nullptr;
	m_flMaxLife = 1.0;
	m_flLife = 1.0;

	int iModelIndex;
	m_pSprite = gEngfuncs.CL_LoadModel("sprites/noise2.spr", &iModelIndex);
	int iBeamIndex;
	if (gEngfuncs.CL_LoadModel("sprites/laserbeam.spr", &iBeamIndex))
	{
		m_pRing = gEngfuncs.pEfxAPI->R_BeamCirclePoints(TE_BEAMRING, m_vecOrigin, m_vecOrigin, iBeamIndex, 0.0, 20.0, 0.1, 0.8, 3.0, 0, 0.0, 1.0, 0.8, 0.6);
		if (m_pRing)
			m_pRing->segments = 16;
	}
}

bool CBlastSphereTent::Think(float flFrametime) // orig client 0x775d0
{
	m_flRing = flFrametime * 4000 + m_flRing;
	m_flShock = flFrametime * 6000 + m_flShock;
	m_flRadius = flFrametime * 1000 + m_flRadius;
	m_flLife = m_flLife - flFrametime;
	if (m_flLife <= 0)
	{
		if (m_pRing)
		{
			m_pRing->brightness = 0.01;
			m_pRing->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
			m_pRing->t = 1.0;
			m_pRing->die = 0.1;
			m_pRing->endEntity = 0;
			m_pRing->startEntity = 0;
			m_pRing = nullptr;
		}
		return true;
	}

	float flFrac = m_flLife / m_flMaxLife;
	if (flFrac < 0)
		flFrac = 0;
	else if (flFrac > 1)
		flFrac = 1;
	m_flAlpha = flFrac * 0.8;

	if (m_pRing)
	{
		m_pRing->source = Vector(-m_flRing + m_vecOrigin.x, m_vecOrigin.y, m_vecOrigin.z);
		m_pRing->target = Vector(m_flRing + m_vecOrigin.x, m_vecOrigin.y, m_vecOrigin.z);
		m_pRing->delta = Vector(m_pRing->target) - Vector(m_pRing->source);
	}

	if (!m_bShaken)
	{
		cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
		if (pLocal)
		{
			const Vector vecDelta = RC2_GetEntityOrigin(pLocal->index) - m_vecOrigin;
			if (vecDelta.Length() < m_flShock)
			{
				m_bShaken = true;
				RC2_ViewShake(3.5, 20, 2, 5);
			}
		}
	}
	return false;
}

// The sphere: rings of the noise sprite from the bottom pole up, 8, 10 or 16 by cl_detail,
// orange, in the world's axes.
void CBlastSphereTent::DrawTransparent() // orig client 0x778c0
{
	if (m_flAlpha <= 0)
		return;

	int iCircle, iRings;
	if (g_iClDetail >= 1)
	{
		iCircle = 3;
		iRings = 16;
	}
	else if (g_iClDetail >= 0)
	{
		iCircle = 1;
		iRings = 10;
	}
	else
	{
		iCircle = 0;
		iRings = 8;
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pSprite, 0);

	const float flR = m_flRadius;
	const float flStep = (flR + flR) / iRings;
	float flZ = flStep - flR;
	float flPrevZ = -flR;
	float flRing = (flZ <= flPrevZ || flZ >= flR) ? 0 : sqrt(flR * flR - flZ * flZ) / flR * flR;
	float flPrevRing = 0;
	const Vector colSphere(1.0, 0.8, 0.6);
	const Vector vecForward(1, 0, 0), vecRight(0, 1, 0), vecUp(0, 0, 1);

	for (int i = 0; i < iRings; i++)
	{
		if (i == 0)
			RC2_DrawEllipsoidCap(m_vecOrigin, vecForward, vecRight, vecUp, flPrevZ, flZ, flRing, iCircle, colSphere, m_flAlpha, colSphere, m_flAlpha);
		else if (i == iRings - 1)
			RC2_DrawEllipsoidCap(m_vecOrigin, vecForward, vecRight, vecUp, flZ, flPrevZ, flPrevRing, iCircle, colSphere, m_flAlpha, colSphere, m_flAlpha);
		else
			RC2_DrawEllipsoidBand(m_vecOrigin, vecForward, vecRight, vecUp, flPrevRing, flRing, flPrevZ, flZ, iCircle, colSphere, m_flAlpha, colSphere, m_flAlpha);

		flPrevRing = flRing;
		flPrevZ = flZ;
		flZ += flStep;
		flRing = (flZ <= -m_flRadius || flZ >= m_flRadius) ? 0 : sqrt(m_flRadius * m_flRadius - flZ * flZ) / m_flRadius * m_flRadius;
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}
