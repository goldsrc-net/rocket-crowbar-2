/***
*
*	Rocket Crowbar 2: the client effects' own drawing (see clte.h): animating disks (a cone or a
*	ring of quads round an axis, its sizes and two colors moving to their end values over its
*	life), fade quads, and the strips trails are drawn as, all facing the view.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "com_model.h"
#include "clte.h"

#include <stdio.h>
#include <string.h>

// --- ring tables ---

const int g_iRingSegments[CLTE_NUM_RINGS + 1] = {8, 10, 12, 16, 20, 24, 32, 0}; // orig .data 0x10136df4
static Vector s_vecRings[CLTE_NUM_RINGS][33];									   // orig .bss 0x101f19f0: cos, sin, 0 round each ring, closed

// --- animating disks ---

#define CLTE_MAX_DISKS 64 // name ours

struct animdisk_t // name ours; 0xac bytes
{
	bool bActive;		  // +0
	bool bRing;			  // +1: a ring of quads rather than a cone
	float flLife;		  // +4
	Vector vecColor[2];	  // +8: inner (cone: apex) and outer
	Vector vecColorRate[2]; // +0x20
	float flAlpha[2];	  // +0x38
	float flAlphaRate[2]; // +0x40
	Vector vecOrigin;	  // +0x48
	Vector vecX;		  // +0x54
	Vector vecY;		  // +0x60
	Vector vecZ;		  // +0x6c: the axis
	int iRing;			  // +0x78
	float flRadius[2];	  // +0x7c: inner and outer (a cone has the first only)
	float flRadiusRate[2]; // +0x84
	float flHeight[2];	  // +0x8c: along the axis (a cone: apex and base)
	float flHeightRate[2]; // +0x94
	float flFrame;		  // +0x9c
	float flNumFrames;	  // +0xa0
	float flFramerate;	  // +0xa4
	model_s* pModel;	  // +0xa8
};

// orig client 0x50780 and orig client 0x507a0: this array's compiler-generated constructor (its Vectors)
static animdisk_t s_Disks[CLTE_MAX_DISKS]; // orig .bss 0x101f3958

// --- fade quads ---

#define CLTE_MAX_QUADGROUPS 64 // name ours
#define CLTE_GROUP_QUADS 64	   // name ours

struct fadequads_t // name ours; 0x1108 bytes
{
	Vector vecVerts[CLTE_GROUP_QUADS * 4]; // +0
	Vector vecColor[CLTE_GROUP_QUADS];	   // +0xc00
	float flAlpha[CLTE_GROUP_QUADS];	   // +0xf00
	float flFade[CLTE_GROUP_QUADS];		   // +0x1000
	int iCount;							   // +0x1100: the quads still showing
	model_s* pModel;					   // +0x1104
};

// orig client 0x507f0 and orig client 0x50810: this array's compiler-generated constructor. The empty
// dynamic initializers of the file's other globals: orig client 0x50740, orig client 0x50760,
// orig client 0x50860, orig client 0x50880, orig client 0x508a0, orig client 0x508c0
static fadequads_t s_QuadGroups[CLTE_MAX_QUADGROUPS]; // orig .bss 0x101a9a58

// orig client 0x52ad0. The ring tables built, the disks and quads cleared
void CLTE_InitShapes()
{
	for (int i = 0; i < CLTE_NUM_RINGS; i++)
	{
		const int iSegs = g_iRingSegments[i];
		float flAngle = 0;
		for (int j = 0; j <= iSegs; j++)
		{
			s_vecRings[i][j].x = cos(flAngle);
			s_vecRings[i][j].y = sin(flAngle);
			s_vecRings[i][j].z = 0;
			flAngle += 6.2831855f / (float)iSegs;
		}
		s_vecRings[i][iSegs] = s_vecRings[i][0];
	}

	for (int i = 0; i < CLTE_MAX_DISKS; i++)
		s_Disks[i].bActive = false;

	for (int i = 0; i < CLTE_MAX_QUADGROUPS; i++)
		s_QuadGroups[i].iCount = 0;
}

// orig client 0x52bc0. From CLTE_Update
void CLTE_UpdateShapes(float flFrametime)
{
	for (int i = 0; i < CLTE_MAX_DISKS; i++)
	{
		animdisk_t* d = &s_Disks[i];
		if (!d->bActive)
			continue;

		d->flHeight[0] = flFrametime * d->flHeightRate[0] + d->flHeight[0];
		d->flHeight[1] = flFrametime * d->flHeightRate[1] + d->flHeight[1];
		d->flRadius[0] = flFrametime * d->flRadiusRate[0] + d->flRadius[0];
		d->flRadius[1] = flFrametime * d->flRadiusRate[1] + d->flRadius[1];
		d->vecColor[0] = d->vecColorRate[0] * flFrametime + d->vecColor[0];
		d->vecColor[1] = d->vecColorRate[1] * flFrametime + d->vecColor[1];
		d->flAlpha[0] = flFrametime * d->flAlphaRate[0] + d->flAlpha[0];
		d->flAlpha[1] = flFrametime * d->flAlphaRate[1] + d->flAlpha[1];

		d->flFrame = flFrametime * d->flFramerate + d->flFrame;
		if (d->flFrame > d->flNumFrames)
			d->flFrame = d->flFrame - d->flNumFrames;
		if (d->flFrame < 0)
			d->flFrame = d->flNumFrames + d->flFrame;

		const float flLife = d->flLife;
		d->flLife = flLife - flFrametime;
		if (flLife - flFrametime < 0)
			d->bActive = false;
	}

	for (int i = 0; i < CLTE_MAX_QUADGROUPS; i++)
	{
		fadequads_t* g = &s_QuadGroups[i];
		if (g->iCount <= 0)
			continue;

		g->iCount = 0;
		for (int j = 0; j < CLTE_GROUP_QUADS; j++)
		{
			const float flAlpha = g->flAlpha[j] - flFrametime * g->flFade[j];
			g->flAlpha[j] = flAlpha;
			if (flAlpha > 0)
			{
				if (flAlpha > 1)
					g->flAlpha[j] = 1;
				g->iCount++;
			}
			else
			{
				g->flAlpha[j] = 0;
			}
		}
	}
}

// Part of orig client 0x52650 (CLTE_DrawTransparent): the disks and the quads, additive, two-sided
void CLTE_DrawShapes()
{
	bool bAny = false;
	for (int i = 0; i < CLTE_MAX_DISKS; i++)
	{
		if (s_Disks[i].bActive)
		{
			bAny = true;
			break;
		}
	}
	if (!bAny)
	{
		for (int i = 0; i < CLTE_MAX_QUADGROUPS; i++)
		{
			if (s_QuadGroups[i].iCount > 0)
			{
				bAny = true;
				break;
			}
		}
		if (!bAny)
			return;
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);

	for (int i = 0; i < CLTE_MAX_DISKS; i++)
	{
		animdisk_t* d = &s_Disks[i];
		if (!d->bActive)
			continue;

		if (d->pModel)
			gEngfuncs.pTriAPI->SpriteTexture(d->pModel, (int)d->flFrame);

		if (!d->bRing)
		{
			CLTE_DrawCone(d->vecOrigin, d->vecX, d->vecY, d->vecZ, d->flHeight[0], d->flHeight[1], d->flRadius[0],
				d->iRing, d->vecColor[0], d->flAlpha[0], d->vecColor[1], d->flAlpha[1]);
		}
		else
		{
			CLTE_DrawRing(d->vecOrigin, d->vecX, d->vecY, d->vecZ, d->flRadius[0], d->flRadius[1], d->flHeight[0],
				d->flHeight[1], d->iRing, d->vecColor[0], d->flAlpha[0], d->vecColor[1], d->flAlpha[1]);
		}
	}

	for (int i = 0; i < CLTE_MAX_QUADGROUPS; i++)
	{
		fadequads_t* g = &s_QuadGroups[i];
		if (g->iCount <= 0)
			continue;

		if (g->pModel)
			gEngfuncs.pTriAPI->SpriteTexture(g->pModel, 0);

		gEngfuncs.pTriAPI->Begin(TRI_QUADS);
		for (int j = 0; j < CLTE_GROUP_QUADS; j++)
		{
			const float flAlpha = g->flAlpha[j];
			if (!(flAlpha > 0))
				continue;

			Vector* v = &g->vecVerts[j * 4];
			gEngfuncs.pTriAPI->Color4f(g->vecColor[j].x, g->vecColor[j].y, g->vecColor[j].z, flAlpha);
			gEngfuncs.pTriAPI->Brightness(g->flAlpha[j]);
			gEngfuncs.pTriAPI->TexCoord2f(0, 0);
			gEngfuncs.pTriAPI->Vertex3f(v[0].x, v[0].y, v[0].z);
			gEngfuncs.pTriAPI->TexCoord2f(1, 0);
			gEngfuncs.pTriAPI->Vertex3f(v[1].x, v[1].y, v[1].z);
			gEngfuncs.pTriAPI->TexCoord2f(1, 1);
			gEngfuncs.pTriAPI->Vertex3f(v[2].x, v[2].y, v[2].z);
			gEngfuncs.pTriAPI->TexCoord2f(0, 1);
			gEngfuncs.pTriAPI->Vertex3f(v[3].x, v[3].y, v[3].z);
		}
		gEngfuncs.pTriAPI->End();
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

static int CLTE_FreeDisk()
{
	int i;
	for (i = 0; i < CLTE_MAX_DISKS; i++)
	{
		if (!s_Disks[i].bActive)
			break;
	}
	if (i < CLTE_MAX_DISKS)
		return i;

	char sz[256];
	sprintf(sz, "Error: overflow: %i animating disks\n", CLTE_MAX_DISKS);
	gEngfuncs.pfnConsolePrint(sz);
	return -1;
}

// orig client 0x52e60. Nothing in client.dll calls it
void CLTE_AddDiskCone(Vector vecOrigin, Vector vecX, Vector vecY, Vector vecZ, int iRing,
	float flApex, float flApexEnd, float flBase, float flBaseEnd, float flRadius, float flRadiusEnd,
	Vector vecColor1, Vector vecColor1End, float flAlpha1, float flAlpha1End,
	Vector vecColor2, Vector vecColor2End, float flAlpha2, float flAlpha2End,
	float flLife, const char* pszModel, float flFrame, float flNumFrames, float flFramerate)
{
	const int i = CLTE_FreeDisk();
	if (i < 0)
		return;

	animdisk_t* d = &s_Disks[i];
	d->bActive = true;
	d->bRing = false;
	d->flLife = flLife;
	d->iRing = iRing;
	d->vecOrigin = vecOrigin;
	d->vecX = vecX;
	d->vecY = vecY;
	d->vecZ = vecZ;
	d->flHeight[0] = flApex;
	d->flHeightRate[0] = (flApexEnd - flApex) / flLife;
	d->flHeight[1] = flBase;
	d->flHeightRate[1] = (flBaseEnd - flBase) / flLife;
	d->flRadius[0] = flRadius;
	d->flRadiusRate[0] = (flRadiusEnd - flRadius) / flLife;
	d->flRadius[1] = 0;
	d->flRadiusRate[1] = 0;
	d->vecColor[0] = vecColor1;
	d->vecColorRate[0] = Vector((vecColor1End.x - vecColor1.x) / flLife, (vecColor1End.y - vecColor1.y) / flLife, (vecColor1End.z - vecColor1.z) / flLife);
	d->vecColor[1] = vecColor2;
	d->vecColorRate[1] = Vector((vecColor2End.x - vecColor2.x) / flLife, (vecColor2End.y - vecColor2.y) / flLife, (vecColor2End.z - vecColor2.z) / flLife);
	d->flAlpha[0] = flAlpha1;
	d->flAlphaRate[0] = (flAlpha1End - flAlpha1) / flLife;
	d->flAlpha[1] = flAlpha2;
	d->flAlphaRate[1] = (flAlpha2End - flAlpha2) / flLife;
	d->flFrame = flFrame;
	d->flNumFrames = flNumFrames;
	d->flFramerate = flFramerate;
	int iIndex;
	d->pModel = gEngfuncs.CL_LoadModel(pszModel, &iIndex);
}

// orig client 0x532f0
void CLTE_AddDiskRing(Vector vecOrigin, Vector vecX, Vector vecY, Vector vecZ, int iRing,
	float flHeight1, float flHeight1End, float flHeight2, float flHeight2End,
	float flRadius1, float flRadius1End, float flRadius2, float flRadius2End,
	Vector vecColor1, Vector vecColor1End, float flAlpha1, float flAlpha1End,
	Vector vecColor2, Vector vecColor2End, float flAlpha2, float flAlpha2End,
	float flLife, const char* pszModel, float flFrame, float flNumFrames, float flFramerate)
{
	const int i = CLTE_FreeDisk();
	if (i < 0)
		return;

	animdisk_t* d = &s_Disks[i];
	d->bActive = true;
	d->bRing = true;
	d->flLife = flLife;
	d->iRing = iRing;
	d->vecOrigin = vecOrigin;
	d->vecX = vecX;
	d->vecY = vecY;
	d->vecZ = vecZ;
	d->flHeight[0] = flHeight1;
	d->flHeightRate[0] = (flHeight1End - flHeight1) / flLife;
	d->flHeight[1] = flHeight2;
	d->flHeightRate[1] = (flHeight2End - flHeight2) / flLife;
	d->flRadius[0] = flRadius1;
	d->flRadiusRate[0] = (flRadius1End - flRadius1) / flLife;
	d->flRadius[1] = flRadius2;
	d->flRadiusRate[1] = (flRadius2End - flRadius2) / flLife;
	d->vecColor[0] = vecColor1;
	d->vecColorRate[0] = Vector((vecColor1End.x - vecColor1.x) / flLife, (vecColor1End.y - vecColor1.y) / flLife, (vecColor1End.z - vecColor1.z) / flLife);
	d->vecColor[1] = vecColor2;
	d->vecColorRate[1] = Vector((vecColor2End.x - vecColor2.x) / flLife, (vecColor2End.y - vecColor2.y) / flLife, (vecColor2End.z - vecColor2.z) / flLife);
	d->flAlpha[0] = flAlpha1;
	d->flAlphaRate[0] = (flAlpha1End - flAlpha1) / flLife;
	d->flAlpha[1] = flAlpha2;
	d->flAlphaRate[1] = (flAlpha2End - flAlpha2) / flLife;
	d->flFrame = flFrame;
	d->flNumFrames = flNumFrames;
	d->flFramerate = flFramerate;
	int iIndex;
	d->pModel = gEngfuncs.CL_LoadModel(pszModel, &iIndex);
}

// orig client 0x53780. iCount quads (at most 64) from parallel arrays, each fading at its rate
void CLTE_AddFadeQuads(int iCount, Vector* pV0, Vector* pV1, Vector* pV2, Vector* pV3, Vector* pColors,
	float* pAlphas, float* pFades, model_s* pModel)
{
	if (iCount < 1)
		return;

	int i;
	for (i = 0; i < CLTE_MAX_QUADGROUPS; i++)
	{
		if (s_QuadGroups[i].iCount < 1)
			break;
	}
	if (i >= CLTE_MAX_QUADGROUPS)
	{
		char sz[256];
		sprintf(sz, "Error: overflow: %i fade quads\n", CLTE_MAX_QUADGROUPS);
		gEngfuncs.pfnConsolePrint(sz);
		return;
	}

	if (iCount > CLTE_GROUP_QUADS)
		iCount = CLTE_GROUP_QUADS;

	fadequads_t* g = &s_QuadGroups[i];
	g->iCount = iCount;
	g->pModel = pModel;
	for (int j = 0; j < iCount; j++)
	{
		g->vecColor[j] = pColors[j];
		g->flAlpha[j] = pAlphas[j];
		g->flFade[j] = pFades[j];
		g->vecVerts[j * 4 + 0] = pV0[j];
		g->vecVerts[j * 4 + 1] = pV1[j];
		g->vecVerts[j * 4 + 2] = pV2[j];
		g->vecVerts[j * 4 + 3] = pV3[j];
	}
}

static void Tri_Color(const Vector& vecColor, float flAlpha)
{
	gEngfuncs.pTriAPI->Color4f(vecColor.x, vecColor.y, vecColor.z, flAlpha);
	gEngfuncs.pTriAPI->Brightness(flAlpha);
}

static void Tri_Vertex(const Vector& v)
{
	gEngfuncs.pTriAPI->Vertex3f(v.x, v.y, v.z);
}

// orig client 0x539b0. A cone: its apex on the axis at flApex, its rim at flBase, flRadius round
void CLTE_DrawCone(Vector vecOrigin, Vector vecX, Vector vecY, Vector vecZ, float flApex, float flBase,
	float flRadius, int iRing, Vector vecColor1, float flAlpha1, Vector vecColor2, float flAlpha2)
{
	bool bFlip = true;

	gEngfuncs.pTriAPI->Begin(TRI_TRIANGLE_FAN);

	Tri_Color(vecColor1, flAlpha1);
	gEngfuncs.pTriAPI->TexCoord2f(1, 0.5);
	Tri_Vertex(vecOrigin + vecZ * flApex);

	for (int i = 0; i <= g_iRingSegments[iRing]; i++)
	{
		const Vector& vecRing = s_vecRings[iRing][i];
		const Vector v = (vecOrigin + vecZ * flBase) + (vecX * vecRing.x * flRadius + vecY * vecRing.y * flRadius);

		Tri_Color(vecColor2, flAlpha2);
		if (bFlip)
			gEngfuncs.pTriAPI->TexCoord2f(0, 0);
		else
			gEngfuncs.pTriAPI->TexCoord2f(0, 1);
		Tri_Vertex(v);
		bFlip = !bFlip;
	}

	gEngfuncs.pTriAPI->End();
}

// orig client 0x53ea0. A ring of quads between radius 1 at height 1 (color 1) and radius 2 at
// height 2 (color 2)
void CLTE_DrawRing(Vector vecOrigin, Vector vecX, Vector vecY, Vector vecZ, float flRadius1, float flRadius2,
	float flHeight1, float flHeight2, int iRing, Vector vecColor1, float flAlpha1, Vector vecColor2, float flAlpha2)
{
	bool bFlip = true;

	gEngfuncs.pTriAPI->Begin(TRI_QUAD_STRIP);

	for (int i = 0; i <= g_iRingSegments[iRing]; i++)
	{
		const Vector& vecRing = s_vecRings[iRing][i];
		const Vector vecOuter = (vecOrigin + vecZ * flHeight2) + (vecX * vecRing.x * flRadius2 + vecY * vecRing.y * flRadius2);
		const Vector vecInner = (vecOrigin + vecZ * flHeight1) + (vecX * vecRing.x * flRadius1 + vecY * vecRing.y * flRadius1);

		Tri_Color(vecColor2, flAlpha2);
		gEngfuncs.pTriAPI->TexCoord2f(0, bFlip ? 0 : 1);
		Tri_Vertex(vecOuter);

		Tri_Color(vecColor1, flAlpha1);
		gEngfuncs.pTriAPI->TexCoord2f(1, bFlip ? 0 : 1);
		Tri_Vertex(vecInner);

		bFlip = !bFlip;
	}

	gEngfuncs.pTriAPI->End();
}

// The strip's sideways direction at a point: across the segment and the line of sight
static Vector Strip_Side(const Vector& vecPoint, const Vector& vecDir)
{
	return CrossProduct(vecPoint - v_origin, vecDir).Normalize();
}

// orig client 0x54ad0. One quad from start to end, as wide as given at each end. Original quirk
// kept: the third vertex is lit with the start's alpha
void CLTE_DrawBeamQuad(Vector vecStart, Vector vecEnd, float flStartWidth, float flEndWidth,
	Vector vecStartColor, float flStartAlpha, Vector vecEndColor, float flEndAlpha)
{
	const Vector vecDir = vecEnd - vecStart;

	gEngfuncs.pTriAPI->Begin(TRI_QUADS);

	Vector vecSide = Strip_Side(vecStart, vecDir);
	Tri_Color(vecStartColor, flStartAlpha);
	gEngfuncs.pTriAPI->TexCoord2f(0, 0);
	Tri_Vertex(vecStart + vecSide * flStartWidth);
	Tri_Color(vecStartColor, flStartAlpha);
	gEngfuncs.pTriAPI->TexCoord2f(1, 0);
	Tri_Vertex(vecStart - vecSide * flStartWidth);

	vecSide = Strip_Side(vecEnd, vecDir);
	gEngfuncs.pTriAPI->Color4f(vecEndColor.x, vecEndColor.y, vecEndColor.z, flEndAlpha);
	gEngfuncs.pTriAPI->Brightness(flStartAlpha);
	gEngfuncs.pTriAPI->TexCoord2f(1, 1);
	Tri_Vertex(vecEnd - vecSide * flEndWidth);
	Tri_Color(vecEndColor, flEndAlpha);
	gEngfuncs.pTriAPI->TexCoord2f(0, 1);
	Tri_Vertex(vecEnd + vecSide * flEndWidth);

	gEngfuncs.pTriAPI->End();
}

// One pair of strip vertices
static void Strip_Pair(const Vector& vecPoint, const Vector& vecSide, float flWidth, const Vector& vecColor, float flAlpha, float flTexL, float flTexR, float flTexV)
{
	Tri_Color(vecColor, flAlpha);
	gEngfuncs.pTriAPI->TexCoord2f(flTexL, flTexV);
	Tri_Vertex(vecPoint + vecSide * flWidth);
	Tri_Color(vecColor, flAlpha);
	gEngfuncs.pTriAPI->TexCoord2f(flTexR, flTexV);
	Tri_Vertex(vecPoint - vecSide * flWidth);
}

// orig client 0x55190. A strip through iCount points. bTexAlong: the texture runs the strip's
// length once (by segment length with bTexByLength, which also fills pLengths), else it repeats
// on every segment
void CLTE_DrawStrip(Vector* pPoints, float* pWidths, Vector* pColors, float* pAlphas, int iCount,
	bool bTexAlong, bool bTexByLength, float* pLengths)
{
	if (iCount < 2)
		return;

	if (iCount == 2)
	{
		CLTE_DrawBeamQuad(pPoints[0], pPoints[1], pWidths[0], pWidths[1], pColors[0], pAlphas[0], pColors[1], pAlphas[1]);
		return;
	}

	bool bFlip = true;
	float flTex = 0;
	float flTotal = 0;

	gEngfuncs.pTriAPI->Begin(TRI_QUAD_STRIP);

	if (bTexAlong)
	{
		if (!bTexByLength)
		{
			flTotal = 1.0 / (float)iCount;
		}
		else
		{
			for (int i = 0; i < iCount - 1; i++)
			{
				const float flLen = (pPoints[i] - pPoints[i + 1]).Length();
				if (pLengths)
					pLengths[i] = flLen;
				flTotal = flLen + flTotal;
			}
		}
	}

	const int iLast = iCount - 1;
	Vector vecDir;
	for (int i = 0; i < iLast; i++)
	{
		vecDir = pPoints[i] - pPoints[i + 1];
		const Vector vecSide = Strip_Side(pPoints[i], vecDir);

		if (!bTexAlong)
		{
			Strip_Pair(pPoints[i], vecSide, pWidths[i], pColors[i], pAlphas[i], 0, 1, bFlip ? 0 : 1);
			bFlip = !bFlip;
		}
		else
		{
			Strip_Pair(pPoints[i], vecSide, pWidths[i], pColors[i], pAlphas[i], 0, 1, flTex);
			if (!bTexByLength)
				flTex = flTotal + flTex;
			else if (!pLengths)
				flTex = vecDir.Length() / flTotal + flTex;
			else
				flTex = pLengths[i] / flTotal + flTex;
		}
	}

	// the last point, across the last segment
	const Vector vecSide = Strip_Side(pPoints[iLast], vecDir);
	if (!bTexAlong)
		Strip_Pair(pPoints[iLast], vecSide, pWidths[iLast], pColors[iLast], pAlphas[iLast], 0, 1, bFlip ? 0 : 1);
	else
		Strip_Pair(pPoints[iLast], vecSide, pWidths[iLast], pColors[iLast], pAlphas[iLast], 0, 1, 1);

	gEngfuncs.pTriAPI->End();
}

// orig client 0x56240. As CLTE_DrawStrip, with a head point before the first. The texture's total is
// counted whether or not bTexAlong is set, and without bTexByLength is 1 / (iCount + 1)
void CLTE_DrawStripHead(Vector vecHead, float flHeadWidth, Vector vecHeadColor, float flHeadAlpha,
	Vector* pPoints, float* pWidths, Vector* pColors, float* pAlphas, int iCount,
	bool bTexAlong, bool bTexByLength, float* pLengths)
{
	if (iCount < 1)
		return;

	if (iCount == 1)
	{
		CLTE_DrawBeamQuad(vecHead, pPoints[0], flHeadWidth, pWidths[0], vecHeadColor, flHeadAlpha, pColors[0], pAlphas[0]);
		return;
	}

	bool bFlip = true;
	float flTex = 0;
	float flTotal = 0;
	float flHeadLen = 0;

	if (!bTexByLength)
	{
		flTotal = 1.0 / (float)(iCount + 1);
	}
	else
	{
		for (int i = 0; i < iCount - 1; i++)
		{
			const float flLen = (pPoints[i] - pPoints[i + 1]).Length();
			if (pLengths)
				pLengths[i] = flLen;
			flTotal = flLen + flTotal;
		}
		flHeadLen = (vecHead - pPoints[0]).Length();
		flTotal = flHeadLen + flTotal;
	}

	gEngfuncs.pTriAPI->Begin(TRI_QUAD_STRIP);

	const int iLast = iCount - 1;
	Vector vecDir;
	for (int i = -1; i < iLast; i++)
	{
		Vector vecPoint, vecColor;
		float flWidth, flAlpha;
		if (i == -1)
		{
			vecDir = vecHead - pPoints[0];
			vecPoint = vecHead;
			vecColor = vecHeadColor;
			flAlpha = flHeadAlpha;
			flWidth = flHeadWidth;
		}
		else
		{
			vecDir = pPoints[i] - pPoints[i + 1];
			vecPoint = pPoints[i];
			vecColor = pColors[i];
			flAlpha = pAlphas[i];
			flWidth = pWidths[i];
		}

		const Vector vecSide = Strip_Side(vecPoint, vecDir);

		if (!bTexAlong)
		{
			Strip_Pair(vecPoint, vecSide, flWidth, vecColor, flAlpha, 0, 1, bFlip ? 0 : 1);
			bFlip = !bFlip;
		}
		else
		{
			Strip_Pair(vecPoint, vecSide, flWidth, vecColor, flAlpha, 0, 1, flTex);
			if (!bTexByLength)
				flTex = flTotal + flTex;
			else if (i == -1)
				flTex = flHeadLen / flTotal + flTex;
			else if (!pLengths)
				flTex = vecDir.Length() / flTotal + flTex;
			else
				flTex = pLengths[i] / flTotal + flTex;
		}
	}

	const Vector vecSide = Strip_Side(pPoints[iLast], vecDir);
	if (!bTexAlong)
		Strip_Pair(pPoints[iLast], vecSide, pWidths[iLast], pColors[iLast], pAlphas[iLast], 0, 1, bFlip ? 0 : 1);
	else
		Strip_Pair(pPoints[iLast], vecSide, pWidths[iLast], pColors[iLast], pAlphas[iLast], 0, 1, 1);

	gEngfuncs.pTriAPI->End();
}

// orig client 0x57690. As CLTE_DrawStrip, closed: the last point joins the first, which is drawn
// again at the end across the average of its two segments. Nothing in client.dll calls it.
// Original bugs kept: with bTexByLength the closing segment counts twice and its length is written
// to pLengths[iCount], one past the points; without pLengths the first point's texture step is the
// length of the averaged direction. VERIFY: read from the disassembly only (no decompile)
void CLTE_DrawStripLoop(Vector* pPoints, float* pWidths, Vector* pColors, float* pAlphas, int iCount,
	bool bTexAlong, bool bTexByLength, float* pLengths)
{
	if (iCount < 2)
		return;

	if (iCount == 2)
	{
		CLTE_DrawBeamQuad(pPoints[0], pPoints[1], pWidths[0], pWidths[1], pColors[0], pAlphas[0], pColors[1], pAlphas[1]);
		return;
	}

	bool bFlip = true;
	float flTex = 0;
	float flTotal = 0;

	gEngfuncs.pTriAPI->Begin(TRI_QUAD_STRIP);

	if (bTexAlong)
	{
		if (bTexByLength)
		{
			for (int i = 0; i < iCount; i++)
			{
				const Vector vecSeg = (i < iCount - 1) ? pPoints[i] - pPoints[i + 1] : pPoints[i] - pPoints[0];
				const float flLen = vecSeg.Length();
				if (pLengths)
					pLengths[i] = flLen;
				flTotal = flLen + flTotal;
			}
			const float flLen = (pPoints[0] - pPoints[iCount - 1]).Length();
			if (pLengths)
				pLengths[iCount] = flLen;
			flTotal = flLen + flTotal;
		}
		else
		{
			flTotal = 1.0 / (float)(iCount + 1);
		}
	}

	for (int i = 0; i < iCount; i++)
	{
		Vector vecDir;
		if (i == 0)
			vecDir = (pPoints[iCount - 1] - pPoints[0]).Normalize() + (pPoints[0] - pPoints[1]).Normalize();
		else if (i < iCount - 1)
			vecDir = pPoints[i] - pPoints[i + 1];
		else
			vecDir = pPoints[i] - pPoints[0];

		const Vector vecSide = Strip_Side(pPoints[i], vecDir);

		if (bTexAlong)
		{
			Strip_Pair(pPoints[i], vecSide, pWidths[i], pColors[i], pAlphas[i], 0, 1, flTex);
			if (!bTexByLength)
				flTex = flTotal + flTex;
			else if (pLengths)
				flTex = pLengths[i] / flTotal + flTex;
			else
				flTex = vecDir.Length() / flTotal + flTex;
		}
		else
		{
			Strip_Pair(pPoints[i], vecSide, pWidths[i], pColors[i], pAlphas[i], 0, 1, bFlip ? 0 : 1);
			bFlip = !bFlip;
		}
	}

	const Vector vecDir = (pPoints[iCount - 1] - pPoints[0]).Normalize() + (pPoints[0] - pPoints[1]).Normalize();
	const Vector vecSide = Strip_Side(pPoints[0], vecDir);
	if (bTexAlong)
		Strip_Pair(pPoints[0], vecSide, pWidths[0], pColors[0], pAlphas[0], 0, 1, 1);
	else
		Strip_Pair(pPoints[0], vecSide, pWidths[0], pColors[0], pAlphas[0], 0, 1, bFlip ? 0 : 1);

	gEngfuncs.pTriAPI->End();
}

// orig client 0x58f90. A strip whose texture runs from flTexStart, flTexScale per unit of length
void CLTE_DrawStripScrolled(Vector* pPoints, float* pWidths, Vector* pColors, float* pAlphas, int iCount,
	float flTexScale, float flTexStart)
{
	if (iCount <= 1)
		return;

	float flTex = flTexStart;

	gEngfuncs.pTriAPI->Begin(TRI_QUAD_STRIP);

	const int iLast = iCount - 1;
	Vector vecDir;
	for (int i = 0; i < iLast; i++)
	{
		vecDir = pPoints[i] - pPoints[i + 1];
		const Vector vecSide = Strip_Side(pPoints[i], vecDir);
		Strip_Pair(pPoints[i], vecSide, pWidths[i], pColors[i], pAlphas[i], 0, 1, flTex);
		flTex = vecDir.Length() * flTexScale + flTex;
	}

	const Vector vecSide = Strip_Side(pPoints[iLast], vecDir);
	Strip_Pair(pPoints[iLast], vecSide, pWidths[iLast], pColors[iLast], pAlphas[iLast], 0, 1, flTex);

	gEngfuncs.pTriAPI->End();
}
