//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Client DLL VGUI Viewport
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
// Rocket Crowbar 2: its viewport is SDK 2.1's (the one Ricochet still ships: one command menu,
// the spectator "menu" of labels, the server browser) with SDK 2.3's team menu, TFC's class
// code taken out, an empty command menu, and its own vote box, player name label and debug
// panel. The functions RC2 doesn't have are halflife-updated's and kept only for the files
// that still call them (reference/dossiers/c5.md).
#include <VGUI_Cursor.h>
#include <VGUI_Frame.h>
#include <VGUI_Label.h>
#include <VGUI_Surface.h>
#include <VGUI_BorderLayout.h>
#include <VGUI_Panel.h>
#include <VGUI_ImagePanel.h>
#include <VGUI_Button.h>
#include <VGUI_ActionSignal.h>
#include <VGUI_InputSignal.h>
#include <VGUI_MenuSeparator.h>
#include <VGUI_TextPanel.h>
#include <VGUI_LoweredBorder.h>
#include <VGUI_LineBorder.h>
#include <VGUI_Scheme.h>
#include <VGUI_Font.h>
#include <VGUI_App.h>
#include <VGUI_BuildGroup.h>

#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "parsemsg.h"
#include "pm_shared.h"
#include "keydefs.h"
#include "demo.h"
#include "demo_api.h"

#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"
#include "vgui_ScorePanel.h"
#include "vgui_SpectatorPanel.h"
#include "rc2/vgui_ServerBrowser.h"
#include "rc2/vgui_DebugPanel.h"
#include "rc2/vgui_VoteMenu.h"

#include "shake.h"
#include "screenfade.h"

extern bool g_iVisibleMouse;
class CCommandMenu;
int g_iPlayerClass;
int g_iTeamNumber;
int g_iUser1 = 0;
int g_iUser2 = 0;
int g_iUser3 = 0;

// Scoreboard positions
#define SBOARD_INDENT_X XRES(104)
#define SBOARD_INDENT_Y YRES(40)

// low-res scoreboard indents
#define SBOARD_INDENT_X_512 30
#define SBOARD_INDENT_Y_512 30

#define SBOARD_INDENT_X_400 0
#define SBOARD_INDENT_Y_400 20

void IN_ResetMouse();
void IN_ResetRelativeMouseState();
extern CMenuPanel* CMessageWindowPanel_Create(const char* szMOTD, const char* szTitle, bool iShadeFullscreen, bool iRemoveMe, int x, int y, int wide, int tall);
extern float* GetClientColor(int clientIndex);

// Rocket Crowbar 2's attribute menu (menu 9, AttrConf), in RC2's own client code before
// vgui_CustomObjects (package c4)
#include "rc2/vgui_attributes.h" // AttrMenu_Create (client 0xcf480), AttrMenu_Forget (0xcfec0)

using namespace vgui;

// Team Colors
int iNumberOfTeamColors = 5;
int iTeamColors[5][3] =
	{
		{255, 170, 0},	 // HL orange (default)
		{125, 165, 210}, // Blue
		{200, 90, 70},	 // Red
		{225, 205, 45},	 // Yellow
		{145, 215, 140}, // Green
};


// Used for Class specific buttons
const char* sTFClasses[] =
	{
		"",
		"SCOUT",
		"SNIPER",
		"SOLDIER",
		"DEMOMAN",
		"MEDIC",
		"HWGUY",
		"PYRO",
		"SPY",
		"ENGINEER",
		"CIVILIAN",
};

const char* sLocalisedClasses[] =
	{
		"#Civilian",
		"#Scout",
		"#Sniper",
		"#Soldier",
		"#Demoman",
		"#Medic",
		"#HWGuy",
		"#Pyro",
		"#Spy",
		"#Engineer",
		"#Random",
		"#Civilian",
};

const char* sTFClassSelection[] =
	{
		"civilian",
		"scout",
		"sniper",
		"soldier",
		"demoman",
		"medic",
		"hwguy",
		"pyro",
		"spy",
		"engineer",
		"randompc",
		"civilian",
};


// Get the name of TGA file, based on GameDir
// orig client 0xe1da0
char* GetVGUITGAName(const char* pszName)
{
	int i;
	char sz[256];
	static char gd[256];
	const char* gamedir;

	if (ScreenWidth < 640)
		i = 320;
	else
		i = 640;
	sprintf(sz, pszName, i);

	gamedir = gEngfuncs.pfnGetGameDirectory();
	sprintf(gd, "%s/gfx/vgui/%s.tga", gamedir, sz);

	return gd;
}

//================================================================
// COMMAND MENU
//================================================================
void CCommandMenu::AddButton(CommandButton* pButton)
{
	if (m_iButtons >= MAX_BUTTONS)
		return;

	m_aButtons[m_iButtons] = pButton;
	m_iButtons++;
	pButton->setParent(this);
	pButton->setFont(Scheme::sf_primary3);

	// give the button a default key binding
	if (m_iButtons < 10)
	{
		pButton->setBoundKey(m_iButtons + '0');
	}
	else if (m_iButtons == 10)
	{
		pButton->setBoundKey('0');
	}
}

void CCommandMenu::RemoveAllButtons()
{
	/*
	for(int i=0;i<m_iButtons;i++)
	{
		CommandButton *pTemp = m_aButtons[i]; 
		m_aButtons[i] = NULL;
		
		pTemp
		if(pTemp)
		{
			delete(pTemp);
		}
		
	}
	*/
	removeAllChildren();
	m_iButtons = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Tries to find a button that has a key bound to the input, and
//			presses the button if found
// Input  : keyNum - the character number of the input key
// Output : Returns true if the command menu should close, false otherwise
//-----------------------------------------------------------------------------
// orig client 0xe1eb0
bool CCommandMenu::KeyInput(int keyNum)
{
	// loop through all our buttons looking for one bound to keyNum
	for (int i = 0; i < m_iButtons; i++)
	{
		if (!m_aButtons[i]->IsNotValid())
		{
			if (m_aButtons[i]->getBoundKey() == keyNum)
			{
				// hit the button
				if (m_aButtons[i]->GetSubMenu())
				{
					// open the sub menu
					gViewPort->SetCurrentCommandMenu(m_aButtons[i]->GetSubMenu());
					return false;
				}
				else
				{
					// run the bound command
					m_aButtons[i]->fireActionSignal();
					return true;
				}
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: clears the current menus buttons of any armed (highlighted)
//			state, and all their sub buttons
//-----------------------------------------------------------------------------
// orig client 0xe1f70
void CCommandMenu::ClearButtonsOfArmedState()
{
	for (int i = 0; i < GetNumButtons(); i++)
	{
		m_aButtons[i]->setArmed(false);

		if (m_aButtons[i]->GetSubMenu())
		{
			m_aButtons[i]->GetSubMenu()->ClearButtonsOfArmedState();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pSubMenu -
// Output : CommandButton
//-----------------------------------------------------------------------------
// orig client 0xe1fe0
CommandButton* CCommandMenu::FindButtonWithSubmenu(CCommandMenu* pSubMenu)
{
	for (int i = 0; i < GetNumButtons(); i++)
	{
		if (m_aButtons[i]->GetSubMenu() == pSubMenu)
			return m_aButtons[i];
	}

	return NULL;
}

// Recalculate the visible buttons
// orig client 0xe2030. SDK 2.1's: no player class buttons, and every button is BUTTON_SIZE_Y tall.
bool CCommandMenu::RecalculateVisibles(int iNewYPos, bool bHideAll)
{
	int iCurrentY = 0;
	int iXPos, iYPos;
	bool bHasButton = false;

	if (0 != iNewYPos)
		setPos(_pos[0], iNewYPos);

	// Cycle through all the buttons in this menu, and see which will be visible
	for (int i = 0; i < m_iButtons; i++)
	{
		if (m_aButtons[i]->IsNotValid() || bHideAll)
		{
			m_aButtons[i]->setVisible(false);
			if (m_aButtons[i]->GetSubMenu() != NULL)
			{
				(m_aButtons[i]->GetSubMenu())->RecalculateVisibles(_pos[1] + iCurrentY, true);
			}
		}
		else
		{
			// If it's got a submenu, force it to check visibilities
			if (m_aButtons[i]->GetSubMenu() != NULL)
			{
				if (!(m_aButtons[i]->GetSubMenu())->RecalculateVisibles(_pos[1] + iCurrentY, false))
				{
					// The submenu had no visible buttons, so don't display this button
					m_aButtons[i]->setVisible(false);
					continue;
				}
			}

			m_aButtons[i]->setVisible(true);

			// Make sure it's at the right Y position
			m_aButtons[i]->getPos(iXPos, iYPos);
			m_aButtons[i]->setPos(iXPos, iCurrentY);

			iCurrentY += BUTTON_SIZE_Y - 1;
			bHasButton = true;
		}
	}

	// Set Size
	setSize(_size[0], iCurrentY + 1);

	return bHasButton;
}

// Make sure all submenus can fit on the screen
// orig client 0xe21a0. SDK 2.1's.
void CCommandMenu::RecalculatePositions(int iYOffset)
{
	int iNewYPos = _pos[1] + iYOffset;
	int iAdjust = 0;

	// Calculate if this is going to fit onscreen, and shuffle it up if it won't
	int iBottom = iNewYPos + _size[1];
	if (iBottom > ScreenHeight)
	{
		// Move in increments of button sizes
		while (iAdjust < (iBottom - ScreenHeight))
		{
			iAdjust += BUTTON_SIZE_Y - 1;
		}
		iNewYPos -= iAdjust;

		// Make sure it doesn't move off the top of the screen (the menu's too big to fit it all)
		if (iNewYPos < 0)
		{
			iAdjust -= (0 - iNewYPos);
			iNewYPos = 0;
		}
	}

	// We need to force all menus below this one to update their positions now, because they
	// might have submenus riding off buttons in this menu that have just shifted.
	for (int i = 0; i < m_iButtons; i++)
		m_aButtons[i]->UpdateSubMenus(iAdjust);

	setPos(_pos[0], iNewYPos);
}


// Make this menu and all menus above it in the chain visible
void CCommandMenu::MakeVisible(CCommandMenu* pChildMenu)
{
	/*
	// Push down the button leading to the child menu
	for (int i = 0; i < m_iButtons; i++)
	{
		if ( (pChildMenu != NULL) && (m_aButtons[i]->GetSubMenu() == pChildMenu) )
		{
			m_aButtons[i]->setArmed( true );
		}
		else
		{
			m_aButtons[i]->setArmed( false );
		}
	}
*/

	setVisible(true);
	if (m_pParentMenu)
		m_pParentMenu->MakeVisible(this);
}

//================================================================
// CreateSubMenu
// orig client 0xe22c0. SDK 2.1's.
CCommandMenu* TeamFortressViewport::CreateSubMenu(CommandButton* pButton, CCommandMenu* pParentMenu)
{
	int iXPos = 0;
	int iYPos = 0;
	int iWide = CMENU_SIZE_X;
	int iTall = 0;

	if (pParentMenu)
	{
		iXPos = pParentMenu->GetXOffset() + CMENU_SIZE_X - 1;
		iYPos = pParentMenu->GetYOffset() + BUTTON_SIZE_Y * (m_pCurrentCommandMenu->GetNumButtons() - 1);
	}

	CCommandMenu* pMenu = new CCommandMenu(pParentMenu, iXPos, iYPos, iWide, iTall);
	pMenu->setParent(this);
	pButton->AddSubMenu(pMenu);
	pButton->setFont(Scheme::sf_primary3);

	// Create the Submenu-open signal
	InputSignal* pISignal = new CMenuHandler_PopupSubMenuInput(pButton, pMenu);
	pButton->addInputSignal(pISignal);

	// Put a > to show it's a submenu
	CImageLabel* pLabel = new CImageLabel("arrow", CMENU_SIZE_X - SUBMENU_SIZE_X, SUBMENU_SIZE_Y);
	pLabel->setParent(pButton);
	pLabel->addInputSignal(pISignal);

	// Reposition
	pLabel->getPos(iXPos, iYPos);
	pLabel->setPos(CMENU_SIZE_X - pLabel->getImageWide(), (BUTTON_SIZE_Y - pLabel->getImageTall()) / 2);

	// Create the mouse off signal for the Label too
	if (!pButton->m_bNoHighlight)
		pLabel->addInputSignal(new CHandler_CommandButtonHighlight(pButton));

	return pMenu;
}

//-----------------------------------------------------------------------------
// Purpose: Makes sure the memory allocated for TeamFortressViewport is nulled out
// Input  : stAllocateBlock -
// Output : void *
//-----------------------------------------------------------------------------
void* TeamFortressViewport::operator new(size_t stAllocateBlock)
{
	//	void *mem = Panel::operator new( stAllocateBlock );
	void* mem = ::operator new(stAllocateBlock);
	memset(mem, 0, stAllocateBlock);
	return mem;
}

//-----------------------------------------------------------------------------
// Purpose: InputSignal handler for the main viewport
//-----------------------------------------------------------------------------
class CViewPortInputHandler : public InputSignal
{
public:
	bool bPressed;

	CViewPortInputHandler()
	{
	}

	void cursorMoved(int x, int y, Panel* panel) override {}
	void cursorEntered(Panel* panel) override {}
	void cursorExited(Panel* panel) override {}
	void mousePressed(MouseCode code, Panel* panel) override
	{
		if (code != MOUSE_LEFT)
		{
			// send a message to close the command menu
			// this needs to be a message, since a direct call screws the timing
			gEngfuncs.pfnClientCmd("ForceCloseCommandMenu\n");
		}
	}
	void mouseReleased(MouseCode code, Panel* panel) override
	{
	}

	void mouseDoublePressed(MouseCode code, Panel* panel) override {}
	void mouseWheeled(int delta, Panel* panel) override {}
	void keyPressed(KeyCode code, Panel* panel) override {}
	void keyTyped(KeyCode code, Panel* panel) override {}
	void keyReleased(KeyCode code, Panel* panel) override {}
	void keyFocusTicked(Panel* panel) override {}
};


//================================================================
// orig client 0xe2690
TeamFortressViewport::TeamFortressViewport(int x, int y, int wide, int tall) : Panel(x, y, wide, tall), m_SchemeManager(wide, tall)
{
	gViewPort = this;
	m_iInitialized = false;
	m_pTeamMenu = NULL;
	m_pScoreBoard = NULL;
	m_pSpectatorMenu = NULL;
	m_pCurrentMenu = NULL;
	m_pCurrentCommandMenu = NULL;

	AttrMenu_Forget();

	Initialize();
	addInputSignal(new CViewPortInputHandler);

	int r, g, b, a;

	Scheme* pScheme = App::getInstance()->getScheme();

	// primary text color
	// Get the colors
	//!! two different types of scheme here, need to integrate
	SchemeHandle_t hPrimaryScheme = m_SchemeManager.getSchemeHandle("Primary Button Text");
	{
		// font
		pScheme->setFont(Scheme::sf_primary1, m_SchemeManager.getFont(hPrimaryScheme));

		// text color
		m_SchemeManager.getFgColor(hPrimaryScheme, r, g, b, a);
		pScheme->setColor(Scheme::sc_primary1, r, g, b, a); // sc_primary1 is non-transparent orange

		// background color (transparent black)
		m_SchemeManager.getBgColor(hPrimaryScheme, r, g, b, a);
		pScheme->setColor(Scheme::sc_primary3, r, g, b, a);

		// armed foreground color
		m_SchemeManager.getFgArmedColor(hPrimaryScheme, r, g, b, a);
		pScheme->setColor(Scheme::sc_secondary2, r, g, b, a);

		// armed background color
		m_SchemeManager.getBgArmedColor(hPrimaryScheme, r, g, b, a);
		pScheme->setColor(Scheme::sc_primary2, r, g, b, a);

		//!! need to get this color from scheme file
		// used for orange borders around buttons
		m_SchemeManager.getBorderColor(hPrimaryScheme, r, g, b, a);
		// pScheme->setColor(Scheme::sc_secondary1, r, g, b, a );
		pScheme->setColor(Scheme::sc_secondary1, 255 * 0.7, 170 * 0.7, 0, 0);
	}

	// Change the second primary font (used in the scoreboard)
	SchemeHandle_t hScoreboardScheme = m_SchemeManager.getSchemeHandle("Scoreboard Text");
	{
		pScheme->setFont(Scheme::sf_primary2, m_SchemeManager.getFont(hScoreboardScheme));
	}

	// Change the third primary font (used in command menu)
	SchemeHandle_t hCommandMenuScheme = m_SchemeManager.getSchemeHandle("CommandMenu Text");
	{
		pScheme->setFont(Scheme::sf_primary3, m_SchemeManager.getFont(hCommandMenuScheme));
	}

	App::getInstance()->setScheme(pScheme);

	// VGUI MENUS
	CreateTeamMenu();
	CreateScoreBoard();
	CreateCommandMenu();
	CreateServerBrowser();
	CreateSpectatorMenu();

	// Rocket Crowbar 2: the vote box (top right) and the name of the player under the crosshair
	// (bottom centre), both in the briefing font
	CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hBriefingText = pSchemes->getSchemeHandle("Briefing Text");

	m_pVotePanel = new CTransparentPanel(255, XRES(435), YRES(5), XRES(200), YRES(100));
	m_pVotePanel->setParent(this);
	m_pVotePanel->setVisible(false);

	m_pVoteLabel = new Label("", 0, 0, XRES(200), YRES(100));
	m_pVoteLabel->setParent(m_pVotePanel);
	m_pVoteLabel->setFont(pSchemes->getFont(hBriefingText));
	m_pVoteLabel->setFont(Scheme::sf_primary1); // VERIFY: slot 0x20c, setFont(SchemeFont 1), right after setFont(Font*)
	pSchemes->getFgColor(hBriefingText, r, g, b, a);
	m_pVoteLabel->setFgColor(r, g, b, a);
	m_pVoteLabel->setBgColor(0, 0, 0, 255);
	m_pVoteLabel->setContentAlignment(vgui::Label::a_northeast);
	m_pVoteLabel->setText("");

	m_pPlayerIDLabel = new Label("", ScreenWidth / 2 - XRES(320) / 2,
		ScreenHeight - YRES(18) - YRES(100) - gHUD.m_iFontHeight / 2 - gHUD.m_iFontHeight, XRES(320), YRES(18));
	m_pPlayerIDLabel->setParent(this);
	m_pPlayerIDLabel->setFont(pSchemes->getFont(hBriefingText));
	m_pPlayerIDLabel->setFont(Scheme::sf_primary1);
	pSchemes->getFgColor(hBriefingText, m_iPlayerIDColor[0], m_iPlayerIDColor[1], m_iPlayerIDColor[2], a);
	m_pPlayerIDLabel->setFgColor(m_iPlayerIDColor[0], m_iPlayerIDColor[1], m_iPlayerIDColor[2], 255);
	m_pPlayerIDLabel->setBgColor(0, 0, 0, 255);
	m_pPlayerIDLabel->setContentAlignment(vgui::Label::a_center);
	m_pPlayerIDLabel->setText("");
	m_pPlayerIDTeamColor = NULL;

	m_pDebugPanel = new CDebugPanel();
	m_pDebugPanel->setParent(this);
}

//-----------------------------------------------------------------------------
// Purpose: Called everytime a new level is started. Viewport clears out it's data.
//-----------------------------------------------------------------------------
// orig client 0xe3150
void TeamFortressViewport::Initialize()
{
	// Force each menu to Initialize
	if (m_pTeamMenu)
	{
		m_pTeamMenu->Initialize();
	}
	if (m_pScoreBoard)
	{
		m_pScoreBoard->Initialize();
		HideScoreBoard();
	}
	if (m_pSpectatorMenu)
	{
		// Spectator menu doesn't need initializing
		m_pSpectatorMenu->setVisible(false);
	}

	// Make sure all menus are hidden
	HideVGUIMenu();
	HideCommandMenu();

	// Clear out some data
	m_iGotAllMOTD = true;
	m_flScoreBoardLastUpdated = 0;

	// reset player info
	g_iTeamNumber = 0;

	strcpy(m_sMapName, "");
	strcpy(m_szServerName, "");
	for (int i = 0; i < 5; i++)
	{
		strcpy(m_sTeamNames[i], "");
	}

	App::getInstance()->setCursorOveride(App::getInstance()->getScheme()->getCursor(Scheme::scu_none));
}

//-----------------------------------------------------------------------------
// Purpose: Read the Command Menu structure from the txt file and create the menu.
//-----------------------------------------------------------------------------
// orig client 0xe32c0. SDK 2.1's, without the class buttons and the detpack strings. Its body
// was in a try block whose catch (orig client 0xe3b4d) cleared m_iInitialized and returned;
// nothing in it throws a C++ exception, so the try isn't kept (halflife-updated dropped it too).
void TeamFortressViewport::CreateCommandMenu()
{
	// COMMAND MENU
	// Create the root of the Command Menu
	m_pCommandMenus[0] = new CCommandMenu(NULL, 0, CMENU_TOP, CMENU_SIZE_X, 300); // This will be resized once we know how many items are in it
	m_pCommandMenus[0]->setParent(this);
	m_pCommandMenus[0]->setVisible(false);
	m_iNumMenus = 1;
	m_iCurrentTeamNumber = m_iUser1 = m_iUser2 = m_iUser3 = 0;
	m_StandardMenu = 0;

	// Read Command Menu from the txt file
	char token[1024];
	char* pfile = (char*)gEngfuncs.COM_LoadFile("commandmenu.txt", 5, NULL);
	if (!pfile)
	{
		gEngfuncs.Con_DPrintf("Unable to open commandmenu.txt\n");
		SetCurrentCommandMenu(NULL);
		return;
	}
	char* pfileStart = pfile; // name ours

	// Now start parsing the menu structure
	m_pCurrentCommandMenu = m_pCommandMenus[0];
	char szLastButtonText[32] = "file start";
	pfile = gEngfuncs.COM_ParseFile(pfile, token);
	while ((strlen(token) > 0) && (m_iNumMenus < MAX_MENUS))
	{
		// Keep looping until we hit the end of this menu
		while (token[0] != '}' && (strlen(token) > 0))
		{
			char cText[32] = "";
			char cBoundKey[32] = "";
			char cCustom[32] = "";
			static const int cCommandLength = 128;
			char cCommand[cCommandLength] = "";
			char szMap[MAX_MAPNAME] = "";
			int iCustom = false;
			int iTeamOnly = 0;
			bool bGetExtraToken = true;
			CommandButton* pButton = NULL;

			// We should never be here without a Command Menu
			if (!m_pCurrentCommandMenu)
			{
				gEngfuncs.Con_Printf("Error in Commandmenu.txt file after '%s'.\n", szLastButtonText);
				m_iInitialized = false;
				return;
			}

			// token should already be the bound key, or the custom name
			strncpy(cCustom, token, 32);
			cCustom[31] = '\0';

			// See if it's a custom button
			if (0 == strcmp(cCustom, "CUSTOM"))
			{
				iCustom = true;

				// Get the next token
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
			// See if it's a map
			else if (0 == strcmp(cCustom, "MAP"))
			{
				// Get the mapname
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
				strncpy(szMap, token, MAX_MAPNAME);
				szMap[MAX_MAPNAME - 1] = '\0';

				// Get the next token
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
			else if (0 == strncmp(cCustom, "TEAM", 4)) // TEAM1, TEAM2, TEAM3, TEAM4
			{
				// make it a team only button
				iTeamOnly = atoi(cCustom + 4);

				// Get the next token
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}

			// Get the button bound key
			strncpy(cBoundKey, token, 32);
			cText[31] = '\0';

			// Get the button text
			pfile = gEngfuncs.COM_ParseFile(pfile, token);
			strncpy(cText, token, 32);
			cText[31] = '\0';

			// save off the last button text we've come across (for error reporting)
			strcpy(szLastButtonText, cText);

			// Get the button command
			pfile = gEngfuncs.COM_ParseFile(pfile, token);
			strncpy(cCommand, token, cCommandLength);
			cCommand[cCommandLength - 1] = '\0';

			// Custom button handling
			if (iCustom)
			{
				pButton = CreateCustomButton(cText, cCommand);

				// Get the next token to see if we're a menu
				pfile = gEngfuncs.COM_ParseFile(pfile, token);

				if (token[0] == '{')
				{
					strcpy(cCommand, token);
				}
				else
				{
					bGetExtraToken = false;
				}
			}
			else if (szMap[0] != '\0')
			{
				// create a map button
				pButton = new MapButton(szMap, cText, 0, BUTTON_SIZE_Y * m_pCurrentCommandMenu->GetNumButtons(), CMENU_SIZE_X, BUTTON_SIZE_Y);
			}
			else if (0 != iTeamOnly)
			{
				// button that only shows up if the player is on team iTeamOnly
				pButton = new TeamOnlyCommandButton(iTeamOnly, cText, 0, BUTTON_SIZE_Y * m_pCurrentCommandMenu->GetNumButtons(), CMENU_SIZE_X, BUTTON_SIZE_Y, false);
			}
			else
			{
				// normal button
				pButton = new CommandButton(cText, 0, BUTTON_SIZE_Y * m_pCurrentCommandMenu->GetNumButtons(), CMENU_SIZE_X, BUTTON_SIZE_Y);
			}

			// add the button into the command menu
			if (pButton)
			{
				m_pCurrentCommandMenu->AddButton(pButton);
				pButton->setBoundKey(cBoundKey[0]);
				pButton->setParentMenu(m_pCurrentCommandMenu);

				// Override font in CommandMenu
				pButton->setFont(Scheme::sf_primary3);
			}

			// Find out if it's a submenu or a button we're dealing with
			if (cCommand[0] == '{')
			{
				if (m_iNumMenus >= MAX_MENUS)
				{
					gEngfuncs.Con_Printf("Too many menus in commandmenu.txt past '%s'\n", szLastButtonText);
				}
				// Original bug (crash), guarded: RC2 doesn't check pButton here (SDK 2.1 neither);
				// a CUSTOM button it doesn't know followed by '{' passes NULL and crashes. Ours
				// makes no submenu for it.
				else if (pButton)
				{
					// Create the menu
					m_pCommandMenus[m_iNumMenus] = CreateSubMenu(pButton, m_pCurrentCommandMenu);
					m_pCurrentCommandMenu = m_pCommandMenus[m_iNumMenus];
					m_iNumMenus++;
				}
			}
			else if (!iCustom)
			{
				// Create the button and attach it to the current menu
				pButton->addActionSignal(new CMenuHandler_StringCommand(cCommand));
				// Create an input signal that'll popup the current menu
				pButton->addInputSignal(new CMenuHandler_PopupSubMenuInput(pButton, m_pCurrentCommandMenu));
			}

			// Get the next token
			if (bGetExtraToken)
			{
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
		}

		// Move back up a menu
		m_pCurrentCommandMenu = m_pCurrentCommandMenu->GetParentMenu();

		pfile = gEngfuncs.COM_ParseFile(pfile, token);
	}

	SetCurrentMenu(NULL);
	SetCurrentCommandMenu(NULL);
	// Original bug (crash), guarded: the original (as SDK 2.3) frees the advanced parse pointer,
	// NULL at the file's end but inside the file when MAX_MENUS stops the loop (a bad free)
	gEngfuncs.COM_FreeFile(pfileStart);

	m_iInitialized = true;
}

//-----------------------------------------------------------------------------
// Purpose: Creates all the class choices under a spy's disguise menus, and
//			maps a command to them
// Output : CCommandMenu
//-----------------------------------------------------------------------------
// orig client 0xe4300. SDK 2.1's; RC2's CreateCustomButton has no disguise buttons, so nothing calls it.
CCommandMenu* TeamFortressViewport::CreateDisguiseSubmenu(CommandButton* pButton, CCommandMenu* pParentMenu, const char* commandText)
{
	// create the submenu, under which the class choices will be listed
	CCommandMenu* pMenu = CreateSubMenu(pButton, pParentMenu);
	m_pCommandMenus[m_iNumMenus] = pMenu;
	m_iNumMenus++;

	// create the class choice buttons
	for (int i = 1; i <= 9; i++) // PC_SCOUT to PC_ENGINEER
	{
		CommandButton* pDisguiseButton = new CommandButton(CHudTextMessage::BufferedLocaliseTextString(sLocalisedClasses[i]), 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y);

		char sz[256];
		sprintf(sz, "%s %d", commandText, i);
		pDisguiseButton->addActionSignal(new CMenuHandler_StringCommand(sz));

		pMenu->AddButton(pDisguiseButton);
	}

	return pMenu;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pButtonText -
//			*pButtonName -
// Output : CommandButton
//-----------------------------------------------------------------------------
// orig client 0xe45f0. RC2 keeps only the map briefing and server info buttons of SDK 2.1's, both
// plain CommandButtons.
CommandButton* TeamFortressViewport::CreateCustomButton(char* pButtonText, char* pButtonName)
{
	CommandButton* pButton = NULL;

	// Map Briefing
	if (0 == strcmp(pButtonName, "!MAPBRIEFING"))
	{
		pButton = new CommandButton(pButtonText, 0, BUTTON_SIZE_Y * m_pCurrentCommandMenu->GetNumButtons(), CMENU_SIZE_X, BUTTON_SIZE_Y);
		pButton->addActionSignal(new CMenuHandler_TextWindow(MENU_MAPBRIEFING));
	}
	else if (0 == strcmp(pButtonName, "!SERVERINFO"))
	{
		pButton = new CommandButton(pButtonText, 0, BUTTON_SIZE_Y * m_pCurrentCommandMenu->GetNumButtons(), CMENU_SIZE_X, BUTTON_SIZE_Y);
		pButton->addActionSignal(new CMenuHandler_TextWindow(MENU_INTRO));
	}
	else
	{
		return NULL;
	}

	// Create an input signal that'll popup the current menu
	pButton->addInputSignal(new CMenuHandler_PopupSubMenuInput(pButton, m_pCurrentCommandMenu));

	return pButton;
}

// orig client 0xe49b0
void TeamFortressViewport::ToggleServerBrowser()
{
	if (!m_iInitialized)
		return;

	if (!m_pServerBrowser)
		return;

	if (m_pServerBrowser->isVisible())
	{
		m_pServerBrowser->setVisible(false);
	}
	else
	{
		m_pServerBrowser->setVisible(true);
	}

	UpdateCursorState();
}

//=======================================================================
// orig client 0xe4a20. Rocket Crowbar 2 has no command menu: +commandmenu does nothing. RC2's
// takes no argument; the index is halflife-updated's, for the callers it still has.
void TeamFortressViewport::ShowCommandMenu(int menuIndex)
{
}

//-----------------------------------------------------------------------------
// Purpose: Handles the key input of "-commandmenu"
// Input  :
//-----------------------------------------------------------------------------
void TeamFortressViewport::InputSignalHideCommandMenu()
{
	if (!m_iInitialized)
		return;

	// if they've just tapped the command menu key, leave it open
	if ((m_flMenuOpenTime + 0.3) > gHUD.m_flTime)
		return;

	HideCommandMenu();
}

//-----------------------------------------------------------------------------
// Purpose: Hides the command menu
//-----------------------------------------------------------------------------
// orig client 0xe4a80
void TeamFortressViewport::HideCommandMenu()
{
	if (!m_iInitialized)
		return;

	if (m_pCommandMenus[0])
	{
		m_pCommandMenus[0]->ClearButtonsOfArmedState();
	}

	m_flMenuOpenTime = 0.0f;
	SetCurrentCommandMenu(NULL);
	UpdateCursorState();
}

//-----------------------------------------------------------------------------
// Purpose: Bring up the scoreboard
//-----------------------------------------------------------------------------
// orig client 0xe4ad0
void TeamFortressViewport::ShowScoreBoard()
{
	if (m_pScoreBoard)
	{
		// No Scoreboard in single-player
		if (gEngfuncs.GetMaxClients() > 1)
		{
			m_pScoreBoard->Open();
			UpdateCursorState();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the scoreboard is up
//-----------------------------------------------------------------------------
// orig client 0xe4b10
bool TeamFortressViewport::IsScoreBoardVisible()
{
	if (m_pScoreBoard)
		return m_pScoreBoard->isVisible();

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Hide the scoreboard
//-----------------------------------------------------------------------------
// orig client 0xe4b40
void TeamFortressViewport::HideScoreBoard()
{
	// Prevent removal of scoreboard during intermission
	if (gHUD.m_iIntermission)
		return;

	if (m_pScoreBoard)
	{
		m_pScoreBoard->setVisible(false);

		GetClientVoiceMgr()->StopSquelchMode();

		UpdateCursorState();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Activate's the player special ability
//			called when the player hits their "special" key
//-----------------------------------------------------------------------------
// orig client 0xe4b90
void TeamFortressViewport::InputPlayerSpecial()
{
	if (!m_iInitialized)
		return;

	// just send the command down to the server
	EngineClientCmd("_special");
}

// Set the submenu of the Command Menu
// orig client 0xe4bc0
void TeamFortressViewport::SetCurrentCommandMenu(CCommandMenu* pNewMenu)
{
	for (int i = 0; i < m_iNumMenus; i++)
		m_pCommandMenus[i]->setVisible(false);

	m_pCurrentCommandMenu = pNewMenu;

	if (m_pCurrentCommandMenu)
		m_pCurrentCommandMenu->MakeVisible(NULL);
}

// orig client 0xe4c30
void TeamFortressViewport::UpdateCommandMenu()
{
	m_pCommandMenus[0]->RecalculateVisibles(0, false);
	m_pCommandMenus[0]->RecalculatePositions(0);
}

// orig client 0xe4c70. SDK 2.2's spectator "menu": the mode and target in the title, the help
// line below it (different for HLTV spectators), hidden when not spectating or for HLTV's
// proxy (IsSpectateOnly 2).
void TeamFortressViewport::UpdateSpectatorMenu()
{
	char sz[128];
	char szHelp[128];
	int iMode;
	const char* pszHelp;

	m_iUser1 = g_iUser1;
	m_iUser2 = g_iUser2;
	m_iUser3 = g_iUser3;

	if (!m_pSpectatorMenu)
		return;

	if (0 != gEngfuncs.IsSpectateOnly())
	{
		pszHelp = "#Spec_Only_Help";
		iMode = gHUD.m_Spectator.m_iMainMode;
	}
	else
	{
		pszHelp = "#Spec_Help";
		iMode = m_iUser1;
	}

	sprintf(szHelp, pszHelp);

	if (0 != iMode && gEngfuncs.IsSpectateOnly() != 2)
	{
		m_pSpectatorMenu->setVisible(true);

		if (m_iUser2 > 0 && m_iUser2 <= gEngfuncs.GetMaxClients() && g_PlayerInfoList[m_iUser2].name)
		{
			// Locked onto a target, show the player's name
			sprintf(sz, "#Spec_Mode%d : %s", iMode, g_PlayerInfoList[m_iUser2].name);
		}
		else
		{
			sprintf(sz, "#Spec_Mode%d", iMode);
		}

		if (m_iUser1 == 4)
		{
			char szTemp[128];
			sprintf(szTemp, "#Directed %s", sz);
			strcpy(sz, szTemp);
		}

		// Original bug (crash), guarded: the original passes the text, with the target's name in
		// it, as setText's printf format
		m_pSpectatorLabel->setText("%s", CHudTextMessage::BufferedLocaliseTextString(sz));
		m_pSpectatorHelpLabel->setText(CHudTextMessage::BufferedLocaliseTextString(szHelp));
	}
	else
	{
		m_pSpectatorMenu->setVisible(false);
	}
}

void COM_FileBase(const char* in, char* out);

void TeamFortressViewport::UpdateSpectatorPanel()
{
	m_iUser1 = g_iUser1;
	m_iUser2 = g_iUser2;
	m_iUser3 = g_iUser3;

	if (!m_pSpectatorPanel)
		return;

	if (0 != g_iUser1 && 0 != gHUD.m_pCvarDraw->value && !gHUD.m_iIntermission) // don't draw in dev_overview mode
	{
		char bottomText[128];
		char helpString2[128];
		char tempString[128];
		char* name;
		char* pBottomText = NULL;
		int player = 0;

		// check if spectator combinations are still valid
		gHUD.m_Spectator.CheckSettings();

		if (!m_pSpectatorPanel->isVisible())
		{
			m_pSpectatorPanel->setVisible(true); // show spectator panel, but
			m_pSpectatorPanel->ShowMenu(false);	 // dsiable all menus/buttons

			snprintf(tempString, sizeof(tempString) - 1, "%c%s", HUD_PRINTCENTER, CHudTextMessage::BufferedLocaliseTextString("#Spec_Duck"));
			tempString[sizeof(tempString) - 1] = '\0';

			gHUD.m_TextMessage.MsgFunc_TextMsg(NULL, strlen(tempString) + 1, tempString);
		}

		sprintf(bottomText, "#Spec_Mode%d", g_iUser1);
		sprintf(helpString2, "#Spec_Mode%d", g_iUser1);

		if (0 != gEngfuncs.IsSpectateOnly())
			strcat(helpString2, " - HLTV");

		// check if we're locked onto a target, show the player's name
		if ((g_iUser2 > 0) && (g_iUser2 <= gEngfuncs.GetMaxClients()) && (g_iUser1 != OBS_ROAMING))
		{
			player = g_iUser2;
		}

		// special case in free map and inset off, don't show names
		if ((g_iUser1 == OBS_MAP_FREE) && 0 == gHUD.m_Spectator.m_pip->value)
			name = NULL;
		else
			name = g_PlayerInfoList[player].name;

		// create player & health string
		if (0 != player && name)
		{
			strncpy(bottomText, name, sizeof(bottomText));
			bottomText[sizeof(bottomText) - 1] = 0;
			pBottomText = bottomText;
		}
		else
		{
			pBottomText = CHudTextMessage::BufferedLocaliseTextString(bottomText);
		}

		// in first person mode colorize player names
		if ((g_iUser1 == OBS_IN_EYE) && 0 != player)
		{
			float* color = GetClientColor(player);
			int r = color[0] * 255;
			int g = color[1] * 255;
			int b = color[2] * 255;

			// set team color, a bit transparent
			m_pSpectatorPanel->m_BottomMainLabel->setFgColor(r, g, b, 0);
			m_pSpectatorPanel->m_BottomMainButton->setFgColor(r, g, b, 0);
		}
		else
		{ // restore GUI color
			m_pSpectatorPanel->m_BottomMainLabel->setFgColor(143, 143, 54, 0);
			m_pSpectatorPanel->m_BottomMainButton->setFgColor(143, 143, 54, 0);
		}

		// add sting auto if we are in auto directed mode
		if (0 != gHUD.m_Spectator.m_autoDirector->value)
		{
			char tempString[128];
			sprintf(tempString, "#Spec_Auto %s", helpString2);
			strcpy(helpString2, tempString);
		}

		m_pSpectatorPanel->m_BottomMainLabel->setText("%s", pBottomText);
		m_pSpectatorPanel->m_BottomMainButton->setText(pBottomText);


		// update extra info field
		char szText[64];

		if (0 != gEngfuncs.IsSpectateOnly())
		{
			// in HLTV mode show number of spectators
			snprintf(szText, 63, "%s: %d", CHudTextMessage::BufferedLocaliseTextString("#Spectators"), gHUD.m_Spectator.m_iSpectatorNumber);
		}
		else
		{
			// otherwise show map name
			char szMapName[64];
			COM_FileBase(gEngfuncs.pfnGetLevelName(), szMapName);

			snprintf(szText, 63, "%s: %s", CHudTextMessage::BufferedLocaliseTextString("#Spec_Map"), szMapName);
		}

		szText[63] = 0;

		// Original bug (crash), guarded: the original passes the text, the server's map name
		// in it, as setText's printf format
		m_pSpectatorPanel->m_ExtraInfo->setText("%s", szText);

		/*
		int timer = (int)( gHUD.m_roundTimer.m_flTimeEnd - gHUD.m_flTime );

		if ( timer < 0 )
			 timer	= 0;

		snprintf ( szText, 63, "%d:%02d\n", (timer / 60), (timer % 60) );
		
		szText[63] = 0;
				
		m_pSpectatorPanel->m_CurrentTime->setText( szText ); */

		// update spectator panel
		gViewPort->m_pSpectatorPanel->Update();
	}
	else
	{
		if (m_pSpectatorPanel->isVisible())
		{
			m_pSpectatorPanel->setVisible(false);
			m_pSpectatorPanel->ShowMenu(false); // dsiable all menus/buttons
		}
	}

	m_flSpectatorPanelLastUpdated = gHUD.m_flTime + 1.0; // update every second
}

//======================================================================
// orig client 0xe4e90
void TeamFortressViewport::CreateScoreBoard()
{
	int xdent = SBOARD_INDENT_X, ydent = SBOARD_INDENT_Y;
	if (ScreenWidth == 512)
	{
		xdent = SBOARD_INDENT_X_512;
		ydent = SBOARD_INDENT_Y_512;
	}
	else if (ScreenWidth == 400)
	{
		xdent = SBOARD_INDENT_X_400;
		ydent = SBOARD_INDENT_Y_400;
	}

	m_pScoreBoard = new ScorePanel(xdent, ydent, ScreenWidth - (xdent * 2), ScreenHeight - (ydent * 2));
	m_pScoreBoard->setParent(this);
	m_pScoreBoard->setVisible(false);
}

// orig client 0xe4fd0
void TeamFortressViewport::CreateServerBrowser()
{
	m_pServerBrowser = new ServerBrowser(0, 0, ScreenWidth, ScreenHeight);
	m_pServerBrowser->setParent(this);
	m_pServerBrowser->setVisible(false);
}

//======================================================================
// Set the VGUI Menu
// orig client 0xe5090
void TeamFortressViewport::SetCurrentMenu(CMenuPanel* pMenu)
{
	m_pCurrentMenu = pMenu;
	if (m_pCurrentMenu)
	{
		// Don't open menus in demo playback
		if (0 != gEngfuncs.pDemoAPI->IsPlayingback())
			return;

		m_pCurrentMenu->Open();
	}
	else
	{
		gEngfuncs.pfnClientCmd("closemenus;");
	}
}

//================================================================
// Text Window
// orig client 0xe50d0. SDK 2.3's MOTD and map briefing windows, always shaded full screen. RC2
// has no class or spectator help text; asked for anything else it passes an uninitialised title
// and the viewport as the text (nothing does).
CMenuPanel* TeamFortressViewport::CreateTextWindow(int iTextToShow)
{
	char sz[256];
	const char* cText = "";
	char* pfile = NULL;
	static const int MAX_TITLE_LENGTH = 32;
	char cTitle[MAX_TITLE_LENGTH];

	if (iTextToShow == SHOW_MOTD)
	{
		if (!m_szServerName || '\0' == m_szServerName[0])
			strcpy(cTitle, "Half-Life");
		else
			strncpy(cTitle, m_szServerName, MAX_TITLE_LENGTH);
		cTitle[MAX_TITLE_LENGTH - 1] = 0;
		cText = m_szMOTD;
	}
	else if (iTextToShow == SHOW_MAPBRIEFING)
	{
		// Get the current mapname, and open it's map briefing text
		if (m_sMapName && '\0' != m_sMapName[0])
		{
			strcpy(sz, "maps/");
			strcat(sz, m_sMapName);
			strcat(sz, ".txt");
		}
		else
		{
			const char* level = gEngfuncs.pfnGetLevelName();
			if (!level)
				return NULL;

			strcpy(sz, level);
			char* ch = strchr(sz, '.');
			*ch = '\0';
			strcat(sz, ".txt");

			// pull out the map name
			strcpy(m_sMapName, level);
			ch = strchr(m_sMapName, '.');
			if (ch)
			{
				*ch = 0;
			}

			ch = strchr(m_sMapName, '/');
			if (ch)
			{
				// move the string back over the '/'
				memmove(m_sMapName, ch + 1, strlen(ch) + 1);
			}
		}

		pfile = (char*)gEngfuncs.COM_LoadFile(sz, 5, NULL);

		if (!pfile)
			return NULL;

		cText = pfile;

		strncpy(cTitle, m_sMapName, MAX_TITLE_LENGTH);
		cTitle[MAX_TITLE_LENGTH - 1] = 0;
	}
	else
	{
		// VERIFY: the original's text is the viewport itself and its title is never written;
		// nothing asks for another window, so an empty one stands in
		cTitle[0] = 0;
	}

	CMenuPanel* pMOTDPanel = CMessageWindowPanel_Create(cText, cTitle, true, false, 0, 0, ScreenWidth, ScreenHeight);
	pMOTDPanel->setParent(this);

	if (pfile)
		gEngfuncs.COM_FreeFile(pfile);

	return pMOTDPanel;
}

//================================================================
// VGUI Menus
// orig client 0xe5410. RC2 opens only the MOTD (5), its attribute menu (9, AttrConf) and its vote
// menu (10); the others (team, map briefing) are ignored. A menu queued behind one that isn't
// the class menu returns without updating the cursor.
void TeamFortressViewport::ShowVGUIMenu(int iMenu)
{
	CMenuPanel* pNewMenu = NULL;

	// Don't open menus in demo playback
	if (0 != gEngfuncs.pDemoAPI->IsPlayingback())
		return;

	// Don't open any menus except the MOTD and the attribute menu during intermission
	if (gHUD.m_iIntermission && iMenu != MENU_INTRO && iMenu != 9)
		return;

	// Don't create one if it's already in the list
	if (m_pCurrentMenu)
	{
		CMenuPanel* pMenu = m_pCurrentMenu;
		while (pMenu != NULL)
		{
			if (pMenu->GetMenuID() == iMenu)
				return;
			pMenu = pMenu->GetNextMenu();
		}
	}

	switch (iMenu)
	{
	case MENU_INTRO:
		pNewMenu = CreateTextWindow(SHOW_MOTD);
		break;

	case 9:
		pNewMenu = AttrMenu_Create(this);
		break;

	case 10:
		if (!m_pVoteMenu)
		{
			m_pVoteMenu = new CVoteMenuPanel();
			m_pVoteMenu->setParent(this);
			m_pVoteMenu->setVisible(false);
		}
		pNewMenu = m_pVoteMenu;
		break;

	default:
		return;
	}

	if (!pNewMenu)
		return;

	// Close the Command Menu if it's open
	HideCommandMenu();

	pNewMenu->SetMenuID(iMenu);
	pNewMenu->SetActive(true);
	pNewMenu->setParent(this);

	// See if another menu is visible, and if so, cache this one for display once the other one's finished
	if (m_pCurrentMenu)
	{
		if (m_pCurrentMenu->GetMenuID() == MENU_CLASS)
		{
			CMenuPanel* temp = m_pCurrentMenu;
			m_pCurrentMenu->Close();
			m_pCurrentMenu = pNewMenu;
			m_pCurrentMenu->SetNextMenu(temp);
			m_pCurrentMenu->Open();
		}
		else
		{
			m_pCurrentMenu->SetNextMenu(pNewMenu);
			return;
		}
	}
	else
	{
		m_pCurrentMenu = pNewMenu;
		m_pCurrentMenu->Open();
	}

	UpdateCursorState();
}

// Removes all VGUI Menu's onscreen
// orig client 0xe5660
void TeamFortressViewport::HideVGUIMenu()
{
	while (m_pCurrentMenu)
	{
		HideTopMenu();
	}
}

// Remove the top VGUI menu, and bring up the next one
// orig client 0xe5690. The next menu is read before Close, not after.
void TeamFortressViewport::HideTopMenu()
{
	if (m_pCurrentMenu)
	{
		CMenuPanel* pNext = m_pCurrentMenu->GetNextMenu();

		// Close the top one
		m_pCurrentMenu->Close();

		// Bring up the next one
		SetCurrentMenu(pNext);
	}

	UpdateCursorState();
}

// Return true if the HUD's allowed to print text messages
// orig client 0xe56e0. RC2 has no player class, so the full screen menus always block them.
bool TeamFortressViewport::AllowedToPrintText()
{
	// Prevent text messages when fullscreen menus are up
	if (m_pCurrentMenu)
	{
		int iId = m_pCurrentMenu->GetMenuID();
		if (iId == MENU_TEAM || iId == MENU_CLASS || iId == MENU_INTRO || iId == MENU_CLASSHELP)
			return false;
	}

	return true;
}

//======================================================================================
// TEAM MENU
//======================================================================================
// orig client 0xe5760
void TeamFortressViewport::CreateTeamMenu()
{
	// Create the panel
	m_pTeamMenu = new CTeamMenuPanel(100, false, 0, 0, ScreenWidth, ScreenHeight);
	m_pTeamMenu->setParent(this);
	m_pTeamMenu->setVisible(false);
}

//======================================================================================
//======================================================================================
// SPECTATOR MENU
//======================================================================================
// Spectator "Menu" explaining the Spectator buttons
// orig client 0xe5820. SDK 2.1's.
void TeamFortressViewport::CreateSpectatorMenu()
{
	// Create the Panel
	m_pSpectatorMenu = new CTransparentPanel(100, 0, ScreenHeight - YRES(60), ScreenWidth, YRES(60));
	m_pSpectatorMenu->setParent(this);
	m_pSpectatorMenu->setVisible(false);

	// Get the scheme used for the Titles
	CSchemeManager* pSchemes = gViewPort->GetSchemeManager();

	// schemes
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle("Title Font");
	SchemeHandle_t hHelpText = pSchemes->getSchemeHandle("Primary Button Text");

	// color schemes
	int r, g, b, a;

	// Create the title
	m_pSpectatorLabel = new Label("Spectator", 0, 0, ScreenWidth, YRES(25));
	m_pSpectatorLabel->setParent(m_pSpectatorMenu);
	m_pSpectatorLabel->setFont(pSchemes->getFont(hTitleScheme));
	pSchemes->getFgColor(hTitleScheme, r, g, b, a);
	m_pSpectatorLabel->setFgColor(r, g, b, a);
	pSchemes->getBgColor(hTitleScheme, r, g, b, a);
	m_pSpectatorLabel->setBgColor(r, g, b, 255);
	m_pSpectatorLabel->setContentAlignment(vgui::Label::a_north);

	// Create the Help
	m_pSpectatorHelpLabel = new Label(CHudTextMessage::BufferedLocaliseTextString("#Spec_Help"), 0, YRES(25), ScreenWidth, YRES(15));
	m_pSpectatorHelpLabel->setParent(m_pSpectatorMenu);
	m_pSpectatorHelpLabel->setFont(pSchemes->getFont(hHelpText));
	pSchemes->getFgColor(hHelpText, r, g, b, a);
	m_pSpectatorHelpLabel->setFgColor(r, g, b, a);
	pSchemes->getBgColor(hHelpText, r, g, b, a);
	m_pSpectatorHelpLabel->setBgColor(r, g, b, 255);
	m_pSpectatorHelpLabel->setContentAlignment(vgui::Label::a_north);

	Label* pLabel = new Label(CHudTextMessage::BufferedLocaliseTextString("#Spec_Help2"), 0, YRES(40), ScreenWidth, YRES(20));
	pLabel->setParent(m_pSpectatorMenu);
	pLabel->setFont(pSchemes->getFont(hHelpText));
	pSchemes->getFgColor(hHelpText, r, g, b, a);
	pLabel->setFgColor(r, g, b, a);
	pSchemes->getBgColor(hHelpText, r, g, b, a);
	pLabel->setBgColor(r, g, b, 255);
	pLabel->setContentAlignment(vgui::Label::a_center);
}

//======================================================================================
// UPDATE HUD SECTIONS
//======================================================================================
// We've got an update on player info
// Recalculate any menus that use it.
// orig client 0xe5da0
void TeamFortressViewport::UpdateOnPlayerInfo()
{
	if (m_pTeamMenu)
		m_pTeamMenu->Update();
	if (m_pScoreBoard)
		m_pScoreBoard->Update();
}

// orig client 0xe5de0. SDK 2.3's with the server browser and without the spectator panel; the
// demo and relative mouse handling are halflife-updated's.
void TeamFortressViewport::UpdateCursorState()
{
	// Need cursor if any VGUI window is up
	if (m_pCurrentMenu || m_pTeamMenu->isVisible() || m_pServerBrowser->isVisible() || GetClientVoiceMgr()->IsInSquelchMode())
	{
		g_iVisibleMouse = true;
		App::getInstance()->setCursorOveride(App::getInstance()->getScheme()->getCursor(Scheme::scu_arrow));
		return;
	}
	else if (m_pCurrentCommandMenu)
	{
		// commandmenu doesn't have cursor if hud_capturemouse is turned off
		if (gHUD.m_pCvarStealMouse->value != 0.0f)
		{
			g_iVisibleMouse = true;
			App::getInstance()->setCursorOveride(App::getInstance()->getScheme()->getCursor(Scheme::scu_arrow));
			return;
		}
	}

	// Don't reset mouse in demo playback
	if (0 == gEngfuncs.pDemoAPI->IsPlayingback())
	{
		IN_ResetMouse();
	}

	if (g_iVisibleMouse)
	{
		// Clear any residual input so our camera doesn't jerk when dismissing the UI
		IN_ResetRelativeMouseState();
	}

	g_iVisibleMouse = false;
	App::getInstance()->setCursorOveride(App::getInstance()->getScheme()->getCursor(Scheme::scu_none));
}

void TeamFortressViewport::UpdateHighlights()
{
	if (m_pCurrentCommandMenu)
		m_pCurrentCommandMenu->MakeVisible(NULL);
}

// orig client 0xe5f30
void TeamFortressViewport::GetAllPlayersInfo()
{
	for (int i = 1; i < MAX_PLAYERS_HUD; i++)
	{
		gEngfuncs.pfnGetPlayerInfo(i, &g_PlayerInfoList[i]);

		if (0 != g_PlayerInfoList[i].thisplayer)
			m_pScoreBoard->m_iPlayerNum = i; // !!!HACK: this should be initialized elsewhere... maybe gotten from the engine
	}
}

// orig client 0xe5f80. SDK 2.1's order, without the class check and with SDK 2.2's spectator
// test; it doesn't resize the viewport to its parent.
void TeamFortressViewport::paintBackground()
{
	if (m_pScoreBoard)
	{
		int x, y;
		getApp()->getCursorPos(x, y);
		m_pScoreBoard->cursorMoved(x, y, m_pScoreBoard);
	}

	// See if the command menu is visible and needs recalculating due to some external change
	if (g_iTeamNumber != m_iCurrentTeamNumber)
	{
		UpdateCommandMenu();

		m_iCurrentTeamNumber = g_iTeamNumber;
	}

	// See if the Spectator Menu needs to be update
	if (0 != g_iUser1 || 0 != m_iUser1 || g_iUser2 != m_iUser2)
	{
		UpdateSpectatorMenu();
	}

	// Update the Scoreboard, if it's visible
	if (m_pScoreBoard->isVisible() && (m_flScoreBoardLastUpdated < gHUD.m_flTime))
	{
		m_pScoreBoard->Update();
		m_flScoreBoardLastUpdated = gHUD.m_flTime + 0.5;
	}

	int extents[4];
	getAbsExtents(extents[0], extents[1], extents[2], extents[3]);
	VGui_ViewportPaintBackground(extents);
}

//================================================================
// Input Handler for Drag N Drop panels
// orig client 0xe60c0
void CDragNDropHandler::cursorMoved(int x, int y, Panel* panel)
{
	if (m_bDragging)
	{
		App::getInstance()->getCursorPos(x, y);
		m_pPanel->setPos(m_iaDragOrgPos[0] + (x - m_iaDragStart[0]), m_iaDragOrgPos[1] + (y - m_iaDragStart[1]));

		if (m_pPanel->getParent() != null)
		{
			m_pPanel->getParent()->repaint();
		}
	}
}

// orig client 0xe6150
void CDragNDropHandler::mousePressed(MouseCode code, Panel* panel)
{
	int x, y;
	App::getInstance()->getCursorPos(x, y);
	m_bDragging = true;
	m_iaDragStart[0] = x;
	m_iaDragStart[1] = y;
	m_pPanel->getPos(m_iaDragOrgPos[0], m_iaDragOrgPos[1]);
	App::getInstance()->setMouseCapture(panel);

	m_pPanel->setDragged(m_bDragging);
	m_pPanel->requestFocus();
}

void CDragNDropHandler::mouseReleased(MouseCode code, Panel* panel)
{
	m_bDragging = false;
	m_pPanel->setDragged(m_bDragging);
	App::getInstance()->setMouseCapture(null);
}

//================================================================
// Number Key Input
// orig client 0xe6220
bool TeamFortressViewport::SlotInput(int iSlot)
{
	// If there's a menu up, give it the input
	if (m_pCurrentMenu)
		return m_pCurrentMenu->SlotInput(iSlot);

	return false;
}

// Direct Key Input
// orig client 0xe6250. SDK 2.3's without the spectator Enter and the class menu's Escape.
bool TeamFortressViewport::KeyInput(bool down, int keynum, const char* pszCurrentBinding)
{
	// Open Text Window?
	if (m_pCurrentMenu && 0 == gEngfuncs.Con_IsVisible())
	{
		int iMenuID = m_pCurrentMenu->GetMenuID();

		// Get number keys as Input for Team/Class menus
		if (iMenuID == MENU_TEAM || iMenuID == MENU_CLASS)
		{
			// Escape gets you out of the Team menu if you have a team
			if (keynum == K_ESCAPE && iMenuID == MENU_TEAM && 0 != g_iTeamNumber)
			{
				HideTopMenu();
				return false;
			}

			for (int i = '0'; i <= '9'; i++)
			{
				if (down && (keynum == i))
				{
					SlotInput(i - '0');
					return false;
				}
			}
		}

		// Grab enter keys to close TextWindows
		if (down && (keynum == K_ENTER || keynum == K_KP_ENTER || keynum == K_SPACE || keynum == K_ESCAPE))
		{
			if (iMenuID == MENU_MAPBRIEFING || iMenuID == MENU_INTRO || iMenuID == MENU_CLASSHELP)
			{
				HideTopMenu();
				return false;
			}
		}

		// Grab jump key on Team Menu as autoassign
		if (pszCurrentBinding)
		{
			if (!down)
				return true;

			if (0 == strcmp(pszCurrentBinding, "+jump") && iMenuID == MENU_TEAM)
			{
				m_pTeamMenu->SlotInput(5);
				return false;
			}
		}
	}

	// if we're in a command menu, try hit one of it's buttons
	if (down && m_pCurrentCommandMenu)
	{
		// Escape hides the command menu
		if (keynum == K_ESCAPE)
		{
			HideCommandMenu();
			return false;
		}

		// only trap the number keys
		if (keynum >= '0' && keynum <= '9')
		{
			if (m_pCurrentCommandMenu->KeyInput(keynum))
			{
				// a final command has been issued, so close the command menu
				HideCommandMenu();
			}

			return false;
		}
	}

	return true;
}

//================================================================
// Message Handlers
bool TeamFortressViewport::MsgFunc_ValClass(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	for (int i = 0; i < 5; i++)
		m_iValidClasses[i] = READ_SHORT();

	// Force the menu to update
	UpdateCommandMenu();

	return true;
}

// orig client 0xe6430
bool TeamFortressViewport::MsgFunc_TeamNames(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iNumberOfTeams = READ_BYTE();

	for (int i = 0; i < m_iNumberOfTeams; i++)
	{
		int teamNum = i + 1;

		gHUD.m_TextMessage.LocaliseTextString(READ_STRING(), m_sTeamNames[teamNum], MAX_TEAMNAME_SIZE);

		// Set the team name buttons
		if (m_pTeamButtons[i])
			m_pTeamButtons[i]->setText(m_sTeamNames[teamNum]);

		// range check this value...m_pDisguiseButtons[5];
		if (teamNum < 5)
		{
			// Set the disguise buttons
			if (m_pDisguiseButtons[teamNum])
				m_pDisguiseButtons[teamNum]->setText(m_sTeamNames[teamNum]);
		}
	}

	// Update the Team Menu
	if (m_pTeamMenu)
		m_pTeamMenu->Update();

	return true;
}

bool TeamFortressViewport::MsgFunc_Feign(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iIsFeigning = READ_BYTE() != 0;

	// Force the menu to update
	UpdateCommandMenu();

	return true;
}

bool TeamFortressViewport::MsgFunc_Detpack(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iIsSettingDetpack = READ_BYTE();

	// Force the menu to update
	UpdateCommandMenu();

	return true;
}

// orig client 0xe6500
bool TeamFortressViewport::MsgFunc_VGUIMenu(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int iMenu = READ_BYTE();

	// Map briefing includes the name of the map (because it's sent down before the client knows what map it is)
	if (iMenu == MENU_MAPBRIEFING)
	{
		strncpy(m_sMapName, READ_STRING(), sizeof(m_sMapName));
		m_sMapName[sizeof(m_sMapName) - 1] = '\0';
	}

	// Bring up the menu6
	ShowVGUIMenu(iMenu);

	return true;
}

// orig client 0xe6580. SDK 2.3's without the HLTV test: spectators see the MOTD too.
bool TeamFortressViewport::MsgFunc_MOTD(const char* pszName, int iSize, void* pbuf)
{
	if (m_iGotAllMOTD)
		m_szMOTD[0] = 0;

	BEGIN_READ(pbuf, iSize);

	m_iGotAllMOTD = READ_BYTE() != 0;

	int roomInArray = sizeof(m_szMOTD) - strlen(m_szMOTD) - 1;

	strncat(m_szMOTD, READ_STRING(), roomInArray >= 0 ? roomInArray : 0);
	m_szMOTD[sizeof(m_szMOTD) - 1] = '\0';

	if (m_iGotAllMOTD)
	{
		ShowVGUIMenu(MENU_INTRO);
	}

	return true;
}

bool TeamFortressViewport::MsgFunc_BuildSt(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iBuildState = READ_SHORT();

	// Force the menu to update
	UpdateCommandMenu();

	return true;
}

bool TeamFortressViewport::MsgFunc_RandomPC(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iRandomPC = READ_BYTE() != 0;

	return true;
}

// orig client 0xe6640
bool TeamFortressViewport::MsgFunc_ServerName(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	strncpy(m_szServerName, READ_STRING(), sizeof(m_szServerName));
	m_szServerName[sizeof(m_szServerName) - 1] = 0;

	return true;
}

// orig client 0xe6690. RC2's ScoreInfo (6 bytes): the client byte, then frags and deaths as shorts,
// or with bit 7 of the client byte set a float damage score sent as a long (player.cpp), then
// the team byte. Each message zeroes the fields the other kind carries.
bool TeamFortressViewport::MsgFunc_ScoreInfo(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	int iByte = READ_BYTE();
	short cl = iByte & 0xff7f;
	short frags, deaths;
	int iDmgScore;

	if ((iByte & 0x80) != 0)
	{
		iDmgScore = READ_LONG();
		frags = 0;
		deaths = 0;
	}
	else
	{
		frags = READ_SHORT();
		deaths = READ_SHORT();
		iDmgScore = 0;
	}

	short teamnumber = READ_BYTE();

	if (cl > 0 && cl <= MAX_PLAYERS_HUD)
	{
		g_PlayerExtraInfo[cl].frags = frags;
		g_PlayerExtraInfo[cl].deaths = deaths;
		memcpy(&g_PlayerExtraInfo[cl].dmgscore, &iDmgScore, sizeof(iDmgScore));
		g_PlayerExtraInfo[cl].teamnumber = teamnumber;

		//Dont go bellow 0!
		if (g_PlayerExtraInfo[cl].teamnumber < 0)
			g_PlayerExtraInfo[cl].teamnumber = 0;

		UpdateOnPlayerInfo();
	}

	return true;
}

// Message handler for TeamScore message
// accepts three values:
//		string: team name
//		short: teams kills
//		short: teams deaths
// if this message is never received, then scores will simply be the combined totals of the players.
// orig client 0xe6780
bool TeamFortressViewport::MsgFunc_TeamScore(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	char* TeamName = READ_STRING();

	// find the team matching the name
	int i;
	for (i = 1; i <= m_pScoreBoard->m_iNumTeams; i++)
	{
		if (!stricmp(TeamName, g_TeamInfo[i].name))
			break;
	}

	if (i > m_pScoreBoard->m_iNumTeams)
		return true;

	// use this new score data instead of combined player scoresw
	g_TeamInfo[i].scores_overriden = true;
	g_TeamInfo[i].frags = READ_SHORT();
	g_TeamInfo[i].deaths = READ_SHORT();

	return true;
}

// Message handler for TeamInfo message
// accepts two values:
//		byte: client number
//		string: client team name
// orig client 0xe6850
bool TeamFortressViewport::MsgFunc_TeamInfo(const char* pszName, int iSize, void* pbuf)
{
	if (!m_pScoreBoard)
		return true;

	BEGIN_READ(pbuf, iSize);
	short cl = READ_BYTE();

	if (cl > 0 && cl <= MAX_PLAYERS_HUD)
	{
		// set the players team
		strncpy(g_PlayerExtraInfo[cl].teamname, READ_STRING(), MAX_TEAM_NAME);
	}

	// rebuild the list of teams
	m_pScoreBoard->RebuildTeams();

	return true;
}

// orig client 0xe68f0
void TeamFortressViewport::DeathMsg(int killer, int victim)
{
	m_pScoreBoard->DeathMsg(killer, victim);
}

bool TeamFortressViewport::MsgFunc_Spectator(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	short cl = READ_BYTE();
	if (cl > 0 && cl <= MAX_PLAYERS_HUD)
	{
		g_IsSpectator[cl] = READ_BYTE();
	}

	return true;
}

// orig client 0xe6970
bool TeamFortressViewport::MsgFunc_AllowSpec(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_iAllowSpectators = READ_BYTE() != 0;

	// Force the menu to update
	UpdateCommandMenu();

	// If the team menu is up, update it too
	if (m_pTeamMenu)
		m_pTeamMenu->Update();

	return true;
}

// orig client 0xe69e0. VoteBegin: a byte (open the vote menu) and what the vote is about.
bool TeamFortressViewport::MsgFunc_VoteBegin(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int iOpenMenu = READ_BYTE();
	strncpy(m_szVoteText, READ_STRING(), 255);
	m_szVoteText[255] = 0;

	if (m_pVoteLabel)
		m_pVoteLabel->setText("");

	if (0 != iOpenMenu)
	{
		ShowVGUIMenu(10);
		// Original bug (crash), guarded: in demo playback ShowVGUIMenu returns before creating
		// the menu, and the original dereferences NULL here
		if (m_pVoteMenu)
			m_pVoteMenu->SetVoteText(m_szVoteText);
	}

	return true;
}

// orig client 0xe6a80. VoteUpdate (3 bytes): votes for, votes against, seconds left; 0 seconds
// ends the vote, 255 leaves the time out.
bool TeamFortressViewport::MsgFunc_VoteUpdate(const char* pszName, int iSize, void* pbuf)
{
	char szTime[252];
	char szText[512];

	BEGIN_READ(pbuf, iSize);

	int iFor = READ_BYTE();
	int iAgainst = READ_BYTE();
	int iTime = READ_BYTE();

	if (0 == iTime)
	{
		if (m_pCurrentMenu && m_pCurrentMenu->GetMenuID() == 10)
			HideTopMenu();

		m_pVotePanel->setVisible(false);
		m_pVoteLabel->setText("");
	}
	else
	{
		sprintf(szText, "Vote to %s\n%i For\n%i Against", m_szVoteText, iFor, iAgainst);

		if (iTime < 255)
		{
			sprintf(szTime, "\n%-3i second%s left\n", iTime, (iTime == 1) ? "" : "s");
			strcat(szText, szTime);
		}

		m_pVotePanel->setVisible(true);
		// Original bug (crash), guarded: the original passes the text, the server's vote text in
		// it, as setText's printf format
		m_pVoteLabel->setText("%s", szText);
	}

	return true;
}

// orig client 0xe6c00. AttrConf (no data): the attribute menu.
bool TeamFortressViewport::MsgFunc_AttrConf(const char* pszName, int iSize, void* pbuf)
{
	ShowVGUIMenu(9);
	return true;
}

// used to reset the player's screen immediately
bool TeamFortressViewport::MsgFunc_ResetFade(const char* pszName, int iSize, void* pbuf)
{

	return true;
}

// used to fade a player's screen out/in when they're spectating someone who is teleported
bool TeamFortressViewport::MsgFunc_SpecFade(const char* pszName, int iSize, void* pbuf)
{

	return true;
}

// hud.cpp's VGUIDebugOn handler (client 0xa1130): shows or hides the panel, and turning it off
// forgets the watched entity
void TeamFortressViewport::SetDebugPanelVisible(bool bVisible)
{
	if (m_pDebugPanel)
		m_pDebugPanel->setVisible(bVisible);
	if (!bVisible)
		g_iDebugEntity = 0;
}

// hud.cpp's DBGCounts and DBGEntInfo handlers (client 0xa11a0, 0xa11e0) pass straight on
bool TeamFortressViewport::MsgFunc_DBGCounts(const char* pszName, int iSize, void* pbuf)
{
	return m_pDebugPanel->MsgFunc_DBGCounts(pszName, iSize, pbuf);
}

bool TeamFortressViewport::MsgFunc_DBGEntInfo(const char* pszName, int iSize, void* pbuf)
{
	return m_pDebugPanel->MsgFunc_DBGEntInfo(pszName, iSize, pbuf);
}

// CHud::MsgFunc_InitHUD hides the debug panel and the vote box
void TeamFortressViewport::HideDebugPanels()
{
	if (m_pDebugPanel)
		m_pDebugPanel->setVisible(false);
	if (m_pVotePanel)
		m_pVotePanel->setVisible(false);
}

