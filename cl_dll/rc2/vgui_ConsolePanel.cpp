//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
// Rocket Crowbar 2: SDK 2.3's vgui_ConsolePanel.cpp, which RC2 links (halflife-updated dropped it):
// client.dll 0xd2270-0xd2600, the same code. The copy constructors and assignments of TextEntry,
// EtchedBorder and LoweredBorder after it (orig client 0xd2c40, orig client 0xd2cd0,
// orig client 0xd2d90, orig client 0xd2e20) are the compiler's, from the VGUI headers.

#include"vgui_ConsolePanel.h"
#include"hud.h"
#include<VGUI_ActionSignal.h>
#include<VGUI_TextGrid.h>
#include<VGUI_TextEntry.h>
#include<VGUI_EtchedBorder.h>
#include<VGUI_LoweredBorder.h>

using namespace vgui;


namespace
{

class Handler : public ActionSignal
{
private:
	
	ConsolePanel* _consolePanel;

public:

	Handler(ConsolePanel* consolePanel)
	{
		_consolePanel=consolePanel;
	}

public:

	// orig client 0xd2450
	virtual void actionPerformed(Panel* panel)
	{
		_consolePanel->doExecCommand();
	}

};

}



// orig client 0xd2270 (not in files-client.tsv: Ghidra missed it)
ConsolePanel::ConsolePanel(int x,int y,int wide,int tall) : Panel(x,y,wide,tall)
{
	setBorder(new EtchedBorder());
	
	_textGrid=new TextGrid(80,21,5,5,200,100);
	_textGrid->setBorder(new LoweredBorder());
	_textGrid->setParent(this);
	
	_textEntry=new TextEntry("",5,5,200,20);
	_textEntry->setParent(this);
	_textEntry->addActionSignal(new Handler(this));
}

// orig client 0xd2470 (not in files-client.tsv: Ghidra missed it)
int ConsolePanel::print(const char* text)
{
	return _textGrid->printf("%s",text);
}

// orig client 0xd24a0 (not in files-client.tsv: Ghidra missed it)
int ConsolePanel::vprintf(const char* format,va_list argList)
{
	return _textGrid->vprintf(format,argList);
}

// orig client 0xd24d0 (not in files-client.tsv: Ghidra missed it)
int ConsolePanel::printf(const char* format,...)
{
	va_list argList;
	va_start(argList,format);
	int ret=vprintf(format,argList);
	va_end(argList);
	return ret;
}

// orig client 0xd2500 (not in files-client.tsv: Ghidra missed it)
void ConsolePanel::doExecCommand()
{
	char buf[2048];
	_textEntry->getText(0,buf,2048);
	_textEntry->setText(null,0);
	gEngfuncs.pfnClientCmd(buf);
}

// orig client 0xd2570 (not in files-client.tsv: Ghidra missed it)
void ConsolePanel::setSize(int wide,int tall)
{
	Panel::setSize(wide,tall);
		
	getPaintSize(wide,tall);

	_textGrid->setBounds(5,5,wide-10,tall-35);
	_textEntry->setBounds(5,tall-25,wide-10,20);
}





