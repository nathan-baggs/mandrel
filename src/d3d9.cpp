#include <windows.h>

#include <d3d9.h>

#include "mandrel/hooks/com_hook.h"
#include "mandrel/utils.h"

namespace
{

template <class F>
struct OrigFunc;

template <class R, class H, class... Tail>
struct OrigFunc<R(WINAPI *)(H, Tail...)>
{
    using type = R(WINAPI *)(Tail...);
};

auto com_hook = mandrel::COMHook{};

::HRESULT WINAPI IDirect3DDevice9_EndScene_Hook(::PROC orig_func, void *that)
{
    using orig_call_type = OrigFunc<decltype(&IDirect3DDevice9_EndScene_Hook)>::type;

    return reinterpret_cast<orig_call_type>(orig_func)(that);
}

::HRESULT WINAPI IDirect3D9_CreateDevice_hook(
    ::PROC orig_func,
    void *that,
    ::UINT Adapter,
    ::D3DDEVTYPE DeviceType,
    ::HWND hFocusWindow,
    ::DWORD BehaviorFlags,
    ::D3DPRESENT_PARAMETERS *pPresentationParameters,
    ::IDirect3DDevice9 **ppReturnedDeviceInterface)
{
    using orig_call_type = OrigFunc<decltype(&IDirect3D9_CreateDevice_hook)>::type;

    mandrel::log("IDirect3D9::CreateDevice called");

    const auto res = reinterpret_cast<orig_call_type>(orig_func)(
        that, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

    com_hook.add_hook(42zu, *ppReturnedDeviceInterface, IDirect3DDevice9_EndScene_Hook);

    return res;
}

}

extern "C"
{
::DWORD WINAPI DllMain(void *, ::DWORD, void *);

__declspec(dllexport) ::IDirect3D9 *WINAPI Direct3DCreate9(::UINT SDKVersion)
{
    mandrel::log("Direct3DCreate9 called");

    const auto d3d9_lib = ::LoadLibraryA("C:\\Windows\\system32\\d3d9.dll");
    mandrel::ensure(d3d9_lib != nullptr, "could not load d3d9");

    const auto direct_create =
        reinterpret_cast<decltype(&Direct3DCreate9)>(::GetProcAddress(d3d9_lib, "Direct3DCreate9"));
    mandrel::ensure(direct_create != NULL, "failed to get address of Direct3DCreate9");

    auto *d3d9 = direct_create(SDKVersion);

    com_hook.add_hook(16zu, d3d9, IDirect3D9_CreateDevice_hook);

    return d3d9;
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
