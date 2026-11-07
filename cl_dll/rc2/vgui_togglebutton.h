/***
*
*	Rocket Crowbar 2 (client): a check button made of an image (one per state) and a label;
*	a click anywhere on it toggles it and tells its handler. client.dll 0xd1c90-0xd2240, used by
*	package c5's panels (0xed18b-0xed72e). All names ours.
*
****/

#pragma once

#include <VGUI_Panel.h>
#include <VGUI_Label.h>
#include <VGUI_ImagePanel.h>
#include "vgui_defaultinputsignal.h"

class CToggleImageButtonHandler
{
public:
	virtual void onToggle() = 0; // slot 0, no arguments
};

class CToggleImageButton : public vgui::Panel, public vgui::CDefaultInputSignal
{
public:
	CToggleImageButton();
	~CToggleImageButton();

	void setImagesTGA(const char* pszChecked, const char* pszUnchecked); // owns them
	void setImages(vgui::Image* pChecked, vgui::Image* pUnchecked);		   // doesn't
	void clearImages();
	void setImageFirst(bool bImageFirst);
	void setText(const char* pszFormat, ...);
	void setHandler(CToggleImageButtonHandler* pHandler);
	bool isChecked();
	void setChecked(bool bChecked);
	void layout();

	void mousePressed(vgui::MouseCode code, vgui::Panel* panel) override;

protected:
	void internalMousePressed(vgui::MouseCode code) override;

private:
	CToggleImageButtonHandler* m_pHandler; // 0xc0
	bool m_bImageFirst;					   // 0xc4: the image left of the label
	vgui::Label m_Label;				   // 0xc8
	vgui::ImagePanel m_Image;			   // 0x198
	vgui::Image* m_pChecked;			   // 0x258
	vgui::Image* m_pUnchecked;			   // 0x25c
	bool m_bOwnImages;					   // 0x260
	bool m_bChecked;					   // 0x261
};
