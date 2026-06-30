#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>

namespace Hydra {

class HydraApplication;

/// ===========================================================================
/// ShutdownSequence
///
/// Encapsulates the ordered teardown steps in reverse startup order.
///
/// Steps (in order):
///   1. Run OnPreShutdown hook
///   2. Shutdown all modules (reverse order) via ModuleManager
///   3. Unregister core services from EngineContext
///   4. Shutdown Logger (flush and close all sinks)
///   5. Run OnPostShutdown hook
/// ===========================================================================
class HYDRA_API ShutdownSequence final : private NonCopyableNonMovable
{
public:
    ShutdownSequence()  = default;
    ~ShutdownSequence() = default;

    /// Execute the full shutdown sequence.
    void Execute(HydraApplication& app);

private:
    void ShutdownModules(HydraApplication& app);
    void UnregisterCoreServices(HydraApplication& app);
    void ShutdownLogger();
};

} // namespace Hydra
