# Aegis
Memory Allocator Written in C++


what next : 
- calculate allocation overhead
- alignment optimization 
- arena growth arena - lot of chunks 
- check point/ rollback 
- Rule of Five: Prevent double-free crashes (= delete).
- Initializer lists: Avoid double initialization overhead.
- Constructor exceptions: Prevent invalid object states.
-Move semantics: Transfer memory without copying.
-Const correctness: Enforce read-only state safety.
-[[nodiscard]] attribute: Prevent ignored return values.