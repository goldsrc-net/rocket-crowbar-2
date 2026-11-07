/***
*
*	Rocket Crowbar 2 (client): the attribute menu ("attrconfig", the AttrConf message). Ten
*	attributes bought with points (a spinner each), ten abilities, nine weapons and one of five
*	transforms (check buttons), the points left, and OK (sends "setattr"), Load and Save (ten
*	slots in <gamedir>/savedattrs.txt) and Restore Defaults.
*
*	client.dll 0xcbdf0-0xd2240, in place of SDK 2.3's vgui_ClassMenu.cpp. All names ours.
*
****/

#pragma once

#include "vgui_TeamFortressViewport.h"

// rc2.ini's attribute prices (attr_costs.cpp; the server's are in dlls/client.cpp)
int Attr_ClampPoints(int i, int iPoints);
float Attr_PointsToValue(int i, int iPoints);
int Attr_ValueToPoints(int i, float flValue);
int Attr_AbilityCost(unsigned int iBit);
int Attr_AbilitiesCost(unsigned int iBits);
int Attr_WeaponCost(unsigned int iBit);
int Attr_WeaponsCost(unsigned int iBits);
int Attr_TransformCost(int iTransform);
float Attr_DefaultValue(int i);

#define ATTR_COUNT 10
#define ATTR_ABILITIES 10
#define ATTR_WEAPONS 9
#define ATTR_TRANSFORMS 5

// RC2's own menu id for the attribute menu: the slot menu reopens it with ShowVGUIMenu(9)
#define RC2_MENU_ATTRIBUTES 9 // VERIFY: what TeamFortressViewport::ShowVGUIMenu (package c5) does with 9

// XRES/YRES as RC2 writes them: scaled and rounded
#define RC2_XRES(x) ((int)((float)ScreenWidth * (1.0f / 640) * (x) + 0.5f))
#define RC2_YRES(y) ((int)((float)ScreenHeight * (1.0f / 480) * (y) + 0.5f))

// The choices (.data 0x101755b0-0x101755e0, .bss 0x10221aa4-0x10221ab0)
extern float g_flAttrValues[ATTR_COUNT]; // 0x101755b0: each attribute's value (not points)
extern unsigned int g_iAttrAbilities;	 // 0x10221aa8
extern unsigned int g_iAttrWeapons;		 // 0x101755d8, 1 (the glock) at start
extern int g_iAttrTransform;			 // 0x101755dc, -1 for none

// A number with "+" and "-" buttons (shift steps 5).
class CAttrSpinner : public vgui::Panel
{
public:
	typedef void (*ChangedFunc)(float flValue, int iParam);

	CAttrSpinner(ChangedFunc pfnChanged, int iParam, const char* pszText, const char* pszFormat, float flValue, float flMin, float flMax, float flStep, int x, int y, int wide);

	virtual void paintBackground();
	virtual void Step(int iDir); // slot 0x1fc
	virtual void SetValue(float flValue); // slot 0x200

private:
	vgui::Label* m_pLabel; // 0xbc
	float m_flValue;	   // 0xc0
	float m_flMin;		   // 0xc4
	float m_flMax;		   // 0xc8
	float m_flStep;		   // 0xcc
	char m_szText[128];	   // 0xd0: put before the number
	char m_szFormat[32];   // 0x150
	ChangedFunc m_pfnChanged; // 0x170
	int m_iParam;		   // 0x174
	bool m_bReady;		   // 0x178: the callback runs only once the constructor is done
};

// A check button drawn with two TGA images ("%d_checked", "%d_unchecked"); a CTransparentPanel
// over it takes the mouse.
class CAttrCheckButton : public vgui::Panel
{
public:
	typedef void (*ToggledFunc)(bool bChecked, int iParam);

	CAttrCheckButton(const char* pszText, int x, int y, int wide, int tall, bool bChecked, ToggledFunc pfnToggled, int iParam);

	virtual void paintBackground();
	void UpdateImages(); // inlined in the original
	void Toggle();		 // client.dll 0xe6c80 (the overlay's mouseReleased)

	vgui::BitmapTGA* m_pChecked;   // 0xbc
	vgui::BitmapTGA* m_pUnchecked; // 0xc0
	int m_iFgColor[4];			   // 0xc4: the text's colour from "Briefing Text"
	vgui::Label* m_pImageFront;	   // 0xd4
	vgui::Label* m_pImageBack;	   // 0xd8
	vgui::Label* m_pText;		   // 0xdc
	bool m_bChecked;			   // 0xe0
	bool m_bHighlight;			   // 0xe1: the cursor is over it
	ToggledFunc m_pfnToggled;	   // 0xe4
	int m_iParam;				   // 0xe8
	bool m_bReady;				   // 0xec
};

// One attribute: its name and value, and a spinner of points.
class CAttrRow : public vgui::Panel
{
public:
	CAttrRow(int i, int x, int y, int wide, int tall);
	virtual void paintBackground();

	CAttrSpinner* m_pSpinner; // 0xbc
	vgui::Label* m_pLabel;	  // 0xc0
	char m_szFormat[128];	  // 0xc4: "<name> - %-4.2f:"
};

class CAttributeMenu : public CMenuPanel
{
public:
	CAttributeMenu(int x, int y, int wide, int tall);

	virtual void paintBackground();
	void UpdateTotal();
	void Refresh();

	CAttrRow* m_pRows[ATTR_COUNT];						 // 0xd4
	CAttrCheckButton* m_pAbilities[ATTR_ABILITIES];		 // 0xfc
	CAttrCheckButton* m_pWeapons[ATTR_WEAPONS];			 // 0x124
	CAttrCheckButton* m_pTransforms[ATTR_TRANSFORMS];	 // 0x148
	bool m_bReady;										 // 0x15c
	vgui::Label* m_pTotalLabel;							 // 0x160
};

// The Load/Save dialog: ten slot buttons and Cancel.
class CAttrSlotMenu : public CMenuPanel
{
public:
	CAttrSlotMenu(int bSave);
	int m_bSave; // 0xd4
};

// for TeamFortressViewport (package c5) and the frame code (package c2)
CAttributeMenu* AttrMenu_Create(vgui::Panel* pParent); // client.dll 0xcf480 (from ShowVGUIMenu, 0xe54de)
void AttrMenu_Think();		 // 0xcfe10 (from 0x4ffc2): a slot chosen in the slot menu is loaded or saved
void AttrMenu_Init();		 // 0xcfe90 (from 0x4fe99)
void AttrMenu_Forget();		 // 0xcfec0 (from 0xe2712): the viewport is gone
