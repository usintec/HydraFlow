#pragma once

#include <HydraCore/Common/Types.h>
#include <HydraCore/Logging/Logger.h>

namespace Hydra {

/// Settings controlling the application update loop.
struct UpdateLoopSettings
{
    bool   fixedTimeStep       = false;
    f64    targetFrameRate     = 60.0;
    f64    maxDeltaTime        = 0.25;  ///< Clamp large dt spikes (seconds)
};

/// Settings for the core application.
struct ApplicationSettings
{
    String             appName        = "HydraFlow Application";
    String             appVersion     = "0.1.0";
    String             configFilePath = "config/hydra.yaml";
    LoggerConfig       logging        = {};
    UpdateLoopSettings updateLoop     = {};

    // -------------------------------------------------------------------------
    // Factory methods
    // -------------------------------------------------------------------------

    /// Returns default settings suitable for development.
    [[nodiscard]] static ApplicationSettings MakeDefault();

    /// Loads settings from a YAML file; falls back to defaults on failure.
    [[nodiscard]] static ApplicationSettings LoadFromFile(StringView path);
};

} // namespace Hydra
