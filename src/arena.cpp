#include "../include/arena.hpp"

// memory alignment must be by power of two (1 2 4 8 16...) so in this function we check if they are divisible and if not we align them for faster access
uintptr_t Arena::align_forward(uintptr_t ptr, size_t align)
{
    size_t remainder = ptr % align;
    if (remainder == 0)
        return ptr;
    return ptr + (align - remainder);
}

void *Arena::arena_alloc(size_t size, size_t align)
{
    uintptr_t current_ptr = (uintptr_t)&arena_buffer[current_offset]; // we grab the pointer to the next available byte in the arena and convert it to an integer to perform arithmentic on it
    uintptr_t aligned_ptr = align_forward(current_ptr, align);        // aligns the pointer to nearest multiple of of align

    size_t padding = aligned_ptr - current_ptr; // calculates sacrifical bytes to align

    // check if there is space vailable
    if (current_offset + padding + size <= arena_buffer_length)
    {
        // increase the offset and return the pointer to the memory
        current_offset += padding + size;
        void *ptr = (void *)aligned_ptr;
        return ptr;
    }

    return nullptr;
}

// in this we do the exact same shit as arena alloc but also makes sure all the memory has standard 0 stored in it by default via memset which is not done in fucntion above it still has random shit stored there but it doesn't matter coz when programmer use this memory the programmer will rewrite it anyways
void *Arena::arena_alloc_zeroed(size_t size, size_t align)
{
    uintptr_t current_ptr = (uintptr_t)&arena_buffer[current_offset];
    uintptr_t aligned_ptr = align_forward(current_ptr, align);

    size_t padding = aligned_ptr - current_ptr;

    if (current_offset + padding + size <= arena_buffer_length)
    {
        current_offset += padding + size;
        void *ptr = (void *)aligned_ptr;
        memset(ptr, 0, size);
        return ptr;
    }

    return nullptr;
}

// frees all memory
void Arena::free_all()
{
    current_offset = 0;
}

// can only grow or shrink the last allocation nothing else can be done
void *Arena::arena_resize(size_t old_size, void *old_memory, size_t new_size)
{
    // null ptrs are not valid here
    if (old_memory == nullptr)
    {
        return nullptr;
    }

    uintptr_t current_ptr = (uintptr_t)old_memory;

    // check if this is the last allocation made in the arena
    if (current_ptr + old_size != (uintptr_t)&arena_buffer[current_offset])
    {
        return nullptr;
    }

    // if size is the same then nothing needs to change
    if (new_size == old_size)
    {
        return old_memory;
    }

    // if new size is smaller then give the unused space back to the arena
    if (new_size < old_size)
    {
        size_t size_diff = old_size - new_size;
        current_offset -= size_diff;
        return old_memory;
    }

    // if new size is bigger then try to extend the allocation forward
    size_t size_diff = new_size - old_size;

    if (current_offset + size_diff <= arena_buffer_length)
    {
        current_offset += size_diff;

        // zero only the newly added memory
        memset((unsigned char *)current_ptr + old_size, 0, size_diff);

        return old_memory;
    }

    return nullptr;
}

// returns arena buffer length
size_t Arena::capacity() const
{
    return arena_buffer_length;
}

// returns current arena offset
size_t Arena::offset() const
{
    return current_offset;
}