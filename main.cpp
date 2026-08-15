#include <cstddef>
#include <cstring>

static unsigned char static_memory[1024 * 1024];
static unsigned char *arena_buffer = static_memory;
static size_t arena_buffer_length = sizeof(static_memory);
static size_t arena_offset;

void *arena_alloc(size_t size)
{
    if (arena_offset + size <= arena_buffer_length)
    {
        void *ptr = &arena_buffer[arena_offset];
        arena_offset += size;
        memset(ptr, 0, size);
        return ptr;
    }
    return NULL;
}