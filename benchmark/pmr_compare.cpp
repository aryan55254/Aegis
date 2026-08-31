#include "../include/arena.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <memory_resource>

// Template function using inline assembly to make the value observable
// to the compiler so operations producing that value cannot be optimized away.
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
        std::cout << "Usage: ./bench_pmr <allocation_size>\n";
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

    Arena aegis(ARENA_SIZE);

    // Backing storage for the PMR arena.
    std::array<std::byte, ARENA_SIZE> buffer;

    std::pmr::monotonic_buffer_resource pmr_arena(
        buffer.data(),
        buffer.size(),
        std::pmr::null_memory_resource());

    // ----------------------------------------
    // Benchmark AEGIS
    // ----------------------------------------

    auto aegis_start =
        std::chrono::high_resolution_clock::now();

    for (size_t batch = 0; batch < BATCHES; ++batch)
    {
        for (size_t i = 0; i < ALLOCATIONS_PER_BATCH; ++i)
        {
            void *ptr =
                aegis.arena_alloc(allocation_size);

            DoNotOptimize(ptr);
        }

        aegis.free_all();
    }

    auto aegis_end =
        std::chrono::high_resolution_clock::now();

    double aegis_total_us =
        std::chrono::duration<double, std::micro>(
            aegis_end - aegis_start)
            .count();

    // ----------------------------------------
    // Benchmark std::pmr::monotonic_buffer_resource
    // ----------------------------------------

    auto pmr_start =
        std::chrono::high_resolution_clock::now();

    for (size_t batch = 0; batch < BATCHES; ++batch)
    {
        for (size_t i = 0; i < ALLOCATIONS_PER_BATCH; ++i)
        {
            void *ptr =
                pmr_arena.allocate(allocation_size);

            DoNotOptimize(ptr);
        }

        pmr_arena.release();
    }

    auto pmr_end =
        std::chrono::high_resolution_clock::now();

    double pmr_total_us =
        std::chrono::duration<double, std::micro>(
            pmr_end - pmr_start)
            .count();

    // ----------------------------------------
    // Calculate per-allocation cost
    // ----------------------------------------

    double aegis_per_allocation_ns =
        (aegis_total_us / TOTAL_ALLOCATIONS) * 1000.0;

    double pmr_per_allocation_ns =
        (pmr_total_us / TOTAL_ALLOCATIONS) * 1000.0;

    // How many times faster PMR is than AEGIS.
    double pmr_speedup =
        aegis_per_allocation_ns / pmr_per_allocation_ns;

    // AEGIS performance as a percentage of PMR.
    double aegis_relative_speed =
        (pmr_per_allocation_ns /
         aegis_per_allocation_ns) *
        100.0;

    // ----------------------------------------
    // Display results
    // ----------------------------------------

    std::cout << "\n";

    std::cout << "========================================\n";
    std::cout << "       AEGIS VS PMR ARENA BENCHMARK\n";
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

    std::cout << "PMR total       : "
              << pmr_total_us << " us\n";

    std::cout << "----------------------------------------\n";

    std::cout << "AEGIS           : "
              << aegis_per_allocation_ns
              << " ns/allocation\n";

    std::cout << "PMR             : "
              << pmr_per_allocation_ns
              << " ns/allocation\n";

    std::cout << "----------------------------------------\n";

    std::cout << "PMR speedup     : "
              << pmr_speedup
              << "x\n";

    std::cout << "AEGIS/PMR       : "
              << aegis_relative_speed
              << "%\n";

    std::cout << "========================================\n";

    return 0;
}