#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <new>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace mandrel
{

template <class PageAllocator>
class FreeListAllocator
{
  public:
    struct alignas(std::max_align_t) Node
    {
        std::size_t size;
        Node *next;
    };
    static_assert(alignof(Node) == alignof(std::max_align_t));
    static_assert(std::is_trivially_destructible_v<Node>);

    class Iterator
    {
      public:
        using difference_type = std::ptrdiff_t;
        using value_type = Node;

        Iterator()
            : n_{nullptr}
        {
        }

        Iterator(Node *n)
            : n_{n}
        {
        }

        auto operator*() const -> value_type &
        {
            return *n_;
        }

        auto operator++() -> Iterator &
        {
            n_ = n_->next;
            return *this;
        }

        auto operator++(int) -> Iterator
        {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        auto operator==(const Iterator &) const -> bool = default;

      private:
        value_type *n_;
    };

    static_assert(std::forward_iterator<Iterator>);

    using iterator = Iterator;
    using const_iterator = const iterator;

    FreeListAllocator(PageAllocator page_allocator, std::size_t capacity)
        : page_allocator_{std::move(page_allocator)}
        , page_{page_allocator_.allocate(capacity)}
        , head_{std::ranges::construct_at(reinterpret_cast<Node *>(page_), capacity - sizeof(Node), nullptr)}
    {
    }

    ~FreeListAllocator()
    {
        page_allocator_.deallocate(page_);
    }

    auto allocate(std::size_t size) -> void *
    {
        auto **cursor = &head_;
        while (*cursor != nullptr)
        {
            if ((*cursor)->size >= size)
            {
                break;
            }

            cursor = &(*cursor)->next;
        }

        if (*cursor == nullptr)
        {
            throw std::bad_alloc();
        }
        else if ((*cursor)->size < size + sizeof(Node))
        {
            auto allocation = reinterpret_cast<std::byte *>(*cursor) + sizeof(Node);
            *cursor = (*cursor)->next;

            return allocation;
        }
        else
        {
            void *new_free_addr = reinterpret_cast<std::byte *>(*cursor) + sizeof(Node) + size;
            auto remaining_space = (*cursor)->size - size;
            if (!std::align(alignof(Node), sizeof(Node), new_free_addr, remaining_space))
            {
                auto allocation = reinterpret_cast<std::byte *>(*cursor) + sizeof(Node);
                *cursor = (*cursor)->next;

                return allocation;
            }

            auto new_free = Node{.size = remaining_space - sizeof(Node), .next = (*cursor)->next};

            std::construct_at(static_cast<Node *>(new_free_addr), new_free);

            (*cursor)->size =
                reinterpret_cast<std::byte *>(new_free_addr) - (reinterpret_cast<std::byte *>(*cursor) + sizeof(Node));

            auto allocation = reinterpret_cast<std::byte *>(*cursor) + sizeof(Node);
            *cursor = static_cast<Node *>(new_free_addr);

            return allocation;
        }
    }

    auto deallocate(void *allocation) -> void
    {
        if (allocation == nullptr)
        {
            return;
        }

        const auto allocation_addr_byte = reinterpret_cast<std::byte *>(allocation);
        auto *node = reinterpret_cast<Node *>(allocation_addr_byte - sizeof(Node));
        const auto *node_end = reinterpret_cast<std::byte *>(node) + sizeof(Node) + node->size;

        if (head_ == nullptr || node < head_)
        {
            node->next = head_;
            head_ = node;

            if (node->next != nullptr && node_end == reinterpret_cast<std::byte *>(node->next))
            {
                node->size += node->next->size + sizeof(Node);
                node->next = node->next->next;
            }

            return;
        }

        auto *cursor = head_;
        while (cursor->next != nullptr && cursor->next < node)
        {
            cursor = cursor->next;
        }

        node->next = cursor->next;

        if (node->next != nullptr && node_end == reinterpret_cast<std::byte *>(node->next))
        {
            node->size += sizeof(Node) + node->next->size;
            node->next = node->next->next;
        }

        const auto *cursor_end = reinterpret_cast<std::byte *>(cursor) + sizeof(Node) + cursor->size;
        if (cursor_end == reinterpret_cast<std::byte *>(node))
        {
            cursor->size += sizeof(Node) + node->size;
            cursor->next = node->next;
        }
        else
        {
            cursor->next = node;
        }
    }

    auto begin() -> iterator
    {
        return {head_};
    }

    auto begin() const -> const_iterator
    {
        return {head_};
    }

    auto cbegin() const -> const_iterator
    {
        return {head_};
    }

    auto end() -> iterator
    {
        return {};
    }

    auto end() const -> const_iterator
    {
        return {};
    }

    auto cend() const -> const_iterator
    {
        return {};
    }

  private:
    [[no_unique_address]] PageAllocator page_allocator_;
    void *page_;
    Node *head_;
};

}
