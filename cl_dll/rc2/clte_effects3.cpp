/***
*
*	Rocket Crowbar 2: client effects CLTE 13-20 (see clte.h): the glock's transphasic charge, the
*	python's charge and ball lightning, the MP5's three bolts, the shotgun crate's charge and the
*	HEV suit's flame.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "event_api.h"
#include "r_efx.h"
#include "com_model.h"
#include "triangleapi.h"
#include "pm_defs.h"
#include "clte.h"
#include "clte_types.h"

void VectorAngles(const float* forward, float* angles);

// From other client files (another package's; TEMP STUBs at the bottom)
int UTIL_PackNormal(Vector vecDir);							// client 0x3b400, as the server's (dlls/util.cpp)
Vector UTIL_ReflectVector(Vector vecDir, Vector vecNormal); // client 0x3b560, as the server's
int EV_RideTarget(int iEnt);								// client 0xdd40: the player iEnt rides, else iEnt. name ours

#define CLTE_SND_CHANGE_VOL 0x40   // SND_CHANGE_VOL
#define CLTE_SND_CHANGE_PITCH 0x80 // SND_CHANGE_PITCH

// orig client 0x65990: Vector's inline float* conversion, emitted out of line in this file (the
// bolts' event calls); the rebuild's Vector converts inline

// The point the local player's shots leave from: his eye, 26 out, 8 right, 8 down
static Vector Glock_Muzzle(int iEnt, const Vector& vecForward, const Vector& vecRight, const Vector& vecUp)
{
	Vector vecViewOfs;
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(vecViewOfs);
	return CLTE_GetOrigin(iEnt) + vecViewOfs + vecForward * 26 + vecRight * 8 - vecUp * 8;
}

// Lets a sprite go now (inlined in the original)
static void Tent_Release(TEMPENTITY* pTent)
{
	pTent->die = 0;
	pTent->flags &= ~FTENT_SPRANIMATELOOP;
}

// --- CLTE 13: the glock's transphasic charge (glock.cpp CLTE_CHARGEGLOW) ---

// vtable client 0x1011da14, 0x50 bytes
class CTentChargeGlow : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;			// +8
	float m_flLife;			// +0xc: [Glock] TransphasicChargeTime
	float m_flVolume;		// +0x10: the zap's, 0.5 to 1 over the charge
	float m_flPitch;		// +0x14: the zap's, 80 to 130
	BEAM* m_pBeams[3];		// +0x18: from the gun to points ahead of it
	Vector m_vecBeamEnd[3]; // +0x24: each beam's end in view space (forward, right, up)
	float m_flNextBeam;		// +0x48
	float m_flNextRing;		// +0x4c
};

// orig client 0x62d10. byte player. The copy goes through an out-of-line copy constructor (client
// 0x998f0); the beams only on the local player's own client
CClientTent* CLTE_CreateChargeGlow(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentChargeGlow*>(pCopy));

	CTentChargeGlow* p = new CTentChargeGlow;
	p->m_iEntity = READ_BYTE();
	if (0 != p->m_iEntity)
	{
		p->m_flLife = RC2Pref(RC2P_Glock_TransphasicChargeTime).f;
		p->m_pBeams[0] = NULL;
		p->m_pBeams[1] = NULL;
		p->m_pBeams[2] = NULL;

		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
		if (!pEnt)
		{
			p->m_iEntity = 0;
			g_pClientTents[iSlot] = p;
			return p;
		}

		p->m_flVolume = 0.5;
		p->m_flPitch = 80;
		p->m_flNextRing = gEngfuncs.pfnRandomFloat(0.2, 0.3);
		gEngfuncs.pEventAPI->EV_PlaySound(p->m_iEntity, pEnt->origin, CHAN_VOICE, "ambience/particle_suck1.wav", 0.8, 0.8, 0, 130);
		gEngfuncs.pEventAPI->EV_PlaySound(p->m_iEntity, pEnt->origin, CHAN_WEAPON, "ambience/zapmachine.wav", p->m_flVolume, 0.8, 0, (int)p->m_flPitch);

		if (0 != gEngfuncs.pEventAPI->EV_IsLocal(p->m_iEntity - 1))
		{
			Vector vecAngles, vecForward, vecRight, vecUp;
			gEngfuncs.GetViewAngles(vecAngles);
			gEngfuncs.pfnAngleVectors(vecAngles, vecForward, vecRight, vecUp);
			p->m_flNextBeam = gEngfuncs.pfnRandomFloat(0.15, 0.2);
			const Vector vecSrc = Glock_Muzzle(p->m_iEntity, vecForward, vecRight, vecUp);

			int iModel;
			if (gEngfuncs.CL_LoadModel("sprites/plasma.spr", &iModel))
			{
				for (int i = 0; i < 3; i++)
				{
					p->m_vecBeamEnd[i].x = gEngfuncs.pfnRandomFloat(-0.1, 0.1);
					p->m_vecBeamEnd[i].y = gEngfuncs.pfnRandomFloat(-1, 1);
					p->m_vecBeamEnd[i].z = gEngfuncs.pfnRandomFloat(-1, 1);
					const float flLen = gEngfuncs.pfnRandomFloat(4, 6);
					p->m_vecBeamEnd[i] = p->m_vecBeamEnd[i].Normalize() * flLen;

					const Vector vecOffset = vecForward * p->m_vecBeamEnd[i].x + vecRight * p->m_vecBeamEnd[i].y + vecUp * p->m_vecBeamEnd[i].z;
					Vector vecEnd = vecSrc + vecOffset;
					const int iFrame = (int)gEngfuncs.pfnRandomFloat(0, 2);
					p->m_pBeams[i] = gEngfuncs.pEfxAPI->R_BeamEntPoint(p->m_iEntity | 0x1000, vecEnd, iModel, 0, 0.3, 0.5, 0.5, 1, iFrame, 20, 1, 0, 0);
					if (p->m_pBeams[i])
						p->m_pBeams[i]->flags |= FBEAM_SHADEOUT;
				}
			}
		}
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x633a0. The zap rises in pitch and volume; on the local player's client the beams
// are redrawn from the gun (one jumps to a new point every 0.15-0.2 s, further as the charge grows)
// and, with cl_detail not negative, a red ring leaves the gun every 0.2-0.3 s
bool CTentChargeGlow::Update(float flFrametime)
{
	if (0 == m_iEntity)
		return true;

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iEntity);
	if (!pEnt)
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	m_flLife = m_flLife - flFrametime;
	if (m_flLife < 0)
	{
		KillEntity(m_iEntity);
		return true;
	}

	m_flVolume = flFrametime * (0.5 / RC2Pref(RC2P_Glock_TransphasicChargeTime).f) + m_flVolume;
	m_flPitch = flFrametime * (50.0 / RC2Pref(RC2P_Glock_TransphasicChargeTime).f) + m_flPitch;
	if (m_flVolume < 0)
		m_flVolume = 0;
	else if (m_flVolume > 1)
		m_flVolume = 1;
	if (m_flPitch < 80)
		m_flPitch = 80;
	else if (m_flPitch > 130)
		m_flPitch = 130;
	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntity, pEnt->origin, CHAN_WEAPON, "ambience/zapmachine.wav", m_flVolume, 0.8,
		CLTE_SND_CHANGE_VOL | CLTE_SND_CHANGE_PITCH, (int)m_flPitch);

	if (0 == gEngfuncs.pEventAPI->EV_IsLocal(m_iEntity - 1))
		return false;

	// VERIFY: the charge's progress assumes a 5 s charge (the default) whatever TransphasicChargeTime says
	float flCharge = (5.0 - m_flLife) * 0.2;
	Vector vecAngles, vecForward, vecRight, vecUp;
	gEngfuncs.GetViewAngles(vecAngles);
	gEngfuncs.pfnAngleVectors(vecAngles, vecForward, vecRight, vecUp);
	if (flCharge < 0)
		flCharge = 0;
	else if (flCharge > 1)
		flCharge = 1;

	Vector vecSrc = Glock_Muzzle(m_iEntity, vecForward, vecRight, vecUp);

	m_flNextRing = m_flNextRing - flFrametime;
	if (m_flNextRing < 0)
	{
		m_flNextRing = gEngfuncs.pfnRandomFloat(0.2, 0.3);
		if (g_iClientDetail >= 0)
		{
			CLTE_AddDiskRing(vecSrc, vecUp, vecRight, vecForward, 1, 0, 3, 1, 3, 1, 13, 4, 15,
				Vector(1, 0, 0), Vector(1, 0, 0), 0.3, 0, Vector(1, 0, 0), Vector(1, 0, 0), 0.9, 0.1,
				0.2, "sprites/shockwave.spr", 0, 0, 0);
		}
	}

	m_flNextBeam = m_flNextBeam - flFrametime;
	if (m_flNextBeam < 0)
	{
		m_flNextBeam = gEngfuncs.pfnRandomFloat(0.15, 0.2);
		const int i = gEngfuncs.pfnRandomLong(0, 2);
		m_vecBeamEnd[i].x = gEngfuncs.pfnRandomFloat(-0.1, 0.1);
		m_vecBeamEnd[i].y = gEngfuncs.pfnRandomFloat(-1, 1);
		m_vecBeamEnd[i].z = gEngfuncs.pfnRandomFloat(-1, 1);
		const Vector vecDir = m_vecBeamEnd[i].Normalize();
		const float flLen = gEngfuncs.pfnRandomFloat(flCharge * 6 + 4, flCharge * 8 + 6);
		m_vecBeamEnd[i] = vecDir * flLen;

		// Original bug kept: no NULL check (a crash if plasma.spr failed to load at Create)
		Beam_Kill(m_pBeams[i]);
		m_pBeams[i] = NULL;

		int iModel;
		if (gEngfuncs.CL_LoadModel("sprites/plasma.spr", &iModel))
		{
			Vector vecEnd = vecSrc + (vecForward * m_vecBeamEnd[i].x + vecRight * m_vecBeamEnd[i].y + vecUp * m_vecBeamEnd[i].z);
			const int iFrame = (int)gEngfuncs.pfnRandomFloat(0, 2);
			m_pBeams[i] = gEngfuncs.pEfxAPI->R_BeamEntPoint(m_iEntity | 0x1000, vecEnd, iModel, 0, 0.3, 0.5, 0.5, 1, iFrame, 20, 1, 0, 0);
			if (m_pBeams[i])
				m_pBeams[i]->flags |= FBEAM_SHADEOUT;
		}
	}

	for (int i = 0; i < 3; i++)
	{
		BEAM* pBeam = m_pBeams[i];
		if (!pBeam)
			continue;
		const Vector vecTarget = vecSrc + (vecForward * m_vecBeamEnd[i].x + vecRight * m_vecBeamEnd[i].y + vecUp * m_vecBeamEnd[i].z);
		VectorCopy(vecTarget, pBeam->target);
		pBeam->delta[0] = pBeam->target[0] - pBeam->source[0];
		pBeam->delta[1] = pBeam->target[1] - pBeam->source[1];
		pBeam->delta[2] = pBeam->target[2] - pBeam->source[2];
		pBeam->brightness = flCharge * 0.4 + 0.5;
		pBeam->width = flCharge * 0.6 + 0.3;
		pBeam->amplitude = flCharge * 0.2 + 0.5;
	}
	return false;
}

// orig client 0x64060
bool CTentChargeGlow::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_WEAPON, "ambience/zapmachine.wav");
	gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_VOICE, "ambience/particle_suck1.wav");
	for (int i = 0; i < 3; i++)
	{
		if (m_pBeams[i])
		{
			Beam_Kill(m_pBeams[i]);
			m_pBeams[i] = NULL;
		}
	}
	m_iEntity = 0;
	return true;
}

// --- CLTE 14: the python's charge, a glow on the player and his view model ---

// vtable client 0x1011da5c, 0x10 bytes
class CTentPythonCharge : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;	// +8
	float m_flLife; // +0xc: [Python] SharedChargeTime
};

// orig client 0x64120. byte player
CClientTent* CLTE_CreatePythonCharge(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentPythonCharge*>(pCopy));

	CTentPythonCharge* p = new CTentPythonCharge;
	p->m_iEntity = READ_BYTE();
	if (0 != p->m_iEntity)
	{
		p->m_flLife = RC2Pref(RC2P_Python_SharedChargeTime).f;
		if (!gEngfuncs.GetEntityByIndex(p->m_iEntity))
			p->m_iEntity = 0;
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x64300. The player's override color whitens to (230, 230, 255) as the charge fills,
// and the local view model glows the same
bool CTentPythonCharge::Update(float flFrametime)
{
	if (0 == m_iEntity)
		return true;

	if (!gEngfuncs.GetEntityByIndex(m_iEntity))
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	m_flLife = m_flLife - flFrametime;
	if (m_flLife < 0)
	{
		KillEntity(m_iEntity);
		return true;
	}

	float flCharge = 1.0 - m_flLife / RC2Pref(RC2P_Python_SharedChargeTime).f;
	if (flCharge < 0)
		flCharge = 0;
	else if (flCharge > 1)
		flCharge = 1;

	CLTE_SetPlayerOverride(m_iEntity, flCharge * 230, flCharge * 230, flCharge * 255, flCharge * 24);

	if (0 != gEngfuncs.pEventAPI->EV_IsLocal(m_iEntity - 1))
	{
		cl_entity_t* pView = gEngfuncs.GetViewModel();
		if (pView)
		{
			pView->curstate.renderfx = kRenderFxGlowShell;
			pView->curstate.renderamt = (int)(flCharge * 16);
			pView->curstate.rendercolor.b = (int)(flCharge * 255);
			pView->curstate.rendercolor.r = pView->curstate.rendercolor.g = (int)(flCharge * 230);
		}
	}
	return false;
}

// orig client 0x644d0
bool CTentPythonCharge::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	CLTE_ClearPlayerOverride(m_iEntity);
	if (0 != gEngfuncs.pEventAPI->EV_IsLocal(m_iEntity - 1))
	{
		cl_entity_t* pView = gEngfuncs.GetViewModel();
		if (pView)
		{
			pView->curstate.renderfx = kRenderFxNone;
			pView->curstate.renderamt = 0;
			pView->curstate.rendercolor.b = 0;
			pView->curstate.rendercolor.g = 0;
			pView->curstate.rendercolor.r = 0;
		}
	}
	m_iEntity = 0;
	return true;
}

// --- CLTE 15: the python's ball lightning (balllightning.cpp CLTE_BALLLIGHTNING) ---

// vtable client 0x1011da84, 0x60 bytes
class CTentBallLightning : public CClientTent
{
public:
	CTentBallLightning();
	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;			 // +8
	float m_flLife;			 // +0xc: [Python] BallMaxLife
	TEMPENTITY* m_pFlare;	 // +0x10: sprites/blflare.spr
	TEMPENTITY* m_pBigFlare; // +0x14: sprites/bigflare1.spr
	dlight_t* m_pLight;		 // +0x18
	dlight_t* m_pELight;	 // +0x1c
	Vector m_vecForward;	 // +0x20: the spinning ring's axes
	Vector m_vecRight;		 // +0x2c
	Vector m_vecUp;			 // +0x38
	Vector m_vecOrigin;		 // +0x44
	Vector m_vecAngles;		 // +0x50: 45 down, spinning in yaw
	model_s* m_pRingModel;	 // +0x5c: sprites/shockwave.spr
};

// orig client 0x64c50. short ball. Original bug kept: with cl_detail negative the lights, and
// without blflare.spr the big flare, are left unset and later read
CTentBallLightning::CTentBallLightning()
{
	m_iEntity = READ_SHORT();
	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iEntity);
	if (!pEnt)
	{
		m_iEntity = 0;
		return;
	}

	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntity, pEnt->origin, CHAN_STATIC, "weapons/blfly.wav", 0.5, 0.8, CLTE_SND_CHANGE_PITCH, 100);
	m_flLife = RC2Pref(RC2P_Python_BallMaxLife).f;
	m_pFlare = NULL;
	m_vecOrigin = pEnt->origin;
	m_vecAngles.x = 45;
	m_vecAngles.y = gEngfuncs.pfnRandomFloat(0, 360);
	m_vecAngles.z = 0;
	m_vecUp = Vector(0, 0, 0);
	m_vecRight = Vector(0, 0, 0);
	m_vecForward = Vector(0, 0, 0);

	if (g_iClientDetail >= 0)
	{
		m_pLight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		if (m_pLight)
		{
			VectorCopy(pEnt->origin, m_pLight->origin);
			m_pLight->radius = 160;
			m_pLight->color.r = 230;
			m_pLight->color.g = 230;
			m_pLight->color.b = 255;
			m_pLight->die = m_flLife * 1.5 + gEngfuncs.GetClientTime();
		}
		m_pELight = gEngfuncs.pEfxAPI->CL_AllocElight(0);
		if (m_pELight)
		{
			VectorCopy(pEnt->origin, m_pELight->origin);
			m_pELight->radius = 100;
			m_pELight->color.r = 230;
			m_pELight->color.g = 230;
			m_pELight->color.b = 255;
			m_pELight->die = m_flLife * 1.5 + gEngfuncs.GetClientTime();
		}
	}

	int iModel;
	model_s* pSprite = gEngfuncs.CL_LoadModel("sprites/blflare.spr", &iModel);
	if (!pSprite)
		return;
	m_pFlare = gEngfuncs.pEfxAPI->CL_TempEntAlloc(pEnt->origin, pSprite);
	if (!m_pFlare)
		return;
	m_pFlare->flags |= FTENT_SPRANIMATELOOP | FTENT_PERSIST | FTENT_SPRANIMATE;
	m_pFlare->entity.curstate.framerate = 30;
	m_pFlare->entity.curstate.scale = 1.4;
	m_pFlare->entity.curstate.renderamt = 240;
	m_pFlare->entity.curstate.rendermode = kRenderGlow;
	m_pFlare->entity.curstate.renderfx = kRenderFxNoDissipation;
	m_pFlare->entity.curstate.rendercolor.b = 255;
	m_pFlare->entity.curstate.rendercolor.r = 230;
	m_pFlare->entity.curstate.rendercolor.g = 230;
	m_pFlare->entity.curstate.frame = gEngfuncs.pfnRandomFloat(0, 6);
	m_pFlare->frameMax = 7;
	m_pFlare->die = m_flLife * 1.5 + m_pFlare->die;

	pSprite = gEngfuncs.CL_LoadModel("sprites/bigflare1.spr", &iModel);
	if (!pSprite)
		return;
	m_pBigFlare = gEngfuncs.pEfxAPI->CL_TempEntAlloc(pEnt->origin, pSprite);
	if (!m_pBigFlare)
		return;
	m_pBigFlare->flags |= FTENT_PERSIST;
	m_pBigFlare->entity.curstate.scale = 2.5;
	m_pBigFlare->entity.curstate.renderamt = 120;
	m_pBigFlare->entity.curstate.rendermode = kRenderGlow;
	m_pBigFlare->entity.curstate.renderfx = kRenderFxNoDissipation;
	m_pBigFlare->entity.curstate.rendercolor.b = 200;
	m_pBigFlare->entity.curstate.rendercolor.g = 200;
	m_pBigFlare->entity.curstate.rendercolor.r = 255;
	m_pBigFlare->die = m_flLife * 1.5 + m_pBigFlare->die;
	m_pRingModel = gEngfuncs.CL_LoadModel("sprites/shockwave.spr", &iModel);
}

// orig client 0x64560
CClientTent* CLTE_CreateBallLightning(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentBallLightning*>(pCopy));

	CTentBallLightning* p = new CTentBallLightning;
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x64780. The flares and lights follow the ball and its ring spins two turns a
// second; done when it fizzles or goes out of the local player's packets
bool CTentBallLightning::Update(float flFrametime)
{
	if (0 == m_iEntity)
		return true;

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iEntity);
	if (!pEnt)
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal || pLocal->curstate.messagenum > pEnt->curstate.messagenum)
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	if (m_pFlare)
		m_pFlare->entity.origin = pEnt->origin;
	if (m_pBigFlare)
		m_pBigFlare->entity.origin = pEnt->origin;
	if (m_pLight)
		VectorCopy(pEnt->origin, m_pLight->origin);
	if (m_pELight)
		VectorCopy(pEnt->origin, m_pELight->origin);

	m_vecAngles.y = flFrametime * 720 + m_vecAngles.y;
	if (m_vecAngles.y > 360)
		m_vecAngles.y = m_vecAngles.y - 360;
	gEngfuncs.pfnAngleVectors(m_vecAngles, m_vecForward, m_vecRight, m_vecUp);
	m_vecOrigin = pEnt->origin;
	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntity, pEnt->origin, CHAN_STATIC, "weapons/blfly.wav", 0.5, 0.8, CLTE_SND_CHANGE_PITCH, 100);

	m_flLife = m_flLife - flFrametime;
	if (m_flLife >= 0)
		return false;

	if (m_pFlare)
	{
		Tent_Release(m_pFlare);
		m_pFlare = NULL;
	}
	if (m_pBigFlare)
	{
		Tent_Release(m_pBigFlare);
		m_pBigFlare = NULL;
	}
	if (m_pLight)
		m_pLight->die = gEngfuncs.GetClientTime() + 0.01;
	if (m_pELight)
		m_pELight->die = gEngfuncs.GetClientTime() + 0.01;
	gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_STATIC, "weapons/blfly.wav");
	m_iEntity = 0;
	return true;
}

// orig client 0x64b20. With cl_detail not negative, a spinning white ring round the ball
void CTentBallLightning::DrawTransparent()
{
	if (g_iClientDetail < 0)
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pRingModel)
		gEngfuncs.pTriAPI->SpriteTexture(m_pRingModel, 0);
	CLTE_DrawRing(m_vecOrigin, m_vecUp, m_vecRight, m_vecForward, 32, 48, 0, 0, 3,
		Vector(0.95, 0.95, 1), 0, Vector(0.95, 0.95, 1), 0.8);
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// orig client 0x64a50
bool CTentBallLightning::KillEntity(int iEnt)
{
	if (m_iEntity != iEnt)
		return false;

	if (m_pFlare)
	{
		Tent_Release(m_pFlare);
		m_pFlare = NULL;
	}
	if (m_pBigFlare)
	{
		Tent_Release(m_pBigFlare);
		m_pBigFlare = NULL;
	}
	if (m_pLight)
		m_pLight->die = gEngfuncs.GetClientTime() + 0.01;
	if (m_pELight)
		m_pELight->die = gEngfuncs.GetClientTime() + 0.01;
	gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_STATIC, "weapons/blfly.wav");
	m_iEntity = 0;
	return true;
}

// --- CLTE 16-18: the MP5's bolts, flown on the client ---
//
// Each is byte shooter, short bolt id, short ms of head start, coord x3 start, short x3 direction
// (in 1/32767). CLTE 0xfd (type, shooter, id) ends one. They trace from their last point every
// 0.05 s and play their hit event on the world.

// The bolts' base (vtable client 0x1011db04): CClientTentCmd whose command does nothing
class CTentBoltBase : public CClientTentCmd
{
public:
	bool Command(int iCmd, int iValue) override;
};

// orig client 0x663b0
bool CTentBoltBase::Command(int iCmd, int iValue)
{
	return false;
}

// The head start a bolt flies this frame: what is left of it, at most 1 s and at most two frames
static float Bolt_HeadStart(float& flHeadStart, float flFrametime)
{
	float flExtra = 0;
	if (flHeadStart > 0)
	{
		flExtra = flHeadStart;
		if (flExtra < 0)
			flExtra = 0;
		else if (flExtra > 1)
			flExtra = 1;
		if (flFrametime + flFrametime < flExtra)
			flExtra = flFrametime + flFrametime;
		flHeadStart = flHeadStart - flExtra;
	}
	return flExtra;
}

// A trace from start to end past the shooter (and whoever he rides), as the original sets it up
static void Bolt_Trace(int iShooter, float* vecStart, float* vecEnd, pmtrace_t* ptr)
{
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(EV_RideTarget(iShooter) - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_NORMAL, iShooter, ptr);
	gEngfuncs.pEventAPI->EV_PopPMStates();
}

// The hit event, 4 units off the surface, with the reversed normal packed in iparam1
static void Bolt_HitEvent(unsigned short usEvent, pmtrace_t& tr, int iShooter)
{
	Vector vecNormal = tr.plane.normal;
	const int iPacked = UTIL_PackNormal(-vecNormal);
	Vector vecAngles(0, 0, 0);
	Vector vecOrigin = Vector(tr.endpos) + vecNormal * 4;
	gEngfuncs.pfnPlaybackEvent(0, NULL, usEvent, 0, vecOrigin, vecAngles, 0, 0, iPacked, 0, 1, iShooter);
}

// Whether a physent is the world (a BSP model or a pushstep mover)
static bool Bolt_IsWorld(physent_t* pe)
{
	return pe && (pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP);
}

// The bolts' shape: three crossed quads 48 long and 8 across along their axes. The original writes
// the first quad out (its first vertex with a zero vector for up * 0) and the rest through Vertex
template <class T>
static void Bolt_DrawQuads(T* p)
{
	Vector v = p->m_vecOrigin + p->m_vecForward * 24 + p->m_vecRight * 4 + Vector(0, 0, 0);
	gEngfuncs.pTriAPI->TexCoord2f(0, 0);
	gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);
	v = p->m_vecOrigin + p->m_vecForward * -24 + p->m_vecRight * 4 + p->m_vecUp * 0;
	gEngfuncs.pTriAPI->TexCoord2f(1, 0);
	gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);
	v = p->m_vecOrigin + p->m_vecForward * -24 + p->m_vecRight * -4 + p->m_vecUp * 0;
	gEngfuncs.pTriAPI->TexCoord2f(1, 1);
	gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);
	v = p->m_vecOrigin + p->m_vecForward * 24 + p->m_vecRight * -4 + p->m_vecUp * 0;
	gEngfuncs.pTriAPI->TexCoord2f(0, 1);
	gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);

	v = p->m_vecOrigin + p->m_vecForward * 24 + p->m_vecRight * 0 + p->m_vecUp * 4;
	gEngfuncs.pTriAPI->TexCoord2f(0, 0);
	gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);
	p->Vertex(-24, 0, 4, 1, 0);
	p->Vertex(-24, 0, -4, 1, 1);
	p->Vertex(24, 0, -4, 0, 1);

	p->Vertex(0, 4, 4, 0, 0);
	p->Vertex(0, -4, 4, 1, 0);
	p->Vertex(0, -4, -4, 1, 1);
	p->Vertex(0, 4, -4, 0, 1);
}

// Reads the bolt message shared by CLTE 16-18 into the fields they share
template <class T>
static void Bolt_Read(T* p)
{
	p->m_iEntity = READ_BYTE();
	p->m_iBolt = READ_SHORT();
	p->m_flHeadStart = READ_SHORT() * 0.001;
	p->m_vecOrigin.x = READ_COORD();
	p->m_vecOrigin.y = READ_COORD();
	p->m_vecOrigin.z = READ_COORD();
	p->m_vecDir.x = READ_SHORT() * 3.051851e-05;
	p->m_vecDir.y = READ_SHORT() * 3.051851e-05;
	p->m_vecDir.z = READ_SHORT() * 3.051851e-05;
	p->m_vecLastTrace = p->m_vecOrigin;
}

// --- CLTE 16: the MP5's primary bolt ---

// vtable client 0x1011dacc, 0x70 bytes
class CTentMP5Bolt : public CTentBoltBase
{
public:
	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEnt) override;
	bool Command(int iCmd, int iValue) override;
	void Vertex(float flForward, float flRight, float flUp, float s, float t);

	int m_iEntity;		   // +8: the shooter
	int m_iBolt;		   // +0xc
	Vector m_vecOrigin;	   // +0x10
	Vector m_vecDir;	   // +0x1c
	Vector m_vecLastTrace; // +0x28
	dlight_t* m_pLight;	   // +0x34
	dlight_t* m_pELight;   // +0x38
	model_s* m_pSprite;	   // +0x3c: sprites/blflare.spr
	Vector m_vecForward;   // +0x40
	Vector m_vecRight;	   // +0x4c
	Vector m_vecUp;		   // +0x58
	float m_flFrame;	   // +0x64
	float m_flNextTrace;   // +0x68
	float m_flHeadStart;   // +0x6c
};

// orig client 0x650a0. The copy goes through an out-of-line copy constructor (client 0x999b0)
CClientTent* CLTE_CreateMP5Bolt(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentMP5Bolt*>(pCopy));

	CTentMP5Bolt* p = new CTentMP5Bolt;
	Bolt_Read(p);
	p->m_flNextTrace = 0;
	p->m_flFrame = gEngfuncs.pfnRandomFloat(0, 6);
	p->m_pLight = NULL;
	p->m_pELight = NULL;
	p->m_pSprite = NULL;

	if (0 != p->m_iEntity)
	{
		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
		if (!pEnt)
		{
			p->m_iEntity = 0;
			g_pClientTents[iSlot] = p;
			return p;
		}

		Vector vecAngles;
		VectorAngles(p->m_vecDir, vecAngles);
		vecAngles.x = -vecAngles.x;
		gEngfuncs.pfnAngleVectors(vecAngles, p->m_vecForward, p->m_vecRight, p->m_vecUp);

		if (g_iClientDetail > 0)
		{
			p->m_pLight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
			if (p->m_pLight)
			{
				VectorCopy(pEnt->origin, p->m_pLight->origin);
				p->m_pLight->radius = 64;
				p->m_pLight->color.r = 255;
				p->m_pLight->color.g = 30;
				p->m_pLight->color.b = 30;
				p->m_pLight->die = gEngfuncs.GetClientTime() + 45.0;
			}
		}
		if (g_iClientDetail >= 0)
		{
			p->m_pELight = gEngfuncs.pEfxAPI->CL_AllocElight(0);
			if (p->m_pELight)
			{
				VectorCopy(p->m_vecOrigin, p->m_pELight->origin);
				p->m_pELight->radius = 32;
				p->m_pELight->color.r = 255;
				p->m_pELight->color.g = 30;
				p->m_pELight->color.b = 30;
				p->m_pELight->die = gEngfuncs.GetClientTime() + 45.0;
			}
		}
		int iModel;
		p->m_pSprite = gEngfuncs.CL_LoadModel("sprites/blflare.spr", &iModel);
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x65500. [MP5] PrimaryFlySpeed along its direction, the sprite cycling 30 frames a
// second; done on a hit (events/mp5blasterhit.sc on the world) or its shooter gone
bool CTentMP5Bolt::Update(float flFrametime)
{
	if (0 == m_iEntity)
		return true;

	if (!gEngfuncs.GetEntityByIndex(m_iEntity))
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	const float flExtra = Bolt_HeadStart(m_flHeadStart, flFrametime);
	const Vector vecVel = m_vecDir * RC2Pref(RC2P_MP5_PrimaryFlySpeed).f;
	m_vecOrigin = vecVel * (flExtra + flFrametime) + m_vecOrigin;

	m_flFrame = flFrametime * 30 + m_flFrame;
	if (!(m_flFrame < 7))
		m_flFrame = m_flFrame - 7;

	m_flNextTrace = m_flNextTrace - flFrametime;
	if (m_flNextTrace < 0)
	{
		pmtrace_t tr;
		Bolt_Trace(m_iEntity, m_vecLastTrace, m_vecOrigin, &tr);
		if (tr.fraction < 1.0 && gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr) != m_iEntity)
		{
			physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
			if (Bolt_IsWorld(pe))
			{
				const unsigned short usHit = gEngfuncs.pfnPrecacheEvent(1, "events/mp5blasterhit.sc");
				if (usHit)
					Bolt_HitEvent(usHit, tr, m_iEntity);
			}
			if (m_iEntity)
			{
				KillEntity(m_iEntity);
				m_iEntity = 0;
			}
			return true;
		}
		m_flNextTrace = 0.05;
		m_vecLastTrace = m_vecOrigin;
	}

	if (m_pLight)
		VectorCopy(m_vecOrigin, m_pLight->origin);
	if (m_pELight)
		VectorCopy(m_vecOrigin, m_pELight->origin);
	return false;
}

// orig client 0x65fc0. A vertex at forward, right and up off the bolt
void CTentMP5Bolt::Vertex(float flForward, float flRight, float flUp, float s, float t)
{
	const Vector v = (m_vecOrigin + m_vecForward * flForward) + m_vecRight * flRight + m_vecUp * flUp;
	gEngfuncs.pTriAPI->TexCoord2f(s, t);
	gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);
}

// orig client 0x65a80. Red, 7 frames
void CTentMP5Bolt::DrawTransparent()
{
	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pSprite, (int)m_flFrame % 7);
	gEngfuncs.pTriAPI->Begin(TRI_QUADS);
	gEngfuncs.pTriAPI->Color4f(1, 0, 0, 0.9);
	gEngfuncs.pTriAPI->Brightness(0.9);
	Bolt_DrawQuads(this);
	gEngfuncs.pTriAPI->End();
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// orig client 0x659b0
bool CTentMP5Bolt::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	if (m_pLight)
		m_pLight->die = gEngfuncs.GetClientTime() + 0.01;
	if (m_pELight)
		m_pELight->die = gEngfuncs.GetClientTime() + 0.01;
	m_iEntity = 0;
	return true;
}

// orig client 0x65a30. CLTE 0xfd: the shooter and the bolt id end it
bool CTentMP5Bolt::Command(int iCmd, int iValue)
{
	if (iCmd != m_iEntity || iValue != m_iBolt)
		return false;
	KillEntity(m_iEntity);
	m_iEntity = 0;
	return true;
}

// --- CLTE 17: the MP5's pulse ---

// vtable client 0x1011dae8, 0x80 bytes
class CTentMP5Pulse : public CTentBoltBase
{
public:
	CTentMP5Pulse();
	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEnt) override;
	bool Command(int iCmd, int iValue) override;

	int m_iEntity;		   // +8: the shooter
	int m_iBolt;		   // +0xc
	Vector m_vecOrigin;	   // +0x10
	Vector m_vecDir;	   // +0x1c
	Vector m_vecLastTrace; // +0x28
	dlight_t* m_pLight;	   // +0x34
	dlight_t* m_pELight;   // +0x38
	TEMPENTITY* m_pFlare;  // +0x3c: sprites/bigflare1.spr
	model_s* m_pWhite;	   // +0x40: sprites/white.spr
	model_s* m_pRing;	   // +0x44: sprites/shockwave.spr
	Vector m_vecAngles;	   // +0x48: rolling 480 degrees a second
	Vector m_vecForward;   // +0x54
	Vector m_vecRight;	   // +0x60
	Vector m_vecUp;		   // +0x6c
	float m_flNextTrace;   // +0x78
	float m_flHeadStart;   // +0x7c
};

// orig client 0x66d50. Original bug kept: the flare is left unset when its entity is missing or
// bigflare1.spr does not load, and is later read
CTentMP5Pulse::CTentMP5Pulse()
{
	Bolt_Read(this);
	m_flNextTrace = 0;
	m_pLight = NULL;
	m_pELight = NULL;
	m_pWhite = NULL;
	m_pRing = NULL;
	if (0 == m_iEntity)
		return;

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iEntity);
	if (!pEnt)
	{
		m_iEntity = 0;
		return;
	}

	VectorAngles(m_vecDir, m_vecAngles);
	m_vecAngles.x = -m_vecAngles.x;
	gEngfuncs.pfnAngleVectors(m_vecAngles, m_vecForward, m_vecRight, m_vecUp);

	if (g_iClientDetail >= 0)
	{
		m_pLight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		if (m_pLight)
		{
			VectorCopy(pEnt->origin, m_pLight->origin);
			m_pLight->radius = 100;
			m_pLight->color.r = 0;
			m_pLight->color.g = 255;
			m_pLight->color.b = 0;
			m_pLight->die = gEngfuncs.GetClientTime() + 45.0;
		}
	}
	m_pELight = gEngfuncs.pEfxAPI->CL_AllocElight(0);
	if (m_pELight)
	{
		VectorCopy(m_vecOrigin, m_pELight->origin);
		m_pELight->radius = 70;
		m_pELight->color.r = 0;
		m_pELight->color.g = 255;
		m_pELight->color.b = 0;
		m_pELight->die = gEngfuncs.GetClientTime() + 45.0;
	}

	int iModel;
	model_s* pSprite = gEngfuncs.CL_LoadModel("sprites/bigflare1.spr", &iModel);
	if (!pSprite)
		return;
	m_pFlare = gEngfuncs.pEfxAPI->CL_TempEntAlloc(m_vecOrigin, pSprite);
	if (!m_pFlare)
		return;
	m_pFlare->flags |= FTENT_PERSIST;
	m_pFlare->entity.curstate.scale = 1.5;
	m_pFlare->entity.curstate.renderamt = 130;
	m_pFlare->entity.curstate.rendermode = kRenderGlow;
	m_pFlare->entity.curstate.renderfx = kRenderFxNoDissipation;
	m_pFlare->entity.curstate.rendercolor.b = 30;
	m_pFlare->entity.curstate.rendercolor.g = 255;
	m_pFlare->entity.curstate.rendercolor.r = 30;
	m_pFlare->die = m_pFlare->die + 45.0;
	m_pWhite = gEngfuncs.CL_LoadModel("sprites/white.spr", &iModel);
	m_pRing = gEngfuncs.CL_LoadModel("sprites/shockwave.spr", &iModel);
}

// orig client 0x66140. The copy goes through the base's out-of-line copy constructor (client 0x9b430)
CClientTent* CLTE_CreateMP5Pulse(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentMP5Pulse*>(pCopy));

	CTentMP5Pulse* p = new CTentMP5Pulse;
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x663d0. As the bolt, at [MP5] PulseFlySpeed, rolling, with events/mp5pulsehit.sc;
// the flare follows
bool CTentMP5Pulse::Update(float flFrametime)
{
	if (0 == m_iEntity)
		return true;

	if (!gEngfuncs.GetEntityByIndex(m_iEntity))
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	const float flExtra = Bolt_HeadStart(m_flHeadStart, flFrametime);
	const Vector vecVel = m_vecDir * RC2Pref(RC2P_MP5_PulseFlySpeed).f;
	m_vecOrigin = vecVel * (flExtra + flFrametime) + m_vecOrigin;

	m_vecAngles.z = flFrametime * 480 + m_vecAngles.z;
	gEngfuncs.pfnAngleVectors(m_vecAngles, m_vecForward, m_vecRight, m_vecUp);

	m_flNextTrace = m_flNextTrace - flFrametime;
	if (m_flNextTrace < 0)
	{
		pmtrace_t tr;
		Bolt_Trace(m_iEntity, m_vecLastTrace, m_vecOrigin, &tr);
		if (tr.fraction < 1.0 && gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr) != m_iEntity)
		{
			physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
			if (Bolt_IsWorld(pe))
			{
				const unsigned short usHit = gEngfuncs.pfnPrecacheEvent(1, "events/mp5pulsehit.sc");
				if (usHit)
					Bolt_HitEvent(usHit, tr, m_iEntity);
			}
			if (m_iEntity)
			{
				KillEntity(m_iEntity);
				m_iEntity = 0;
			}
			return true;
		}
		m_flNextTrace = 0.05;
		m_vecLastTrace = m_vecOrigin;
	}

	if (m_pLight)
		VectorCopy(m_vecOrigin, m_pLight->origin);
	if (m_pELight)
		VectorCopy(m_vecOrigin, m_pELight->origin);
	if (m_pFlare)
		m_pFlare->entity.origin = m_vecOrigin;
	return false;
}

// orig client 0x66990. A green cone and two green rings round the pulse, a little different each
// frame; finer with more cl_detail
void CTentMP5Pulse::DrawTransparent()
{
	const float flRadius = gEngfuncs.pfnRandomFloat(8, 10);
	const float flRadius2 = gEngfuncs.pfnRandomFloat(4, 6);
	const float flRadius3 = gEngfuncs.pfnRandomFloat(2, 4);
	const float flBase = gEngfuncs.pfnRandomFloat(-10, -12);
	const float flBase2 = gEngfuncs.pfnRandomFloat(-14, -18);
	const float flBase3 = gEngfuncs.pfnRandomFloat(-16, -20);

	int iRing;
	if (g_iClientDetail >= 1)
		iRing = 4;
	else
		iRing = (g_iClientDetail >= 0 ? 1 : 0) + 2;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pWhite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pWhite, 0);

	CLTE_DrawCone(m_vecOrigin, m_vecUp, m_vecRight, m_vecForward, 0, flBase, flRadius, iRing,
		Vector(0, 1, 0), 0.9, Vector(0, 1, 0), 0.9);

	const float flHeight = flBase2 + flBase;
	const float flWidth = flRadius2 + flRadius;
	CLTE_DrawRing(m_vecOrigin, m_vecUp, m_vecRight, m_vecForward, flWidth, flRadius, flHeight, flBase, iRing,
		Vector(0, 1, 0), 0.9, Vector(0, 1, 0), 0.9);

	if (m_pRing)
		gEngfuncs.pTriAPI->SpriteTexture(m_pRing, 0);
	CLTE_DrawRing(m_vecOrigin, m_vecUp, m_vecRight, m_vecForward, (flRadius3 + flRadius2) + flRadius, flWidth,
		(flBase3 + flBase2) + flBase, flHeight, iRing, Vector(0, 1, 0), 0, Vector(0, 1, 0), 0.9);

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// orig client 0x668a0
bool CTentMP5Pulse::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	if (m_pLight)
		m_pLight->die = gEngfuncs.GetClientTime() + 0.01;
	if (m_pELight)
		m_pELight->die = gEngfuncs.GetClientTime() + 0.01;
	if (m_pFlare)
	{
		Tent_Release(m_pFlare);
		m_pFlare = NULL;
	}
	m_iEntity = 0;
	return true;
}

// orig client 0x66940
bool CTentMP5Pulse::Command(int iCmd, int iValue)
{
	if (iCmd != m_iEntity || iValue != m_iBolt)
		return false;
	KillEntity(m_iEntity);
	m_iEntity = 0;
	return true;
}

// --- CLTE 18: the MP5's bouncing bolt ---

// vtable client 0x1011db30, 0x7c bytes
class CTentMP5Bouncy : public CTentBoltBase
{
public:
	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEnt) override;
	bool Command(int iCmd, int iValue) override;
	void Trace(Vector vecStart, Vector vecEnd, int iDepth);
	void Vertex(float flForward, float flRight, float flUp, float s, float t);

	int m_iEntity;			// +8: the shooter
	int m_iBolt;			// +0xc
	Vector m_vecOrigin;		// +0x10
	Vector m_vecDir;		// +0x1c
	Vector m_vecLastTrace;	// +0x28
	dlight_t* m_pLight;		// +0x34
	dlight_t* m_pELight;	// +0x38
	model_s* m_pSprite;		// +0x3c: sprites/blflare.spr
	Vector m_vecForward;	// +0x40
	Vector m_vecRight;		// +0x4c
	Vector m_vecUp;			// +0x58
	float m_flFrame;		// +0x64
	float m_flNextTrace;	// +0x68
	int m_iBouncesLeft;		// +0x6c: [MP5] TertiaryMaxBounces
	int m_iIgnore;			// +0x70: the shooter until the first bounce, then -1
	float m_flHeadStart;	// +0x74
	unsigned short m_usHit; // +0x78: events/mp5bouncyhit.sc
};

// orig client 0x670b0. The copy goes through an out-of-line copy constructor (client 0x99aa0)
CClientTent* CLTE_CreateMP5Bouncy(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentMP5Bouncy*>(pCopy));

	CTentMP5Bouncy* p = new CTentMP5Bouncy;
	Bolt_Read(p);
	p->m_flNextTrace = 0;
	p->m_iBouncesLeft = (unsigned char)RC2Pref(RC2P_MP5_TertiaryMaxBounces).c;
	p->m_iIgnore = p->m_iEntity;
	p->m_flFrame = gEngfuncs.pfnRandomFloat(0, 6);
	p->m_pLight = NULL;
	p->m_pELight = NULL;
	p->m_pSprite = NULL;
	p->m_usHit = gEngfuncs.pfnPrecacheEvent(1, "events/mp5bouncyhit.sc");

	if (0 != p->m_iEntity)
	{
		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
		if (!pEnt)
		{
			p->m_iEntity = 0;
			g_pClientTents[iSlot] = p;
			return p;
		}

		Vector vecAngles;
		VectorAngles(p->m_vecDir, vecAngles);
		vecAngles.x = -vecAngles.x;
		gEngfuncs.pfnAngleVectors(vecAngles, p->m_vecForward, p->m_vecRight, p->m_vecUp);

		if (g_iClientDetail > 0)
		{
			p->m_pLight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
			if (p->m_pLight)
			{
				VectorCopy(pEnt->origin, p->m_pLight->origin);
				p->m_pLight->radius = 64;
				p->m_pLight->color.r = 127;
				p->m_pLight->color.g = 127;
				p->m_pLight->color.b = 255;
				p->m_pLight->die = gEngfuncs.GetClientTime() + 45.0;
			}
		}
		if (g_iClientDetail >= 0)
		{
			p->m_pELight = gEngfuncs.pEfxAPI->CL_AllocElight(0);
			if (p->m_pELight)
			{
				VectorCopy(p->m_vecOrigin, p->m_pELight->origin);
				p->m_pELight->radius = 32;
				p->m_pELight->color.r = 127;
				p->m_pELight->color.g = 127;
				p->m_pELight->color.b = 255;
				p->m_pELight->die = gEngfuncs.GetClientTime() + 45.0;
			}
		}
		int iModel;
		p->m_pSprite = gEngfuncs.CL_LoadModel("sprites/blflare.spr", &iModel);
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x67540. At [MP5] TertiaryFlySpeed; the trace bounces it (Trace), then its axes follow
// its direction. VERIFY: unlike at Create the pitch is not negated for the axes here
bool CTentMP5Bouncy::Update(float flFrametime)
{
	if (0 == m_iEntity)
		return true;

	if (!gEngfuncs.GetEntityByIndex(m_iEntity))
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	const float flExtra = Bolt_HeadStart(m_flHeadStart, flFrametime);
	const Vector vecVel = m_vecDir * RC2Pref(RC2P_MP5_TertiaryFlySpeed).f;
	m_vecOrigin = vecVel * (flExtra + flFrametime) + m_vecOrigin;

	m_flFrame = flFrametime * 30 + m_flFrame;
	if (!(m_flFrame < 7))
		m_flFrame = m_flFrame - 7;

	m_flNextTrace = m_flNextTrace - flFrametime;
	if (m_flNextTrace < 0)
	{
		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(0, 1);
		gEngfuncs.pEventAPI->EV_PushPMStates();
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		Trace(m_vecLastTrace, m_vecOrigin, 8);
		gEngfuncs.pEventAPI->EV_PopPMStates();
		m_vecLastTrace = m_vecOrigin;

		Vector vecAngles;
		VectorAngles(m_vecDir, vecAngles);
		gEngfuncs.pfnAngleVectors(vecAngles, m_vecForward, m_vecRight, m_vecUp);
		if (g_iClientDetail < 0)
			m_flNextTrace = 0.05;
		else
			m_flNextTrace = 0;
	}

	if (m_pLight)
		VectorCopy(m_vecOrigin, m_pLight->origin);
	if (m_pELight)
		VectorCopy(m_vecOrigin, m_pELight->origin);
	return false;
}

// orig client 0x67890. One leg of the bolt's flight, iDepth - 1 legs at most: into the sky, a teamed
// entity or a non-world physent it dies; off the world it plays its event and, bounces left,
// reflects and flies on with what is left of the leg
void CTentMP5Bouncy::Trace(Vector vecStart, Vector vecEnd, int iDepth)
{
	iDepth--;
	if (iDepth < 1)
		return;

	if (m_iIgnore > 0)
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(EV_RideTarget(m_iIgnore) - 1);
	else
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(-1);

	pmtrace_t tr;
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_NORMAL, m_iIgnore, &tr);

	bool bKill = false;
	bool bEvent = false;
	if (gEngfuncs.PM_PointContents(tr.endpos, NULL) == CONTENTS_SKY)
	{
		bKill = true;
	}
	else if (!(tr.fraction < 1.0) || (tr.ent != 0 && gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr) == m_iIgnore))
	{
		m_vecOrigin = vecEnd;
		return;
	}
	else
	{
		physent_t* pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
		int iIndex;
		cl_entity_t* pHit;
		if (tr.ent != 0 && (iIndex = gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr)) != 0 && (pHit = gEngfuncs.GetEntityByIndex(iIndex)) != NULL)
		{
			if (pHit->curstate.team != 0 || (pe && !Bolt_IsWorld(pe)))
				bKill = true;
			else
				bEvent = pe != NULL;
		}
		else
		{
			bEvent = Bolt_IsWorld(pe);
		}
	}

	if (bKill)
	{
		if (m_iEntity)
		{
			KillEntity(m_iEntity);
			m_iEntity = 0;
		}
		return;
	}

	if (bEvent)
		Bolt_HitEvent(m_usHit, tr, m_iEntity);

	if (m_iBouncesLeft <= 0)
	{
		if (m_iEntity)
		{
			KillEntity(m_iEntity);
			m_iEntity = 0;
		}
		m_vecOrigin = vecEnd;
		return;
	}

	const float flDist = (vecEnd - vecStart).Length();
	m_iBouncesLeft--;
	m_iIgnore = -1;
	const Vector vecNormal = tr.plane.normal;
	m_vecDir = UTIL_ReflectVector(m_vecDir, vecNormal);
	m_vecOrigin = vecNormal + Vector(tr.endpos);

	const float flLeft = (1.0 - tr.fraction) * flDist;
	if (flLeft > 1.0)
	{
		const Vector vecFrom = m_vecOrigin;
		Trace(vecFrom, vecFrom + m_vecDir * flLeft, iDepth);
	}
}

// orig client 0x68300. As CTentMP5Bolt::Vertex
void CTentMP5Bouncy::Vertex(float flForward, float flRight, float flUp, float s, float t)
{
	const Vector v = (m_vecOrigin + m_vecForward * flForward) + m_vecRight * flRight + m_vecUp * flUp;
	gEngfuncs.pTriAPI->TexCoord2f(s, t);
	gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);
}

// orig client 0x67dc0. As the bolt's, blue, 3 frames
void CTentMP5Bouncy::DrawTransparent()
{
	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pSprite)
		gEngfuncs.pTriAPI->SpriteTexture(m_pSprite, (int)m_flFrame % 3);
	gEngfuncs.pTriAPI->Begin(TRI_QUADS);
	gEngfuncs.pTriAPI->Color4f(0.5, 0.5, 1, 0.9);
	gEngfuncs.pTriAPI->Brightness(0.9);
	Bolt_DrawQuads(this);
	gEngfuncs.pTriAPI->End();
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// orig client 0x67cf0
bool CTentMP5Bouncy::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	if (m_pLight)
		m_pLight->die = gEngfuncs.GetClientTime() + 0.01;
	if (m_pELight)
		m_pELight->die = gEngfuncs.GetClientTime() + 0.01;
	m_iEntity = 0;
	return true;
}

// orig client 0x67d70
bool CTentMP5Bouncy::Command(int iCmd, int iValue)
{
	if (iCmd != m_iEntity || iValue != m_iBolt)
		return false;
	KillEntity(m_iEntity);
	m_iEntity = 0;
	return true;
}

// --- CLTE 19: the shotgun crate's charge, a blue glow and a rising drone ---

// vtable client 0x1011db4c, 0x18 bytes
class CTentCrateCharge : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;	  // +8
	float m_flLife;	  // +0xc: [Shotgun] CrateChargeTime
	float m_flVolume; // +0x10
	float m_flPitch;  // +0x14
};

// orig client 0x68480. byte player
CClientTent* CLTE_CreateCrateCharge(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentCrateCharge*>(pCopy));

	CTentCrateCharge* p = new CTentCrateCharge;
	p->m_iEntity = READ_BYTE();
	p->m_flVolume = 0.5;
	p->m_flPitch = 110;
	if (0 != p->m_iEntity)
	{
		p->m_flLife = RC2Pref(RC2P_Shotgun_CrateChargeTime).f;
		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
		if (!pEnt)
			p->m_iEntity = 0;
		else
			gEngfuncs.pEventAPI->EV_PlaySound(p->m_iEntity, pEnt->origin, CHAN_WEAPON, "ambience/dronemachine1.wav", p->m_flVolume, 0.8, 0, (int)p->m_flPitch);
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x686f0
bool CTentCrateCharge::Update(float flFrametime)
{
	if (0 == m_iEntity)
		return true;

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iEntity);
	if (!pEnt)
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	m_flLife = m_flLife - flFrametime;
	if (m_flLife < 0)
	{
		KillEntity(m_iEntity);
		return true;
	}

	float flCharge = 1.0 - m_flLife / RC2Pref(RC2P_Shotgun_CrateChargeTime).f;
	if (flCharge < 0)
		flCharge = 0;
	else if (flCharge > 1)
		flCharge = 1;

	m_flVolume = flCharge * 0.3 + 0.5;
	m_flPitch = flCharge * 40 + 110;
	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntity, pEnt->origin, CHAN_WEAPON, "ambience/dronemachine1.wav", m_flVolume, 0.8,
		CLTE_SND_CHANGE_VOL | CLTE_SND_CHANGE_PITCH, (int)m_flPitch);

	CLTE_SetPlayerOverride(m_iEntity, 0, 0, flCharge * 255, flCharge * 20);

	if (0 != gEngfuncs.pEventAPI->EV_IsLocal(m_iEntity - 1))
	{
		cl_entity_t* pView = gEngfuncs.GetViewModel();
		if (pView)
		{
			pView->curstate.renderfx = kRenderFxGlowShell;
			pView->curstate.renderamt = (int)(flCharge * 16);
			pView->curstate.rendercolor.b = (int)(flCharge * 255);
			pView->curstate.rendercolor.r = 0;
			pView->curstate.rendercolor.g = 0;
		}
	}
	return false;
}

// orig client 0x68910
bool CTentCrateCharge::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	CLTE_ClearPlayerOverride(m_iEntity);
	gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_WEAPON, "ambience/dronemachine1.wav");
	if (0 != gEngfuncs.pEventAPI->EV_IsLocal(m_iEntity - 1))
	{
		cl_entity_t* pView = gEngfuncs.GetViewModel();
		if (pView)
		{
			pView->curstate.renderfx = kRenderFxNone;
			pView->curstate.renderamt = 0;
			pView->curstate.rendercolor.b = 0;
			pView->curstate.rendercolor.g = 0;
			pView->curstate.rendercolor.r = 0;
		}
	}
	m_iEntity = 0;
	return true;
}

// --- CLTE 20: the HEV suit's flame as it lands (shotgunprojectiles.cpp CLTE_HEVFLAME) ---

// vtable client 0x1011db74, 0x30 bytes
class CTentHEVFlame : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;			 // +8
	float m_flLife;			 // +0xc: 1 s
	BEAM* m_pTrails[4];		 // +0x10: smoke from attachments 1-4 (entity | n << 12)
	TEMPENTITY* m_pGlows[2]; // +0x20: on attachments 0 and 1
	dlight_t* m_pLight;		 // +0x28
	bool m_bSound;			 // +0x2c
};

// orig client 0x689b0. short suit. The copy goes through an out-of-line copy constructor (client
// 0x99bb0). Original bug kept: with cl_detail negative the light is left unset and later read
CClientTent* CLTE_CreateHEVFlame(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentHEVFlame*>(pCopy));

	CTentHEVFlame* p = new CTentHEVFlame;
	p->m_iEntity = READ_SHORT();
	p->m_flLife = 1.0;
	p->m_pTrails[3] = NULL;
	p->m_pTrails[2] = NULL;
	p->m_pTrails[1] = NULL;
	p->m_pTrails[0] = NULL;
	p->m_pGlows[1] = NULL;
	p->m_pGlows[0] = NULL;

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
	if (!pEnt)
	{
		p->m_iEntity = 0;
		g_pClientTents[iSlot] = p;
		return p;
	}

	p->m_bSound = true;
	gEngfuncs.pEventAPI->EV_PlaySound(p->m_iEntity, pEnt->origin, CHAN_STATIC, "weapons/rocket1.wav", 1.0, 0.8, 0, 100);

	if (g_iClientDetail >= 0)
	{
		p->m_pLight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		if (p->m_pLight)
		{
			VectorCopy(pEnt->origin, p->m_pLight->origin);
			p->m_pLight->radius = 160;
			p->m_pLight->color.r = 255;
			p->m_pLight->color.g = 255;
			p->m_pLight->color.b = 255;
			p->m_pLight->die = gEngfuncs.GetClientTime() + 10.0;
		}
	}

	int iModel;
	if (gEngfuncs.CL_LoadModel("sprites/smoke.spr", &iModel))
	{
		p->m_pTrails[0] = gEngfuncs.pEfxAPI->R_BeamFollow(p->m_iEntity | 0x1000, iModel, 2, 2, 0.95, 0.95, 1, 0.6);
		p->m_pTrails[1] = gEngfuncs.pEfxAPI->R_BeamFollow(p->m_iEntity | 0x2000, iModel, 2, 2, 0.95, 0.95, 1, 0.6);
		p->m_pTrails[2] = gEngfuncs.pEfxAPI->R_BeamFollow(p->m_iEntity | 0x3000, iModel, 4, 0.5, 0.95, 0.95, 1, 0.5);
		p->m_pTrails[3] = gEngfuncs.pEfxAPI->R_BeamFollow(p->m_iEntity | 0x4000, iModel, 4, 0.5, 0.95, 0.95, 1, 0.5);

		model_s* pGlow = gEngfuncs.CL_LoadModel("sprites/animglow01.spr", &iModel);
		if (pGlow)
		{
			for (int i = 0; i < 2; i++)
			{
				TEMPENTITY* pTent = gEngfuncs.pEfxAPI->CL_TempEntAlloc(pEnt->origin, pGlow);
				p->m_pGlows[i] = pTent;
				if (!pTent)
					break;
				pTent->flags |= FTENT_SPRANIMATELOOP | FTENT_PERSIST | FTENT_SPRANIMATE;
				pTent->entity.curstate.framerate = 30;
				pTent->entity.curstate.scale = 0.3;
				pTent->entity.curstate.renderamt = 240;
				pTent->entity.curstate.rendermode = kRenderGlow;
				pTent->entity.curstate.renderfx = kRenderFxNoDissipation;
				pTent->entity.curstate.rendercolor.r = 255;
				pTent->entity.curstate.rendercolor.b = 255;
				pTent->entity.curstate.rendercolor.g = 255;
				pTent->entity.curstate.frame = gEngfuncs.pfnRandomFloat(0, 6);
				pTent->frameMax = 7;
				pTent->die = pTent->die + 5.0;
			}
		}
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x68fd0. A second of flame: the glows on the suit's first two attachments, the light
// on it; then the sound and the two thick trails stop (the thin two fade on their own)
bool CTentHEVFlame::Update(float flFrametime)
{
	if (0 == m_iEntity)
		return true;

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iEntity);
	if (!pEnt)
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
	if (!pLocal || pLocal->curstate.messagenum > pEnt->curstate.messagenum)
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	m_flLife = m_flLife - flFrametime;
	if (m_flLife >= 0)
	{
		if (m_pGlows[0])
			m_pGlows[0]->entity.origin = pEnt->attachment[0];
		if (m_pGlows[1])
			m_pGlows[1]->entity.origin = pEnt->attachment[1];
		if (m_pLight)
			VectorCopy(pEnt->origin, m_pLight->origin);
		return false;
	}

	if (m_bSound)
		gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_STATIC, "weapons/rocket1.wav");
	m_bSound = false;
	for (int i = 0; i < 2; i++)
	{
		if (m_pTrails[i])
		{
			m_pTrails[i]->flags &= ~(FBEAM_STARTENTITY | FBEAM_ENDENTITY);
			m_pTrails[i]->endEntity = 0;
			m_pTrails[i]->startEntity = 0;
			m_pTrails[i] = NULL;
		}
	}
	for (int i = 0; i < 2; i++)
	{
		if (m_pGlows[i])
		{
			Tent_Release(m_pGlows[i]);
			m_pGlows[i] = NULL;
		}
	}
	if (m_pLight)
	{
		m_pLight->die = gEngfuncs.GetClientTime() + 0.01;
		m_pLight = NULL;
	}
	return false;
}

// orig client 0x69220
bool CTentHEVFlame::KillEntity(int iEnt)
{
	if (m_iEntity != iEnt)
		return false;

	for (int i = 0; i < 4; i++)
	{
		if (m_pTrails[i])
		{
			m_pTrails[i]->flags &= ~(FBEAM_STARTENTITY | FBEAM_ENDENTITY);
			m_pTrails[i]->endEntity = 0;
			m_pTrails[i]->startEntity = 0;
			m_pTrails[i] = NULL;
		}
	}
	for (int i = 0; i < 2; i++)
	{
		if (m_pGlows[i])
		{
			Tent_Release(m_pGlows[i]);
			m_pGlows[i] = NULL;
		}
	}
	if (m_pLight)
	{
		m_pLight->die = gEngfuncs.GetClientTime() + 0.01;
		m_pLight = NULL;
	}
	if (m_bSound)
		gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_STATIC, "weapons/rocket1.wav");
	m_iEntity = 0;
	return true;
}

// TEMP STUB (c2): real one belongs in the client's event files (client 0xdd40, c1's)
int EV_RideTarget(int iEnt)
{
	return iEnt;
}
