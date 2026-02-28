#include <windows.h>

#include "mandrel/utils.h"

extern "C"
{
::DWORD WINAPI DllMain(void *, ::DWORD, void *);

::DWORD WINAPI DllMain(void *, ::DWORD fdwReason, void *)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            mandrel::log("DLLMain called for d3d9.dll");
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH: break;
    }

    return 1;
}
}
