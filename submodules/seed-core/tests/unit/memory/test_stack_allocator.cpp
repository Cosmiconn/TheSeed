#include <doctest/doctest.h>
#include "core/memory/stack_allocator.h"
#include "core/memory/block_allocator.h"

using namespace seed::memory;

TEST_CASE("StackAllocator_BasicLIFO") {
    BlockAllocator blockAlloc;
    StackAllocator stack(&blockAlloc, 4096);

    void* p1 = stack.allocate(64, 8);
    CHECK(p1 != nullptr);

    StackAllocator::Marker m = stack.getMarker();

    void* p2 = stack.allocate(128, 16);
    CHECK(p2 != nullptr);

    void* p3 = stack.allocate(256, 32);
    CHECK(p3 != nullptr);

    // Free back to marker
    stack.freeToMarker(m);
    CHECK(stack.totalUsed() == 64);
}

TEST_CASE("StackAllocator_Reset") {
    BlockAllocator blockAlloc;
    StackAllocator stack(&blockAlloc, 4096);

    stack.allocate(100, 8);
    stack.allocate(200, 8);
    CHECK(stack.totalUsed() > 0);

    stack.reset();
    CHECK(stack.totalUsed() == 0);
}

TEST_CASE("StackAllocator_Overflow") {
    BlockAllocator blockAlloc;
    StackAllocator stack(&blockAlloc, 64);

    void* p1 = stack.allocate(32, 8);
    CHECK(p1 != nullptr);

    void* p2 = stack.allocate(64, 8); // Would overflow
    CHECK(p2 == nullptr);
}
