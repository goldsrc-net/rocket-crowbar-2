/***
*
*	Rocket Crowbar 2 (client): CToggleImageButton. See vgui_togglebutton.h. All names ours.
*
****/

#include <stdarg.h>
#include <stdio.h>

#include "hud.h"
#include "cl_util.h"
#include "vgui_loadtga.h"
#include "rc2/vgui_togglebutton.h"

using namespace vgui;

// orig client 0xd1c90
CToggleImageButton::CToggleImageButton() : m_Label(""), m_Image(nullptr)
{
	m_pHandler = nullptr;
	m_pUnchecked = nullptr;
	m_pChecked = nullptr;
	m_bOwnImages = false;
	m_bChecked = false;
	m_bImageFirst = true;

	m_Label.setParent(this);
	m_Label.setFgColor(255, 255, 255, 0);
	m_Label.setBgColor(0, 0, 0, 255);
	m_Label.addInputSignal(this);
	m_Image.setParent(this);
	m_Image.addInputSignal(this);
	setPaintBackgroundEnabled(false);
}

// orig client 0xd1dc0
CToggleImageButton::~CToggleImageButton()
{
	clearImages();
}

// orig client 0xd1df0
void CToggleImageButton::setImagesTGA(const char* pszChecked, const char* pszUnchecked)
{
	clearImages();
	m_pChecked = vgui_LoadTGA(pszChecked);
	m_pUnchecked = vgui_LoadTGA(pszUnchecked);
	m_bOwnImages = true;
	layout();
}

// orig client 0xd1e40
void CToggleImageButton::setImages(Image* pChecked, Image* pUnchecked)
{
	clearImages();
	m_pUnchecked = pUnchecked;
	m_pChecked = pChecked;
	m_bOwnImages = false;
	layout();
}

// orig client 0xd1e80
void CToggleImageButton::clearImages()
{
	if (m_bOwnImages)
	{
		delete m_pChecked;
		delete m_pUnchecked;
	}
	m_pChecked = nullptr;
	m_pUnchecked = nullptr;
	m_bOwnImages = false;
	layout();
}

// orig client 0xd1ef0
void CToggleImageButton::setImageFirst(bool bImageFirst)
{
	m_bImageFirst = bImageFirst;
	layout();
}

// orig client 0xd1f40
void CToggleImageButton::setText(const char* pszFormat, ...)
{
	char sz[512];
	va_list args;
	va_start(args, pszFormat);
	vsnprintf(sz, sizeof(sz), pszFormat, args);
	va_end(args);
	m_Label.setText(sz);
	layout();
}

// orig client 0xd2010
void CToggleImageButton::setHandler(CToggleImageButtonHandler* pHandler)
{
	m_pHandler = pHandler;
}

// orig client 0xd2030
bool CToggleImageButton::isChecked()
{
	return m_bChecked;
}

// orig client 0xd2050
void CToggleImageButton::setChecked(bool bChecked)
{
	m_bChecked = bChecked;
	layout();
}

// orig client 0xd2080
void CToggleImageButton::internalMousePressed(MouseCode code)
{
	m_bChecked = !m_bChecked;
	if (m_pHandler)
		m_pHandler->onToggle();
	layout();
}

// The image and the label side by side, each centred on the other's height.
// orig client 0xd20c0
void CToggleImageButton::layout()
{
	Image* pImage = m_bChecked ? m_pChecked : m_pUnchecked;
	Panel* pPanels[2] = {&m_Image, &m_Label};
	int iSize[2][2] = {{0, 0}, {0, 0}}; // wide, tall of each

	if (pImage)
		pImage->getSize(iSize[0][0], iSize[0][1]);
	m_Image.setImage(pImage);
	m_Image.setSize(iSize[0][0], iSize[0][1]);
	m_Label.getSize(iSize[1][0], iSize[1][1]);
	m_Label.setContentAlignment(Label::a_west);

	const int iFirst = m_bImageFirst ? 0 : 1;
	// VERIFY: as the original, the image's height against the label's width picks the taller
	const int iTaller = (iSize[0][1] <= iSize[1][0]) ? 1 : 0;
	pPanels[iFirst]->setPos(0, (iSize[iTaller][1] - iSize[iFirst][1]) / 2);
	const int iSecond = (iFirst == 0) ? 1 : 0;
	pPanels[iSecond]->setPos(iSize[iFirst][0] + 5, (iSize[iTaller][1] - iSize[iSecond][1]) / 2);

	setSize(iSize[1][0] + iSize[0][0] + 5, (iSize[0][1] > iSize[1][1]) ? iSize[0][1] : iSize[1][1]);
	repaint();
}

// A click on the label or the image is a click on the button.
// orig client 0xd2240
void CToggleImageButton::mousePressed(MouseCode code, Panel* panel)
{
	internalMousePressed(code);
}
