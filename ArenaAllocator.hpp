#pragma once

#include <cstddef>
#include <cstdint>

class Arena {
private:
    char* bump_pointer;
    char* arena_start;
    // 4GB capacity = 4ULL * 1024 * 1024 * 1024
    static constexpr size_t ARENA_SIZE = 1024;

public:
    Arena();
    ~Arena();
    void* allocate(size_t size, uintptr_t alignment);
    void reset();
};
