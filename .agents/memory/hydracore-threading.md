---
name: HydraCore Threading Module 6
description: ThreadPool/WorkerThread/Future/Promise/Mutex/ConditionVariable/Semaphore design and a std::counting_semaphore gotcha.
---

Module 6 design: `Hydra::Mutex`/`RecursiveMutex` wrap std mutexes with lowercase `lock()/unlock()/try_lock()` (required for Lockable concept interop with std::lock_guard/unique_lock). `ConditionVariable` wraps `std::condition_variable_any` (not `condition_variable`) so it can wait on the custom Mutex type, not just `std::mutex`. `Future<T>`/`Promise<T>` are a from-scratch shared-state implementation (mutex+condvar+optional+exception_ptr), not aliases of `std::future`/`std::promise`. `ThreadPool::Submit` uses C++20 pack init-capture (`... capturedArgs = std::forward<Args>(args)`) to bind arbitrary args into the queued task lambda.

**Gotcha:** `std::counting_semaphore<LeastMaxValue>` has a platform-defined ceiling (on this glibc/gcc, `INT_MAX`, i.e. `2147483647`). Defaulting the template parameter to `std::numeric_limits<isize>::max()` (which is `PTRDIFF_MAX`, 64-bit) fails a `static_assert` inside `<semaphore>` at instantiation time. Default to `std::numeric_limits<i32>::max()` instead for a "large but safe" ceiling.

**Why:** the assert only fires when the template is actually instantiated (e.g. in a test creating a `Semaphore` object), so it silently compiles fine everywhere except the first real usage — easy to miss until tests are written.

**How to apply:** when adding any `std::counting_semaphore`-based wrapper, cap the default `LeastMaxValue` at 32-bit `INT_MAX`, not `ptrdiff_t`/`size_t` max.
