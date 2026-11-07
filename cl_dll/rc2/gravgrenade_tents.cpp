/***
*
*	Rocket Crowbar 2 client: the gravity grenade's effect (CLTE 0x32).
*
*	Named after its sound, weapons/gravgren.wav (no server file of this tree sends type 0x32
*	yet). While the entity lives: a glow (sprites/animglow01.spr) and a flare
*	(sprites/bigflare1.spr) pulsing on it, a pulsing dynamic and entity light, a beam straight up
*	384 units, an octagon of eight beams (radius 48) tumbling around it with sparks
*	(sprites/gradsmoke2.spr trails) flying off its corners, bubbles under water, and every few
*	tenths of a second a piece of debris thrown up out of it that falls under a gravity of 400
*	leaving a trail of sprites/gradsmoke.spr and stops where it hits something. Once the entity
*	is gone the debris and sparks finish before the effect ends.
*
*	The original's functions 0x7dee0-0x80cb0 of client.dll's run "new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"
#include "pmtrace.h"
#include "pm_defs.h"

#include <cmath>

// c3 needs:
// orig 0xdd80 ("new: start .. ev_hldm"): the height of the water surface above vecPosition
// between flMinZ and flMaxZ, by bisection on PM_PointContents (as UTIL_WaterLevel). name ours
float EV_WaterLevel(float* position, float minz, float maxz); // orig client 0xdd80 (rc2/ev_helpers.cpp)

#define GRAVGREN_SOUND "weapons/gravgren.wav"

class CGravGrenadeTent : public CClientTent // name ours
{
public:
	CGravGrenadeTent();

	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEntIndex) override;

	bool UpdateDebris(float flFrametime);
	void SpawnDebris();

	int m_iEntIndex;			  // orig +0x8
	bool m_bSeen;				  // orig +0xc: the entity was current on the last think
	bool m_bDead;				  // orig +0xd: the entity is gone (KillEntity)
	model_s* m_pDebrisSprite;	  // orig +0x10: sprites/gradsmoke.spr
	model_s* m_pSparkSprite;	  // orig +0x14: sprites/gradsmoke2.spr
	Vector m_vecOrigin;			  // orig +0x18
	BEAM* m_pBeam;				  // orig +0x24: the beam up
	TEMPENTITY* m_pGlow;		  // orig +0x28
	TEMPENTITY* m_pFlare;		  // orig +0x2c
	dlight_t* m_pDlight;		  // orig +0x30
	dlight_t* m_pElight;		  // orig +0x34
	float m_flPulse;			  // orig +0x38: the glows' phase
	float m_flNextShake;		  // orig +0x3c
	float m_flNextBubbles;		  // orig +0x40
	int m_iBubbleModel;			  // orig +0x44: sprites/bubble.spr's index
	Vector m_vecRing[9];		  // orig +0x48: the octagon's corners in its own frame, the first again last
	Vector m_vecCorner[8];		  // orig +0xb4: the corners in the world
	BEAM* m_pRingBeams[8];		  // orig +0x114
	CTentTrail m_Sparks[8];		  // orig +0x134: one per corner
	float m_flNextSparks;		  // orig +0x274
	Vector m_vecAngles;			  // orig +0x278: the octagon's
	Vector m_vecAVelocity;		  // orig +0x284
	float m_flNextDebrisTrace;	  // orig +0x290
	float m_flNextDebris;		  // orig +0x294
	int m_iDebrisCount;			  // orig +0x298: pieces still moving or fading
	Vector m_vecDebris[16];		  // orig +0x29c
	Vector m_vecDebrisPrev[16];	  // orig +0x35c: where each was at the last trace
	Vector m_vecDebrisVel[16];	  // orig +0x41c
	CTentTrail m_DebrisTrails[16]; // orig +0x4dc
	float m_flDebrisLife[16];	  // orig +0x75c: -1 flying, then the trail's fade time left, 0 unused
};

// The factory (the dispatcher's case 0x32; c3_tents.h). pCopy: a copy of that one, through the
// implicit copy constructor (inline in the original).
CClientTent* RC2_NewGravGrenadeTent(const CClientTent* pCopy) // orig client 0x7dee0
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CGravGrenadeTent* pTent = pCopy ? new CGravGrenadeTent(*static_cast<const CGravGrenadeTent*>(pCopy)) : new CGravGrenadeTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Reads the message: short entity.
CGravGrenadeTent::CGravGrenadeTent() // orig client 0x80cb0
{
	m_iEntIndex = READ_SHORT();
	m_vecOrigin = Vector(0, 0, 0);
	m_bSeen = false;
	m_flNextDebrisTrace = 0;
	m_pDebrisSprite = nullptr;
	m_pSparkSprite = nullptr;
	m_iDebrisCount = 0;
	m_flNextDebris = 0.1;
	m_flNextSparks = 0;
	m_bDead = false;
	m_flNextBubbles = 0.1;
	m_iBubbleModel = 0;
	m_flPulse = gEngfuncs.pfnRandomFloat(0, 6.2831855);
	m_flNextShake = gEngfuncs.pfnRandomFloat(0.1, 0.2);

	for (int i = 0; i < 16; i++)
		m_flDebrisLife[i] = 0;

	double flAngle = 0;
	for (int i = 0; i < 8; i++)
	{
		m_vecRing[i].x = cos(flAngle) * 48.0;
		m_vecRing[i].y = sin(flAngle) * 48.0;
		m_vecRing[i].z = 0;
		m_pRingBeams[i] = nullptr;
		m_vecCorner[i] = Vector(0, 0, 0);
		flAngle += 0.785398163f;
	}
	m_vecRing[8] = m_vecRing[0];

	const float flRoll = gEngfuncs.pfnRandomFloat(0, 360);
	const float flYaw = gEngfuncs.pfnRandomFloat(0, 360);
	const float flPitch = gEngfuncs.pfnRandomFloat(-90, 90);
	m_vecAngles = Vector(flPitch, flYaw, flRoll);

	float flSign = gEngfuncs.pfnRandomLong(0, 1) ? 1 : -1;
	m_vecAVelocity.x = gEngfuncs.pfnRandomFloat(200, 250) * flSign;
	flSign = gEngfuncs.pfnRandomLong(0, 1) ? 1 : -1;
	m_vecAVelocity.y = gEngfuncs.pfnRandomFloat(150, 200) * flSign;
	flSign = gEngfuncs.pfnRandomLong(0, 1) ? 1 : -1;
	m_vecAVelocity.z = gEngfuncs.pfnRandomFloat(100, 150) * flSign;

	// VERIFY: m_pBeam, m_pGlow, m_pFlare, m_pDlight and m_pElight are left unset when there is no
	// entity (harmless: nothing reads them then) and m_pBeam, m_pGlow, m_pFlare also when their
	// sprite doesn't load, which KillEntity and Update then dereference (an original bug, kept:
	// nothing sends CLTE 0x32, neither the server nor the client's loopback)
	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_bDead = true;
		m_iEntIndex = 0;
		return;
	}

	m_vecOrigin = pEntity->origin;
	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, m_vecOrigin, CHAN_WEAPON, GRAVGREN_SOUND, 1.0, 0.5, 0, 100);

	int iModelIndex;
	m_pDebrisSprite = gEngfuncs.CL_LoadModel("sprites/gradsmoke.spr", &iModelIndex);
	m_pSparkSprite = gEngfuncs.CL_LoadModel("sprites/gradsmoke2.spr", &iModelIndex);
	if (!gEngfuncs.CL_LoadModel("sprites/bubble.spr", &m_iBubbleModel))
		m_flNextBubbles = 9999;

	int iBeamModel;
	if (gEngfuncs.CL_LoadModel("sprites/laserbeam.spr", &iBeamModel))
	{
		for (int i = 0; i < 8; i++)
		{
			m_vecCorner[i] = m_vecOrigin;
			m_pRingBeams[i] = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecOrigin, m_vecOrigin, iBeamModel, 0, 2.0, 0.5, 0.95, 0, 0, 0, 0.9, 0.9, 1.0);
		}

		Vector vecTop(m_vecOrigin.x, m_vecOrigin.y, m_vecOrigin.z + 384);
		m_pBeam = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecOrigin, vecTop, iBeamModel, 0, 8.0, 0.2, 0.95, 0, 0, 0, 0.6, 0.6, 1.0);
		if (m_pBeam)
			m_pBeam->flags |= FBEAM_SHADEOUT;
	}

	model_s* pModel = gEngfuncs.CL_LoadModel("sprites/animglow01.spr", &iModelIndex);
	if (pModel)
	{
		m_pGlow = gEngfuncs.pEfxAPI->CL_TempEntAlloc(m_vecOrigin, pModel);
		if (m_pGlow)
		{
			m_pGlow->flags |= FTENT_SPRANIMATELOOP | FTENT_PERSIST | FTENT_SPRANIMATE;
			m_pGlow->entity.curstate.scale = 1.0;
			m_pGlow->entity.curstate.renderamt = 254;
			m_pGlow->entity.curstate.rendermode = kRenderGlow;
			m_pGlow->entity.curstate.renderfx = kRenderFxNoDissipation;
			m_pGlow->entity.curstate.rendercolor.r = 240;
			m_pGlow->entity.curstate.rendercolor.g = 240;
			m_pGlow->entity.curstate.rendercolor.b = 255;
			m_pGlow->entity.curstate.framerate = 20;
			m_pGlow->entity.curstate.frame = gEngfuncs.pfnRandomFloat(0, 6);
			m_pGlow->frameMax = 7;
			m_pGlow->die += 10;
		}
	}

	pModel = gEngfuncs.CL_LoadModel("sprites/bigflare1.spr", &iModelIndex);
	if (pModel)
	{
		m_pFlare = gEngfuncs.pEfxAPI->CL_TempEntAlloc(m_vecOrigin, pModel);
		if (m_pFlare)
		{
			m_pFlare->flags |= FTENT_PERSIST;
			m_pFlare->entity.curstate.scale = 2.0;
			m_pFlare->entity.curstate.renderamt = 180;
			m_pFlare->entity.curstate.rendermode = kRenderGlow;
			m_pFlare->entity.curstate.renderfx = kRenderFxNoDissipation;
			m_pFlare->entity.curstate.rendercolor.r = 225;
			m_pFlare->entity.curstate.rendercolor.g = 220;
			m_pFlare->entity.curstate.rendercolor.b = 255;
			m_pFlare->die += 10;
		}
	}

	m_pDlight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
	if (m_pDlight)
	{
		m_pDlight->origin = m_vecOrigin;
		m_pDlight->radius = 300;
		m_pDlight->color.r = 225;
		m_pDlight->color.g = 220;
		m_pDlight->color.b = 255;
		m_pDlight->die = gEngfuncs.GetClientTime() + 10.0;
	}

	m_pElight = gEngfuncs.pEfxAPI->CL_AllocElight(0);
	if (m_pElight)
	{
		m_pElight->origin = m_vecOrigin;
		m_pElight->radius = 200;
		m_pElight->color.r = 225;
		m_pElight->color.g = 220;
		m_pElight->color.b = 255;
		m_pElight->die = gEngfuncs.GetClientTime() + 10.0;
	}
}

// The original also has, at orig client 0x7fed0, the compiler-made default constructor of the
// pmtrace_t the debris trace uses (empty: its Vector members construct to nothing).

// The debris and the sparks: traces the flying debris (more often at higher cl_detail) and
// adds a point to each one's trail, moves it under gravity, ages the landed ones, and below
// cl_detail 1 nothing more; above, every 0.07 s a spark from each corner of the octagon flies
// outward at 300 while it lasts. True while any trail still has points. Inline in Update three
// times in the original, a call there once.
bool CGravGrenadeTent::UpdateDebris(float flFrametime) // orig client 0x7fef0
{
	m_flNextDebrisTrace -= flFrametime;
	if (m_flNextDebrisTrace <= 0)
	{
		if (g_iClientDetail < 0)
			m_flNextDebrisTrace = 0.15;
		else if (g_iClientDetail == 0)
			m_flNextDebrisTrace = 0.1;
		else
			m_flNextDebrisTrace = 0.075;

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
		gEngfuncs.pEventAPI->EV_PushPMStates();
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);

		pmtrace_t tr;
		for (int i = 0; i < 16; i++)
		{
			if (m_flDebrisLife[i] != -1)
				continue;

			m_DebrisTrails[i].AddPoint(m_vecDebris[i], Vector(0, 0, 0), Vector(0, 0, 0), Vector(0.7, 0.7, 1.0), 0.8, 3.0, 0.533333, 0, nullptr);
			gEngfuncs.pEventAPI->EV_PlayerTrace(m_vecDebrisPrev[i], m_vecDebris[i], PM_STUDIO_BOX, -1, &tr);
			m_vecDebrisPrev[i] = m_vecDebris[i];
			if (tr.fraction < 1.0)
				m_flDebrisLife[i] = 1.8;
		}

		gEngfuncs.pEventAPI->EV_PopPMStates();
	}

	bool bAny = false;
	m_iDebrisCount = 0;
	for (int i = 0; i < 16; i++)
	{
		if (m_flDebrisLife[i] == 0)
			continue;

		if (m_DebrisTrails[i].Update(flFrametime) > 0)
			bAny = true;
		m_iDebrisCount++;

		if (m_flDebrisLife[i] != -1)
		{
			m_flDebrisLife[i] -= flFrametime;
			if (m_flDebrisLife[i] < 0)
			{
				m_flDebrisLife[i] = 0;
				m_iDebrisCount--;
			}
		}
		else
		{
			m_vecDebrisVel[i] = m_vecDebrisVel[i] + Vector(0, 0, -400) * flFrametime;
			m_vecDebris[i] = m_vecDebris[i] + m_vecDebrisVel[i] * flFrametime;
		}
	}

	if (g_iClientDetail > 0)
	{
		if (!m_bDead)
		{
			m_flNextSparks -= flFrametime;
			if (m_flNextSparks <= 0)
			{
				m_flNextSparks = 0.07;
				for (int i = 0; i < 8; i++)
				{
					const Vector vecDir = (m_vecCorner[i] - m_vecOrigin).Normalize() * 300;
					m_Sparks[i].AddPoint(m_vecCorner[i], vecDir, Vector(0, 0, 0), Vector(0.9, 0.9, 1.0), 0.95, 2.0, 1.9, 0, nullptr);
				}
			}
		}

		for (int i = 0; i < 8; i++)
		{
			if (m_Sparks[i].Update(flFrametime) > 0)
				bAny = true;
		}
	}

	return bAny;
}

// A new piece of debris from the grenade, thrown up at 450-550 in a random slot (the search for
// a free one first goes unused: an original oddity, kept).
void CGravGrenadeTent::SpawnDebris() // orig client 0x805b0
{
	int iFree;
	for (iFree = 0; iFree < 16; iFree++)
	{
		if (m_flDebrisLife[iFree] == 0)
			break;
	}

	const int i = gEngfuncs.pfnRandomLong(0, 15);
	m_vecDebris[i] = m_vecOrigin;
	m_vecDebrisPrev[i] = m_vecOrigin;

	Vector vecDir;
	vecDir.x = gEngfuncs.pfnRandomFloat(-1, 1); // VERIFY: which of x and y takes the first draw
	vecDir.y = gEngfuncs.pfnRandomFloat(-1, 1);
	vecDir.z = gEngfuncs.pfnRandomFloat(1.3, 1.7);
	vecDir = vecDir.Normalize();
	m_vecDebrisVel[i] = vecDir * gEngfuncs.pfnRandomFloat(450, 550);

	m_flDebrisLife[i] = -1;
	m_DebrisTrails[i].Reset();
	m_DebrisTrails[i].AddPoint(m_vecDebris[i], Vector(0, 0, 0), Vector(0, 0, 0), Vector(0.7, 0.7, 1.0), 0.8, 3.0, 0.533333, 0, nullptr);
}

bool CGravGrenadeTent::Update(float flFrametime) // orig client 0x7e2e0
{
	if (!m_iEntIndex || m_bDead)
	{
		if (g_iClientDetail < 0)
			return true;
		return !(UpdateDebris(flFrametime) && m_bSeen);
	}

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		KillEntity(m_iEntIndex);
		return !(UpdateDebris(flFrametime) && m_bSeen);
	}

	if (m_pGlow)
		m_pGlow->die = gEngfuncs.GetClientTime() + 10;
	if (m_pFlare)
		m_pFlare->die = gEngfuncs.GetClientTime() + 10;
	if (m_pDlight)
		m_pDlight->die = gEngfuncs.GetClientTime() + 10;
	if (m_pElight)
		m_pElight->die = gEngfuncs.GetClientTime() + 10;
	m_bSeen = true;

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (pLocal)
	{
		// shake the view of a player within 768
		m_flNextShake -= flFrametime;
		if (m_flNextShake <= 0)
		{
			m_flNextShake = gEngfuncs.pfnRandomFloat(0.3, 0.5);
			const Vector vecDelta(pLocal->origin.x - m_vecOrigin.x, pLocal->origin.y - m_vecOrigin.y, pLocal->origin.z - m_vecOrigin.z);
			const float flDist = vecDelta.Length();
			if (flDist < 768)
			{
				float flFrac = 1.0 - flDist * 0.00130208f;
				if (flFrac < 0)
					flFrac = 0;
				else if (flFrac > 1)
					flFrac = 1;
				V_StartViewShake(flFrac * 1.5 + 0.5, 20, flFrac * 0.5, 0);
			}
		}

		// not in this update: let the debris finish and hide the rest
		if (pLocal->curstate.messagenum > pEntity->curstate.messagenum)
		{
			UpdateDebris(flFrametime);
			m_bSeen = false;
			const Vector vecAway(4096, 4096, 4096);
			if (m_pGlow)
				m_pGlow->entity.origin = vecAway;
			if (m_pFlare)
				m_pFlare->entity.origin = vecAway;
			if (m_pDlight)
				m_pDlight->origin = vecAway;
			if (m_pElight)
				m_pElight->origin = vecAway;
			return false;
		}
	}

	m_vecOrigin = pEntity->origin;

	// tumble the octagon
	const Vector vecTurn(flFrametime * m_vecAVelocity.x, flFrametime * m_vecAVelocity.y, flFrametime * m_vecAVelocity.z);
	m_vecAngles = Vector(vecTurn.x + m_vecAngles.x, vecTurn.y + m_vecAngles.y, vecTurn.z + m_vecAngles.z);
	Vector vecForward, vecRight, vecUp;
	gEngfuncs.pfnAngleVectors(m_vecAngles, vecForward, vecRight, vecUp);

	for (int i = 0; i < 8; i++)
	{
		BEAM* pBeam = m_pRingBeams[i];
		if (!pBeam)
			continue;

		const Vector vecStart = m_vecOrigin + vecForward * m_vecRing[i].x + vecRight * m_vecRing[i].y + vecUp * m_vecRing[i].z;
		m_vecCorner[i] = vecStart;
		pBeam->source = vecStart;
		const Vector vecEnd = m_vecOrigin + vecForward * m_vecRing[i + 1].x + vecRight * m_vecRing[i + 1].y + vecUp * m_vecRing[i + 1].z;
		pBeam->target = vecEnd;
		pBeam->delta = Vector(pBeam->target.x - pBeam->source.x, pBeam->target.y - pBeam->source.y, pBeam->target.z - pBeam->source.z);
	}

	// pulse the glow, the flare and the lights
	m_flPulse = flFrametime * 3.14159265358979 + m_flPulse;
	const float flPulse = sin(m_flPulse);
	if (m_pGlow)
		m_pGlow->entity.origin = m_vecOrigin;
	if (m_pFlare)
	{
		m_pFlare->entity.origin = m_vecOrigin;
		m_pFlare->entity.curstate.renderamt = (int)(flPulse * 20 + 160);
	}
	if (m_pDlight)
	{
		m_pDlight->origin = m_vecOrigin;
		m_pDlight->radius = flPulse * 10 + 290;
	}
	if (m_pElight)
	{
		m_pElight->origin = m_vecOrigin;
		m_pElight->radius = flPulse * 10 + 190;
	}

	// bubbles in a 64 cube around it, up to the surface, under water
	if (g_iClientDetail >= 0)
	{
		m_flNextBubbles -= flFrametime;
		if (m_flNextBubbles <= 0)
		{
			m_flNextBubbles = gEngfuncs.pfnRandomFloat(0.1, 0.15);
			if (gEngfuncs.PM_PointContents(m_vecOrigin, nullptr) == CONTENTS_WATER)
			{
				Vector vecMins, vecMaxs;
				const Vector vecOne(1, 1, 1);
				VectorMA(m_vecOrigin, -32, vecOne, vecMins);
				VectorMA(m_vecOrigin, 32, vecOne, vecMaxs);
				const float flHeight = EV_WaterLevel(m_vecOrigin, m_vecOrigin.z, m_vecOrigin.z + 1024) - vecMins.z;
				gEngfuncs.pEfxAPI->R_Bubbles(vecMins, vecMaxs, flHeight, m_iBubbleModel, gEngfuncs.pfnRandomLong(2, 5), 8);
			}
		}
	}

	m_flNextDebris -= flFrametime;
	if (m_flNextDebris <= 0)
	{
		// VERIFY: the original tests the timer itself here (< 0, nearly always), not cl_detail,
		// so the two shorter intervals are all but unreachable; kept
		if (m_flNextDebris < 0)
			m_flNextDebris = gEngfuncs.pfnRandomFloat(0.4, 0.6);
		else if (g_iClientDetail == 0)
			m_flNextDebris = gEngfuncs.pfnRandomFloat(0.2, 0.4);
		else
			m_flNextDebris = gEngfuncs.pfnRandomFloat(0.15, 0.35);
		SpawnDebris();
	}

	UpdateDebris(flFrametime);
	return false;
}

// The debris trails (each led by its piece while it flies) and the sparks. cl_detail -2 draws
// nothing, -3 no debris, -4 no sparks.
void CGravGrenadeTent::DrawTransparent() // orig client 0x80860
{
	if (!m_bSeen)
		return;
	if (m_iDebrisCount <= 0 && g_iClientDetail < 1)
		return;
	if (g_iClientDetail == -2)
		return;

	bool bDebris = true;
	bool bSparks = true;

	int i;
	for (i = 0; i < 16; i++)
	{
		if (m_DebrisTrails[i].CanDraw())
			break;
	}
	if (i >= 16)
	{
		if (g_iClientDetail <= 0)
			return;
		bDebris = false;
	}

	if (g_iClientDetail == -3)
	{
		bDebris = false;
	}
	else if (g_iClientDetail > 0 || g_iClientDetail == -4)
	{
		for (i = 0; i < 8; i++)
		{
			if (m_Sparks[i].CanDraw())
				break;
		}
		if (i >= 8)
		{
			if (!bDebris)
				return;
			bSparks = false;
		}
		if (g_iClientDetail == -4)
			bSparks = false;
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);

	if (m_pDebrisSprite && bDebris)
	{
		gEngfuncs.pTriAPI->SpriteTexture(m_pDebrisSprite, 0);
		for (i = 0; i < 16; i++)
		{
			if (m_flDebrisLife[i] == 0)
				continue;
			if (m_flDebrisLife[i] != -1)
				m_DebrisTrails[i].Draw(true);
			else
				m_DebrisTrails[i].DrawToHead(m_vecDebris[i], Vector(0.7, 0.7, 1.0), 0.8, 3.0, true);
		}
	}

	if (g_iClientDetail > 0 && m_pSparkSprite && bSparks)
	{
		gEngfuncs.pTriAPI->SpriteTexture(m_pSparkSprite, 0);
		for (i = 0; i < 8; i++)
		{
			if (!m_bDead)
				m_Sparks[i].DrawToHead(m_vecCorner[i], Vector(0.9, 0.9, 1.0), 0.95, 2.0, true);
			else
				m_Sparks[i].Draw(true);
		}
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// The grenade is gone: stop its sound, drop the glows, lights and beams. Always false, so the
// effect stays for its debris and sparks to finish.
bool CGravGrenadeTent::KillEntity(int iEntIndex) // orig client 0x80b50
{
	if (iEntIndex != m_iEntIndex)
		return false;

	gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, CHAN_WEAPON, GRAVGREN_SOUND);

	if (m_pGlow)
	{
		m_pGlow->die = 0;
		m_pGlow = nullptr;
	}
	if (m_pFlare)
	{
		m_pFlare->die = 0;
		m_pFlare = nullptr;
	}
	if (m_pDlight)
	{
		m_pDlight->die = gEngfuncs.GetClientTime() + 0.01;
		m_pDlight = nullptr;
	}
	if (m_pElight)
	{
		m_pElight->die = gEngfuncs.GetClientTime() + 0.01;
		m_pElight = nullptr;
	}

	// VERIFY: die = 0.1 is an absolute time (long past), as in the other effects' kills
	if (m_pBeam)
	{
		m_pBeam->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
		m_pBeam->t = 1.0;
		m_pBeam->die = 0.1;
		m_pBeam->endEntity = 0;
		m_pBeam->startEntity = 0;
		m_pBeam = nullptr;
	}
	for (int i = 0; i < 8; i++)
	{
		if (!m_pRingBeams[i])
			continue;
		m_pRingBeams[i]->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
		m_pRingBeams[i]->t = 1.0;
		m_pRingBeams[i]->die = 0.1;
		m_pRingBeams[i]->endEntity = 0;
		m_pRingBeams[i]->startEntity = 0;
		m_pRingBeams[i] = nullptr;
	}

	m_iEntIndex = 0;
	m_bDead = true;
	return false;
}
