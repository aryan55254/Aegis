# AEGIS Arena Allocator

A small, manually managed, bump-pointer arena allocator written in C++.

AEGIS is intentionally an arena allocator rather than a general-purpose allocator: memory is allocated linearly and reclaimed in bulk with `free_all()`.

---

## Contents

1. [What is AEGIS?](#what-is-aegis)
2. [Why an Arena Allocator?](#why-an-arena-allocator)
3. [How AEGIS Works](#how-aegis-works)
4. [Project Structure](#project-structure)
5. [Building on Linux](#building-on-linux)
6. [Using AEGIS](#using-aegis)
7. [API Reference](#api-reference)
8. [Ownership and Move Semantics](#ownership-and-move-semantics)
9. [Allocation and Alignment](#allocation-and-alignment)
10. [Zeroed vs Non-Zeroed Allocation](#zeroed-vs-non-zeroed-allocation)
11. [Resizing Allocations](#resizing-allocations)
12. [Example Usage](#example-usage)
13. [Benchmarking](#benchmarking)
14. [Benchmark Results](#benchmark-results)
15. [Correctness Testing](#correctness-testing)
16. [Hardening Testing](#hardening-testing)
17. [Sanitizer Testing](#sanitizer-testing)
18. [What Was Hardened](#what-was-hardened)
19. [Current Limitations](#current-limitations)
20. [Future Direction](#future-direction)

---

## What is AEGIS?

AEGIS is an arena-based memory allocator built in C++.

An arena provides a region of memory and allows the programmer to make linear allocations inside that region. Individual allocations are not freed. Instead, the entire arena can be reset at once.

The core idea is simple:

```text
allocate → allocate → allocate → ... → reset everything
```

This makes arenas useful for workloads where many temporary allocations share a lifetime.

AEGIS is intended to remain an arena allocator rather than becoming a general-purpose allocator.

---

## How AEGIS Works

The arena owns a backing buffer and tracks a `current_offset`.

For each allocation, AEGIS:

1. Finds the next suitably aligned address.
2. Calculates any alignment padding.
3. Checks that the allocation fits inside the arena.
4. Returns the aligned address.
5. Advances `current_offset`.

Conceptually:

```text
backing buffer
┌──────────────────────────────────────────┐
│ used │ padding │ requested allocation    │ free
└──────────────────────────────────────────┘
                 ↑     
          current_offset
```

The important property is that allocations move only forward.

`free_all()` simply resets the offset to zero. It does not walk through individual allocations.

---

## Project Structure

A typical AEGIS repository is organized like this:

```text
aegis/
├── include/
│   └── arena.hpp
├── src/
│   └── arena.cpp
├── benchmark/
│   ├── arena_benchmark.cpp
│   ├── zeroed_benchmark_fix_batches.cpp
│   ├── malloc_compare.cpp
│   ├── calloc_compare.cpp
│   └── pmr_compare.cpp
├── correctness_tests/
│   └── test.cpp
├── hardening_tests/
│   └── test.cpp
└── README.md
```

The important separation is:

* `include/` — public declarations
* `src/` — implementation
* `benchmark/` — performance experiments
* `correctness_tests/` — expected valid behavior
* `hardening_tests/` — hostile and edge-case inputs

---

## Building on Linux

AEGIS currently consists of a header and implementation file.

For a program using AEGIS:

```bash
g++ -std=c++17 main.cpp src/arena.cpp -Iinclude -lfmt -o app
```

The important part is that both `arena.cpp` and your program are compiled and linked.

For example, from `benchmark/`:

```bash
g++ -O2 arena_benchmark.cpp ../src/arena.cpp \
    -I../include -lfmt -o bench
```

Then:

```bash
./bench
```

`arena.hpp` contains the class declaration and function declarations.

`arena.cpp` contains the actual function definitions.

Including the header tells the compiler what `Arena` looks like. Compiling and linking `arena.cpp` supplies the implementations.

This is why compiling only:

```bash
g++ arena_benchmark.cpp -I../include -lfmt -o bench
```

can result in linker errors such as:

```text
undefined reference to `Arena::arena_alloc(...)`
```
---

## Using AEGIS

Include the public header:

```cpp
#include "arena.hpp"
```

Create an arena:

```cpp
Arena arena(1024 * 1024);
```

This creates a 1 MiB arena.

Then allocate memory:

```cpp
void *memory = arena.arena_alloc(64);
```

Always check for allocation failure when failure is possible:

```cpp
void *memory = arena.arena_alloc(64);

if (memory == nullptr)
{
    // Arena does not have enough space.
}
```

When the allocations are no longer needed:

```cpp
arena.free_all();
```

The arena can then be reused:

```cpp
arena.free_all();

void *again = arena.arena_alloc(128);
```

---

## API Reference

### `arena_alloc()`

```cpp
void *arena_alloc(
    size_t size,
    size_t align = alignof(std::max_align_t)
);
```

Allocates `size` bytes using the requested alignment.

The returned memory is **not zeroed**.

If the request cannot be satisfied, `nullptr` is returned.

Example:

```cpp
int *values =
    static_cast<int *>(arena.arena_alloc(
        100 * sizeof(int),
        alignof(int)
    ));
```

Use this when the allocated memory will immediately be initialized by the caller.

---

### `arena_alloc_zeroed()`

```cpp
void *arena_alloc_zeroed(
    size_t size,
    size_t align = alignof(std::max_align_t)
);
```

Works like `arena_alloc()` but initializes the returned memory to zero.

Example:

```cpp
int *values =
    static_cast<int *>(arena.arena_alloc_zeroed(
        100 * sizeof(int),
        alignof(int)
    ));
```

Every byte in the requested allocation is zeroed.

This costs additional time because the memory must be written.

---

### `arena_resize()`

```cpp
void *arena_resize(
    size_t old_size,
    void *ptr,
    size_t new_size
);
```

Resizes the most recent allocation.

Growing:

```cpp
void *buffer = arena.arena_alloc(128);

buffer = arena.arena_resize(
    128,
    buffer,
    256
);
```

Shrinking:

```cpp
buffer = arena.arena_resize(
    256,
    buffer,
    128
);
```

The existing contents are preserved when growing or shrinking within the supported semantics.

When growing, the newly added portion is zeroed.

A resize fails when:

* `ptr == nullptr`
* the allocation is not the most recent allocation
* the new size exceeds available capacity

A failed resize leaves the arena state unchanged.

---

### `free_all()`

```cpp
void free_all();
```

Resets the arena.

It is O(1):

```cpp
arena.free_all();
```

It does not individually free allocations.

It also does not zero the backing buffer.

The backing memory remains owned by the arena and can immediately be reused.

---

### `capacity()`

```cpp
size_t capacity() const;
```

Returns the total arena capacity.

Example:

```cpp
std::cout << arena.capacity();
```

---

### `offset()`

```cpp
size_t offset() const;
```

Returns the arena's current offset.

This is useful for inspecting how much of the arena is currently occupied.

Example:

```cpp
std::cout << arena.offset();
```

---

## Ownership and Move Semantics

`Arena` owns its backing memory.

Copy construction and copy assignment are disabled because copying an owning arena would create ambiguous ownership.

Move construction and move assignment are supported:

```cpp
Arena first(1024 * 1024);

Arena second(std::move(first));
```

The backing memory is transferred rather than copied.

The moved-from arena is left in a valid empty state.

This allows arenas to be stored or returned in move-aware contexts without copying the underlying buffer.

---

## Allocation and Alignment

AEGIS supports explicit alignment:

```cpp
void *ptr =
    arena.arena_alloc(
        128,
        64
    );
```

The allocator verifies that the requested alignment is valid.

Valid alignments are powers of two:

```text
1
2
4
8
16
32
64
...
```

Invalid alignments such as `3`, `6`, or `10` are rejected.

The allocator calculates alignment padding and skips those bytes when necessary.

The alignment check is important because the underlying alignment arithmetic assumes power-of-two alignment.

Invalid alignment requests return `nullptr` without changing the arena's allocation state.

---

## Zeroed vs Non-Zeroed Allocation

AEGIS deliberately provides two allocation paths.

### Non-zeroed

```cpp
void *ptr = arena.arena_alloc(4096);
```

The allocator does not write the returned memory.

The memory may contain old contents from a previous use of the backing buffer.

This is useful when the caller is immediately going to overwrite the allocation:

```cpp
struct Record
{
    int id;
    double value;
};

Record *record =
    static_cast<Record *>(
        arena.arena_alloc(sizeof(Record))
    );

// Caller initializes the object.
record->id = 42;
record->value = 3.14;
```

There is no reason for the allocator to first write zeros if the caller is going to overwrite every relevant byte.

### Zeroed

```cpp
void *ptr =
    arena.arena_alloc_zeroed(4096);
```

This uses `memset()` to zero the allocation.

The benchmark showed the expected behavior: zeroed allocation becomes increasingly expensive as allocation size grows because more memory must be written.

For example, in the fixed-batch benchmark, AEGIS zeroed allocation measured approximately:

```text
64 B      5.52 ns
128 B     5.10 ns
256 B     8.08 ns
512 B    12.20 ns
1 KiB    19.48 ns
4 KiB    70.21 ns
```

The non-zeroed allocator stayed around roughly 5 ns per allocation across the same workload.

---

## Resizing Allocations

AEGIS only allows `arena_resize()` to resize the most recent allocation.

Consider:

```text
A → B → C
```

If `C` is resized, the arena can safely extend or shrink the end of the buffer.

If `A` is resized, `B` and `C` are already occupying memory after it.

Moving or extending `A` would require relocating other allocations.

Therefore:

```cpp
arena_resize(..., C, ...); // supported
arena_resize(..., A, ...); // fails
```

This restriction keeps the allocator simple and preserves the bump-pointer model.

---

## Example Usage

### Simple scratch arena

```cpp
#include "arena.hpp"
#include <iostream>

int main()
{
    Arena arena(1024 * 1024);

    int *values =
        static_cast<int *>(
            arena.arena_alloc(
                100 * sizeof(int),
                alignof(int)
            )
        );

    if (!values)
        return 1;

    for (int i = 0; i < 100; ++i)
        values[i] = i;

    std::cout << values[50] << '\n';

    arena.free_all();
}
```

---

### Zero-initialized data

```cpp
Arena arena(4096);

int *values =
    static_cast<int *>(
        arena.arena_alloc_zeroed(
            100 * sizeof(int),
            alignof(int)
        )
    );

if (!values)
    return 1;

// values are initially zero.
```

---

### Multiple temporary allocations

```cpp
Arena arena(1024 * 1024);

char *name =
    static_cast<char *>(
        arena.arena_alloc(128)
    );

int *scores =
    static_cast<int *>(
        arena.arena_alloc(
            100 * sizeof(int),
            alignof(int)
        )
    );

double *weights =
    static_cast<double *>(
        arena.arena_alloc(
            100 * sizeof(double),
            alignof(double)
        )
    );

// Use all three allocations...

arena.free_all();
```

No individual `free()` calls are necessary.

---

### Reusing the same arena

```cpp
Arena arena(1024 * 1024);

for (int frame = 0; frame < 1000; ++frame)
{
    void *temporary =
        arena.arena_alloc(4096);

    if (!temporary)
        break;

    // Use temporary memory...

    arena.free_all();
}
```

This is one of the main patterns an arena allocator is designed for.

---

## Benchmarking

AEGIS was benchmarked using a fixed 1 MiB arena.

The primary workload used:

```text
Allocations per batch : 256
Batches               : 39,063
Total allocations     : 10,000,128
```

Each batch allocates 256 blocks and then resets the arena.

The allocation sizes tested were:

```text
64 B
128 B
256 B
512 B
1 KiB
4 KiB
```

The allocation result is passed through a `DoNotOptimize()` helper implemented with inline assembly so the compiler cannot simply eliminate the allocation result because it is never otherwise used.

The benchmark is compiled with optimization enabled:

```bash
g++ -O2 arena_benchmark.cpp ../src/arena.cpp \
    -I../include -lfmt -o bench
```

The benchmark measures the complete repeated workload and reports an average cost per allocation.

---

## Benchmark Methodology

Two types of arena benchmark were used.

### Variable batch size

The first benchmark kept the total allocation count fixed and changed the number of allocations per batch according to allocation size.

This prevents the 1 MiB arena from being exceeded while testing different allocation sizes.

### Fixed batch size

A second benchmark fixed:

```text
256 allocations per batch
39,063 batches
```

for every allocation size.

This produces approximately 10 million allocations for every test while keeping the batch structure identical.

The final count is:

```text
256 × 39,063 = 10,000,128 allocations
```

---

## AEGIS vs malloc

AEGIS was compared against the system `malloc()` using the same fixed-batch workload.

Representative results from the benchmark:

| Allocation |    AEGIS |     malloc | AEGIS speed advantage |
| ---------: | -------: | ---------: | --------------------: |
|       64 B | ~5.56 ns |   ~8.83 ns |                ~1.59× |
|      128 B | ~5.44 ns |  ~14.90 ns |                ~2.74× |
|      256 B | ~5.10 ns |  ~15.24 ns |                ~2.99× |
|      512 B | ~5.39 ns |  ~15.53 ns |                ~2.88× |
|      1 KiB | ~5.51 ns | ~123.09 ns |               ~22.32× |
|      4 KiB | ~5.96 ns | ~684.73 ns |              ~114.82× |

These numbers are workload- and machine-specific.

They should not be interpreted as a universal claim that AEGIS is faster than `malloc()` in every workload.

The benchmark demonstrates the expected advantage of a monotonic arena when many allocations share a lifetime.

---

## AEGIS vs calloc

Zeroed allocation was compared against `calloc()` using the same fixed-batch methodology.

Representative results:

| Allocation | AEGIS zeroed |     calloc | AEGIS speed advantage |
| ---------: | -----------: | ---------: | --------------------: |
|       64 B |     ~5.75 ns |  ~10.52 ns |                ~1.83× |
|      128 B |     ~6.36 ns |  ~15.98 ns |                ~2.52× |
|      256 B |     ~9.31 ns |  ~20.78 ns |                ~2.23× |
|      512 B |    ~12.58 ns |  ~36.04 ns |                ~2.86× |
|      1 KiB |    ~20.15 ns | ~143.70 ns |                ~7.13× |
|      4 KiB |    ~69.65 ns | ~727.67 ns |               ~10.45× |

The increasing AEGIS cost is expected: `arena_alloc_zeroed()` has to write the requested memory.

This benchmark is specifically useful because it separates the cost of simply advancing the arena from the cost of initializing the returned memory.

---

## AEGIS vs `std::pmr::monotonic_buffer_resource`

The final comparison used:

```cpp
std::pmr::monotonic_buffer_resource
```

This is a standard-library monotonic/bump-style memory resource and is conceptually very close to AEGIS.

The PMR resource was configured with:

* a 1 MiB caller-provided buffer
* no upstream allocation
* `std::pmr::null_memory_resource()`
* `release()` as the bulk reset operation

This makes the comparison a fixed-buffer monotonic-arena comparison rather than a comparison involving dynamic upstream chunk acquisition.

Representative results:

| Allocation |    AEGIS |      PMR | AEGIS as % of PMR |
| ---------: | -------: | -------: | ----------------: |
|       64 B | ~6.27 ns | ~0.92 ns |            ~14.6% |
|      128 B | ~6.05 ns | ~0.92 ns |            ~15.2% |
|      256 B | ~6.85 ns | ~0.92 ns |            ~13.5% |
|      512 B | ~5.96 ns | ~0.90 ns |            ~15.2% |
|      1 KiB | ~6.33 ns | ~0.90 ns |            ~14.2% |
|      4 KiB | ~6.24 ns | ~0.99 ns |            ~15.9% |

Under this particular benchmark, PMR is substantially faster.

This is useful information rather than a failure of the project: it demonstrates that a mature standard-library monotonic resource has a highly optimized hot path.

The benchmark should be treated as a controlled workload comparison, not a universal ranking of allocators.

---

## Correctness Testing

AEGIS has a dedicated correctness suite covering expected valid behavior.

The current suite contains **16 correctness tests**.

Tests include:

* basic allocation
* multiple allocations
* alignment
* zeroed allocation
* exact capacity bounds
* partial capacity bounds
* `free_all()`
* resize to the same size
* resize growth
* resize shrinking
* rejecting resize of a non-last allocation
* resizing a null pointer
* resizing beyond capacity
* move construction
* move assignment
* self move assignment

The test suite uses assertions, so a failing condition stops execution at the point where the invariant is violated.

Example build:

```bash
cd correctness_tests

g++ -std=c++17 test.cpp ../src/arena.cpp \
    -I../include -lfmt -o test

./test
```

Current result:

```text
16 / 16 correctness tests passed
```

---

## Hardening Testing

Correctness tests cover expected behavior.

Hardening tests deliberately exercise unusual, invalid, or hostile inputs.

The current suite contains **19 hardening tests**.

It covers:

* zero-sized arenas
* tiny arenas
* zero-sized allocations
* zero-sized zeroed allocations
* invalid alignments
* large alignments
* failed allocations preserving state
* repeated failed allocations
* repeated `free_all()`
* allocation after repeated resets
* resize after `free_all()`
* resize to zero
* failed resize preserving state
* resize shrink preserving contents
* repeated resize
* moving an empty arena
* move assignment with an empty source
* allocation after move construction
* allocation after move assignment

Build and run:

```bash
cd hardening_tests

g++ -std=c++17 test.cpp ../src/arena.cpp \
    -I../include -lfmt -o test

./test
```

Current result:

```text
19 / 19 hardening tests passed
```

---

## Sanitizer Testing

Both test suites were also run with AddressSanitizer and UndefinedBehaviorSanitizer.

Example:

```bash
g++ -std=c++17 -g \
    -fsanitize=address,undefined \
    test.cpp ../src/arena.cpp \
    -I../include -lfmt -o test_sanitize
```

Then:

```bash
./test_sanitize
```

Both the correctness suite and hardening suite completed successfully under ASan + UBSan with no sanitizer errors.

This is important for a manual allocator because the implementation relies on raw memory, pointer arithmetic, alignment, ownership, and explicit lifetime management.

---

## Current Limitations

AEGIS is intentionally small and specialized.

Current limitations include:

* the backing arena is fixed-size
* the current implementation has a temporary 1 MiB limit
* individual allocations cannot be freed
* `arena_resize()` only works on the most recent allocation
* the arena does not automatically grow
* the allocator is not thread-safe
* it is not intended to replace a general-purpose allocator

These are design constraints rather than bugs in the arena model.

---


## Project Status

AEGIS has reached a stable stopping point as a small arena allocator.

The implementation has:

* a complete basic arena allocation model
* explicit alignment support
* zeroed and non-zeroed allocation paths
* last-allocation resizing
* O(1) bulk reset
* RAII ownership
* move semantics
* invalid-input handling
* correctness tests
* hardening tests
* ASan + UBSan validation
* benchmarks against `malloc()`
* benchmarks against `calloc()`
* benchmarks against `std::pmr::monotonic_buffer_resource`

The project is intentionally being kept focused rather than being expanded into a general-purpose allocator.

The benchmark results, tests, and limitations provide a concrete basis for evaluating the implementation and for future optimization work if AEGIS is revisited.
