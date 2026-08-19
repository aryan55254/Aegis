#pragma once

#include <cstddef>
#include <cstring>
#include <cstdint>
#include <fmt/core.h>

class Arena
{
private:
    unsigned char *arena_buffer;
    size_t arena_buffer_length;
    size_t current_offset;

public:
    Arena(const Arena &) = delete;            // not allow copy constructor : it will cause ownership errors
    Arena &operator=(const Arena &) = delete; // not allow copy assignment : too wide of a scope to decide what exactly it should do in this case

    explicit Arena(size_t size)
    {
        if (size > 1024 * 1024)
        {
            fmt::print("Allocations above 1MB not allowed as of now");
        }
        arena_buffer = new unsigned char[size];
        arena_buffer_length = size;
        current_offset = 0;
    }

    ~Arena()
    {
        free_all();
        delete[] arena_buffer;
    }

    void free_all();

    static uintptr_t align_forward(uintptr_t ptr, size_t align);

    void *arena_alloc(size_t size, size_t align = alignof(std::max_align_t));

    void *arena_alloc_zeroed(size_t size, size_t align = alignof(std::max_align_t));

    void *arena_resize(size_t old_size, void *old_memory, size_t new_size, size_t new_align);
};