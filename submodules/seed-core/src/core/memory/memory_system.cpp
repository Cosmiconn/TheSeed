#include "core/memory/memory_system.h"

namespace seed::memory {

BlockAllocator*  g_blockAllocator  = nullptr;
MemoryTracker*   g_memoryTracker   = nullptr;
ArenaAllocator*  g_frameArena      = nullptr;

} // namespace seed::memory
