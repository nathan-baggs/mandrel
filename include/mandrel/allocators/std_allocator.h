#pragma once

#include <cstddef>
#include <type_traits>

#include "mandrel/allocators/global_allocator.h"

namespace mandrel
{

template <class T>
class STDAllocator
{
  public:
    using value_type = T;
    using pointer = value_type *;
    using is_always_equal = std::true_type;

    STDAllocator() = default;

    template <class U>
    constexpr STDAllocator(const STDAllocator<U> &)
    {
    }

    [[nodiscard]] auto allocate(std::size_t n) -> T *
    {
        return static_cast<T *>(GlobalAllocator::allocate(n * sizeof(T)));
    }

    auto deallocate(T *p, std::size_t) -> void
    {
        GlobalAllocator::deallocate(p);
    }
};

template <class T, class U>
auto operator==(const STDAllocator<T> &, const STDAllocator<U> &) -> bool
{
    return true;
};

template <class T, class U>
auto operator!=(const STDAllocator<T> &, const STDAllocator<U> &) -> bool
{
    return false;
};
}
