/***
*
*	Rocket Crowbar 2 (client): the attribute menu. See vgui_attributes.h. All names ours.
*
*	Layout as the original computes it: RC2_XRES/RC2_YRES of 640x480 units, rounded.
*
****/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hud.h"
#include "cl_util.h"
#include "rc2prefs.h"
#include "rc2/vgui_attributes.h"

#include <VGUI_LineBorder.h>
#include <VGUI_InputSignal.h>
#include <VGUI_ActionSignal.h>

using namespace vgui;

float g_flAttrValues[ATTR_COUNT] = {1, 1, 1, 1, 100, 0, 0, 0, 1, 0}; // .data 0x101755b0
unsigned int g_iAttrWeapons = 1;									  // .data 0x101755d8
int g_iAttrTransform = -1;											  // .data 0x101755dc
unsigned int g_iAttrAbilities = 0;									  // .bss 0x10221aa8

static bool s_bAttrDirty;			   // .bss 0x10221aa4: the menu shows stale values (paintBackground refreshes)
static CAttributeMenu* s_pAttrMenu;	   // .bss 0x10221aa0: the open menu
static CAttributeMenu* s_pAttrMenuMade; // .bss 0x10221aac: the one menu ever made
static int s_iAttrSlot = -1;		   // .data 0x101755e0: the slot the slot menu chose
static bool s_bAttrSlotLoad;		   // .bss 0x10221ab0: load it (else save to it)
static char s_szAttrSlots[10][255];	   // .bss 0x102210a8: savedattrs.txt's lines
static char s_szAttrPath[256];		   // .bss 0x10220ca8. VERIFY: size (the next variable is at 0x10220da8)
static char s_szSpinnerText[128];	   // .bss 0x10228e20: CAttrSpinner's text (constructor and Step)
static char s_szSpinnerText2[128];	   // .bss 0x10228d60: CAttrSpinner::SetValue's

static void AttrMenu_Reset();

static int Attr_Total() { return (unsigned short)RC2Pref(RC2P_Attributes_TotalPoints).s; }

// What the choices cost.
// orig client 0xcbdf0
static int AttrMenu_TotalCost()
{
	int iCost = 0;
	for (int i = 0; i < ATTR_COUNT; i++)
		iCost += Attr_ValueToPoints(i, g_flAttrValues[i]);
	return Attr_AbilitiesCost(g_iAttrAbilities) + iCost + Attr_WeaponsCost(g_iAttrWeapons) + Attr_TransformCost(g_iAttrTransform);
}

// orig client 0xcbe70
static const char* AttrMenu_AbilityName(int i)
{
	switch (i)
	{
	case 0: return "Wallhop";
	case 1: return "Aqualung";
	case 2: return "Jet Boost";
	case 3: return "Jet Pack";
	case 4: return "Energy Draw";
	case 5: return "Create Health";
	case 6: return "Enhanced Crowbarang";
	case 7: return "Shield";
	case 8: return "Long Jump";
	case 9: return "Submersible Weapons";
	}
	return "Undefined";
}

// orig client 0xcbf10
static const char* AttrMenu_WeaponName(int i)
{
	switch (i)
	{
	case 0: return "Glock";
	case 1: return "357 Magnum";
	case 2: return "MP5";
	case 3: return "Shotgun";
	case 4: return "Crossbow";
	case 5: return "RPG";
	case 6: return "Egon";
	case 7: return "Gauss";
	case 8: return "Hornetgun";
	}
	return "Undefined";
}

// orig client 0xcbfa0
static const char* AttrMenu_TransformName(int i)
{
	switch (i)
	{
	case 0: return "Assassin";
	case 1: return "Houndeye";
	case 2: return "Controller";
	case 3: return "Stukabat";
	case 4: return "Headcrab";
	}
	return "Undefined";
}

// orig client 0xcc010
static const char* AttrMenu_AttributeName(int i)
{
	switch (i)
	{
	case 0: return "Jump";
	case 1: return "Speed";
	case 2: return "Attack";
	case 3: return "Defend";
	case 4: return "Health";
	case 5: return "Extra Health";
	case 6: return "Armor";
	case 7: return "Extra Armor";
	case 8: return "Energy Regen";
	case 9: return "Dmg Reflect";
	}
	return "Undefined";
}

// The next row of the menu: a new column when this one reaches the buttons.
// orig client 0xcc0b0
static void AttrMenu_NextCell(int* px, int* py)
{
	if (*py >= RC2_YRES(360) - RC2_YRES(29) - RC2_YRES(30))
	{
		*px += RC2_XRES(170);
		*py = RC2_YRES(45);
	}
}

static void AttrMenu_SetLabelScheme(Label* pLabel, CSchemeManager* pSchemes, SchemeHandle_t hScheme) // inlined everywhere
{
	int r, g, b, a;
	pLabel->setFont(pSchemes->getFont(hScheme));
	pLabel->setFont(Scheme::sf_primary1);
	pSchemes->getFgColor(hScheme, r, g, b, a);
	pLabel->setFgColor(r, g, b, a);
	pLabel->setFgColor(Scheme::sc_primary1);
	pSchemes->getBgColor(hScheme, r, g, b, a);
	pLabel->setBgColor(r, g, b, a);
}

static void AttrMenu_SpinnerChanged(float flValue, int i);
static void AttrMenu_AbilityToggled(bool bChecked, int i);
static void AttrMenu_WeaponToggled(bool bChecked, int i);
static void AttrMenu_TransformToggled(bool bChecked, int i);

// The menu's four buttons: 1 OK, 2 Load, 3 Save, 4 Restore Defaults.
class CAttrMenuButtonHandler : public ActionSignal // vtable 0x1011f55c
{
public:
	CAttrMenuButtonHandler(int iButton) : m_iButton(iButton) {}
	virtual void actionPerformed(Panel* panel);

private:
	int m_iButton;
};

// --- CAttrCheckButton

// The overlay's mouse: highlight, and toggle on release.
class CAttrCheckSignal : public InputSignal // vtable 0x1011f564
{
public:
	CAttrCheckSignal(CAttrCheckButton* pButton) : m_pButton(pButton) {}
	void cursorMoved(int x, int y, Panel* panel) override {}			   // orig client 0xd1310
	void cursorEntered(Panel* panel) override { m_pButton->m_bHighlight = true; } // orig client 0xe6c20
	void cursorExited(Panel* panel) override { m_pButton->m_bHighlight = false; } // orig client 0xe6c40
	void mousePressed(MouseCode code, Panel* panel) override {}			   // orig client 0xe6c60
	void mouseDoublePressed(MouseCode code, Panel* panel) override {}	   // orig client 0xd1330
	void mouseReleased(MouseCode code, Panel* panel) override { m_pButton->Toggle(); } // orig client 0xe6c80
	void mouseWheeled(int delta, Panel* panel) override {}				   // orig client 0xd1350
	void keyPressed(KeyCode code, Panel* panel) override {}				   // orig client 0xd1370
	void keyTyped(KeyCode code, Panel* panel) override {}				   // orig client 0xd1390
	void keyReleased(KeyCode code, Panel* panel) override {}			   // orig client 0xd13b0
	void keyFocusTicked(Panel* panel) override {}						   // orig client 0xd13d0

private:
	CAttrCheckButton* m_pButton;
};

static BitmapTGA* AttrMenu_LoadTGA(const char* pszName) // inlined
{
	FileInputStream* pFile = new FileInputStream(GetVGUITGAName(pszName), false);
	BitmapTGA* pImage = new BitmapTGA(pFile, true);
	pFile->close();
	return pImage;
}

// Inlined in CAttributeMenu's constructor, once per check button.
CAttrCheckButton::CAttrCheckButton(const char* pszText, int x, int y, int wide, int tall, bool bChecked, ToggledFunc pfnToggled, int iParam) : Panel(x, y, wide, tall)
{
	CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hText = pSchemes->getSchemeHandle("Briefing Text");

	m_bReady = false;
	m_bHighlight = false;
	m_bChecked = bChecked;
	m_pfnToggled = pfnToggled;
	m_iParam = iParam;

	m_pChecked = AttrMenu_LoadTGA("%d_checked");
	m_pUnchecked = AttrMenu_LoadTGA("%d_unchecked");

	int iImageWide, iImageTall;
	m_pChecked->getSize(iImageWide, iImageTall);
	const int yImage = tall / 2 - iImageTall / 2;

	// VERIFY: as the original, the text's width takes off the image's height, not its width
	m_pText = new Label("", iImageWide + 4, 1, wide - iImageTall - 4, tall - 2);
	m_pText->setParent(this);
	m_pText->setText(pszText);
	m_pText->setFont(pSchemes->getFont(hText));
	m_pText->setFont(Scheme::sf_primary1);
	pSchemes->getFgColor(hText, m_iFgColor[0], m_iFgColor[1], m_iFgColor[2], m_iFgColor[3]);
	m_pText->setFgColor(m_iFgColor[0], m_iFgColor[1], m_iFgColor[2], m_iFgColor[3]);
	m_pText->setFgColor(Scheme::sc_primary1);
	int r, g, b, a;
	pSchemes->getBgColor(hText, r, g, b, a);
	m_pText->setBgColor(r, g, b, a);
	m_pText->setTextAlignment(Label::a_west);
	m_pText->setContentAlignment(Label::a_west);

	m_pImageBack = new Label("", 2, yImage, iImageWide, iImageTall);
	m_pImageBack->setImage(m_bChecked ? m_pUnchecked : m_pChecked);
	m_pImageBack->setParent(this);
	m_pImageBack->setFgColor(255, 255, 255, 0);
	m_pImageBack->setBgColor(0, 0, 0, 0);

	m_pImageFront = new Label("", 2, yImage, iImageWide, iImageTall);
	m_pImageFront->setImage(m_bChecked ? m_pChecked : m_pUnchecked);
	m_pImageFront->setParent(this);
	m_pImageFront->setFgColor(255, 255, 255, 255);
	m_pImageFront->setBgColor(0, 0, 0, 0);

	CTransparentPanel* pOverlay = new CTransparentPanel(0, 0, 0, wide, tall);
	pOverlay->setParent(this);
	pOverlay->addInputSignal(new CAttrCheckSignal(this));
	m_bReady = true;
}

void CAttrCheckButton::UpdateImages()
{
	if (!m_bReady)
		return;
	m_pImageFront->setImage(m_bChecked ? m_pChecked : m_pUnchecked);
	m_pImageBack->setImage(m_bChecked ? m_pUnchecked : m_pChecked);
}

void CAttrCheckButton::Toggle()
{
	m_bChecked = !m_bChecked;
	if (m_pfnToggled && m_bReady)
		m_pfnToggled(m_bChecked, m_iParam);
	UpdateImages();
}

// orig client 0xd13f0
void CAttrCheckButton::paintBackground()
{
	if (m_bHighlight)
	{
		drawSetColor(Scheme::sc_secondary1);
		drawOutlinedRect(0, 0, _size[0], _size[1]);
	}
}

// --- CAttrSpinner

// The "+" (1) and "-" (-1) buttons.
class CAttrSpinnerButtonHandler : public ActionSignal // vtable 0x101201e0
{
public:
	CAttrSpinnerButtonHandler(CAttrSpinner* pSpinner, int iDir) : m_pSpinner(pSpinner), m_iDir(iDir) {}
	void actionPerformed(Panel* panel) override { m_pSpinner->Step(m_iDir); } // orig client 0xe6d60

private:
	CAttrSpinner* m_pSpinner;
	int m_iDir;
};

// orig client 0xd0880
CAttrSpinner::CAttrSpinner(ChangedFunc pfnChanged, int iParam, const char* pszText, const char* pszFormat, float flValue, float flMin, float flMax, float flStep, int x, int y, int wide)
	: Panel(x, y, wide, RC2_YRES(20) - 2)
{
	CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hText = pSchemes->getSchemeHandle("Briefing Text");

	m_flMax = flMax;
	m_bReady = false;
	m_flValue = flValue;
	m_flMin = flMin;
	m_flStep = (flStep == 0) ? 0.1f : flStep;
	m_iParam = iParam;
	m_pfnChanged = pfnChanged;
	strcpy(m_szFormat, pszFormat ? pszFormat : "%4.2f ");
	strcpy(m_szText, pszText ? pszText : "");

	const int wideLabel = wide - RC2_XRES(10);
	m_pLabel = new Label("0.0", 0, 0, wide - RC2_XRES(10), RC2_YRES(20) + 2);
	m_pLabel->setParent(this);
	m_pLabel->setText("0.0");
	AttrMenu_SetLabelScheme(m_pLabel, pSchemes, hText);
	m_pLabel->setTextAlignment(Label::a_east);
	m_pLabel->setContentAlignment(Label::a_east);

	CommandButton* pUp = new CommandButton("", wideLabel, 0, RC2_XRES(10), RC2_YRES(10));
	pUp->addActionSignal(new CAttrSpinnerButtonHandler(this, 1));
	pUp->setParent(this);

	const int tallButton = RC2_YRES(10);
	CommandButton* pDown = new CommandButton("", wideLabel, tallButton - 1, RC2_XRES(10), tallButton);
	pDown->addActionSignal(new CAttrSpinnerButtonHandler(this, -1));
	pDown->setParent(this);

	// Step(0), inlined
	requestFocus();
	if (!isKeyDown(KEY_LSHIFT))
		isKeyDown(KEY_RSHIFT);
	if (m_flMax != m_flMin)
	{
		if (m_flValue > m_flMax)
			m_flValue = m_flMax;
		if (m_flValue < m_flMin)
			m_flValue = m_flMin;
	}
	if (m_pfnChanged && m_bReady)
		m_pfnChanged(m_flValue, m_iParam);
	char sz[128];
	sprintf(sz, m_szFormat, (double)m_flValue);
	strcpy(s_szSpinnerText, m_szText);
	strcat(s_szSpinnerText, sz);
	m_pLabel->setText(s_szSpinnerText);
	m_pLabel->setTextAlignment(Label::a_east);
	m_pLabel->setContentAlignment(Label::a_east);
	m_bReady = true;
}

// orig client 0xd0fe0
void CAttrSpinner::Step(int iDir)
{
	requestFocus();
	if (isKeyDown(KEY_LSHIFT) || isKeyDown(KEY_RSHIFT))
		iDir *= 5;

	m_flValue = (float)iDir * m_flStep + m_flValue;
	if (m_flMax != m_flMin)
	{
		if (m_flValue > m_flMax)
			m_flValue = m_flMax;
		if (m_flValue < m_flMin)
			m_flValue = m_flMin;
	}
	if (m_pfnChanged && m_bReady)
		m_pfnChanged(m_flValue, m_iParam);

	char sz[128];
	sprintf(sz, m_szFormat, (double)m_flValue);
	strcpy(s_szSpinnerText, m_szText);
	strcat(s_szSpinnerText, sz);
	m_pLabel->setText(s_szSpinnerText);
	m_pLabel->setTextAlignment(Label::a_east);
	m_pLabel->setContentAlignment(Label::a_east);
}

// No clamp and no callback.
// orig client 0xd11c0
void CAttrSpinner::SetValue(float flValue)
{
	m_flValue = flValue;
	char sz[128];
	sprintf(sz, m_szFormat, (double)flValue);
	strcpy(s_szSpinnerText2, m_szText);
	strcat(s_szSpinnerText2, sz);
	m_pLabel->setText(s_szSpinnerText2);
	m_pLabel->setTextAlignment(Label::a_east);
	m_pLabel->setContentAlignment(Label::a_east);
}

// orig client 0xd12d0
void CAttrSpinner::paintBackground()
{
	drawSetColor(Scheme::sc_secondary1);
	drawOutlinedRect(0, 0, _size[0], _size[1]);
}

// --- CAttrRow

// Inlined in CAttributeMenu's constructor.
CAttrRow::CAttrRow(int i, int x, int y, int wide, int tall) : Panel(x, y, wide, tall)
{
	CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hText = pSchemes->getSchemeHandle("Briefing Text");

	strcpy(m_szFormat, AttrMenu_AttributeName(i));
	strcat(m_szFormat, " - %-4.2f:");

	m_pLabel = new Label("", 0, 0, RC2_XRES(165) - RC2_XRES(40) - RC2_XRES(25), RC2_YRES(20));
	m_pLabel->setParent(this);
	m_pLabel->setText("");
	AttrMenu_SetLabelScheme(m_pLabel, pSchemes, hText);
	m_pLabel->setTextAlignment(Label::a_west);
	m_pLabel->setContentAlignment(Label::a_west);

	m_pSpinner = new CAttrSpinner(AttrMenu_SpinnerChanged, i, "", "%4.0f ",
		(float)Attr_ValueToPoints(i, g_flAttrValues[i]), (float)Attr_ClampPoints(i, -9999), (float)Attr_ClampPoints(i, 9999), 1.0f,
		RC2_XRES(165) - RC2_XRES(25) - RC2_XRES(40), 1, RC2_XRES(40));
	m_pSpinner->setParent(this);

	char sz[384];
	sprintf(sz, m_szFormat, 0.0);
	m_pLabel->setText(sz);
}

// orig client 0xd1c70
void CAttrRow::paintBackground()
{
}

// --- CAttributeMenu

// orig client 0xcc180
CAttributeMenu::CAttributeMenu(int x, int y, int wide, int tall) : CMenuPanel(false, x, y, wide, tall)
{
	s_pAttrMenu = this;
	m_bReady = false;

	CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitle = pSchemes->getSchemeHandle("Title Font");
	SchemeHandle_t hText = pSchemes->getSchemeHandle("Briefing Text");

	setBorder(new LineBorder(Color(178, 118, 0, 0)));

	Label* pTitle = new Label("Adjust Attributes", 1, 1, RC2_XRES(540) - 2, RC2_YRES(30));
	pTitle->setParent(this);
	AttrMenu_SetLabelScheme(pTitle, pSchemes, hTitle);
	pTitle->setContentAlignment(Label::a_north);
	pTitle->setText("Adjust Attributes");

	m_pTotalLabel = new Label("0", 1, RC2_YRES(20), RC2_XRES(540) - 2, RC2_YRES(12));
	m_pTotalLabel->setParent(this);
	AttrMenu_SetLabelScheme(m_pTotalLabel, pSchemes, hText);
	m_pTotalLabel->setContentAlignment(Label::a_north);

	int xCell = RC2_XRES(10);
	int yCell = RC2_YRES(45);
	char sz[384];

	for (int i = 0; i < ATTR_COUNT; i++)
	{
		CAttrRow* pRow = new CAttrRow(i, xCell, yCell, RC2_XRES(165), RC2_YRES(20));
		m_pRows[i] = pRow;
		pRow->setParent(this);
		sprintf(sz, pRow->m_szFormat, (double)g_flAttrValues[i]);
		pRow->m_pLabel->setText(sz);
		yCell += RC2_YRES(20);
		AttrMenu_NextCell(&xCell, &yCell);
	}

	for (int i = 0; i < ATTR_ABILITIES; i++)
	{
		const unsigned int iBit = 1u << i;
		sprintf(sz, "%s (%d)", AttrMenu_AbilityName(i), Attr_AbilityCost(iBit));
		m_pAbilities[i] = new CAttrCheckButton(sz, xCell, yCell, RC2_XRES(165), RC2_YRES(20), (g_iAttrAbilities & iBit) != 0, AttrMenu_AbilityToggled, i);
		m_pAbilities[i]->setParent(this);
		yCell += RC2_YRES(20);
		AttrMenu_NextCell(&xCell, &yCell);
	}
	yCell += RC2_YRES(20);
	AttrMenu_NextCell(&xCell, &yCell);

	for (int i = 0; i < ATTR_WEAPONS; i++)
	{
		const unsigned int iBit = 1u << i;
		sprintf(sz, "%s (%d)", AttrMenu_WeaponName(i), Attr_WeaponCost(iBit));
		m_pWeapons[i] = new CAttrCheckButton(sz, xCell, yCell, RC2_XRES(165), RC2_YRES(20), (g_iAttrWeapons & iBit) != 0, AttrMenu_WeaponToggled, i);
		m_pWeapons[i]->setParent(this);
		yCell += RC2_YRES(20);
		AttrMenu_NextCell(&xCell, &yCell);
	}
	yCell += RC2_YRES(20);
	AttrMenu_NextCell(&xCell, &yCell);

	Label* pTransforms = new Label("Transforms", xCell, yCell, RC2_XRES(165), RC2_YRES(20));
	pTransforms->setParent(this);
	AttrMenu_SetLabelScheme(pTransforms, pSchemes, hText);
	pTransforms->setContentAlignment(Label::a_center);
	pTransforms->setText("Transforms");
	yCell += RC2_YRES(20);
	AttrMenu_NextCell(&xCell, &yCell);

	for (int i = 0; i < ATTR_TRANSFORMS; i++)
	{
		sprintf(sz, "%s (%d)", AttrMenu_TransformName(i), Attr_TransformCost(i));
		m_pTransforms[i] = new CAttrCheckButton(sz, xCell, yCell, RC2_XRES(165), RC2_YRES(20), g_iAttrTransform == i, AttrMenu_TransformToggled, i);
		m_pTransforms[i]->setParent(this);
		yCell += RC2_YRES(20);
		AttrMenu_NextCell(&xCell, &yCell);
	}

	// the buttons along the bottom
	const int yButton = tall - RC2_YRES(10) - RC2_YRES(30);
	const int wButton = RC2_XRES(122);
	const int xGap = RC2_XRES(10);
	const char* pszButtons[4] = {"OK", "Load", "Save", "Restore Defaults"};
	const int xButtons[4] = {
		RC2_XRES(12), // VERIFY: as the original, 12 for OK where the others step by 10
		wButton + xGap * 2,
		xGap + (xGap + wButton) * 2,
		(xGap + wButton) + xGap + (xGap + wButton) * 2,
	};
	for (int i = 0; i < 4; i++)
	{
		CommandButton* pButton = new CommandButton(pszButtons[i], xButtons[i], yButton, wButton, RC2_YRES(30));
		pButton->addActionSignal(new CAttrMenuButtonHandler(i + 1));
		pButton->setParent(this);
	}

	if (AttrMenu_TotalCost() > Attr_Total())
		AttrMenu_Reset();
	UpdateTotal();

	Reset();
	Panel::setVisible(false);
	m_bReady = true;
}

// orig client 0xceac0
void CAttributeMenu::UpdateTotal()
{
	char sz[256];
	const int iTotal = Attr_Total();
	sprintf(sz, "Total Attribute Points: %d, Free Attribute Points: %d", iTotal, iTotal - AttrMenu_TotalCost());
	m_pTotalLabel->setText(sz);
}

// Every control from the choices.
// orig client 0xceb30
void CAttributeMenu::Refresh()
{
	char sz[264];
	for (int i = 0; i < ATTR_COUNT; i++)
	{
		m_pRows[i]->m_pSpinner->SetValue((float)Attr_ValueToPoints(i, g_flAttrValues[i]));
		sprintf(sz, m_pRows[i]->m_szFormat, (double)g_flAttrValues[i]);
		m_pRows[i]->m_pLabel->setText(sz);
	}
	for (int i = 0; i < ATTR_ABILITIES; i++)
	{
		m_pAbilities[i]->m_bChecked = ((1u << i) & g_iAttrAbilities) != 0;
		m_pAbilities[i]->UpdateImages();
	}
	for (int i = 0; i < ATTR_WEAPONS; i++)
	{
		m_pWeapons[i]->m_bChecked = ((1u << i) & g_iAttrWeapons) != 0;
		m_pWeapons[i]->UpdateImages();
	}
	for (int i = 0; i < ATTR_TRANSFORMS; i++)
	{
		m_pTransforms[i]->m_bChecked = g_iAttrTransform == i;
		m_pTransforms[i]->UpdateImages();
	}
	UpdateTotal();
}

// orig client 0xcede0
void CAttributeMenu::paintBackground()
{
	if (s_bAttrDirty && m_bReady)
	{
		s_bAttrDirty = false;
		Refresh();
	}
	if (m_bReady)
		CTransparentPanel::paintBackground(); // the transparency's black fill, as the original's inlined copy
}

// The defaults: rc2.ini's minimum attributes, no abilities, the glock, no transform.
// orig client 0xcee60
static void AttrMenu_Reset()
{
	for (int i = 0; i < ATTR_COUNT; i++)
		g_flAttrValues[i] = Attr_DefaultValue(i);
	g_iAttrAbilities = 0;
	g_iAttrWeapons = 1;
	g_iAttrTransform = -1;
	s_bAttrDirty = true;
}

// An attribute's spinner moved: as many points as there are left.
// orig client 0xceed0
static void AttrMenu_SpinnerChanged(float flValue, int i)
{
	if (!s_pAttrMenu || !s_pAttrMenu->m_bReady)
		return;

	int iPoints = Attr_ClampPoints(i, (int)flValue);
	g_flAttrValues[i] = Attr_PointsToValue(i, iPoints);
	if (AttrMenu_TotalCost() > Attr_Total())
		iPoints += Attr_Total() - AttrMenu_TotalCost();

	if (iPoints < 0)
	{
		AttrMenu_Reset();
		return;
	}

	g_flAttrValues[i] = Attr_PointsToValue(i, iPoints);
	CAttrRow* pRow = s_pAttrMenu->m_pRows[i];
	pRow->m_pSpinner->SetValue((float)iPoints);
	char sz[260];
	sprintf(sz, pRow->m_szFormat, (double)g_flAttrValues[i]);
	pRow->m_pLabel->setText(sz);
	s_pAttrMenu->UpdateTotal();
}

static void AttrMenu_Uncheck(CAttrCheckButton* pButton) // inlined
{
	pButton->m_bChecked = false;
	pButton->UpdateImages();
}

// orig client 0xcf040
static void AttrMenu_AbilityToggled(bool bChecked, int i)
{
	if (!s_pAttrMenu || !s_pAttrMenu->m_bReady)
		return;

	const unsigned int iBit = 1u << i;
	if (bChecked)
		g_iAttrAbilities |= iBit;
	else
		g_iAttrAbilities &= ~iBit;

	if (AttrMenu_TotalCost() > Attr_Total())
	{
		// too dear: back off
		g_iAttrAbilities &= ~iBit;
		AttrMenu_Uncheck(s_pAttrMenu->m_pAbilities[i]);
		if (AttrMenu_TotalCost() > Attr_Total())
		{
			AttrMenu_Reset();
			return;
		}
	}
	s_pAttrMenu->UpdateTotal();
}

// orig client 0xcf180
static void AttrMenu_WeaponToggled(bool bChecked, int i)
{
	if (!s_pAttrMenu || !s_pAttrMenu->m_bReady)
		return;

	const unsigned int iBit = 1u << i;
	if (bChecked)
		g_iAttrWeapons |= iBit;
	else
		g_iAttrWeapons &= ~iBit;

	if (AttrMenu_TotalCost() > Attr_Total())
	{
		g_iAttrWeapons &= ~iBit;
		AttrMenu_Uncheck(s_pAttrMenu->m_pWeapons[i]);
		if (AttrMenu_TotalCost() > Attr_Total())
		{
			AttrMenu_Reset();
			return;
		}
	}
	s_pAttrMenu->UpdateTotal();
}

// One transform at most.
// orig client 0xcf2c0
static void AttrMenu_TransformToggled(bool bChecked, int i)
{
	if (!s_pAttrMenu || !s_pAttrMenu->m_bReady)
		return;

	const int iOld = g_iAttrTransform;
	if (!bChecked)
	{
		g_iAttrTransform = -1;
		s_pAttrMenu->UpdateTotal();
		return;
	}

	g_iAttrTransform = i;
	if (AttrMenu_TotalCost() > Attr_Total())
	{
		g_iAttrTransform = iOld;
		AttrMenu_Uncheck(s_pAttrMenu->m_pTransforms[i]);
		if (AttrMenu_TotalCost() > Attr_Total())
		{
			AttrMenu_Reset();
			return;
		}
	}
	else
	{
		for (int j = 0; j < ATTR_TRANSFORMS; j++)
		{
			if (j != g_iAttrTransform)
				AttrMenu_Uncheck(s_pAttrMenu->m_pTransforms[j]);
		}
	}
	s_pAttrMenu->UpdateTotal();
}

// The menu, made once and kept: again it only comes back hidden and ready.
// orig client 0xcf480
CAttributeMenu* AttrMenu_Create(Panel* pParent)
{
	if (s_pAttrMenu)
	{
		s_pAttrMenu->Reset();
		s_pAttrMenu->setVisible(false);
		s_pAttrMenu->m_bReady = true;
		s_pAttrMenu->setParent(pParent);
		return s_pAttrMenu;
	}

	if (!s_pAttrMenuMade)
		s_pAttrMenuMade = new CAttributeMenu(RC2_XRES(50), RC2_YRES(50), RC2_XRES(540), RC2_YRES(360));
	s_pAttrMenuMade->setParent(pParent);
	s_pAttrMenu = s_pAttrMenuMade;
	return s_pAttrMenuMade;
}

// orig client 0xcf630
void CAttrMenuButtonHandler::actionPerformed(Panel* panel)
{
	if (!s_pAttrMenu || !s_pAttrMenu->m_bReady)
		return;

	switch (m_iButton)
	{
	case 1: // OK: setattr <ten points> <abilities> <weapons> <transform>
	{
		char szCmd[256];
		char sz[256];
		strcpy(szCmd, "setattr ");
		for (int i = 0; i < ATTR_COUNT; i++)
		{
			sprintf(sz, "%d ", Attr_ValueToPoints(i, g_flAttrValues[i]));
			strcat(szCmd, sz);
		}
		sprintf(sz, "%u %u %d\n", g_iAttrAbilities, g_iAttrWeapons, g_iAttrTransform);
		strcat(szCmd, sz);
		gEngfuncs.pfnClientCmd(szCmd);

		CMenuPanel* pNext = nullptr;
		if (s_pAttrMenu)
		{
			pNext = s_pAttrMenu->GetNextMenu();
			s_pAttrMenu->m_bReady = false;
			s_pAttrMenu->Close();
		}
		if (gViewPort)
		{
			// VERIFY: client.dll 0xe5090 and 0xe5de0 are TeamFortressViewport::SetCurrentMenu and
			// UpdateCursorState (package c5)
			gViewPort->SetCurrentMenu(pNext);
			gViewPort->UpdateCursorState();
		}
		break;
	}
	case 2: // Load
		s_pAttrMenu->m_bReady = false;
		s_pAttrMenu->Close();
		new CAttrSlotMenu(0);
		break;
	case 3: // Save
		s_pAttrMenu->m_bReady = false;
		s_pAttrMenu->Close();
		new CAttrSlotMenu(1);
		break;
	case 4: // Restore Defaults
		AttrMenu_Reset();
		break;
	}
}

// --- savedattrs.txt: ten lines, "-" for an empty slot

// orig client 0xcf900
static const char* AttrMenu_SavedPath()
{
	sprintf(s_szAttrPath, "%s/savedattrs.txt", gEngfuncs.pfnGetGameDirectory());
	return s_szAttrPath;
}

// orig client 0xcf930
static void AttrMenu_LoadSlots()
{
	FILE* pFile = fopen(AttrMenu_SavedPath(), "rt");
	if (!pFile)
	{
		for (int i = 0; i < 10; i++)
			s_szAttrSlots[i][0] = '\0';
		return;
	}

	for (int i = 0; i < 10; i++)
	{
		char* psz = s_szAttrSlots[i];
		psz[0] = '\0';
		fgets(psz, 255, pFile);
		psz[254] = '\0'; // VERIFY: the original writes psz[255], the next slot's first byte, which the next fgets overwrites
		if ('\0' != psz[0])
		{
			for (int j = (int)strlen(psz) - 1; j >= 0; j--)
			{
				if (psz[j] == '\n' || psz[j] == '\r')
					psz[j] = '\0';
			}
		}
	}
	fclose(pFile);
}

// orig client 0xcfa00
static void AttrMenu_SaveSlots()
{
	FILE* pFile = fopen(AttrMenu_SavedPath(), "w");
	if (!pFile)
		return;
	for (int i = 0; i < 10; i++)
		fprintf(pFile, ('\0' == s_szAttrSlots[i][0]) ? "-\n" : "%s\n", s_szAttrSlots[i]);
	fclose(pFile);
}

// The choices as a slot's line: "<ten points> <abilities> <weapons> <transform>".
// orig client 0xcfa80
static void AttrMenu_FormatSlot(int iSlot)
{
	char* pszLine = s_szAttrSlots[iSlot];
	char sz[256];
	pszLine[0] = '\0';
	for (int i = 0; i < ATTR_COUNT; i++)
	{
		sprintf(sz, "%d ", Attr_ValueToPoints(i, g_flAttrValues[i]));
		if ('\0' == pszLine[0])
			strcpy(pszLine, sz);
		else
			strcat(pszLine, sz);
	}
	sprintf(sz, "%u ", g_iAttrAbilities);
	strcat(pszLine, sz);
	sprintf(sz, "%u ", g_iAttrWeapons);
	strcat(pszLine, sz);
	sprintf(sz, "%d", g_iAttrTransform);
	strcat(pszLine, sz);
}

// A slot's line back into the choices; the defaults when it is short, bad or too dear.
// orig client 0xcfc60
static void AttrMenu_ParseSlot(int iSlot)
{
	const char* psz = s_szAttrSlots[iSlot];
	if (strlen(psz) > 2)
	{
		const char* pszNext = nullptr;
		int i = 0;
		while (true)
		{
			pszNext = nullptr;
			if (!psz)
				break;
			if ('\0' == *psz)
				goto fail;
			const int iPoints = Attr_ClampPoints(i, atoi(psz));
			g_flAttrValues[i] = Attr_PointsToValue(i, iPoints);
			i++;
			if (i < ATTR_COUNT)
				g_flAttrValues[i] = Attr_DefaultValue(i);
			psz = strpbrk(psz, "\n\t\r ");
			if (psz)
			{
				psz++;
				pszNext = psz;
				if ('\0' == *pszNext)
					goto fail;
			}
			pszNext = psz;
			if (i >= ATTR_COUNT)
				break;
		}

		g_iAttrAbilities = 0;
		if (pszNext)
		{
			sscanf(pszNext, "%u", &g_iAttrAbilities);
			const char* p = strpbrk(pszNext, "\n\t\r ");
			if (p && '\0' != p[1])
			{
				g_iAttrWeapons = 1;
				sscanf(p + 1, "%u", &g_iAttrWeapons);
				const char* q = strpbrk(p + 1, "\n\t\r ");
				if (q && '\0' != q[1])
				{
					g_iAttrTransform = -1;
					sscanf(q + 1, "%d", &g_iAttrTransform);
					if (AttrMenu_TotalCost() <= Attr_Total())
					{
						s_bAttrDirty = true;
						return;
					}
				}
			}
		}
	}
fail:
	AttrMenu_Reset();
	s_bAttrDirty = true;
}

// Each frame: the slot the slot menu chose is loaded or saved.
// orig client 0xcfe10
void AttrMenu_Think()
{
	if (s_iAttrSlot < 0 || s_iAttrSlot >= 10)
		return;

	AttrMenu_LoadSlots();
	if (s_bAttrSlotLoad)
	{
		AttrMenu_ParseSlot(s_iAttrSlot);
		s_iAttrSlot = -1;
		return;
	}
	AttrMenu_FormatSlot(s_iAttrSlot);
	AttrMenu_SaveSlots();
	s_iAttrSlot = -1;
}

// orig client 0xcfe90
void AttrMenu_Init()
{
	s_iAttrSlot = -1;
	s_bAttrSlotLoad = false;
}

// orig client 0xcfec0
void AttrMenu_Forget()
{
	s_iAttrSlot = -1;
	s_pAttrMenu = nullptr;
	s_bAttrSlotLoad = false;
}

// --- CAttrSlotMenu

// A slot button (0-9) or Cancel (-1): note the slot for AttrMenu_Think, and back to the
// attribute menu.
class CAttrSlotHandler : public ActionSignal // vtable 0x1011ff50
{
public:
	CAttrSlotHandler(int bSave, int iSlot, CAttrSlotMenu* pMenu) : m_bSave(bSave), m_iSlot(iSlot), m_pMenu(pMenu) {}
	void actionPerformed(Panel* panel) override;

private:
	int m_bSave;
	int m_iSlot;
	CAttrSlotMenu* m_pMenu;
};

// orig client 0xd06c0
void CAttrSlotHandler::actionPerformed(Panel* panel)
{
	if (m_iSlot >= 0 && m_iSlot < 10)
	{
		s_iAttrSlot = m_iSlot;
		s_bAttrSlotLoad = 0 == m_bSave;
	}

	bool bClosed = false;
	CMenuPanel* pNext = nullptr;
	if (m_pMenu)
	{
		pNext = m_pMenu->GetNextMenu();
		m_pMenu->Close();
		m_pMenu = nullptr;
		bClosed = true;
	}
	if (gViewPort && bClosed)
	{
		gViewPort->SetCurrentMenu(pNext);
		gViewPort->ShowVGUIMenu(RC2_MENU_ATTRIBUTES); // VERIFY: 0xe5410 is ShowVGUIMenu
	}
}

// orig client 0xcfef0
CAttrSlotMenu::CAttrSlotMenu(int bSave) : CMenuPanel(true, RC2_XRES(146), RC2_YRES(112), RC2_XRES(345), RC2_YRES(255))
{
	m_bSave = bSave;
	setBorder(new LineBorder(Color(178, 118, 0, 0)));

	const int yTop = RC2_YRES(30);
	int xSlot = RC2_XRES(10);
	CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitle = pSchemes->getSchemeHandle("Title Font");
	pSchemes->getSchemeHandle("Briefing Text");

	// VERIFY: as the original, the border is set twice (the first one leaks)
	setBorder(new LineBorder(Color(178, 118, 0, 0)));

	Label* pTitle = new Label("Save", 1, 1, RC2_XRES(335), RC2_YRES(25));
	pTitle->setParent(this);
	AttrMenu_SetLabelScheme(pTitle, pSchemes, hTitle);
	pTitle->setContentAlignment(Label::a_north);
	pTitle->setText(bSave ? "Save" : "Load");

	int ySlot = yTop;
	for (int i = 1; i <= 10; i++)
	{
		char sz[64];
		sprintf(sz, "Slot %d", i);
		CommandButton* pButton = new CommandButton(sz, xSlot, ySlot, RC2_XRES(160), RC2_YRES(30));
		pButton->addActionSignal(new CAttrSlotHandler(m_bSave, i - 1, this));
		pButton->setParent(this);

		const int tall = RC2_YRES(30);
		ySlot += tall + RC2_YRES(5);
		if (i % 5 == 0)
		{
			xSlot += RC2_XRES(160) + RC2_XRES(5);
			ySlot = tall;
		}
	}

	CommandButton* pCancel = new CommandButton("Cancel", RC2_XRES(10), RC2_YRES(210), RC2_XRES(325), RC2_YRES(30));
	pCancel->addActionSignal(new CAttrSlotHandler(m_bSave, -1, this));
	pCancel->setParent(this);

	SetMenuID(bSave + 99); // VERIFY: RC2's ids for the slot menus
	Reset();
	Open();
	if (gViewPort)
	{
		setParent(gViewPort);
		gViewPort->SetCurrentMenu(this);
		gViewPort->UpdateCursorState();
	}
	Panel::setVisible(true);
}

// CMenuPanel::SlotInput: halflife-updated's inline in vgui_TeamFortressViewport.h (package c5)
// reads the same as orig client 0xd0840, and its SetActiveInfo as 0xd0860 (an empty one of RC2's)
