#pragma once

#include <unordered_set>

#include "mandrel/allocators/std_allocator.h"

namespace mandrel
{
template <class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
using UnorderedSet = std::unordered_set<Key, Hash, KeyEqual, STDAllocator<Key>>;
}
