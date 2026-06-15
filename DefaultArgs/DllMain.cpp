#include "../Core/Core.h"

#ifdef _MSC_VER
#pragma comment(linker, "/export:?GetDefaultArgsString@DefaultArgs@@YAPEBDXZ=GetDefaultArgsString")
#endif
DLLEXPORT char const* GetDefaultArgsString()
{
    return "/uplayproductid:635 /belaunch /nologo /windowtitle:\"Rainbow Six Siege: Operation Throwback\"";
}

BOOL APIENTRY DllMain(HMODULE handle_module, DWORD call_reason, LPVOID lp_reserved)
{
    return TRUE;
}
