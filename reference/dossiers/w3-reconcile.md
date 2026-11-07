# w3-reconcile: the client packages c2-c5 pointed at each other's real code

The four client packages (c2, c3, c4, c5) were reconstructed in parallel, and each wrote stand-ins
under its own names for the others' functions, globals and classes. After the merge (main 0047e14)
most of those stand-ins still linked next to the real code, so their callers never reached it: no
c3 effect was ever created (the dispatcher called 36 factories returning NULL), tri.cpp drew the
effects through empty stubs, HUD_Frame and the HUD resets called empty stubs instead of the player
name label, the attribute menu, the pullers and the transform HUD, and so on. This pass gives every
original function, global and class one name, the real definition's, and points every caller at
it. Nothing new was reconstructed. Package c1 (client.dll below 0x4ec50) is still being
reconstructed elsewhere; its stubs stay.

Worktree ~/Projects/goldsrc.net/mod-rc2-wt/w3-reconcile (branch wt/w3-reconcile, from main 0047e14),
uncommitted. The scripts used are in ~/stignored/rc2-tools/w3/ (below).

## 1. Scope

- Every TEMP STUB block `scripts/merge/stub_addrs.py . cl_dll pm_shared` listed at 0047e14
  (reference/dossiers/w3-stubs-before.txt), plus the weak stubs without a marker (pm_client.cpp,
  studio_rc2_stubs.cpp) and the extern names those stubs served.
- Files: the 25 c3 effect files in cl_dll/rc2 (shield_tents.cpp to tcontroller_tents.cpp), c3_needs.h,
  c3_tents.h, c3_stubs.cpp; c2's clte.h, clte_types.h, clte.cpp, clte_particles.cpp,
  clte_effects4.cpp, attributes.cpp, cdll_int.cpp; c4's c4_needs.h, c4_stubs.cpp, input_stubs_V.cpp,
  studio_rc2.h, pm_client.cpp, transform_hud.cpp/.h, entity.cpp, hud_msg.cpp, input.cpp,
  inputw32.cpp, tri.cpp, StudioModelRenderer.cpp/.h, pm_shared/pm_shared.cpp (CLIENT_DLL block
  only); c5's view.cpp, vgui_TeamFortressViewport.cpp, rc2/vgui_DebugPanel.cpp,
  rc2/vgui_PlayerID.cpp.
- Shared headers: cl_dll/view.h (declarations of V_StartViewShake and V_StartScreenTint), cl_dll/hud.h
  (one inline accessor, CHudEnergy::GetEnergy). Nothing in dlls/ was touched.
- Deleted: cl_dll/rc2/studio_rc2_stubs.cpp (every stub in it had a real counterpart; git keeps it).
- Moved: the classes of CLTE 0x29, 0x39, 0x3b and 0x41 from their .cpp files into c3_tents.h (the
  dispatcher and Particle_Seek reach into them); CTentShield from clte_effects4.cpp into
  clte_types.h (shield_tents.cpp defines two of its methods).

## 2. Status

```
check                                                      result
scripts/build.sh i386 client                               ok
scripts/build.sh i386                                      ok
scripts/build.sh amd64                                     ok
check_coverage.py --lib client                             786 of 1466 (786 at 0047e14)
stub_addrs.py . cl_dll pm_shared dlls                      no TEMP STUBs left (section 5, after the c1 merge)
grep '^class CClientTent|^class CTentTrail|^class CRC2Trail'  one each (clte.h; CRC2Trail gone)
```

Built only. Nothing has run in an engine, so "the effects are created and drawn now" is what the
code says, not what anyone has seen.

## 3. What was unified (old names -> kept name, by original address)

Names are ours unless SDK 2.3's (client.dll keeps none for RC2's code). Where the original's
address belongs to a function another package wrote, that package's name was kept.

### The CLTE framework (c2's names win)

```
client      kept (file)                                 replaced
class       CClientTent (clte.h), 5 slots               c3's CClientTent (c3_needs.h, an ODR duplicate):
            Update(float)/Think()/DrawNormal/              Think(float)/Unused1/DrawNormal/
            DrawTransparent/KillEntity(int)                DrawTransparent/EntityGone(int)
inline      CLTE_FreeSlot (clte.h)                      RC2_FindFreeClientTent
0x5a6a0..   CTentTrail (clte.h, 0x28)                   CRC2Trail (c3_needs.h, a byte array and stubs)
0x5a720     CTentTrail::Free (new name for the          CRC2Trail::Free; ~CTentTrail now calls it
            destructor's body)
0x5ad60     CTentTrail::Reset                           CRC2Trail::Clear
0x5af10..   CTentParticles (clte.h, 0xc)                CRC2Particles
0x5b9e0     CTentParticles::HasAlive                    CRC2Particles::HasLive
typedef     pfnTrailPoint, pfnTentParticle              RC2TrailPointFunc, RC2ParticleFunc
0x508e0     Point_Darken                                RC2_TrailPointDrainToBlue
0x509c0     Particle_FadeToBlue                         RC2_ParticleTintBlue
0x50b90     Particle_Seek                               RC2_HoundeyeChargeParticle
0x539b0     CLTE_DrawCone                               RC2_DrawEllipsoidCap
0x53ea0     CLTE_DrawRing                               RC2_DrawEllipsoidBand
0x532f0     CLTE_AddDiskRing                            RC2_AddAnimatingDisk
0x54ad0     CLTE_DrawBeamQuad                           RC2_DrawBeamQuad
0x55190     CLTE_DrawStrip                              RC2_DrawStrip
0x58f90     CLTE_DrawStripScrolled                      RC2_DrawTexturedStrip
0x59ee0     CLTE_ShieldPoint                            RC2_ShieldIntercept
0x59be0     CLTE_GetOrigin                              RC2_GetEntityOrigin (c3, c5), GetEntityRenderOrigin (c4)
0x59b90     CLTE_SetOrigin                              RC2_SetEntityDrawOrigin (c4 renderer)
0x59d50     CLTE_SetOriginV                             PM_SetPredictedOrigin (c4 pm_shared)
0x598d0     CLTE_GetPlayerOverride                      RC2_GetEntityShellColor (c4 renderer)
0x52580     CLTE_DrawNormal                             ClientTents_DrawNormal (c4 tri.cpp)
0x52650     CLTE_DrawTransparent                        ClientTents_DrawTransparent (c4 tri.cpp)
0x52370     CLTE_Shutdown                               ClientTents_Clear (c4 input.cpp's HUD_Shutdown)
0x52280     CLTE_KillByEntity                           ClientTents_Remove (c4 transform_hud.cpp)
0x523f0     CLTE_Update                                 c4's weak stub (deleted)
0x510b0     __MsgFunc_CLTE                              c4's weak stub (deleted)
0x4f4f0     __MsgFunc_SetAttr                           c4's weak stub (deleted)
0x4f4c0     PM_GetAbilityFlags (attributes.cpp)         RC2_GetPlayerStateBits (c4 renderer)
CLTE 0x25   CTentShield (now clte_types.h)              c3's CShieldTent; c3's DrawTransparent (0x71ad0)
                                                        and Flash (0x727a0) are its methods now, c2's
                                                        empty DrawTransparent stub and c3's Think/
                                                        EntityGone stubs deleted
```

The shield's members, c3 -> c2 (same offsets, c2's names describe the shape: +0x28 is the
ellipsoid's half height, 40, +0x2c its radius, 20): m_iEntIndex -> m_iEntity, m_bBand -> m_bSweeping,
m_flBandWait -> m_flSweepWait, m_flBandZ -> m_flSweepZ, m_pSprite -> m_pNoise, m_pBandSprite ->
m_pLightning, m_flRadius (+0x28) -> m_flHeight, m_flHeight (+0x2c) -> m_flWidth, m_flBandHalf ->
m_flSweepMargin, m_flBandFrame -> m_flFrame.

### The dispatcher and c3's factories

clte.cpp's 36 NULL-returning `CLTE_CreateType26..49` (and `CLTE_CreateRoachWalk`) are gone; each
case calls c3's factory for that type, `RC2_New<Name>Tent` (c3_tents.h; the CLTE type and the
factory's address are on each declaration, 0x721b0 for 0x26 to 0x95c20 for 0x49). There are 36
types, 0x26-0x49 (c3's dossier says 37).

clte_types.h's partial classes (CTentType29, CTentType39, CTentRoachWalk, CTentType41) were replaced
by c3's real ones, moved into c3_tents.h. Every member the framework touches, at the offsets both
packages read from the code:

```
CLTE  class                 c2's partial name -> c3's (offset)
0x29  CEgonTractorTent      m_iEntity -> m_iEntIndex (+8), m_iValue -> m_iHeld (+0xc), m_pBeam -> m_pLightning
                            (+0x10), m_pBeam2 -> m_pLaser (+0x14), m_vecEnd (+0x1c), m_iLaserModel (+0x28)
0x39  CTripmineChargeTent   m_iEntity -> m_iEntIndex (+8), m_iActive -> m_flCharge (+0x14; CLTE 0x3a writes 0,
                            the same bits as int or float)
0x3b  CRoachWalkTent        m_iEntity -> m_iEntIndex (+8), m_vecOrigin (+0x14), m_pSmokeModel -> m_pTrailSprite
                            (+0x20), m_Trail (+0x24), m_pBeam (+0x4c), m_bArmed (+0x50), m_flArmTimeLeft
                            (+0x54), m_flArmTime (+0x58); c2 had the two arm times swapped, harmless since
                            the dispatcher writes both with the same value
0x41  CHoundeyeChargeTent   Particle_Seek's m_vecTarget (+0x18) -> m_vecOrigin (+0x18)
```

### Other functions

```
client      kept (file)                                 replaced
0x9e9a0     GetEntity (ev_common.cpp)                   RC2_GetEntity (c3)
0xa9fd0     CL_IsThirdPerson (in_camera.cpp)            CAM_IsThirdPerson (c3; returned bool, the real one int)
0xe9ca0     V_StartViewShake (view.cpp)                 RC2_ViewShake (c3)
0xec590     V_StartScreenTint (view.cpp)                V_StartColorFade (c3)
0xb0e50     VectorAnglesWithRoll (pm_math.cpp)          RC2_BasisToAngles (c5 view.cpp)
0xb07d0     AngleBetweenVectors (pm_math.cpp)           c5's stub in view.cpp
0x4fe80     RC2_ResetClientState (cdll_int.cpp; no      RC2_InitHUD (c4 hud_msg.cpp, CHud::MsgFunc_InitHUD
            longer static)                              calls 0x4fe80 too)
0xbd0e0     PM_ResetPullers (pullers_client.cpp)        RC2_ResetEntityTables (c2 cdll_int.cpp)
0xd9150     PlayerID_Enable (vgui_PlayerID.cpp)         RC2_ResetViewportFlags (c2)
0xcfe90     AttrMenu_Init (vgui_attributes.cpp)         RC2_ResetPendingMenu (c2)
0xcb1c0     TransformHud_VidInit (transform_hud.cpp)    RC2_LoadCrosshairs (c2)
0xd8980     PlayerID_Frame (vgui_PlayerID.cpp)          RC2_ViewportFrame (c2)
0xcfe10     AttrMenu_Think (vgui_attributes.cpp)        RC2_RunPendingMenu (c2)
0xd9060     PlayerID_AddEntity (vgui_PlayerID.cpp)      DebugPanel_AddEntity (c4 entity.cpp; it passed
                                                        modelname too, which 0xd9060 never reads)
0xd9180     PlayerID_Disable (vgui_PlayerID.cpp)        VGui_ShutdownState (c4 input.cpp)
0xd6b00     DrawDebugEntityBox (vgui_DebugPanel.cpp)    RC2_DrawTransparentExtra (c4 tri.cpp)
0xcf480     AttrMenu_Create (vgui_attributes.cpp)       CreateAttributeMenu (c5 viewport)
0xcfec0     AttrMenu_Forget (vgui_attributes.cpp)       ResetAttributeMenu (c5 viewport)
0xaf980     PM_GetRideInfo (pm_client.cpp)              RC2_GetPlayerAttachment (c5 vgui_PlayerID.cpp)
0xc9e00,    RC2_TempEntThink, RC2_HudRedraw             c4's weak stubs (deleted)
0xca800     (transform_hud.cpp)
(inline)    gHUD.m_Energy.GetEnergy() (hud.h)           HUD_GetEnergy (c2 attributes.cpp). 0x9cdc0 is
                                                        CHudEnergy::Init; PM_GetEnergy (0x4f680) reads
                                                        gHUD+0x204 (m_Energy +0x18) inline
-           the viewport's ToggleServerBrowser,         c4's weak stubs (deleted; c5's are real)
            MsgFunc_AttrConf/VoteBegin/VoteUpdate,
            SetDebugPanelVisible, MsgFunc_DBGCounts/
            DBGEntInfo, HideDebugPanels
```

### Globals (one definition each, in the file whose code owns them)

```
address            kept (defined in)                    replaced
.data 0x101a9598   g_iClientDetail (cdll_int.cpp)       g_iClDetail (c3), g_iDetailLevel (c4)
.bss 0x10206918    g_iHighestEntIndex (entity.cpp;      g_iMaxPlayerEnt (c2 stub, c3), g_iMaxEntityIndex
                   HUD_AddEntity updates it)            (c5 stub), c4's weak one
.bss 0x101f24c4    g_bEntityFrozen[1024] (clte.cpp)     s_iTentTable1[256] ints (c2), g_bEntityHidden[2048] (c4)
.bss 0x101edc7c    g_abRC2AntiFriction[1024] (clte.cpp) s_iTentTable2 (c2), g_rgbEntAftexShell (c4)
.bss 0x101f69f8    g_iLocalPlayerFrozen (clte.cpp)      s_iTentState1 (c2), g_bViewLockTent (c4),
                                                        g_bScreenTintBlue (c5)
.bss 0x101f69fc    g_iRC2LocalAntiFriction (clte.cpp)   s_iTentState2 (c2), g_bScreenTintMagenta (c5)
.bss 0x101f6a00    g_bLocalTransforming (clte.cpp)      s_iTentState3 (c2), g_bViewLockEffect (c4),
                                                        g_bViewOverride (c5)
.bss 0x101f6a08    g_iClientFrame (clte.cpp)            g_iRC2FrameCount (c4 renderer)
.data 0x1020bb28   cl_debuggren (hud.cpp)               g_pClDebugGren (c3)
.data 0x101768b8   g_bMainView (view.cpp)               g_bDrawClientTents (c4 tri.cpp)
.bss 0x10227991    g_bViewEntityOverride (view.cpp,     g_bViewChasing (c4 renderer)
                   no longer static)
.bss 0x10227160    g_iViewLock (view.cpp)               g_iPMSpecView (c4 pm_shared)
.bss 0x10227164    g_bLockViewOrigin (view.cpp)         g_bPMSetViewOrigin (c4 pm_shared)
.bss 0x102270f0    g_bLockViewAngles (view.cpp)         g_bPMSetViewAngles (c4 pm_shared)
.bss 0x10227084    g_vecLockedViewOrigin (view.cpp)     pm_shared's v_origin (c4's VERIFY: the original's
                                                        0xb6ef0 reads and writes 0x10227084; view.cpp's
                                                        v_origin is 0x10226818)
.bss 0x10226848    g_vecLockedViewAngles (view.cpp)     pm_shared's v_angles (v_angles is 0x10227148)
.bss 0x1020be40    cam_blocked (in_camera.cpp)          g_bThirdPersonBlocked (c5 view.cpp)
.bss 0x10220c3c    g_iPlayerClass (stock)               g_iPlayerViewState (c5)
.bss 0x10220c4c    g_iBatPerch (transform_hud.cpp)      g_iPlayerViewSubState (c5)
.bss 0x10220c40    g_bHoundeyeAsleep (transform_hud.cpp) g_bPlayerViewBusy (c5)
.bss 0x10220c48    g_flHoundeyeSleepFade (transform_hud  s_flHoundeyeSleepFade (c4, was static),
                   .cpp, now global)                     g_flScreenFlash (c5)
.bss 0x1022fe30    g_vecRaceViewAngles (pm_shared.cpp)  g_vecFixedViewAngles (c5)
.data 0x1013cfa0   g_iAlive (stock, entity.cpp)         g_bTiltBodyWithView (c5; HUD_TxferPredictionData
                                                        sets it, CL_CreateMove and the view read it)
.bss 0x10220c44    g_bScopeView (view.cpp, real now)    c5's stub; only V_CalcNormalRefdef touches it,
                                                        though it lies among transform_hud.cpp's statics
```

How the c5 view.cpp externs were tied to addresses (c5's stub block named none): the globals the
original's V_CalcNormalRefdef (0xe9ee0), V_StartScreenTint (0xec590) and V_UpdateScreenTint
(0xec6c0) read at the places c5's code tests them (~/stignored/rc2-tools/w3/globrefs.py over
0xe9400-0xecd00, then xrefs.py for each address's other readers and writers). E.g. the third
person pitch test at 0xeac46 reads 0x1013cfa0, 0x101f69f8, 0x101f6a00, 0x10220c40 in c5's order; the
view state switch at 0xeac92 reads 0x10220c3c (6, then 4 with 0x10220c4c == 2) and copies
0x1022fe30.

What each table means, from who writes it: 0x101f24c4 is set by CLTE 0x2f (the frozen effect,
0x7bff0) and cleared by its KillEntity (0x7c1f0), so "frozen" (the renderer's white shell and the
skipped studio events are the frozen look); 0x101edc7c is set and cleared by CLTE 0x35 (0x83520,
0x83720), the antifriction effect whose look is the magenta aftex.spr shell. The local-player words
are set by the frozen (0x7bff0), antifriction (0x83520) and transform (0x8ebf0) effects and read by
input (view locks) and the view (tints, no pitch).

## 4. Signature differences and how each was settled

- **CLTE_DrawBeamQuad's 4th argument** (c3 `int iUnused`, c2 `float flEndWidth`): c2's. The original's
  strip drawers 0x55190 and 0x56240 pass the next point's width there, and the body scales the end
  vertices' side vector by it. c3's only caller, the houndeye's DrawTransparent (0x91290), pushes 0
  (`push 0` before `push 0x40400000` at 0x913c7), which is 0.0f, so it now passes 0.0f.
- **CLTE_AddDiskRing's last three** (c3 ints, c2 floats): c2's floats. 0x532f0 copies them into the
  disk untyped (`mov`), and the draw pass loads the first with `fld dword ptr [esi - 0xc]`
  (0x5274a), a float frame. Every caller in the tree passes 0, 0, 0 (bit-identical either way).
- **CLTE_ShieldPoint's hit flag** (c3 `int*`, c2 `bool*`): c3's `int*`. 0x59ee0 writes it as a dword
  (`mov dword ptr [ebx], 0` at 0x59ef4, `mov dword ptr [ebx], 1` at 0x59f39). c2's declaration and
  body changed to int.
- **The point callbacks' user value** (c3 `int iUser`, c2 `void* pUser`): c2's. The callers pass 0
  (a null pointer constant) or, for the houndeye (0x41), the effect itself, which Particle_Seek reads
  as the effect; that removes c3's VERIFY about the pointer truncated to an int on 64-bit.
- **V_StartColorFade(Vector, Vector, float, float, float, int) vs V_StartScreenTint(10 floats + int)**:
  the same stack layout under VC6 (a Vector by value is three floats on the stack; the call at
  0x99027 copies two Vectors onto it). The three call sites now pass the six colour floats.
- **CAM_IsThirdPerson (bool) vs CL_IsThirdPerson (int)**: the int one; c3's uses are tests.
- **RC2_SetEntityDrawOrigin(int, Vector) vs CLTE_SetOrigin(int, float, float, float)**: the same
  layout under VC6; the renderer passes the three components.
- **GetEntityRenderOrigin(float* out, int) vs Vector CLTE_GetOrigin(int)**: c4 read the hidden return
  pointer of a Vector returned by value as an argument; the caller takes the Vector.
- **DebugPanel_AddEntity(type, ent, modelname) vs PlayerID_AddEntity(type, ent)**: 0xd9060 reads only
  two arguments ([esp+0x18], [esp+0x1c] after its pushes); HUD_AddEntity's third push is dead.
- **AngleBetweenVectors**: c5's stub computed acos(dot / (l1 l2)); the original's 0xb07d0 is SDK 2.3's
  acos(dot) / (l1 l2) (`call acos` at 0xb0828, then `fdivp` by the lengths). pm_math.cpp's (the
  original's) is kept, so V_CalcSpectatorRefdef's smoothing test now has the original's bug.
  mathlib.h (halflife-updated's) declares a `const float*` version nothing defines; view.cpp
  declares the `const Vector&` one it calls.
- **Free() (0x5a720)**: CTentTrail's destructor body. The original calls 0x5a720 explicitly too, and
  effects are deleted through CClientTent* without a virtual destructor, so member destructors never
  run; Free() is that body under a name, ~CTentTrail calls it.

Behavior that changes because calls now reach real code (all intended): c3's effects are created,
updated and drawn; the shield draws and flashes; tri.cpp draws the effects and the debug box;
HUD_Shutdown frees the effects and turns the player name label off; HUD_Frame runs the player name
label and the attribute menu; the HUD resets clear the pullers, enable the name label and clear the
pending menu; HUD_VidInit loads the transform HUD sprites; InitHUD runs RC2_ResetClientState; the
viewport creates and forgets the real attribute menu; the renderer sees frozen and antifriction
entities, the frame counter, the view-entity override, the player override shell and the jetpack bit;
pm_shared's spectator move writes the locked view origin and angles view.cpp reads, and its
predicted origin reaches the effects' origin cache; the view's locks and tints follow the frozen,
antifriction, transforming and houndeye-asleep state; PM_GetEnergy returns the HUD's energy instead
of 100.

## 5. After c1 merged: the last stubs

c1 merged into main while this pass ran, so its stand-ins for c2-c5 code and the c2-c5 stubs for
c1's code were reconciled at the merge, the same way (main ca6fd79 + this branch). No TEMP STUB is
left anywhere in the tree, and no `__attribute__((weak))` either.

```
client           kept (file)                               replaced
0x3b630          UTIL_RandomSpreadDir (hl/hl_weapons.cpp)  RC2_SpreadVector (c3)
0xdd80           EV_WaterLevel (rc2/ev_helpers.cpp)        RC2_WaterLevel (c3)
0xdd40           EV_PlayerForEntity (rc2/ev_helpers.cpp)   EV_RideTarget (c2)
0x3b400, 0x3b560 UTIL_PackNormal, UTIL_ReflectVector       c2's copies (dropped at the c1 merge)
                 (hl/hl_weapons.cpp)
0x4a900          CHudAmmo::RestoreCrosshair (ammo.cpp)     Ammo_RestoreCrosshair (c4)
0xee70           EV_PenetrateThickness (ev_hldm.cpp)       EV_TraceVisible (c4; its target is the
                                                           cl_entity_t*, whose first member, index, is
                                                           what 0xee70 compares, so &pEnt->index)
0x49110          CVoiceStatus::IsPlayerAudible             c5's IsPlayerVoiceEnabled, which read
                 (voice_status.cpp)                        m_VoiceEnabledPlayers; 0x49110 reads this+0x28,
                                                           m_AudiblePlayers in SDK 2.3's layout
0x3b900          __MsgFunc_WeaponSync (hl/hl_weapons.cpp)  c4's weak stub
0x4f4c0          PM_GetAbilityFlags (rc2/attributes.cpp)   RC2_GetPlayerAbilities (c1)
0xaf980          PM_GetRideInfo (rc2/pm_client.cpp)        RC2_GetPlayerRiding (c1)
0xec590          V_StartScreenTint (view.cpp)              c1's V_ScreenTint, now an inline front end in
                                                           ev_hldm.cpp (Vectors for the six colour floats)
.data 0x1a8004   gHUD.m_Energy.GetEnergy()                 g_flRC2LocalEnergy (c1); 0x4f698 in PM_GetEnergy
                                                           reads the same address
.bss 0x101f69f8  g_iLocalPlayerFrozen                      g_iRC2LocalFrozen (c1)
.data 0x101a9598 g_iClientDetail                           g_iDetailLevel (c1's events)
-                the CLTE_* functions, cl_gibcount,        c1's weak stubs; CLTE_ShieldPoint's took bool*,
                 CL_SpawnGib, g_flTransformFOV             so c1's callers bound to it: their flags are int
```

On the server, dlls/rc2prefs.cpp's IsTempAdmin stub was dead (its caller already uses
Admin_IsTempAdmin, maintainent.cpp) and is gone.

## 6. VERIFY list

None added. Resolved and removed (20 markers): c2's on the four .bss words and the two tables
CLTE_Reset clears (their readers and setters are now known, section 3), on the unknown members of
c3's types (replaced by c3's classes), on what raises the shield's flash (Flash, 0x727a0); c3's on the
view shake's parameter meanings (view.cpp's V_StartViewShake), on the houndeye passing itself as an
int, on 0x59ee0 being read only to its entry; c4's on g_bEntityHidden's name and size, on what 0xd6b00
draws, on pm_shared's v_origin/v_angles (they are the locked view origin and angles).

Open, not this pass's (noted for whoever owns them): hud_spectator.cpp keeps halflife-updated's
bodies of SetModes (0xa70b0) and DrawOverviewEntities (0xa81b0) (c4's VERIFY 6); the original's read
0x10227084/0x10226848, view.cpp's g_vecLockedViewOrigin/Angles, where halflife-updated's read
v_origin/v_angles.

## 7. Behavior worth testing

Every c3 effect (CLTE 0x26-0x49) now appears for the first time, and the shield (0x25) draws: the c3
dossier's list applies. Also: the player name label under the crosshair (HUD_Frame); the attribute
menu from the viewport; thirdperson blocked by walls (cam_blocked now shared by the view and
CL_IsThirdPerson); the blue tint while frozen, magenta under antifriction, black during the
houndeye's sleep; spectator in-eye (pm_shared now writes the locked view origin and angles).

## 8. Original bugs

- HUD_StudioEvent and the renderer index the 1024-byte frozen table (and the renderer the
  antifriction one) by entity index unchecked; the original reads its neighbouring table above 1023.
  Kept (c4 had made its copy 2048 bytes).
- V_CalcSpectatorRefdef's turn test uses the original's AngleBetweenVectors, acos(dot) / (l1 l2)
  (section 4). Kept.

## Helper scripts (~/stignored/rc2-tools/w3/)

- rdis.py, callers.py (copied from ../c5, pointed at this worktree): annotated disassembly with the
  thunks followed; callers of an address.
- globrefs.py <lo> <hi>: every .data/.bss address the original's functions in a range touch.
- xrefs.py <va>...: every instruction naming an address.
- rename_c3.pl, cut_c3_needs.py, move_classes.py, reconcile_c4c5.py, reconcile_view.py: the edits of
  this pass, in the order run (one-shot: they match exact text of 0047e14 and stop on a mismatch).
