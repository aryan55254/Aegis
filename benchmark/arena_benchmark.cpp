#include "../include/arena.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>

// Template function using inline assembly to make the value observable to the compiler
// so operations producing that value cannot be optimized away.
template <typename T>
inline void DoNotOptimize(T const &value)
{
    asm volatile("" : : "g"(value) : "memory");
}

int main(int argc, char *argv[])
{
    constexpr size_t ARENA_SIZE = 1024 * 1024;
    constexpr size_t TOTAL_ALLOCATIONS = 10'000'000;

    if (argc != 2)
    {
        std::cout << "Usage: ./bench <allocation_size>\n";
        return 1;
    }

    size_t allocation_size = std::stoull(argv[1]);

    if (allocation_size == 0 || allocation_size > ARENA_SIZE)
    {
        std::cout << "Invalid allocation size.\n";
        return 1;
    }

    // Calculate how many allocations can fit in one arena.
    size_t allocations_per_batch =
        ARENA_SIZE / allocation_size;

    // Calculate how many batches are needed for the requested
    // total number of allocations.
    size_t batches =
        (TOTAL_ALLOCATIONS + allocations_per_batch - 1) / allocations_per_batch;

    Arena a1(ARENA_SIZE);

    // Benchmark allocations + free_all().
    auto alloc_start =
        std::chrono::high_resolution_clock::now();

    size_t allocations_done = 0;

    for (size_t batch = 0; batch < batches; ++batch)
    {
        size_t allocations_this_batch =
            std::min(
                allocations_per_batch,
                TOTAL_ALLOCATIONS - allocations_done);

        for (size_t i = 0; i < allocations_this_batch; ++i)
        {
            // Keep the allocation result observable so the compiler
            // cannot optimize the allocation away.
            void *ptr = a1.arena_alloc(allocation_size);

            DoNotOptimize(ptr);
        }

        allocations_done += allocations_this_batch;

        a1.free_all();
    }

    auto alloc_end =
        std::chrono::high_resolution_clock::now();

    double alloc_total_us =
        std::chrono::duration<double, std::micro>(
            alloc_end - alloc_start)
            .count();

    // Benchmark the same loop + free_all() without allocations.
    auto baseline_start =
        std::chrono::high_resolution_clock::now();

    allocations_done = 0;

    for (size_t batch = 0; batch < batches; ++batch)
    {
        size_t allocations_this_batch =
            std::min(
                allocations_per_batch,
                TOTAL_ALLOCATIONS - allocations_done);

        for (size_t i = 0; i < allocations_this_batch; ++i)
        {
            DoNotOptimize(i);
        }

        allocations_done += allocations_this_batch;

        a1.free_all();
    }

    auto baseline_end =
        std::chrono::high_resolution_clock::now();

    double baseline_total_us =
        std::chrono::duration<double, std::micro>(
            baseline_end - baseline_start)
            .count();

    // Remove loop + free_all() overhead.
    double allocation_total_us =
        alloc_total_us - baseline_total_us;

    double per_allocation_us =
        allocation_total_us / TOTAL_ALLOCATIONS;

    double per_allocation_ns =
        per_allocation_us * 1000.0;

    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "        AEGIS ALLOCATION BENCHMARK\n";
    std::cout << "========================================\n";
    std::cout << "Allocations     : "
              << TOTAL_ALLOCATIONS << "\n";
    std::cout << "Allocation size : "
              << allocation_size << " bytes\n";
    std::cout << "Arena capacity  : "
              << ARENA_SIZE / 1024 << " KiB\n";
    std::cout << "Alloc/batch     : "
              << allocations_per_batch << "\n";
    std::cout << "Batches         : "
              << batches << "\n";
    std::cout << "----------------------------------------\n";

    std::cout << std::fixed << std::setprecision(3);

    std::cout << "Total benchmark : "
              << alloc_total_us << " us\n";

    std::cout << "Baseline        : "
              << baseline_total_us << " us\n";

    std::cout << "Allocation time : "
              << allocation_total_us << " us\n";

    std::cout << "Per allocation  : "
              << per_allocation_ns << " ns\n";

    std::cout << "========================================\n";

    return 0;
}