/***
*
*	Rocket Crowbar 2 client: the antifriction mark (CLTE 0x35).
*
*	Sent by the server's antifric_timer (antifrictimer.cpp) while an entity is frictionless, and
*	killed with CLTE 0xfe/0x35. It draws nothing itself: it only marks the entity in c2's table
*	(and the local player in c2's flag) for as long as it lives, and other client code reads the
*	marks.
*
*	The original's functions 0x83520-0x83770 of client.dll's run "new: cdll_int .. com_weapons".
*
****/

#include "c3_needs.h"
#include "c3_tents.h"

// c3 needs:
// orig .bss 0x101edc7c (c2's; c2 clears it, 0x100 dwords, with the rest of the effect state):
// per entity index, whether it is frictionless. name ours
extern unsigned char g_abRC2AntiFriction[1024];
// orig .bss 0x101f69fc (c2's): the local player is frictionless; view.cpp's 0xec590 reads it.
// name ours
extern int g_iRC2LocalAntiFriction;

class CAntiFrictionTent : public CClientTent // name ours
{
public:
	CAntiFrictionTent();

	bool Think(float flFrametime) override;
	bool EntityGone(int iEntIndex) override;

	int m_iEntIndex; // orig +0x8
};

// The factory (the dispatcher's case 0x35; c3_tents.h). pCopy: a copy of that one, through the
// implicit copy constructor (inline in the original).
CClientTent* RC2_NewAntiFrictionTent(const CClientTent* pCopy) // orig client 0x83520
{
	const int iSlot = RC2_FindFreeClientTent();
	if (iSlot < 0)
		return nullptr;

	CAntiFrictionTent* pTent = pCopy ? new CAntiFrictionTent(*static_cast<const CAntiFrictionTent*>(pCopy)) : new CAntiFrictionTent();
	g_pClientTents[iSlot] = pTent;
	return pTent;
}

// Inline in the factory (orig 0x83520): reads the message, short entity, and marks it.
CAntiFrictionTent::CAntiFrictionTent()
{
	m_iEntIndex = READ_SHORT();
	if (!m_iEntIndex)
		return;

	if (gEngfuncs.pEventAPI->EV_IsLocal(m_iEntIndex - 1))
		g_iRC2LocalAntiFriction = 1;
	g_abRC2AntiFriction[m_iEntIndex] = 1;
}

// Lives until its entity's kill.
bool CAntiFrictionTent::Think(float flFrametime) // orig client 0x83700
{
	return m_iEntIndex == 0;
}

bool CAntiFrictionTent::EntityGone(int iEntIndex) // orig client 0x83720
{
	if (iEntIndex != m_iEntIndex)
		return false;

	if (gEngfuncs.pEventAPI->EV_IsLocal(m_iEntIndex - 1))
		g_iRC2LocalAntiFriction = 0;
	g_abRC2AntiFriction[m_iEntIndex] = 0;
	m_iEntIndex = 0;
	return true;
}
