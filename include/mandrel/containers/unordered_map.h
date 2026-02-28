#pragma once

#include <unordered_map>
#include <winscard.h>

#include "mandrel/allocators/std_allocator.h"

namespace mandrel
{
template <class Key, class T>
using UnorderedMap =
    std::unordered_map<Key, T, std::hash<Key>, std::equal_to<Key>, STDAllocator<std::pair<const Key, T>>>;
}
