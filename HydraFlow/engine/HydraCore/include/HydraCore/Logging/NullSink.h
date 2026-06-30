#pragma once

#include <HydraCore/Logging/ILogSink.h>

namespace Hydra {

// =============================================================================
// NullSink
//
// A no-operation sink that silently discards every message it receives.
//
// Intended uses:
//   - Unit tests that want a logger with no output side-effects.
//   - Temporarily silencing a logger without removing it from the factory.
//   - Benchmarking logger overhead in isolation from I/O.
//
// This sink is always thread-safe and requires no synchronisation because it
// performs no state mutations.
// =============================================================================

class NullSink final : public ILogSink
{
public:
    NullSink()  = default;
    ~NullSink() override = default;

    /// Discards the message without any output.
    void Sink(const LogMessage& /*msg*/) override {}

    /// No-op: nothing to flush.
    void Flush() override {}
};

} // namespace Hydra
