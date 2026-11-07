/***
*
*	Rocket Crowbar 2: the expressions of the entity alteration language (see alteration.h).
*
*	An expression is stripped of whitespace and cut at its first '=', and the inside of its
*	first (...) group (float) or {...} group (vector) is what is evaluated, so a group is only
*	useful around a whole expression. There is no precedence: an expression splits at its
*	first operator outside groups and both sides are evaluated on their own, left to right.
*
*	Float:   a number; {vector}.x .y .z .l(ength); a + - * / ^ b; a! (sine of a turns);
*	         p[N].field and e[N].field (player N, 0-99; entity N, 0-9999; without N the player
*	         and entity the alteration is for); t (time), f (frametime), r (random 0..1),
*	         s (random -1..1). A '-' only splits when a nonzero number follows it.
*	         player fields: a(ngles) v(elocity, .l) o/p(osition) f(orward) r(ight) u(p),
*	         each .x .y .z; entity fields add m(ovedir, .l) and c(enter).
*	Vector:  x,y,z (floats); a + - b; a * / b (b a {vector}: per component, else a float;
*	         a zero divisor leaves that component); a^ (the forward vector of angles a);
*	         a& (the angles of vector a); a! (normalized, (0,0,1) for zero); pN./eN. fields
*	         (player: v m o/p a e(ye offset) f r u, v and m with .n to normalize; entity:
*	         v o/p c a e f r u); r (random components), rn (random direction); and of the
*	         alteration's player t (where its view trace hits), o (eye position), v (velocity).
*
*	In the original this code sits between maprules.cpp and monstermaker.cpp in link order
*	(the key=value side that uses it is p1's); the file name is ours.
*
****/

#include <ctype.h>
#include <math.h>
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "maintainent.h"
#include "alteration.h"

// orig 0x71c00
void ALT_StripSpaces(char* psz)
{
	// After each removal the loop moves on without looking at the character that took the
	// space's place, and it clears psz[len-2] where it means the old last position: every
	// space removed costs the string its last character, and of two spaces in a row only the
	// first goes (both kept; "a b" comes out "a").
	int iLen = strlen(psz);
	for (int i = 0; i < iLen; i++)
	{
		if (0 != isspace((unsigned char)psz[i]))
		{
			for (int j = i; j < iLen - 1; j++)
				psz[j] = psz[j + 1];
			psz[iLen - 2] = '\0';
			iLen--;
		}
	}
}

// orig 0x71b90
char* ALT_FindEnclosed(char* psz, char chOpen, char chClose)
{
	const int iLen = strlen(psz);
	int iDepth = 0;
	char* pszStart = psz;
	for (int i = 0; i < iLen; i++)
	{
		const char c = psz[i];
		if (c == chOpen)
		{
			if (0 == iDepth)
			{
				psz[i] = '\0';
				pszStart = &psz[i + 1];
			}
			iDepth++;
		}
		else if (c == chClose)
		{
			if (--iDepth == 0)
			{
				psz[i] = '\0';
				return pszStart;
			}
		}
	}
	return NULL;
}

// The working copy every evaluation makes: strlen + 15 bytes, all but the last zeroed (so
// reading a little past the expression's end, which the comma split does, reads zeros).
// Name ours.
static char* ALT_Copy(const char* pszIn)
{
	const size_t n = strlen(pszIn);
	char* psz = (char*)malloc(n + 0xf);
	memset(psz, 0, n + 0xe);
	memcpy(psz, pszIn, n + 1);
	return psz;
}

// The expression part of a working copy: past the first '=', inside the first group.
// Name ours.
static char* ALT_Expression(char* pszBuf, char chOpen, char chClose)
{
	ALT_StripSpaces(pszBuf);
	char* psz = pszBuf;
	char* pszEquals = strpbrk(pszBuf, "=");
	if (pszEquals)
		psz = pszEquals + 1;
	char* pszInner = ALT_FindEnclosed(psz, chOpen, chClose);
	if (pszInner)
		psz = pszInner;
	return psz;
}

static bool ALT_IsNumberChar(char c)
{
	return 0 != isdigit((unsigned char)c) || c == '-' || c == 'E' || c == 'e' || c == '.';
}

// The index after a p or e and where the field starts: up to iMaxDigits digits, or
// iDefault. Name ours.
static int ALT_ParseIndex(const char* psz, int iMaxDigits, int iDefault, int& iPos)
{
	if (0 == isdigit((unsigned char)psz[1]))
	{
		iPos = 1;
		return iDefault;
	}
	int iIndex = psz[1] - '0';
	iPos = 2;
	while (iPos <= iMaxDigits && 0 != isdigit((unsigned char)psz[iPos]))
	{
		iIndex = iIndex * 10 + psz[iPos] - '0';
		iPos++;
	}
	return iIndex;
}

// The field letter's suffix: past the letter and an optional '.'. Name ours.
static char ALT_Suffix(const char* psz, int& iPos)
{
	iPos++;
	if (psz[iPos] == '.')
		iPos++;
	return (char)tolower((unsigned char)psz[iPos]);
}

static float ALT_Component(const Vector& vec, char c)
{
	switch (c)
	{
	case 'x':
		return vec.x;
	case 'y':
		return vec.y;
	case 'z':
		return vec.z;
	}
	return 0;
}

// orig 0x71c60
float ALT_EvalFloat(const char* pszExpr)
{
	char* pszBuf = ALT_Copy(pszExpr);
	char* s = ALT_Expression(pszBuf, '(', ')');
	const int iLen = strlen(s);

	int i = 0;
	while (i < iLen && ALT_IsNumberChar(s[i]))
		i++;
	if (i >= iLen)
	{
		const float flValue = atof(s);
		free(pszBuf);
		return flValue;
	}

	// {vector}.component
	if (s[0] == '{')
	{
		int iDepth = 0;
		for (i = 0; i < iLen; i++)
		{
			if (s[i] == '{')
				iDepth++;
			else if (s[i] == '}' && --iDepth <= 0)
				break;
		}
		if (i >= iLen)
		{
			free(pszBuf);
			return 0;
		}

		const Vector vec = ALT_EvalVector(s);
		i++;
		if (s[i] == '.')
			i++;
		const char c = (char)tolower((unsigned char)s[i]);
		free(pszBuf);
		if (c == 'l')
			return vec.Length();
		return ALT_Component(vec, c);
	}

	// the first operator outside parentheses splits it
	int iDepth = 0;
	for (i = 0; i < iLen; i++)
	{
		const char c = s[i];
		const bool bOperator = c == '+' || (c == '-' && atof(&s[i]) != 0) || c == '*' || c == '/' || c == '^' || c == '!';
		if (bOperator && iDepth <= 0)
			break;
		if (c == '(')
			iDepth++;
		else if (c == ')')
			iDepth--;
	}

	if (i < iLen)
	{
		const char chOperator = s[i];
		s[i] = '\0';
		const float a = ALT_EvalFloat(s);
		float b = 0;
		if (chOperator != '!')
			b = ALT_EvalFloat(&s[i + 1]);

		float flResult = a;
		switch (chOperator)
		{
		case '+':
			flResult = b + a;
			break;
		case '-':
			flResult = a - b;
			break;
		case '*':
			flResult = b * a;
			break;
		case '/':
			if (b != 0)
				flResult = a / b;
			break;
		case '^':
			flResult = pow(a, b);
			break;
		case '!':
		{
			double flTurns = a;
			while (flTurns < 0.0)
				flTurns += 1.0;
			while (flTurns > 1.0)
				flTurns -= 1.0;
			flResult = sin(flTurns * 6.283185307179586);
			break;
		}
		}
		free(pszBuf);
		return flResult;
	}

	if (ALT_IsNumberChar(s[0]) || atof(s) != 0)
	{
		const float flValue = atof(s);
		free(pszBuf);
		return flValue;
	}

	int iPos;
	const char chName = (char)tolower((unsigned char)s[0]);
	if (chName == 'p')
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(ALT_ParseIndex(s, 2, g_iLCSCallerIndex, iPos));
		if (!pPlayer)
		{
			free(pszBuf);
			return 0;
		}
		if (s[iPos] == '.')
			iPos++;

		const char chField = (char)tolower((unsigned char)s[iPos]);
		char c;
		switch (chField)
		{
		case 'a':
			c = ALT_Suffix(s, iPos);
			free(pszBuf);
			return ALT_Component(pPlayer->pev->v_angle, c);
		case 'v':
			c = ALT_Suffix(s, iPos);
			free(pszBuf);
			if (c == 'l')
				return pPlayer->pev->velocity.Length();
			return ALT_Component(pPlayer->pev->velocity, c);
		case 'o':
		case 'p':
			c = ALT_Suffix(s, iPos);
			free(pszBuf);
			return ALT_Component(pPlayer->pev->origin, c);
		case 'f':
		case 'r':
		case 'u':
			c = ALT_Suffix(s, iPos);
			free(pszBuf);
			UTIL_MakeVectors(pPlayer->pev->v_angle);
			return ALT_Component(chField == 'f' ? gpGlobals->v_forward : (chField == 'r' ? gpGlobals->v_right : gpGlobals->v_up), c);
		}
		free(pszBuf);
		return 0;
	}

	if (chName == 'e')
	{
		CBaseEntity* pEntity = CBaseEntity::Instance(INDEXENT(ALT_ParseIndex(s, 4, g_iLCSSelectedIndex, iPos)));
		if (!pEntity)
		{
			free(pszBuf);
			return 0;
		}
		if (s[iPos] == '.')
			iPos++;

		const char chField = (char)tolower((unsigned char)s[iPos]);
		char c;
		switch (chField)
		{
		case 'a':
			c = ALT_Suffix(s, iPos);
			free(pszBuf);
			return ALT_Component(pEntity->IsPlayer() ? pEntity->pev->v_angle : pEntity->pev->angles, c);
		case 'v':
			c = ALT_Suffix(s, iPos);
			free(pszBuf);
			if (c == 'l')
				return pEntity->pev->velocity.Length();
			return ALT_Component(pEntity->pev->velocity, c);
		case 'm':
			c = ALT_Suffix(s, iPos);
			free(pszBuf);
			if (c == 'l')
				return pEntity->pev->movedir.Length();
			return ALT_Component(pEntity->pev->movedir, c);
		case 'o':
		case 'p':
			c = ALT_Suffix(s, iPos);
			free(pszBuf);
			return ALT_Component(pEntity->pev->origin, c);
		case 'c':
			c = ALT_Suffix(s, iPos);
			free(pszBuf);
			return ALT_Component(pEntity->Center(), c);
		case 'f':
		case 'r':
		case 'u':
			c = ALT_Suffix(s, iPos);
			free(pszBuf);
			if (pEntity->IsPlayer())
				UTIL_MakeVectors(pEntity->pev->v_angle);
			else
				UTIL_MakeAimVectors(pEntity->pev->v_angle);
			return ALT_Component(chField == 'f' ? gpGlobals->v_forward : (chField == 'r' ? gpGlobals->v_right : gpGlobals->v_up), c);
		}
		free(pszBuf);
		return 0;
	}

	free(pszBuf);
	switch (chName)
	{
	case 't':
		return gpGlobals->time;
	case 'f':
		return gpGlobals->frametime;
	case 'r':
		return RANDOM_FLOAT(0, 1);
	case 's':
		return RANDOM_FLOAT(-1, 1);
	}
	return 0;
}

static Vector ALT_Normalized(const Vector& vec)
{
	if (vec.Length() == 0)
		return Vector(0, 0, 1);
	return vec.Normalize();
}

// orig 0x72a90
Vector ALT_EvalVector(const char* pszExpr)
{
	char* pszBuf = ALT_Copy(pszExpr);
	char* s = ALT_Expression(pszBuf, '{', '}');
	const int iLen = strlen(s);

	// x,y,z, when a comma comes before any '{'
	int i = 0;
	bool bBrace = false;
	for (; i < iLen; i++)
	{
		if (s[i] == ',')
			break;
		if (s[i] == '{')
		{
			bBrace = true;
			break;
		}
	}
	if (i < iLen && !bBrace)
	{
		float rgflValue[3];
		int iStart = 0;
		int j = 0;
		for (int k = 0; k < 3; k++)
		{
			while (j < iLen && s[j] != ',')
				j++;
			if (j < iLen)
				s[j] = '\0';
			rgflValue[k] = ALT_EvalFloat(s + iStart);
			iStart = ++j;
		}
		free(pszBuf);
		return Vector(rgflValue[0], rgflValue[1], rgflValue[2]);
	}

	// the first operator outside braces splits it
	int iDepth = 0;
	for (i = 0; i < iLen; i++)
	{
		const char c = s[i];
		if ((c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '&' || c == '!') && iDepth <= 0)
			break;
		if (c == '{')
			iDepth++;
		else if (c == '}')
			iDepth--;
	}

	if (i < iLen)
	{
		const char chOperator = s[i];
		s[i] = '\0';
		const Vector a = ALT_EvalVector(s);
		const char* pszRight = &s[i + 1];

		Vector vecResult = a;
		switch (chOperator)
		{
		case '+':
			vecResult = ALT_EvalVector(pszRight) + a;
			break;
		case '-':
			vecResult = a - ALT_EvalVector(pszRight);
			break;
		case '*':
			if (pszRight[0] == '{')
			{
				const Vector b = ALT_EvalVector(pszRight);
				vecResult = Vector(a.x * b.x, a.y * b.y, a.z * b.z);
			}
			else
			{
				const float b = ALT_EvalFloat(pszRight);
				vecResult = Vector(b * a.x, b * a.y, b * a.z);
			}
			break;
		case '/':
			if (pszRight[0] == '{')
			{
				const Vector b = ALT_EvalVector(pszRight);
				if (b.x != 0)
					vecResult.x = a.x / b.x;
				if (b.y != 0)
					vecResult.y = a.y / b.y;
				if (b.z != 0)
					vecResult.z = a.z / b.z;
			}
			else
			{
				const float b = ALT_EvalFloat(pszRight);
				if (b != 0)
					vecResult = Vector(a.x / b, a.y / b, a.z / b);
			}
			break;
		case '^':
			UTIL_MakeVectors(a);
			vecResult = gpGlobals->v_forward;
			break;
		case '&':
			vecResult = UTIL_VecToAngles(a);
			break;
		case '!':
			vecResult = ALT_Normalized(a);
			break;
		}
		free(pszBuf);
		return vecResult;
	}

	// VERIFY: the field lists below follow the decompile, whose branches here lost their
	// returns; the operator part above and the float evaluator were read from the disassembly
	int iPos;
	const char chName = (char)tolower((unsigned char)s[0]);
	if (chName == 'p')
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(ALT_ParseIndex(s, 2, g_iLCSCallerIndex, iPos));
		if (!pPlayer)
		{
			free(pszBuf);
			return g_vecZero;
		}
		if (s[iPos] == '.')
			iPos++;

		const char chField = (char)tolower((unsigned char)s[iPos]);
		char c;
		switch (chField)
		{
		case 'v':
			c = ALT_Suffix(s, iPos);
			free(pszBuf);
			return c != 'n' ? pPlayer->pev->velocity : ALT_Normalized(pPlayer->pev->velocity);
		case 'm':
			c = ALT_Suffix(s, iPos);
			free(pszBuf);
			return c != 'n' ? pPlayer->pev->movedir : ALT_Normalized(pPlayer->pev->movedir);
		case 'o':
		case 'p':
			free(pszBuf);
			return pPlayer->pev->origin;
		case 'a':
			free(pszBuf);
			return pPlayer->pev->v_angle;
		case 'e':
			free(pszBuf);
			return pPlayer->pev->view_ofs;
		case 'f':
		case 'r':
		case 'u':
			free(pszBuf);
			UTIL_MakeVectors(pPlayer->pev->v_angle);
			return chField == 'f' ? gpGlobals->v_forward : (chField == 'r' ? gpGlobals->v_right : gpGlobals->v_up);
		}
	}
	else if (chName == 'e')
	{
		CBaseEntity* pEntity = CBaseEntity::Instance(INDEXENT(ALT_ParseIndex(s, 4, g_iLCSSelectedIndex, iPos)));
		if (!pEntity)
		{
			free(pszBuf);
			return g_vecZero;
		}
		if (s[iPos] == '.')
			iPos++;

		const char chField = (char)tolower((unsigned char)s[iPos]);
		char c;
		switch (chField)
		{
		case 'v':
			c = ALT_Suffix(s, iPos);
			free(pszBuf);
			return c != 'n' ? pEntity->pev->velocity : ALT_Normalized(pEntity->pev->velocity);
		case 'o':
		case 'p':
			free(pszBuf);
			return pEntity->pev->origin;
		case 'c':
			free(pszBuf);
			return pEntity->Center();
		case 'a':
			free(pszBuf);
			return pEntity->IsPlayer() ? pEntity->pev->v_angle : pEntity->pev->angles;
		case 'e':
			free(pszBuf);
			return pEntity->pev->view_ofs;
		case 'f':
		case 'r':
		case 'u':
			free(pszBuf);
			if (pEntity->IsPlayer())
				UTIL_MakeVectors(pEntity->pev->v_angle);
			else
				UTIL_MakeAimVectors(pEntity->pev->v_angle);
			return chField == 'f' ? gpGlobals->v_forward : (chField == 'r' ? gpGlobals->v_right : gpGlobals->v_up);
		}
	}

	if (chName == 'r')
	{
		const char c = (char)tolower((unsigned char)s[1]);
		free(pszBuf);
		// the components come out z first, then y, then x
		if (c != 'n')
		{
			const float z = RANDOM_FLOAT(-1, 1);
			const float y = RANDOM_FLOAT(-1, 1);
			const float x = RANDOM_FLOAT(-1, 1);
			return Vector(x, y, z);
		}
		Vector vec;
		do
		{
			const float z = RANDOM_FLOAT(-1, 1);
			const float y = RANDOM_FLOAT(-1, 1);
			const float x = RANDOM_FLOAT(-1, 1);
			vec = Vector(x, y, z);
		} while (vec.Length() == 0);
		return ALT_Normalized(vec);
	}

	CBaseEntity* pPlayer = UTIL_PlayerByIndex(g_iLCSCallerIndex);
	if (pPlayer)
	{
		switch (chName)
		{
		case 't':
		{
			free(pszBuf);
			UTIL_MakeVectors(pPlayer->pev->v_angle);
			const Vector vecSrc = pPlayer->pev->origin + pPlayer->pev->view_ofs;
			TraceResult tr;
			UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 8192, ignore_monsters, pPlayer->edict(), &tr);
			return tr.vecEndPos;
		}
		case 'o':
			free(pszBuf);
			return pPlayer->pev->view_ofs + pPlayer->pev->origin;
		case 'v':
		{
			free(pszBuf);
			const char c = (char)tolower((unsigned char)s[1]);
			return c != 'n' ? pPlayer->pev->velocity : ALT_Normalized(pPlayer->pev->velocity);
		}
		}
	}

	free(pszBuf);
	return g_vecZero;
}

// orig 0x73fb0
// VERIFY: read from the decompile only (it was coherent here)
void ALT_SplitArgs(const char* pszLine, void (*pfnArg)(int iIndex, char* pszArg))
{
	char* pszBuf = (char*)malloc(strlen(pszLine) + 1);
	strcpy(pszBuf, pszLine);

	const int iLen = strlen(pszBuf);
	int iIndex = 0;
	int iBrackets = 0, iBraces = 0, iParens = 0;
	bool bQuoted = false;
	char* pszArg = NULL;
	int i = 0;
	for (; i < iLen; i++)
	{
		char* pch = &pszBuf[i];
		if (0 != isspace((unsigned char)pszBuf[i]) && iBrackets + (bQuoted ? 1 : 0) + iBraces + iParens < 1)
		{
			if (pszArg)
			{
				*pch = '\0';
				pfnArg(iIndex, pszArg);
				i++;
				iIndex++;
			}
			while (pszBuf[i] != '\0' && 0 != isspace((unsigned char)pszBuf[i]))
				i++;
			pszArg = &pszBuf[i];
			pch = pszArg;
			if (pszBuf[i] == '\0')
				break;
		}
		if (!pszArg)
			pszArg = pch;

		const char c = *pch;
		if (c == '"' || c == '\'')
			bQuoted = !bQuoted;
		if (c == '[')
			iBrackets++;
		if (c == ']')
			iBrackets--;
		if (c == '(')
			iParens++;
		if (c == ')')
			iParens--;
		if (c == '{')
			iBraces++;
		if (c == '}')
			iBraces--;
	}

	if (pszArg)
	{
		if (i < iLen)
			pszBuf[i] = '\0';
		pfnArg(iIndex, pszArg);
	}
	free(pszBuf);
}
