#pragma once

#include <concepts>
#include <cstddef>
#include <utility>

namespace mandrel
{

template <class T>
concept PageAllocatorType = requires(T t, std::size_t s) {
    { t(s) } -> std::same_as<void *>;
};

template <class T>
concept PageDeallocatorType = requires(T t, void *a) {
    { t(a) } -> std::same_as<void>;
};

template <PageAllocatorType Allocator, PageDeallocatorType Deallocator>
class PageAllocator
{
  public:
    PageAllocator(Allocator allocator, Deallocator deallocator)
        : allocator_{std::move(allocator)}
        , deallocator_{std::move(deallocator)}
    {
    }

    auto allocate(std::size_t size) const -> void *
    {
        return allocator_(size);
    }

    auto deallocate(void *allocation) const -> void
    {
        deallocator_(allocation);
    }

  private:
    [[no_unique_address]] Allocator allocator_;
    [[no_unique_address]] Deallocator deallocator_;
};

}
