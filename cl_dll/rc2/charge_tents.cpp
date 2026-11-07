/***
*
*	Rocket Crowbar 2 client: the hum of charging mines (CLTE 0x39 and 0x3c).
*
*	CLTE 0x39 is a tripmine's laser charging (server tripmine.cpp): a hum that rises with the
*	charge, the beam and glow entities' colour cycled through red, green and blue and their
*	brightness raised with it, the glow rolled. CLTE 0x3c is a conc charge arming (server
*	satchel.cpp, CLTE_CONCCHARGE): a drone that rises with the arming and beeps faster and
*	faster. Both charge over a time from the client's copy of rc2.ini.
*
*	The original's functions 0x8a790-0x8b280 of client.dll's run "new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"

// c3 needs: the client's own copy of rc2.ini (orig .bss 0x10210aa8, 8 bytes a key as the
// server's, filled from the RCCFG message by orig 0xbd490, "new: train .. util" or near; read
// with rc2prefs.h's RC2Pref()). name ours, after the server's g_rc2PrefValues
#include "rc2prefs.h"

// the engine's sound flags (dlls/util.h; the client has no header with them)
#ifndef SND_CHANGE_VOL
#define SND_CHANGE_VOL (1 << 6)
#define SND_CHANGE_PITCH (1 << 7)
#endif

//
// CLTE 0x39: short tripmine, short its beam entity, short its glow entity. VERIFY: the server
// (tripmine.cpp) writes tripmine, beam, 0, glow, 0, so the third short read here is the 0 and
// the glow is never touched; whether the server reconstruction or the original is off is open.
// c2's dispatcher also touches this effect: CLTE 0x3a (the laser tripped) plays
// debris/beamstart9.wav on the tripmine and zeroes m_flCharge of every 0x39 effect on it.
//
class CTripmineChargeTent : public CClientTent // name ours
{
public:
	CTripmineChargeTent();

	bool Think(float flFrametime) override;
	bool EntityGone(int iEntIndex) override;

	int m_iEntIndex;	// orig +0x8: the tripmine
	int m_iBeam;		// orig +0xc: its beam entity
	int m_iGlow;		// orig +0x10: its glow entity
	float m_flCharge;	// orig +0x14: 0-1 (CLTE 0x3a zeroes it)
	float m_flRoll;		// orig +0x18: the glow's roll
	float m_flRollSpeed; // orig +0x1c: 200-300 a second, either way
	float m_flHue;		// orig +0x20: 0-3 around red, green, blue
};

// The factory (the dispatcher's case 0x39). The implicit copy constructor is inline here.
CClientTent* RC2_NewTripmineChargeTent(const CClientTent* pCopy) // orig client 0x8a790
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CTripmineChargeTent* pTent = pCopy ? new CTripmineChargeTent(*static_cast<const CTripmineChargeTent*>(pCopy)) : new CTripmineChargeTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory: reads the message. The rest stays unset when there is no tripmine.
CTripmineChargeTent::CTripmineChargeTent()
{
	m_iEntIndex = READ_SHORT();
	m_iBeam = READ_SHORT();
	m_iGlow = READ_SHORT();
	if (!m_iEntIndex)
		return;

	cl_entity_t* pEntity = RC2_GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_iEntIndex = 0;
		return;
	}

	m_flCharge = 0;
	m_flHue = gEngfuncs.pfnRandomFloat(0, 3);
	m_flRoll = gEngfuncs.pfnRandomFloat(0, 360);
	const int iDir = gEngfuncs.pfnRandomLong(0, 1) ? 1 : -1;
	m_flRollSpeed = gEngfuncs.pfnRandomFloat(200, 300) * iDir;
	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, CHAN_STATIC, "ambience/labdrone2.wav", 0.4, 0.9, 0, 80);
}

bool CTripmineChargeTent::Think(float flFrametime) // orig client 0x8aa80
{
	if (!m_iEntIndex)
		return true;

	m_flHue += flFrametime * 1.5;
	if (m_flHue > 3)
		m_flHue -= 3;

	m_flCharge += (1.0 / RC2Pref(RC2P_Tripmine_TargetFireInterval).f) * flFrametime;
	if (m_flCharge < 0)
		m_flCharge = 0;
	else if (m_flCharge > 1)
		m_flCharge = 1;

	m_flRoll += flFrametime * m_flRollSpeed;
	while (m_flRoll < 0)
		m_flRoll += 360;
	while (m_flRoll > 360)
		m_flRoll -= 360;

	cl_entity_t* pEntity = RC2_GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		EntityGone(m_iEntIndex);
		return true;
	}

	// red to green to blue and back
	Vector colHue;
	if (m_flHue <= 1)
		colHue = Vector(1 - m_flHue, m_flHue, 0);
	else if (m_flHue <= 2)
		colHue = Vector(0, 2 - m_flHue, m_flHue - 1);
	else
		colHue = Vector(m_flHue - 2, 0, 3 - m_flHue);

	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, CHAN_STATIC, "ambience/labdrone2.wav", m_flCharge * 0.6 + 0.4, 0.9, SND_CHANGE_VOL | SND_CHANGE_PITCH, (int)(m_flCharge * 70 + 80));

	cl_entity_t* pBeam = RC2_GetEntity(m_iBeam);
	if (pBeam)
	{
		pBeam->curstate.renderamt = (int)(m_flCharge * 80 + 90);
		pBeam->curstate.rendercolor.r = (int)(colHue.x * 255);
		pBeam->curstate.rendercolor.g = (int)(colHue.y * 255);
		pBeam->curstate.rendercolor.b = (int)(colHue.z * 255);
	}

	cl_entity_t* pGlow = RC2_GetEntity(m_iGlow);
	if (pGlow)
	{
		pGlow->curstate.renderamt = (int)(m_flCharge * 80 + 170);
		pGlow->curstate.rendercolor.r = (int)(colHue.x * 255);
		pGlow->curstate.rendercolor.g = (int)(colHue.y * 255);
		pGlow->curstate.rendercolor.b = (int)(colHue.z * 255);
		pGlow->curstate.angles[2] = m_flRoll;
		pGlow->angles[2] = m_flRoll;
	}
	return false;
}

bool CTripmineChargeTent::EntityGone(int iEntIndex) // orig client 0x8ae10
{
	if (iEntIndex != m_iEntIndex)
		return false;
	gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, CHAN_STATIC, "ambience/labdrone2.wav");
	m_iEntIndex = 0;
	return true;
}

//
// CLTE 0x3c: short conc charge (the server sends a byte 0 after it, unread).
//
class CConcChargeTent : public CClientTent // name ours
{
public:
	CConcChargeTent();

	bool Think(float flFrametime) override;
	bool EntityGone(int iEntIndex) override;

	int m_iEntIndex;  // orig +0x8
	float m_flArm;	  // orig +0xc: 0-1 over [Satchel] ConcArmTime
	float m_flBeep;	  // orig +0x10: time to the next beep
};

// The factory (the dispatcher's case 0x3c). The implicit copy constructor is inline here.
CClientTent* RC2_NewConcChargeTent(const CClientTent* pCopy) // orig client 0x8ae60
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CConcChargeTent* pTent = pCopy ? new CConcChargeTent(*static_cast<const CConcChargeTent*>(pCopy)) : new CConcChargeTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory: reads the message.
CConcChargeTent::CConcChargeTent()
{
	m_iEntIndex = READ_SHORT();
	if (!m_iEntIndex)
		return;

	cl_entity_t* pEntity = RC2_GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		m_iEntIndex = 0;
		return;
	}

	m_flArm = 0;
	m_flBeep = gEngfuncs.pfnRandomFloat(0.1, 0.25);
	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, CHAN_STATIC, "ambience/dronemachine2.wav", 0.3, 0.5, 0, 90);
}

bool CConcChargeTent::Think(float flFrametime) // orig client 0x8b0c0
{
	if (!m_iEntIndex)
		return true;

	m_flArm += (1.0 / RC2Pref(RC2P_Satchel_ConcArmTime).f) * flFrametime;
	if (m_flArm < 0)
		m_flArm = 0;
	else if (m_flArm > 1)
		m_flArm = 1;

	cl_entity_t* pEntity = RC2_GetEntity(m_iEntIndex);
	if (!pEntity)
	{
		EntityGone(m_iEntIndex);
		return true;
	}

	m_flBeep -= flFrametime;
	if (m_flBeep <= 0)
	{
		// beeps come faster as it arms
		m_flBeep = (1.0 - m_flArm * 0.85) * gEngfuncs.pfnRandomFloat(0.8, 0.9);
		gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, CHAN_WEAPON, "fvox/beep.wav", m_flArm * 0.2 + 0.5, 0.6, 0, (int)(m_flArm * 20 + 120));
	}

	gEngfuncs.pEventAPI->EV_PlaySound(m_iEntIndex, pEntity->origin, CHAN_STATIC, "ambience/dronemachine2.wav", m_flArm * 0.7 + 0.3, 0.5, SND_CHANGE_VOL | SND_CHANGE_PITCH, (int)(m_flArm * 100 + 90));
	return false;
}

bool CConcChargeTent::EntityGone(int iEntIndex) // orig client 0x8b280
{
	if (iEntIndex != m_iEntIndex)
		return false;
	gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, CHAN_STATIC, "ambience/dronemachine2.wav");
	gEngfuncs.pEventAPI->EV_StopSound(m_iEntIndex, CHAN_WEAPON, "fvox/beep.wav");
	m_iEntIndex = 0;
	return true;
}
