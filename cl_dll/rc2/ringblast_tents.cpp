/***
*
*	Rocket Crowbar 2 client: a blast of rings thrown along a direction (CLTE 0x43).
*
*	No server file in this tree sends type 0x43. A pale disc of sprites/heflare4.spr and the
*	three rings stacked behind it shoot out along the direction, growing to 280 units across
*	over 0.3 seconds as they fade.
*
*	The original's functions 0x91c40-0x92780 (and 0x9b1a0) of client.dll's run
*	"new: cdll_int .. com_weapons". 0x92200-0x922e0 are SDK inlines the original compiled out
*	of line here (see the end of the file).
*
****/

#include "c3_needs.h"
#include "c3_tents.h"

#include <cmath>

#define RINGBLAST_RINGS 4		 // name ours
#define RINGBLAST_MAX_POINTS 12 // name ours

//
// CLTE 0x43: coord x3 the origin, short x3 the direction (each /32767).
//
class CRingBlastTent : public CClientTent // name ours
{
public:
	CRingBlastTent();

	bool Update(float flFrametime) override;
	void DrawTransparent() override;

	float m_flTimeLeft;		// orig +0x8
	float m_flLife;			// orig +0xc: 0.3
	Vector m_vecOrigin;		// orig +0x10
	Vector m_vecDir;		// orig +0x1c
	Vector m_vecAxis1;		// orig +0x28: across the direction
	Vector m_vecAxis2;		// orig +0x34: across both
	Vector m_vecCircle[RINGBLAST_MAX_POINTS]; // orig +0x40: (cos, sin, 0) of each point, from a random start
	int m_iPoints;			// orig +0xd0: 8, 10 or 12 by cl_detail
	Vector m_vecRing[RINGBLAST_RINGS][RINGBLAST_MAX_POINTS];	  // orig +0xd4
	Vector2D m_vecRingUV[RINGBLAST_RINGS][RINGBLAST_MAX_POINTS]; // orig +0x314: texture coordinates
	Vector m_vecCenter;		// orig +0x494: the disc's centre
	float m_flAlpha;		// orig +0x4a0
	model_s* m_pSprite;		// orig +0x4a4: sprites/heflare4.spr
};

// The factory (the dispatcher's case 0x43; c3_tents.h). pCopy: a copy of that one instead,
// through the implicit copy constructor, which the original has out of line (
// orig client 0x9b1a0). The dispatcher always passes null.
CClientTent* RC2_NewRingBlastTent(const CClientTent* pCopy) // orig client 0x91c40
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CRingBlastTent* pTent = pCopy ? new CRingBlastTent(*static_cast<const CRingBlastTent*>(pCopy)) : new CRingBlastTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x91c40): reads the message and lays out the circle.
CRingBlastTent::CRingBlastTent()
{
	m_vecOrigin.x = READ_COORD();
	m_vecOrigin.y = READ_COORD();
	m_vecOrigin.z = READ_COORD();
	m_vecDir.x = READ_SHORT() * 3.051851e-05f;
	m_vecDir.y = READ_SHORT() * 3.051851e-05f;
	m_vecDir.z = READ_SHORT() * 3.051851e-05f;
	m_vecDir = m_vecDir.Normalize();
	m_flAlpha = 0.95;
	m_flTimeLeft = m_flLife = 0.3;

	// two axes across the direction, from a random one
	do
	{
		m_vecAxis1.x = gEngfuncs.pfnRandomFloat(-1, 1);
		m_vecAxis1.y = gEngfuncs.pfnRandomFloat(-1, 1);
		m_vecAxis1.z = gEngfuncs.pfnRandomFloat(-1, 1);
	} while (m_vecAxis1.x * m_vecAxis1.x + m_vecAxis1.y * m_vecAxis1.y + m_vecAxis1.z * m_vecAxis1.z == 0);
	m_vecAxis2 = CrossProduct(m_vecAxis1, m_vecDir).Normalize();
	m_vecAxis1 = CrossProduct(m_vecAxis2, m_vecDir).Normalize();

	int iModelIndex;
	m_pSprite = gEngfuncs.CL_LoadModel("sprites/heflare4.spr", &iModelIndex);

	if (g_iClientDetail < 0)
		m_iPoints = 8;
	else if (g_iClientDetail == 0)
		m_iPoints = 10;
	else
		m_iPoints = 12;

	float flAngle = gEngfuncs.pfnRandomFloat(0, 6.2831855);
	const float flStep = 6.283185307179586 / m_iPoints;
	m_vecCenter = m_vecOrigin;
	for (int i = 0; i < m_iPoints; i++)
	{
		m_vecCircle[i].x = cos(flAngle);
		m_vecCircle[i].y = sin(flAngle);
		m_vecCircle[i].z = 0;
		for (int j = 0; j < RINGBLAST_RINGS; j++)
		{
			m_vecRing[j][i] = m_vecCenter;
			m_vecRingUV[j][i] = Vector2D(0.5, 0.5) + m_vecCircle[i].Make2D() * 0.5 * ((j + 1) * 0.25f);
		}
		flAngle = flStep + flAngle;
	}
}

bool CRingBlastTent::Update(float flFrametime) // orig client 0x92350
{
	m_flTimeLeft -= flFrametime;
	if (m_flTimeLeft < 0)
		return true;

	float flDone = 1.0 - m_flTimeLeft / m_flLife;
	if (flDone < 0)
		flDone = 0;
	else if (flDone > 1)
		flDone = 1;

	m_flAlpha = (1.0 - flDone) * 0.7;
	const float flAhead = flDone * 256 + 16;
	const float flRadius = flDone * 280 + 16;
	// VERIFY: read off the x87 code as (256d - 16d - flAhead) / 4, i.e. the rings stack back
	// toward the origin, 4 + 4d apart
	const float flSpacing = ((flDone * 256) - flDone * 16 - flAhead) * 0.25;

	m_vecCenter = m_vecDir * flAhead + m_vecOrigin;

	for (int j = 1; j <= RINGBLAST_RINGS; j++)
	{
		const Vector vecRingCenter = m_vecDir * (j * flSpacing + flAhead) + m_vecOrigin;
		const float flRingRadius = j * 0.25f * flRadius;
		for (int i = 0; i < m_iPoints; i++)
		{
			const Vector vecOut = m_vecAxis2 * m_vecCircle[i].y * flRingRadius;
			const Vector vecAcross = m_vecAxis1 * m_vecCircle[i].x * flRingRadius;
			m_vecRing[j - 1][i] = (vecAcross + vecRingCenter) + vecOut;
		}
	}
	return false;
}

// The disc (a fan from the centre to the first ring), then a strip between each ring and the
// next, all in pale blue.
void CRingBlastTent::DrawTransparent() // orig client 0x92780
{
	if (m_flAlpha <= 0)
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pSprite, 0);

	gEngfuncs.pTriAPI->Color4f(0.9, 0.9, 1.0, m_flAlpha);
	gEngfuncs.pTriAPI->Brightness(m_flAlpha);

	gEngfuncs.pTriAPI->Begin(TRI_TRIANGLE_FAN);
	gEngfuncs.pTriAPI->TexCoord2f(0.5, 0.5);
	gEngfuncs.pTriAPI->Vertex3f(m_vecCenter.x, m_vecCenter.y, m_vecCenter.z);
	for (int i = 0; i < m_iPoints; i++)
	{
		gEngfuncs.pTriAPI->TexCoord2f(m_vecRingUV[0][i].x, m_vecRingUV[0][i].y);
		gEngfuncs.pTriAPI->Vertex3f(m_vecRing[0][i].x, m_vecRing[0][i].y, m_vecRing[0][i].z);
	}
	gEngfuncs.pTriAPI->TexCoord2f(m_vecRingUV[0][0].x, m_vecRingUV[0][0].y);
	gEngfuncs.pTriAPI->Vertex3f(m_vecRing[0][0].x, m_vecRing[0][0].y, m_vecRing[0][0].z);
	gEngfuncs.pTriAPI->End();

	for (int j = 0; j < RINGBLAST_RINGS - 1; j++)
	{
		gEngfuncs.pTriAPI->Begin(TRI_QUAD_STRIP);
		for (int i = 0; i < m_iPoints; i++)
		{
			gEngfuncs.pTriAPI->TexCoord2f(m_vecRingUV[j + 1][i].x, m_vecRingUV[j + 1][i].y);
			gEngfuncs.pTriAPI->Vertex3f(m_vecRing[j + 1][i].x, m_vecRing[j + 1][i].y, m_vecRing[j + 1][i].z);
			gEngfuncs.pTriAPI->TexCoord2f(m_vecRingUV[j][i].x, m_vecRingUV[j][i].y);
			gEngfuncs.pTriAPI->Vertex3f(m_vecRing[j][i].x, m_vecRing[j][i].y, m_vecRing[j][i].z);
		}
		gEngfuncs.pTriAPI->TexCoord2f(m_vecRingUV[j + 1][0].x, m_vecRingUV[j + 1][0].y);
		gEngfuncs.pTriAPI->Vertex3f(m_vecRing[j + 1][0].x, m_vecRing[j + 1][0].y, m_vecRing[j + 1][0].z);
		gEngfuncs.pTriAPI->TexCoord2f(m_vecRingUV[j][0].x, m_vecRingUV[j][0].y);
		gEngfuncs.pTriAPI->Vertex3f(m_vecRing[j][0].x, m_vecRing[j][0].y, m_vecRing[j][0].z);
		gEngfuncs.pTriAPI->End();
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// The SDK's vector.h inlines, which the original compiled out of line in this file, used by the
// constructor above (halflife-updated's vector.h has the same):
// orig client 0x92200: Vector2D::Vector2D() (returns this; the m_vecRingUV elements' constructor)
// orig client 0x92220: Vector2D::operator+(const Vector2D&)
// orig client 0x92260: Vector2D::operator*(float)
// orig client 0x922a0: Vector::Make2D()
// orig client 0x922e0: CrossProduct(const Vector&, const Vector&)
