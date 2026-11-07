//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "vgui_int.h"
#include <VGUI_Label.h>
#include <VGUI_BorderLayout.h>
#include <VGUI_LineBorder.h>
#include <VGUI_SurfaceBase.h>
#include <VGUI_TextEntry.h>
#include <VGUI_ActionSignal.h>
#include <string.h>
#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "vgui_TeamFortressViewport.h"

using namespace vgui;

// orig client 0xd91b0
void VGui_ViewportPaintBackground(int extents[4])
{
	gEngfuncs.VGui_ViewportPaintBackground(extents);
}

// orig client 0xd91d0
void* VGui_GetPanel()
{
	return (Panel*)gEngfuncs.VGui_GetPanel();
}

// orig client 0xd91f0. The Layout and BorderLayout copy constructors, operator= and destructor that follow it (client 0xd9330-0xd93a0) are the compiler's, from the VGUI headers.
void VGui_Startup()
{
	Panel* root = (Panel*)VGui_GetPanel();
	root->setBgColor(128, 128, 0, 0);
	//root->setNonPainted(false);
	//root->setBorder(new LineBorder());
	root->setLayout(new BorderLayout(0));


	//root->getSurfaceBase()->setEmulatedCursorVisible(true);

	if (gViewPort != NULL)
	{
		//		root->removeChild(gViewPort);

		// free the memory
		//		delete gViewPort;
		//		gViewPort = NULL;

		gViewPort->Initialize();
	}
	else
	{
		gViewPort = new TeamFortressViewport(0, 0, root->getWide(), root->getTall());
		gViewPort->setParent(root);
	}
}

void VGui_Shutdown()
{
	delete gViewPort;
	gViewPort = NULL;
}
