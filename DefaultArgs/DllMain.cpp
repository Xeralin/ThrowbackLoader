#include "../Core/Core.h"

DLLEXPORT char const* GetDefaultArgsString()
{
    return "/uplayproductid:635 /belaunch /nologo";
}

BOOL APIENTRY DllMain(HMODULE handle_module, DWORD call_reason, LPVOID lp_reserved)
{
    return TRUE;
}
