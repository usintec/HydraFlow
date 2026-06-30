#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>

namespace Hydra {

class HydraApplication;
class EngineContext;

/// ===========================================================================
/// StartupSequence
///
/// Encapsulates the ordered steps to bring HydraCore from a cold state to
/// a fully initialized, ready-to-update state.
///
/// Steps (in order):
///   1. Initialize Logger
///   2. Load ApplicationSettings / ConfigManager
///   3. Register core services into EngineContext
///   4. Run OnPreInitialize hook
///   5. Initialize all modules via ModuleManager
///   6. Run OnPostInitialize hook
/// ===========================================================================
class HYDRA_API StartupSequence final : private NonCopyableNonMovable
{
public:
    StartupSequence()  = default;
    ~StartupSequence() = default;

    /// Execute the full startup sequence.
    /// Returns false if any step fails (error already logged).
    bool Execute(HydraApplication& app);

private:
    bool InitializeLogger(HydraApplication& app);
    bool LoadConfig(HydraApplication& app);
    bool RegisterCoreServices(HydraApplication& app);
    bool InitializeModules(HydraApplication& app);
};

} // namespace Hydra
