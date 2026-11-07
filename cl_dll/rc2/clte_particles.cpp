/***
*
*	Rocket Crowbar 2: the client effects' point lists (see clte.h). CTentTrail keeps a trail of
*	points, newest first, and draws them as one strip; CTentParticles keeps loose particles drawn as
*	view-facing quads. A point's callback may move it instead of the default motion. The three
*	callbacks here are the ones the effect file defines before its message handler.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "clte.h"
#include "clte_types.h"

#include <stdlib.h>
#include <string.h>

// orig client 0x508e0. A trail point darkens to blue: red and green fall 8/7 a second
int Point_Darken(float flFrametime, Vector* pPos, Vector* pVel, Vector* pAccel, Vector* pColor,
	float* pAlpha, float* pWidth, float* pFade, void* pUser)
{
	pColor->x = pColor->x - flFrametime * 1.1428572;
	pColor->y = pColor->y - flFrametime * 1.1428572;
	pColor->z = pColor->z - 0;
	if (pColor->x < 0)
		pColor->x = 0;
	if (pColor->y < 0)
		pColor->y = 0;
	if (pColor->z < 0)
		pColor->z = 0;
	return 0;
}

static float Clamp01(float fl)
{
	if (fl < 0)
		return 0;
	if (fl < 1)
		return fl;
	return 1;
}

// orig client 0x509c0. A particle's color moves to (0.4, 0.4, 1) over the rest of its life
int Particle_FadeToBlue(float flFrametime, Vector* pPos, Vector* pVel, Vector* pAccel, Vector* pColor,
	float* pAlpha, float* pSize, float* pFade, float* pLife, void* pUser)
{
	if (*pLife != 0)
	{
		const float flLife = *pLife;
		pColor->x = ((0.4 - pColor->x) / flLife) * flFrametime + pColor->x;
		pColor->y = ((0.4 - pColor->y) / flLife) * flFrametime + pColor->y;
		pColor->z = ((1.0 - pColor->z) / flLife) * flFrametime + pColor->z;
	}
	pColor->x = Clamp01(pColor->x);
	pColor->y = Clamp01(pColor->y);
	if (pColor->z < 0)
		pColor->z = 0;
	else if (pColor->z > 1)
		pColor->z = 1;
	return 0;
}

// orig client 0x50b90. A particle that grows in place for its first fifth, then flies to its effect's
// target (pUser, CLTE 0x41's object), arriving as its life runs out; it dies if it overshoots
int Particle_Seek(float flFrametime, Vector* pPos, Vector* pVel, Vector* pAccel, Vector* pColor,
	float* pAlpha, float* pSize, float* pFade, float* pLife, void* pUser)
{
	if (!pUser)
		return 0;

	const float flPhase = Clamp01(*pLife);
	if (flPhase > 0.8)
	{
		*pVel = Vector(0, 0, 0);
		*pAccel = Vector(0, 0, 0);
		*pAlpha = flFrametime * 4.5 + *pAlpha;
		*pSize = flFrametime * 10.0 + *pSize;
		if (*pAlpha > 0.9)
			*pAlpha = 0.9;
		if (*pSize > 2.0)
			*pSize = 2.0;
		*pLife = *pLife - flFrametime;
		return 1;
	}

	*pSize = 2.0;
	if (flPhase <= 0.05)
	{
		*pAlpha = *pAlpha - flFrametime * 18.0;
		if (*pAlpha < 0.01)
			*pAlpha = 0.01;
	}
	else
	{
		*pAlpha = 0.9;
	}

	float flTime = *pLife;
	if (flTime < 0.01)
		flTime = 0.01;

	const Vector& vecTarget = static_cast<CTentType41*>(pUser)->m_vecTarget;
	*pAccel = (vecTarget - *pPos) * (2.0 / (flTime * flTime));
	*pVel = *pAccel * flFrametime + *pVel;
	*pPos = *pVel * flFrametime + *pPos;

	if (DotProduct(vecTarget - *pPos, *pAccel) < 0)
		*pLife = -1.0;

	*pLife = *pLife - flFrametime;
	return 1;
}

// --- CTentTrail ---

// orig client 0x5a6a0
CTentTrail::CTentTrail()
{
	m_iAllocated = 16;
	m_iCount = 0;
	m_iRenderAllocated = 0;
	m_pPoints = (tentpoint_t*)malloc(m_iAllocated * sizeof(tentpoint_t));
	memset(m_pPoints, 0, m_iAllocated * sizeof(tentpoint_t));
	m_pRenderColor = NULL;
	m_pRenderPos = NULL;
	m_pRenderAlpha = NULL;
	m_pRenderWidth = NULL;
	m_pRenderLength = NULL;
	m_iRenderAllocated = 0;
	m_iRenderCount = 0;
}

// orig client 0x5a720. orig client 0x51030 is its compiler-generated vector deleting destructor
CTentTrail::~CTentTrail()
{
	m_iAllocated = 0;
	m_iCount = 0;
	if (m_pPoints)
		free(m_pPoints);
	if (m_pRenderPos)
		free(m_pRenderPos);
	if (m_pRenderColor)
		free(m_pRenderColor);
	if (m_pRenderWidth)
		free(m_pRenderWidth);
	if (m_pRenderAlpha)
		free(m_pRenderAlpha);
	if (m_pRenderLength)
		free(m_pRenderLength);
	m_pPoints = NULL;
	m_pRenderColor = NULL;
	m_pRenderPos = NULL;
	m_pRenderAlpha = NULL;
	m_pRenderWidth = NULL;
	m_pRenderLength = NULL;
}

// orig client 0x5a7c0. Moves and fades the points, then rebuilds the arrays Draw hands the strip
// drawer; the live count
int CTentTrail::Update(float flFrametime)
{
	if (!(flFrametime > 0) || !m_pPoints)
		return m_iCount;

	for (int i = 0; i < m_iCount; i++)
	{
		tentpoint_t* p = &m_pPoints[i];
		if (!p->callback || 0 == p->callback(flFrametime, &p->pos, &p->vel, &p->accel, &p->color, &p->alpha, &p->width, &p->fade, p->user))
		{
			p = &m_pPoints[i];
			p->vel = p->accel * flFrametime + p->vel;
			p->pos = p->vel * flFrametime + p->pos;
			p->alpha = p->alpha - flFrametime * p->fade;
		}
		p = &m_pPoints[i];
		p->alpha = Clamp01(p->alpha);
	}

	CountAlive();

	m_iRenderCount = m_iCount;
	if (m_iRenderCount < 1)
		return m_iRenderCount;

	if (m_iRenderAllocated < m_iRenderCount)
	{
		if (m_pRenderPos)
			free(m_pRenderPos);
		if (m_pRenderColor)
			free(m_pRenderColor);
		if (m_pRenderWidth)
			free(m_pRenderWidth);
		if (m_pRenderAlpha)
			free(m_pRenderAlpha);
		if (m_pRenderLength)
			free(m_pRenderLength);
		m_pRenderPos = (Vector*)malloc(m_iRenderCount * sizeof(Vector));
		m_pRenderColor = (Vector*)malloc(m_iRenderCount * sizeof(Vector));
		m_pRenderWidth = (float*)malloc(m_iRenderCount * sizeof(float));
		m_pRenderAlpha = (float*)malloc(m_iRenderCount * sizeof(float));
		m_pRenderLength = (float*)malloc(m_iRenderCount * sizeof(float));
		m_iRenderAllocated = m_iRenderCount;
	}

	for (int i = 0; i < m_iCount; i++)
	{
		if (m_pRenderPos)
			m_pRenderPos[i] = m_pPoints[i].pos;
		if (m_pRenderColor)
			m_pRenderColor[i] = m_pPoints[i].color;
		if (m_pRenderWidth)
			m_pRenderWidth[i] = m_pPoints[i].width;
		if (m_pRenderAlpha)
			m_pRenderAlpha[i] = m_pPoints[i].alpha;
		m_pRenderLength[i] = 0;
	}

	return m_iCount;
}

// orig client 0x5ab80
bool CTentTrail::CanDraw()
{
	return m_pRenderPos && m_iRenderCount > 1;
}

// orig client 0x5abb0
void CTentTrail::Draw(bool bTexAlong)
{
	if (m_pRenderPos && m_iRenderCount > 1)
		CLTE_DrawStrip(m_pRenderPos, m_pRenderWidth, m_pRenderColor, m_pRenderAlpha, m_iRenderCount, bTexAlong, true, m_pRenderLength);
}

// orig client 0x5ac00. The trail with a head point before it (the moving thing the trail follows)
void CTentTrail::DrawToHead(Vector vecHead, Vector vecColor, float flAlpha, float flWidth, bool bTexAlong)
{
	if (m_pRenderPos && m_iRenderCount >= 1)
		CLTE_DrawStripHead(vecHead, flWidth, vecColor, flAlpha, m_pRenderPos, m_pRenderWidth, m_pRenderColor, m_pRenderAlpha,
			m_iRenderCount, bTexAlong, true, m_pRenderLength);
}

// orig client 0x5ac80. A new point at the head of the trail
void CTentTrail::AddPoint(Vector vecPos, Vector vecVel, Vector vecAccel, Vector vecColor, float flAlpha,
	float flWidth, float flFade, void* pUser, pfnTrailPoint pfnCallback)
{
	InsertFront();
	if (m_pPoints)
	{
		tentpoint_t* p = &m_pPoints[0];
		p->pos = vecPos;
		p->vel = vecVel;
		p->accel = vecAccel;
		p->color = vecColor;
		p->alpha = flAlpha;
		p->width = flWidth;
		p->fade = flFade;
		p->user = pUser;
		p->callback = pfnCallback;
	}
	CountAlive();
}

// orig client 0x5ad60
void CTentTrail::Reset()
{
	if (!m_pPoints)
		return;
	memset(m_pPoints, 0, m_iAllocated * sizeof(tentpoint_t));
	m_iCount = 0;
}

// orig client 0x5adb0. Room for iCount points, in steps of 8; the live ones kept
void CTentTrail::Grow(int iCount)
{
	if (m_iAllocated >= iCount)
		return;

	int iNew = m_iAllocated;
	do
	{
		iNew += 8;
	} while (iNew <= iCount);

	tentpoint_t* pOld = m_pPoints;
	m_iAllocated = iNew;
	m_pPoints = (tentpoint_t*)malloc(m_iAllocated * sizeof(tentpoint_t));
	memset(m_pPoints, 0, m_iAllocated * sizeof(tentpoint_t));
	if (pOld)
	{
		memcpy(m_pPoints, pOld, m_iCount * sizeof(tentpoint_t));
		free(pOld);
	}
}

// orig client 0x5ae70. The points still showing, counted from the head
void CTentTrail::CountAlive()
{
	m_iCount = 0;
	for (int i = 0; i < m_iAllocated; i++)
	{
		if (!m_pPoints)
			return;
		if (m_pPoints[i].alpha <= 0)
			return;
		m_iCount++;
	}
}

// orig client 0x5aec0. Every live point moved down one
void CTentTrail::InsertFront()
{
	CountAlive();
	Grow(m_iCount + 1);
	for (int i = m_iCount; i > 0; i--)
		m_pPoints[i] = m_pPoints[i - 1];
}

// --- CTentParticles ---

// orig client 0x5af10
CTentParticles::CTentParticles()
{
	m_iAllocated = 128;
	m_iCount = 0;
	m_pParticles = (tentparticle_t*)malloc(m_iAllocated * sizeof(tentparticle_t));
	memset(m_pParticles, 0, m_iAllocated * sizeof(tentparticle_t));
}

// orig client 0x5af80
CTentParticles::~CTentParticles()
{
	if (m_pParticles)
		free(m_pParticles);
	m_pParticles = NULL;
	m_iAllocated = 0;
}

// orig client 0x5afc0. Into the first dead slot, or a new one
void CTentParticles::Add(Vector vecPos, Vector vecVel, Vector vecAccel, Vector vecColor, float flAlpha, float flSize,
	float flFade, float flLife, void* pUser, pfnTentParticle pfnCallback)
{
	int i = 0;
	if (m_pParticles && m_iAllocated > 0)
	{
		for (; i < m_iAllocated; i++)
		{
			if (m_pParticles[i].life <= 0)
				break;
		}
	}

	Grow(i);

	tentparticle_t* p = &m_pParticles[i];
	p->pos = vecPos;
	p->vel = vecVel;
	p->accel = vecAccel;
	p->color = vecColor;
	p->alpha = flAlpha;
	p->size = flSize;
	p->fade = flFade;
	p->life = flLife;
	p->user = pUser;
	p->callback = pfnCallback;
}

// orig client 0x5b0f0. The live count
int CTentParticles::Update(float flFrametime)
{
	m_iCount = 0;
	for (int i = 0; i < m_iAllocated; i++)
	{
		tentparticle_t* p = &m_pParticles[i];
		if (!(p->life > 0))
			continue;

		m_iCount++;
		if (!p->callback || 0 == p->callback(flFrametime, &p->pos, &p->vel, &p->accel, &p->color, &p->alpha, &p->size, &p->fade, &p->life, p->user))
		{
			p = &m_pParticles[i];
			p->vel = p->accel * flFrametime + p->vel;
			p->pos = p->vel * flFrametime + p->pos;
			p->alpha = p->alpha - flFrametime * p->fade;
			p->life = p->life - flFrametime;
		}

		p = &m_pParticles[i];
		p->alpha = Clamp01(p->alpha);
		if (p->life < 0)
		{
			p->life = 0;
			m_iCount--;
		}
	}
	return m_iCount;
}

// orig client 0x5b370. Each live particle a quad facing the view, size out from its center
void CTentParticles::Draw()
{
	if (m_iCount <= 0 || !HasAlive())
		return;

	gEngfuncs.pTriAPI->Begin(TRI_QUADS);

	for (int i = 0; i < m_iAllocated; i++)
	{
		tentparticle_t* p = &m_pParticles[i];
		if (!(p->life > 0))
			continue;

		gEngfuncs.pTriAPI->Color4f(p->color.x, p->color.y, p->color.z, p->alpha);
		gEngfuncs.pTriAPI->Brightness(p->alpha);

		Vector v = (p->pos + g_vecTentUp * p->size) + g_vecTentRight * p->size;
		gEngfuncs.pTriAPI->TexCoord2f(0, 0);
		gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);

		v = (p->pos - g_vecTentUp * p->size) + g_vecTentRight * p->size;
		gEngfuncs.pTriAPI->TexCoord2f(1, 0);
		gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);

		v = (p->pos - g_vecTentRight * p->size) - g_vecTentUp * p->size;
		gEngfuncs.pTriAPI->TexCoord2f(1, 1);
		gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);

		v = (p->pos + g_vecTentUp * p->size) - g_vecTentRight * p->size;
		gEngfuncs.pTriAPI->TexCoord2f(0, 1);
		gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);
	}

	gEngfuncs.pTriAPI->End();
}

// orig client 0x5b9e0
bool CTentParticles::HasAlive()
{
	for (int i = 0; i < m_iAllocated; i++)
	{
		if (m_pParticles[i].life > 0)
			return true;
	}
	return false;
}

// orig client 0x5ba20. Room for index iIndex, in steps of 16
void CTentParticles::Grow(int iIndex)
{
	if (m_iAllocated > iIndex)
		return;

	const int iOld = m_iAllocated;
	int iNew = m_iAllocated;
	do
	{
		iNew += 16;
	} while (iNew <= iIndex);

	tentparticle_t* pOld = m_pParticles;
	m_iAllocated = iNew;
	m_pParticles = (tentparticle_t*)malloc(iNew * sizeof(tentparticle_t));
	memset(m_pParticles, 0, iNew * sizeof(tentparticle_t));
	if (pOld)
	{
		memcpy(m_pParticles, pOld, iOld * sizeof(tentparticle_t));
		free(pOld);
	}
}
