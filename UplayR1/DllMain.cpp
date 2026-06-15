#include "../Core/Core.h"

DLLEXPORT const char* ThrowbackLoader_GetVersion()
{
    return get_version_string();
}

BOOL APIENTRY DllMain(HMODULE handle_module, DWORD call_reason, LPVOID lp_reserved)
{
    const auto result = handle_windows_calls(call_reason);
    if (call_reason == DLL_PROCESS_ATTACH)
    {
        execute_streaminginstall_workaround();
    }
    return result;
}
