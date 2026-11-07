/***
*
*	Rocket Crowbar 2: the expression side of the entity alteration language that the LCS
*	lines advcreate, edit and maintain (and RC2's spawn command) use (alteration.cpp). The
*	key=value handling that calls these is p1's (the callbacks around 0x1670).
*
****/

#pragma once

// orig 0x71c00: removes whitespace from psz in place. Name ours. See alteration.cpp for the
// original's off-by-one, which this keeps.
void ALT_StripSpaces(char* psz);

// orig 0x71b90: the inside of psz's first chOpen..chClose group (nesting counted), cut out
// in place (both delimiters become NULs), or NULL when the group isn't closed. Name ours.
char* ALT_FindEnclosed(char* psz, char chOpen, char chClose);

// orig 0x71c60: a float expression. Name ours.
float ALT_EvalFloat(const char* pszExpr);

// orig 0x72a90: a vector expression. Name ours.
Vector ALT_EvalVector(const char* pszExpr);

// orig 0x73fb0: pfnArg(index, arg) for each whitespace-separated argument of pszLine, quotes,
// [], () and {} keeping their contents together. Name ours.
void ALT_SplitArgs(const char* pszLine, void (*pfnArg)(int iIndex, char* pszArg));
