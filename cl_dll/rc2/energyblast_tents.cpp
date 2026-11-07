/***
*
*	Rocket Crowbar 2 client: two blast spheres (CLTE 0x38 and 0x3d).
*
*	CLTE 0x38 is a big energy blast: a sphere of sprites/noise2.spr growing at 1200 units a
*	second while it fades from white to pale blue over 1.25 s, a beam torus spreading at 4000,
*	a burst of 40-50 sparks flying out (cl_detail above 0), and a screen shake for the local
*	player once a front 6000 units a second wide passes him. CLTE 0x3d is a small one: a pale
*	blue sphere growing at 4000 units a second for 0.45 s. No sender of either was found in
*	the server.
*
*	The original's functions 0x89a40-0x8a6b8 and 0x8b2f0-0x8ba12 (and the copy constructor
*	0x9acb0) of client.dll's run "new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"

#include <cmath>

// c3 needs:
// orig 0xe9ca0 (view.cpp, a stock file RC2 changed): starts RC2's view shake unless one of a
// higher iPriority is running: stores the three floats (orig .bss 0x10227978, 0x1022682c,
// 0x1022716c) and picks random amplitudes and phases for three axes. What each float means is
// view.cpp's to settle. name ours
void RC2_ViewShake(float fl1, float fl2, float fl3, int iPriority);
// orig 0x509c0 (c2): a CRC2Particles think that tints the particle toward (0.4, 0.4, 1) over
// its remaining life and clamps the colour to 0-1; leaves the default motion to run. name ours
int RC2_ParticleTintBlue(float flFrametime, Vector* pvecOrigin, Vector* pvecVelocity, Vector* pvecAccel, Vector* pcolColor, float* pflAlpha, float* pflSize, float* pflFade, float* pflLife, int iUser);

// A sphere of flRadius around vecOrigin in colColor/flAlpha, in rings from the bottom pole up:
// 8, 10 or 16 by cl_detail. Inline, the same, in both effects' draws in the original.
static void DrawBlastSphere(const Vector& vecOrigin, float flRadius, const Vector& colColor, float flAlpha, model_s* pSprite) // name ours
{
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
	if (pSprite)
		gEngfuncs.pTriAPI->SpriteTexture(pSprite, 0);

	const float flStep = (flRadius + flRadius) / iRings;
	float flZ = flStep - flRadius;
	float flPrevZ = -flRadius;
	float flPrevRing = 0;
	float flRing;
	if (flZ <= -flRadius || flZ >= flRadius)
		flRing = 0;
	else
		flRing = sqrt(flRadius * flRadius - flZ * flZ) / flRadius * flRadius;

	const Vector vecForward(1, 0, 0);
	const Vector vecRight(0, 1, 0);
	const Vector vecUp(0, 0, 1);

	for (int i = 0; i < iRings; i++)
	{
		if (i == 0)
			RC2_DrawEllipsoidCap(vecOrigin, vecForward, vecRight, vecUp, flPrevZ, flZ, flRing, iCircle, colColor, flAlpha, colColor, flAlpha);
		else if (i == iRings - 1)
			RC2_DrawEllipsoidCap(vecOrigin, vecForward, vecRight, vecUp, flZ, flPrevZ, flPrevRing, iCircle, colColor, flAlpha, colColor, flAlpha);
		else
			RC2_DrawEllipsoidBand(vecOrigin, vecForward, vecRight, vecUp, flPrevRing, flRing, flPrevZ, flZ, iCircle, colColor, flAlpha, colColor, flAlpha);

		flPrevRing = flRing;
		flPrevZ = flZ;
		flZ += flStep;
		if (-flRadius < flZ && flZ < flRadius)
			flRing = sqrt(flRadius * flRadius - flZ * flZ) / flRadius * flRadius;
		else
			flRing = 0;
	}
}

//
// CLTE 0x38: coord x, y, z.
//
class CEnergyBlastTent : public CClientTent // name ours
{
public:
	CEnergyBlastTent();

	bool Think(float flFrametime) override;
	void DrawTransparent() override;

	Vector m_vecOrigin;		 // orig +0x8
	float m_flTorusRadius;	 // orig +0x14: 4000 a second
	float m_flShakeRadius;	 // orig +0x18: 6000 a second
	float m_flSphereRadius;	 // orig +0x1c: 1200 a second
	float m_flLife;			 // orig +0x20: 1.25
	float m_flMaxLife;		 // orig +0x24: 1.25
	float m_flAlpha;		 // orig +0x28: 0.1, then 0.95 * the life left
	bool m_bShaken;			 // orig +0x2c
	Vector m_colColor;		 // orig +0x30: white to (0.6, 0.6, 1)
	BEAM* m_pTorus;			 // orig +0x3c
	CRC2Particles m_Sparks;	 // orig +0x40
	model_s* m_pSprite;		 // orig +0x4c: sprites/noise2.spr
	model_s* m_pSparkSprite; // orig +0x50: sprites/glow03.spr
};

// The factory (the dispatcher's case 0x38). pCopy: through the implicit copy constructor,
// out of line in the original (orig client 0x9acb0); the copy shares the sparks' array.
CClientTent* RC2_NewEnergyBlastTent(const CClientTent* pCopy) // orig client 0x89a40
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CEnergyBlastTent* pTent = pCopy ? new CEnergyBlastTent(*static_cast<const CEnergyBlastTent*>(pCopy)) : new CEnergyBlastTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory: reads the message, throws the sparks, makes the torus.
CEnergyBlastTent::CEnergyBlastTent()
{
	m_vecOrigin.x = READ_COORD();
	m_vecOrigin.y = READ_COORD();
	m_vecOrigin.z = READ_COORD();
	m_flMaxLife = m_flLife = 1.25;
	m_colColor = Vector(1, 1, 1);
	m_bShaken = false;
	m_flTorusRadius = 0;
	m_flShakeRadius = 0;
	m_flSphereRadius = 0;
	m_flAlpha = 0.1;
	m_pTorus = nullptr;

	int iModelIndex;
	m_pSprite = gEngfuncs.CL_LoadModel("sprites/noise2.spr", &iModelIndex);
	m_pSparkSprite = gEngfuncs.CL_LoadModel("sprites/glow03.spr", &iModelIndex);

	if (g_iClDetail > 0)
	{
		const int iSparks = gEngfuncs.pfnRandomLong(40, 50);
		for (int i = 0; i < iSparks; i++)
		{
			// a random direction; each spark slows to a stop over its life and tints blue
			const float flZ = gEngfuncs.pfnRandomFloat(-1, 1);
			const float flY = gEngfuncs.pfnRandomFloat(-1, 1);
			const float flX = gEngfuncs.pfnRandomFloat(-1, 1);
			const Vector vecDir = Vector(flX, flY, flZ).Normalize();
			const float flLife = gEngfuncs.pfnRandomFloat(m_flMaxLife * 0.9, m_flMaxLife);
			const float flSpeed = gEngfuncs.pfnRandomFloat(1000, 1100);
			const Vector vecAccel = -vecDir * (flSpeed / flLife);
			const Vector vecVelocity = vecDir * flSpeed;
			m_Sparks.Add(m_vecOrigin, vecVelocity, vecAccel, Vector(1, 1, 1), 0.95, 4, 0.95 / flLife, flLife, 0, RC2_ParticleTintBlue);
		}
	}

	if (gEngfuncs.CL_LoadModel("sprites/laserbeam.spr", &iModelIndex))
	{
		m_pTorus = gEngfuncs.pEfxAPI->R_BeamCirclePoints(TE_BEAMTORUS, m_vecOrigin, m_vecOrigin, iModelIndex, 0, 20, 0.1, 0.8, 3, 0, 0, 1, 1, 1);
		if (m_pTorus)
			m_pTorus->segments = 16;
	}
}

bool CEnergyBlastTent::Think(float flFrametime) // orig client 0x89f30
{
	m_flTorusRadius += flFrametime * 4000;
	m_flShakeRadius += flFrametime * 6000;
	m_flSphereRadius += flFrametime * 1200;
	m_flLife -= flFrametime;
	if (m_flLife <= 0)
	{
		// let the torus go
		if (m_pTorus)
		{
			m_pTorus->brightness = 0.01;
			m_pTorus->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
			m_pTorus->t = 1.0;
			m_pTorus->die = 0.1;
			m_pTorus->endEntity = 0;
			m_pTorus->startEntity = 0;
			m_pTorus = nullptr;
		}
		return true;
	}

	float flFrac = m_flLife / m_flMaxLife;
	if (flFrac < 0)
		flFrac = 0;
	else if (flFrac > 1)
		flFrac = 1;

	m_flAlpha = flFrac * 0.95;
	// VERIFY: the constant is (-0.4, -0.4, 0), read as (0.6, 0.6, 1) - (1, 1, 1)
	m_colColor = (Vector(0.6, 0.6, 1) - Vector(1, 1, 1)) * flFrac + Vector(1, 1, 1);

	if (m_pTorus)
	{
		m_pTorus->source = Vector(m_vecOrigin.x - m_flTorusRadius, m_vecOrigin.y, m_vecOrigin.z);
		m_pTorus->target = Vector(m_vecOrigin.x + m_flTorusRadius, m_vecOrigin.y, m_vecOrigin.z);
		m_pTorus->delta = m_pTorus->target - m_pTorus->source;
		m_pTorus->r = m_colColor.x;
		m_pTorus->g = m_colColor.y;
		m_pTorus->b = m_colColor.z;
		if (flFrac >= 0.15)
			m_pTorus->brightness = 0.8;
		else
			m_pTorus->brightness = flFrac * 6.6666667 * 0.8;
	}

	// shake the local player once the front reaches him
	if (!m_bShaken)
	{
		cl_entity_t* pPlayer = gEngfuncs.GetLocalPlayer();
		if (pPlayer)
		{
			const Vector vecDelta = RC2_GetEntityOrigin(pPlayer->index) - m_vecOrigin;
			if (vecDelta.Length() < m_flShakeRadius)
			{
				m_bShaken = true;
				RC2_ViewShake(3.8, 20, 2.25, 10);
			}
		}
	}

	if (g_iClDetail > 0)
		m_Sparks.Update(flFrametime);
	return false;
}

void CEnergyBlastTent::DrawTransparent() // orig client 0x8a380
{
	if (m_flAlpha <= 0)
		return;

	DrawBlastSphere(m_vecOrigin, m_flSphereRadius, m_colColor, m_flAlpha, m_pSprite);

	if (g_iClDetail > 0)
	{
		if (m_pSparkSprite && m_Sparks.HasLive())
			gEngfuncs.pTriAPI->SpriteTexture(m_pSparkSprite, 0);
		m_Sparks.Draw();
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

//
// CLTE 0x3d: coord x, y, z, then a direction as three shorts of 1/32767 (normalized and kept,
// never used).
//
class CExpandingSphereTent : public CClientTent // name ours
{
public:
	CExpandingSphereTent();

	bool Think(float flFrametime) override;
	void DrawTransparent() override;

	Vector m_vecOrigin;	 // orig +0x8
	Vector m_vecDir;	 // orig +0x14
	float m_flRadius;	 // orig +0x20: 4000 a second
	float m_flLife;		 // orig +0x24: 0.45
	float m_flMaxLife;	 // orig +0x28: 0.45
	float m_flAlpha;	 // orig +0x2c: 0.1, then 0.95 * the life left
	model_s* m_pSprite;	 // orig +0x30: sprites/noise2.spr
};

// The factory (the dispatcher's case 0x3d). The implicit copy constructor is inline here.
CClientTent* RC2_NewExpandingSphereTent(const CClientTent* pCopy) // orig client 0x8b2f0
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CExpandingSphereTent* pTent = pCopy ? new CExpandingSphereTent(*static_cast<const CExpandingSphereTent*>(pCopy)) : new CExpandingSphereTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory: reads the message.
CExpandingSphereTent::CExpandingSphereTent()
{
	m_vecOrigin.x = READ_COORD();
	m_vecOrigin.y = READ_COORD();
	m_vecOrigin.z = READ_COORD();
	m_vecDir.x = READ_SHORT() * (1.0 / 32767);
	m_vecDir.y = READ_SHORT() * (1.0 / 32767);
	m_vecDir.z = READ_SHORT() * (1.0 / 32767);
	m_vecDir = m_vecDir.Normalize();
	m_flRadius = 0;
	m_flAlpha = 0.1;
	m_flMaxLife = 0.45;
	m_flLife = 0.45;

	int iModelIndex;
	m_pSprite = gEngfuncs.CL_LoadModel("sprites/noise2.spr", &iModelIndex);
}

bool CExpandingSphereTent::Think(float flFrametime) // orig client 0x8b650
{
	m_flRadius += flFrametime * 4000;
	m_flLife -= flFrametime;
	if (m_flLife <= 0)
		return true;

	float flFrac = m_flLife / m_flMaxLife;
	if (flFrac < 0)
		flFrac = 0;
	else if (flFrac > 1)
		flFrac = 1;
	m_flAlpha = flFrac * 0.95;
	return false;
}

void CExpandingSphereTent::DrawTransparent() // orig client 0x8b6f0
{
	if (m_flAlpha <= 0)
		return;

	DrawBlastSphere(m_vecOrigin, m_flRadius, Vector(0.75, 0.75, 1.0), m_flAlpha, m_pSprite);
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}
