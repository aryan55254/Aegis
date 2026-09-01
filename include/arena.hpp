#pragma once
#include <stdexcept>
#include <cstddef>
#include <cstring>
#include <cstdint>

class Arena
{
private:
    static constexpr size_t MAX_ARENA_SIZE = 16 * 1024 * 1024;

    unsigned char *arena_buffer;
    size_t arena_buffer_length;
    size_t current_offset;

    // function to align the allocations for faster access and is used by arena_alloc , arena_alloc_zeroes
    static uintptr_t align_forward(uintptr_t ptr, size_t align);

public:
    Arena(const Arena &) = delete;            // not allow copy constructor : it will cause ownership errors
    Arena &operator=(const Arena &) = delete; // not allow copy assignment : too wide of a scope to decide what exactly it should do in this case

    // allow move constructor , this allows the programmer to use constructor to move the arena instance resources they referreing to a new arena instance which is this new one
    Arena(Arena &&other) noexcept
        : arena_buffer(other.arena_buffer),
          arena_buffer_length(other.arena_buffer_length),
          current_offset(other.current_offset)
    {
        // empty the old instance
        other.arena_buffer = nullptr;
        other.arena_buffer_length = 0;
        other.current_offset = 0;
    };
    // allow move assignment , if someone uses std::move or something assignment operator backed this allows them to move the resources of the one they are referencing to this new instance but with everything same
    Arena &operator=(Arena &&other) noexcept
    {
        // prevent self move
        if (this == &other)
        {
            return *this;
        };
        // clean up the destination instance
        delete[] this->arena_buffer;

        // move everything to the destination
        arena_buffer = other.arena_buffer;
        arena_buffer_length = other.arena_buffer_length;
        current_offset = other.current_offset;

        // empty the old instance
        other.arena_buffer = nullptr;
        other.arena_buffer_length = 0;
        other.current_offset = 0;

        return *this;
    };

    // normal constructor
    explicit Arena(size_t size)
        : arena_buffer(new unsigned char[size]),
          arena_buffer_length(size),
          current_offset(0)
    {
        if (size > MAX_ARENA_SIZE)
        {
            throw std::invalid_argument("Arena size exceeds maximum capacity");
        }

        arena_buffer = new unsigned char[size];
        arena_buffer_length = size;
    }

    // destructor
    ~Arena()
    {
        free_all();
        delete[] arena_buffer;
    }

    // frees up all memory
    void free_all();

    // allocate some memory needs the size for it and the power to which it aligns by the programmer
    void *arena_alloc(size_t size, size_t align = alignof(std::max_align_t));

    // same thing as arena_alloc but here it also makes sure all the memory has standard 0 stored in it by default but in case of normal arena_alloc it still stores the older shit that memory taken had this is slower because every single byte has to be zeroed but there isn't much difference in working with both of them because the either this or standard one both you will rewrite either the zeroes or the older shit when u add your own shit
    void *arena_alloc_zeroed(size_t size, size_t align = alignof(std::max_align_t));

    // this is to resize and we can only resize the last allocation either increase it or shrink it
    void *arena_resize(size_t old_size, void *old_memory, size_t new_size);

    // user help functions which returns how much memory in arena is left
    size_t capacity() const;

    // user help functipn which returns the current offset of the arena
    size_t offset() const;
};