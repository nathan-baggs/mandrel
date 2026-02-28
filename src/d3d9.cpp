#include <windows.h>

#include <d3d9.h>

#include "mandrel/utils.h"

extern "C"
{
::DWORD WINAPI DllMain(void *, ::DWORD, void *);

__declspec(dllexport) ::IDirect3D9 *WINAPI Direct3DCreate9(::UINT SDKVersion)
{
    mandrel::log("Direct3DCreate9 called");

    const auto d3d9 = ::LoadLibraryA("C:\\Windows\\system32\\d3d9.dll");
    mandrel::ensure(d3d9 != nullptr, "could not load d3d9");

    const auto direct_create = reinterpret_cast<decltype(&Direct3DCreate9)>(::GetProcAddress(d3d9, "Direct3DCreate9"));
    mandrel::ensure(direct_create != NULL, "failed to get address of Direct3DCreate9");

    return direct_create(SDKVersion);
}

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
