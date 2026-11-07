/***
*
*	Rocket Crowbar 2 client: the snark camera's trail (CLTE 0x3e).
*
*	The server's snarkcam.cpp sends it from the projectile's Spawn: a small red glow
*	(sprites/glow04.spr) on the projectile and a trail of sprites/gradsmoke.spr behind it, a point
*	dropped every 0.05-0.15 s by cl_detail. When the projectile goes (a CLTE kill) the glow goes
*	and the trail fades out on its own.
*
*	The original's functions 0x8baf0-0x8c57e of client.dll's run "new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"

//
// CLTE 0x3e: short the projectile.
//
class CSnarkTrailTent : public CClientTent // name ours
{
public:
	CSnarkTrailTent();

	bool Think(float flFrametime) override;
	void DrawTransparent() override;
	bool EntityGone(int iEntIndex) override;

	void AddTrailPoint();

	int m_iEntIndex;		// orig +0x8
	bool m_bVisible;		// orig +0xc: in the local player's view this frame
	float m_flNextPoint;	// orig +0x10
	Vector m_vecOrigin;		// orig +0x14
	CRC2Trail m_Trail;		// orig +0x20
	model_s* m_pGlowSprite; // orig +0x48: sprites/glow04.spr
	model_s* m_pTrailSprite; // orig +0x4c: sprites/gradsmoke.spr
	bool m_bDying;			// orig +0x50: the projectile is gone, the trail fades
};

// The factory (the dispatcher's case 0x3e). The implicit copy constructor is inline here; the
// copy shares the trail's arrays.
CClientTent* RC2_NewSnarkTrailTent(const CClientTent* pCopy) // orig client 0x8baf0
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CSnarkTrailTent* pTent = pCopy ? new CSnarkTrailTent(*static_cast<const CSnarkTrailTent*>(pCopy)) : new CSnarkTrailTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// A trail point at the projectile: still, colour (1, 0, 0), alpha 0.8, width
// 1.5, fading 0.4 a second. Inline at each use in the original.
void CSnarkTrailTent::AddTrailPoint() // name ours
{
	m_Trail.AddPoint(m_vecOrigin, Vector(0, 0, 0), Vector(0, 0, 0), Vector(1, 0, 0), 0.8, 1.5, 0.4, 0, nullptr);
}

// Inline in the factory: reads the message and starts the trail.
CSnarkTrailTent::CSnarkTrailTent()
{
	m_iEntIndex = READ_SHORT();
	m_bVisible = false;
	m_bDying = false;
	m_pGlowSprite = nullptr;

	cl_entity_t* pEntity = RC2_GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_iEntIndex = 0;
		return;
	}

	m_vecOrigin = pEntity->origin;
	AddTrailPoint();

	if (g_iClDetail < 0)
		m_flNextPoint = 0.1;
	else if (g_iClDetail == 0)
		m_flNextPoint = 0.075;
	else
		m_flNextPoint = 0.05;

	int iModelIndex;
	m_pGlowSprite = gEngfuncs.CL_LoadModel("sprites/glow04.spr", &iModelIndex);
	m_pTrailSprite = gEngfuncs.CL_LoadModel("sprites/gradsmoke.spr", &iModelIndex);
}

bool CSnarkTrailTent::Think(float flFrametime) // orig client 0x8be10
{
	if (m_bDying)
		return m_Trail.Update(flFrametime) == 0;

	if (!m_iEntIndex)
		return true;

	cl_entity_t* pEntity = RC2_GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		EntityGone(m_iEntIndex);
		return true;
	}

	m_bVisible = true;
	cl_entity_t* pPlayer = gEngfuncs.GetLocalPlayer();
	if (!pPlayer || pPlayer->curstate.messagenum <= pEntity->curstate.messagenum)
	{
		m_vecOrigin = pEntity->origin;
		m_flNextPoint -= flFrametime;
		if (m_flNextPoint <= 0)
		{
			// VERIFY: these are 0.15, 0.1, 0.075, the constructor's 0.1, 0.075, 0.05
			if (g_iClDetail < 0)
				m_flNextPoint = 0.15;
			else if (g_iClDetail == 0)
				m_flNextPoint = 0.1;
			else
				m_flNextPoint = 0.075;
			AddTrailPoint();
		}
	}
	else
	{
		m_bVisible = false;
	}

	m_Trail.Update(flFrametime);
	return false;
}

// The glow, a view-facing quad of 8 units in red, then the trail led by the projectile.
void CSnarkTrailTent::DrawTransparent() // orig client 0x8bff0
{
	if (!m_bVisible)
		return;

	bool bTrail = true;
	if (!m_Trail.CanDraw())
	{
		bTrail = false;
		if (g_iClDetail < 0 || m_bDying)
			return;
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);

	if (g_iClDetail >= 0 && !m_bDying)
	{
		if (m_pGlowSprite)
			gEngfuncs.pTriAPI->SpriteTexture(m_pGlowSprite, 0);

		gEngfuncs.pTriAPI->Begin(TRI_QUADS);
		gEngfuncs.pTriAPI->Color4f(1.0, 0.2, 0.2, 0.95);
		gEngfuncs.pTriAPI->Brightness(0.95);

		Vector vecPoint = m_vecOrigin + g_vecTentRight * 4 + g_vecTentUp * 4;
		gEngfuncs.pTriAPI->TexCoord2f(0, 0);
		gEngfuncs.pTriAPI->Vertex3f(vecPoint.x, vecPoint.y, vecPoint.z);

		vecPoint = m_vecOrigin + g_vecTentRight * -4 + g_vecTentUp * 4;
		gEngfuncs.pTriAPI->TexCoord2f(1, 0);
		gEngfuncs.pTriAPI->Vertex3f(vecPoint.x, vecPoint.y, vecPoint.z);

		vecPoint = m_vecOrigin + g_vecTentRight * -4 + g_vecTentUp * -4;
		gEngfuncs.pTriAPI->TexCoord2f(1, 1);
		gEngfuncs.pTriAPI->Vertex3f(vecPoint.x, vecPoint.y, vecPoint.z);

		vecPoint = m_vecOrigin + g_vecTentRight * 4 + g_vecTentUp * -4;
		gEngfuncs.pTriAPI->TexCoord2f(0, 1);
		gEngfuncs.pTriAPI->Vertex3f(vecPoint.x, vecPoint.y, vecPoint.z);

		gEngfuncs.pTriAPI->End();
	}

	if (m_pTrailSprite && bTrail)
		gEngfuncs.pTriAPI->SpriteTexture(m_pTrailSprite, 0);
	m_Trail.DrawToHead(m_vecOrigin, Vector(1, 0, 0), 0.8, 1.5, true);

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// The projectile is gone: a last point where it was, and the trail fades out (the effect stays
// until it has).
bool CSnarkTrailTent::EntityGone(int iEntIndex) // orig client 0x8c4e0
{
	if (iEntIndex != m_iEntIndex)
		return false;

	AddTrailPoint();
	m_bDying = true;
	m_iEntIndex = 0;
	return false;
}
