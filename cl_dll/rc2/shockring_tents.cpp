/***
*
*	Rocket Crowbar 2 client: two expanding beam rings (CLTE 0x33 and 0x36).
*
*	Each is a flat TE_BEAMRING of sprites/laserbeam.spr at a point, its two ends pushed apart
*	along x each frame so it spreads, fading out over the second half of its life. 0x33 is wide
*	and pale blue (12 units, 2000 units/s, 0.75 s), 0x36 thin and magenta (6 units, 960/s,
*	0.4 s). No server file of this tree sends either type yet.
*
*	The original's functions 0x814b0-0x819a0 and 0x83770-0x83c60 of client.dll's run
*	"new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"

// A beam's kill, as every effect of the run does it. VERIFY: die = 0.1 is an absolute time
// (long past). name ours
static void KillRingBeam(BEAM* pBeam)
{
	pBeam->brightness = 0.01;
	pBeam->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
	pBeam->t = 1.0;
	pBeam->die = 0.1;
	pBeam->endEntity = 0;
	pBeam->startEntity = 0;
}

//
// CLTE 0x33: coord x, y, z.
//
class CBlueShockRingTent : public CClientTent // name ours
{
public:
	CBlueShockRingTent();

	bool Update(float flFrametime) override;

	Vector m_vecOrigin;	 // orig +0x8
	float m_flRadius;	 // orig +0x14
	float m_flLife;		 // orig +0x18
	float m_flMaxLife;	 // orig +0x1c
	BEAM* m_pBeam;		 // orig +0x20
};

// The factory (the dispatcher's case 0x33; c3_tents.h). pCopy: a copy of that one, through the
// implicit copy constructor (inline in the original).
CClientTent* RC2_NewBlueShockRingTent(const CClientTent* pCopy) // orig client 0x814b0
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CBlueShockRingTent* pTent = pCopy ? new CBlueShockRingTent(*static_cast<const CBlueShockRingTent*>(pCopy)) : new CBlueShockRingTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x814b0): reads the message.
CBlueShockRingTent::CBlueShockRingTent()
{
	m_vecOrigin.x = READ_COORD();
	m_vecOrigin.y = READ_COORD();
	m_vecOrigin.z = READ_COORD();
	m_flRadius = 0;
	m_flMaxLife = 0.75;
	m_flLife = 0.75;
	m_pBeam = nullptr;

	int iModelIndex;
	if (gEngfuncs.CL_LoadModel("sprites/laserbeam.spr", &iModelIndex))
	{
		m_pBeam = gEngfuncs.pEfxAPI->R_BeamCirclePoints(TE_BEAMRING, m_vecOrigin, m_vecOrigin, iModelIndex, 0, 12.0, 0.1, 0.6, 3.0, 0, 0, 0.6, 0.6, 1.0);
		if (m_pBeam)
			m_pBeam->segments = 16;
	}
}

bool CBlueShockRingTent::Update(float flFrametime) // orig client 0x81750
{
	m_flRadius = flFrametime * 2000 + m_flRadius;
	m_flLife -= flFrametime;
	if (m_flLife <= 0)
	{
		if (m_pBeam)
		{
			KillRingBeam(m_pBeam);
			m_pBeam = nullptr;
		}
		return true;
	}

	float flFrac = m_flLife / m_flMaxLife;
	if (flFrac < 0)
		flFrac = 0;
	else if (flFrac > 1)
		flFrac = 1;

	if (m_pBeam)
	{
		m_pBeam->source = Vector(-m_flRadius + m_vecOrigin.x, m_vecOrigin.y, m_vecOrigin.z);
		m_pBeam->target = Vector(m_flRadius + m_vecOrigin.x, m_vecOrigin.y, m_vecOrigin.z);
		m_pBeam->delta = Vector(m_pBeam->target.x - m_pBeam->source.x, m_pBeam->target.y - m_pBeam->source.y, m_pBeam->target.z - m_pBeam->source.z);
		if (flFrac < 0.5)
			m_pBeam->brightness = (flFrac + flFrac) * 0.6;
		else
			m_pBeam->brightness = 0.6;
	}
	return false;
}

//
// CLTE 0x36: coord x, y, z.
//
class CPurpleShockRingTent : public CClientTent // name ours
{
public:
	CPurpleShockRingTent();

	bool Update(float flFrametime) override;

	Vector m_vecOrigin;	 // orig +0x8
	float m_flRadius;	 // orig +0x14
	float m_flLife;		 // orig +0x18
	float m_flMaxLife;	 // orig +0x1c
	BEAM* m_pBeam;		 // orig +0x20
};

// The factory (the dispatcher's case 0x36; c3_tents.h). pCopy: a copy of that one, through the
// implicit copy constructor (inline in the original).
CClientTent* RC2_NewPurpleShockRingTent(const CClientTent* pCopy) // orig client 0x83770
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CPurpleShockRingTent* pTent = pCopy ? new CPurpleShockRingTent(*static_cast<const CPurpleShockRingTent*>(pCopy)) : new CPurpleShockRingTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x83770): reads the message.
CPurpleShockRingTent::CPurpleShockRingTent()
{
	m_vecOrigin.x = READ_COORD();
	m_vecOrigin.y = READ_COORD();
	m_vecOrigin.z = READ_COORD();
	m_flRadius = 0;
	m_flMaxLife = 0.4;
	m_flLife = 0.4;
	m_pBeam = nullptr;

	int iModelIndex;
	if (gEngfuncs.CL_LoadModel("sprites/laserbeam.spr", &iModelIndex))
	{
		m_pBeam = gEngfuncs.pEfxAPI->R_BeamCirclePoints(TE_BEAMRING, m_vecOrigin, m_vecOrigin, iModelIndex, 0, 6.0, 0.1, 0.6, 3.0, 0, 0, 1.0, 0, 1.0);
		if (m_pBeam)
			m_pBeam->segments = 16;
	}
}

bool CPurpleShockRingTent::Update(float flFrametime) // orig client 0x83a10
{
	m_flRadius = flFrametime * 960 + m_flRadius;
	m_flLife -= flFrametime;
	if (m_flLife <= 0)
	{
		if (m_pBeam)
		{
			KillRingBeam(m_pBeam);
			m_pBeam = nullptr;
		}
		return true;
	}

	float flFrac = m_flLife / m_flMaxLife;
	if (flFrac < 0)
		flFrac = 0;
	else if (flFrac > 1)
		flFrac = 1;

	if (m_pBeam)
	{
		m_pBeam->source = Vector(-m_flRadius + m_vecOrigin.x, m_vecOrigin.y, m_vecOrigin.z);
		m_pBeam->target = Vector(m_flRadius + m_vecOrigin.x, m_vecOrigin.y, m_vecOrigin.z);
		m_pBeam->delta = Vector(m_pBeam->target.x - m_pBeam->source.x, m_pBeam->target.y - m_pBeam->source.y, m_pBeam->target.z - m_pBeam->source.z);
		if (flFrac < 0.5)
			m_pBeam->brightness = (flFrac + flFrac) * 0.6;
		else
			m_pBeam->brightness = 0.6;
	}
	return false;
}
