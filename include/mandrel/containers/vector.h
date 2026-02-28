#pragma once

#include <vector>

#include "mandrel/allocators/std_allocator.h"

namespace mandrel
{
template <class T>
using Vector = std::vector<T, STDAllocator<T>>;
}
