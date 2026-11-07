/***
*
*	Rocket Crowbar 2 client: the egon's imploder shot and implosion (CLTE 0x2d and 0x2e).
*
*	CLTE 0x2d is the imploder's shot: six sparks (sprites/glow01.spr with lights) that fly
*	from the gun along the aim and, once the shot has a target, home on it ([Egon]
*	ImploderTrackSpeed, TrackAccel, TrackingLife, ImplodeHitDist: the client's copies), each
*	with a smoke trail and three crackling beams. The server kills it with CLTE 0xff/0x2d
*	(shrinkerproject.cpp); who sends it isn't in the server (VERIFY: likely the client's egon
*	prediction).
*
*	CLTE 0x2e is the implosion around a hit entity (server shrinkerproject.cpp,
*	RC2_CLTE_IMPLODE: short entity): 250 orange flares (sprites/sqflare.spr) spiral in on
*	it while it shrinks (its curstate.scale goes from 1 to 0.05 in a second), then burst out.
*
*	The original's functions 0x788a0-0x7ba70 (and 0x9a320) of client.dll's run
*	"new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "rc2prefs.h"

#include <cmath>

// c3 needs:
// orig .bss 0x10210aa8 (owner: the client's RCCFG handler, outside c3): the client's copy of
// rc2.ini's NETWORK_SYNC keys, one 8-byte slot per key in the server's order, read through
// rc2prefs.h's RC2Pref() over the client's g_rc2PrefValues.

//
// CLTE 0x2d: coord x, y, z (the muzzle), short x, y, z (the aim, each /32767), short the
// target entity (0: none yet; it flies straight).
//
#define IMPLODER_SPARKS 6 // name ours

class CImploderShotTent : public CClientTent // name ours
{
public:
	CImploderShotTent();

	bool Think(float flFrametime) override;
	void DrawTransparent() override;
	bool EntityGone(int iEntIndex) override;

	Vector m_vecOrigin[IMPLODER_SPARKS];	 // orig +0x8
	Vector m_vecVelocity[IMPLODER_SPARKS];	 // orig +0x50
	Vector m_vecSteer[IMPLODER_SPARKS];		 // orig +0x98: the turn toward the target, added to the velocity each second
	BEAM* m_pBeams[IMPLODER_SPARKS][3];		 // orig +0xe0
	TEMPENTITY* m_pGlows[IMPLODER_SPARKS];	 // orig +0x128
	CRC2Trail m_Trails[IMPLODER_SPARKS];	 // orig +0x140
	dlight_t* m_pLights[IMPLODER_SPARKS];	 // orig +0x230
	dlight_t* m_pELights[IMPLODER_SPARKS];	 // orig +0x248
	Vector m_vecBeamEnds[3];				 // orig +0x260: in a spark's own axes, (-4, 10 around)
	float m_flRoll[IMPLODER_SPARKS];		 // orig +0x284
	float m_flRollSpeed[IMPLODER_SPARKS];	 // orig +0x29c: 160-180 degrees a second, either way
	bool m_bTracking[IMPLODER_SPARKS];		 // orig +0x2b4: still steering; false once it reached the target
	float m_flSpeed;						 // orig +0x2bc
	int m_iTarget;							 // orig +0x2c0
	float m_flLife;							 // orig +0x2c4: the fade-out once it stops tracking
	float m_flTrackLife;					 // orig +0x2c8
	float m_flTrailWait;					 // orig +0x2cc: time to the next trail point
	model_s* m_pBeamSprite;					 // orig +0x2d0: sprites/laserbeam.spr
};

// The factory (the dispatcher's case 0x2d; c3_tents.h). The copy is inline here.
CClientTent* RC2_NewImploderShotTent(const CClientTent* pCopy) // orig client 0x788a0
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CImploderShotTent* pTent = pCopy ? new CImploderShotTent(*static_cast<const CImploderShotTent*>(pCopy)) : new CImploderShotTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

CImploderShotTent::CImploderShotTent() // orig client 0x79eb0
{
	Vector vecSrc;
	vecSrc.x = READ_COORD();
	vecSrc.y = READ_COORD();
	vecSrc.z = READ_COORD();
	Vector vecDir;
	vecDir.x = READ_SHORT() * (1.0f / 32767);
	vecDir.y = READ_SHORT() * (1.0f / 32767);
	vecDir.z = READ_SHORT() * (1.0f / 32767);
	m_iTarget = READ_SHORT();

	Vector vecAngles, vecRight, vecUp;
	VectorAngles(vecDir, vecAngles);
	vecAngles.x = -vecAngles.x;
	gEngfuncs.pfnAngleVectors(vecAngles, nullptr, vecRight, vecUp);

	m_flSpeed = RC2Pref(RC2P_Egon_ImploderTrackSpeed).f;
	for (int i = 0; i < IMPLODER_SPARKS; i++)
	{
		m_bTracking[i] = true;
		m_pGlows[i] = nullptr;
		m_pLights[i] = nullptr;
		m_pELights[i] = nullptr;
		m_pBeams[i][0] = m_pBeams[i][1] = m_pBeams[i][2] = nullptr;
	}

	if (m_iTarget == 0)
	{
		m_flTrackLife = 0;
		m_flLife = RC2Pref(RC2P_Egon_ImploderTrackingLife).f;
	}
	else
	{
		m_flLife = 1.0;
		m_flTrackLife = RC2Pref(RC2P_Egon_ImploderTrackingLife).f;
	}

	int iModelIndex, iBeamIndex;
	model_s* pGlow = gEngfuncs.CL_LoadModel("sprites/glow01.spr", &iModelIndex);
	if (!gEngfuncs.CL_LoadModel("sprites/smoke.spr", &iModelIndex))
	{
		m_flLife = 0;
		m_iTarget = 0;
		return;
	}
	m_pBeamSprite = gEngfuncs.CL_LoadModel("sprites/laserbeam.spr", &iBeamIndex);

	float flAngle = 0;
	for (int j = 0; j < 3; j++)
	{
		m_vecBeamEnds[j].x = -4;
		m_vecBeamEnds[j].y = cos(flAngle) * 10.0;
		m_vecBeamEnds[j].z = sin(flAngle) * 10.0;
		flAngle += 2.0943952; // 2 pi / 3
	}

	if (g_iClDetail > 0)
		m_flTrailWait = 0.075;
	else
		m_flTrailWait = 0.1;

	for (int i = 0; i < IMPLODER_SPARKS; i++)
	{
		// without a target the sparks also spread sideways at 300
		if (m_iTarget != 0)
		{
			m_vecSteer[i] = Vector(0, 0, 0);
		}
		else
		{
			const float flUp = gEngfuncs.pfnRandomFloat(-1, 1);
			const float flRight = gEngfuncs.pfnRandomFloat(-1, 1);
			m_vecSteer[i] = vecRight * flRight + vecUp * flUp;
			m_vecSteer[i] = m_vecSteer[i].Normalize() * 300;
		}

		if (m_iTarget == 0)
			m_vecVelocity[i] = RC2_SpreadVector(vecDir, vecRight, vecUp, 0.5) * m_flSpeed;
		else
			m_vecVelocity[i] = RC2_SpreadVector(vecDir, vecRight, vecUp, 0.9) * m_flSpeed;

		m_vecOrigin[i] = vecSrc;

		if (g_iClDetail >= 0)
			m_Trails[i].AddPoint(m_vecOrigin[i], Vector(0, 0, 0), Vector(0, 0, 0), Vector(1.0, 0.8, 0.0), 0.7, 2.0, 0.7, 0, nullptr);

		if (pGlow)
		{
			m_pGlows[i] = gEngfuncs.pEfxAPI->CL_TempEntAlloc(m_vecOrigin[i], pGlow);
			if (m_pGlows[i])
			{
				m_pGlows[i]->flags |= FTENT_PERSIST;
				m_pGlows[i]->entity.curstate.scale = 0.2;
				m_pGlows[i]->entity.curstate.renderamt = 150;
				m_pGlows[i]->entity.curstate.rendermode = kRenderGlow;
				m_pGlows[i]->entity.curstate.renderfx = kRenderFxNoDissipation;
				m_pGlows[i]->entity.curstate.rendercolor.r = 255;
				m_pGlows[i]->entity.curstate.rendercolor.g = 200;
				m_pGlows[i]->entity.curstate.rendercolor.b = 0;
				m_pGlows[i]->die += 10;
			}
		}

		if (g_iClDetail >= 1)
		{
			m_pLights[i] = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
			if (m_pLights[i])
			{
				m_pLights[i]->origin = vecSrc;
				m_pLights[i]->radius = 32;
				m_pLights[i]->color.r = 255;
				m_pLights[i]->color.g = 200;
				m_pLights[i]->color.b = 0;
				m_pLights[i]->die = gEngfuncs.GetClientTime() + 10.0;
			}
			m_pELights[i] = gEngfuncs.pEfxAPI->CL_AllocElight(0);
			if (m_pELights[i])
			{
				m_pELights[i]->origin = vecSrc;
				m_pELights[i]->radius = 20;
				m_pELights[i]->color.r = 255;
				m_pELights[i]->color.g = 200;
				m_pELights[i]->color.b = 0;
				m_pELights[i]->die = gEngfuncs.GetClientTime() + 10.0;
			}
		}

		m_flRoll[i] = gEngfuncs.pfnRandomFloat(-180, 180);
		m_flRollSpeed[i] = gEngfuncs.pfnRandomFloat(160, 180);
		if (gEngfuncs.pfnRandomLong(0, 100) < 50)
			m_flRollSpeed[i] = m_flRollSpeed[i] * -1;

		for (int j = 0; j < 3; j++)
		{
			const float flFramerate = gEngfuncs.pfnRandomFloat(8, 10);
			m_pBeams[i][j] = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecOrigin[i], m_vecOrigin[i], iBeamIndex, 0.0, 0.7, 0.3, 1.0, 1.0, (int)gEngfuncs.pfnRandomFloat(0, 4), flFramerate, 1.0, 0.8, 0.0);
			if (m_pBeams[i][j])
				m_pBeams[i][j]->flags |= FBEAM_SHADEOUT;
		}
	}
}

// While tracking, each spark steers at the target's centre until it gets within
// ImplodeHitDist; once tracking is over the shot fades for a second and ends.
bool CImploderShotTent::Think(float flFrametime) // orig client 0x78c40
{
	float flFade = 1.0;
	Vector vecTarget(0, 0, 0);

	if (m_iTarget == 0 || m_flTrackLife <= 0)
	{
		m_flLife = m_flLife - flFrametime;
		if (m_flLife <= 0)
		{
			for (int i = 0; i < IMPLODER_SPARKS; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					if (m_pBeams[i][j])
					{
						m_pBeams[i][j]->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
						m_pBeams[i][j]->t = 1.0;
						m_pBeams[i][j]->die = 0.1;
						m_pBeams[i][j]->endEntity = 0;
						m_pBeams[i][j]->startEntity = 0;
						m_pBeams[i][j] = nullptr;
					}
				}
				if (m_pGlows[i])
				{
					m_pGlows[i]->die = 0;
					m_pGlows[i] = nullptr;
				}
				if (m_pLights[i])
				{
					m_pLights[i]->die = gEngfuncs.GetClientTime() + 0.01;
					m_pLights[i] = nullptr;
				}
				if (m_pELights[i])
				{
					m_pELights[i]->die = gEngfuncs.GetClientTime() + 0.01;
					m_pELights[i] = nullptr;
				}
			}
			return true;
		}
		if (m_flLife < 1.0)
			flFade = m_flLife;
		if (flFade < 0)
			flFade = 0;
		else if (flFade > 1)
			flFade = 1;
	}
	else
	{
		m_flLife = 1.0;
		m_flTrackLife = m_flTrackLife - flFrametime;
		int iTracking = 0;
		for (int i = 0; i < IMPLODER_SPARKS; i++)
		{
			if (m_bTracking[i])
				iTracking++;
		}
		if (iTracking == 0)
			m_flTrackLife = 0;

		cl_entity_t* pTarget = RC2_GetEntity(m_iTarget);
		if (!pTarget)
		{
			// clears m_iTarget, so the sparks below stop steering
			EntityGone(m_iTarget);
		}
		else
		{
			float flScale = 1.0;
			if (pTarget->curstate.scale != 0)
				flScale = pTarget->curstate.scale;
			const Vector vecMaxs = Vector(pTarget->curstate.maxs) * flScale;
			const Vector vecMins = Vector(pTarget->curstate.mins) * flScale;
			const Vector vecCentre = (vecMaxs + vecMins) * 0.5;
			vecTarget = vecCentre + pTarget->origin;
		}
	}

	for (int i = 0; i < IMPLODER_SPARKS; i++)
	{
		if (m_iTarget != 0)
		{
			const Vector vecDelta = vecTarget - m_vecOrigin[i];
			float flDist = vecDelta.Length();
			Vector vecDir;
			if (flDist == 0)
			{
				vecDir = Vector(0, 0, 1);
				flDist = 1.0;
			}
			else
			{
				vecDir = vecDelta / flDist;
			}

			if (flDist >= RC2Pref(RC2P_Egon_ImploderImplodeHitDist).f)
			{
				if (m_bTracking[i])
					m_vecSteer[i] = vecDir * RC2Pref(RC2P_Egon_ImploderTrackAccel).f;
			}
			else
			{
				if (m_bTracking[i])
				{
					m_vecVelocity[i] = vecDir * m_flSpeed;
					m_vecSteer[i] = Vector(0, 0, 0);
				}
				m_bTracking[i] = false;
			}
		}

		m_vecVelocity[i] = m_vecVelocity[i] + m_vecSteer[i] * flFrametime;
		m_vecVelocity[i] = m_vecVelocity[i].Normalize() * m_flSpeed;
		m_vecOrigin[i] = m_vecOrigin[i] + m_vecVelocity[i] * flFrametime;
		m_flRoll[i] = flFrametime * m_flRollSpeed[i] + m_flRoll[i];

		Vector vecAngles;
		VectorAngles(m_vecVelocity[i], vecAngles);
		vecAngles.z = m_flRoll[i];
		vecAngles.x = -vecAngles.x;
		Vector vecForward, vecRight, vecUp;
		gEngfuncs.pfnAngleVectors(vecAngles, vecForward, vecRight, vecUp);

		if (m_pGlows[i])
		{
			m_pGlows[i]->die = gEngfuncs.GetClientTime() + 10.0;
			m_pGlows[i]->entity.origin = m_vecOrigin[i];
			m_pGlows[i]->entity.curstate.renderamt = (int)(flFade * 150);
		}
		if (m_pLights[i])
		{
			m_pLights[i]->origin = m_vecOrigin[i];
			m_pLights[i]->die = gEngfuncs.GetClientTime() + 10.0;
			m_pLights[i]->radius = flFade * 32;
			if (m_pLights[i]->radius < 2)
				m_pLights[i]->radius = 2;
		}
		if (m_pELights[i])
		{
			m_pELights[i]->origin = m_vecOrigin[i];
			m_pELights[i]->die = gEngfuncs.GetClientTime() + 10.0;
			m_pELights[i]->radius = flFade * 20;
			if (m_pELights[i]->radius < 2)
				m_pELights[i]->radius = 2;
		}

		for (int j = 0; j < 3; j++)
		{
			BEAM* pBeam = m_pBeams[i][j];
			if (!pBeam)
				continue;
			pBeam->source = m_vecOrigin[i];
			pBeam->target = vecForward * m_vecBeamEnds[j].x + m_vecOrigin[i] + vecRight * m_vecBeamEnds[j].y + vecUp * m_vecBeamEnds[j].z;
			pBeam->delta = pBeam->target - pBeam->source;
			pBeam->brightness = flFade;
		}
	}

	if (g_iClDetail >= 0)
	{
		m_flTrailWait = m_flTrailWait - flFrametime;
		if (m_flTrailWait <= 0)
		{
			if (g_iClDetail == 0)
				m_flTrailWait = 0.1;
			else
				m_flTrailWait = 0.075;

			for (int i = 0; i < IMPLODER_SPARKS; i++)
			{
				m_Trails[i].AddPoint(m_vecOrigin[i], Vector(0, 0, 0), Vector(0, 0, 0), Vector(1.0, 0.8, 0.0), flFade * 0.7, 2.0, 0.7, 0, nullptr);
				m_Trails[i].Update(flFrametime);
			}
		}

		// a frame that added points updates the trails twice (kept)
		for (int i = 0; i < IMPLODER_SPARKS; i++)
			m_Trails[i].Update(flFrametime);
	}
	return false;
}

// The trails, led by each spark.
void CImploderShotTent::DrawTransparent() // orig client 0x79cd0
{
	if (g_iClDetail < 0)
		return;

	int i;
	for (i = 0; i < IMPLODER_SPARKS; i++)
	{
		if (m_Trails[i].CanDraw())
			break;
	}
	if (i >= IMPLODER_SPARKS)
		return;

	float flFade = 1.0;
	if (m_flLife < 1.0)
		flFade = m_flLife;
	if (flFade < 0)
		flFade = 0;
	else if (flFade > 1)
		flFade = 1;
	if (flFade <= 0)
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pBeamSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pBeamSprite, 0);

	const float flAlpha = flFade * 0.7;
	for (i = 0; i < IMPLODER_SPARKS; i++)
		m_Trails[i].DrawToHead(m_vecOrigin[i], Vector(1.0, 0.8, 0.0), flAlpha, 2.0, false);

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// The target is gone: the sparks fly on untracked. Never ends the effect.
bool CImploderShotTent::EntityGone(int iEntIndex) // orig client 0x79e80
{
	if (iEntIndex == m_iTarget)
		m_iTarget = 0;
	return false;
}

//
// CLTE 0x2e: short entity.
//
#define IMPLODE_FLARES 250 // name ours
#define IMPLODE_DRAWN 150  // name ours: the draw only goes this far (kept)

class CImplodeTent : public CClientTent // name ours
{
public:
	CImplodeTent();

	bool Think(float flFrametime) override;
	void DrawTransparent() override;
	bool EntityGone(int iEntIndex) override;

	void SpawnFlare(int iFlare);

	int m_iEntIndex;						 // orig +0x8
	float m_flScale;						 // orig +0xc: the entity's draw scale, 1 down to 0.05
	Vector m_vecOrigin;						 // orig +0x10
	Vector m_vecExtents;					 // orig +0x1c: the entity's half width and depth (+8) and height (72 for a player)
	Vector m_vecFlare[IMPLODE_FLARES];		 // orig +0x28
	Vector m_vecFlareVel[IMPLODE_FLARES];	 // orig +0xbe0
	Vector m_vecFlareAccel[IMPLODE_FLARES];	 // orig +0x1798
	float m_flFlareAlpha[IMPLODE_FLARES];	 // orig +0x2350
	bool m_bFlareAlive[IMPLODE_FLARES];		 // orig +0x2738
	Vector m_vecSpawnSize;					 // orig +0x2834: m_vecExtents * m_flScale
	Vector m_vecPrevSpawnSize;				 // orig +0x2840: last frame's
	float m_flSpin;							 // orig +0x284c: 2 turns a second
	float m_flPrevSpin;						 // orig +0x2850
	model_s* m_pSprite;						 // orig +0x2854: sprites/sqflare.spr
	float m_flSpawn;						 // orig +0x2858: flares owed
	bool m_bBurst;							 // orig +0x285c: shrunk all the way, burst out
	bool m_bVisible;						 // orig +0x285d
	bool m_bEnded;							 // orig +0x285e: the entity was killed (CLTE 0xff); ends when the flares are gone
};

// orig .data 0x10137214: flips each spawn, so every other flare starts on the opposite side.
// name ours
static bool s_bImplodeOtherSide = false;

// The factory (the dispatcher's case 0x2e; c3_tents.h). pCopy: a copy through the implicit
// copy constructor, out of line in the original (orig client 0x9a320).
CClientTent* RC2_NewImplodeTent(const CClientTent* pCopy) // orig client 0x7aa70
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CImplodeTent* pTent = pCopy ? new CImplodeTent(*static_cast<const CImplodeTent*>(pCopy)) : new CImplodeTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x7aa70).
CImplodeTent::CImplodeTent()
{
	m_iEntIndex = READ_SHORT();
	m_bVisible = true;
	m_bBurst = false;
	m_bEnded = false;
	for (int i = 0; i < IMPLODE_FLARES; i++)
		m_bFlareAlive[i] = false;
	m_flSpawn = 0;
	m_pSprite = nullptr;
	m_vecOrigin = Vector(0, 0, 0);
	m_flScale = 1.0;
	m_flPrevSpin = m_flSpin = gEngfuncs.pfnRandomFloat(0, 3.1415927);

	cl_entity_t* pEntity = RC2_GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_iEntIndex = 0;
		return;
	}

	m_vecOrigin = pEntity->origin;
	const float flMinX = fabs(pEntity->curstate.mins.x), flMaxX = fabs(pEntity->curstate.maxs.x);
	m_vecExtents.x = (flMinX > flMaxX ? flMinX : flMaxX) + 8;
	const float flMinY = fabs(pEntity->curstate.mins.y), flMaxY = fabs(pEntity->curstate.maxs.y);
	m_vecExtents.y = (flMinY > flMaxY ? flMinY : flMaxY) + 8;
	m_vecExtents.z = pEntity->curstate.maxs.z - pEntity->curstate.mins.z;
	if (pEntity->player)
		m_vecExtents.z = 72;

	int iModelIndex;
	m_pSprite = gEngfuncs.CL_LoadModel("sprites/sqflare.spr", &iModelIndex);
}

bool CImplodeTent::Think(float flFrametime) // orig client 0x7aec0
{
	if (!m_iEntIndex && !m_bEnded)
		return true;

	cl_entity_t* pEntity = nullptr;
	if (!m_bEnded)
	{
		pEntity = RC2_GetEntity(m_iEntIndex);
		if (!pEntity)
		{
			EntityGone(m_iEntIndex);
			m_iEntIndex = 0;
			return true;
		}
	}

	m_flPrevSpin = m_flSpin;
	m_vecPrevSpawnSize = m_vecSpawnSize;

	if (!m_bBurst)
	{
		m_bVisible = true;
		cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
		if (pLocal && pEntity && pEntity->curstate.messagenum < pLocal->curstate.messagenum)
		{
			m_bVisible = false;
			return m_bEnded;
		}
		if (pEntity)
			m_vecOrigin = pEntity->origin;

		m_flScale = m_flScale - flFrametime;
		if (m_flScale <= 0.05)
		{
			// shrunk away: the burst
			m_flScale = 0.05;
			m_bBurst = true;
			int iBurst;
			if (g_iClDetail >= 1)
				iBurst = 50;
			else if (g_iClDetail != 0)
				iBurst = 20;
			else
				iBurst = 30;

			for (int i = 0; i < IMPLODE_FLARES && iBurst > 0; i++)
			{
				if (m_bFlareAlive[i])
					continue;
				m_bFlareAlive[i] = true;
				iBurst--;
				m_flFlareAlpha[i] = 0.9;
				m_vecFlare[i] = m_vecOrigin;

				Vector vecDir;
				vecDir.x = gEngfuncs.pfnRandomFloat(-1, 1);
				vecDir.y = gEngfuncs.pfnRandomFloat(-1, 1);
				vecDir.z = gEngfuncs.pfnRandomFloat(-1, 1);
				vecDir = vecDir.Normalize();
				m_vecFlareVel[i] = vecDir * gEngfuncs.pfnRandomFloat(500, 600);
				// Vector / float, out of line in the original: orig client 0x7b7f0
				m_vecFlareAccel[i] = Vector(-m_vecFlareVel[i].x, -m_vecFlareVel[i].y, -m_vecFlareVel[i].z) / 1.5;
			}
		}

		m_flSpin = flFrametime * 12.566371 + m_flSpin; // 4 pi
		m_vecSpawnSize = m_vecExtents * m_flScale;

		if (g_iClDetail >= 1)
			m_flSpawn = flFrametime * 86.666667 + m_flSpawn;
		else if (g_iClDetail == 0)
			m_flSpawn = flFrametime * 60 + m_flSpawn;
		else
			m_flSpawn = flFrametime * 40 + m_flSpawn;
	}
	else
	{
		m_flSpawn = 0;
		m_flScale = 1.0;
	}

	if (pEntity)
		pEntity->curstate.scale = m_flScale;

	if (m_bEnded)
		m_flSpawn = 0;

	bool bAny = false;
	for (int i = 0; i < IMPLODE_FLARES; i++)
	{
		if (!m_bFlareAlive[i])
		{
			if (m_flSpawn < 1.0)
				continue;
			m_flSpawn = m_flSpawn - 1.0;

			// a new flare on the shrinking outline, spiralling in; it moves this frame too
			const float flFrac = gEngfuncs.pfnRandomFloat(0, 1);
			s_bImplodeOtherSide = !s_bImplodeOtherSide;
			float flAngle = (m_flSpin - m_flPrevSpin) * flFrac + m_flPrevSpin;
			if (s_bImplodeOtherSide)
				flAngle += 3.14159265358979;
			const float flCos = cos(flAngle);
			m_bFlareAlive[i] = true;
			m_flFlareAlpha[i] = 0.9;
			const float flSin = sin(flAngle);
			m_vecFlare[i].x = ((m_vecSpawnSize.x - m_vecPrevSpawnSize.x) * flFrac + m_vecPrevSpawnSize.x) * flCos;
			m_vecFlare[i].y = ((m_vecSpawnSize.y - m_vecPrevSpawnSize.y) * flFrac + m_vecPrevSpawnSize.y) * flSin;
			m_vecFlare[i].z = (m_vecSpawnSize.z - m_vecPrevSpawnSize.z) * flFrac + m_vecPrevSpawnSize.z;
			m_vecFlare[i] = Vector(m_vecOrigin.x + m_vecFlare[i].x, m_vecFlare[i].y + m_vecOrigin.y, m_vecFlare[i].z + m_vecOrigin.z);
			m_vecFlareVel[i] = Vector(0, 0, 0);
			const float flPull = gEngfuncs.pfnRandomFloat(50, 80);
			m_vecFlareAccel[i] = Vector(flPull * flCos, flPull * flSin, 0);
		}

		m_flFlareAlpha[i] = m_flFlareAlpha[i] - flFrametime * 0.6666667;
		if (m_flFlareAlpha[i] <= 0)
		{
			m_bFlareAlive[i] = false;
			m_flFlareAlpha[i] = 0;
			continue;
		}
		bAny = true;
		m_vecFlareVel[i].x = flFrametime * m_vecFlareAccel[i].x + m_vecFlareVel[i].x;
		m_vecFlareVel[i].y = flFrametime * m_vecFlareAccel[i].y + m_vecFlareVel[i].y;
		m_vecFlareVel[i].z = flFrametime * m_vecFlareAccel[i].z + m_vecFlareVel[i].z;
		m_vecFlare[i].x = flFrametime * m_vecFlareVel[i].x + m_vecFlare[i].x;
		m_vecFlare[i].y = flFrametime * m_vecFlareVel[i].y + m_vecFlare[i].y;
		m_vecFlare[i].z = flFrametime * m_vecFlareVel[i].z + m_vecFlare[i].z;
	}

	if (!bAny && m_bEnded)
		return true;

	// flares still owed go to random slots, live ones included
	while (m_flSpawn >= 1.0 && !m_bBurst && !m_bEnded)
	{
		m_flSpawn = m_flSpawn - 1.0;
		SpawnFlare(gEngfuncs.pfnRandomLong(0, IMPLODE_FLARES - 1));
	}
	return false;
}

// A new flare in slot iFlare, as the spawn in Think does.
void CImplodeTent::SpawnFlare(int iFlare) // orig client 0x7b840
{
	const float flFrac = gEngfuncs.pfnRandomFloat(0, 1);
	s_bImplodeOtherSide = !s_bImplodeOtherSide;
	float flAngle = (m_flSpin - m_flPrevSpin) * flFrac + m_flPrevSpin;
	if (s_bImplodeOtherSide)
		flAngle += 3.14159265358979;
	const float flCos = cos(flAngle);
	m_bFlareAlive[iFlare] = true;
	m_flFlareAlpha[iFlare] = 0.9;
	const float flSin = sin(flAngle);
	m_vecFlare[iFlare].x = ((m_vecSpawnSize.x - m_vecPrevSpawnSize.x) * flFrac + m_vecPrevSpawnSize.x) * flCos;
	m_vecFlare[iFlare].y = ((m_vecSpawnSize.y - m_vecPrevSpawnSize.y) * flFrac + m_vecPrevSpawnSize.y) * flSin;
	m_vecFlare[iFlare].z = (m_vecSpawnSize.z - m_vecPrevSpawnSize.z) * flFrac + m_vecPrevSpawnSize.z;
	m_vecFlare[iFlare].x = m_vecOrigin.x + m_vecFlare[iFlare].x;
	m_vecFlare[iFlare].y = m_vecOrigin.y + m_vecFlare[iFlare].y;
	m_vecFlare[iFlare].z = m_vecOrigin.z + m_vecFlare[iFlare].z;
	m_vecFlareVel[iFlare] = Vector(0, 0, 0);
	const float flPull = gEngfuncs.pfnRandomFloat(50, 80);
	m_vecFlareAccel[iFlare].x = flPull * flCos;
	m_vecFlareAccel[iFlare].y = flPull * flSin;
	m_vecFlareAccel[iFlare].z = 0;
}

bool CImplodeTent::EntityGone(int iEntIndex) // orig client 0x7ba40
{
	if (iEntIndex == m_iEntIndex)
	{
		m_iEntIndex = 0;
		m_bEnded = true;
	}
	return false;
}

// Each flare a view-facing quad 6 units across, orange.
void CImplodeTent::DrawTransparent() // orig client 0x7ba70
{
	if (!m_bVisible)
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pSprite, 0);
	gEngfuncs.pTriAPI->Begin(TRI_QUADS);

	for (int i = 0; i < IMPLODE_DRAWN; i++)
	{
		if (!m_bFlareAlive[i])
			continue;
		const float flAlpha = m_flFlareAlpha[i];
		if (flAlpha <= 0)
			continue;

		gEngfuncs.pTriAPI->Color4f(1.0, 0.6, 0, flAlpha);
		gEngfuncs.pTriAPI->Brightness(m_flFlareAlpha[i]);

		const Vector& vecPos = m_vecFlare[i];
		Vector vecCorner = (g_vecTentRight * 3 + vecPos) + g_vecTentUp * 3;
		gEngfuncs.pTriAPI->TexCoord2f(0, 0);
		gEngfuncs.pTriAPI->Vertex3f(vecCorner.x, vecCorner.y, vecCorner.z);

		vecCorner = vecPos + g_vecTentUp * 3 + g_vecTentRight * -3;
		gEngfuncs.pTriAPI->TexCoord2f(1, 0);
		gEngfuncs.pTriAPI->Vertex3f(vecCorner.x, vecCorner.y, vecCorner.z);

		vecCorner = vecPos + g_vecTentUp * -3 + g_vecTentRight * -3;
		gEngfuncs.pTriAPI->TexCoord2f(1, 1);
		gEngfuncs.pTriAPI->Vertex3f(vecCorner.x, vecCorner.y, vecCorner.z);

		vecCorner = vecPos + g_vecTentRight * 3 + g_vecTentUp * -3;
		gEngfuncs.pTriAPI->TexCoord2f(0, 1);
		gEngfuncs.pTriAPI->Vertex3f(vecCorner.x, vecCorner.y, vecCorner.z);
	}

	gEngfuncs.pTriAPI->End();
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}
