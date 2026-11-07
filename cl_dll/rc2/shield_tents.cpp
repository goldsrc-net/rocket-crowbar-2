/***
*
*	Rocket Crowbar 2 client: the shield's effects (CLTE 0x25 and 0x26).
*
*	CLTE 0x25 is a player's shield, an ellipsoid of sprites/noise1.spr around him with a bright
*	band (sprites/lgtning.spr) sweeping up and down it; its class, factory and think are c2's
*	(c3_needs.h), its draw and hit flash are here. CLTE 0x26 is a hit on the shield (server
*	shield.cpp, CLTE_SHIELDHIT): a ring of sprites/shockwave.spr spreading on the shield's
*	surface where the hit landed, which also flashes the shield.
*
*	The original's functions 0x71ad0-0x72ac0 (and 0x99d60) of client.dll's run
*	"new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"

#include <cmath>

// The radius of an ellipsoid's ring at height flZ (flRadius across, flHeight up): 0 outside
// it. Inline in the original at each use. name ours
static float ShieldRingRadius(float flZ, float flRadius, float flHeight)
{
	if (flZ <= -flRadius || flZ >= flRadius)
		return 0;
	return sqrt(flRadius * flRadius - flZ * flZ) / flRadius * flHeight;
}

// The shield: an ellipsoid of rings in pale blue whose brightness is the hit flash, from the
// bottom pole up (6, 8 or 10 rings by cl_detail, each ring a little ragged), and the bright band.
void CShieldTent::DrawTransparent() // orig client 0x71ad0
{
	if (!m_bVisible)
		return;
	if (m_flFlash <= 0 && !m_bBand)
		return;

	int iCircle, iRings;
	if (g_iClDetail >= 1)
	{
		iCircle = 4;
		iRings = 10;
	}
	else if (g_iClDetail >= 0)
	{
		iCircle = 3;
		iRings = 8;
	}
	else
	{
		iCircle = 1;
		iRings = 6;
	}

	const float flStep = (m_flRadius + m_flRadius) / iRings;
	float flZ = flStep - m_flRadius;
	float flPrevZ = -m_flRadius;
	float flRing = ShieldRingRadius(flZ, m_flRadius, m_flHeight) + gEngfuncs.pfnRandomFloat(-1, 1);
	float flPrevRing = 0;
	const Vector colShield(0.25, 0.25, 1.0);

	float flAlpha = m_flFlash;
	if (flAlpha > 0.1)
		flAlpha += gEngfuncs.pfnRandomFloat(-0.09, 0.05);
	if (flAlpha < 0)
		flAlpha = 0;
	else if (flAlpha > 1)
		flAlpha = 1;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);

	if (flAlpha > 0)
	{
		if (m_pSprite)
			gEngfuncs.pTriAPI->SpriteTexture(m_pSprite, 0);

		for (int i = 0; i < iRings; i++)
		{
			if (i == 0)
				RC2_DrawEllipsoidCap(m_vecOrigin, m_vecForward, m_vecRight, m_vecUp, flPrevZ, flZ, flRing, iCircle, colShield, flAlpha, colShield, flAlpha);
			else if (i == iRings - 1)
				RC2_DrawEllipsoidCap(m_vecOrigin, m_vecForward, m_vecRight, m_vecUp, flZ, flPrevZ, flPrevRing, iCircle, colShield, flAlpha, colShield, flAlpha);
			else
				RC2_DrawEllipsoidBand(m_vecOrigin, m_vecForward, m_vecRight, m_vecUp, flPrevRing, flRing, flPrevZ, flZ, iCircle, colShield, flAlpha, colShield, flAlpha);

			flPrevRing = flRing;
			flPrevZ = flZ;
			flZ += flStep;
			flRing = ShieldRingRadius(flZ, m_flRadius, m_flHeight) + gEngfuncs.pfnRandomFloat(-1, 1);
		}
	}

	if (m_bBand)
	{
		// VERIFY: the original tests the noise sprite (+0x1c) but binds the band's (+0x20)
		if (m_pSprite)
			gEngfuncs.pTriAPI->SpriteTexture(m_pBandSprite, (int)m_flBandFrame % 2);

		const float flTop = m_flBandHalf + m_flBandZ;
		const float flBottom = m_flBandZ - m_flBandHalf;
		const float flTopRing = ShieldRingRadius(flTop, m_flRadius, m_flHeight) + 1.0;
		const float flBottomRing = ShieldRingRadius(flBottom, m_flRadius, m_flHeight) + 1.0;
		const Vector colBand(0.8, 0.8, 1.0);

		// the band fades toward the poles
		float flBandAlpha = 0.8 - pow(fabs(m_flBandZ) / m_flRadius, 6.0) * 0.8;
		if (flBandAlpha < 0)
			flBandAlpha = 0;
		else if (flBandAlpha > 1)
			flBandAlpha = 1;

		RC2_DrawEllipsoidBand(m_vecOrigin, m_vecForward, m_vecRight, m_vecUp, flBottomRing, flTopRing, flBottom, flTop, iCircle, colBand, flBandAlpha, colBand, flBandAlpha);
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// A hit on this player's shield brightens it: more for a stronger hit, up to 0.8.
void CShieldTent::Flash(int iEntIndex, int iStrength) // orig client 0x727a0
{
	if (iEntIndex != m_iEntIndex)
		return;

	float flFlash = (iStrength + 1) * 0.0375 + gEngfuncs.pfnRandomFloat(-0.05, 0.05) + m_flFlash + 0.3;
	m_flFlash = flFlash;
	if (flFlash < 0)
	{
		m_flFlash = 0;
		return;
	}
	if (flFlash > 0.8)
		flFlash = 0.8;
	m_flFlash = flFlash;
}

//
// CLTE 0x26: byte player, byte yaw (0-255 around), byte height (127 the middle, 40 units
// either way), byte effect (low 4 bits the strength, bit 0x10: no ring, only the flash).
//
#define SHIELDHIT_NORING 0x10 // name ours

class CShieldHitTent : public CClientTent // name ours
{
public:
	CShieldHitTent();

	bool Think(float flFrametime) override;
	void DrawTransparent() override;
	bool EntityGone(int iEntIndex) override;

	Vector RingPoint(float flYaw, float flZ);

	int m_iEntIndex;	 // orig +0x8
	float m_flLife;		 // orig +0xc
	float m_flMaxLife;	 // orig +0x10
	float m_flRadius;	 // orig +0x14: the shield's: 40, 30 ducked
	float m_flHeight;	 // orig +0x18: 20, 26 ducked
	model_s* m_pSprite;	 // orig +0x1c: sprites/shockwave.spr
	int m_iFlags;		 // orig +0x20: the effect byte's high bits
	int m_iStrength;	 // orig +0x24: the effect byte's low 4 bits
	Vector m_vecOrigin;	 // orig +0x28: the player's
	Vector m_vecHit;	 // orig +0x34: the hit, from the player's origin
	Vector m_vecRight;	 // orig +0x40
	Vector m_vecUp;		 // orig +0x4c
	Vector m_vecNormal;	 // orig +0x58: the shield's surface normal at the hit
	Vector m_vecAngles;	 // orig +0x64: m_vecNormal's, rolled at random each frame
	float m_flInner;	 // orig +0x70: the ring's inner radius
	float m_flOuter;	 // orig +0x74: its outer radius
	float m_flMaxInner;	 // orig +0x78
	float m_flMaxOuter;	 // orig +0x7c
	float m_flInnerAlpha; // orig +0x80
	float m_flOuterAlpha; // orig +0x84
};

// The factory (the dispatcher's case 0x26; c3_tents.h). pCopy: a copy of that one instead,
// through the implicit copy constructor, which the original has out of line
// (orig client 0x99d60). The dispatcher always passes null.
CClientTent* RC2_NewShieldHitTent(const CClientTent* pCopy) // orig client 0x721b0
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CShieldHitTent* pTent = pCopy ? new CShieldHitTent(*static_cast<const CShieldHitTent*>(pCopy)) : new CShieldHitTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x721b0): reads the message.
CShieldHitTent::CShieldHitTent()
{
	m_iEntIndex = READ_BYTE();
	const int iYaw = READ_BYTE();
	const int iHeight = READ_BYTE();
	const int iEffect = READ_BYTE();

	m_flLife = m_flMaxLife = gEngfuncs.pfnRandomFloat(0.25, 0.3);
	m_iFlags = iEffect & 0xfffffff0;
	m_pSprite = nullptr;
	m_iStrength = iEffect & 0xf;
	m_flOuter = 0;
	m_flInner = 0;
	m_flOuterAlpha = 0;
	m_flInnerAlpha = 0;

	const float flSize = (m_iStrength + 1) + (m_iStrength + 1);
	m_flMaxInner = flSize;
	m_flMaxOuter = flSize + flSize;
	m_flMaxInner += gEngfuncs.pfnRandomFloat(-1, 0);
	m_flMaxOuter += gEngfuncs.pfnRandomFloat(0, 1);

	cl_entity_t* pEntity = RC2_GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_iEntIndex = 0;
		return;
	}

	if (pEntity->curstate.usehull == 1)
	{
		m_flRadius = 30;
		m_flHeight = 26;
	}
	else
	{
		m_flRadius = 40;
		m_flHeight = 20;
	}

	if ((m_iFlags & SHIELDHIT_NORING) == 0)
	{
		const float flYaw = iYaw * 0.024639942; // 2 pi / 255
		const float flZ = (iHeight - 127) * 0.31496063; // 40 / 127
		m_vecHit = RingPoint(flYaw, flZ);

		// the normal: toward the same point on a shell a little bigger, at the height the
		// surface's slope moves it to
		const float flRadius = m_flRadius;
		const float flHeight = m_flHeight;
		const float flSign = flZ > 0 ? 1.0 : -1.0;
		const float flSaveRadius = m_flRadius;
		const float flSaveHeight = m_flHeight;
		m_flRadius = flRadius / flHeight + m_flRadius;
		m_flHeight = m_flHeight + 1.0;
		const Vector vecOuter = RingPoint(flYaw, (flRadius / flHeight) * (1.0 - sqrt(flRadius * flRadius - flZ * flZ) / flRadius) * flSign + flZ);
		m_vecNormal = vecOuter - m_vecHit;
		m_vecNormal = m_vecNormal.Normalize();
		VectorAngles(m_vecNormal, m_vecAngles);
		m_vecAngles.z = gEngfuncs.pfnRandomFloat(0, 360);
		m_vecAngles.x = -m_vecAngles.x;
		gEngfuncs.pfnAngleVectors(m_vecAngles, nullptr, m_vecRight, m_vecUp);
		m_flHeight = flSaveHeight;
		m_flRadius = flSaveRadius;
	}
	else
	{
		m_vecAngles = m_vecNormal = m_vecUp = m_vecRight = m_vecHit = Vector(0, 0, 0);
	}

	// flash the player's shield
	for (int i = 0; i < MAX_CLIENT_TENTS; i++)
	{
		if (g_pClientTents[i] && g_pClientTents[i]->m_iType == 0x25)
			static_cast<CShieldTent*>(g_pClientTents[i])->Flash(m_iEntIndex, m_iStrength);
	}

	int iModelIndex;
	m_pSprite = gEngfuncs.CL_LoadModel("sprites/shockwave.spr", &iModelIndex);
}

// The point on the shield at flYaw around and flZ up, from its centre.
Vector CShieldHitTent::RingPoint(float flYaw, float flZ) // orig client 0x72840
{
	const float flRing = ShieldRingRadius(flZ, m_flRadius, m_flHeight);
	return Vector(flRing * cos(flYaw), flRing * sin(flYaw), flZ);
}

// The ring spreads to its full size and fades over its quarter second, following the player.
bool CShieldHitTent::Think(float flFrametime) // orig client 0x728e0
{
	if (!m_iEntIndex)
		return true;

	if ((m_iFlags & SHIELDHIT_NORING) != 0)
	{
		EntityGone(m_iEntIndex);
		m_iEntIndex = 0;
		return true;
	}

	if (!RC2_GetEntity(m_iEntIndex))
	{
		EntityGone(m_iEntIndex);
		m_iEntIndex = 0;
		return true;
	}

	m_flLife -= flFrametime;
	if (m_flLife < 0)
	{
		EntityGone(m_iEntIndex);
		m_iEntIndex = 0;
		return true;
	}

	m_vecAngles.z = gEngfuncs.pfnRandomFloat(0, 360);
	gEngfuncs.pfnAngleVectors(m_vecAngles, nullptr, m_vecRight, m_vecUp);
	m_vecOrigin = RC2_GetEntityOrigin(m_iEntIndex);

	float flFrac = m_flLife / m_flMaxLife;
	if (flFrac < 0)
		flFrac = 0;
	else if (flFrac > 1)
		flFrac = 1;

	m_flInner = (1.0 - flFrac) * m_flMaxInner;
	m_flOuter = (1.0 - flFrac) * m_flMaxOuter;
	m_flInnerAlpha = flFrac * 0.3;
	m_flOuterAlpha = flFrac * 0.95;
	return false;
}

bool CShieldHitTent::EntityGone(int iEntIndex) // orig client 0x72a90
{
	if (m_iEntIndex == iEntIndex)
	{
		m_iEntIndex = 0;
		return true;
	}
	return false;
}

// The ring: flat on the shield at the hit, fading out toward its outer edge.
void CShieldHitTent::DrawTransparent() // orig client 0x72ac0
{
	if ((m_iFlags & SHIELDHIT_NORING) != 0)
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pSprite, 0);

	const Vector colRing(0.9, 0.9, 1.0);
	RC2_DrawEllipsoidBand(m_vecOrigin + m_vecHit, m_vecRight, m_vecUp, m_vecNormal, m_flInner, m_flOuter, 0, 0, 1, colRing, m_flInnerAlpha, colRing, m_flOuterAlpha);

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}
