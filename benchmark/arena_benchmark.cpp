#include <arena.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>

int main()
{
    constexpr size_t ALLOCATIONS_PER_BATCH = 10000;
    constexpr size_t BATCHES = 1000;
    constexpr size_t TOTAL_ALLOCATIONS =
        ALLOCATIONS_PER_BATCH * BATCHES;

    Arena a1(1024 * 1024);

    // Benchmark allocations + free_all().
    auto alloc_start = std::chrono::high_resolution_clock::now();

    for (size_t batch = 0; batch < BATCHES; ++batch)
    {
        for (size_t i = 0; i < ALLOCATIONS_PER_BATCH; ++i)
        {
            a1.arena_alloc(64);
        }

        a1.free_all();
    }

    auto alloc_end = std::chrono::high_resolution_clock::now();

    double alloc_total_us =
        std::chrono::duration<double, std::micro>(
            alloc_end - alloc_start)
            .count();

    // Benchmark the same loop + free_all() without allocations.
    auto baseline_start = std::chrono::high_resolution_clock::now();

    for (size_t batch = 0; batch < BATCHES; ++batch)
    {
        for (size_t i = 0; i < ALLOCATIONS_PER_BATCH; ++i)
        {
            // intentionally empty
        }

        a1.free_all();
    }

    auto baseline_end = std::chrono::high_resolution_clock::now();

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
    std::cout << "Allocations     : " << TOTAL_ALLOCATIONS << "\n";
    std::cout << "Allocation size : 64 bytes\n";
    std::cout << "Arena capacity  : 1 MiB\n";
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