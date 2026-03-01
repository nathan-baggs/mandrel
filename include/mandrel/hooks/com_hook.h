#pragma once

#include <cstddef>
#include <functional>
#include <shared_mutex>
#include <tuple>
#include <utility>

#include <windows.h>

#include "mandrel/containers/vector.h"
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
        ::VirtualProtect(address, size, protection, &protection);
    }

    void *address;
    ::SIZE_T size;
    ::DWORD protection;
};
}

class COMHook
{
  public:
    template <std::size_t Index, class T, class R, class... Args>
    auto add_hook(T *obj, R(WINAPI *hook)(::PROC, void *, Args...))
    {
        auto *com_obj = reinterpret_cast<::PROC **>(obj);
        auto *com_vtable = *com_obj;
        auto *orig_func = ::PROC{};

        {
            auto lock = std::scoped_lock{mutex};
            auto hook = std::ranges::find_if(
                hooks, [com_vtable](const auto &e) { return e.vtable == com_vtable && e.index == Index; });
            if (hook != std::ranges::cend(hooks))
            {
                return;
            }
        }

        {
            const auto aw = impl::AutoWritable{com_vtable + Index, sizeof(::PROC *)};
            orig_func = std::exchange(com_vtable[Index], reinterpret_cast<::PROC>(&com_thunk<Index, R, Args...>));
        }

        {
            auto lock = std::scoped_lock{mutex};

            hooks.push_back(
                Hook{
                    .vtable = com_vtable,
                    .index = Index,
                    .orig_func = orig_func,
                    .hook_func = reinterpret_cast<::PROC>(hook),
                });
        }

        mandrel::log(
            "COM hook installed {} [{}] -> {}", static_cast<void *>(com_vtable), Index, reinterpret_cast<void *>(hook));
    }

  private:
    template <std::size_t Index, class R, class... Args>
    static auto WINAPI com_thunk(void *that, Args... args) -> R
    {
        auto *com_obj = reinterpret_cast<::PROC **>(that);
        auto *com_vtable = *com_obj;

        Hook h{};

        {
            auto lock = std::shared_lock(mutex);

            auto hook = std::ranges::find_if(
                hooks, [com_vtable](const auto &e) { return e.vtable == com_vtable && e.index == Index; });
            ensure(hook != std::ranges::cend(hooks), "could not find hook");

            h = *hook;
        }

        using hook_call_type = R(WINAPI *)(::PROC, void *, Args...);

        return reinterpret_cast<hook_call_type>(h.hook_func)(h.orig_func, that, args...);
    }

    struct Hook
    {
        void *vtable;
        std::size_t index;
        ::PROC orig_func;
        ::PROC hook_func;
    };

    static inline Vector<Hook> hooks;
    static inline std::shared_mutex mutex;
};

}
