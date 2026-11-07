/***
*
*	Rocket Crowbar 2 client: a green beam that leaves a point level and curves up (or down)
*	into a direction (CLTE 0x42).
*
*	No server file in this tree sends type 0x42. The beam is a strip of sprites/laserbeam.spr
*	through eight points: 64 units level along the direction's heading, five 16-unit steps
*	turning to the direction's elevation, then the direction for the given length. It fades
*	over 3 seconds, its texture running along it faster as it fades in.
*
*	The original's functions 0x914e0-0x91c20 (and 0x9b070) of client.dll's run
*	"new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"

#include <cmath>

// c3 needs:
// orig 0x58f90 (c2): a strip of camera-facing quads through iCount points, each with its width,
// colour and alpha, the bound texture running flTexScale per unit of length from flTexOffset.
// name ours
void RC2_DrawTexturedStrip(Vector* pvecPoints, float* pflWidths, Vector* pcolColors, float* pflAlphas, int iCount, float flTexScale, float flTexOffset);

#define CURVEDBEAM_POINTS 8 // name ours

//
// CLTE 0x42: coord x3 the start, short x3 the direction (each /32767), short the length of
// its straight end.
//
class CCurvedBeamTent : public CClientTent // name ours
{
public:
	CCurvedBeamTent();

	bool Think(float flFrametime) override;
	void DrawTransparent() override;
	bool EntityGone(int iEntIndex) override;

	Vector m_vecStart;			  // orig +0x8
	Vector m_vecDir;			  // orig +0x14
	float m_flLength;			  // orig +0x20
	float m_flTimeLeft;			  // orig +0x24
	float m_flLife;				  // orig +0x28: 3
	float m_flTexOffset;		  // orig +0x2c
	Vector m_vecPoints[CURVEDBEAM_POINTS]; // orig +0x30
	Vector m_colPoints[CURVEDBEAM_POINTS]; // orig +0x90
	float m_flAlphas[CURVEDBEAM_POINTS];   // orig +0xf0
	float m_flWidths[CURVEDBEAM_POINTS];   // orig +0x110
	model_s* m_pSprite;			  // orig +0x130: sprites/laserbeam.spr
};

// The factory (the dispatcher's case 0x42; c3_tents.h). pCopy: a copy of that one instead,
// through the implicit copy constructor, which the original has out of line (
// orig client 0x9b070). The dispatcher always passes null.
CClientTent* RC2_NewCurvedBeamTent(const CClientTent* pCopy) // orig client 0x914e0
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CCurvedBeamTent* pTent = pCopy ? new CCurvedBeamTent(*static_cast<const CCurvedBeamTent*>(pCopy)) : new CCurvedBeamTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x914e0): reads the message and lays out the points.
CCurvedBeamTent::CCurvedBeamTent()
{
	m_vecStart.x = READ_COORD();
	m_vecStart.y = READ_COORD();
	m_vecStart.z = READ_COORD();
	m_vecDir.x = READ_SHORT() * 3.051851e-05f;
	m_vecDir.y = READ_SHORT() * 3.051851e-05f;
	m_vecDir.z = READ_SHORT() * 3.051851e-05f;
	m_flLength = READ_SHORT();
	m_vecDir = m_vecDir.Normalize();

	Vector vecLevel = m_vecDir;
	vecLevel.z = 0;
	vecLevel = vecLevel.Normalize();

	// the elevation of the direction, turned through in five steps
	const float flStep = asin(m_vecDir.z) * 0.2;
	float flAngle = flStep;

	m_vecPoints[0] = m_vecStart;
	m_vecPoints[1] = m_vecPoints[0] + vecLevel * 64;
	for (int i = 2; i < 7; i++)
	{
		const Vector vecStepDir = Vector(0, 0, 1) * sin(flAngle) + vecLevel * cos(flAngle);
		m_vecPoints[i] = m_vecPoints[i - 1] + vecStepDir * 16;
		flAngle = flAngle + flStep;
	}
	m_vecPoints[7] = m_vecPoints[6] + m_vecDir * m_flLength;

	for (int i = 0; i < CURVEDBEAM_POINTS; i++)
	{
		m_colPoints[i] = Vector(0.1, 1.0, 0.1);
		m_flAlphas[i] = 0.95;
		m_flWidths[i] = 18;
	}
	m_flAlphas[0] = 0;
	m_flWidths[0] = 12;

	m_flTexOffset = gEngfuncs.pfnRandomFloat(0, 1);
	int iModelIndex;
	m_pSprite = gEngfuncs.CL_LoadModel("sprites/laserbeam.spr", &iModelIndex);
	m_flLife = m_flTimeLeft = 3.0;
}

// Over its life the alpha goes from 0.95 to 0 and the colour's red and blue from 0.1 to 0;
// the start point keeps its own (alpha 0).
bool CCurvedBeamTent::Think(float flFrametime) // orig client 0x91a10
{
	m_flTimeLeft -= flFrametime;
	if (m_flTimeLeft < 0)
		return true;

	float flFrac = m_flTimeLeft / m_flLife;
	if (flFrac < 0)
		flFrac = 0;
	else if (flFrac > 1)
		flFrac = 1;

	const float flAlpha = flFrac * 0.95;
	const float flTint = flFrac * 0.1;
	for (int i = 1; i < CURVEDBEAM_POINTS; i++)
	{
		m_flAlphas[i] = flAlpha;
		m_colPoints[i] = Vector(flTint, 1.0, flTint);
	}

	m_flTexOffset = m_flTexOffset - (flFrac * 5 + 3) * flFrametime;
	while (m_flTexOffset < 0)
		m_flTexOffset = m_flTexOffset + 1.0;
	while (m_flTexOffset > 1)
		m_flTexOffset = m_flTexOffset - 1.0;
	return false;
}

void CCurvedBeamTent::DrawTransparent() // orig client 0x91b90
{
	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pSprite, 0);

	RC2_DrawTexturedStrip(m_vecPoints, m_flWidths, m_colPoints, m_flAlphas, CURVEDBEAM_POINTS, 0.01, m_flTexOffset);

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// It belongs to no entity: nothing kills it early.
bool CCurvedBeamTent::EntityGone(int iEntIndex) // orig client 0x91c20
{
	return false;
}
