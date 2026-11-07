/***
*
*	Rocket Crowbar 2: client effects CLTE 1-5 (see clte.h): the rocket trail, the jetboost, the
*	visible jetpack and the energy draw.
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
#include "clte.h"
#include "clte_types.h"

// --- CLTE 1: a rocket's rainbow smoke trail ---

// vtable client 0x1011d864, 0x1c bytes
class CTentRocketTrail : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	BEAM* m_pBeam;	 // +8
	int m_iModel;	 // +0xc: sprites/smoke.spr
	int m_iEntity;	 // +0x10
	float m_flHue;	 // +0x14: 0-3 round red, green and blue
	float m_flLife;	 // +0x18
};

// The trail's color for a hue of 0-3: red to green to blue and back
static void RocketTrail_Color(float flHue, float& r, float& g, float& b)
{
	if (flHue <= 1)
	{
		r = 1 - flHue;
		g = flHue;
		b = 0;
	}
	else if (flHue <= 2)
	{
		r = 0;
		g = 2 - flHue;
		b = flHue - 1;
	}
	else
	{
		r = flHue - 2;
		g = 0;
		b = 3 - flHue;
	}
}

// orig client 0x5bac0. short entity
CClientTent* CLTE_CreateRocketTrail(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentRocketTrail*>(pCopy));

	CTentRocketTrail* p = new CTentRocketTrail;
	p->m_iEntity = READ_SHORT();
	if (0 != p->m_iEntity && gEngfuncs.CL_LoadModel("sprites/smoke.spr", &p->m_iModel))
	{
		p->m_flHue = gEngfuncs.pfnRandomFloat(0, 3);
		p->m_flLife = 120;

		float r, g, b;
		RocketTrail_Color(p->m_flHue, r, g, b);
		const Vector vecColor(r, g, b);
		p->m_pBeam = gEngfuncs.pEfxAPI->R_BeamFollow(p->m_iEntity, p->m_iModel, 5, 5, vecColor.x, vecColor.y, vecColor.z, 0.8);
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x5bde0. The hue turns 1.5 a second; done with no model or beam, or 120 s on
bool CTentRocketTrail::Update(float flFrametime)
{
	if (0 == m_iModel)
		return true;

	m_flHue = flFrametime * 1.5 + m_flHue;
	if (m_flHue > 3)
		m_flHue = m_flHue - 3;

	if (!m_pBeam)
		return true;

	RocketTrail_Color(m_flHue, m_pBeam->r, m_pBeam->g, m_pBeam->b);

	m_flLife = m_flLife - flFrametime;
	return !(m_flLife > 0);
}

// orig client 0x5bf20. The trail keeps fading for 5 s after its rocket goes
bool CTentRocketTrail::KillEntity(int iEnt)
{
	if (iEnt == m_iEntity)
	{
		m_flLife = 5;
		m_iEntity = 0;
	}
	return false;
}

// --- CLTE 2: the jetboost's burst (player.cpp EnergyEffectOf) ---

// vtable client 0x1011d880, 0x18 bytes
class CTentJetboost : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	float m_flLife;		  // +8
	int m_iEntity;		  // +0xc
	TEMPENTITY* m_pFlame; // +0x10: the fireball on the player's back
	BEAM* m_pTrail;		  // +0x14
};

// orig client 0x5bff0. byte player
CClientTent* CLTE_CreateJetboost(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentJetboost*>(pCopy));

	CTentJetboost* p = new CTentJetboost;
	const int iEnt = READ_BYTE();
	p->m_iEntity = iEnt;
	p->m_flLife = 1.0;
	if (0 != iEnt)
	{
		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(iEnt);
		if (!pEnt)
		{
			p->m_iEntity = 0;
			g_pClientTents[iSlot] = p;
			return p;
		}

		// the sound at the player's feet
		cl_entity_t* pPlayer = gEngfuncs.GetEntityByIndex(p->m_iEntity);
		Vector vecFeet(0, 0, 0);
		if (pPlayer && 0 != p->m_iEntity)
		{
			const Vector vecOrigin = CLTE_GetOrigin(p->m_iEntity);
			vecFeet = Vector(vecOrigin.x, vecOrigin.y, (pPlayer->curstate.mins.z - 4.0) + vecOrigin.z);
		}
		Vector vecSound = vecFeet;
		gEngfuncs.pEventAPI->EV_PlaySound(p->m_iEntity, vecSound, CHAN_ITEM, "weapons/jetboost.wav", 1.0, 0.8, 0, 100);

		int iModel;
		if (gEngfuncs.CL_LoadModel("sprites/smoke.spr", &iModel))
		{
			p->m_pTrail = gEngfuncs.pEfxAPI->R_BeamFollow(p->m_iEntity, iModel, 2, 6, 0.95, 0.95, 1, 0.6);

			model_s* pFire = gEngfuncs.CL_LoadModel("sprites/xfireball3.spr", &iModel);
			if (pFire)
			{
				p->m_pFlame = gEngfuncs.pEfxAPI->CL_TempEntAlloc(pEnt->origin, pFire);
				if (p->m_pFlame)
				{
					p->m_pFlame->flags |= FTENT_PLYRATTACHMENT | FTENT_PERSIST | FTENT_SPRANIMATE;
					p->m_pFlame->tentOffset.y = 0;
					p->m_pFlame->tentOffset.x = 0;
					p->m_pFlame->clientIndex = (short)p->m_iEntity;
					p->m_pFlame->entity.curstate.framerate = 30;
					p->m_pFlame->entity.curstate.scale = 0.9;
					p->m_pFlame->entity.curstate.renderamt = 230;
					p->m_pFlame->entity.curstate.rendermode = kRenderGlow;
					p->m_pFlame->entity.curstate.renderfx = kRenderFxNoDissipation;
					p->m_pFlame->entity.curstate.frame = 0;
					p->m_pFlame->frameMax = 20;
					p->m_pFlame->die = p->m_pFlame->die + 2.0;
					p->m_pFlame->tentOffset.z = (pEnt->curstate.usehull == 1) ? -20 : -40;
				}
			}
		}
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x5c490. A second's burst: the fireball follows the player down (lower when he
// stands, scaled with him) until 1.4 s are left
bool CTentJetboost::Update(float flFrametime)
{
	if (0 == m_iEntity)
		return true;

	m_flLife = m_flLife - flFrametime;
	if (m_flLife <= 0)
	{
		if (m_pTrail)
		{
			m_pTrail->flags &= ~(FBEAM_STARTENTITY | FBEAM_ENDENTITY);
			m_pTrail->endEntity = 0;
			m_pTrail->startEntity = 0;
			m_pTrail = NULL;
		}
		return true;
	}

	const float flScale = CLTE_GetEntityScale(m_iEntity);
	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iEntity);
	if (pEnt && m_pFlame)
	{
		m_pFlame->tentOffset.z = (pEnt->curstate.usehull == 1) ? -20 : -40;
		m_pFlame->tentOffset.z = flScale * m_pFlame->tentOffset.z;
	}

	// VERIFY: the life starts at 1, so this lets go of the fireball on the first update
	if (m_flLife < 1.4)
		m_pFlame = NULL;

	return false;
}

// orig client 0x5c580
bool CTentJetboost::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	if (m_pFlame)
	{
		m_pFlame->die = 0;
		m_pFlame = NULL;
	}
	gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_ITEM, "weapons/jetboost.wav");
	if (m_pTrail)
	{
		m_pTrail->flags &= ~(FBEAM_STARTENTITY | FBEAM_ENDENTITY);
		m_pTrail->endEntity = 0;
		m_pTrail->startEntity = 0;
		m_pTrail = NULL;
	}
	m_iEntity = 0;
	return true;
}

// --- CLTE 3: the visible jetpack (cl_visiblejetpack); CLTE 4 ends it ---

// orig client 0x5c610. byte player
CClientTent* CLTE_CreateJetpack(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentJetpack*>(pCopy));

	CTentJetpack* p = new CTentJetpack;
	p->m_iEntity = READ_BYTE();
	p->m_pSmoke = NULL;
	p->m_pFlame = NULL;
	p->m_pSpark = NULL;
	if (0 != p->m_iEntity)
	{
		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
		if (!pEnt)
		{
			p->m_iEntity = 0;
			g_pClientTents[iSlot] = p;
			return p;
		}

		gEngfuncs.pEventAPI->EV_PlaySound(p->m_iEntity, pEnt->origin, CHAN_STATIC, "ambience/industrial4.wav", 0.8, 0.8, 0, 110);

		int iModel;
		if (gEngfuncs.CL_LoadModel("sprites/smoke.spr", &iModel))
		{
			p->m_pSmoke = gEngfuncs.pEfxAPI->R_BeamFollow(p->m_iEntity, iModel, 1.8, 6, 0.95, 0.95, 1, 0.5);
			if (p->m_pSmoke)
				p->m_pSmoke->flags |= FBEAM_FOREVER;

			if (gEngfuncs.CL_LoadModel("sprites/laserbeam.spr", &iModel))
			{
				p->m_pFlame = gEngfuncs.pEfxAPI->R_BeamPoints(pEnt->origin, pEnt->origin, iModel, 0, 3, 0.12, 0.8, 9, 0, 0, 1, 0.9, 0.2);
				if (p->m_pFlame)
					p->m_pFlame->flags |= FBEAM_SHADEOUT;

				model_s* pSpark = gEngfuncs.CL_LoadModel("sprites/xspark4.spr", &iModel);
				if (pSpark)
				{
					p->m_pSpark = gEngfuncs.pEfxAPI->CL_TempEntAlloc(pEnt->origin, pSpark);
					if (p->m_pSpark)
					{
						p->m_pSpark->flags |= FTENT_SPRANIMATELOOP | FTENT_PERSIST | FTENT_SPRANIMATE;
						p->m_pSpark->entity.curstate.framerate = 20;
						p->m_pSpark->entity.curstate.scale = 0.4;
						p->m_pSpark->entity.curstate.renderamt = 230;
						p->m_pSpark->entity.curstate.rendermode = kRenderGlow;
						p->m_pSpark->entity.curstate.renderfx = kRenderFxNoDissipation;
						p->m_pSpark->entity.curstate.frame = 0;
						p->m_pSpark->frameMax = 11;
						p->m_pSpark->die = p->m_pSpark->die + 20.0;
						p->UpdatePosition();
					}
				}
			}
		}
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x5ca40. The flame from the pack down and back, and the spark at its nozzle, with the
// player's yaw and roll and scale (higher when ducked)
void CTentJetpack::UpdatePosition()
{
	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iEntity);
	if (!pEnt || !m_pFlame)
		return;

	Vector vecAngles = pEnt->angles;
	vecAngles.x = 0;
	Vector vecForward, vecRight, vecUp;
	gEngfuncs.pfnAngleVectors(vecAngles, vecForward, vecRight, vecUp);

	const float flScale = CLTE_GetEntityScale(m_iEntity);
	Vector vecPos = CLTE_GetOrigin(m_iEntity);
	if (pEnt->curstate.usehull == 1)
		vecPos = vecPos + (vecUp * 12) * flScale;
	else
		vecPos = vecPos + (vecUp * 2) * flScale;
	vecPos = vecPos - (vecForward * 9) * flScale;

	const Vector vecEnd = (vecPos - (vecUp * 30) * flScale) - (vecForward * 2) * flScale;

	if (m_pFlame)
	{
		m_pFlame->source = vecPos;
		m_pFlame->target = vecEnd;
		m_pFlame->delta = vecEnd - vecPos;
	}
	if (m_pSpark)
	{
		m_pSpark->entity.origin = vecPos;
		m_pSpark->entity.curstate.scale = flScale * 0.55 + 0.05;
	}
}

// orig client 0x5cfb0. Out of sight (an older packet than the local player's, and not him) the beam
// and spark are parked out of the map; otherwise they follow. The spark's life is pushed on each
// frame. Original quirk kept: while in sight it gains a second as well as the frame each update
bool CTentJetpack::Update(float flFrametime)
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
	if (!pLocal)
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	if (pLocal->curstate.messagenum > pEnt->curstate.messagenum && pLocal->index != pEnt->index)
	{
		if (m_pFlame)
		{
			m_pFlame->source = Vector(4095, 4095, 4095);
			m_pFlame->target = Vector(4096, 4096, 4096);
			m_pFlame->delta = Vector(1, 1, 1);
		}
		if (m_pSpark)
		{
			m_pSpark->entity.origin = Vector(4095, 4095, 4095);
			m_pSpark->die = flFrametime + m_pSpark->die;
		}
		return false;
	}

	// the original repeats UpdatePosition's code here
	UpdatePosition();

	if (m_pSpark)
		m_pSpark->die = m_pSpark->die + flFrametime + 1.0;

	return false;
}

// orig client 0x5d500
bool CTentJetpack::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	if (m_pSpark)
	{
		m_pSpark->flags &= ~FTENT_SPRANIMATELOOP;
		m_pSpark->die = 0;
		m_pSpark = NULL;
	}
	gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_STATIC, "ambience/industrial4.wav");
	if (m_pSmoke)
	{
		Beam_Release(m_pSmoke);
		m_pSmoke = NULL;
	}
	if (m_pFlame)
	{
		Beam_Kill(m_pFlame);
		m_pFlame = NULL;
	}
	m_iEntity = 0;
	return true;
}

// --- CLTE 5: the energy draw (player.cpp); CLTE 6 ends it ---

// orig client 0x5d5e0. byte player: 16 beams from points round him to his middle
CClientTent* CLTE_CreateEnergyDraw(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentEnergyDraw*>(pCopy));

	CTentEnergyDraw* p = new CTentEnergyDraw;
	p->m_iEntity = READ_BYTE();
	for (int i = 0; i < CTentEnergyDraw::NUM_BEAMS; i++)
		p->m_pBeams[i] = NULL;
	p->m_flSparkTime = 0;

	if (0 != p->m_iEntity)
	{
		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
		if (!pEnt)
		{
			p->m_iEntity = 0;
			g_pClientTents[iSlot] = p;
			return p;
		}

		gEngfuncs.pEventAPI->EV_PlaySound(p->m_iEntity, pEnt->origin, CHAN_VOICE, "weapons/energydraw.wav", 1.0, 0.8, 0, 100);

		int iModel;
		if (gEngfuncs.CL_LoadModel("sprites/gradbeam.spr", &iModel))
		{
			for (int i = 0; i < CTentEnergyDraw::NUM_BEAMS; i++)
			{
				Vector vecAngles;
				vecAngles.x = gEngfuncs.pfnRandomFloat(-90, 90);
				vecAngles.y = gEngfuncs.pfnRandomFloat(0, 360);
				vecAngles.z = 0;
				Vector vecDir;
				gEngfuncs.pfnAngleVectors(vecAngles, vecDir, NULL, NULL);

				p->m_vecBeamPoints[i][0] = vecDir * gEngfuncs.pfnRandomFloat(64, 72);
				p->m_vecBeamPoints[i][1] = Vector(0, 0, 16);

				Vector vecEnd = pEnt->origin + p->m_vecBeamPoints[i][1];
				Vector vecStart = pEnt->origin + p->m_vecBeamPoints[i][0];
				const int iFrame = (int)gEngfuncs.pfnRandomFloat(1, 8);
				p->m_pBeams[i] = gEngfuncs.pEfxAPI->R_BeamPoints(vecStart, vecEnd, iModel, 0, 1, 0, 0.95,
					gEngfuncs.pfnRandomFloat(-3, -4), iFrame, 0, 0.7, 0.7, 1);
				if (p->m_pBeams[i])
					p->m_pBeams[i]->flags |= FBEAM_SHADEIN;
			}
		}
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x5dad0. The beams follow the player, scaled with him; with cl_detail not negative,
// a teleport spark round him every 0.1-0.2 s
bool CTentEnergyDraw::Update(float flFrametime)
{
	if (0 == m_iEntity)
		return true;

	if (!gEngfuncs.GetEntityByIndex(m_iEntity))
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	const float flScale = CLTE_GetEntityScale(m_iEntity);
	for (int i = 0; i < NUM_BEAMS; i++)
	{
		BEAM* pBeam = m_pBeams[i];
		if (!pBeam)
			continue;

		pBeam->source = m_vecBeamPoints[i][0] * flScale + CLTE_GetOrigin(m_iEntity);
		m_pBeams[i]->target = m_vecBeamPoints[i][1] * flScale + CLTE_GetOrigin(m_iEntity);
		m_pBeams[i]->delta = m_pBeams[i]->target - m_pBeams[i]->source;
	}

	if (g_iClientDetail >= 0)
	{
		m_flSparkTime = m_flSparkTime - flFrametime;
		if (m_flSparkTime <= 0)
		{
			int iModel;
			model_s* pSprite = gEngfuncs.CL_LoadModel("sprites/b-tele1.spr", &iModel);
			if (pSprite)
			{
				Vector vecOffset;
				vecOffset.x = gEngfuncs.pfnRandomFloat(-48, 48) * flScale;
				vecOffset.y = gEngfuncs.pfnRandomFloat(-48, 48) * flScale;
				vecOffset.z = gEngfuncs.pfnRandomFloat(-30, 30) * flScale;
				Vector vecPos = vecOffset + CLTE_GetOrigin(m_iEntity);

				TEMPENTITY* pSpark = gEngfuncs.pEfxAPI->CL_TempEntAlloc(vecPos, pSprite);
				if (pSpark)
				{
					pSpark->flags |= FTENT_SPRANIMATE;
					pSpark->entity.curstate.framerate = 30;
					pSpark->entity.curstate.scale = flScale * 0.2;
					pSpark->die = pSpark->die + 1.0;
					pSpark->entity.curstate.renderamt = 230;
					pSpark->entity.curstate.rendermode = kRenderGlow;
					pSpark->entity.curstate.renderfx = kRenderFxNoDissipation;
					pSpark->entity.curstate.frame = 0;
					pSpark->frameMax = 11;
					m_flSparkTime = gEngfuncs.pfnRandomFloat(0.1, 0.2);
				}
			}
		}
	}
	return false;
}

// orig client 0x5df30
bool CTentEnergyDraw::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_VOICE, "weapons/energydraw.wav");
	for (int i = 0; i < NUM_BEAMS; i++)
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

// --- CLTE 7: the scientist mine's idle rings (scientistmine.cpp CLTE_SCIMINE_IDLE) ---

// vtable client 0x1011d8e4, 0x10c bytes
class CTentSciMineIdle : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;			 // +8
	BEAM* m_pBeams[8];		 // +0xc: the ring's eight sides
	Vector m_vecPoints[17];	 // +0x2c: the ring, 24 out, closed at [8]; the rest unused
	Vector m_vecOffset;		 // +0xf8: the ring's height above the mine
	float m_flAlpha;		 // +0x104
	int m_iState;			 // +0x108: 1 rising and brightening, 2 fading
};

// orig client 0x5dfd0. short mine
CClientTent* CLTE_CreateSciMineIdle(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentSciMineIdle*>(pCopy));

	CTentSciMineIdle* p = new CTentSciMineIdle;
	p->m_iEntity = READ_SHORT();
	for (int i = 0; i < 8; i++)
		p->m_pBeams[i] = NULL;

	if (0 != p->m_iEntity)
	{
		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
		if (!pEnt)
		{
			p->m_iEntity = 0;
			g_pClientTents[iSlot] = p;
			return p;
		}

		float flAngle = 0;
		for (int i = 0; i < 9; i++)
		{
			p->m_vecPoints[i].z = gEngfuncs.pfnRandomFloat(-2, 2);
			p->m_vecPoints[i].x = cos(flAngle) * 24.0;
			p->m_vecPoints[i].y = sin(flAngle) * 24.0;
			flAngle = flAngle + 0.7853982;
		}
		p->m_vecOffset.z = -16;
		p->m_vecPoints[8] = p->m_vecPoints[0];
		p->m_vecOffset.y = 0;
		p->m_vecOffset.x = 0;

		int iModel;
		if (gEngfuncs.CL_LoadModel("sprites/lgtning.spr", &iModel))
		{
			for (int i = 0; i < 8; i++)
			{
				Vector vecEnd = pEnt->origin + p->m_vecPoints[i + 1] + p->m_vecOffset;
				Vector vecStart = pEnt->origin + p->m_vecPoints[i] + p->m_vecOffset;
				const float flFramerate = gEngfuncs.pfnRandomFloat(10, 20);
				const int iFrame = (int)gEngfuncs.pfnRandomFloat(0, 2);
				p->m_pBeams[i] = gEngfuncs.pEfxAPI->R_BeamPoints(vecStart, vecEnd, iModel, 0, 0.6, 0.3, 0.01, 1, iFrame, flFramerate, 1, 0.05, 0);
			}
			p->m_flAlpha = 0.01;
			p->m_iState = 1;
		}
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x5e4c0. The ring climbs 208/7 units a second, brightening, and fades from 72 up; done
// above 88 or once faded
bool CTentSciMineIdle::Update(float flFrametime)
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

	if (m_vecOffset.z > 88 || (m_flAlpha <= 0.01 && m_iState == 2))
	{
		KillEntity(m_iEntity);
		return true;
	}

	const float flScale = CLTE_GetEntityScale(m_iEntity);
	m_vecOffset.z = flFrametime * 29.714285f + m_vecOffset.z;
	if (m_vecOffset.z > 72)
		m_iState = 2;

	if (m_iState == 1)
	{
		if (m_flAlpha < 0.9)
			m_flAlpha = flFrametime * 1.6714285f + m_flAlpha;
		else
			m_flAlpha = 0.9;
	}
	else
	{
		if (m_flAlpha > 0.01)
			m_flAlpha = m_flAlpha - flFrametime * 1.6714285f;
		else
			m_flAlpha = 0.01;
	}

	for (int i = 0; i < 8; i++)
	{
		BEAM* pBeam = m_pBeams[i];
		if (!pBeam)
			continue;

		pBeam->source = m_vecOffset * flScale + (pEnt->origin + m_vecPoints[i] * flScale);
		pBeam->target = m_vecOffset * flScale + (pEnt->origin + m_vecPoints[i + 1] * flScale);
		pBeam->delta = pBeam->target - pBeam->source;
		pBeam->brightness = m_flAlpha;
	}
	return false;
}

// orig client 0x5ea50
bool CTentSciMineIdle::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	for (int i = 0; i < 8; i++)
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

// --- CLTE 8: the scientist mine spinning up (CLTE_SCIMINE_TRIGGERED): four rings rising in turn ---

// vtable client 0x1011d924, 0x150 bytes
class CTentSciMineSpin : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;			// +8
	BEAM* m_pBeams[4][8];	// +0xc
	Vector m_vecPoints[9];	// +0x8c: the ring, closed
	Vector m_vecOffset[4];	// +0xf8: each ring's height
	float m_flAlpha[4];		// +0x128
	int m_iState[4];		// +0x138: -1 waiting, 1 rising, 2 fading
	float m_flLife;			// +0x148
	float m_flNextRing;		// +0x14c
};

// orig client 0x5ead0. short mine. The copy goes through the base's out-of-line copy constructor
// (client 0x9b430, another package's)
CClientTent* CLTE_CreateSciMineSpin(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentSciMineSpin*>(pCopy));

	CTentSciMineSpin* p = new CTentSciMineSpin;
	p->m_iEntity = READ_SHORT();
	for (int k = 0; k < 4; k++)
	{
		for (int i = 0; i < 8; i++)
			p->m_pBeams[k][i] = NULL;
	}

	if (0 != p->m_iEntity)
	{
		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
		if (!pEnt)
		{
			p->m_iEntity = 0;
			g_pClientTents[iSlot] = p;
			return p;
		}

		p->m_flLife = 0.75;
		p->m_flNextRing = 0;

		float flAngle = 0;
		for (int i = 0; i < 9; i++)
		{
			p->m_vecPoints[i].z = 0;
			p->m_vecPoints[i].x = cos(flAngle) * 24.0;
			p->m_vecPoints[i].y = sin(flAngle) * 24.0;
			flAngle = flAngle + 0.7853982;
		}
		p->m_vecPoints[8] = p->m_vecPoints[0];

		int iModel;
		if (gEngfuncs.CL_LoadModel("sprites/lgtning.spr", &iModel))
		{
			for (int k = 0; k < 4; k++)
			{
				p->m_vecOffset[k].z = -8;
				p->m_vecOffset[k].y = 0;
				p->m_vecOffset[k].x = 0;
				p->m_flAlpha[k] = 0.01;
				p->m_iState[k] = -1;
				for (int i = 0; i < 8; i++)
				{
					Vector vecEnd = pEnt->origin + p->m_vecPoints[i + 1] + p->m_vecOffset[k];
					Vector vecStart = pEnt->origin + p->m_vecPoints[i] + p->m_vecOffset[k];
					const float flFramerate = gEngfuncs.pfnRandomFloat(10, 20);
					const int iFrame = (int)gEngfuncs.pfnRandomFloat(0, 2);
					p->m_pBeams[k][i] = gEngfuncs.pEfxAPI->R_BeamPoints(vecStart, vecEnd, iModel, 0, 0.6, 0.3, 0.01, 1, iFrame, flFramerate, 1, 0.05, 0);
				}
			}
		}
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x5f060. For 0.75 s a ring starts every 0.09-0.14 s and climbs 704/3 a second
bool CTentSciMineSpin::Update(float flFrametime)
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

	const float flLife = m_flLife;
	m_flLife = flLife - flFrametime;
	if (flLife - flFrametime < 0)
	{
		KillEntity(m_iEntity);
		return true;
	}

	m_flNextRing = m_flNextRing - flFrametime;
	const float flScale = CLTE_GetEntityScale(m_iEntity);

	for (int k = 0; k < 4; k++)
	{
		if (m_flAlpha[k] <= 0.01 && m_iState[k] == 2)
		{
			for (int i = 0; i < 8; i++)
			{
				if (m_pBeams[k][i])
					m_pBeams[k][i]->brightness = 0.01;
			}
			m_iState[k] = -1;
		}

		if (m_iState[k] == -1)
		{
			if (m_flNextRing > 0)
				continue;
			m_flNextRing = gEngfuncs.pfnRandomFloat(0, 0.05) + 0.09375;
			m_flAlpha[k] = 0.01;
			m_iState[k] = 1;
			m_vecOffset[k].z = -8;
		}

		m_vecOffset[k].z = flFrametime * 234.66667f + m_vecOffset[k].z;
		if (m_vecOffset[k].z > 72)
			m_iState[k] = 2;

		if (m_iState[k] == 1)
		{
			if (m_flAlpha[k] >= 0.9)
				m_flAlpha[k] = 0.9;
			else
				m_flAlpha[k] = flFrametime * 26.400002f + m_flAlpha[k];
		}
		else if (m_flAlpha[k] <= 0.01)
		{
			m_flAlpha[k] = 0.01;
		}
		else
		{
			m_flAlpha[k] = m_flAlpha[k] - flFrametime * 26.400002f;
		}

		for (int i = 0; i < 8; i++)
		{
			BEAM* pBeam = m_pBeams[k][i];
			if (!pBeam)
				continue;

			pBeam->source = pEnt->origin + m_vecPoints[i] * flScale + m_vecOffset[k] * flScale;
			pBeam->target = pEnt->origin + m_vecPoints[i + 1] * flScale + m_vecOffset[k] * flScale;
			pBeam->delta = pBeam->target - pBeam->source;
			m_pBeams[k][i]->brightness = m_flAlpha[k];
		}
	}
	return false;
}

// orig client 0x5f5f0
bool CTentSciMineSpin::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	for (int k = 0; k < 4; k++)
	{
		for (int i = 0; i < 8; i++)
		{
			if (m_pBeams[k][i])
			{
				Beam_Kill(m_pBeams[k][i]);
				m_pBeams[k][i] = NULL;
			}
		}
	}
	m_iEntity = 0;
	return true;
}

// --- CLTE 9: making health ("createhealth"): a ring bobbing up and down the player ---

// vtable client 0x1011d958, 0xd4 bytes
class CTentCreateHealth : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;			// +8
	BEAM* m_pBeams[10];		// +0xc
	Vector m_vecPoints[11];	// +0x34: the ring, closed
	Vector m_vecOffset;		// +0xb8
	float m_flSpeed;		// +0xc4: up the player and back
	float m_flAlpha;		// +0xc8
	int m_iState;			// +0xcc: 1 up, 2 down, 3 up, 4 down and fading
	float m_flLife;			// +0xd0: [Attributes] CreateHealthTimeTaken
};

// orig client 0x5f690. byte player
CClientTent* CLTE_CreateCreateHealth(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentCreateHealth*>(pCopy));

	CTentCreateHealth* p = new CTentCreateHealth;
	p->m_iEntity = READ_BYTE();
	for (int i = 0; i < 10; i++)
		p->m_pBeams[i] = NULL;

	if (0 != p->m_iEntity)
	{
		p->m_flLife = RC2Pref(RC2P_Attributes_CreateHealthTimeTaken).f;
		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
		if (!pEnt)
		{
			p->m_iEntity = 0;
		}
		else
		{
			gEngfuncs.pEventAPI->EV_PlaySound(p->m_iEntity, pEnt->origin, CHAN_VOICE, "weapons/mkhealth.wav", 1.0, 0.8, 0, 100);

			float flAngle = 0;
			for (int i = 0; i < 11; i++)
			{
				p->m_vecPoints[i].z = 0;
				p->m_vecPoints[i].x = cos(flAngle) * 24.0;
				p->m_vecPoints[i].y = sin(flAngle) * 24.0;
				flAngle = flAngle + 0.62831855;
			}
			p->m_vecOffset.z = -36;
			p->m_vecPoints[10].x = p->m_vecPoints[0].x;
			p->m_vecOffset.y = 0;
			p->m_vecOffset.x = 0;
			p->m_vecPoints[10].y = p->m_vecPoints[0].y;
			p->m_vecPoints[10].z = p->m_vecPoints[0].z;

			int iModel;
			if (gEngfuncs.CL_LoadModel("sprites/lgtning.spr", &iModel))
			{
				for (int i = 0; i < 10; i++)
				{
					Vector vecEnd = pEnt->origin + p->m_vecPoints[i + 1] + p->m_vecOffset;
					Vector vecStart = pEnt->origin + p->m_vecPoints[i] + p->m_vecOffset;
					const int iFrame = (int)gEngfuncs.pfnRandomFloat(0, 2);
					p->m_pBeams[i] = gEngfuncs.pEfxAPI->R_BeamPoints(vecStart, vecEnd, iModel, 0, 1.1, 0.1, 0.01, 1, iFrame, 15, 0.5, 0.5, 1);
				}
				p->m_flAlpha = 0.01;
				p->m_iState = 1;
				p->m_flSpeed = 0;
			}
		}
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x5fb30. The ring runs 168 units up and down per (CreateHealthTimeTaken - 1.5) s,
// turning at +-28, twice, fading on the last way down
bool CTentCreateHealth::Update(float flFrametime)
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

	float flMaxSpeed = 168.0 / (RC2Pref(RC2P_Attributes_CreateHealthTimeTaken).f - 1.5);
	if (flMaxSpeed < 1.0)
		flMaxSpeed = 56;
	const float flAccel = flMaxSpeed * 4.0 * flFrametime;
	const float flScale = CLTE_GetEntityScale(m_iEntity);

	if (m_vecOffset.z > 28)
	{
		if (m_iState == 1)
			m_iState = 2;
		else if (m_iState == 3)
			m_iState = 4;

		if (-flMaxSpeed < m_flSpeed)
			m_flSpeed = m_flSpeed - flAccel;
		if (-flMaxSpeed > m_flSpeed)
			m_flSpeed = -flMaxSpeed;
	}
	else if (m_vecOffset.z < -28)
	{
		if (m_iState == 2)
			m_iState = 3;

		if (m_flSpeed < flMaxSpeed)
			m_flSpeed = flAccel + m_flSpeed;
		if (m_flSpeed > flMaxSpeed)
			m_flSpeed = flMaxSpeed;
	}
	else if (m_flSpeed < 0)
	{
		m_flSpeed = -flMaxSpeed;
	}
	else
	{
		m_flSpeed = flMaxSpeed;
	}

	m_vecOffset.z = flFrametime * m_flSpeed + m_vecOffset.z;

	if (m_iState != 4)
	{
		if (m_flAlpha < 0.9)
			m_flAlpha = flFrametime * 3.6 + m_flAlpha;
		else
			m_flAlpha = 0.9;
	}
	else if (m_flAlpha > 0.01)
	{
		m_flAlpha = m_flAlpha - flFrametime * 3.6;
	}
	else
	{
		m_flAlpha = 0.01;
	}

	for (int i = 0; i < 10; i++)
	{
		BEAM* pBeam = m_pBeams[i];
		if (!pBeam)
			continue;

		pBeam->source = (m_vecPoints[i] * flScale + CLTE_GetOrigin(m_iEntity)) + m_vecOffset * flScale;
		m_pBeams[i]->target = (m_vecPoints[i + 1] * flScale + CLTE_GetOrigin(m_iEntity)) + m_vecOffset * flScale;
		m_pBeams[i]->delta = m_pBeams[i]->target - m_pBeams[i]->source;
		m_pBeams[i]->brightness = m_flAlpha;
	}
	return false;
}

// orig client 0x60150
bool CTentCreateHealth::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	for (int i = 0; i < 10; i++)
	{
		if (m_pBeams[i])
		{
			Beam_Kill(m_pBeams[i]);
			m_pBeams[i] = NULL;
		}
	}
	gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_VOICE, "weapons/mkhealth.wav");
	m_iEntity = 0;
	return true;
}
