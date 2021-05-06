#include "solib.h"

LPHSO solibLoadLibrary(char *szLibrary)
{
}

void solibFreeLibrary(LPHSO lpSoLibrary)
{
}

void *solibGetProcAddress(LPHSO lpSoLibrary, char *szFunctionName)
{
}

void *solibInstallRelocation(LPHSO lpSoLibrary, char *szSymbolName, void *pfnDestProc)
{
}
