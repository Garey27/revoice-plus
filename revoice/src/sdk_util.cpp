// Selected portions of dlls/util.cpp from SDK 2.1.
// Functions copied from there as needed...
// And modified to avoid buffer overflows (argh).

/***
*
*	Copyright (c) 1999, 2000 Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

/*

===== util.cpp ========================================================

  Utility code.  Really not optional after all.

*/

#include "precompiled.h"

//=========================================================
// UTIL_LogPrintf - Prints a logged message to console.
// Preceded by LOG: ( timestamp ) < message >
//=========================================================
void UTIL_LogPrintf(char *fmt, ...)
{
	va_list argptr;
	static char string[1024];

	va_start(argptr, fmt);
	vsnprintf(string, sizeof(string), fmt, argptr);
	va_end(argptr);

	// Print to server console
	ALERT(at_logged, "%s", string);
}

char *UTIL_VarArgs(char *format, ...)
{
	va_list argptr;
	static char string[1024];

	va_start(argptr, format);
	vsprintf(string, format, argptr);
	va_end(argptr);

	return string;
}

void UTIL_ServerPrintf(const char *fmt, ...)
{
	// Check is null, test the demo started before than searches pointer to refs
	if (&g_engfuncs == nullptr || g_engfuncs.pfnServerPrint == nullptr)
		return;

	static char string[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(string, sizeof(string), fmt, ap);
	va_end(ap);

	if (strlen(string) < sizeof(string) - 2)
		strcat(string, "\n");
	else
		string[strlen(string) - 1] = '\n';

	SERVER_PRINT(string);
}
