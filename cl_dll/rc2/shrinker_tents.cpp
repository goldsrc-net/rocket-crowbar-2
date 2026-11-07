/***
*
*	Rocket Crowbar 2 client: the shrinker's effects (CLTE 0x27 and 0x28).
*
*	CLTE 0x27 is the shrinker's shot in flight (server shrinkerproject.cpp, RC2_CLTE_SHRINKER):
*	an orange light and a flickering sprites/shrinkf.spr flare on it, leaving a trail of
*	sprites/sqflare.spr sparks that shoot backward and fall behind, and the shot's hum. CLTE 0x28
*	is a shrinking (or a return to size) of what it hit (server shrinktimer.cpp): the client
*	scales the entity's model itself, from 1 down to 0.2 (or back up) over 0.6 seconds, while
*	sparks spiral off its outline.
*
*	The original's functions 0x72bf0-0x75500 (and 0x99e90, 0x9a090) of client.dll's run
*	"new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"

#include <cmath>

extern Vector v_angles; // view.cpp (stock): the view's angles

#define SHRINKER_SPARKS 150 // name ours

//
// CLTE 0x27: short the shot.
//
class CShrinkerGlowTent : public CClientTent // name ours
{
public:
	CShrinkerGlowTent();

	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEntIndex) override;

	int m_iEntIndex;							 // orig +0x8
	Vector m_vecOrigin;							 // orig +0xc: where the shot was last frame
	Vector m_vecNewOrigin;						 // orig +0x18: where it is now
	Vector m_vecDir;							 // orig +0x24: its direction of travel
	Vector m_vecSparkOrigin[SHRINKER_SPARKS];	 // orig +0x30
	Vector m_vecSparkVelocity[SHRINKER_SPARKS];	 // orig +0x738
	Vector m_vecSparkAccel[SHRINKER_SPARKS];	 // orig +0xe40
	float m_flSparkAlpha[SHRINKER_SPARKS];		 // orig +0x1548
	bool m_bSparkActive[SHRINKER_SPARKS];		 // orig +0x17a0
	Vector m_vecAngles;							 // orig +0x1838: m_vecDir's, with a random roll for each spark
	dlight_t* m_pDlight;						 // orig +0x1844
	dlight_t* m_pElight;						 // orig +0x1848
	model_s* m_pSparkSprite;					 // orig +0x184c: sprites/sqflare.spr
	model_s* m_pFlareSprite;					 // orig +0x1850: sprites/shrinkf.spr
	float m_flSparksDue;						 // orig +0x1854: sparks owed, by cl_detail
	bool m_bDying;								 // orig +0x1858: the shot is gone; the sparks die out
	bool m_bVisible;							 // orig +0x1859: the shot is in this client's PVS
};

// The factory (the dispatcher's case 0x27; c3_tents.h). pCopy: a copy of that one instead,
// through the implicit copy constructor, which the original has out of line
// (orig client 0x99e90).
CClientTent* RC2_NewShrinkerGlowTent(const CClientTent* pCopy) // orig client 0x72bf0
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CShrinkerGlowTent* pTent = pCopy ? new CShrinkerGlowTent(*static_cast<const CShrinkerGlowTent*>(pCopy)) : new CShrinkerGlowTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x72bf0): reads the message, starts the hum and the lights.
CShrinkerGlowTent::CShrinkerGlowTent()
{
	m_iEntIndex = READ_SHORT();
	m_bVisible = true;
	m_bDying = false;
	for (int i = 0; i < SHRINKER_SPARKS; i++)
		m_bSparkActive[i] = false;
	m_flSparksDue = 0;
	m_pSparkSprite = nullptr;
	m_pFlareSprite = nullptr;
	m_vecOrigin = m_vecNewOrigin = m_vecAngles = Vector(0, 0, 0);
	m_pDlight = nullptr;
	m_pElight = nullptr;

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_iEntIndex = 0;
		return;
	}

	m_vecOrigin = pEntity->origin;
	m_vecNewOrigin = pEntity->origin;
	m_vecDir = Vector(0, 0, 1);

	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, CHAN_STATIC, "ambience/crtnoise.wav", 0.6, 0.7, 0, 130);

	int iModelIndex;
	m_pSparkSprite = gEngfuncs.CL_LoadModel("sprites/sqflare.spr", &iModelIndex);
	m_pFlareSprite = gEngfuncs.CL_LoadModel("sprites/shrinkf.spr", &iModelIndex);

	if (g_iClientDetail >= 0)
	{
		m_pDlight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		if (m_pDlight)
		{
			m_pDlight->origin = pEntity->origin;
			m_pDlight->radius = 120;
			m_pDlight->color.r = 255;
			m_pDlight->color.g = 180;
			m_pDlight->color.b = 0;
			m_pDlight->die = gEngfuncs.GetClientTime() + 10.0;
		}

		m_pElight = gEngfuncs.pEfxAPI->CL_AllocElight(0);
		if (m_pElight)
		{
			m_pElight->origin = pEntity->origin;
			m_pElight->radius = 100;
			m_pElight->color.r = 255;
			m_pElight->color.g = 180;
			m_pElight->color.b = 0;
			m_pElight->die = gEngfuncs.GetClientTime() + 10.0;
		}
	}
}

// Follows the shot and sheds sparks: 150, 100 or 50 a second (cl_detail above, at, below 0),
// first into free slots in order, then any still owed onto random slots, live or not. Each spark
// starts somewhere along the stretch the shot covered this frame, a little off its line, and
// flies backward, decelerating hard until it turns forward again, fading over 0.72 seconds.
bool CShrinkerGlowTent::Update(float flFrametime) // orig client 0x73120
{
	m_vecNewOrigin = m_vecOrigin;
	cl_entity_t* pEntity = nullptr;

	if (!m_bDying)
	{
		if (m_iEntIndex <= 0)
			return true;

		pEntity = GetEntity(m_iEntIndex);
		if (!pEntity)
		{
			KillEntity(m_iEntIndex);
			m_iEntIndex = 0;
			return true;
		}

		m_bVisible = true;
		cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
		if (pLocal && pLocal->curstate.messagenum > pEntity->curstate.messagenum)
		{
			m_bVisible = false;
			return false;
		}

		m_vecNewOrigin = pEntity->origin;
		VectorAngles(m_vecDir, m_vecAngles);
		m_vecAngles.x = -m_vecAngles.x;

		if (g_iClientDetail >= 1)
			m_flSparksDue += flFrametime * 150;
		else if (g_iClientDetail == 0)
			m_flSparksDue += flFrametime * 100;
		else
			m_flSparksDue += flFrametime * 50;
	}
	else
	{
		m_flSparksDue = 0;
	}

	bool bAnyAlive = false;
	for (int i = 0; i < SHRINKER_SPARKS; i++)
	{
		if (!m_bSparkActive[i])
		{
			if (m_flSparksDue < 1 || m_bDying)
				continue;

			m_flSparksDue -= 1.0;
			m_vecAngles.z = gEngfuncs.pfnRandomFloat(0, 360);
			Vector vecForward, vecRight, vecUp;
			gEngfuncs.pfnAngleVectors(m_vecAngles, vecForward, vecRight, vecUp);
			m_bSparkActive[i] = true;
			m_flSparkAlpha[i] = 0.9;

			const Vector vecMove(m_vecNewOrigin.x - m_vecOrigin.x, m_vecNewOrigin.y - m_vecOrigin.y, m_vecNewOrigin.z - m_vecOrigin.z);
			m_vecSparkOrigin[i] = m_vecOrigin + vecMove * gEngfuncs.pfnRandomFloat(0, 1);
			m_vecSparkOrigin[i] = m_vecSparkOrigin[i] + vecUp * gEngfuncs.pfnRandomFloat(4, 16);
			m_vecSparkVelocity[i] = vecForward * gEngfuncs.pfnRandomFloat(-60, -80);
			m_vecSparkAccel[i] = vecForward * gEngfuncs.pfnRandomFloat(500, 600);
			// a new spark moves in the same frame
		}

		m_flSparkAlpha[i] -= flFrametime * 1.25;
		if (m_flSparkAlpha[i] <= 0)
		{
			m_bSparkActive[i] = false;
			m_flSparkAlpha[i] = 0;
			continue;
		}

		bAnyAlive = true;
		m_vecSparkVelocity[i].x += flFrametime * m_vecSparkAccel[i].x;
		m_vecSparkVelocity[i].y += flFrametime * m_vecSparkAccel[i].y;
		m_vecSparkVelocity[i].z += flFrametime * m_vecSparkAccel[i].z;
		m_vecSparkOrigin[i].x += flFrametime * m_vecSparkVelocity[i].x;
		m_vecSparkOrigin[i].y += flFrametime * m_vecSparkVelocity[i].y;
		m_vecSparkOrigin[i].z += flFrametime * m_vecSparkVelocity[i].z;
	}

	if (m_bDying)
	{
		if (!bAnyAlive || !m_bVisible)
			return true;
	}
	else
	{
		// keep the lights on the shot (each frame pushes their end a second further off)
		if (m_pDlight)
		{
			m_pDlight->origin = pEntity->origin;
			m_pDlight->die += 1.0;
		}
		if (m_pElight)
		{
			m_pElight->origin = pEntity->origin;
			m_pElight->die += 1.0;
		}
	}

	while (m_flSparksDue >= 1 && !m_bDying)
	{
		m_flSparksDue -= 1.0;
		const int i = gEngfuncs.pfnRandomLong(0, SHRINKER_SPARKS - 1);
		m_vecAngles.z = gEngfuncs.pfnRandomFloat(0, 360);
		Vector vecForward, vecRight, vecUp;
		gEngfuncs.pfnAngleVectors(m_vecAngles, vecForward, vecRight, vecUp);
		m_bSparkActive[i] = true;
		m_flSparkAlpha[i] = 0.9;

		const Vector vecMove(m_vecNewOrigin.x - m_vecOrigin.x, m_vecNewOrigin.y - m_vecOrigin.y, m_vecNewOrigin.z - m_vecOrigin.z);
		const float flFrac = gEngfuncs.pfnRandomFloat(0, 1);
		const Vector vecOffset(vecMove.x * flFrac, vecMove.y * flFrac, vecMove.z * flFrac);
		m_vecSparkOrigin[i] = Vector(vecOffset.x + m_vecOrigin.x, vecOffset.y + m_vecOrigin.y, vecOffset.z + m_vecOrigin.z);
		const float flUp = gEngfuncs.pfnRandomFloat(4, 16);
		m_vecSparkOrigin[i] = m_vecSparkOrigin[i] + Vector(vecUp.x * flUp, vecUp.y * flUp, vecUp.z * flUp);
		m_vecSparkVelocity[i] = vecForward * gEngfuncs.pfnRandomFloat(-60, -80);
		m_vecSparkAccel[i] = vecForward * gEngfuncs.pfnRandomFloat(500, 600);
	}

	if (m_vecNewOrigin.x != m_vecOrigin.x || m_vecNewOrigin.y != m_vecOrigin.y || m_vecNewOrigin.z != m_vecOrigin.z)
	{
		const Vector vecMove(m_vecNewOrigin.x - m_vecOrigin.x, m_vecNewOrigin.y - m_vecOrigin.y, m_vecNewOrigin.z - m_vecOrigin.z);
		const float flLength = sqrt(vecMove.y * vecMove.y + vecMove.z * vecMove.z + vecMove.x * vecMove.x);
		if (flLength == 0)
			m_vecDir = Vector(0, 0, 1);
		else
		{
			const float flInv = 1.0 / flLength;
			m_vecDir = Vector(vecMove.x * flInv, vecMove.y * flInv, vecMove.z * flInv);
		}
		m_vecOrigin = m_vecNewOrigin;
	}
	return false;
}

// The shot is gone: put the lights out at once, stop the hum, and let the sparks die out.
bool CShrinkerGlowTent::KillEntity(int iEntIndex) // orig client 0x73b20
{
	if (iEntIndex == m_iEntIndex)
	{
		if (m_pElight)
		{
			m_pElight->die = gEngfuncs.GetClientTime() + 0.01;
			m_pElight = nullptr;
		}
		if (m_pDlight)
		{
			m_pDlight->die = gEngfuncs.GetClientTime() + 0.01;
			m_pDlight = nullptr;
		}
		gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, CHAN_STATIC, "ambience/crtnoise.wav");
		m_iEntIndex = 0;
		m_bDying = true;
	}
	return false;
}

// One view-facing quad corner set: origin + right * flRight + up * flUp. name ours
static void ShrinkerQuadVertex(const Vector& vecOrigin, const Vector& vecRight, const Vector& vecUp, float flRight, float flUp, float s, float t)
{
	const Vector vecVertex = vecOrigin + vecRight * flRight + vecUp * flUp;
	gEngfuncs.pTriAPI->TexCoord2f(s, t);
	gEngfuncs.pTriAPI->Vertex3f(vecVertex.x, vecVertex.y, vecVertex.z);
}

// The sparks, orange 8-unit quads; and while the shot lives, its flare: a white quad of 25 to
// 27 units, rolled at random and jittered by up to 2 units each frame.
void CShrinkerGlowTent::DrawTransparent() // orig client 0x73be0
{
	if (!m_bVisible)
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pSparkSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pSparkSprite, 0);

	gEngfuncs.pTriAPI->Begin(TRI_QUADS);
	for (int i = 0; i < SHRINKER_SPARKS; i++)
	{
		if (!m_bSparkActive[i])
			continue;
		const float flAlpha = m_flSparkAlpha[i];
		if (flAlpha <= 0)
			continue;

		gEngfuncs.pTriAPI->Color4f(1.0, 0.6, 0, flAlpha);
		gEngfuncs.pTriAPI->Brightness(m_flSparkAlpha[i]);
		ShrinkerQuadVertex(m_vecSparkOrigin[i], g_vecTentRight, g_vecTentUp, 4, 4, 0, 0);
		ShrinkerQuadVertex(m_vecSparkOrigin[i], g_vecTentRight, g_vecTentUp, -4, 4, 1, 0);
		ShrinkerQuadVertex(m_vecSparkOrigin[i], g_vecTentRight, g_vecTentUp, -4, -4, 1, 1);
		ShrinkerQuadVertex(m_vecSparkOrigin[i], g_vecTentRight, g_vecTentUp, 4, -4, 0, 1);
	}
	gEngfuncs.pTriAPI->End();

	if (!m_bDying)
	{
		// VERIFY: the original tests the spark sprite (+0x184c) but binds the flare's (+0x1850)
		if (m_pSparkSprite)
			gEngfuncs.pTriAPI->SpriteTexture(m_pFlareSprite, 0);

		const float flSize = gEngfuncs.pfnRandomFloat(-1, 1) + 26;
		Vector vecAngles = v_angles;
		vecAngles.z = gEngfuncs.pfnRandomFloat(0, 360);
		Vector vecRight, vecUp;
		gEngfuncs.pfnAngleVectors(vecAngles, nullptr, vecRight, vecUp);

		Vector vecFlare = m_vecNewOrigin;
		vecFlare.x = gEngfuncs.pfnRandomFloat(-2, 2) + vecFlare.x;
		vecFlare.y = gEngfuncs.pfnRandomFloat(-2, 2) + vecFlare.y;
		vecFlare.z = gEngfuncs.pfnRandomFloat(-2, 2) + vecFlare.z;

		gEngfuncs.pTriAPI->Begin(TRI_QUADS);
		gEngfuncs.pTriAPI->Color4f(1, 1, 1, 0.95);
		gEngfuncs.pTriAPI->Brightness(0.95);
		// VERIFY: which of the rolled right/up vectors goes with which corner (a square rolled at
		// random either way)
		ShrinkerQuadVertex(vecFlare, vecRight, vecUp, flSize, flSize, 0, 0);
		ShrinkerQuadVertex(vecFlare, vecRight, vecUp, -flSize, flSize, 1, 0);
		ShrinkerQuadVertex(vecFlare, vecRight, vecUp, -flSize, -flSize, 1, 1);
		ShrinkerQuadVertex(vecFlare, vecRight, vecUp, flSize, -flSize, 0, 1);
		gEngfuncs.pTriAPI->End();
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

//
// CLTE 0x28: short the entity, byte 0 to shrink it, 1 to bring it back to size.
//
class CShrinkTent : public CClientTent // name ours
{
public:
	CShrinkTent();

	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEntIndex) override;

	void SpawnSpark(int i);

	int m_iEntIndex;							 // orig +0x8
	bool m_bRestore;							 // orig +0xc: growing back to size
	Vector m_vecOrigin;							 // orig +0x10: the entity's
	float m_flSizeX;							 // orig +0x1c: 2, or 8 when it has no width (and shrinks)
	float m_flSizeY;							 // orig +0x20: the same for y
	float m_flHeight;							 // orig +0x24: its bounding box's; a player's 72, 14.4 coming back
	float m_flScale;							 // orig +0x28: the model scale given the entity, 0.2-1
	Vector m_vecSparkOrigin[SHRINKER_SPARKS];	 // orig +0x2c
	Vector m_vecSparkVelocity[SHRINKER_SPARKS];	 // orig +0x734
	Vector m_vecSparkAccel[SHRINKER_SPARKS];	 // orig +0xe3c
	float m_flSparkAlpha[SHRINKER_SPARKS];		 // orig +0x1544
	bool m_bSparkActive[SHRINKER_SPARKS];		 // orig +0x179c
	Vector m_vecRing;							 // orig +0x1834: the spiral's size this frame
	Vector m_vecPrevRing;						 // orig +0x1840: and last frame
	float m_flAngle;							 // orig +0x184c: the spiral's angle this frame
	float m_flPrevAngle;						 // orig +0x1850: and last frame
	model_s* m_pSprite;							 // orig +0x1854: sprites/sqflare.spr
	float m_flSparksDue;						 // orig +0x1858
	bool m_bDone;								 // orig +0x185c: the scale has arrived
	bool m_bVisible;							 // orig +0x185d
	bool m_bDying;								 // orig +0x185e
};

// The factory (the dispatcher's case 0x28; c3_tents.h). pCopy: a copy of that one instead,
// through the implicit copy constructor, which the original has out of line
// (orig client 0x9a090).
CClientTent* RC2_NewShrinkTent(const CClientTent* pCopy) // orig client 0x74600
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CShrinkTent* pTent = pCopy ? new CShrinkTent(*static_cast<const CShrinkTent*>(pCopy)) : new CShrinkTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// The larger of |a| and |b| (b's on a tie). Inline in the original. name ours
static float ShrinkExtent(float a, float b)
{
	return fabs(a) > fabs(b) ? fabs(a) : fabs(b);
}

// Inline in the factory (orig 0x74600): reads the message and sizes the spiral from the
// entity's bounding box.
CShrinkTent::CShrinkTent()
{
	m_iEntIndex = READ_SHORT();
	m_bRestore = READ_BYTE() != 0;
	m_bVisible = true;
	m_bDone = false;
	m_bDying = false;
	for (int i = 0; i < SHRINKER_SPARKS; i++)
		m_bSparkActive[i] = false;
	m_flSparksDue = 0;
	m_pSprite = nullptr;
	m_vecOrigin = Vector(0, 0, 0);
	m_flScale = m_bRestore ? 0.2 : 1.0;
	m_flAngle = m_flPrevAngle = gEngfuncs.pfnRandomFloat(0, M_PI);

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_iEntIndex = 0;
		return;
	}

	m_vecOrigin = pEntity->origin;

	// VERIFY: literal: 8 only when the extent is 0 and the byte is 0 (the sum of the two is tested)
	const float flRestore = (byte)m_bRestore;
	m_flSizeX = (flRestore + ShrinkExtent(pEntity->curstate.mins.x, pEntity->curstate.maxs.x) == 0) ? 8 : 2;
	m_flSizeY = (flRestore + ShrinkExtent(pEntity->curstate.mins.y, pEntity->curstate.maxs.y) == 0) ? 8 : 2;
	m_flHeight = pEntity->curstate.maxs.z - pEntity->curstate.mins.z;
	if (pEntity->player)
		m_flHeight = m_bRestore ? 14.4 : 72;

	int iModelIndex;
	m_pSprite = gEngfuncs.CL_LoadModel("sprites/sqflare.spr", &iModelIndex);
}

// Which half of the spiral the next spark starts on: flips with every spark (the original's
// static at .data 0x101371b0, initially 1). name ours
static bool s_bShrinkSparkFlip = true;

// A spark at a random point of the spiral's sweep this frame. Inline at both uses in the
// original (Update's two spawn loops). name ours
void CShrinkTent::SpawnSpark(int i)
{
	const float flFrac = gEngfuncs.pfnRandomFloat(0, 1);
	float flAngle = (m_flAngle - m_flPrevAngle) * flFrac + m_flPrevAngle;
	s_bShrinkSparkFlip = !s_bShrinkSparkFlip;
	if (s_bShrinkSparkFlip)
		flAngle += M_PI;

	const Vector vecDir(cos(flAngle), sin(flAngle), 0);
	m_bSparkActive[i] = true;
	m_flSparkAlpha[i] = 0.9;

	m_vecSparkOrigin[i].x = ((m_vecRing.x - m_vecPrevRing.x) * flFrac + m_vecPrevRing.x) * vecDir.x;
	m_vecSparkOrigin[i].y = ((m_vecRing.y - m_vecPrevRing.y) * flFrac + m_vecPrevRing.y) * vecDir.y;
	m_vecSparkOrigin[i].z = (m_vecRing.z - m_vecPrevRing.z) * flFrac + m_vecPrevRing.z;
	m_vecSparkOrigin[i] = Vector(m_vecOrigin.x + m_vecSparkOrigin[i].x, m_vecOrigin.y + m_vecSparkOrigin[i].y, m_vecOrigin.z + m_vecSparkOrigin[i].z);
	m_vecSparkVelocity[i] = Vector(0, 0, 0);

	if (m_bRestore)
		m_vecSparkAccel[i] = vecDir * gEngfuncs.pfnRandomFloat(100, 150);
	else
		m_vecSparkAccel[i] = vecDir * gEngfuncs.pfnRandomFloat(50, 80);
}

// Scales the entity a step toward its target (4/3 of the way a second), turns the spiral
// (two turns a second) and sheds sparks along it: 86.7, 60 or 40 a second by cl_detail, first
// into free slots in order, then any still owed onto random slots. The sparks fade over 1.35
// seconds, accelerating outward.
bool CShrinkTent::Update(float flFrametime) // orig client 0x74b30
{
	if (!m_iEntIndex && !m_bDying)
		return true;

	cl_entity_t* pEntity = nullptr;
	if (!m_bDying)
	{
		pEntity = GetEntity(m_iEntIndex);
		if (!pEntity)
		{
			KillEntity(m_iEntIndex);
			m_iEntIndex = 0;
			return true;
		}
	}

	m_flPrevAngle = m_flAngle;
	m_vecPrevRing = m_vecRing;

	if (!m_bDone)
	{
		m_bVisible = true;
		cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
		if (pLocal && pEntity && pLocal->curstate.messagenum > pEntity->curstate.messagenum)
		{
			m_bVisible = false;
			return m_bDying;
		}
		if (pEntity)
			m_vecOrigin = pEntity->origin;

		if (!m_bRestore)
		{
			m_flScale -= flFrametime * 1.3333334;
			if (m_flScale <= 0.2)
			{
				m_flScale = 0.2;
				m_bDone = true;
			}
		}
		else
		{
			m_flScale += flFrametime * 1.3333334;
			if (m_flScale >= 1)
			{
				m_flScale = 1;
				m_bDone = true;
			}
		}

		m_flAngle += flFrametime * 12.566371; // 4 pi

		const float flRing = m_bRestore ? m_flScale * 5.0 : m_flScale;
		m_vecRing = Vector(flRing * m_flSizeX, flRing * m_flSizeY, flRing * m_flHeight);

		if (g_iClientDetail >= 1)
			m_flSparksDue += flFrametime * 86.666664;
		else if (g_iClientDetail == 0)
			m_flSparksDue += flFrametime * 60;
		else
			m_flSparksDue += flFrametime * 40;
	}
	else
	{
		m_flSparksDue = 0;
		m_flScale = m_bRestore ? 1.0 : 0.2;
	}

	if (pEntity)
		pEntity->curstate.scale = m_flScale;

	if (m_bDying)
		m_flSparksDue = 0;

	bool bAnyAlive = false;
	for (int i = 0; i < SHRINKER_SPARKS; i++)
	{
		if (!m_bSparkActive[i])
		{
			if (m_flSparksDue < 1)
				continue;
			m_flSparksDue -= 1.0;
			SpawnSpark(i);
			// a new spark moves in the same frame
		}

		m_flSparkAlpha[i] -= flFrametime * 0.6666667;
		if (m_flSparkAlpha[i] <= 0)
		{
			m_bSparkActive[i] = false;
			m_flSparkAlpha[i] = 0;
			continue;
		}

		bAnyAlive = true;
		m_vecSparkVelocity[i].x += flFrametime * m_vecSparkAccel[i].x;
		m_vecSparkVelocity[i].y += flFrametime * m_vecSparkAccel[i].y;
		m_vecSparkVelocity[i].z += flFrametime * m_vecSparkAccel[i].z;
		m_vecSparkOrigin[i].x += flFrametime * m_vecSparkVelocity[i].x;
		m_vecSparkOrigin[i].y += flFrametime * m_vecSparkVelocity[i].y;
		m_vecSparkOrigin[i].z += flFrametime * m_vecSparkVelocity[i].z;
	}

	if (!bAnyAlive && m_bDying)
		return true;

	while (m_flSparksDue >= 1 && !m_bDone && !m_bDying)
	{
		m_flSparksDue -= 1.0;
		SpawnSpark(gEngfuncs.pfnRandomLong(0, SHRINKER_SPARKS - 1));
	}
	return false;
}

bool CShrinkTent::KillEntity(int iEntIndex) // orig client 0x754d0
{
	if (iEntIndex == m_iEntIndex)
	{
		m_iEntIndex = 0;
		m_bDying = true;
	}
	return false;
}

// The sparks, orange 6-unit quads.
void CShrinkTent::DrawTransparent() // orig client 0x75500
{
	if (!m_bVisible)
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pSprite, 0);

	gEngfuncs.pTriAPI->Begin(TRI_QUADS);
	for (int i = 0; i < SHRINKER_SPARKS; i++)
	{
		if (!m_bSparkActive[i])
			continue;
		const float flAlpha = m_flSparkAlpha[i];
		if (flAlpha <= 0)
			continue;

		gEngfuncs.pTriAPI->Color4f(1.0, 0.6, 0, flAlpha);
		gEngfuncs.pTriAPI->Brightness(m_flSparkAlpha[i]);
		ShrinkerQuadVertex(m_vecSparkOrigin[i], g_vecTentRight, g_vecTentUp, 3, 3, 0, 0);
		ShrinkerQuadVertex(m_vecSparkOrigin[i], g_vecTentRight, g_vecTentUp, -3, 3, 1, 0);
		ShrinkerQuadVertex(m_vecSparkOrigin[i], g_vecTentRight, g_vecTentUp, -3, -3, 1, 1);
		ShrinkerQuadVertex(m_vecSparkOrigin[i], g_vecTentRight, g_vecTentUp, 3, -3, 0, 1);
	}
	gEngfuncs.pTriAPI->End();

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}
