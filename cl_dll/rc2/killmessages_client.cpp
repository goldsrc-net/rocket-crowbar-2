/***
*
*	Rocket Crowbar 2: client.dll has the server's death message table and its fill-in code
*	compiled in as well (dlls/rc2/killmessages.cpp: the table at client .data 0x137a58 is the
*	server's line for line, and 0x9c3a0-0x9c990 are the server's 0x320a0-0x32540 instruction for
*	instruction). The death notice (death.cpp) prints RC2's message with it.
*
*	Built through this file because cl_dll/CMakeLists.txt isn't a work package's to edit; the
*	merge can list ../dlls/rc2/killmessages.cpp among the client's sources instead
*	(reference/dossiers/c4.md, "Cross-file needs").
*
****/

#include "../../dlls/rc2/killmessages.cpp"
