/***
*
*	Rocket Crowbar 2: the energy meter and the status icons, two HUD elements of RC2's own. In
*	client.dll they sit between demo.cpp and entity.cpp (0x9cb00-0x9d210; most of it Ghidra's
*	function list missed). The file, class and member names are ours; reference/dossiers/c4.md.
*
*	The server sends "Energy" (a byte, the player's energy 0-100) and "EIcons" (a byte of flags;
*	bit 0 shows the gauss beacon icon).
*
****/

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "ammohistory.h"

#include <math.h>

DECLARE_MESSAGE(m_EIcons, EIcons) // orig client 0x9cb00 (not in files-client.tsv: Ghidra missed it)
DECLARE_MESSAGE(m_Energy, Energy) // orig client 0x9cd90 (not in files-client.tsv: Ghidra missed it)

#define EICON_GAUSSBECON 1 // name ours

// orig client 0x9cb30 (not in files-client.tsv: Ghidra missed it)
bool CHudEIcons::Init()
{
	HOOK_MESSAGE(EIcons);
	gHUD.AddHudElem(this);
	m_bIcons = 0;
	return true;
}

// orig client 0x9cb70
bool CHudEIcons::VidInit()
{
	m_hBecon = 0;
	m_prcBecon = &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("gauss_becon"));
	return true;
}

// orig client 0x9cbb0
bool CHudEIcons::MsgFunc_EIcons(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_bIcons = READ_BYTE();

	if (0 != m_bIcons)
		m_iFlags |= HUD_ACTIVE;
	else
		m_iFlags &= ~HUD_ACTIVE;

	return true;
}

// orig client 0x9cc20 (not in files-client.tsv: Ghidra missed it). The icon row sits on the bottom
// edge from the middle of the screen; only the beacon icon exists.
bool CHudEIcons::Draw(float flTime)
{
	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH) != 0)
		return true;

	if (!gHUD.HasSuit())
		return true;

	if (0 == m_bIcons)
		return true;

	if (0 == m_hBecon)
		m_hBecon = gHUD.GetSprite(gHUD.GetSpriteIndex("gauss_becon"));

	int iHeight = 0; // name ours
	if ((m_bIcons & EICON_GAUSSBECON) != 0)
	{
		const int h = m_prcBecon->bottom - m_prcBecon->top;
		if (h > 0)
			iHeight = h;
	}

	const int x = ScreenWidth / 2;
	const int y = ScreenHeight - iHeight - 5;

	int r = 255, g = 160, b = 0;
	ScaleColors(r, g, b, 150);

	if ((m_bIcons & EICON_GAUSSBECON) != 0)
	{
		SPR_Set(m_hBecon, r, g, b);
		SPR_DrawAdditive(0, x, y, m_prcBecon);
	}

	return true;
}

// orig client 0x9cdc0 (not in files-client.tsv: Ghidra missed it)
bool CHudEnergy::Init()
{
	HOOK_MESSAGE(Energy);
	gHUD.AddHudElem(this);
	m_flEnergyTarget = m_flEnergy = 100;
	return true;
}

// orig client 0x9ce10
bool CHudEnergy::VidInit()
{
	m_hEmpty = 0;
	m_hFull = 0;
	m_prcFull = &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("enfull"));
	m_prcEmpty = &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("enempty"));
	return true;
}

// orig client 0x9ce80. The original is void (its message hook returns whatever is left in eax);
// ours answers true.
bool CHudEnergy::MsgFunc_Energy(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_flEnergyTarget = READ_BYTE();
	m_iFlags |= HUD_ACTIVE;
	return true;
}

// orig client 0x9ced0 (not in files-client.tsv: Ghidra missed it). The meter fills from the
// bottom, left of a three-digit number, 50 units a second toward the server's value; it is
// brighter the further it has to go. It moves up over the ammo rows while the weapon has ammo.
bool CHudEnergy::Draw(float flTime)
{
	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH) != 0)
		return true;

	if (!gHUD.HasSuit())
		return true;

	if (0 == m_hFull)
		m_hFull = gHUD.GetSprite(gHUD.GetSpriteIndex("enfull"));

	if (0 == m_hEmpty)
		m_hEmpty = gHUD.GetSprite(gHUD.GetSpriteIndex("enempty"));

	if (m_flEnergy != m_flEnergyTarget)
	{
		const float flStep = gHUD.m_flTimeDelta * 50.0; // name ours
		const float flDiff = m_flEnergyTarget - m_flEnergy;

		if (fabs(flDiff) < fabs(flStep))
			m_flEnergy = m_flEnergyTarget;
		else if (m_flEnergyTarget > m_flEnergy)
			m_flEnergy += flStep;
		else
			m_flEnergy -= flStep;
	}

	const int iHalfFont = gHUD.m_iFontHeight / 2; // name ours
	int y = ScreenHeight - iHalfFont - gHUD.m_iFontHeight;

	const Rect& rcNumber = gHUD.GetSpriteRect(gHUD.m_HUD_number_0);
	const int x = ScreenWidth - 5 * (rcNumber.right - rcNumber.left) - (m_prcFull->right - m_prcFull->left);

	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_RC2_NOWEAPONS) == 0)
	{
		const WEAPON* pWeapon = gHUD.m_Ammo.GetActiveWeapon();
		if (pWeapon && pWeapon->iAmmoType > 0)
			y -= iHalfFont + gHUD.m_iFontHeight;
		if (pWeapon && pWeapon->iAmmo2Type > 0)
			y -= gHUD.m_iFontHeight;
	}

	int a = (int)(fabs(m_flEnergyTarget - m_flEnergy) * 0.01f * 128.0f + 100.0f);
	if (a <= 100)
		a = 100;
	else if (a >= 250)
		a = 250;

	int r = 255, g = 160, b = 0;
	ScaleColors(r, g, b, a);

	const float flFraction = m_flEnergy * 0.01f;		  // name ours
	const int iOffset = iHalfFont - (m_prcFull->bottom - m_prcFull->top) / 2; // name ours

	if (flFraction < 1.0f)
	{
		SPR_Set(m_hEmpty, r, g, b);
		SPR_DrawAdditive(0, x, y + iOffset, m_prcEmpty);
	}

	if (flFraction > 0)
	{
		Rect rc = *m_prcFull;
		rc.top = (int)((1.0f - flFraction) * (rc.bottom - rc.top) + rc.top);
		SPR_Set(m_hFull, r, g, b);
		SPR_DrawAdditive(0, x, rc.top - m_prcFull->top + iOffset + y, &rc);
	}

	gHUD.DrawHudNumber(x + (m_prcFull->right - m_prcFull->left), y, DHN_3DIGITS | DHN_DRAWZERO, (int)m_flEnergy, r, g, b);

	return true;
}
