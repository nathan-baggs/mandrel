#include <mandrel/allocators/page_allocator.h>

#include <gtest/gtest.h>

TEST(page_allocator, allocate)
{
    auto requested_size = 0zu;
    auto test_allocator = [&requested_size](std::size_t size)
    {
        requested_size = size;
        return reinterpret_cast<void *>(0xaabbccdd);
    };
    const auto test_deallocator = [](void *) {};

    auto allocator = mandrel::PageAllocator{test_allocator, test_deallocator};
    const auto *allocation = allocator.allocate(4096zu);

    ASSERT_EQ(requested_size, 4096u);
    ASSERT_EQ(allocation, reinterpret_cast<void *>(0xaabbccdd));
}

TEST(page_allocator, deallocate)
{
    auto test_allocator = [](std::size_t) { return reinterpret_cast<void *>(0xaabbccdd); };
    void *requested_deallocation = nullptr;
    const auto test_deallocator = [&requested_deallocation](void *allocation) { requested_deallocation = allocation; };

    auto allocator = mandrel::PageAllocator{test_allocator, test_deallocator};
    allocator.deallocate(reinterpret_cast<void *>(0xaabbccdd));

    ASSERT_EQ(requested_deallocation, reinterpret_cast<void *>(0xaabbccdd));
}
