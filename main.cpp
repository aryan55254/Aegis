#include <cstddef>
#include <cstring>
#include <cstdint>

static unsigned char static_memory[1024 * 1024];
static unsigned char *arena_buffer = static_memory;
static size_t arena_buffer_length = sizeof(static_memory);
static size_t arena_offset;

// memory alignment must be by power of two (1 2 4 8 16...) so in this function we check if they are divisible and if not we align them for faster access
static uintptr_t align_forward(uintptr_t ptr, size_t align)
{
    size_t remainder = ptr % align;
    if (remainder == 0)
        return ptr;
    return ptr + (align - remainder);
}

void *arena_alloc(size_t size, size_t align = alignof(std::max_align_t))
{
    uintptr_t current_ptr = (uintptr_t)&arena_buffer[arena_offset]; // we grab the pointer to the next available pyte in the arena and convert it to an integer to perform arithmentic on it
    uintptr_t aligned_ptr = align_forward(current_ptr, align);      // aligns the pointer to nearest multiple of of align

    size_t padding = aligned_ptr - current_ptr; // calculates sacrifical bytes to align

    if (arena_offset + padding + size <= arena_buffer_length)
    {
        arena_offset += padding + size;
        void *ptr = (void *)aligned_ptr;
        memset(ptr, 0, size);
        return ptr;
    }

    return nullptr;
}