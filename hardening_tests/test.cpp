#include "../include/arena.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <utility>

void test_zero_sized_arena()
{
    constexpr size_t arena_size = 0;

    Arena arena(arena_size);

    assert(arena.capacity() == arena_size);
    assert(arena.offset() == 0);

    void *ptr = arena.arena_alloc(1);

    assert(ptr == nullptr);
    assert(arena.offset() == 0);
}

void test_tiny_arenas()
{
    const size_t arena_sizes[] = {
        1, 2, 3, 7, 8, 15, 16
    };

    for (size_t arena_size : arena_sizes)
    {
        Arena arena(arena_size);

        assert(arena.capacity() == arena_size);
        assert(arena.offset() == 0);

        void *ptr = arena.arena_alloc(arena_size);

        assert(ptr != nullptr);
        assert(arena.offset() == arena_size);

        void *failed = arena.arena_alloc(1);

        assert(failed == nullptr);
        assert(arena.offset() == arena_size);
    }
}

void test_zero_sized_allocation()
{
    constexpr size_t arena_size = 1024;

    Arena arena(arena_size);

    size_t old_offset = arena.offset();

    void *ptr = arena.arena_alloc(0);

    /*
     * Zero-sized allocations should not consume arena space.
     */
    assert(arena.offset() == old_offset);

    /*
     * The exact pointer returned for a zero-sized allocation
     * is an implementation decision, but it must not advance
     * the arena.
     */
    (void)ptr;
}

void test_zero_sized_zeroed_allocation()
{
    constexpr size_t arena_size = 1024;

    Arena arena(arena_size);

    size_t old_offset = arena.offset();

    void *ptr =
        arena.arena_alloc_zeroed(0);

    assert(arena.offset() == old_offset);

    (void)ptr;
}

void test_invalid_alignment()
{
    constexpr size_t arena_size = 4096;
    constexpr size_t allocation_size = 16;

    const size_t invalid_alignments[] = {
        3, 5, 6, 7, 10, 12
    };

    for (size_t alignment : invalid_alignments)
    {
        Arena arena(arena_size);

        size_t old_offset = arena.offset();

        void *ptr =
            arena.arena_alloc(
                allocation_size,
                alignment);

        /*
         * Alignment is required to be a power of two.
         * Invalid alignment should therefore fail without
         * modifying arena state.
         */
        assert(ptr == nullptr);
        assert(arena.offset() == old_offset);
    }
}

void test_large_alignment()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 16;

    const size_t alignments[] = {
        128, 256, 512, 1024
    };

    for (size_t alignment : alignments)
    {
        Arena arena(arena_size);

        void *ptr =
            arena.arena_alloc(
                allocation_size,
                alignment);

        if (ptr != nullptr)
        {
            uintptr_t address =
                reinterpret_cast<uintptr_t>(ptr);

            assert(address % alignment == 0);
        }
    }
}

void test_failed_allocation_preserves_state()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t first_size = 256;

    Arena arena(arena_size);

    void *ptr =
        arena.arena_alloc(first_size);

    assert(ptr != nullptr);

    size_t old_offset = arena.offset();

    void *failed =
        arena.arena_alloc(arena_size);

    assert(failed == nullptr);
    assert(arena.offset() == old_offset);
}

void test_repeated_failed_allocations()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 512;

    Arena arena(arena_size);

    void *ptr =
        arena.arena_alloc(allocation_size);

    assert(ptr != nullptr);

    size_t expected_offset = arena.offset();

    for (size_t i = 0; i < 100; ++i)
    {
        void *failed =
            arena.arena_alloc(arena_size);

        assert(failed == nullptr);
        assert(arena.offset() == expected_offset);
    }
}

void test_repeated_free_all()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 128;

    Arena arena(arena_size);

    arena.arena_alloc(allocation_size);

    for (size_t i = 0; i < 100; ++i)
    {
        arena.free_all();

        assert(arena.offset() == 0);
        assert(arena.capacity() == arena_size);
    }
}

void test_allocate_after_repeated_free_all()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 128;

    Arena arena(arena_size);

    for (size_t i = 0; i < 100; ++i)
    {
        void *ptr =
            arena.arena_alloc(allocation_size);

        assert(ptr != nullptr);
        assert(arena.offset() == allocation_size);

        arena.free_all();

        assert(arena.offset() == 0);
    }
}

void test_resize_after_free_all()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 128;
    constexpr size_t new_size = 256;

    Arena arena(arena_size);

    void *ptr =
        arena.arena_alloc(allocation_size);

    assert(ptr != nullptr);

    arena.free_all();

    assert(arena.offset() == 0);

    /*
     * The old allocation no longer belongs to the active
     * arena after free_all().
     */
    void *resized =
        arena.arena_resize(
            allocation_size,
            ptr,
            new_size);

    assert(resized == nullptr);
    assert(arena.offset() == 0);
}

void test_resize_zero_size()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 128;

    Arena arena(arena_size);

    void *ptr =
        arena.arena_alloc(allocation_size);

    assert(ptr != nullptr);

    size_t old_offset = arena.offset();

    void *resized =
        arena.arena_resize(
            allocation_size,
            ptr,
            0);

    assert(resized == ptr);
    assert(
        arena.offset() ==
        old_offset - allocation_size);
}

void test_resize_failed_grow_preserves_state()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 512;

    Arena arena(arena_size);

    void *ptr =
        arena.arena_alloc(allocation_size);

    assert(ptr != nullptr);

    size_t old_offset = arena.offset();

    void *resized =
        arena.arena_resize(
            allocation_size,
            ptr,
            arena_size + 1);

    assert(resized == nullptr);
    assert(arena.offset() == old_offset);
}

void test_resize_contents_after_shrink()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t old_size = 256;
    constexpr size_t new_size = 128;

    Arena arena(arena_size);

    void *ptr =
        arena.arena_alloc(old_size);

    assert(ptr != nullptr);

    auto *memory =
        static_cast<unsigned char *>(ptr);

    for (size_t i = 0; i < old_size; ++i)
    {
        memory[i] =
            static_cast<unsigned char>(i);
    }

    void *resized =
        arena.arena_resize(
            old_size,
            ptr,
            new_size);

    assert(resized == ptr);

    /*
     * Data inside the remaining allocation must survive.
     */
    for (size_t i = 0; i < new_size; ++i)
    {
        assert(
            memory[i] ==
            static_cast<unsigned char>(i));
    }
}

void test_resize_multiple_times()
{
    constexpr size_t arena_size = 4096;

    const size_t sizes[] = {
        64, 128, 256, 512, 1024, 256
    };

    Arena arena(arena_size);

    void *ptr =
        arena.arena_alloc(sizes[0]);

    assert(ptr != nullptr);

    size_t current_size = sizes[0];

    for (size_t i = 1;
         i < sizeof(sizes) / sizeof(sizes[0]);
         ++i)
    {
        void *resized =
            arena.arena_resize(
                current_size,
                ptr,
                sizes[i]);

        assert(resized == ptr);

        current_size = sizes[i];

        assert(arena.offset() == current_size);
    }
}

void test_move_empty_arena()
{
    constexpr size_t arena_size = 1024;

    Arena original(arena_size);

    Arena moved(std::move(original));

    assert(moved.capacity() == arena_size);
    assert(moved.offset() == 0);

    assert(original.capacity() == 0);
    assert(original.offset() == 0);
}

void test_move_assignment_empty_source()
{
    constexpr size_t source_size = 2048;
    constexpr size_t destination_size = 1024;

    Arena source(source_size);
    Arena destination(destination_size);

    destination = std::move(source);

    assert(destination.capacity() == source_size);
    assert(destination.offset() == 0);

    assert(source.capacity() == 0);
    assert(source.offset() == 0);
}

void test_move_then_allocate()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 128;

    Arena original(arena_size);

    Arena moved(std::move(original));

    void *ptr =
        moved.arena_alloc(allocation_size);

    assert(ptr != nullptr);
    assert(moved.offset() == allocation_size);
}

void test_move_assignment_then_allocate()
{
    constexpr size_t source_size = 2048;
    constexpr size_t allocation_size = 256;

    Arena source(source_size);
    Arena destination(1024);

    destination = std::move(source);

    void *ptr =
        destination.arena_alloc(allocation_size);

    assert(ptr != nullptr);
    assert(destination.offset() == allocation_size);
}

int main()
{
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "         AEGIS HARDENING TESTS\n";
    std::cout << "========================================\n";

    test_zero_sized_arena();
    std::cout << "[PASS] Zero-sized arena\n";

    test_tiny_arenas();
    std::cout << "[PASS] Tiny arenas\n";

    test_zero_sized_allocation();
    std::cout << "[PASS] Zero-sized allocation\n";

    test_zero_sized_zeroed_allocation();
    std::cout << "[PASS] Zero-sized zeroed allocation\n";

    test_invalid_alignment();
    std::cout << "[PASS] Invalid alignment\n";

    test_large_alignment();
    std::cout << "[PASS] Large alignment\n";

    test_failed_allocation_preserves_state();
    std::cout << "[PASS] Failed allocation preserves state\n";

    test_repeated_failed_allocations();
    std::cout << "[PASS] Repeated failed allocations\n";

    test_repeated_free_all();
    std::cout << "[PASS] Repeated free_all()\n";

    test_allocate_after_repeated_free_all();
    std::cout << "[PASS] Allocate after repeated free_all()\n";

    test_resize_after_free_all();
    std::cout << "[PASS] Resize after free_all()\n";

    test_resize_zero_size();
    std::cout << "[PASS] Resize to zero\n";

    test_resize_failed_grow_preserves_state();
    std::cout << "[PASS] Failed resize preserves state\n";

    test_resize_contents_after_shrink();
    std::cout << "[PASS] Resize shrink preserves contents\n";

    test_resize_multiple_times();
    std::cout << "[PASS] Repeated resize\n";

    test_move_empty_arena();
    std::cout << "[PASS] Move empty arena\n";

    test_move_assignment_empty_source();
    std::cout << "[PASS] Move assignment with empty source\n";

    test_move_then_allocate();
    std::cout << "[PASS] Allocate after move construction\n";

    test_move_assignment_then_allocate();
    std::cout << "[PASS] Allocate after move assignment\n";

    std::cout << "========================================\n";
    std::cout << "        ALL HARDENING TESTS PASSED\n";
    std::cout << "========================================\n\n";

    return 0;
}