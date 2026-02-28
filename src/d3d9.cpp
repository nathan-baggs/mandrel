#include <cstddef>

#include <limits>
#include <processthreadsapi.h>
#include <windows.h>

#include <d3d9.h>
#include <psapi.h>

#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>
#include <imgui.h>
#include <winnt.h>

#include "mandrel/allocators/imgui_allocator.h"
#include "mandrel/containers/vector.h"
#include "mandrel/hooks/com_hook.h"
#include "mandrel/utils.h"

LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
auto orig_wind_proc = ::WNDPROC{};

::LRESULT WINAPI wind_proc(const ::HWND hWnd, ::UINT uMsg, ::WPARAM wParam, ::LPARAM lParam)
{
    if (::ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
    {
        return true;
    }
    return ::CallWindowProc(orig_wind_proc, hWnd, uMsg, wParam, lParam);
}

::HRESULT WINAPI IDirect3DDevice9_EndScene_Hook(::PROC orig_func, void *that)
{
    using orig_call_type = OrigFunc<decltype(&IDirect3DDevice9_EndScene_Hook)>::type;

    [[maybe_unused]] static auto initialised = [that]()
    {
        auto *device = reinterpret_cast<::LPDIRECT3DDEVICE9>(that);

        ::ImGui::SetAllocatorFunctions(mandrel::imgui_allocator, mandrel::imgui_deallocator, nullptr);

        auto params = ::D3DDEVICE_CREATION_PARAMETERS{};
        device->GetCreationParameters(&params);
        const auto window = params.hFocusWindow;

        orig_wind_proc = reinterpret_cast<::WNDPROC>(
            ::SetWindowLongPtr(window, GWLP_WNDPROC, reinterpret_cast<::LONG_PTR>(wind_proc)));

        IMGUI_CHECKVERSION();
        ::ImGui::CreateContext();

        auto &io = ::ImGui::GetIO();
        io.ConfigFlags |= ::ImGuiConfigFlags_DockingEnable;

        ::ImGui_ImplWin32_Init(window);
        ::ImGui_ImplDX9_Init(device);

        mandrel::log("imgui initialisation done");
        return true;
    }();

    ::ImGui_ImplDX9_NewFrame();
    ::ImGui_ImplWin32_NewFrame();
    ::ImGui::NewFrame();

    ::ImGui::DockSpaceOverViewport(0, ::ImGui::GetMainViewport(), ::ImGuiDockNodeFlags_PassthruCentralNode);

    ::ImGui::Begin("Memory Usage");

    static auto memory_samples = mandrel::Vector<float>(1000u);

    auto pmc = ::PROCESS_MEMORY_COUNTERS{};
    pmc.cb = sizeof(::PROCESS_MEMORY_COUNTERS);
    mandrel::ensure(
        ::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc)) == TRUE, "failed to get memory info");

    const auto current_mem_mb = static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);

    memory_samples.erase(std::ranges::begin(memory_samples));
    memory_samples.push_back(current_mem_mb);

    ::ImGui::Text("current: %.2f MB", current_mem_mb);

    ::ImGui::PlotLines(
        "usage (MB)",
        memory_samples.data(),
        memory_samples.size(),
        0,
        nullptr,
        0.0f,
        std::numeric_limits<float>::max(),
        ::ImVec2(0, 80.0f));

    ::ImGui::End();

    ::ImGui::EndFrame();
    ::ImGui::Render();
    ::ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return reinterpret_cast<orig_call_type>(orig_func)(that);
}

::HRESULT WINAPI IDirect3DDevice9_SetStreamSource_hook(
    ::PROC orig_func,
    void *that,
    ::UINT StreamNumber,
    ::IDirect3DVertexBuffer9 *pStreamData,
    ::UINT OffsetInBytes,
    ::UINT Stride)
{
    using orig_call_type = OrigFunc<decltype(&IDirect3DDevice9_SetStreamSource_hook)>::type;

    return reinterpret_cast<orig_call_type>(orig_func)(that, StreamNumber, pStreamData, OffsetInBytes, Stride);
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

    com_hook.add_hook<42zu>(*ppReturnedDeviceInterface, IDirect3DDevice9_EndScene_Hook);
    com_hook.add_hook<100zu>(*ppReturnedDeviceInterface, IDirect3DDevice9_SetStreamSource_hook);

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

    com_hook.add_hook<16zu>(d3d9, IDirect3D9_CreateDevice_hook);

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
