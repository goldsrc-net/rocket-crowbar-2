/***
*
*	Rocket Crowbar 2 client: the suck grenade's pull point (CLTE 0x34).
*
*	Sent by the server's suck_grenade (suckgrenade.cpp, type 52) when it turns into a pull point,
*	and killed with CLTE 0xfe/52. On the entity: a heptagon of seven beams (radius 56) spinning
*	about the vertical with a glow (sprites/animglow01.spr) on each corner, two trails of
*	sprites/gradsmoke2.spr drawn by the tips of a second, counter-spinning 64-unit arm, and
*	ambience/alien_blipper.wav, all swelling in over the first quarter second and shrinking away
*	over the last 0.75 of its 10 seconds.
*
*	The original's functions 0x819a0-0x83520 of client.dll's run "new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"

#include <cmath>

#define SUCKGREN_SOUND "ambience/alien_blipper.wav"

#ifndef SND_CHANGE_VOL
#define SND_CHANGE_VOL (1 << 6) // as dlls/util.h
#define SND_CHANGE_PITCH (1 << 7)
#endif

class CSuckGrenadeTent : public CClientTent // name ours
{
public:
	CSuckGrenadeTent();

	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEntIndex) override;

	int m_iEntIndex;		   // orig +0x8
	bool m_bSeen;			   // orig +0xc: the entity was current on the last think
	model_s* m_pSprite;		   // orig +0x10: sprites/gradsmoke2.spr
	Vector m_vecOrigin;		   // orig +0x14
	float m_flLife;			   // orig +0x20: 10 s
	float m_flScale;		   // orig +0x24: 0-1, the size and brightness of it all
	CTentTrail m_Trails[2];	   // orig +0x28
	Vector m_vecTip[2];		   // orig +0x78: the arm's ends
	Vector m_vecArmAngles;	   // orig +0x90
	Vector m_vecArmAVelocity;  // orig +0x9c
	float m_flNextTrail;	   // orig +0xa8
	Vector m_vecAngles;		   // orig +0xac: the heptagon's
	Vector m_vecAVelocity;	   // orig +0xb8
	Vector m_vecRing[8];	   // orig +0xc4: the heptagon's corners in its own frame, the first again last
	Vector m_vecCorner[7];	   // orig +0x124: the corners in the world
	BEAM* m_pBeams[7];		   // orig +0x178
	TEMPENTITY* m_pGlows[7];   // orig +0x194
};

// The factory (the dispatcher's case 0x34; c3_tents.h). pCopy: a copy of that one, through the
// implicit copy constructor (inline in the original).
CClientTent* RC2_NewSuckGrenadeTent(const CClientTent* pCopy) // orig client 0x819a0
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CSuckGrenadeTent* pTent = pCopy ? new CSuckGrenadeTent(*static_cast<const CSuckGrenadeTent*>(pCopy)) : new CSuckGrenadeTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Reads the message: short entity.
CSuckGrenadeTent::CSuckGrenadeTent() // orig client 0x82f40
{
	m_iEntIndex = READ_SHORT();
	m_vecOrigin = Vector(0, 0, 0);
	m_vecTip[1] = Vector(0, 0, 0);
	m_bSeen = false;
	m_pSprite = nullptr;
	m_flScale = 0;
	m_flNextTrail = 0;
	m_flLife = 10;
	m_vecTip[0] = Vector(0, 0, 0);

	double flAngle = 0;
	for (int i = 0; i < 7; i++)
	{
		m_vecRing[i].x = cos(flAngle) * 56.0;
		m_vecRing[i].y = sin(flAngle) * 56.0;
		m_vecRing[i].z = 0;
		m_pBeams[i] = nullptr;
		m_pGlows[i] = nullptr;
		m_vecCorner[i] = Vector(0, 0, 0);
		flAngle += 0.897597901f; // 2 pi / 7
	}
	m_vecRing[7] = m_vecRing[0];

	m_vecAngles = Vector(0, gEngfuncs.pfnRandomFloat(0, 360), 0);
	const float flSign = gEngfuncs.pfnRandomLong(0, 1) ? 1 : -1;
	m_vecAVelocity = Vector(0, gEngfuncs.pfnRandomFloat(150, 200) * flSign, 0);

	// the arm turns the other way
	m_vecArmAngles = Vector(0, gEngfuncs.pfnRandomFloat(0, 360), 0);
	const int iArmSign = m_vecAVelocity.y < 0 ? 1 : -1;
	m_vecArmAVelocity = Vector(0, gEngfuncs.pfnRandomFloat(150, 200) * iArmSign, 0);

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_iEntIndex = 0;
		return;
	}

	m_vecOrigin = pEntity->origin;
	m_vecTip[1] = pEntity->origin;
	m_vecTip[0] = pEntity->origin;
	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, m_vecOrigin, CHAN_STATIC, SUCKGREN_SOUND, 0.1, 0.8, 0, 60);

	int iModelIndex;
	m_pSprite = gEngfuncs.CL_LoadModel("sprites/gradsmoke2.spr", &iModelIndex);

	for (int i = 0; i < 7; i++)
		m_vecCorner[i] = m_vecOrigin;

	model_s* pGlowModel = gEngfuncs.CL_LoadModel("sprites/animglow01.spr", &iModelIndex);
	int iBeamModel;
	if (!gEngfuncs.CL_LoadModel("sprites/laserbeam.spr", &iBeamModel))
		return;

	for (int i = 0; i < 7; i++)
	{
		m_pBeams[i] = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecOrigin, m_vecOrigin, iBeamModel, 0, 1.5, 0.5, 0.95, 0, 0, 0, 0.9, 0.9, 1.0);
		if (!pGlowModel)
			continue;

		m_pGlows[i] = gEngfuncs.pEfxAPI->CL_TempEntAlloc(m_vecOrigin, pGlowModel);
		TEMPENTITY* pGlow = m_pGlows[i];
		if (!pGlow)
			continue;

		pGlow->flags |= FTENT_SPRANIMATELOOP | FTENT_PERSIST | FTENT_SPRANIMATE;
		pGlow->entity.curstate.scale = 0.2;
		pGlow->entity.curstate.renderamt = 254;
		pGlow->entity.curstate.rendermode = kRenderTransAdd;
		pGlow->entity.curstate.renderfx = kRenderFxNoDissipation;
		pGlow->entity.curstate.rendercolor.r = 240;
		pGlow->entity.curstate.rendercolor.g = 240;
		pGlow->entity.curstate.rendercolor.b = 255;
		pGlow->entity.curstate.framerate = 22;
		pGlow->entity.curstate.frame = gEngfuncs.pfnRandomFloat(0, 6);
		pGlow->frameMax = 7;
		pGlow->die += 10;
	}
}

bool CSuckGrenadeTent::Update(float flFrametime) // orig client 0x81d00
{
	if (!m_iEntIndex)
		return true;

	cl_entity_t* pEntity = GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		KillEntity(m_iEntIndex);
		return true;
	}

	m_flLife -= flFrametime;
	if (m_flLife < 0)
	{
		KillEntity(m_iEntIndex);
		return true;
	}

	// VERIFY: once faded out in the last 0.75 s it jumps back to full (the last branch), as the
	// original does
	if (m_flLife > 0.75 && m_flScale < 1.0)
		m_flScale += flFrametime * 1.3333333333;
	else if (m_flLife <= 0.75 && m_flScale > 0)
		m_flScale = m_flScale - flFrametime * 1.3333333333;
	else
		m_flScale = 1.0;
	if (m_flScale < 0)
		m_flScale = 0;
	else if (m_flScale > 1)
		m_flScale = 1;

	for (int i = 0; i < 7; i++)
	{
		if (m_pGlows[i])
			m_pGlows[i]->die = gEngfuncs.GetClientTime() + 10;
	}
	m_bSeen = true;

	// not in this update: hide it
	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (pLocal && pLocal->curstate.messagenum > pEntity->curstate.messagenum)
	{
		for (int i = 0; i < 7; i++)
		{
			BEAM* pBeam = m_pBeams[i];
			if (pBeam)
			{
				pBeam->source = Vector(4096, 4096, 4096);
				pBeam->target = Vector(4095, 4095, 4095);
				pBeam->delta = Vector(pBeam->target.x - pBeam->source.x, pBeam->target.y - pBeam->source.y, pBeam->target.z - pBeam->source.z);
			}
			if (m_pGlows[i])
			{
				m_pGlows[i]->entity.origin = Vector(4096, 4096, 4096);
				m_pGlows[i]->entity.curstate.origin = Vector(4096, 4096, 4096);
			}
		}
		m_bSeen = false;
		return false;
	}

	m_vecOrigin = pEntity->origin;
	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, m_vecOrigin, CHAN_STATIC, SUCKGREN_SOUND, m_flScale * 0.8 + 0.2, 0.8, SND_CHANGE_VOL | SND_CHANGE_PITCH, (int)(m_flScale * 100 + 80));

	// spin the heptagon
	const Vector vecTurn(flFrametime * m_vecAVelocity.x, flFrametime * m_vecAVelocity.y, flFrametime * m_vecAVelocity.z);
	m_vecAngles = Vector(vecTurn.x + m_vecAngles.x, vecTurn.y + m_vecAngles.y, vecTurn.z + m_vecAngles.z);
	Vector vecForward, vecRight, vecUp;
	gEngfuncs.pfnAngleVectors(m_vecAngles, vecForward, vecRight, vecUp);

	for (int i = 0; i < 7; i++)
	{
		const Vector vecStart = m_vecOrigin + (vecForward * m_vecRing[i].x + vecRight * m_vecRing[i].y + vecUp * m_vecRing[i].z) * m_flScale;
		m_vecCorner[i] = vecStart;

		BEAM* pBeam = m_pBeams[i];
		if (pBeam)
		{
			pBeam->source = vecStart;
			const Vector vecEnd = m_vecOrigin + (vecForward * m_vecRing[i + 1].x + vecRight * m_vecRing[i + 1].y + vecUp * m_vecRing[i + 1].z) * m_flScale;
			pBeam->target = vecEnd;
			pBeam->delta = Vector(pBeam->target.x - pBeam->source.x, pBeam->target.y - pBeam->source.y, pBeam->target.z - pBeam->source.z);
			pBeam->brightness = m_flScale * 0.95;
		}

		TEMPENTITY* pGlow = m_pGlows[i];
		if (pGlow)
		{
			pGlow->entity.origin = m_vecCorner[i];
			pGlow->entity.curstate.origin = m_vecCorner[i];
			pGlow->entity.curstate.renderamt = (int)(m_flScale * 254);
		}
	}

	// the arm and its trails
	const Vector vecArmTurn(flFrametime * m_vecArmAVelocity.x, flFrametime * m_vecArmAVelocity.y, flFrametime * m_vecArmAVelocity.z);
	m_vecArmAngles = Vector(vecArmTurn.x + m_vecArmAngles.x, vecArmTurn.y + m_vecArmAngles.y, vecArmTurn.z + m_vecArmAngles.z);
	gEngfuncs.pfnAngleVectors(m_vecArmAngles, vecForward, vecRight, vecUp);

	const Vector vecArm = vecForward * 64 * m_flScale;
	m_vecTip[0] = vecArm + m_vecOrigin;
	m_vecTip[1] = m_vecOrigin - vecArm;

	m_flNextTrail -= flFrametime;
	if (m_flNextTrail <= 0)
	{
		if (g_iClientDetail < 0)
			m_flNextTrail = 0.1;
		else if (g_iClientDetail == 0)
			m_flNextTrail = 0.075;
		else
			m_flNextTrail = 0.05;

		m_Trails[0].AddPoint(m_vecTip[0], Vector(0, 0, 0), Vector(0, 0, 0), Vector(0.9, 0.9, 1.0), m_flScale * 0.95, 2.0, 1.266667, 0, nullptr);
		m_Trails[1].AddPoint(m_vecTip[1], Vector(0, 0, 0), Vector(0, 0, 0), Vector(0.9, 0.9, 1.0), m_flScale * 0.95, 2.0, 1.266667, 0, nullptr);
	}

	m_Trails[0].Update(flFrametime);
	m_Trails[1].Update(flFrametime);
	return false;
}

// The arm's two trails, each led by its tip.
void CSuckGrenadeTent::DrawTransparent() // orig client 0x82d10
{
	if (!m_bSeen || g_iClientDetail < 0)
		return;
	if (!m_Trails[0].CanDraw() && !m_Trails[1].CanDraw())
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pSprite, 0);

	m_Trails[0].DrawToHead(m_vecTip[0], Vector(0.9, 0.9, 1.0), m_flScale * 0.95, 2.0, true);
	m_Trails[1].DrawToHead(m_vecTip[1], Vector(0.9, 0.9, 1.0), m_flScale * 0.95, 2.0, true);

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// The pull point is gone: stop the sound, drop the beams and glows.
bool CSuckGrenadeTent::KillEntity(int iEntIndex) // orig client 0x82e80
{
	if (iEntIndex != m_iEntIndex)
		return false;

	gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, CHAN_STATIC, SUCKGREN_SOUND);

	for (int i = 0; i < 7; i++)
	{
		if (m_pBeams[i])
		{
			// VERIFY: die = 0.1 is an absolute time (long past)
			m_pBeams[i]->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
			m_pBeams[i]->t = 1.0;
			m_pBeams[i]->die = 0.1;
			m_pBeams[i]->endEntity = 0;
			m_pBeams[i]->startEntity = 0;
			m_pBeams[i] = nullptr;
		}
		if (m_pGlows[i])
		{
			m_pGlows[i]->die = 0;
			m_pGlows[i] = nullptr;
		}
	}

	m_iEntIndex = 0;
	return true;
}
