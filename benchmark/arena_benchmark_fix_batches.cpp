#include "../include/arena.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>

template <typename T>
inline void DoNotOptimize(T const &value)
{
    asm volatile("" : : "g"(value) : "memory");
}

int main(int argc, char *argv[])
{
    constexpr size_t ARENA_SIZE = 1024 * 1024;
    constexpr size_t ALLOCATIONS_PER_BATCH = 256;
    constexpr size_t BATCHES = 39'063;
    constexpr size_t TOTAL_ALLOCATIONS =
        ALLOCATIONS_PER_BATCH * BATCHES;

    if (argc != 2)
    {
        std::cout << "Usage: ./bench_fixed <allocation_size>\n";
        return 1;
    }

    size_t allocation_size = std::stoull(argv[1]);

    if (allocation_size == 0 ||
        allocation_size > ARENA_SIZE / ALLOCATIONS_PER_BATCH)
    {
        std::cout << "Invalid allocation size.\n";
        std::cout << "Maximum supported size: "
                  << ARENA_SIZE / ALLOCATIONS_PER_BATCH
                  << " bytes\n";
        return 1;
    }

    Arena a1(ARENA_SIZE);

    // Benchmark allocations + free_all().
    auto alloc_start =
        std::chrono::high_resolution_clock::now();

    for (size_t batch = 0; batch < BATCHES; ++batch)
    {
        for (size_t i = 0; i < ALLOCATIONS_PER_BATCH; ++i)
        {
            void *ptr = a1.arena_alloc(allocation_size);
            DoNotOptimize(ptr);
        }

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

    for (size_t batch = 0; batch < BATCHES; ++batch)
    {
        for (size_t i = 0; i < ALLOCATIONS_PER_BATCH; ++i)
        {
            DoNotOptimize(i);
        }

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

    double per_allocation_ns =
        (allocation_total_us / TOTAL_ALLOCATIONS) * 1000.0;

    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "    AEGIS FIXED-BATCH ALLOCATION BENCH\n";
    std::cout << "========================================\n";
    std::cout << "Allocations     : "
              << TOTAL_ALLOCATIONS << "\n";
    std::cout << "Allocation size : "
              << allocation_size << " bytes\n";
    std::cout << "Arena capacity  : "
              << ARENA_SIZE / 1024 << " KiB\n";
    std::cout << "Alloc/batch     : "
              << ALLOCATIONS_PER_BATCH << "\n";
    std::cout << "Batches         : "
              << BATCHES << "\n";
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