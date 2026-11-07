/***
*
*	Rocket Crowbar 2 client, work package c3: what c3's effect files (CLTE 0x26-0x49, the second
*	half of client.dll's run "new: cdll_int .. com_weapons") use from other files. The effect
*	framework is c2's (clte.h: CClientTent, the effect table, the draw helpers, CTentTrail and
*	CTentParticles, the point callbacks, the shared entity state; clte_types.h: the shield),
*	the view's shake and screen tint view.cpp's (view.h), GetEntity ev_common.cpp's and
*	CL_IsThirdPerson in_camera.cpp's. What is still nobody's has a stand-in in c3_stubs.cpp.
*	reference/dossiers/c3.md and w3-reconcile.md list them.
*
****/

#pragma once

#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "com_model.h"
#include "r_efx.h"
#include "event_api.h"
#include "triangleapi.h"
#include "parsemsg.h"
#include "Exports.h"
#include "eventscripts.h"
#include "view.h"
#include "clte.h"
#include "clte_types.h"

#include <cstdio>

// pm_math.cpp (stock)
void VectorAngles(const float* forward, float* angles);

// orig client 0x3b630 (cl_dll/hl/hl_weapons.cpp): vecDir spread by up to flSpread along vecRight
// and vecUp, normalized
Vector UTIL_RandomSpreadDir(Vector vecDir, Vector vecRight, Vector vecUp, float flSpread);

//
// Compiler-generated copy constructors the original emits out of line at the end of this run
// (orig 0x998f0-0x9b430), used by the factories' copy path (a factory given a non-null pCopy;
// the dispatcher never does that). In C++ they are the implicit copy constructors, so there is
// nothing to write; the effects of c3 note theirs at their factories. These six are of classes
// that aren't c3's:
//
// orig client 0x998f0: c2's CLTE 0x0d effect (factory 0x62d10, vtable 0x1011da14)
// orig client 0x999b0: c2's CLTE 0x10 effect (factory 0x650a0, vtable 0x1011dacc)
// orig client 0x99aa0: c2's CLTE 0x12 effect (factory 0x670b0, vtable 0x1011db30)
// orig client 0x99bb0: c2's CLTE 0x14 effect (factory 0x689b0, vtable 0x1011db74)
// orig client 0x99c30: c2's CLTE 0x1d effect (factory 0x6c600, vtable 0x1011dc78)
// orig client 0x9b430: CClientTent's own (vtable 0x1011d84c, m_iType), called by the inline
//                      copy constructors of c3's CLTE 0x31 and 0x46 effects
