/***
*
*	Rocket Crowbar 2: client effects CLTE 21-37 (see clte.h).
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

#ifndef SND_CHANGE_VOL
#define SND_CHANGE_VOL (1 << 6)   // as dlls/util.h
#define SND_CHANGE_PITCH (1 << 7) // as dlls/util.h
#endif

// --- CLTE 21: the crowbarang's glint (crowbarang.cpp RC2_CLTE_CROWBARANG_GLINT): half a second of red
// glow shell and light on it, with a blip ---

// vtable client 0x1011db8c, 0x18 bytes
class CTentCrowbarangGlint : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;	  // +8
	float m_flLife;	  // +0xc
	bool m_bVisible;  // +0x10: in the local player's current packet
	dlight_t* m_pLight; // +0x14
};

// orig client 0x69380. short entity
CClientTent* CLTE_CreateCrowbarangGlint(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentCrowbarangGlint*>(pCopy));

	CTentCrowbarangGlint* p = new CTentCrowbarangGlint;
	p->m_iEntity = READ_SHORT();
	p->m_flLife = 0.5;
	p->m_pLight = NULL;
	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
	if (!pEnt)
	{
		p->m_iEntity = 0;
		g_pClientTents[iSlot] = p;
		return p;
	}

	gEngfuncs.pEventAPI->EV_PlaySound(p->m_iEntity, pEnt->origin, CHAN_VOICE, "buttons/blip2.wav", 0.8, 0.7, 0, gEngfuncs.pfnRandomLong(105, 110));
	if (g_iClientDetail >= 0)
	{
		p->m_pLight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		if (p->m_pLight)
		{
			p->m_pLight->origin = pEnt->origin;
			p->m_pLight->radius = 8;
			p->m_pLight->color.r = 255;
			p->m_pLight->color.g = 0;
			p->m_pLight->color.b = 0;
			p->m_pLight->die = gEngfuncs.GetClientTime() + 5.0;
		}
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x69680. The glow rises and falls over the half second: f = 1 - 2 * life left,
// brightness f * (4 - 4f); the shell only while the entity is in the current packet
bool CTentCrowbarangGlint::Update(float flFrametime)
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
	if (pLocal)
	{
		const float flLife = m_flLife - flFrametime;
		m_flLife = flLife;
		if (flLife >= 0)
		{
			m_bVisible = !(pLocal->curstate.messagenum > pEnt->curstate.messagenum);

			float f = 1.0 - (flLife + flLife);
			if (f < 0)
				f = 0;
			else if (f >= 1)
				f = 1;
			f = f * (4.0 - f * 4.0);
			if (f < 0)
				f = 0;
			else if (f > 1)
				f = 1;

			if (m_bVisible)
			{
				pEnt->curstate.renderfx = kRenderFxGlowShell;
				pEnt->curstate.renderamt = (int)(f * 40);
				pEnt->curstate.rendercolor.r = (byte)(int)(f * 255);
				pEnt->curstate.rendercolor.g = 0;
				pEnt->curstate.rendercolor.b = 0;
			}
			if (m_pLight)
				m_pLight->radius = f * 72;
			return false;
		}
	}

	KillEntity(m_iEntity);
	m_iEntity = 0;
	return true;
}

// orig client 0x69840
bool CTentCrowbarangGlint::KillEntity(int iEnt)
{
	if (m_iEntity != iEnt)
		return false;

	gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_VOICE, "buttons/blip2.wav");
	if (m_pLight)
	{
		m_pLight->die = gEngfuncs.GetClientTime() + 0.01;
		m_pLight = NULL;
	}
	m_iEntity = 0;
	return true;
}

// --- CLTE 22: a tracer line (xbeam5), split in two past 32 units, that fades after half a second ---

// vtable client 0x1011dba4, 0x48 bytes
class CTentType22 : public CClientTent
{
public:
	bool Update(float flFrametime) override;

	Vector m_vecStart; // +8
	Vector m_vecEnd;   // +0x14
	Vector m_vecDir;   // +0x20
	int m_iColor;	   // +0x2c: 1 red, else blue
	float m_flLife;	   // +0x30
	int m_iUnused34;   // +0x34
	int m_iUnused38;   // +0x38
	int m_iState;	   // +0x3c: 0 waiting, 1 fading
	BEAM* m_pFar;	   // +0x40: from 32 out to the end
	BEAM* m_pNear;	   // +0x44
};

// orig client 0x698c0. coord x3 start, coord x3 end, byte color
CClientTent* CLTE_CreateTracer(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentType22*>(pCopy));

	CTentType22* p = new CTentType22;
	p->m_vecStart.x = READ_COORD();
	p->m_vecStart.y = READ_COORD();
	p->m_vecStart.z = READ_COORD();
	p->m_vecEnd.x = READ_COORD();
	p->m_vecEnd.y = READ_COORD();
	p->m_vecEnd.z = READ_COORD();
	p->m_iColor = READ_BYTE();
	p->m_vecDir = Vector(p->m_vecEnd.x - p->m_vecStart.x, p->m_vecEnd.y - p->m_vecStart.y, p->m_vecEnd.z - p->m_vecStart.z).Normalize();
	p->m_flLife = 0.5;
	p->m_iState = 0;
	p->m_pFar = NULL;
	p->m_pNear = NULL;

	Vector vecColor = Vector(0, 0, 1);
	if (p->m_iColor == 1)
		vecColor = Vector(1, 0, 0);

	int iModel;
	if (gEngfuncs.CL_LoadModel("sprites/xbeam5.spr", &iModel))
	{
		if ((p->m_vecStart - p->m_vecEnd).Length() > 32)
		{
			Vector vecMid = p->m_vecStart + p->m_vecDir * 32;
			p->m_pFar = gEngfuncs.pEfxAPI->R_BeamPoints(vecMid, p->m_vecEnd, iModel, 0, 1.1, 0, 0.6, -3, 0, 0, vecColor.x, vecColor.y, vecColor.z);
			p->m_pNear = gEngfuncs.pEfxAPI->R_BeamPoints(p->m_vecStart, vecMid, iModel, 0, 1.1, 0, 0.6, -3, 0, 0, vecColor.x, vecColor.y, vecColor.z);
		}
		else
		{
			p->m_pNear = gEngfuncs.pEfxAPI->R_BeamPoints(p->m_vecStart, p->m_vecEnd, iModel, 0, 1.1, 0, 0.6, -3, 0, 0, vecColor.x, vecColor.y, vecColor.z);
		}
		if (p->m_pNear)
			p->m_pNear->flags |= FBEAM_SHADEIN;
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// Fades a beam flRate a second; true once it is out (both beams then die)
static bool Type22_Fade(BEAM*& pBeam, BEAM*& pOther, float flFrametime, float flRate)
{
	pBeam->brightness = pBeam->brightness - flFrametime * flRate;
	if (pBeam->brightness > 0)
		return false;

	pBeam->brightness = 0.01;
	Beam_Kill(pBeam);
	pBeam = NULL;
	if (pOther)
	{
		pOther->brightness = 0.01;
		Beam_Kill(pOther);
		pOther = NULL;
	}
	return true;
}

// orig client 0x69d40
bool CTentType22::Update(float flFrametime)
{
	if (!m_pFar && !m_pNear)
		return true;

	if (m_iState == 0)
	{
		m_flLife = m_flLife - flFrametime;
		if (m_flLife < 0)
			m_iState = 1;
		return false;
	}
	if (m_iState != 1)
		return false;

	if (m_pFar && Type22_Fade(m_pFar, m_pNear, flFrametime, 1.2))
		return true;
	if (m_pNear && Type22_Fade(m_pNear, m_pFar, flFrametime, 1.2))
		return true;
	return false;
}

// --- CLTE 23: a crossbow bolt's smoke trail (crossbowbolts.cpp), pale rainbow colors ---

// vtable client 0x1011dbcc, 0x1c bytes
class CTentBoltTrail : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;
	Vector GetColor();

	BEAM* m_pBeam;	 // +8
	int m_iModel;	 // +0xc
	int m_iEntity;	 // +0x10
	float m_flHue;	 // +0x14: 0-3
	float m_flLife;	 // +0x18
};

// orig client 0x69f60. short bolt
CClientTent* CLTE_CreateBoltTrail(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentBoltTrail*>(pCopy));

	CTentBoltTrail* p = new CTentBoltTrail;
	p->m_iEntity = READ_SHORT();
	p->m_pBeam = NULL;
	if (0 != p->m_iEntity && gEngfuncs.CL_LoadModel("sprites/smoke.spr", &p->m_iModel))
	{
		p->m_flHue = gEngfuncs.pfnRandomFloat(0, 3);
		p->m_flLife = 120;
		const Vector vecColor = p->GetColor();
		p->m_pBeam = gEngfuncs.pEfxAPI->R_BeamFollow(p->m_iEntity, p->m_iModel, 2, 4, vecColor.x, vecColor.y, vecColor.z, 0.9);
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x6a1e0. The hue's color (red to green to blue), paled: 0.7 + 0.3 * each
Vector CTentBoltTrail::GetColor()
{
	float c[3];
	if (m_flHue > 1)
	{
		if (m_flHue > 2)
		{
			c[1] = 0;
			c[0] = m_flHue - 2;
			c[2] = 3 - m_flHue;
		}
		else
		{
			c[0] = 0;
			c[1] = 2 - m_flHue;
			c[2] = m_flHue - 1;
		}
	}
	else
	{
		c[2] = 0;
		c[1] = m_flHue;
		c[0] = 1 - m_flHue;
	}
	for (int i = 0; i < 3; i++)
		c[i] = c[i] * 0.3 + 0.7;
	return Vector(c[0], c[1], c[2]);
}

// orig client 0x6a2f0. The hue turns 4 a second (the original repeats GetColor's code)
bool CTentBoltTrail::Update(float flFrametime)
{
	if (0 == m_iModel)
		return true;

	m_flHue = flFrametime * 4 + m_flHue;
	if (m_flHue > 3)
		m_flHue = m_flHue - 3;

	if (!m_pBeam)
		return true;

	const Vector vecColor = GetColor();
	m_pBeam->r = vecColor.x;
	m_pBeam->g = vecColor.y;
	m_pBeam->b = vecColor.z;

	m_flLife = m_flLife - flFrametime;
	return !(m_flLife > 0);
}

// orig client 0x6a470. The trail is let go and the effect lasts 2 more seconds
bool CTentBoltTrail::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	if (m_pBeam)
	{
		m_pBeam->flags &= ~(FBEAM_STARTENTITY | FBEAM_ENDENTITY);
		m_pBeam->endEntity = 0;
		m_pBeam->startEntity = 0;
		m_pBeam = NULL;
	}
	m_flLife = 2;
	m_iEntity = 0;
	return false;
}

// --- CLTE 24: the impailer (crossbowbolts.cpp): a rope from the bolt to the body it is in, which
// bleeds while the blood color isn't 255 ---

// vtable client 0x1011dbe4, 0x38 bytes
class CTentImpailer : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;		 // +8: the bolt
	int m_iTarget;		 // +0xc: the body
	int m_iUnused[6];	 // +0x10. VERIFY: unused by every function here
	float m_flHeight;	 // +0x28: up the body from its origin, scaled with it
	int m_iBloodColor;	 // +0x2c
	float m_flNextBlood; // +0x30
	BEAM* m_pBeam;		 // +0x34
};

// orig client 0x6a4c0. short bolt, short body, byte blood color, coord height. Original bug kept: a
// missing body is read through NULL
CClientTent* CLTE_CreateImpailer(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentImpailer*>(pCopy));

	CTentImpailer* p = new CTentImpailer;
	p->m_iEntity = READ_SHORT();
	p->m_iTarget = READ_SHORT();
	p->m_iBloodColor = READ_BYTE();
	const float flHeight = READ_COORD();
	p->m_flNextBlood = 0;
	p->m_flHeight = flHeight;
	p->m_pBeam = NULL;

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
	if (!pEnt)
	{
		p->m_iEntity = 0;
		g_pClientTents[iSlot] = p;
		return p;
	}

	cl_entity_t* pTarget = gEngfuncs.GetEntityByIndex(p->m_iTarget);
	int iModel;
	if (gEngfuncs.CL_LoadModel("sprites/rope.spr", &iModel))
	{
		Vector vecEnd(pTarget->origin.x, pTarget->origin.y, pTarget->origin.z + p->m_flHeight);
		Vector vecStart = pEnt->origin;
		p->m_pBeam = gEngfuncs.pEfxAPI->R_BeamPoints(vecStart, vecEnd, iModel, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1);
		if (p->m_pBeam)
			p->m_pBeam->flags |= FBEAM_SOLID;
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x6a830. The rope follows both; with cl_detail not negative, blood at the body every
// 0.2-0.4 s
bool CTentImpailer::Update(float flFrametime)
{
	if (0 == m_iEntity)
		return true;

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iEntity);
	if (!pEnt || !gEngfuncs.GetEntityByIndex(m_iTarget))
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	if (m_pBeam)
	{
		const float flScale = CLTE_GetEntityScale(m_iTarget);
		m_pBeam->source = pEnt->origin;
		const float flUp = flScale * m_flHeight;
		const Vector vecTarget = CLTE_GetOrigin(m_iTarget);
		m_pBeam->target = Vector(vecTarget.x, vecTarget.y, flUp + vecTarget.z);
		m_pBeam->delta = m_pBeam->target - m_pBeam->source;
	}

	const float flNext = m_flNextBlood;
	m_flNextBlood = flNext - flFrametime;
	if (flNext - flFrametime < 0 && m_iBloodColor != 255)
	{
		m_flNextBlood = gEngfuncs.pfnRandomFloat(0.2, 0.4);
		if (g_iClientDetail >= 0)
		{
			int iBlood, iSpray;
			if (gEngfuncs.CL_LoadModel("sprites/blood.spr", &iBlood) && gEngfuncs.CL_LoadModel("sprites/bloodspray.spr", &iSpray))
			{
				const float flUp = CLTE_GetEntityScale(m_iTarget) * m_flHeight;
				const Vector vecTarget = CLTE_GetOrigin(m_iTarget);
				Vector vecBlood(vecTarget.x, vecTarget.y, flUp + vecTarget.z);
				gEngfuncs.pEfxAPI->R_BloodSprite(vecBlood, m_iBloodColor, iSpray, iBlood, gEngfuncs.pfnRandomFloat(6, 8));
			}
		}
	}
	return false;
}

// orig client 0x6ab30. Either end going ends it; the entity becomes -1
bool CTentImpailer::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity && iEnt != m_iTarget)
		return false;

	if (m_pBeam)
	{
		m_pBeam->brightness = 0.01;
		Beam_Kill(m_pBeam);
		m_pBeam = NULL;
	}
	m_iEntity = -1;
	return true;
}

// --- CLTE 25: a model stuck to an entity at an offset (RC2_CLTE_ATTACH, CLTE_ATTACHED,
// RC2_CLTE_CROWBARANG_EMBED: the beacon, the conc charge, the crowbarang in a wall or a body) ---

// vtable client 0x1011dbfc, 0x28 bytes
class CTentAttach : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;		// +8: what is stuck
	int m_iTarget;		// +0xc: what it is in; 0 the world
	Vector m_vecAngles; // +0x10: relative to the target's
	Vector m_vecOffset; // +0x1c: along the target's forward, right and up (world origin without one)
};

// orig client 0x6abc0. short entity, short target, coord x3 offset, coord x3 angles
CClientTent* CLTE_CreateAttach(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentAttach*>(pCopy));

	CTentAttach* p = new CTentAttach;
	p->m_iEntity = READ_SHORT();
	p->m_iTarget = READ_SHORT();
	p->m_vecOffset.x = READ_COORD();
	p->m_vecOffset.y = READ_COORD();
	p->m_vecOffset.z = READ_COORD();
	p->m_vecAngles.x = READ_COORD();
	p->m_vecAngles.y = READ_COORD();
	p->m_vecAngles.z = READ_COORD();
	if (!gEngfuncs.GetEntityByIndex(p->m_iEntity))
		p->m_iEntity = 0;
	else if (0 != p->m_iTarget && !gEngfuncs.GetEntityByIndex(p->m_iTarget))
		p->m_iTarget = 0;
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x6ae40. A player's pitch is negated for its vectors; anything else's for the angles
// the stuck entity gets (as CLTE 11's update does)
bool CTentAttach::Update(float flFrametime)
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

	if (0 == m_iTarget)
	{
		pEnt->origin = m_vecOffset;
		pEnt->angles = m_vecAngles;
		return false;
	}

	cl_entity_t* pTarget = gEngfuncs.GetEntityByIndex(m_iTarget);
	if (!pTarget)
	{
		KillEntity(m_iEntity);
		m_iTarget = 0;
		return true;
	}

	const float flScale = CLTE_GetEntityScale(m_iTarget);
	Vector vecAngles = pTarget->angles;
	if (0 != pTarget->player)
		vecAngles.x = -vecAngles.x;
	Vector vecForward, vecRight, vecUp;
	gEngfuncs.pfnAngleVectors(vecAngles, vecForward, vecRight, vecUp);

	const Vector vecUpOfs = (vecUp * m_vecOffset.z) * flScale;
	const Vector vecRightOfs = (vecRight * m_vecOffset.y) * flScale;
	const Vector vecForwardOfs = (vecForward * m_vecOffset.x) * flScale;
	const Vector vecOrigin = ((CLTE_GetOrigin(m_iTarget) + vecForwardOfs) + vecRightOfs) + vecUpOfs;

	if (0 == pTarget->player)
		vecAngles.x = -vecAngles.x;
	pEnt->origin = vecOrigin;
	pEnt->angles = vecAngles + m_vecAngles;
	return false;
}

// orig client 0x6b340
bool CTentAttach::KillEntity(int iEnt)
{
	return iEnt == m_iEntity || (m_iTarget > 0 && iEnt == m_iTarget);
}

// --- CLTE 26: a crossbow bolt's rope (crossbowbolts.cpp): from the bolt, or the body it holds, to the
// shooter ---

// vtable client 0x1011dc14, 0x34 bytes
class CTentBoltRope : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;		// +8: the bolt
	int m_iShooter;		// +0xc
	int m_iTarget;		// +0x10: the body on the rope; 0 or less none
	int m_iUnused[3];	// +0x14. VERIFY: unused here
	Vector m_vecUnused; // +0x20. VERIFY: unused here (copied as a Vector)
	float m_flHeight;	// +0x2c: up the body
	BEAM* m_pBeam;		// +0x30
};

// orig client 0x6b370. short bolt, byte shooter, short body, coord height. A rope already on the
// bolt goes first. Original bug kept: a missing shooter is read through NULL
CClientTent* CLTE_CreateBoltRope(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentBoltRope*>(pCopy));

	CTentBoltRope* p = new CTentBoltRope;
	p->m_iEntity = READ_SHORT();
	p->m_iShooter = READ_BYTE();
	p->m_iTarget = READ_SHORT();
	p->m_flHeight = READ_COORD();
	p->m_pBeam = NULL;

	// the original clears its own type around this, though it isn't in the list yet
	const int iType = p->m_iType;
	p->m_iType = 0;
	CLTE_KillByEntity(p->m_iEntity, 26);
	p->m_iType = iType;

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
	if (!pEnt)
	{
		p->m_iEntity = 0;
	}
	else
	{
		cl_entity_t* pShooter = gEngfuncs.GetEntityByIndex(p->m_iShooter);
		if (p->m_iTarget < 1 || gEngfuncs.GetEntityByIndex(p->m_iTarget))
		{
			int iModel;
			if (gEngfuncs.CL_LoadModel("sprites/rope.spr", &iModel))
			{
				Vector vecEnd(pShooter->origin.x, pShooter->origin.y, pShooter->origin.z + p->m_flHeight);
				Vector vecStart = pEnt->origin;
				p->m_pBeam = gEngfuncs.pEfxAPI->R_BeamPoints(vecStart, vecEnd, iModel, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1);
				if (p->m_pBeam)
					p->m_pBeam->flags |= FBEAM_SOLID;
			}
		}
		else
		{
			p->m_iEntity = 0;
		}
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x6b680. The rope runs from the body (its height up, scaled) or 4 above the bolt, to 8
// above the shooter (scaled)
bool CTentBoltRope::Update(float flFrametime)
{
	if (0 == m_iEntity)
		return true;

	if (!gEngfuncs.GetEntityByIndex(m_iEntity) || !gEngfuncs.GetEntityByIndex(m_iShooter))
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	cl_entity_t* pTarget = NULL;
	if (m_iTarget > 0)
	{
		pTarget = gEngfuncs.GetEntityByIndex(m_iTarget);
		if (!pTarget)
		{
			KillEntity(m_iEntity);
			m_iEntity = 0;
			return true;
		}
	}

	if (m_pBeam)
	{
		if (!pTarget)
		{
			const Vector vecBolt = CLTE_GetOrigin(m_iEntity);
			m_pBeam->source = Vector(vecBolt.x, vecBolt.y, vecBolt.z + 4.0);
		}
		else
		{
			const float flUp = CLTE_GetEntityScale(m_iTarget) * m_flHeight;
			const Vector vecBody = CLTE_GetOrigin(m_iTarget);
			m_pBeam->source = Vector(vecBody.x, vecBody.y, flUp + vecBody.z);
		}

		const float flScale = CLTE_GetEntityScale(m_iShooter);
		const Vector vecShooter = CLTE_GetOrigin(m_iShooter);
		m_pBeam->target = Vector(vecShooter.x, vecShooter.y, flScale * 8.0 + vecShooter.z);
		m_pBeam->delta = m_pBeam->target - m_pBeam->source;
	}
	return false;
}

// orig client 0x6b920. Any of the three going ends it; the bolt becomes -1
bool CTentBoltRope::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity && iEnt != m_iShooter && (m_iTarget < 1 || iEnt != m_iTarget))
		return false;

	if (m_pBeam)
	{
		m_pBeam->brightness = 0.01;
		Beam_Kill(m_pBeam);
		m_pBeam = NULL;
	}
	m_iEntity = -1;
	return true;
}

// --- CLTE 27: a colored tracer line (xbeam5), as CLTE 22: split past 48, a second before fading ---

// vtable client 0x1011dc30, 0x48 bytes
class CTentType27 : public CClientTent
{
public:
	bool Update(float flFrametime) override;

	Vector m_vecStart; // +8
	Vector m_vecEnd;   // +0x14
	Vector m_vecDir;   // +0x20
	int m_iColor;	   // +0x2c: 1 yellow, 2 magenta, 3 orange, else green
	float m_flLife;	   // +0x30
	int m_iUnused34;   // +0x34
	int m_iUnused38;   // +0x38
	int m_iState;	   // +0x3c
	BEAM* m_pFar;	   // +0x40
	BEAM* m_pNear;	   // +0x44
};

// orig client 0x6b9c0. coord x3 start, coord x3 end, byte color
CClientTent* CLTE_CreateColorTracer(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentType27*>(pCopy));

	CTentType27* p = new CTentType27;
	p->m_vecStart.x = READ_COORD();
	p->m_vecStart.y = READ_COORD();
	p->m_vecStart.z = READ_COORD();
	p->m_vecEnd.x = READ_COORD();
	p->m_vecEnd.y = READ_COORD();
	p->m_vecEnd.z = READ_COORD();
	p->m_iColor = READ_BYTE();
	p->m_vecDir = Vector(p->m_vecEnd.x - p->m_vecStart.x, p->m_vecEnd.y - p->m_vecStart.y, p->m_vecEnd.z - p->m_vecStart.z).Normalize();
	p->m_flLife = 1.0;
	p->m_iState = 0;
	p->m_pFar = NULL;
	p->m_pNear = NULL;

	Vector vecColor = Vector(0, 1, 0);
	if (p->m_iColor == 1)
		vecColor = Vector(1, 1, 0);
	else if (p->m_iColor == 2)
		vecColor = Vector(1, 0, 1);
	else if (p->m_iColor == 3)
		vecColor = Vector(1, 0.5, 0);

	int iModel;
	if (gEngfuncs.CL_LoadModel("sprites/xbeam5.spr", &iModel))
	{
		if ((p->m_vecStart - p->m_vecEnd).Length() > 48)
		{
			Vector vecMid = p->m_vecStart + p->m_vecDir * 48;
			p->m_pFar = gEngfuncs.pEfxAPI->R_BeamPoints(vecMid, p->m_vecEnd, iModel, 0, 1.5, 0, 0.7, -4, 0, 0, vecColor.x, vecColor.y, vecColor.z);
			p->m_pNear = gEngfuncs.pEfxAPI->R_BeamPoints(p->m_vecStart, vecMid, iModel, 0, 1.5, 0, 0.7, -4, 0, 0, vecColor.x, vecColor.y, vecColor.z);
		}
		else
		{
			p->m_pNear = gEngfuncs.pEfxAPI->R_BeamPoints(p->m_vecStart, p->m_vecEnd, iModel, 0, 1.5, 0, 0.7, -4, 0, 0, vecColor.x, vecColor.y, vecColor.z);
		}
		if (p->m_pNear)
			p->m_pNear->flags |= FBEAM_SHADEIN;
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x6be90. As CLTE 22's, fading 0.7 a second
bool CTentType27::Update(float flFrametime)
{
	if (!m_pFar && !m_pNear)
		return true;

	if (m_iState == 0)
	{
		m_flLife = m_flLife - flFrametime;
		if (m_flLife < 0)
			m_iState = 1;
		return false;
	}
	if (m_iState != 1)
		return false;

	if (m_pFar && Type22_Fade(m_pFar, m_pNear, flFrametime, 0.7))
		return true;
	if (m_pNear && Type22_Fade(m_pNear, m_pFar, flFrametime, 0.7))
		return true;
	return false;
}

// --- CLTE 28: the crossbow's charge (its whine and the view model's blue shell) ---

// vtable client 0x1011dc48, 0x18 bytes
class CTentCrossbowCharge : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;		// +8: the player
	float m_flTimeLeft; // +0xc
	float m_flTime;		// +0x10
	int m_iMode;		// +0x14: 0 zoomed secondary, 1 zoomed tertiary, 2 secondary
};

// orig client 0x6c0b0. byte player, byte mode. Original bug kept: another mode leaves the time unset
CClientTent* CLTE_CreateCrossbowCharge(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentCrossbowCharge*>(pCopy));

	CTentCrossbowCharge* p = new CTentCrossbowCharge;
	p->m_iEntity = READ_BYTE();
	p->m_iMode = READ_BYTE();
	if (0 != p->m_iEntity)
	{
		if (p->m_iMode == 0)
			p->m_flTimeLeft = RC2Pref(RC2P_Crossbow_ZoomedSecondaryChargeTime).f;
		else if (p->m_iMode == 1)
			p->m_flTimeLeft = RC2Pref(RC2P_Crossbow_ZoomedTertiaryChargeTime).f;
		else if (p->m_iMode == 2)
			p->m_flTimeLeft = RC2Pref(RC2P_Crossbow_SecondaryChargeTime).f;
		p->m_flTime = p->m_flTimeLeft;

		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
		if (pEnt)
			gEngfuncs.pEventAPI->EV_PlaySound(p->m_iEntity, pEnt->origin, CHAN_WEAPON, "ambience/labdrone2.wav", 0.3, 0.7, 0, 90);
		else
			p->m_iEntity = 0;
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x6c330. The whine rises (pitch 90-160, louder), the player's light override and the
// local view model's shell brighten as the charge fills
bool CTentCrossbowCharge::Update(float flFrametime)
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

	m_flTimeLeft = m_flTimeLeft - flFrametime;
	if (m_flTimeLeft < 0)
	{
		KillEntity(m_iEntity);
		return true;
	}

	float f = 1.0 - m_flTimeLeft / m_flTime;
	if (f < 0)
		f = 0;
	else if (f > 1)
		f = 1;

	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntity, pEnt->origin, CHAN_WEAPON, "ambience/labdrone2.wav", f * 0.7 + 0.3, 0.7,
		SND_CHANGE_VOL | SND_CHANGE_PITCH, (int)(f * 70 + 90));

	const float flLight = f * 24;
	const Vector vecColor(f * 127, f * 127, f * 255);
	CLTE_SetPlayerOverride(m_iEntity, vecColor.x, vecColor.y, vecColor.z, flLight);

	if (0 != gEngfuncs.pEventAPI->EV_IsLocal(m_iEntity - 1))
	{
		cl_entity_t* pView = gEngfuncs.GetViewModel();
		if (pView)
		{
			pView->curstate.renderfx = kRenderFxGlowShell;
			pView->curstate.renderamt = (int)(f * 16);
			pView->curstate.rendercolor.b = (byte)(int)(f * 255);
			const byte c = (byte)(int)(f * 127);
			pView->curstate.rendercolor.g = c;
			pView->curstate.rendercolor.r = c;
		}
	}
	return false;
}

// orig client 0x6c560
bool CTentCrossbowCharge::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_WEAPON, "ambience/labdrone2.wav");
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

// --- CLTE 29: a crossbow bolt's glow (crossbowbolts.cpp): two red trails spiraling round its flight
// (with cl_detail negative, a plain red beam) ---

// vtable client 0x1011dc78, 0xb0 bytes
class CTentBoltGlow : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEnt) override;
	void AddPoints();

	BEAM* m_pBeam;			// +8: low detail only
	int m_iEntity;			// +0xc
	CTentTrail m_Trails[2]; // +0x10
	Vector m_vecOrigin;		// +0x60
	Vector m_vecVel[2];		// +0x6c: each trail's new points' velocity
	Vector m_vecAccel[2];	// +0x84
	float m_flAngle;		// +0x9c: the spiral's roll
	float m_flSpin;			// +0xa0: +-480 a second
	float m_flNextPoint;	// +0xa4
	model_s* m_pModel;		// +0xa8: sprites/gradsmoke.spr
	bool m_bVisible;		// +0xac
};

// The spiral's velocities and accelerations from the bolt's angles and the roll
static void BoltGlow_Vectors(CTentBoltGlow* p, cl_entity_t* pEnt)
{
	Vector vecAngles = pEnt->angles + Vector(0, 0, p->m_flAngle);
	vecAngles.x = -vecAngles.x;
	Vector vecForward, vecRight, vecUp;
	gEngfuncs.pfnAngleVectors(vecAngles, vecForward, vecRight, vecUp);

	p->m_vecVel[0] = vecRight * 32 - vecForward * 8;
	p->m_vecAccel[0] = vecRight * 120;
	p->m_vecVel[1] = vecRight * -32 - vecForward * 8;
	p->m_vecAccel[1] = vecRight * -120;
}

void CTentBoltGlow::AddPoints()
{
	m_Trails[0].AddPoint(m_vecOrigin, m_vecVel[0], m_vecAccel[0], Vector(1, 0, 0), 0.9, 3, 0.9, NULL, NULL);
	m_Trails[1].AddPoint(m_vecOrigin, m_vecVel[1], m_vecAccel[1], Vector(1, 0, 0), 0.9, 3, 0.9, NULL, NULL);
}

// orig client 0x6c600. short bolt. The copy goes through an out-of-line copy constructor (client
// 0x99c30, another package's). VERIFY: what that constructor does with the trails (the implicit one
// here copies their pointers)
CClientTent* CLTE_CreateBoltGlow(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentBoltGlow*>(pCopy));

	CTentBoltGlow* p = new CTentBoltGlow;
	p->m_iEntity = READ_SHORT();
	p->m_pBeam = NULL;
	if (0 != p->m_iEntity)
	{
		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
		if (!pEnt)
		{
			p->m_iEntity = 0;
			g_pClientTents[iSlot] = p;
			return p;
		}

		int iModel;
		if (g_iClientDetail < 0)
		{
			if (gEngfuncs.CL_LoadModel("sprites/smoke.spr", &iModel))
				p->m_pBeam = gEngfuncs.pEfxAPI->R_BeamFollow(p->m_iEntity, iModel, 1, 1, 1, 0, 0, 0.6);
		}
		else
		{
			p->m_vecOrigin = CLTE_GetOrigin(p->m_iEntity);
			p->m_flAngle = gEngfuncs.pfnRandomFloat(0, 360);
			p->m_flSpin = (float)((0 != gEngfuncs.pfnRandomLong(0, 1)) ? 1 : -1) * 480.0;
			BoltGlow_Vectors(p, pEnt);
			p->AddPoints();
			if (g_iClientDetail < 1)
				p->m_flNextPoint = 0.075;
			else
				p->m_flNextPoint = 0.05;
			p->m_pModel = gEngfuncs.CL_LoadModel("sprites/gradsmoke.spr", &iModel);
		}
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x6cc20. A point on each trail every 0.05 s (0.075 at cl_detail 0); out of the current
// packet the trails only fade. Done once the bolt is gone and both trails have faded
bool CTentBoltGlow::Update(float flFrametime)
{
	int iCount0, iCount1;
	if (0 == m_iEntity)
	{
		if (g_iClientDetail < 0)
			return true;
		iCount0 = m_Trails[0].Update(flFrametime);
		iCount1 = m_Trails[1].Update(flFrametime);
	}
	else
	{
		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iEntity);
		if (pEnt)
		{
			m_bVisible = true;
			cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
			if (pLocal && pEnt->curstate.messagenum < pLocal->curstate.messagenum)
			{
				m_Trails[0].Update(flFrametime);
				m_Trails[1].Update(flFrametime);
				m_bVisible = false;
				return false;
			}

			if (g_iClientDetail >= 0)
			{
				m_vecOrigin = CLTE_GetOrigin(m_iEntity);
				m_flAngle = flFrametime * m_flSpin + m_flAngle;
				BoltGlow_Vectors(this, pEnt);

				const float flNext = m_flNextPoint;
				m_flNextPoint = flNext - flFrametime;
				if (flNext - flFrametime <= 0)
				{
					AddPoints();
					if (g_iClientDetail == 0)
						m_flNextPoint = 0.075;
					else
						m_flNextPoint = 0.05;
				}
				m_Trails[0].Update(flFrametime);
				m_Trails[1].Update(flFrametime);
			}
			return false;
		}

		KillEntity(m_iEntity);
		m_iEntity = 0;
		if (g_iClientDetail < 0)
			return true;
		iCount0 = m_Trails[0].Update(flFrametime);
		iCount1 = m_Trails[1].Update(flFrametime);
	}

	if ((iCount1 > 0 || iCount0 > 0) && m_bVisible)
		return false;
	return true;
}

// orig client 0x6d2a0. The trails, additive; while the bolt flies they run to it
void CTentBoltGlow::DrawTransparent()
{
	if (!m_bVisible || g_iClientDetail < 0)
		return;
	if (!m_Trails[0].CanDraw() && !m_Trails[1].CanDraw())
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pModel)
		gEngfuncs.pTriAPI->SpriteTexture(m_pModel, 0);

	if (0 != m_iEntity)
	{
		m_Trails[0].DrawToHead(m_vecOrigin, Vector(1, 0, 0), 0.9, 3, true);
		m_Trails[1].DrawToHead(m_vecOrigin, Vector(1, 0, 0), 0.9, 3, true);
	}
	else
	{
		m_Trails[0].Draw(true);
		m_Trails[1].Draw(true);
	}
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// orig client 0x6d430. The trails get a last point at the bolt and fade on (false), unless they
// aren't drawn
bool CTentBoltGlow::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	if (m_pBeam)
	{
		m_pBeam->flags &= ~(FBEAM_STARTENTITY | FBEAM_ENDENTITY);
		m_pBeam->endEntity = 0;
		m_pBeam->startEntity = 0;
		m_pBeam = NULL;
	}
	m_iEntity = 0;
	if (g_iClientDetail < 0 || !m_bVisible)
		return true;

	AddPoints();
	return false;
}

// --- CLTE 30: an explosive crossbow bolt armed (crossbowbolts.cpp): a red glow shell and lights
// growing, and a rising wind, over [Crossbow] ExplodeBoltDetonateDelay ---

// vtable client 0x1011dc98, 0x1c bytes
class CTentBoltArmed : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;		// +8
	float m_flTimeLeft; // +0xc
	float m_flTime;		// +0x10
	dlight_t* m_pDLight; // +0x14
	dlight_t* m_pELight; // +0x18
};

// orig client 0x6d5b0. short bolt. Original bug kept: the entity light's decay is written to the
// dynamic light's
CClientTent* CLTE_CreateBoltArmed(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentBoltArmed*>(pCopy));

	CTentBoltArmed* p = new CTentBoltArmed;
	p->m_iEntity = READ_SHORT();
	// Original bug (crash), guarded: the original allocates the lights only with cl_detail not
	// negative and leaves them unset otherwise; Update and KillEntity write through them
	p->m_pDLight = NULL;
	p->m_pELight = NULL;
	if (0 != p->m_iEntity)
	{
		p->m_flTimeLeft = RC2Pref(RC2P_Crossbow_ExplodeBoltDetonateDelay).f;
		p->m_flTime = RC2Pref(RC2P_Crossbow_ExplodeBoltDetonateDelay).f;
		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
		if (!pEnt)
		{
			p->m_iEntity = 0;
			g_pClientTents[iSlot] = p;
			return p;
		}

		if (g_iClientDetail >= 0)
		{
			p->m_pDLight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
			if (p->m_pDLight)
			{
				p->m_pDLight->origin = pEnt->origin;
				p->m_pDLight->radius = 1;
				p->m_pDLight->decay = 0;
				p->m_pDLight->color.r = 255;
				p->m_pDLight->color.g = 0;
				p->m_pDLight->color.b = 0;
				p->m_pDLight->die = p->m_flTime + p->m_flTime + gEngfuncs.GetClientTime();
			}
			p->m_pELight = gEngfuncs.pEfxAPI->CL_AllocElight(0);
			if (p->m_pELight)
			{
				p->m_pELight->origin = pEnt->origin;
				p->m_pELight->radius = 1;
				p->m_pDLight->decay = 0;
				p->m_pELight->color.r = 255;
				p->m_pELight->color.g = 0;
				p->m_pELight->color.b = 0;
				p->m_pELight->die = p->m_flTime + p->m_flTime + gEngfuncs.GetClientTime();
			}
		}
		gEngfuncs.pEventAPI->EV_PlaySound(p->m_iEntity, pEnt->origin, CHAN_WEAPON, "ambience/alienwind2.wav", 0.3, 0.7, 0, 80);
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x6d950. It never ends by itself: the bolt's removal ends it. Original bug kept: the
// entity light's radius is written to the dynamic light, so it never grows
bool CTentBoltArmed::Update(float flFrametime)
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

	m_flTimeLeft = m_flTimeLeft - flFrametime;
	float f = 1.0 - m_flTimeLeft / m_flTime;
	if (f < 0)
		f = 0;
	else if (f > 1)
		f = 1;

	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntity, pEnt->origin, CHAN_WEAPON, "ambience/alienwind2.wav", f * 0.7 + 0.3, 0.7,
		SND_CHANGE_VOL | SND_CHANGE_PITCH, (int)(f * 60 + 80));

	pEnt->curstate.renderfx = kRenderFxGlowShell;
	pEnt->curstate.rendercolor.r = (byte)(int)(f * 255);
	pEnt->curstate.rendercolor.g = 0;
	pEnt->curstate.rendercolor.b = 0;
	pEnt->curstate.renderamt = (int)(f * 16);

	if (m_pDLight)
	{
		m_pDLight->radius = f * 100;
		m_pDLight->origin = pEnt->origin;
	}
	if (m_pELight)
	{
		m_pDLight->radius = f * 90;
		m_pELight->origin = pEnt->origin;
	}
	return false;
}

// orig client 0x6db30
bool CTentBoltArmed::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_WEAPON, "ambience/alienwind2.wav");
	if (m_pDLight)
		m_pDLight->die = gEngfuncs.GetClientTime() + 0.01;
	if (m_pELight)
		m_pELight->die = gEngfuncs.GetClientTime() + 0.01;
	m_iEntity = 0;
	return true;
}

// --- CLTE 31: the suck mine's rope (suckmine.cpp CLTE_SUCKMINE_ROPE): from 48 above the mine to the
// middle of its target ---

// vtable client 0x1011dccc, 0x24 bytes
class CTentSuckMineRope : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;	   // +8: the mine
	int m_iTarget;	   // +0xc
	float m_flHeight;  // +0x10: up the target, scaled with it
	BEAM* m_pBeam;	   // +0x14
	Vector m_vecStart; // +0x18
};

// orig client 0x6dbc0. short mine, byte target. Original bug kept: a missing target is read
// through NULL
CClientTent* CLTE_CreateSuckMineRope(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentSuckMineRope*>(pCopy));

	CTentSuckMineRope* p = new CTentSuckMineRope;
	p->m_iEntity = READ_SHORT();
	p->m_iTarget = READ_BYTE();
	p->m_pBeam = NULL;

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
	if (!pEnt)
	{
		p->m_iEntity = 0;
		g_pClientTents[iSlot] = p;
		return p;
	}

	cl_entity_t* pTarget = gEngfuncs.GetEntityByIndex(p->m_iTarget);
	p->m_vecStart = Vector(pEnt->origin.x, pEnt->origin.y, pEnt->origin.z + 48.0);
	const float flRandom = gEngfuncs.pfnRandomFloat(-8, 8);
	p->m_flHeight = (pTarget->curstate.maxs.z + pTarget->curstate.mins.z) * 0.5 + flRandom;

	int iModel;
	if (gEngfuncs.CL_LoadModel("sprites/rope.spr", &iModel))
	{
		Vector vecEnd = pTarget->origin + Vector(0, 0, p->m_flHeight);
		p->m_pBeam = gEngfuncs.pEfxAPI->R_BeamPoints(p->m_vecStart, vecEnd, iModel, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1);
		if (p->m_pBeam)
			p->m_pBeam->flags |= FBEAM_SOLID;
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x6df50
bool CTentSuckMineRope::Update(float flFrametime)
{
	if (0 == m_iEntity)
		return true;

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iEntity);
	if (!pEnt || !gEngfuncs.GetEntityByIndex(m_iTarget))
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	m_vecStart = Vector(pEnt->origin.x, pEnt->origin.y, pEnt->origin.z + 48.0);
	const float flScale = CLTE_GetEntityScale(m_iTarget);
	const float flHeight = m_flHeight;
	const Vector vecTarget = CLTE_GetOrigin(m_iTarget);
	if (m_pBeam)
	{
		m_pBeam->source = m_vecStart;
		m_pBeam->target = Vector(vecTarget.x, vecTarget.y, flScale * flHeight + vecTarget.z);
		m_pBeam->delta = m_pBeam->target - m_pBeam->source;
	}
	return false;
}

// orig client 0x6e130
bool CTentSuckMineRope::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity && iEnt != m_iTarget)
		return false;

	if (m_pBeam)
	{
		m_pBeam->brightness = 0.01;
		Beam_Kill(m_pBeam);
		m_pBeam = NULL;
	}
	m_iEntity = -1;
	return true;
}

// --- CLTE 32: the sphere mine's sparks (spheremine.cpp): yellow flares thrown back off its path
// as it rolls, and a flare on it ---

#define SPHEREMINE_SPARKS 100 // name ours

// vtable client 0x1011dce4, 0x1034 bytes
class CTentSphereMine : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	void DrawTransparent() override;
	bool KillEntity(int iEnt) override;
	void Spawn(int i, const Vector& vecOrigin, const Vector& vecForward, const Vector& vecRight, const Vector& vecUp);
	void Corner(Vector vecPos, float flRight, float flUp, float s, float t);

	int m_iEntity;							 // +8
	Vector m_vecOrigin;						 // +0xc: last frame's
	Vector m_vecDir;						 // +0x18: the way it last moved
	Vector m_vecPos[SPHEREMINE_SPARKS];		 // +0x24
	Vector m_vecVel[SPHEREMINE_SPARKS];		 // +0x4d4
	Vector m_vecAccel[SPHEREMINE_SPARKS];	 // +0x984
	float m_flAlpha[SPHEREMINE_SPARKS];		 // +0xe34
	bool m_bActive[SPHEREMINE_SPARKS];		 // +0xfc4
	model_s* m_pModel;						 // +0x1028: sprites/sqflare.spr
	float m_flSpawn;						 // +0x102c: sparks owed
	bool m_bDone;							 // +0x1030: the mine is gone
	bool m_bVisible;						 // +0x1031
};

// orig client 0x6e1c0. short mine
CClientTent* CLTE_CreateSphereMine(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentSphereMine*>(pCopy));

	CTentSphereMine* p = new CTentSphereMine;
	p->m_iEntity = READ_SHORT();
	p->m_bVisible = true;
	p->m_bDone = false;
	for (int i = 0; i < SPHEREMINE_SPARKS; i++)
		p->m_bActive[i] = false;
	p->m_flSpawn = 5.0;
	p->m_vecOrigin.x = 0;
	p->m_pModel = NULL;
	p->m_vecOrigin.y = 0;
	p->m_vecOrigin.z = 0;
	p->m_vecDir = Vector(0, 0, 1);

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
	if (!pEnt)
	{
		p->m_iEntity = 0;
		g_pClientTents[iSlot] = p;
		return p;
	}

	p->m_vecOrigin = pEnt->origin;
	int iModel;
	p->m_pModel = gEngfuncs.CL_LoadModel("sprites/sqflare.spr", &iModel);
	g_pClientTents[iSlot] = p;
	return p;
}

// Inline in the original's update (twice): spark i starts somewhere along this frame's roll, thrown
// back at 60-80 and pushed 500-600 out sideways
void CTentSphereMine::Spawn(int i, const Vector& vecOrigin, const Vector& vecForward, const Vector& vecRight, const Vector& vecUp)
{
	m_bActive[i] = true;
	m_flAlpha[i] = 0.9;

	const Vector vecMoved = vecOrigin - m_vecOrigin;
	m_vecPos[i] = vecMoved * gEngfuncs.pfnRandomFloat(0, 1) + m_vecOrigin;
	m_vecVel[i] = vecForward * gEngfuncs.pfnRandomFloat(-60, -80);

	do
	{
		const Vector vecUpPart = vecUp * gEngfuncs.pfnRandomFloat(-1, 1);
		const Vector vecRightPart = vecRight * gEngfuncs.pfnRandomFloat(-1, 1);
		m_vecAccel[i] = vecUpPart + vecRightPart;
	} while (m_vecAccel[i].Length() <= 0.01);

	m_vecAccel[i] = m_vecAccel[i].Normalize() * gEngfuncs.pfnRandomFloat(500, 600);
}

// orig client 0x6e5d0. 80 sparks a second (40 at cl_detail 0, 20 below) into free slots, the rest
// into random ones; each fades 2 a second. Out of the current packet nothing moves. Once the mine
// is gone it is done when the sparks are (or at once if it isn't drawn)
bool CTentSphereMine::Update(float flFrametime)
{
	Vector vecOrigin = m_vecOrigin;
	Vector vecForward, vecRight, vecUp;

	if (!m_bDone)
	{
		if (m_iEntity <= 0)
			return true;

		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iEntity);
		if (!pEnt)
		{
			KillEntity(m_iEntity);
			m_iEntity = 0;
			return true;
		}

		m_bVisible = true;
		cl_entity_t* pLocal = gEngfuncs.GetLocalPlayer();
		if (pLocal && pLocal->curstate.messagenum > pEnt->curstate.messagenum)
		{
			m_bVisible = false;
			return false;
		}

		vecOrigin = pEnt->origin;
		Vector vecAngles;
		VectorAngles(m_vecDir, vecAngles);
		vecAngles.x = -vecAngles.x;
		gEngfuncs.pfnAngleVectors(vecAngles, vecForward, vecRight, vecUp);

		if (g_iClientDetail >= 1)
			m_flSpawn = flFrametime * 80 + m_flSpawn;
		else if (g_iClientDetail == 0)
			m_flSpawn = flFrametime * 40 + m_flSpawn;
		else
			m_flSpawn = flFrametime * 20 + m_flSpawn;
	}
	else
	{
		m_flSpawn = 0;
	}

	bool bAny = false;
	for (int i = 0; i < SPHEREMINE_SPARKS; i++)
	{
		if (!m_bActive[i])
		{
			if (m_flSpawn < 1.0 || m_bDone)
				continue;
			m_flSpawn = m_flSpawn - 1.0;
			Spawn(i, vecOrigin, vecForward, vecRight, vecUp);
		}

		m_flAlpha[i] = m_flAlpha[i] - (flFrametime + flFrametime);
		if (m_flAlpha[i] <= 0)
		{
			m_bActive[i] = false;
			m_flAlpha[i] = 0;
			continue;
		}

		bAny = true;
		m_vecVel[i] = m_vecAccel[i] * flFrametime + m_vecVel[i];
		m_vecPos[i] = m_vecVel[i] * flFrametime + m_vecPos[i];
	}

	if (m_bDone && (!bAny || !m_bVisible))
	{
		m_iEntity = 0;
		return true;
	}

	while (m_flSpawn >= 1.0 && !m_bDone)
	{
		m_flSpawn = m_flSpawn - 1.0;
		Spawn(gEngfuncs.pfnRandomLong(0, SPHEREMINE_SPARKS - 1), vecOrigin, vecForward, vecRight, vecUp);
	}

	if (!(vecOrigin == m_vecOrigin))
	{
		m_vecDir = (vecOrigin - m_vecOrigin).Normalize();
		m_vecOrigin = vecOrigin;
	}
	return false;
}

// orig client 0x6f610 is Vector's operator==, emitted out of line here (vector.h's here)

// orig client 0x6f660. The mine going leaves the sparks to fade (false)
bool CTentSphereMine::KillEntity(int iEnt)
{
	if (iEnt == m_iEntity)
	{
		m_iEntity = 0;
		m_bDone = true;
	}
	return false;
}

// orig client 0x6fd00. A corner of a view-facing quad
void CTentSphereMine::Corner(Vector vecPos, float flRight, float flUp, float s, float t)
{
	const float x = g_vecTentRight.x * flRight + vecPos.x + g_vecTentUp.x * flUp;
	const float y = g_vecTentRight.y * flRight + vecPos.y + g_vecTentUp.y * flUp;
	const float z = g_vecTentRight.z * flRight + vecPos.z + g_vecTentUp.z * flUp;
	gEngfuncs.pTriAPI->TexCoord2f(s, t);
	gEngfuncs.pTriAPI->Vertex3f(x, y, z);
}

// orig client 0x6f690. The sparks 3 wide, and while the mine is there a 6-wide flare on it
void CTentSphereMine::DrawTransparent()
{
	if (!m_bVisible)
		return;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	if (m_pModel)
		gEngfuncs.pTriAPI->SpriteTexture(m_pModel, 0);
	gEngfuncs.pTriAPI->Begin(TRI_QUADS);

	for (int i = 0; i < SPHEREMINE_SPARKS; i++)
	{
		if (!m_bActive[i])
			continue;
		const float flAlpha = m_flAlpha[i];
		if (!(flAlpha > 0))
			continue;

		gEngfuncs.pTriAPI->Color4f(1, 1, 0.25, flAlpha);
		gEngfuncs.pTriAPI->Brightness(m_flAlpha[i]);

		Vector v = (m_vecPos[i] + g_vecTentRight * 3) + g_vecTentUp * 3;
		gEngfuncs.pTriAPI->TexCoord2f(0, 0);
		gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);
		v = (m_vecPos[i] + g_vecTentRight * -3) + g_vecTentUp * 3;
		gEngfuncs.pTriAPI->TexCoord2f(1, 0);
		gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);
		v = (m_vecPos[i] + g_vecTentRight * -3) + g_vecTentUp * -3;
		gEngfuncs.pTriAPI->TexCoord2f(1, 1);
		gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);
		v = (m_vecPos[i] + g_vecTentRight * 3) + g_vecTentUp * -3;
		gEngfuncs.pTriAPI->TexCoord2f(0, 1);
		gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);
	}

	if (!m_bDone)
	{
		gEngfuncs.pTriAPI->Color4f(1, 1, 0.3, 0.95);
		gEngfuncs.pTriAPI->Brightness(0.95);

		Vector v = (m_vecOrigin + g_vecTentRight * 6) + g_vecTentUp * 6;
		gEngfuncs.pTriAPI->TexCoord2f(0, 0);
		gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);
		v = (m_vecOrigin + g_vecTentRight * -6) + g_vecTentUp * 6;
		gEngfuncs.pTriAPI->TexCoord2f(1, 0);
		gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);
		Corner(m_vecOrigin, -6, -6, 1, 1);
		Corner(m_vecOrigin, 6, -6, 0, 1);
	}

	gEngfuncs.pTriAPI->End();
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// Lights all three beams of an effect down to 0.01 and kills them (inlined in the original)
static void Beam_FadeKill(BEAM*& pBeam)
{
	if (!pBeam)
		return;
	pBeam->brightness = 0.01;
	Beam_Kill(pBeam);
	pBeam = NULL;
}

// --- CLTE 33: a lightning bolt between two points with a smoky one along it, 0.15 s fading ---

// vtable client 0x1011dd00, 0x1c bytes
class CTentType33 : public CClientTent
{
public:
	bool Update(float flFrametime) override;

	BEAM* m_pBolt;	  // +8
	BEAM* m_pTail;	  // +0xc: 6 units back from the start to it
	BEAM* m_pSmoke;	  // +0x10
	float m_flLife;	  // +0x14
	float m_flTime;	  // +0x18
};

// orig client 0x6fdf0. coord x3 start, coord x3 end
CClientTent* CLTE_CreateLightningFlash(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentType33*>(pCopy));

	CTentType33* p = new CTentType33;
	Vector vecStart, vecEnd;
	vecStart.x = READ_COORD();
	vecStart.y = READ_COORD();
	vecStart.z = READ_COORD();
	vecEnd.x = READ_COORD();
	vecEnd.y = READ_COORD();
	vecEnd.z = READ_COORD();
	p->m_pSmoke = NULL;
	p->m_flLife = 0.15;
	p->m_flTime = 0.15;
	p->m_pBolt = NULL;
	p->m_pTail = NULL;

	int iModel;
	if (gEngfuncs.CL_LoadModel("sprites/lgtning.spr", &iModel))
	{
		const float flSpeed = gEngfuncs.pfnRandomFloat(2, 2.2);
		const float flFrame = gEngfuncs.pfnRandomFloat(0, 2);
		const float flFramerate = gEngfuncs.pfnRandomFloat(10, 15);
		const int iFrame = (int)flFrame;
		p->m_pBolt = gEngfuncs.pEfxAPI->R_BeamPoints(vecStart, vecEnd, iModel, 0, 2, 0, 1, flSpeed, iFrame, flFramerate, 0.8, 0.8, 1);

		const Vector vecDir = Vector(vecEnd.x - vecStart.x, vecEnd.y - vecStart.y, vecEnd.z - vecStart.z).Normalize();
		Vector vecBack = vecStart - Vector(vecDir.x * 6.0, vecDir.y * 6.0, vecDir.z * 6.0);
		p->m_pTail = gEngfuncs.pEfxAPI->R_BeamPoints(vecBack, vecStart, iModel, 0, 2, 0, 1, flSpeed, iFrame, flFramerate, 0.8, 0.8, 1);
		if (p->m_pTail)
			p->m_pTail->flags |= FBEAM_SHADEIN;

		if (gEngfuncs.CL_LoadModel("sprites/smoke.spr", &iModel))
		{
			const float flSmokeFramerate = gEngfuncs.pfnRandomFloat(10, 15);
			const int iSmokeFrame = (int)gEngfuncs.pfnRandomFloat(0, 2);
			const float flSmokeSpeed = gEngfuncs.pfnRandomFloat(2, 2.2);
			p->m_pSmoke = gEngfuncs.pEfxAPI->R_BeamPoints(vecStart, vecEnd, iModel, 0, 1, 4, 0.9, flSmokeSpeed, iSmokeFrame, flSmokeFramerate, 0.8, 0.8, 1);
		}
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x702e0
bool CTentType33::Update(float flFrametime)
{
	m_flLife = m_flLife - flFrametime;
	if (m_flLife < 0)
	{
		Beam_FadeKill(m_pBolt);
		Beam_FadeKill(m_pTail);
		Beam_FadeKill(m_pSmoke);
		return true;
	}

	float f = m_flLife / m_flTime;
	if (f < 0)
		f = 0;
	else if (f > 1)
		f = 1;

	if (m_pBolt)
		m_pBolt->brightness = f;
	if (m_pTail)
		m_pTail->brightness = f;
	if (m_pSmoke)
	{
		m_pSmoke->brightness = f * 0.9;
		m_pSmoke->amplitude = f * 4.0;
	}
	return false;
}

// --- CLTE 34: the gauss beacon stuck (gaussbecon.cpp RC2_CLTE_STUCK): a pulsing red flare on it and a
// ping every 1-1.5 s ---

// vtable client 0x1011dd18, 0x1c bytes
class CTentBeaconStuck : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;		  // +8
	bool m_bVisible;	  // +0xc
	dlight_t* m_pLight;	  // +0x10
	TEMPENTITY* m_pFlare; // +0x14
	float m_flNextPing;	  // +0x18
};

// orig client 0x70490. short beacon
CClientTent* CLTE_CreateBeaconStuck(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentBeaconStuck*>(pCopy));

	CTentBeaconStuck* p = new CTentBeaconStuck;
	p->m_iEntity = READ_SHORT();
	p->m_pFlare = NULL;
	p->m_pLight = NULL;
	p->m_flNextPing = gEngfuncs.pfnRandomFloat(1, 1.5);

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
	if (!pEnt)
	{
		p->m_iEntity = 0;
		g_pClientTents[iSlot] = p;
		return p;
	}

	int iModel;
	model_s* pSprite = gEngfuncs.CL_LoadModel("sprites/redflare1.spr", &iModel);
	if (pSprite)
	{
		p->m_pFlare = gEngfuncs.pEfxAPI->CL_TempEntAlloc(pEnt->attachment[0], pSprite);
		if (p->m_pFlare)
		{
			p->m_pFlare->flags |= FTENT_PERSIST;
			p->m_pFlare->entity.curstate.scale = 0.2;
			p->m_pFlare->entity.curstate.renderamt = 164;
			p->m_pFlare->entity.curstate.rendermode = kRenderGlow;
			p->m_pFlare->entity.curstate.renderfx = kRenderFxNoDissipation;
			p->m_pFlare->entity.curstate.rendercolor.g = 255;
			p->m_pFlare->entity.curstate.rendercolor.r = 255;
			p->m_pFlare->entity.curstate.rendercolor.b = 255;
			p->m_pFlare->entity.curstate.frame = 0;
			p->m_pFlare->die = p->m_pFlare->die + 10.0;
		}
	}

	if (g_iClientDetail > 0)
	{
		p->m_pLight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		if (p->m_pLight)
		{
			p->m_pLight->origin = pEnt->origin;
			p->m_pLight->radius = 32;
			p->m_pLight->color.r = 255;
			p->m_pLight->color.g = 0;
			p->m_pLight->color.b = 0;
			p->m_pLight->die = gEngfuncs.GetClientTime() + 10.0;
		}
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x70830. The flare and light sit 16 in front of it and pulse with the client clock;
// both lives are pushed on by the frame, twice while in the current packet
bool CTentBeaconStuck::Update(float flFrametime)
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

	m_flNextPing = m_flNextPing - flFrametime;
	if (m_flNextPing < 0)
	{
		gEngfuncs.pEventAPI->EV_PlaySound(m_iEntity, pEnt->origin, CHAN_VOICE, "turret/tu_ping.wav", 0.4, 0.7, 0, 100);
		m_flNextPing = gEngfuncs.pfnRandomFloat(1, 1.5);
	}

	m_bVisible = !(pEnt->curstate.messagenum < pLocal->curstate.messagenum);

	if (m_pFlare)
		m_pFlare->die = flFrametime + m_pFlare->die;
	if (m_pLight)
		m_pLight->die = flFrametime + m_pLight->die;

	if (m_bVisible)
	{
		Vector vecAngles(-pEnt->angles.x, pEnt->angles.y, pEnt->angles.z);
		Vector vecForward;
		gEngfuncs.pfnAngleVectors(vecAngles, vecForward, NULL, NULL);
		const Vector vecPos = vecForward * 16.0 + pEnt->origin;

		if (m_pFlare)
		{
			m_pFlare->entity.curstate.renderamt = (int)(sin(gEngfuncs.GetClientTime() * 3.141592653589793) * 32.0 + 164.0);
			m_pFlare->entity.origin = vecPos;
			m_pFlare->die = flFrametime + m_pFlare->die;
		}
		if (m_pLight)
		{
			m_pLight->radius = sin(gEngfuncs.GetClientTime() * 3.141592653589793) * 8.0 + 32.0;
			m_pLight->origin = vecPos;
			m_pLight->die = flFrametime + m_pLight->die;
		}
	}
	return false;
}

// orig client 0x70b40
bool CTentBeaconStuck::KillEntity(int iEnt)
{
	if (m_iEntity != iEnt)
		return false;

	gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_VOICE, "turret/tu_ping.wav");
	if (m_pLight)
	{
		m_pLight->die = gEngfuncs.GetClientTime() + 0.01;
		m_pLight = NULL;
	}
	if (m_pFlare)
	{
		m_pFlare->die = gEngfuncs.GetClientTime() + 0.01;
		m_pFlare = NULL;
	}
	m_iEntity = 0;
	return true;
}

// --- CLTE 35: a charging hum (pulsemachine) on a player, 0.5 or 0.75 s, rising ---

// vtable client 0x1011dd4c, 0x18 bytes
class CTentType35 : public CClientTent
{
public:
	bool Update(float flFrametime) override;
	bool KillEntity(int iEnt) override;

	int m_iEntity;		// +8
	float m_flTimeLeft; // +0xc
	float m_flTime;		// +0x10
	int m_iMode;		// +0x14: 0 0.5 s, 1 0.75 s
};

// orig client 0x70be0. byte player, byte mode. Original bug kept: another mode leaves the time unset
CClientTent* CLTE_CreateChargeHum(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentType35*>(pCopy));

	CTentType35* p = new CTentType35;
	p->m_iEntity = READ_BYTE();
	p->m_iMode = READ_BYTE();
	if (0 != p->m_iEntity)
	{
		if (p->m_iMode == 0)
			p->m_flTimeLeft = 0.5;
		else if (p->m_iMode == 1)
			p->m_flTimeLeft = 0.75;
		p->m_flTime = p->m_flTimeLeft;

		cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(p->m_iEntity);
		if (!pEnt)
			p->m_iEntity = 0;
		else
			gEngfuncs.pEventAPI->EV_PlaySound(p->m_iEntity, pEnt->origin, CHAN_WEAPON, "ambience/pulsemachine.wav", 0.6, 0.8, 0, 110);
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x70e60. Pitch 110-180 and louder as it fills
bool CTentType35::Update(float flFrametime)
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

	m_flTimeLeft = m_flTimeLeft - flFrametime;
	if (!(m_flTimeLeft >= 0))
	{
		KillEntity(m_iEntity);
		return true;
	}

	float f = 1.0 - m_flTimeLeft / m_flTime;
	if (f < 0)
		f = 0;
	else if (f > 1)
		f = 1;
	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntity, pEnt->origin, CHAN_WEAPON, "ambience/pulsemachine.wav", f * 0.4 + 0.6, 0.8,
		SND_CHANGE_VOL | SND_CHANGE_PITCH, (int)(f * 70 + 110));
	return false;
}

// orig client 0x70f90
bool CTentType35::KillEntity(int iEnt)
{
	if (iEnt != m_iEntity)
		return false;

	gEngfuncs.pEventAPI->EV_StopSound(m_iEntity, CHAN_WEAPON, "ambience/pulsemachine.wav");
	m_iEntity = 0;
	return true;
}

// --- CLTE 36: a blue lightning bolt between two points with smoke along it, 0.5 s fading ---

// vtable client 0x1011dd68, 0x18 bytes
class CTentType36 : public CClientTent
{
public:
	bool Update(float flFrametime) override;

	BEAM* m_pBolt;	// +8
	BEAM* m_pSmoke; // +0xc
	float m_flLife; // +0x10
	float m_flTime; // +0x14
};

// orig client 0x70fe0. coord x3 start, coord x3 end
CClientTent* CLTE_CreateBlueLightning(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentType36*>(pCopy));

	CTentType36* p = new CTentType36;
	Vector vecStart, vecEnd;
	vecStart.x = READ_COORD();
	vecStart.y = READ_COORD();
	vecStart.z = READ_COORD();
	vecEnd.x = READ_COORD();
	vecEnd.y = READ_COORD();
	vecEnd.z = READ_COORD();
	p->m_pSmoke = NULL;
	p->m_flLife = 0.5;
	p->m_flTime = 0.5;
	p->m_pBolt = NULL;

	int iModel;
	if (gEngfuncs.CL_LoadModel("sprites/lgtning.spr", &iModel))
	{
		const float flFramerate = gEngfuncs.pfnRandomFloat(10, 15);
		const int iFrame = (int)gEngfuncs.pfnRandomFloat(0, 2);
		const float flSpeed = gEngfuncs.pfnRandomFloat(2, 2.2);
		p->m_pBolt = gEngfuncs.pEfxAPI->R_BeamPoints(vecStart, vecEnd, iModel, 0, 1.5, 0.3, 1, flSpeed, iFrame, flFramerate, 0.4, 0.4, 1);

		if (gEngfuncs.CL_LoadModel("sprites/smoke.spr", &iModel))
		{
			const float flSmokeFramerate = gEngfuncs.pfnRandomFloat(10, 15);
			const int iSmokeFrame = (int)gEngfuncs.pfnRandomFloat(0, 2);
			const float flSmokeSpeed = gEngfuncs.pfnRandomFloat(2, 2.2);
			p->m_pSmoke = gEngfuncs.pEfxAPI->R_BeamPoints(vecStart, vecEnd, iModel, 0, 0.8, 2, 0.9, flSmokeSpeed, iSmokeFrame, flSmokeFramerate, 0.7, 0.7, 1);
		}
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x71390
bool CTentType36::Update(float flFrametime)
{
	m_flLife = m_flLife - flFrametime;
	if (m_flLife < 0)
	{
		Beam_FadeKill(m_pBolt);
		Beam_FadeKill(m_pSmoke);
		return true;
	}

	float f = m_flLife / m_flTime;
	if (f < 0)
		f = 0;
	else if (f > 1)
		f = 1;

	if (m_pBolt)
		m_pBolt->brightness = f;
	if (m_pSmoke)
	{
		m_pSmoke->brightness = f * 0.9;
		m_pSmoke->amplitude = f + f;
	}
	return false;
}

// --- CLTE 37: a player's shield (player.cpp, m_bClientShield): drawn by DrawTransparent round him,
// a band sweeping up it now and then; the bit in g_iShieldPlayers makes shots meet it ---

// vtable client 0x1011dd80, 0x6c bytes: CTentShield, in clte_types.h

// orig client 0x714f0. byte player
CClientTent* CLTE_CreateShield(CClientTent* pCopy)
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return NULL;
	if (pCopy)
		return CLTE_CreateCopy(iSlot, static_cast<CTentShield*>(pCopy));

	CTentShield* p = new CTentShield;
	p->m_iEntity = READ_BYTE();
	p->m_pNoise = NULL;
	p->m_pLightning = NULL;
	p->m_flSweepWait = gEngfuncs.pfnRandomFloat(0.25, 0.5);
	p->m_bSweeping = false;
	p->m_flSweepZ = 0;
	p->m_flFlash = 0;
	p->m_flYaw = gEngfuncs.pfnRandomFloat(0, 360);
	p->m_flSweepMargin = 2.0;
	const float flFrame = gEngfuncs.pfnRandomFloat(0, 2);
	p->m_flHeight = 40;
	p->m_flFrame = flFrame;
	p->m_flWidth = 20;
	p->m_bVisible = true;

	if (!gEngfuncs.GetEntityByIndex(p->m_iEntity))
	{
		p->m_iEntity = 0;
	}
	else
	{
		g_iShieldPlayers |= 1 << (p->m_iEntity - 1);
		int iModel;
		p->m_pNoise = gEngfuncs.CL_LoadModel("sprites/noise1.spr", &iModel);
		p->m_pLightning = gEngfuncs.CL_LoadModel("sprites/lgtning.spr", &iModel);
	}
	g_pClientTents[iSlot] = p;
	return p;
}

// orig client 0x717c0. Shown unless out of the current packet (always for the local player); a
// sweep runs up the shield at its height a second, then waits 0.5-1 s
bool CTentShield::Update(float flFrametime)
{
	if (0 == m_iEntity)
		return true;

	cl_entity_t* pEnt = gEngfuncs.GetEntityByIndex(m_iEntity);
	cl_entity_t* pLocal = pEnt ? gEngfuncs.GetLocalPlayer() : NULL;
	if (!pEnt || !pLocal)
	{
		KillEntity(m_iEntity);
		m_iEntity = 0;
		return true;
	}

	if (pEnt->curstate.messagenum < pLocal->curstate.messagenum && 0 == gEngfuncs.pEventAPI->EV_IsLocal(m_iEntity - 1))
		m_bVisible = false;
	else
		m_bVisible = true;

	if (pEnt->curstate.usehull == 1)
	{
		m_flHeight = 30;
		m_flWidth = 26;
	}
	else
	{
		m_flHeight = 40;
		m_flWidth = 20;
	}

	m_flYaw = flFrametime * 180.0 + m_flYaw;
	if (m_flYaw >= 360.0)
		m_flYaw = m_flYaw - 360.0;

	float flFlash = m_flFlash - flFrametime * 6.6666665f;
	if (flFlash < 0)
		flFlash = 0;
	else if (flFlash >= 1)
		flFlash = 1;
	m_flFlash = flFlash;

	if (!m_bSweeping)
	{
		m_flSweepWait = m_flSweepWait - flFrametime;
		if (m_flSweepWait < 0)
		{
			m_bSweeping = true;
			m_flSweepZ = m_flSweepMargin - m_flHeight;
		}
	}
	else
	{
		m_flSweepZ = (m_flHeight + m_flHeight) * 0.5 * flFrametime + m_flSweepZ;
		if (m_flSweepZ > m_flHeight - m_flSweepMargin)
		{
			m_bSweeping = false;
			m_flSweepWait = gEngfuncs.pfnRandomFloat(0.5, 1.0);
		}
	}

	if (m_bVisible)
	{
		m_flFrame = flFrametime * 12.0 + m_flFrame;
		if (m_flFrame >= 3.0)
			m_flFrame = m_flFrame - 3.0;

		Vector vecAngles(0, m_flYaw, 0);
		gEngfuncs.pfnAngleVectors(vecAngles, m_vecForward, m_vecRight, m_vecUp);
		m_vecOrigin = CLTE_GetOrigin(m_iEntity);
	}
	return false;
}

// orig client 0x71a80
bool CTentShield::KillEntity(int iEnt)
{
	if (m_iEntity != iEnt)
		return false;

	g_iShieldPlayers &= ~(1 << (m_iEntity - 1));
	m_iEntity = 0;
	return true;
}

