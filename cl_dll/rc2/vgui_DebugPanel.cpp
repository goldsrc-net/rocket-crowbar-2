// Rocket Crowbar 2: the entity debug panel. The server's debug mode (player.cpp, SendDebugCounts)
// sends VGUIDebugOn to show it, DBGCounts with the entity count and the eight commonest
// classnames, and DBGEntInfo with what it knows of the entity under watch; the panel draws a
// small compass of that entity against the view, and DrawDebugEntityBox draws its box in the
// world. Names ours.

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "event_api.h"
#include "cl_entity.h"
#include "com_model.h"
#ifndef PITCH
#define PITCH 0
#define YAW 1
#endif
#include <VGUI_Font.h>

#include "vgui_TeamFortressViewport.h"
#include "rc2/vgui_DebugPanel.h"

int g_iDebugEntity = 0;			   // name ours
static model_s* g_pWhiteSprite;	   // name ours
static char g_szDebugCounts[512];  // name ours
static char g_szDebugLine[128];	   // name ours

// the highest entity index HUD_AddEntity has seen, plus one (entity.cpp, package c3)
extern int g_iMaxEntityIndex; // name ours
// the entity's interpolated origin (RC2's own client code, package c2)
extern Vector RC2_GetEntityOrigin(int iEntity); // name ours

#define DEBUG_PANEL_WIDE 616
#define DEBUG_PANEL_TALL 117

// orig client 0xd4d60
CDebugPanel::CDebugPanel() : CTransparentPanel(200, ScreenWidth - DEBUG_PANEL_WIDE, ScreenHeight - gHUD.m_iFontHeight * 4 - DEBUG_PANEL_TALL, DEBUG_PANEL_WIDE, DEBUG_PANEL_TALL)
{
	setVisible(false);

	Font* pFont = new Font("Courier New", 12, 0, 0.0f, 0, false, false, false, false);

	auto MakeLabel = [&](int x, int y, int wide, int tall, int r, int g, int b) -> Label*
	{
		Label* pLabel = new Label("", x, y, wide, tall);
		pLabel->setParent(this);
		pLabel->setFont(pFont);
		pLabel->setFgColor(r, g, b, 0);
		pLabel->setBgColor(0, 0, 0, 255);
		pLabel->setContentAlignment(vgui::Label::a_northwest);
		pLabel->setText("");
		return pLabel;
	};

	m_pCountsLabel = MakeLabel(0, 0, 140, 13, 0, 255, 0);
	m_pCountsList = MakeLabel(0, 13, 120, 104, 255, 170, 0);
	m_pEntityLabel = MakeLabel(140, 0, 400, 13, 255, 170, 0);
	m_pNameLabel = MakeLabel(140, 13, 400, 26, 255, 170, 0);
	m_pHealthLabel = MakeLabel(140, 39, 400, 13, 255, 170, 0);
	m_pThinkLabel = MakeLabel(140, 52, 400, 13, 255, 170, 0);
	m_pTouchLabel = MakeLabel(140, 65, 400, 13, 255, 170, 0);
	m_pOriginLabel = MakeLabel(140, 78, 400, 13, 255, 170, 0);
	m_pAnglesLabel = MakeLabel(140, 91, 400, 13, 255, 170, 0);
	m_pStatusLabel = MakeLabel(140, 104, 400, 13, 255, 170, 0);

	m_vecOrigin = g_vecZero;
	m_vecVelocity = g_vecZero;
	m_vecAngles = g_vecZero;
	m_flLastUpdate = 0;
}

// orig client 0xd5710. A line of single pixels from (x, y): angle 0 points up.
void CDebugPanel::DrawLine(int x, int y, float flAngle, float flLength)
{
	float flSin = sin(flAngle);
	float flCos = -cos(flAngle);
	int iLength = (int)flLength;

	for (int i = 0; i < iLength; i++)
	{
		int px = (int)(i * flSin + x + 0.5f);
		int py = (int)(i * flCos + y + 0.5f);
		drawFilledRect(px, py, px + 1, py + 1);
	}
}

// orig client 0xd57e0. Over the panel's black: a compass at (570, 58) with the entity's yaw
// against the view (green), its velocity (blue) and the direction to it (red), and bars at
// x 610 (its pitch), 611 (its vertical speed) and 612 (the pitch to it).
void CDebugPanel::paintBackground()
{
	CTransparentPanel::paintBackground();

	if (m_vecOrigin[0] == 0 && m_vecOrigin[1] == 0 && m_vecOrigin[2] == 0)
		return;

	cl_entity_t* pPlayer = gEngfuncs.GetLocalPlayer();
	if (!pPlayer)
		return;

	Vector vecEye;
	Vector vecView;
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(vecEye);
	vecEye = vecEye + pPlayer->origin;
	gEngfuncs.GetViewAngles(vecView);

	Vector vecTarget = m_vecOrigin;
	Vector vecVelocity = m_vecVelocity;
	Vector vecAngles = m_vecAngles;
	vecView[PITCH] = -vecView[PITCH];

	auto Bar = [this](int x, int iLength)
	{
		if (iLength < -40)
			iLength = -40;
		else if (iLength > 40)
			iLength = 40;

		if (iLength > 0)
			drawFilledRect(x, 58, x + 1, 58 + iLength);
		else if (iLength < 0)
			drawFilledRect(x, 58 + iLength, x + 1, 58);
	};

	// the entity's facing, and its pitch
	drawSetColor(0, 255, 0, 0);
	DrawLine(570, 58, (vecView[YAW] - vecAngles[YAW]) * (M_PI / 180), 20);
	Bar(610, (int)(vecAngles[PITCH] * (1.0f / 90) * 40));

	// its velocity
	drawSetColor(128, 128, 255, 0);
	Bar(611, (int)(vecVelocity[2] * -0.001f * 40));

	if (vecVelocity[0] != 0 && vecVelocity[1] != 0)
	{
		float flAngle = vecView[YAW] * (M_PI / 180) - atan2(vecVelocity[1], vecVelocity[0]);
		float flSpeed = sqrt(vecVelocity[0] * vecVelocity[0] + vecVelocity[1] * vecVelocity[1] + vecVelocity[2] * vecVelocity[2]);

		if (flSpeed < 0)
			flSpeed = 0;
		else if (flSpeed > 1000)
			flSpeed = 1000;

		DrawLine(570, 58, flAngle, flSpeed * 0.001f * 40);
	}

	// where it is
	drawSetColor(255, 0, 0, 0);

	Vector vecDir = vecEye - vecTarget;
	float flLength = sqrt(vecDir[2] * vecDir[2] + vecDir[1] * vecDir[1] + vecDir[0] * vecDir[0]);
	if (flLength == 0)
	{
		vecDir = Vector(0, 0, 1);
	}
	else
	{
		float flInv = 1 / flLength;
		vecDir = vecDir * flInv;
	}

	Vector vecToAngles;
	VectorAngles(vecDir, vecToAngles);
	vecToAngles[PITCH] = -vecView[PITCH] - vecToAngles[PITCH];
	vecToAngles[YAW] = vecView[YAW] - vecToAngles[YAW] + 180;

	Bar(612, (int)(vecToAngles[PITCH] * (-1.0f / 90) * 40));

	float flDist = sqrt((vecEye[0] - vecTarget[0]) * (vecEye[0] - vecTarget[0]) + (vecEye[1] - vecTarget[1]) * (vecEye[1] - vecTarget[1])) * (1.0 / 512);
	if (flDist < 0)
		flDist = 0;
	else if (flDist > 1)
		flDist = 1;

	DrawLine(570, 58, vecToAngles[YAW] * (M_PI / 180), flDist * 40);

	// the compass centre and the bars' zero
	drawSetColor(255, 170, 0, 0);
	drawFilledRect(568, 56, 572, 60);
	drawFilledRect(610, 57, 613, 59);
}

// orig client 0xd5e40. DBGCounts: the entity count against gpGlobals->maxEntities, green up to
// half full, then through yellow to red; then eight counts and classnames.
bool CDebugPanel::MsgFunc_DBGCounts(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int iCount = READ_SHORT();
	int iMax = READ_SHORT();

	sprintf(g_szDebugCounts, "%4d/%-4d Entities", iCount, iMax);
	m_pCountsLabel->setText(g_szDebugCounts);

	float flFull = (float)iCount / iMax;
	if (flFull <= 0.5)
	{
		m_pCountsLabel->setFgColor(0, 255, 0, 0);
	}
	else
	{
		float f = (flFull - 0.5) * 2;
		int r, g;
		if (f <= 0.5)
		{
			r = (int)(f * 2 * 255);
			g = 255;
		}
		else
		{
			r = 255;
			g = 255 - (int)((f - 0.5) * 2 * 255);
		}
		m_pCountsLabel->setFgColor(r, g, 0, 0);
	}

	strcpy(g_szDebugCounts, "");
	for (int i = 0; i < 8; i++)
	{
		int iClassCount = READ_SHORT();
		const char* pszClassname = READ_STRING();
		snprintf(g_szDebugLine, 127, " %4d %s\n", iClassCount, pszClassname);
		g_szDebugLine[127] = 0;
		strncat(g_szDebugCounts, g_szDebugLine, 511 - strlen(g_szDebugCounts));
		g_szDebugCounts[511] = 0;
	}

	m_pCountsList->setText(g_szDebugCounts);
	return true;
}

// orig client 0xd6040. DBGEntInfo: a byte first, non-zero when the entity is gone (1-5 why, 255
// clears the panel); 0 is followed by the entity's state (player.cpp). The velocity is worked out
// from the last two origins when the server sends none and the entity was seen before.
bool CDebugPanel::MsgFunc_DBGEntInfo(const char* pszName, int iSize, void* pbuf)
{
	char szLine[512];
	char szClassname[112];
	char szModel[16];
	char szNetname[128];
	char szTarget[128];
	char szTargetname[128];

	BEGIN_READ(pbuf, iSize);

	int iGone = READ_BYTE();
	if (0 != iGone)
	{
		m_pStatusLabel->setFgColor(255, 0, 0, 0);

		switch (iGone)
		{
		case 1:
			m_pStatusLabel->setText("Dropped: S/N mismatch");
			break;
		case 2:
			m_pStatusLabel->setText("Dropped: Freed");
			break;
		case 3:
			m_pStatusLabel->setText("Dropped: no private data");
			break;
		case 4:
			m_pStatusLabel->setText("Dropped: entindex was zero");
			break;
		case 5:
			m_pStatusLabel->setText("Dropped: FL_KILLME");
			break;
		case 255:
			m_pEntityLabel->setText("");
			m_pNameLabel->setText("");
			m_pHealthLabel->setText("");
			m_pThinkLabel->setText("");
			m_pTouchLabel->setText("");
			m_pOriginLabel->setText("");
			m_pAnglesLabel->setText("");
			m_pStatusLabel->setText("");
			m_vecOrigin = g_vecZero;
			break;
		default:
			m_pStatusLabel->setText("Dropped: unkown reason");
			break;
		}

		g_iDebugEntity = 0;
		return true;
	}

	int iAddress = READ_LONG();
	int iEntity = READ_SHORT();
	strcpy(szClassname, READ_STRING());
	g_iDebugEntity = iEntity;
	sprintf(szLine, "0x%08X  %d  \"%s\"", iAddress, iEntity, szClassname);
	m_pEntityLabel->setText(szLine);

	strcpy(szClassname, READ_STRING()); // netname
	strcpy(szTarget, READ_STRING());
	strcpy(szTargetname, READ_STRING());
	strcpy(szNetname, READ_STRING()); // model
	sprintf(szLine, "Netname: \"%s\"   Model: \"%s\"\nTarget: \"%s\"    Targetname: \"%s\"", szClassname, szNetname, szTarget, szTargetname);
	m_pNameLabel->setText(szLine);

	int iHealth = READ_SHORT();
	int iArmor = READ_SHORT();
	int iWaterlevel = READ_BYTE();
	int iFlags = READ_LONG();
	sprintf(szLine, "Health: %-4d  Armor: %-4d  Waterlevel: %1d  Flags: 0x%08X", iHealth, iArmor, iWaterlevel, iFlags);
	m_pHealthLabel->setText(szLine);

	int iThink = READ_SHORT();
	strcpy(szClassname, READ_STRING());
	float flThink = iThink * 0.001f;
	sprintf(szLine, "Think % 7.3f: %s", flThink, szClassname);
	m_pThinkLabel->setText(szLine);

	// red when it is due to think but has no think function
	if (flThink != 0 && 0 == strcmp(szClassname, "none"))
		m_pThinkLabel->setFgColor(255, 0, 0, 0);
	else
		m_pThinkLabel->setFgColor(255, 160, 0, 0);

	strcpy(szModel, READ_STRING());
	sprintf(szLine, "Touch: %s", szModel);
	m_pTouchLabel->setText(szLine);

	Vector vecOrigin, vecVelocity;
	vecOrigin[0] = READ_COORD();
	vecOrigin[1] = READ_COORD();
	vecOrigin[2] = READ_COORD();
	vecVelocity[0] = READ_COORD();
	vecVelocity[1] = READ_COORD();
	vecVelocity[2] = READ_COORD();

	m_vecVelocity = vecVelocity;
	if (m_flLastUpdate != 0 && vecVelocity[0] == 0 && vecVelocity[1] == 0)
	{
		float flTime = gEngfuncs.GetClientTime() - m_flLastUpdate;
		if (flTime > 0)
		{
			m_vecVelocity[0] = (vecOrigin[0] - m_vecOrigin[0]) / flTime;
			m_vecVelocity[1] = (vecOrigin[1] - m_vecOrigin[1]) / flTime;
			m_vecVelocity[2] = (vecOrigin[2] - m_vecOrigin[2]) / flTime;
		}
	}
	m_vecOrigin = vecOrigin;

	sprintf(szLine, "Origin: % 7.2f % 7.2f % 7.2f  Velocity: % 7.2f % 7.2f % 7.2f",
		vecOrigin[0], vecOrigin[1], vecOrigin[2], vecVelocity[0], vecVelocity[1], vecVelocity[2]);
	m_pOriginLabel->setText(szLine);

	Vector vecAngles, vecViewAngle;
	vecAngles[0] = READ_COORD();
	vecAngles[1] = READ_COORD();
	vecAngles[2] = READ_COORD();
	vecViewAngle[0] = READ_COORD();
	vecViewAngle[1] = READ_COORD();
	vecViewAngle[2] = READ_COORD();
	m_vecAngles = vecAngles;

	sprintf(szLine, "Angles: % 7.2f % 7.2f % 7.2f  V. Angle: % 7.2f % 7.2f % 7.2f",
		vecAngles[0], vecAngles[1], vecAngles[2], vecViewAngle[0], vecViewAngle[1], vecViewAngle[2]);
	m_pAnglesLabel->setText(szLine);

	m_pStatusLabel->setText("Monitoring...");
	m_pStatusLabel->setFgColor(255, 160, 0, 0);

	m_flLastUpdate = gEngfuncs.GetClientTime();
	return true;
}

// orig client 0xd6b00. The watched entity's box in red, with blue lines from its origin to each
// corner, or a green cross when it has no size; drawn only once the client has it in this
// frame's packet. The bottom is lifted half a unit.
void DrawDebugEntityBox()
{
	if (g_iDebugEntity <= 0 || g_iDebugEntity > g_iMaxEntityIndex)
		return;

	cl_entity_t* pPlayer = gEngfuncs.GetLocalPlayer();
	cl_entity_t* pEntity = gEngfuncs.GetEntityByIndex(g_iDebugEntity);
	if (!pPlayer || !pEntity)
		return;

	if (pPlayer->curstate.messagenum > pEntity->curstate.messagenum)
		return;

	if (!g_pWhiteSprite)
	{
		int iIndex;
		g_pWhiteSprite = gEngfuncs.CL_LoadModel("sprites/white.spr", &iIndex);
		if (!g_pWhiteSprite)
			return;
	}

	Vector vecOrigin = RC2_GetEntityOrigin(g_iDebugEntity);
	Vector vecMins = pEntity->curstate.mins + vecOrigin;
	Vector vecMaxs = pEntity->curstate.maxs + vecOrigin;

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	gEngfuncs.pTriAPI->SpriteTexture(g_pWhiteSprite, 0);

	auto Vertex = [](float r, float g, float b, float x, float y, float z)
	{
		gEngfuncs.pTriAPI->Color4f(r, g, b, 1);
		gEngfuncs.pTriAPI->Brightness(1);
		gEngfuncs.pTriAPI->Vertex3f(x, y, z);
	};

	if (vecMins[0] == vecMaxs[0] && vecMins[1] == vecMaxs[1] && vecMins[2] == vecMaxs[2])
	{
		Vector c = vecOrigin;
		c[2] += 0.5;

		gEngfuncs.pTriAPI->Begin(TRI_LINES);
		Vertex(0, 1, 0, c[0] - 16, c[1], c[2]);
		Vertex(0, 1, 0, c[0] + 16, c[1], c[2]);
		Vertex(0, 1, 0, c[0], c[1] - 16, c[2]);
		Vertex(0, 1, 0, c[0], c[1] + 16, c[2]);
		Vertex(0, 1, 0, c[0], c[1], c[2] - 16);
		Vertex(0, 1, 0, c[0], c[1], c[2] + 16);
		gEngfuncs.pTriAPI->End();
		gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
		return;
	}

	float lx = vecMins[0], ly = vecMins[1], lz = vecMins[2] + 0.5;
	float hx = vecMaxs[0], hy = vecMaxs[1], hz = vecMaxs[2];

	gEngfuncs.pTriAPI->Begin(TRI_LINES);

	// the box
	const float box[12][6] = {
		{lx, ly, lz, hx, ly, lz}, {lx, ly, lz, lx, hy, lz}, {lx, ly, lz, lx, ly, hz},
		{hx, hy, hz, lx, hy, hz}, {hx, hy, hz, hx, ly, hz}, {hx, hy, hz, hx, hy, lz},
		{hx, ly, lz, hx, hy, lz}, {lx, hy, lz, lx, hy, hz}, {lx, hy, lz, hx, hy, lz},
		{lx, hy, hz, lx, ly, hz}, {hx, ly, hz, hx, ly, lz}, {hx, ly, hz, lx, ly, hz}};
	for (int i = 0; i < 12; i++)
	{
		Vertex(1, 0, 0, box[i][0], box[i][1], box[i][2]);
		Vertex(1, 0, 0, box[i][3], box[i][4], box[i][5]);
	}

	// the origin to each corner
	const float corners[8][3] = {
		{lx, ly, lz}, {hx, hy, hz}, {hx, ly, lz}, {lx, hy, lz},
		{lx, ly, hz}, {lx, hy, hz}, {hx, ly, hz}, {hx, hy, lz}};
	for (int i = 0; i < 8; i++)
	{
		Vertex(0, 0, 1, vecOrigin[0], vecOrigin[1], vecOrigin[2]);
		Vertex(0, 0, 1, corners[i][0], corners[i][1], corners[i][2]);
	}

	gEngfuncs.pTriAPI->End();
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

// TEMP STUB (c5): real one belongs in entity.cpp (HUD_AddEntity, package c3)
int g_iMaxEntityIndex = 0;
// TEMP STUB (c5): real one belongs in RC2's client code at client 0x59be0 (package c2)
Vector RC2_GetEntityOrigin(int iEntity)
{
	cl_entity_t* pEntity = gEngfuncs.GetEntityByIndex(iEntity);
	return pEntity ? Vector(pEntity->origin) : g_vecZero;
}
