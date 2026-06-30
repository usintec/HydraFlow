#pragma once

/// ===========================================================================
/// HydraCore — Convenience Umbrella Header
///
/// Include this single header to bring in the entire public API of HydraCore.
/// For compilation-speed-sensitive translation units, prefer including only
/// the specific subsystem headers you need.
/// ===========================================================================

// Common
#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>

// Logging
#include <HydraCore/Logging/Logger.h>

// Config
#include <HydraCore/Config/ApplicationSettings.h>
#include <HydraCore/Config/ConfigManager.h>

// Application
#include <HydraCore/Application/IHydraModule.h>
#include <HydraCore/Application/EngineContext.h>
#include <HydraCore/Application/ModuleManager.h>
#include <HydraCore/Application/StartupSequence.h>
#include <HydraCore/Application/ShutdownSequence.h>
#include <HydraCore/Application/HydraApplication.h>
