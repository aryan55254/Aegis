#include "../include/arena.hpp"

#include <chrono>
#include <cstdlib>
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
        std::cout << "Usage: ./bench_compare <allocation_size>\n";
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

    // ----------------------------------------
    // Benchmark AEGIS arena_alloc()
    // ----------------------------------------

    auto aegis_start =
        std::chrono::high_resolution_clock::now();

    for (size_t batch = 0; batch < BATCHES; ++batch)
    {
        for (size_t i = 0; i < ALLOCATIONS_PER_BATCH; ++i)
        {
            void *ptr = a1.arena_alloc(allocation_size);

            // Keep the allocation result observable.
            DoNotOptimize(ptr);
        }

        a1.free_all();
    }

    auto aegis_end =
        std::chrono::high_resolution_clock::now();

    double aegis_total_us =
        std::chrono::duration<double, std::micro>(
            aegis_end - aegis_start)
            .count();

    // ----------------------------------------
    // Benchmark malloc()
    // ----------------------------------------

    auto malloc_start =
        std::chrono::high_resolution_clock::now();

    for (size_t batch = 0; batch < BATCHES; ++batch)
    {
        void *allocations[ALLOCATIONS_PER_BATCH];

        for (size_t i = 0; i < ALLOCATIONS_PER_BATCH; ++i)
        {
            allocations[i] = std::malloc(allocation_size);

            // Keep the allocation result observable.
            DoNotOptimize(allocations[i]);
        }

        for (size_t i = 0; i < ALLOCATIONS_PER_BATCH; ++i)
        {
            std::free(allocations[i]);
        }
    }

    auto malloc_end =
        std::chrono::high_resolution_clock::now();

    double malloc_total_us =
        std::chrono::duration<double, std::micro>(
            malloc_end - malloc_start)
            .count();

    // ----------------------------------------
    // Calculate per-allocation cost
    // ----------------------------------------

    double aegis_per_allocation_ns =
        (aegis_total_us / TOTAL_ALLOCATIONS) * 1000.0;

    double malloc_per_allocation_ns =
        (malloc_total_us / TOTAL_ALLOCATIONS) * 1000.0;

    double speedup =
        malloc_per_allocation_ns / aegis_per_allocation_ns;

    // ----------------------------------------
    // Display results
    // ----------------------------------------

    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "       AEGIS VS MALLOC BENCHMARK\n";
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

    std::cout << "AEGIS total     : "
              << aegis_total_us << " us\n";

    std::cout << "malloc total    : "
              << malloc_total_us << " us\n";

    std::cout << "----------------------------------------\n";

    std::cout << "AEGIS           : "
              << aegis_per_allocation_ns
              << " ns/allocation\n";

    std::cout << "malloc          : "
              << malloc_per_allocation_ns
              << " ns/allocation\n";

    std::cout << "Speedup         : "
              << speedup << "x\n";

    std::cout << "========================================\n";

    return 0;
}