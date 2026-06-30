#pragma once

#include <HydraCore/Common/Types.h>
#include <HydraCore/Logging/LoggerFactory.h>   // for LoggerConfig (Module 2)

namespace Hydra {

// =============================================================================
// UpdateLoopSettings
//
// Controls the timing behaviour of HydraApplication's main update loop.
// All time values are in seconds unless noted otherwise.
// =============================================================================

struct UpdateLoopSettings
{
    /// When false (default), deltaTime is the true elapsed time since the last
    /// frame.  When true, deltaTime is locked to 1.0/targetFrameRate and the
    /// loop sleeps to maintain that cadence — useful for deterministic sims.
    bool fixedTimeStep = false;

    /// Target frame rate used for the sleep budget and fixed-step dt value.
    f64  targetFrameRate = 60.0;

    /// Maximum deltaTime clamped per frame.  Prevents the "spiral of death"
    /// where a spike in one frame causes all subsequent frames to be enormous.
    f64  maxDeltaTime    = 0.25;
};

// =============================================================================
// ApplicationSettings
//
// Plain-data configuration struct consumed by HydraApplication.
// Populated either programmatically or via ApplicationSettings::LoadFromFile().
//
// The LoggerConfig member uses the Module 2 struct so all sink/rotation
// settings (file, console, rotation thresholds) are available at startup time.
// =============================================================================

struct ApplicationSettings
{
    String            appName        = "HydraFlow Application"; ///< Displayed in startup log
    String            appVersion     = "0.1.0";                 ///< Displayed in startup log
    String            configFilePath = "config/hydra.yaml";     ///< Optional YAML config path

    LoggerConfig      logging    = {};   ///< Controls logger sinks, level, rotation
    UpdateLoopSettings updateLoop = {};  ///< Controls the main loop timing

    // -------------------------------------------------------------------------
    // Factory helpers
    // -------------------------------------------------------------------------

    /// Returns default settings suitable for a development environment:
    /// console logging at Debug level, no file sink.
    [[nodiscard]] static ApplicationSettings MakeDefault();

    /// Loads settings from a YAML file, falling back to defaults on any error.
    /// If the file does not exist the defaults are used without any error logged
    /// (since the logger may not be initialised yet at call time).
    [[nodiscard]] static ApplicationSettings LoadFromFile(StringView path);
};

} // namespace Hydra
