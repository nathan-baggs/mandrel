#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>

#include "mandrel/containers/unordered_set.h"

namespace mandrel
{

template <class T>
class ResourceTracker
{
  public:
    auto track(T resource) -> void
    {
        {
            const auto lock = std::scoped_lock{track_mutex_};
            tracked_resources_.insert(resource);
        }

        ++live_resources_;
    }

    auto untrack(T resource) -> bool
    {
        const auto lock = std::scoped_lock{track_mutex_};
        if (tracked_resources_.erase(resource) > 0)
        {
            --live_resources_;
            return true;
        }

        return false;
    }

    auto live_count() const -> std::size_t
    {
        return live_resources_;
    }

  private:
    std::mutex track_mutex_;
    UnorderedSet<T> tracked_resources_;
    std::atomic<std::size_t> live_resources_{};
};

}
