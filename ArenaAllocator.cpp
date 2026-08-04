#include "ArenaAllocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cassert>

// constructor
Arena::Arena() {
    bump_pointer = new char[ARENA_SIZE];
    arena_start = bump_pointer;
}

// destructor
Arena::~Arena() {
    delete[] arena_start;
}

// allocator
void* Arena::allocate(size_t size, uintptr_t alignment) {
    assert(alignment > 0 && ((alignment - 1) & alignment) == 0);

    char* aligned_ptr = (char*)(((uintptr_t)bump_pointer + alignment - 1) & ~(alignment - 1));

    if (aligned_ptr + size > arena_start + ARENA_SIZE) {
        return nullptr;
    }

    bump_pointer = aligned_ptr + size;
    return aligned_ptr;
}

// reset arena
void Arena::reset() {
    bump_pointer = arena_start;
}