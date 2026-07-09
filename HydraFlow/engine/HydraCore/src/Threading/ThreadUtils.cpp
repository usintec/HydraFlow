#include <HydraCore/Threading/ThreadUtils.h>

#include <thread>
#include <chrono>
#include <functional> // std::hash<std::thread::id>

// pthread gives us thread naming on Linux/macOS; Windows would need a
// different API (SetThreadDescription), but HydraCore currently only
// targets Linux (see Platform.h), so we guard behind that macro.
#if defined(HYDRA_PLATFORM_LINUX)
    #include <pthread.h>
#endif

namespace Hydra::Thread {

u32 GetHardwareConcurrency() noexcept
{
    // hardware_concurrency() *may* return 0 if it can't determine the
    // answer (implementation-defined escape hatch in the standard) — we
    // never want callers (e.g. ThreadPool sizing) to divide by zero or
    // spawn a zero-thread pool, so we clamp to at least 1.
    const unsigned int count = std::thread::hardware_concurrency();
    return (count == 0) ? 1u : static_cast<u32>(count);
}

void SetCurrentThreadName(StringView name)
{
#if defined(HYDRA_PLATFORM_LINUX)
    // The Linux kernel limits thread names ("comm") to 16 bytes
    // including the null terminator, so we truncate defensively before
    // handing the string to pthread — passing a longer name would
    // otherwise fail with ERANGE and silently do nothing.
    constexpr usize kMaxLen = 15;
    String truncated(name.substr(0, std::min(name.size(), kMaxLen)));
    pthread_setname_np(pthread_self(), truncated.c_str());
#else
    (void)name; // No-op on platforms without a thread-naming API wired up yet.
#endif
}

String GetCurrentThreadName()
{
#if defined(HYDRA_PLATFORM_LINUX)
    char buffer[16] = {};
    if (pthread_getname_np(pthread_self(), buffer, sizeof(buffer)) == 0)
    {
        return String(buffer);
    }
    return String();
#else
    return String();
#endif
}

u64 GetCurrentThreadId() noexcept
{
    // std::thread::id deliberately has no arithmetic/integer conversion
    // (the standard only guarantees it's comparable and hashable), so we
    // go through std::hash to get a stable-for-this-process u64 we can
    // print in logs or store in a map keyed by "which thread did this".
    const std::thread::id id = std::this_thread::get_id();
    return static_cast<u64>(std::hash<std::thread::id>{}(id));
}

void SleepForMilliseconds(u64 milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void SleepForMicroseconds(u64 microseconds)
{
    std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}

void YieldThread() noexcept
{
    std::this_thread::yield();
}

} // namespace Hydra::Thread
