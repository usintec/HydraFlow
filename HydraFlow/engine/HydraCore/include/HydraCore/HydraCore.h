#pragma once

// =============================================================================
// HydraCore — Umbrella Header
//
// Including this single header brings in the entire public API of HydraCore.
// For large translation units sensitive to compile time, prefer including only
// the specific subsystem header(s) you need.
//
// Subsystem include order matters for documentation purposes only — all headers
// are individually self-contained and include their own transitive dependencies.
// =============================================================================

// ---------------------------------------------------------------------------
// Common — platform detection, primitive types, RAII helpers
// ---------------------------------------------------------------------------
#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>

// ---------------------------------------------------------------------------
// Module 2: Logging
// Full-featured logging subsystem: sinks, formatters, log rotation, macros.
// Placed before Config and Application so all modules can use HYDRA_* macros.
// ---------------------------------------------------------------------------
#include <HydraCore/Logging/LogLevel.h>
#include <HydraCore/Logging/LogMessage.h>
#include <HydraCore/Logging/Formatter.h>
#include <HydraCore/Logging/ILogSink.h>
#include <HydraCore/Logging/ConsoleSink.h>
#include <HydraCore/Logging/FileSink.h>
#include <HydraCore/Logging/NullSink.h>
#include <HydraCore/Logging/Logger.h>
#include <HydraCore/Logging/LoggerFactory.h>
#include <HydraCore/Logging/LoggingMacros.h>   // HYDRA_INFO / HYDRA_ERROR / etc.

// ---------------------------------------------------------------------------
// Module 1: Config
// YAML/JSON configuration loading; ApplicationSettings value type.
// ---------------------------------------------------------------------------
#include <HydraCore/Config/ApplicationSettings.h>
#include <HydraCore/Config/ConfigManager.h>

// ---------------------------------------------------------------------------
// Module 3: Configuration
// Structured, typed, validated, environment-aware configuration system.
// ---------------------------------------------------------------------------
#include <HydraCore/Configuration/ConfigValue.h>
#include <HydraCore/Configuration/ConfigNode.h>
#include <HydraCore/Configuration/Configuration.h>
#include <HydraCore/Configuration/ConfigSchema.h>
#include <HydraCore/Configuration/EnvironmentSettings.h>
#include <HydraCore/Configuration/IHotReloadListener.h>
#include <HydraCore/Configuration/ConfigurationManager.h>

// ---------------------------------------------------------------------------
// Module 1: Application
// Engine lifecycle, module registration, service locator.
// ---------------------------------------------------------------------------
#include <HydraCore/Application/IHydraModule.h>
#include <HydraCore/Application/EngineContext.h>
#include <HydraCore/Application/ModuleManager.h>
#include <HydraCore/Application/StartupSequence.h>
#include <HydraCore/Application/ShutdownSequence.h>
#include <HydraCore/Application/HydraApplication.h>
