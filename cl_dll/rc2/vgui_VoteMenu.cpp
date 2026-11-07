// Rocket Crowbar 2: the vote menu, a small box in the middle of the screen with what the vote
// is about and Yes (1, "vote yes") and No (2, "vote no") buttons. Names ours.

#include "hud.h"
#include "cl_util.h"
#include <VGUI_LineBorder.h>

#include "vgui_TeamFortressViewport.h"
#include "rc2/vgui_VoteMenu.h"

#define VOTEMENU_WIDE (XRES(160) + XRES(20))
#define VOTEMENU_TALL (YRES(45) + YRES(30) * 2)

// orig client 0xe8c50. Its border is set twice, the same each time.
CVoteMenuPanel::CVoteMenuPanel() : CMenuPanel(100, false, XRES(320) - VOTEMENU_WIDE / 2, YRES(240) - VOTEMENU_TALL / 2, VOTEMENU_WIDE, VOTEMENU_TALL)
{
	setBorder(new LineBorder(Color(255 * 0.7, 170 * 0.7, 0, 0)));

	int iButtonY = YRES(30);
	CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hBriefingText = pSchemes->getSchemeHandle("Briefing Text");

	setBorder(new LineBorder(Color(255 * 0.7, 170 * 0.7, 0, 0)));

	int r, g, b, a;

	// VERIFY: its width is YRES(195), not XRES
	m_pLabel = new Label("", 1, 1, YRES(195), YRES(25));
	m_pLabel->setParent(this);
	m_pLabel->setFont(pSchemes->getFont(hBriefingText));
	m_pLabel->setFont(Scheme::sf_primary1);
	pSchemes->getFgColor(hBriefingText, r, g, b, a);
	m_pLabel->setFgColor(r, g, b, a);
	m_pLabel->setFgColor(Scheme::sc_primary1);
	pSchemes->getBgColor(hBriefingText, r, g, b, a);
	m_pLabel->setBgColor(r, g, b, a);
	m_pLabel->setContentAlignment(vgui::Label::a_north);
	m_pLabel->setText("No vote");

	CommandButton* pButton = new CommandButton("Yes", XRES(10), iButtonY, XRES(160), YRES(30));
	pButton->addActionSignal(new CMenuHandler_StringCommand("vote yes"));
	pButton->setParent(this);
	pButton->setBoundKey('1');

	iButtonY = iButtonY + YRES(30) + YRES(5);
	pButton = new CommandButton("No", XRES(10), iButtonY, XRES(160), YRES(30));
	pButton->addActionSignal(new CMenuHandler_StringCommand("vote no"));
	pButton->setParent(this);
	pButton->setBoundKey('2');
}

// orig client 0xe9350
void CVoteMenuPanel::SetVoteText(const char* pszText)
{
	// Original bug (crash), guarded: the original passes the server's vote text (map and player
	// names in it) as setText's printf format
	if (m_pLabel)
		m_pLabel->setText("%s", pszText);
}
