#pragma once

#include <stacktrace>

#include "mandrel/allocators/std_allocator.h"

namespace mandrel
{

using StackTrace = std::basic_stacktrace<STDAllocator<std::stacktrace_entry>>;

}
