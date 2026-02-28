#pragma once

#include <cstddef>
#include <shared_mutex>

#include <utility>
#include <windows.h>

#include "mandrel/containers/unordered_map.h"
#include "mandrel/utils.h"

namespace mandrel
{

namespace impl
{

struct AutoWritable
{
    AutoWritable(void *address, ::SIZE_T size)
        : address{address}
        , size{size}
        , protection{}
    {
        ensure(
            ::VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &protection) != 0,
            "failed to change memory protection");
    }

    ~AutoWritable()
    {
        ensure(::VirtualProtect(address, size, protection, &protection) != 0, "failed to restore memory protection");
    }

    void *address;
    ::SIZE_T size;
    ::DWORD protection;
};
}

class COMHook
{
  public:
    template <class T, class R, class... Args>
    auto add_hook(std::size_t index, T *obj, R(WINAPI *hook)(::PROC, void *, Args...))
    {
        auto *com_obj = reinterpret_cast<::PROC **>(obj);
        auto *com_vtable = *com_obj;
        auto *orig_func = ::PROC{};

        {
            const auto aw = impl::AutoWritable{com_vtable + index, sizeof(::PROC *)};
            orig_func = std::exchange(com_vtable[index], reinterpret_cast<::PROC>(&com_thunk<R, Args...>));
        }

        {
            auto lock = std::scoped_lock{mutex};
            hooks_lookup[obj] = Hook{.orig_func = orig_func, .hook_func = reinterpret_cast<::PROC>(hook)};
        }

        mandrel::log(
            "COM hook installed {} [{}] -> {}", static_cast<void *>(obj), index, reinterpret_cast<void *>(hook));
    }

  private:
    template <class R, class... Args>
    static auto WINAPI com_thunk(void *that, Args... args) -> R
    {
        auto hook = std::ranges::cend(hooks_lookup);

        {
            auto lock = std::shared_lock(mutex);
            hook = hooks_lookup.find(that);
        }

        ensure(hook != std::ranges::cend(hooks_lookup), "could not find hook");

        using hook_call_type = R(WINAPI *)(::PROC, void *, Args...);

        const auto [key, value] = *hook;
        const auto [orig_func, hook_func] = value;
        return reinterpret_cast<hook_call_type>(hook_func)(orig_func, that, args...);
    }

    struct Hook
    {
        ::PROC orig_func;
        ::PROC hook_func;
    };

    static inline UnorderedMap<void *, Hook> hooks_lookup;
    static inline std::shared_mutex mutex;
};

}
