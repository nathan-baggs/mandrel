#pragma once

#include <unordered_map>

#include "mandrel/allocators/std_allocator.h"

namespace mandrel
{
template <class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
using UnorderedMap = std::unordered_map<Key, T, Hash, KeyEqual, STDAllocator<std::pair<const Key, T>>>;
}
