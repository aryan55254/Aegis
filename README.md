# AEGIS Arena Allocator

## What is AEGIS?

AEGIS is an arena-based memory allocator built in C++.

An arena allocator provides a region of memory and allows the programmer to make linear allocations inside that region. Individual allocations are not freed; instead, the entire arena can be reset at once.

AEGIS is intended to remain an arena allocator rather than becoming a general-purpose allocator.

## How AEGIS Works

The arena owns a backing buffer and tracks a `current_offset`.

When the programmer requests memory, AEGIS finds the next suitably aligned address, checks whether enough capacity remains, returns a pointer to that address, and moves the offset forward.

This gives AEGIS a simple linear bump-pointer allocation model.

Allocations are automatically aligned according to the alignment requested by the programmer. Alignment padding is skipped when necessary.

## Current API

### `arena_alloc()`

Allocates memory from the arena using the requested size and alignment.

The allocation does not zero the memory, so previously stored contents may still exist in the returned region. This avoids unnecessary writes when the programmer is going to overwrite the memory anyway.

If the arena does not have enough remaining capacity, `nullptr` is returned.

### `arena_alloc_zeroed()`

Works like `arena_alloc()` but zeroes the allocated memory using `memset()`.

This is slower than normal allocation because the requested memory has to be written to, but is useful when zero-initialized memory is actually required.

### `arena_resize()`

Resizes an existing allocation.

Currently it can only resize the most recent allocation because only the last allocation can safely extend into or return space from the end of the arena.

It supports both growing and shrinking.

Growing moves the offset forward and zeroes the newly added memory.

Shrinking moves the offset backward and returns the unused space to the arena.

Passing `nullptr` or attempting to resize an allocation that is not the most recent allocation returns `nullptr`.

### `free_all()`

Resets the arena by setting `current_offset` back to zero.

It does not individually free allocations or zero the memory. The backing buffer remains owned by the arena and can be reused.

This operation is O(1).

### `capacity()`

Returns the total capacity of the arena.

### `offset()`

Returns the arena's current offset.

## C++ Ownership

`Arena` owns its backing memory and uses RAII to release it when the object is destroyed.

Copy construction and copy assignment are disabled because copying the arena would create an ownership problem.

Move construction and move assignment are supported so ownership of the backing memory can be transferred without copying the actual allocation.

Moved-from arenas are left in a valid empty state.

## Current Limitations

- The backing buffer is currently fixed-size.
- There is currently a temporary 1 MB limit on the arena size.
- Individual allocations cannot be freed.
- `arena_resize()` only works on the most recent allocation.
- The arena does not currently grow automatically.

The 1 MB limitation is temporary. The planned direction is a chunk-based arena that can acquire additional chunks as needed.

## Current State

The core arena allocator and its C++ ownership model are implemented.

The next stage is to benchmark the current implementation, measure allocation overhead, and investigate where performance improvements are actually possible rather than optimizing blindly.

## Future Direction

AEGIS will remain focused on being an arena allocator.

Planned work includes:

- Benchmarking allocation overhead
- Investigating the allocation hot path
- Chunk-based arena growth
- Further performance optimization