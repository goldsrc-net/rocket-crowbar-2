/***
*
*	Rocket Crowbar 2 client: the egon's tractor beam (CLTE 0x29).
*
*	Byte the player, short what the beam holds (0x4000: nothing), coord x3 the beam's end;
*	killed with RC2_CLTE_KILL (0xff) on the player (server egon.cpp). c2's CLTE dispatcher
*	(orig 0x510b0) reads the byte; a player who has the effect already gets the short (unless
*	0x4000) and the end into it. Otherwise it calls the factory here, which reads the rest, and
*	then sets m_iEntIndex and, the player in view, makes m_pLightning (sprites/lgtning.spr,
*	attachment 1 to the end, FBEAM_SINENOISE) and m_pLaser (sprites/laserbeam.spr, the same)
*	and puts laserbeam.spr's index in m_iLaserModel. Update keeps both beams on the end, green
*	with nothing held and red with something, and draws a third, white, beam from the end to the
*	middle of what is held.
*
*	The original's functions 0x991f0-0x99770 of client.dll's run "new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"

CClientTent* RC2_NewEgonTractorTent(const CClientTent* pCopy) // orig client 0x991f0
{
	const int iSlot = CLTE_FreeSlot();
	if (iSlot < 0)
		return nullptr;

	CEgonTractorTent* pTent = pCopy ? new CEgonTractorTent(*static_cast<const CEgonTractorTent*>(pCopy)) : new CEgonTractorTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x991f0).
CEgonTractorTent::CEgonTractorTent()
{
	const int iHeld = READ_SHORT();
	if (iHeld == 0x4000)
		m_iHeld = 0;
	else
		m_iHeld = iHeld;
	m_vecEnd.x = READ_COORD();
	m_vecEnd.y = READ_COORD();
	m_vecEnd.z = READ_COORD();
	m_pLightning = nullptr;
	m_pLaser = nullptr;
	m_pHoldBeam = nullptr;
	m_iLaserModel = 0;
	m_iEntIndex = 0;
}

// The end of a beam the way this effect ends them: dark, its time up, entities let go.
static void KillTractorBeam(BEAM*& pBeam)
{
	pBeam->brightness = 0.01;
	pBeam->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY);
	pBeam->t = 1.0;
	pBeam->die = 0.1;
	pBeam->endEntity = 0;
	pBeam->startEntity = 0;
	pBeam = nullptr;
}

bool CEgonTractorTent::Update(float flFrametime) // orig client 0x99420
{
	if (!m_iEntIndex)
	{
		KillEntity(0);
		return true;
	}

	if (!GetEntity(m_iEntIndex))
	{
		KillEntity(m_iEntIndex);
		m_iEntIndex = 0;
		return true;
	}

	if (m_iHeld < 1)
	{
		if (m_pHoldBeam)
			KillTractorBeam(m_pHoldBeam);
	}
	else
	{
		cl_entity_t* pHeld = GetEntity(m_iHeld);
		if (!pHeld)
		{
			m_iHeld = 0;
		}
		else
		{
			const Vector vecHalf = (pHeld->curstate.maxs + pHeld->curstate.mins) * 0.5;
			const Vector vecOrigin = CLTE_GetOrigin(m_iHeld);
			Vector vecCenter = vecHalf + vecOrigin;
			if (!m_pHoldBeam)
			{
				m_pHoldBeam = gEngfuncs.pEfxAPI->R_BeamPoints(m_vecEnd, vecCenter, m_iLaserModel, 0, 0.9, 0.5, 0.9, 3.0, 0, 0, 1.0, 1.0, 1.0);
			}
			else
			{
				m_pHoldBeam->source = m_vecEnd;
				m_pHoldBeam->target = vecCenter;
				m_pHoldBeam->delta = m_pHoldBeam->target - m_pHoldBeam->source;
			}
		}
	}

	if (m_pLightning)
	{
		m_pLightning->target = m_vecEnd;
		if (m_iHeld < 1)
		{
			m_pLightning->r = 0;
			m_pLightning->g = 1.0;
		}
		else
		{
			m_pLightning->r = 1.0;
			m_pLightning->g = 0;
		}
		m_pLightning->b = 0;
	}

	if (m_pLaser)
	{
		m_pLaser->target = m_vecEnd;
		if (m_iHeld < 1)
			m_pLaser->amplitude = 0.5;
		else
			m_pLaser->amplitude = 0.2;
	}

	return false;
}

// The player gone: all three beams end and m_iEntIndex becomes -1 (so Update ends it next).
// What is held gone: its beam ends. Never deletes on its own.
bool CEgonTractorTent::KillEntity(int iEntIndex) // orig client 0x99770
{
	if (iEntIndex == m_iEntIndex)
	{
		if (m_pLightning)
			KillTractorBeam(m_pLightning);
		if (m_pLaser)
			KillTractorBeam(m_pLaser);
		if (m_pHoldBeam)
			KillTractorBeam(m_pHoldBeam);
		m_iEntIndex = -1;
	}

	if (iEntIndex == m_iHeld && m_iHeld > 0 && m_pHoldBeam)
		KillTractorBeam(m_pHoldBeam);
	return false;
}
