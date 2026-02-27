#include <cstddef>
#include <ranges>

#include <windows.h>

#include <gtest/gtest.h>

#include <mandrel/allocators/free_list_allocator.h>
#include <mandrel/allocators/page_allocator.h>

struct VirtualAllocHelper
{
    auto operator()(std::size_t size) const -> void *
    {
        return ::VirtualAlloc(nullptr, size, MEM_COMMIT, PAGE_READWRITE);
    }
};

struct VirtualDeallocHelper
{
    auto operator()(void *allocation) const -> void
    {
        ::VirtualFree(allocation, 0, MEM_RELEASE);
    }
};

constexpr auto initial_usable_capacity() -> std::size_t
{
    return 4096zu -
           sizeof(mandrel::FreeListAllocator<mandrel::PageAllocator<VirtualAllocHelper, VirtualDeallocHelper>>::Node);
}

TEST(free_list_allocator, constructor)
{
    auto page_allocator = mandrel::PageAllocator{VirtualAllocHelper{}, VirtualDeallocHelper{}};
    auto free_list_allocator = mandrel::FreeListAllocator{std::move(page_allocator), 4096zu};

    ASSERT_EQ(std::ranges::distance(free_list_allocator), 1zu);
    ASSERT_EQ((*std::ranges::cbegin(free_list_allocator)).size, initial_usable_capacity());
}

TEST(free_list_allocator, allocate_basic_split)
{
    auto page_allocator = mandrel::PageAllocator{VirtualAllocHelper{}, VirtualDeallocHelper{}};
    auto free_list_allocator = mandrel::FreeListAllocator{std::move(page_allocator), 4096zu};

    auto *ptr = free_list_allocator.allocate(100zu);
    ASSERT_NE(ptr, nullptr);

    ASSERT_EQ(std::ranges::distance(free_list_allocator), 1zu);
    ASSERT_LT((*std::ranges::cbegin(free_list_allocator)).size, initial_usable_capacity());
}

TEST(free_list_allocator, allocate_throws_bad_alloc)
{
    auto page_allocator = mandrel::PageAllocator{VirtualAllocHelper{}, VirtualDeallocHelper{}};
    auto free_list_allocator = mandrel::FreeListAllocator{std::move(page_allocator), 4096zu};

    EXPECT_THROW(free_list_allocator.allocate(5000zu), std::bad_alloc);
}

TEST(free_list_allocator, allocate_exact_exhaustion)
{
    auto page_allocator = mandrel::PageAllocator{VirtualAllocHelper{}, VirtualDeallocHelper{}};
    auto free_list_allocator = mandrel::FreeListAllocator{std::move(page_allocator), 4096zu};

    auto *ptr = free_list_allocator.allocate(initial_usable_capacity());
    ASSERT_NE(ptr, nullptr);
    ASSERT_EQ(std::ranges::distance(free_list_allocator), 0zu);
}

TEST(free_list_allocator, allocate_no_split_due_to_node_size)
{
    auto page_allocator = mandrel::PageAllocator{VirtualAllocHelper{}, VirtualDeallocHelper{}};
    auto free_list_allocator = mandrel::FreeListAllocator{std::move(page_allocator), 4096zu};

    const auto request_size = initial_usable_capacity() - (sizeof(decltype(free_list_allocator)::Node) - 1zu);
    auto *ptr = free_list_allocator.allocate(request_size);

    ASSERT_NE(ptr, nullptr);
    ASSERT_EQ(std::ranges::distance(free_list_allocator), 0zu);
}

TEST(free_list_allocator, deallocate_nullptr_is_noop)
{
    auto page_allocator = mandrel::PageAllocator{VirtualAllocHelper{}, VirtualDeallocHelper{}};
    auto free_list_allocator = mandrel::FreeListAllocator{std::move(page_allocator), 4096zu};

    free_list_allocator.deallocate(nullptr);
    ASSERT_EQ(std::ranges::distance(free_list_allocator), 1zu);
}

TEST(free_list_allocator, deallocate_basic_restore)
{
    auto page_allocator = mandrel::PageAllocator{VirtualAllocHelper{}, VirtualDeallocHelper{}};
    auto free_list_allocator = mandrel::FreeListAllocator{std::move(page_allocator), 4096zu};

    auto *ptr = free_list_allocator.allocate(100zu);
    free_list_allocator.deallocate(ptr);

    ASSERT_EQ(std::ranges::distance(free_list_allocator), 1zu);
    ASSERT_EQ((*std::ranges::cbegin(free_list_allocator)).size, initial_usable_capacity());
}

TEST(free_list_allocator, deallocate_merge_right)
{
    auto page_allocator = mandrel::PageAllocator{VirtualAllocHelper{}, VirtualDeallocHelper{}};
    auto free_list_allocator = mandrel::FreeListAllocator{std::move(page_allocator), 4096zu};

    auto *ptr1 = free_list_allocator.allocate(100zu);
    auto *ptr2 = free_list_allocator.allocate(100zu);
    auto *ptr3 = free_list_allocator.allocate(100zu);

    free_list_allocator.deallocate(ptr2);
    ASSERT_EQ(std::ranges::distance(free_list_allocator), 2zu);

    free_list_allocator.deallocate(ptr1);
    ASSERT_EQ(std::ranges::distance(free_list_allocator), 2zu);

    free_list_allocator.deallocate(ptr3);
    ASSERT_EQ(std::ranges::distance(free_list_allocator), 1zu);
    ASSERT_EQ((*std::ranges::cbegin(free_list_allocator)).size, initial_usable_capacity());
}

TEST(free_list_allocator, deallocate_merge_left)
{
    auto page_allocator = mandrel::PageAllocator{VirtualAllocHelper{}, VirtualDeallocHelper{}};
    auto free_list_allocator = mandrel::FreeListAllocator{std::move(page_allocator), 4096zu};

    auto *ptr1 = free_list_allocator.allocate(100zu);
    auto *ptr2 = free_list_allocator.allocate(100zu);
    auto *ptr3 = free_list_allocator.allocate(100zu);

    free_list_allocator.deallocate(ptr1);
    ASSERT_EQ(std::ranges::distance(free_list_allocator), 2zu);

    free_list_allocator.deallocate(ptr2);
    ASSERT_EQ(std::ranges::distance(free_list_allocator), 2zu);

    free_list_allocator.deallocate(ptr3);
    ASSERT_EQ(std::ranges::distance(free_list_allocator), 1zu);
}

TEST(free_list_allocator, deallocate_merge_both_sides)
{
    auto page_allocator = mandrel::PageAllocator{VirtualAllocHelper{}, VirtualDeallocHelper{}};
    auto free_list_allocator = mandrel::FreeListAllocator{std::move(page_allocator), 4096zu};

    auto *ptr1 = free_list_allocator.allocate(100zu);
    auto *ptr2 = free_list_allocator.allocate(100zu);
    auto *ptr3 = free_list_allocator.allocate(100zu);
    auto *ptr4 = free_list_allocator.allocate(100zu);

    free_list_allocator.deallocate(ptr1);
    free_list_allocator.deallocate(ptr3);
    ASSERT_EQ(std::ranges::distance(free_list_allocator), 3zu);

    free_list_allocator.deallocate(ptr2);

    ASSERT_EQ(std::ranges::distance(free_list_allocator), 2zu);

    free_list_allocator.deallocate(ptr4);
    ASSERT_EQ(std::ranges::distance(free_list_allocator), 1zu);
    ASSERT_EQ((*std::ranges::cbegin(free_list_allocator)).size, initial_usable_capacity());
}

TEST(free_list_allocator, alignment_padding_is_preserved)
{
    auto page_allocator = mandrel::PageAllocator{VirtualAllocHelper{}, VirtualDeallocHelper{}};
    auto free_list_allocator = mandrel::FreeListAllocator{std::move(page_allocator), 4096zu};

    auto *ptr1 = free_list_allocator.allocate(1zu);
    auto *ptr2 = free_list_allocator.allocate(1zu);

    auto diff = reinterpret_cast<std::byte *>(ptr2) - reinterpret_cast<std::byte *>(ptr1);

    ASSERT_GE(static_cast<std::size_t>(diff), sizeof(decltype(free_list_allocator)::Node) + 1zu);

    auto ptr2_val = reinterpret_cast<std::uintptr_t>(ptr2);
    ASSERT_EQ(ptr2_val % alignof(std::max_align_t), 0zu);

    free_list_allocator.deallocate(ptr1);
    free_list_allocator.deallocate(ptr2);
    ASSERT_EQ(std::ranges::distance(free_list_allocator), 1zu);
    ASSERT_EQ((*std::ranges::cbegin(free_list_allocator)).size, initial_usable_capacity());
}
