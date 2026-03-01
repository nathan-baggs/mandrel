#pragma once

#include <sstream>

#include "mandrel/allocators/std_allocator.h"

namespace mandrel
{

using StringStream = std::basic_stringstream<char, std::char_traits<char>, STDAllocator<char>>;

}
