#include <HydraCore/Logging/ConsoleSink.h>
#include <HydraCore/Logging/Formatter.h>
#include <HydraCore/Logging/LogLevel.h>

#include <cstdio>
#include <iostream>

// Platform-specific TTY detection
#if defined(HYDRA_PLATFORM_WINDOWS)
    #include <io.h>
    #include <windows.h>
    #define HYDRA_ISATTY(fd) _isatty(fd)
    #define HYDRA_FILENO_STDOUT 1
    #define HYDRA_FILENO_STDERR 2
#else
    #include <unistd.h>
    #define HYDRA_ISATTY(fd) isatty(fd)
    #define HYDRA_FILENO_STDOUT STDOUT_FILENO
    #define HYDRA_FILENO_STDERR STDERR_FILENO
#endif

namespace Hydra {

// =============================================================================
// ConsoleSink — construction
// =============================================================================

ConsoleSink::ConsoleSink()
{
    // Detect whether stdout is an interactive terminal.
    // If it is, we enable ANSI colour codes for a better developer experience.
    m_ColorEnabled = DetectTty(HYDRA_FILENO_STDOUT);

#if defined(HYDRA_PLATFORM_WINDOWS)
    // On Windows 10+ we must explicitly enable virtual-terminal processing
    // so that ANSI escape sequences are rendered rather than printed literally.
    if (m_ColorEnabled)
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD  mode = 0;
        if (GetConsoleMode(hOut, &mode))
        {
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
        HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
        if (GetConsoleMode(hErr, &mode))
        {
            SetConsoleMode(hErr, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif

    // Build a PatternFormatter with colour support matching the detected state.
    // ConsoleSink uses the colourised {LEVEL} token for eye-catching output.
    auto fmt = std::make_shared<PatternFormatter>(
        "[{datetime}] [{name}] [{LEVEL}] [t:{thread}] {message}");
    fmt->SetColorEnabled(m_ColorEnabled);
    SetFormatter(std::move(fmt));
}

// =============================================================================
// Color control
// =============================================================================

void ConsoleSink::SetColorEnabled(bool enabled) noexcept
{
    m_ColorEnabled = enabled;
    // Propagate the change to the formatter so {LEVEL} tokens update too.
    if (auto* pf = dynamic_cast<PatternFormatter*>(GetFormatter().get()))
    {
        pf->SetColorEnabled(enabled);
    }
}

bool ConsoleSink::IsColorEnabled() const noexcept
{
    return m_ColorEnabled;
}

// =============================================================================
// ILogSink::Sink
// =============================================================================

void ConsoleSink::Sink(const LogMessage& msg)
{
    // Format the message outside the lock — formatting is pure / thread-safe.
    String formatted = FormatMessage(msg);
    formatted += '\n';

    // Errors and fatals go to stderr; everything else to stdout.
    // The lock prevents interleaved output from concurrent threads.
    std::lock_guard lock(m_Mutex);

    if (msg.level >= LogLevel::Error)
    {
        std::fwrite(formatted.data(), 1, formatted.size(), stderr);
    }
    else
    {
        std::fwrite(formatted.data(), 1, formatted.size(), stdout);
    }
}

// =============================================================================
// ILogSink::Flush
// =============================================================================

void ConsoleSink::Flush()
{
    std::lock_guard lock(m_Mutex);
    std::fflush(stdout);
    std::fflush(stderr);
}

// =============================================================================
// TTY detection helper
// =============================================================================

bool ConsoleSink::DetectTty(int fd) noexcept
{
    // HYDRA_ISATTY returns non-zero when the fd is connected to a terminal.
    return HYDRA_ISATTY(fd) != 0;
}

} // namespace Hydra
