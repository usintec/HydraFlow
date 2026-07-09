---
name: Over-aligned custom allocators need aligned backing storage
description: Page/arena-style allocators that bump-allocate from a raw buffer must back that buffer with an aligned allocation, or over-aligned requests intermittently fail depending on heap layout.
---

Plain `new byte[size]` (or `malloc`) only guarantees `alignof(std::max_align_t)` (typically 16 bytes) for the returned address. A bump-pointer/arena allocator that computes `AlignUp(offset, alignment)` starting from offset 0 assumes the page's base address itself is aligned to at least the largest alignment any caller will request. If a caller asks for 32- or 64-byte alignment, the allocator can silently return misaligned pointers whenever the underlying heap happens to place the page at an address that isn't a multiple of that alignment — this is heap-layout-dependent, so it can pass in an isolated unit test run and fail once other allocations shift the heap (e.g. in a full test suite).

**Why:** Found via a real, reproducible (not flaky) failure: `ArenaAllocator` requested pages with `new byte[m_PageSize]`, and a test allocating with 32-byte alignment failed only when run as part of the full suite (heap-dependent), not in isolation.

**How to apply:** When implementing a paged/arena/pool allocator that hands out over-aligned memory from within a larger block, allocate that block with C++17 aligned `::operator new[](size, std::align_val_t(N))` (matched with `::operator delete[](ptr, std::align_val_t(N))`), where N is a fixed alignment guarantee (e.g. 64 bytes, cache-line size) that covers all realistic alignment requests. Document the guaranteed max alignment as a named constant.
