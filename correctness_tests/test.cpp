#include "../include/arena.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <utility>

void test_basic_allocation()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 64;

    Arena arena(arena_size);

    size_t old_offset = arena.offset();

    void *ptr = arena.arena_alloc(allocation_size);

    assert(ptr != nullptr);
    assert(arena.capacity() == arena_size);
    assert(arena.offset() == old_offset + allocation_size);

    // Make sure allocated memory is actually usable.
    auto *memory = static_cast<unsigned char *>(ptr);

    for (size_t i = 0; i < allocation_size; ++i)
    {
        memory[i] = static_cast<unsigned char>(i);
    }

    for (size_t i = 0; i < allocation_size; ++i)
    {
        assert(memory[i] == static_cast<unsigned char>(i));
    }
}

void test_multiple_allocations()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 64;
    constexpr size_t allocation_count = 4;

    Arena arena(arena_size);

    void *allocations[allocation_count];

    size_t expected_offset = arena.offset();

    for (size_t i = 0; i < allocation_count; ++i)
    {
        allocations[i] = arena.arena_alloc(allocation_size);

        assert(allocations[i] != nullptr);

        expected_offset += allocation_size;

        assert(arena.offset() == expected_offset);
    }

    for (size_t i = 0; i < allocation_count; ++i)
    {
        for (size_t j = i + 1; j < allocation_count; ++j)
        {
            uintptr_t first =
                reinterpret_cast<uintptr_t>(allocations[i]);

            uintptr_t second =
                reinterpret_cast<uintptr_t>(allocations[j]);

            assert(first + allocation_size <= second ||
                   second + allocation_size <= first);
        }
    }
}

void test_alignment()
{
    constexpr size_t arena_size = 4096;
    constexpr size_t allocation_size = 16;

    const size_t alignments[] = {
        1, 2, 4, 8, 16, 32, 64};

    for (size_t alignment : alignments)
    {
        Arena arena(arena_size);

        void *ptr =
            arena.arena_alloc(allocation_size, alignment);

        assert(ptr != nullptr);

        uintptr_t address =
            reinterpret_cast<uintptr_t>(ptr);

        assert(address % alignment == 0);
    }
}

void test_zeroed_allocation()
{
    constexpr size_t arena_size = 1024;

    const size_t allocation_sizes[] = {
        1, 8, 16, 31, 64, 127, 256};

    for (size_t allocation_size : allocation_sizes)
    {
        Arena arena(arena_size);

        void *ptr =
            arena.arena_alloc_zeroed(allocation_size);

        assert(ptr != nullptr);

        auto *memory =
            static_cast<unsigned char *>(ptr);

        for (size_t i = 0; i < allocation_size; ++i)
        {
            assert(memory[i] == 0);
        }
    }
}

void test_bounds()
{
    constexpr size_t arena_size = 1024;

    Arena arena(arena_size);

    void *ptr = arena.arena_alloc(arena_size);

    assert(ptr != nullptr);
    assert(arena.offset() == arena_size);

    size_t old_offset = arena.offset();

    void *failed = arena.arena_alloc(1);

    assert(failed == nullptr);
    assert(arena.offset() == old_offset);
}

void test_partial_bounds()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 256;

    Arena arena(arena_size);

    size_t expected_offset = 0;

    while (expected_offset + allocation_size <= arena_size)
    {
        void *ptr = arena.arena_alloc(allocation_size);

        assert(ptr != nullptr);

        expected_offset += allocation_size;

        assert(arena.offset() == expected_offset);
    }

    size_t old_offset = arena.offset();

    void *failed = arena.arena_alloc(allocation_size);

    assert(failed == nullptr);
    assert(arena.offset() == old_offset);
}

void test_free_all()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 256;

    Arena arena(arena_size);

    void *ptr = arena.arena_alloc(allocation_size);

    assert(ptr != nullptr);
    assert(arena.offset() == allocation_size);

    arena.free_all();

    assert(arena.offset() == 0);

    void *new_ptr = arena.arena_alloc(allocation_size);

    assert(new_ptr != nullptr);
    assert(arena.offset() == allocation_size);
}

void test_resize_same_size()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 128;

    Arena arena(arena_size);

    void *ptr = arena.arena_alloc(allocation_size);

    assert(ptr != nullptr);

    size_t old_offset = arena.offset();

    void *resized =
        arena.arena_resize(
            allocation_size,
            ptr,
            allocation_size);

    assert(resized == ptr);
    assert(arena.offset() == old_offset);
}

void test_resize_grow()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t old_size = 128;
    constexpr size_t new_size = 256;

    Arena arena(arena_size);

    void *ptr = arena.arena_alloc(old_size);

    assert(ptr != nullptr);

    auto *memory =
        static_cast<unsigned char *>(ptr);

    for (size_t i = 0; i < old_size; ++i)
    {
        memory[i] = 0xAB;
    }

    size_t old_offset = arena.offset();

    void *resized =
        arena.arena_resize(
            old_size,
            ptr,
            new_size);

    assert(resized == ptr);

    size_t expected_offset =
        old_offset + (new_size - old_size);

    assert(arena.offset() == expected_offset);

    // Existing data must survive the resize.
    for (size_t i = 0; i < old_size; ++i)
    {
        assert(memory[i] == 0xAB);
    }

    // Newly added memory should be zeroed.
    for (size_t i = old_size; i < new_size; ++i)
    {
        assert(memory[i] == 0);
    }
}

void test_resize_shrink()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t old_size = 256;
    constexpr size_t new_size = 128;

    Arena arena(arena_size);

    void *ptr = arena.arena_alloc(old_size);

    assert(ptr != nullptr);

    size_t old_offset = arena.offset();

    void *resized =
        arena.arena_resize(
            old_size,
            ptr,
            new_size);

    assert(resized == ptr);

    size_t expected_offset =
        old_offset - (old_size - new_size);

    assert(arena.offset() == expected_offset);
}

void test_resize_non_last_allocation()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 128;
    constexpr size_t new_size = 256;

    Arena arena(arena_size);

    void *first = arena.arena_alloc(allocation_size);
    void *second = arena.arena_alloc(allocation_size);

    assert(first != nullptr);
    assert(second != nullptr);

    size_t old_offset = arena.offset();

    void *resized =
        arena.arena_resize(
            allocation_size,
            first,
            new_size);

    assert(resized == nullptr);
    assert(arena.offset() == old_offset);
}

void test_resize_null_pointer()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t old_size = 128;
    constexpr size_t new_size = 256;

    Arena arena(arena_size);

    size_t old_offset = arena.offset();

    void *resized =
        arena.arena_resize(
            old_size,
            nullptr,
            new_size);

    assert(resized == nullptr);
    assert(arena.offset() == old_offset);
}

void test_resize_beyond_capacity()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t old_size = 512;
    constexpr size_t new_size = 1025;

    Arena arena(arena_size);

    void *ptr = arena.arena_alloc(old_size);

    assert(ptr != nullptr);

    size_t old_offset = arena.offset();

    void *resized =
        arena.arena_resize(
            old_size,
            ptr,
            new_size);

    assert(resized == nullptr);
    assert(arena.offset() == old_offset);
}

void test_move_constructor()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 128;

    Arena original(arena_size);

    void *ptr =
        original.arena_alloc(allocation_size);

    assert(ptr != nullptr);

    size_t original_offset = original.offset();
    size_t original_capacity = original.capacity();

    Arena moved(std::move(original));

    assert(moved.offset() == original_offset);
    assert(moved.capacity() == original_capacity);

    assert(original.offset() == 0);
    assert(original.capacity() == 0);
}

void test_move_assignment()
{
    constexpr size_t source_size = 2048;
    constexpr size_t destination_size = 1024;
    constexpr size_t allocation_size = 256;

    Arena source(source_size);

    void *ptr =
        source.arena_alloc(allocation_size);

    assert(ptr != nullptr);

    size_t source_offset = source.offset();
    size_t source_capacity = source.capacity();

    Arena destination(destination_size);

    destination.arena_alloc(128);

    destination = std::move(source);

    assert(destination.offset() == source_offset);
    assert(destination.capacity() == source_capacity);

    assert(source.offset() == 0);
    assert(source.capacity() == 0);
}

void test_self_move_assignment()
{
    constexpr size_t arena_size = 1024;
    constexpr size_t allocation_size = 128;

    Arena arena(arena_size);

    void *ptr =
        arena.arena_alloc(allocation_size);

    assert(ptr != nullptr);

    size_t old_offset = arena.offset();
    size_t old_capacity = arena.capacity();

    arena = std::move(arena);

    assert(arena.offset() == old_offset);
    assert(arena.capacity() == old_capacity);
}

int main()
{
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "        AEGIS CORRECTNESS TESTS\n";
    std::cout << "========================================\n";

    test_basic_allocation();
    std::cout << "[PASS] Basic allocation\n";

    test_multiple_allocations();
    std::cout << "[PASS] Multiple allocations\n";

    test_alignment();
    std::cout << "[PASS] Alignment\n";

    test_zeroed_allocation();
    std::cout << "[PASS] Zeroed allocation\n";

    test_bounds();
    std::cout << "[PASS] Exact bounds\n";

    test_partial_bounds();
    std::cout << "[PASS] Partial bounds\n";

    test_free_all();
    std::cout << "[PASS] free_all()\n";

    test_resize_same_size();
    std::cout << "[PASS] Resize same size\n";

    test_resize_grow();
    std::cout << "[PASS] Resize grow\n";

    test_resize_shrink();
    std::cout << "[PASS] Resize shrink\n";

    test_resize_non_last_allocation();
    std::cout << "[PASS] Resize non-last allocation\n";

    test_resize_null_pointer();
    std::cout << "[PASS] Resize null pointer\n";

    test_resize_beyond_capacity();
    std::cout << "[PASS] Resize beyond capacity\n";

    test_move_constructor();
    std::cout << "[PASS] Move constructor\n";

    test_move_assignment();
    std::cout << "[PASS] Move assignment\n";

    test_self_move_assignment();
    std::cout << "[PASS] Self move assignment\n";

    std::cout << "========================================\n";
    std::cout << "        ALL TESTS PASSED\n";
    std::cout << "========================================\n\n";

    return 0;
}