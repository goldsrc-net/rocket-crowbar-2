// Rocket Crowbar 2: the entity debug panel (VGUIDebugOn, DBGCounts, DBGEntInfo). client.dll keeps
// no file names: this is the run of RC2's code linked between vgui_CustomObjects and vgui_grid.
// Class and member names ours.
#pragma once

#include "vgui_TeamFortressViewport.h"

// The entity the server reports on (DBGEntInfo), 0 for none; the 3D box is drawn around it
extern int g_iDebugEntity; // name ours

void DrawDebugEntityBox(); // name ours

class CDebugPanel : public CTransparentPanel // name ours
{
public:
	CDebugPanel();

	bool MsgFunc_DBGCounts(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_DBGEntInfo(const char* pszName, int iSize, void* pbuf);

	void paintBackground() override;

private:
	void DrawLine(int x, int y, float flAngle, float flLength); // name ours

	Label* m_pCountsList;	 // 0xc0: the eight commonest classnames
	Label* m_pCountsLabel;	 // 0xc4: "n/max Entities", green to red
	Label* m_pEntityLabel;	 // 0xc8: address, index, classname
	Label* m_pStatusLabel;	 // 0xcc: "Monitoring..." or why the entity is gone
	Label* m_pNameLabel;	 // 0xd0: netname, model, target, targetname
	Label* m_pHealthLabel;	 // 0xd4: health, armour, waterlevel, flags
	Label* m_pThinkLabel;	 // 0xd8
	Label* m_pTouchLabel;	 // 0xdc
	Label* m_pOriginLabel;	 // 0xe0: origin and velocity
	Label* m_pAnglesLabel;	 // 0xe4: angles and v_angle
	Vector m_vecOrigin;		 // 0xe8: last origin received
	Vector m_vecVelocity;	 // 0xf4: as sent, or worked out from the last two origins
	Vector m_vecAngles;		 // 0x100
	float m_flLastUpdate;	 // 0x10c: client time of the last DBGEntInfo
};
