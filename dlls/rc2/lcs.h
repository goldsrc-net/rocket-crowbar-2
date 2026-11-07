/***
*
*	Rocket Crowbar 2: level customization scripts, maps/lcs/<map>.lcs (lcs.cpp). The ancestor
*	is RC 1.9's .lec system (mod-rockcrowbar dlls/levelcustom.cpp).
*
*	A script is one command per line, '#' starting a comment line:
*	  create <classname> {x y z} {pitch yaw roll}         spawn it once
*	  respawncreate <classname> {x y z} {pitch yaw roll}  spawn it, and again whenever it dies
*	  advcreate <classname> <alteration arguments>        spawn it and apply the alterations
*	  removeall / killall / useall <classname>            every entity of that class ('*'
*	                                                      before a prefix matches the prefix)
*	  removeent / killent / useent <entity index>
*	  edit <entity index> <alteration arguments>          alter it once
*	  maintain <entity index> <alteration arguments>      keep the alteration applied
*	  setspawncommand <alteration arguments>              (written as "setspawncmd", see below)
*	Multiplayer only. The creates are precached in pass 0 (world precache), everything runs in
*	pass 1 (three seconds into the map). The in-game command (LCS_Command) appends lines.
*
****/

#pragma once

// orig 0x6d4d0: run the current map's script; iPass 0 precaches what it creates, 1 executes.
// Called by CWorld (pass 0 from Precache, pass 1 from a timedevent 3 s on). Name ours.
void LCS_Load(int iPass);

// orig 0x6db00: the client command that writes script lines: the subcommand is CMD_ARGV(iArg),
// pEntity the player typing it (a client.cpp command calls it with iArg 2). Name ours.
void LCS_Command(int iArg, edict_t* pEntity);
