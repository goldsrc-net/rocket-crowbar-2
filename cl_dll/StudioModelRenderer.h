//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#pragma once

/*
====================
CStudioModelRenderer

====================
*/
class CStudioModelRenderer
{
public:
	// Construction/Destruction
	CStudioModelRenderer();
	virtual ~CStudioModelRenderer();

	// Initialization
	virtual void Init();

public:
	// Public Interfaces
	virtual bool StudioDrawModel(int flags);
	virtual bool StudioDrawPlayer(int flags, struct entity_state_s* pplayer);

public:
	// Local interfaces
	//

	// Look up animation data for sequence
	virtual mstudioanim_t* StudioGetAnim(model_t* m_pSubModel, mstudioseqdesc_t* pseqdesc);

	// Interpolate model position and angles and set up matrices
	virtual void StudioSetUpTransform(bool trivial_accept);

	// Set up model bone positions
	virtual void StudioSetupBones();

	// Find final attachment points
	virtual void StudioCalcAttachments();

	// Save bone matrices and names
	virtual void StudioSaveBones();

	// Merge cached bones with current bones for model
	virtual void StudioMergeBones(model_t* m_pSubModel);

	// Determine interpolation fraction
	virtual float StudioEstimateInterpolant();

	// Determine current frame for rendering
	virtual float StudioEstimateFrame(mstudioseqdesc_t* pseqdesc);

	// Apply special effects to transform matrix
	virtual void StudioFxTransform(cl_entity_t* ent, float transform[3][4]);

	// Spherical interpolation of bones
	virtual void StudioSlerpBones(vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s);

	// Compute bone adjustments ( bone controllers )
	virtual void StudioCalcBoneAdj(float dadt, float* adj, const byte* pcontroller1, const byte* pcontroller2, byte mouthopen);

	// Get bone quaternions
	virtual void StudioCalcBoneQuaterion(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* q);

	// Get bone positions
	virtual void StudioCalcBonePosition(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* pos);

	// Compute rotations
	virtual void StudioCalcRotations(float pos[][3], vec4_t* q, mstudioseqdesc_t* pseqdesc, mstudioanim_t* panim, float f);

	// Send bones and verts to renderer. RC2: bShellColor lets a weapon model take the
	// player's coloured shell (RC2_GetEntityShellColor)
	virtual void StudioRenderModel(bool bShellColor);

	// Finalize rendering
	virtual void StudioRenderFinal();

	// RC2 (vtable slot 20, name ours): the extra shells drawn over a model
	virtual void StudioRenderShells();

	// GL&D3D vs. Software renderer finishing functions
	virtual void StudioRenderFinal_Software();
	virtual void StudioRenderFinal_Hardware();

	// Player specific data
	// Determine pitch and blending amounts for players
	virtual void StudioPlayerBlend(mstudioseqdesc_t* pseqdesc, int* pBlend, float* pPitch);

	// Estimate gait frame for player
	virtual void StudioEstimateGait(entity_state_t* pplayer);

	// Process movement of player
	virtual void StudioProcessGait(entity_state_t* pplayer);

	// RC2 (vtable slots 26-31, names ours): the transformed players (playerclass 1-5) are drawn
	// with their monster's model. iMode is the playerclass; flYaw turns the assassin's spine.
	virtual void StudioSetupTransformBones(int iMode, entity_state_t* pplayer, float flYaw);
	virtual bool StudioDrawAssassin(int flags, entity_state_t* pplayer);
	virtual bool StudioDrawHoundeye(int flags, entity_state_t* pplayer);
	virtual bool StudioDrawController(int flags, entity_state_t* pplayer);
	virtual bool StudioDrawStukabat(int flags, entity_state_t* pplayer);
	virtual bool StudioDrawHeadcrab(int flags, entity_state_t* pplayer);

private:
	// What every transformed player's draw does after its own gait and angles (name ours; not a
	// function of the original, which repeats it in each)
	bool StudioDrawTransformFinish(int iMode, int flags, entity_state_t* pplayer, float flYaw);

public:
	// Client clock
	double m_clTime;
	// Old Client clock
	double m_clOldTime;

	// Do interpolation?
	bool m_fDoInterp;
	// Do gait estimation?
	bool m_fGaitEstimation;

	// Current render frame #
	int m_nFrameCount;

	// Cvars that studio model code needs to reference
	//
	// Use high quality models?
	cvar_t* m_pCvarHiModels;
	// Developer debug output desired?
	cvar_t* m_pCvarDeveloper;
	// Draw entities bone hit boxes, etc?
	cvar_t* m_pCvarDrawEntities;

	// The entity which we are currently rendering.
	cl_entity_t* m_pCurrentEntity;

	// The model for the entity being rendered
	model_t* m_pRenderModel;

	// Player info for current player, if drawing a player
	player_info_t* m_pPlayerInfo;

	// The index of the player being drawn
	int m_nPlayerIndex;

	// The player's gait movement
	float m_flGaitMovement;

	// RC2: +0x44 in the original, between m_flGaitMovement and m_pStudioHeader; no code in
	// client.dll reads or writes it (VERIFY)
	int m_iRC2Unused44; // name ours

	// Pointer to header block for studio model data
	studiohdr_t* m_pStudioHeader;

	// Pointers to current body part and submodel
	mstudiobodyparts_t* m_pBodyPart;
	mstudiomodel_t* m_pSubModel;

	// Palette substition for top and bottom of model
	int m_nTopColor;
	int m_nBottomColor;

	//
	// Sprite model used for drawing studio model chrome
	model_t* m_pChromeSprite;

	// RC2: sprites for the shells and the models of the jetpack and the transforms, loaded in
	// Init and again when missing (all names ours)
	model_t* m_pWhiteSprite;	 // +0x60 sprites/white.spr
	model_t* m_pAftexSprite;	 // +0x64 sprites/aftex.spr
	model_t* m_pJetpackModel;	 // +0x68 models/jetpack.mdl
	model_t* m_pAssassinModel;	 // +0x6c models/hassassin.mdl
	model_t* m_pHoundeyeModel;	 // +0x70 models/houndeye.mdl
	model_t* m_pControllerModel; // +0x74 models/controller.mdl
	model_t* m_pStukabatModel;	 // +0x78 models/stukabat.mdl
	model_t* m_pHeadcrabModel;	 // +0x7c models/headcrab.mdl

	// Caching
	// Number of bones in bone cache
	int m_nCachedBones;
	// Names of cached bones
	char m_nCachedBoneNames[MAXSTUDIOBONES][32];
	// Cached bone & light transformation matrices
	float m_rgCachedBoneTransform[MAXSTUDIOBONES][3][4];
	float m_rgCachedLightTransform[MAXSTUDIOBONES][3][4];

	// Software renderer scale factors
	float m_fSoftwareXScale, m_fSoftwareYScale;

	// Current view vectors and render origin
	float m_vUp[3];
	float m_vRight[3];
	float m_vNormal[3];

	float m_vRenderOrigin[3];

	// RC2: the model origin StudioSetUpTransform computes (a local in SDK 2.3), +0x40bc; the
	// draws hand it to RC2_SetEntityDrawOrigin
	Vector m_vecModelPos; // name ours

	// Model render counters ( from engine )
	int* m_pStudioModelCount;
	int* m_pModelsDrawn;

	// Matrices
	// Model to world transformation
	float (*m_protationmatrix)[3][4];
	// Model to view transformation
	float (*m_paliastransform)[3][4];

	// Concatenated bone and light transforms
	float (*m_pbonetransform)[MAXSTUDIOBONES][3][4];
	float (*m_plighttransform)[MAXSTUDIOBONES][3][4];
};
