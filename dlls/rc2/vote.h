/***
*
*	Rocket Crowbar 2: the players' votes (dlls/rc2/vote.cpp). Names ours.
*
****/

#pragma once

void ResetVote();						 // no vote running (map start)
void StopVote();						 // ends a running vote (the admin command)
void VoteThink();						 // every frame (StartFrame)
void ClientVote(edict_t* pEdict);		 // the client command "vote yes|no"
void ClientVoteMap(edict_t* pEdict);	 // "votemap"
void ClientVoteKick(edict_t* pEdict);	 // "votekick"
