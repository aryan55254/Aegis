#include <cstddef>
#include <cstring>
#include <cstdint>

static unsigned char static_memory[1024 * 1024];
static unsigned char *arena_buffer = static_memory;
static size_t arena_buffer_length = sizeof(static_memory);
static size_t current_offset;
static size_t prev_offset;

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
    uintptr_t current_ptr = (uintptr_t)&arena_buffer[current_offset]; // we grab the pointer to the next available byte in the arena and convert it to an integer to perform arithmentic on it
    uintptr_t aligned_ptr = align_forward(current_ptr, align);        // aligns the pointer to nearest multiple of of align

    size_t padding = aligned_ptr - current_ptr; // calculates sacrifical bytes to align

    if (current_offset + padding + size <= arena_buffer_length)
    {
        prev_offset = current_offset;
        current_offset += padding + size;
        void *ptr = (void *)aligned_ptr;
        memset(ptr, 0, size);
        return ptr;
    }

    return nullptr;
}

void free_all()
{
    prev_offset = current_offset;
    current_offset = 0;
}

// can only rezise the last allocaton and only increase it not discrease it
void *arena_resize(size_t old_size, void *old_memory, size_t new_size, size_t new_align)
{

    if (old_memory == NULL)
    {
        return arena_alloc(new_size, new_align);
    }

    if (new_size <= old_size)
    {
        return old_memory;
    }

    uintptr_t current_ptr = (uintptr_t)old_memory;

    if (current_ptr + old_size == (uintptr_t)&arena_buffer[current_offset])
    {
        size_t size_diff = new_size - old_size;
        if (current_offset + size_diff <= arena_buffer_length)
        {
            prev_offset = current_offset;
            current_offset += size_diff;
            void *ptr = (void *)current_ptr;
            memset(ptr, 0, size_diff);
            return ptr;
        }
        return nullptr;
    }
    return nullptr;
}