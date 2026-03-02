#include <algorithm>
#include <atomic>
#include <cstddef>
#include <limits>

#include <utility>
#include <windows.h>

#include <d3d9.h>
#include <processthreadsapi.h>
#include <psapi.h>

#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>
#include <imgui.h>
#include <winnt.h>

#include "mandrel/allocators/imgui_allocator.h"
#include "mandrel/containers/sstream.h"
#include "mandrel/containers/stack_trace.h"
#include "mandrel/containers/unordered_set.h"
#include "mandrel/containers/vector.h"
#include "mandrel/hooks/com_hook.h"
#include "mandrel/resource_tracker.h"
#include "mandrel/utils.h"

LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{

auto read_log_tail() -> mandrel::String
{
    const auto log_file = ::CreateFileA(
        mandrel::get_temp("log.txt").string().c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (log_file == INVALID_HANDLE_VALUE)
    {
        return "could not open log file";
    }

    const auto file_size = ::GetFileSize(log_file, nullptr);
    if (file_size == INVALID_FILE_SIZE)
    {
        return "could not get log file size";
    }

    static constexpr auto max_read_size = 1024u;
    const auto read_size = std::min<::DWORD>(file_size, max_read_size);
    if (file_size > 1024u)
    {
        ::SetFilePointer(log_file, -static_cast<LONG>(read_size), nullptr, FILE_END);
    }

    auto buffer = mandrel::String(read_size, '\0');
    auto bytes_read = ::DWORD{};
    if (::ReadFile(log_file, buffer.data(), read_size, &bytes_read, nullptr) == FALSE)
    {
        return "could not read log file";
    }

    return buffer;
}

auto largest_free_page() -> std::size_t
{
    auto largest_size = std::size_t{};

    auto mem_info = ::MEMORY_BASIC_INFORMATION{};

    std::byte *addr = nullptr;
    while (::VirtualQuery(addr, &mem_info, sizeof(mem_info)) != 0)
    {
        if (mem_info.State == MEM_FREE && mem_info.RegionSize > largest_size)
        {
            largest_size = mem_info.RegionSize;
        }

        addr += mem_info.RegionSize;
    }

    return largest_size;
}

template <class F>
struct OrigFunc;

template <class R, class Head, class... Tail>
struct OrigFunc<R(WINAPI *)(Head, Tail...)>
{
    using type = R(WINAPI *)(Tail...);
};

template <class F>
using OrigFuncType = OrigFunc<F>::type;

auto com_hook = mandrel::COMHook{};
auto orig_wind_proc = ::WNDPROC{};

auto tracked_vertex_buffers = mandrel::ResourceTracker<void *>{};
auto tracked_index_buffers = mandrel::ResourceTracker<void *>{};
auto tracked_textures = mandrel::ResourceTracker<void *>{};
auto tracked_state_blocks = mandrel::ResourceTracker<void *>{};

::LRESULT WINAPI wind_proc(const ::HWND hWnd, ::UINT uMsg, ::WPARAM wParam, ::LPARAM lParam)
{
    if (::ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
    {
        return true;
    }
    return ::CallWindowProc(orig_wind_proc, hWnd, uMsg, wParam, lParam);
}

}

__declspec(dllexport) ::HRESULT WINAPI IDirect3DDevice9_EndScene_Hook(::PROC orig_func, void *that);
__declspec(dllexport) ::HRESULT WINAPI IDirect3DDevice9_SetStreamSource_hook(
    ::PROC orig_func,
    void *that,
    ::UINT StreamNumber,
    ::IDirect3DVertexBuffer9 *pStreamData,
    ::UINT OffsetInBytes,
    ::UINT Stride);
__declspec(dllexport) ::ULONG WINAPI IDirect3DVertexBuffer9_Release_hook(::PROC orig_func, void *that);
__declspec(dllexport) ::HRESULT WINAPI IDirect3DDevice9_CreateVertexBuffer_hook(
    ::PROC orig_func,
    void *that,
    ::UINT Length,
    ::DWORD Usage,
    ::DWORD FVF,
    ::D3DPOOL Pool,
    ::IDirect3DVertexBuffer9 **ppVertexBuffer,
    ::HANDLE *pSharedHandle);
__declspec(dllexport) ::ULONG WINAPI IDirect3DIndexBuffer9_Release_hook(::PROC orig_func, void *that);
__declspec(dllexport) ::HRESULT WINAPI IDirect3DDevice9_CreateIndexBuffer_hook(
    ::PROC orig_func,
    void *that,
    ::UINT Length,
    ::DWORD Usage,
    ::D3DFORMAT Format,
    ::D3DPOOL Pool,
    ::IDirect3DIndexBuffer9 **ppIndexBuffer,
    ::HANDLE *pSharedHandle);
__declspec(dllexport) ::ULONG WINAPI IDirect3DTexture9_Release_hook(::PROC orig_func, void *that);
__declspec(dllexport) ::HRESULT WINAPI IDirect3DDevice9_CreateTexture_hook(
    ::PROC orig_func,
    void *that,
    ::UINT Width,
    ::UINT Height,
    ::UINT Levels,
    ::DWORD Usage,
    ::D3DFORMAT Format,
    ::D3DPOOL Pool,
    ::IDirect3DTexture9 **ppTexture,
    ::HANDLE *pSharedHandle);
__declspec(dllexport) ::HRESULT WINAPI IDirect3DStateBlock9_Release_hook(::PROC orig_func, void *that);
__declspec(dllexport) ::HRESULT WINAPI IDirect3DDevice9_CreateStateBlock_hook(
    ::PROC orig_func,
    void *that,
    ::D3DSTATEBLOCKTYPE Type,
    ::IDirect3DStateBlock9 **ppSB);
__declspec(dllexport) ::HRESULT WINAPI IDirect3D9_CreateDevice_hook(
    ::PROC orig_func,
    void *that,
    ::UINT Adapter,
    ::D3DDEVTYPE DeviceType,
    ::HWND hFocusWindow,
    ::DWORD BehaviorFlags,
    ::D3DPRESENT_PARAMETERS *pPresentationParameters,
    ::IDirect3DDevice9 **ppReturnedDeviceInterface);

__declspec(dllexport) ::HRESULT WINAPI IDirect3DDevice9_EndScene_Hook(::PROC orig_func, void *that)
{
    using orig_call_type = OrigFuncType<decltype(&IDirect3DDevice9_EndScene_Hook)>;

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

    auto statex = ::MEMORYSTATUSEX{};
    statex.dwLength = sizeof(::MEMORYSTATUSEX);
    mandrel::ensure(
        ::GlobalMemoryStatusEx(&statex) != FALSE, "failed to get global memory status: {}", ::GetLastError());

    const auto current_mem_mb = static_cast<float>(statex.ullAvailVirtual) / (1024.0f * 1024.0f);

    memory_samples.erase(std::ranges::begin(memory_samples));
    memory_samples.push_back(current_mem_mb);

    ::ImGui::Text("current: %.2f MB", current_mem_mb);
    ::ImGui::Text("largest free block: %.2f MB", static_cast<float>(largest_free_page()) / (1024.0f * 1024.0f));

    ::ImGui::PlotLines(
        "usage (MB)",
        memory_samples.data(),
        memory_samples.size(),
        0,
        nullptr,
        0.0f,
        std::numeric_limits<float>::max(),
        ::ImVec2(0, 80.0f));

    static auto vb_count_samples = mandrel::Vector<float>(1000u);
    vb_count_samples.erase(std::ranges::begin(vb_count_samples));
    vb_count_samples.push_back(static_cast<float>(tracked_vertex_buffers.live_count()));
    ::ImGui::Text("live vertex buffers: %zu", tracked_vertex_buffers.live_count());
    ::ImGui::PlotLines(
        "vertex buffer count",
        vb_count_samples.data(),
        vb_count_samples.size(),
        0,
        nullptr,
        0.0f,
        static_cast<float>(std::max<std::size_t>(tracked_vertex_buffers.live_count(), 100u)),
        ::ImVec2(0, 80.0f));

    static auto ib_count_samples = mandrel::Vector<float>(1000u);
    ib_count_samples.erase(std::ranges::begin(ib_count_samples));
    ib_count_samples.push_back(static_cast<float>(tracked_index_buffers.live_count()));
    ::ImGui::Text("live index buffers: %zu", tracked_index_buffers.live_count());
    ::ImGui::PlotLines(
        "index buffer count",
        ib_count_samples.data(),
        ib_count_samples.size(),
        0,
        nullptr,
        0.0f,
        static_cast<float>(std::max<std::size_t>(tracked_index_buffers.live_count(), 100u)),
        ::ImVec2(0, 80.0f));

    static auto texture_count_samples = mandrel::Vector<float>(1000u);
    texture_count_samples.erase(std::ranges::begin(texture_count_samples));
    texture_count_samples.push_back(static_cast<float>(tracked_textures.live_count()));
    ::ImGui::Text("live textures: %zu", tracked_textures.live_count());
    ::ImGui::PlotLines(
        "texture count",
        texture_count_samples.data(),
        texture_count_samples.size(),
        0,
        nullptr,
        0.0f,
        static_cast<float>(std::max<std::size_t>(tracked_textures.live_count(), 100u)),
        ::ImVec2(0, 80.0f));

    ::ImGui::Text("live state blocks: %zu", tracked_state_blocks.live_count());

    ::ImGui::End();

    ::ImGui::Begin("Log");
    ::ImGui::TextUnformatted(read_log_tail().c_str());
    ::ImGui::End();

    ::ImGui::EndFrame();
    ::ImGui::Render();
    ::ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return reinterpret_cast<orig_call_type>(orig_func)(that);
}

__declspec(dllexport) ::HRESULT WINAPI IDirect3DDevice9_SetStreamSource_hook(
    ::PROC orig_func,
    void *that,
    ::UINT StreamNumber,
    ::IDirect3DVertexBuffer9 *pStreamData,
    ::UINT OffsetInBytes,
    ::UINT Stride)
{
    using orig_call_type = OrigFuncType<decltype(&IDirect3DDevice9_SetStreamSource_hook)>;

    return reinterpret_cast<orig_call_type>(orig_func)(that, StreamNumber, pStreamData, OffsetInBytes, Stride);
}

__declspec(dllexport) ::ULONG WINAPI IDirect3DVertexBuffer9_Release_hook(::PROC orig_func, void *that)
{
    using orig_call_type = OrigFunc<decltype(&IDirect3DVertexBuffer9_Release_hook)>::type;

    const auto res = reinterpret_cast<orig_call_type>(orig_func)(that);

    if (res == 0)
    {
        tracked_vertex_buffers.untrack(that);
        mandrel::log("IDirect3DVertexBuffer9 released {}", that);
    }

    return res;
}

__declspec(dllexport) ::HRESULT WINAPI IDirect3DDevice9_CreateVertexBuffer_hook(
    ::PROC orig_func,
    void *that,
    ::UINT Length,
    ::DWORD Usage,
    ::DWORD FVF,
    ::D3DPOOL Pool,
    ::IDirect3DVertexBuffer9 **ppVertexBuffer,
    ::HANDLE *pSharedHandle)
{
    using orig_call_type = OrigFuncType<decltype(&IDirect3DDevice9_CreateVertexBuffer_hook)>;

    const auto res =
        reinterpret_cast<orig_call_type>(orig_func)(that, Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);

    mandrel::log("IDirect3DDevice9::CreateVertexBuffer called {} [{}]", static_cast<void *>(*ppVertexBuffer), res);

    com_hook.add_hook<2zu>(*ppVertexBuffer, IDirect3DVertexBuffer9_Release_hook);

    tracked_vertex_buffers.track(*ppVertexBuffer);

    return res;
}

__declspec(dllexport) ::ULONG WINAPI IDirect3DIndexBuffer9_Release_hook(::PROC orig_func, void *that)
{
    using orig_call_type = OrigFuncType<decltype(&IDirect3DIndexBuffer9_Release_hook)>;

    const auto res = reinterpret_cast<orig_call_type>(orig_func)(that);

    if (res == 0)
    {
        if (tracked_index_buffers.untrack(that))
        {
            mandrel::log("IDirect3DIndexBuffer9 released {}", that);
        }
    }

    return res;
}

__declspec(dllexport) ::HRESULT WINAPI IDirect3DDevice9_CreateIndexBuffer_hook(
    ::PROC orig_func,
    void *that,
    ::UINT Length,
    ::DWORD Usage,
    ::D3DFORMAT Format,
    ::D3DPOOL Pool,
    ::IDirect3DIndexBuffer9 **ppIndexBuffer,
    ::HANDLE *pSharedHandle)
{
    using orig_call_type = OrigFuncType<decltype(&IDirect3DDevice9_CreateIndexBuffer_hook)>;

    const auto res =
        reinterpret_cast<orig_call_type>(orig_func)(that, Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);

    mandrel::log("IDirect3DDevice9::CreateIndexBuffer called {} [{}]", static_cast<void *>(*ppIndexBuffer), res);

    com_hook.add_hook<2zu>(*ppIndexBuffer, IDirect3DIndexBuffer9_Release_hook);

    tracked_index_buffers.track(*ppIndexBuffer);

    return res;
}

__declspec(dllexport) ::ULONG WINAPI IDirect3DTexture9_Release_hook(::PROC orig_func, void *that)
{
    using orig_call_type = OrigFuncType<decltype(&IDirect3DTexture9_Release_hook)>;

    const auto res = reinterpret_cast<orig_call_type>(orig_func)(that);

    if (res == 0)
    {
        if (tracked_textures.untrack(that))
        {
            mandrel::log("IDirect3DTexture9 released {}", that);
        }
    }

    return res;
}

__declspec(dllexport) ::HRESULT WINAPI IDirect3DDevice9_CreateTexture_hook(
    ::PROC orig_func,
    void *that,
    ::UINT Width,
    ::UINT Height,
    ::UINT Levels,
    ::DWORD Usage,
    ::D3DFORMAT Format,
    ::D3DPOOL Pool,
    ::IDirect3DTexture9 **ppTexture,
    ::HANDLE *pSharedHandle)
{
    using orig_call_type = OrigFuncType<decltype(&IDirect3DDevice9_CreateTexture_hook)>;

    const auto res = reinterpret_cast<orig_call_type>(
        orig_func)(that, Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);

    mandrel::log("IDirect3DDevice9::CreateTexture called {} [{}]", static_cast<void *>(*ppTexture), res);

    if (res == S_OK && *ppTexture != nullptr)
    {
        tracked_textures.track(*ppTexture);
        com_hook.add_hook<2zu>(*ppTexture, IDirect3DTexture9_Release_hook);
    }

    if (res == E_OUTOFMEMORY)
    {
        mandrel::log(
            "IDirect3DDevice9::CreateTexture failed with out of memory {} {} {}",
            Width,
            Height,
            std::to_underlying(Pool));
    }

    return res;
}

__declspec(dllexport) ::HRESULT WINAPI IDirect3DStateBlock9_Release_hook(::PROC orig_func, void *that)
{
    using orig_call_type = OrigFuncType<decltype(&IDirect3DStateBlock9_Release_hook)>;

    const auto res = reinterpret_cast<orig_call_type>(orig_func)(that);

    if (res == 0)
    {
        if (tracked_state_blocks.untrack(that))
        {
            mandrel::log("IDirect3DStateBlock9 released {}", that);
        }
    }

    return res;
}

__declspec(dllexport) ::HRESULT WINAPI IDirect3DDevice9_CreateStateBlock_hook(
    ::PROC orig_func,
    void *that,
    ::D3DSTATEBLOCKTYPE Type,
    ::IDirect3DStateBlock9 **ppSB)
{
    using orig_call_type = OrigFuncType<decltype(&IDirect3DDevice9_CreateStateBlock_hook)>;

    const auto res = reinterpret_cast<orig_call_type>(orig_func)(that, Type, ppSB);

    mandrel::log("IDirect3DDevice9::CreateStateBlock called {} [{}]", static_cast<void *>(*ppSB), res);

    com_hook.add_hook<2zu>(*ppSB, IDirect3DStateBlock9_Release_hook);

    tracked_state_blocks.track(*ppSB);

    return res;
}

__declspec(dllexport) ::HRESULT WINAPI IDirect3D9_CreateDevice_hook(
    ::PROC orig_func,
    void *that,
    ::UINT Adapter,
    ::D3DDEVTYPE DeviceType,
    ::HWND hFocusWindow,
    ::DWORD BehaviorFlags,
    ::D3DPRESENT_PARAMETERS *pPresentationParameters,
    ::IDirect3DDevice9 **ppReturnedDeviceInterface)
{
    using orig_call_type = OrigFuncType<decltype(&IDirect3D9_CreateDevice_hook)>;

    const auto res = reinterpret_cast<orig_call_type>(orig_func)(
        that, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

    com_hook.add_hook<23zu>(*ppReturnedDeviceInterface, IDirect3DDevice9_CreateTexture_hook);
    com_hook.add_hook<26zu>(*ppReturnedDeviceInterface, IDirect3DDevice9_CreateVertexBuffer_hook);
    com_hook.add_hook<27zu>(*ppReturnedDeviceInterface, IDirect3DDevice9_CreateIndexBuffer_hook);
    com_hook.add_hook<42zu>(*ppReturnedDeviceInterface, IDirect3DDevice9_EndScene_Hook);
    com_hook.add_hook<60zu>(*ppReturnedDeviceInterface, IDirect3DDevice9_CreateStateBlock_hook);
    com_hook.add_hook<100zu>(*ppReturnedDeviceInterface, IDirect3DDevice9_SetStreamSource_hook);

    mandrel::log("IDirect3D9::CreateDevice called {} [{}]", static_cast<void *>(*ppReturnedDeviceInterface), res);

    return res;
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
