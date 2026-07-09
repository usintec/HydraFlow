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
// Module 4: Memory
// Custom allocators, allocation tracking, leak detection, profiling hooks.
// ---------------------------------------------------------------------------
#include <HydraCore/Memory/Alignment.h>
#include <HydraCore/Memory/IAllocator.h>
#include <HydraCore/Memory/LinearAllocator.h>
#include <HydraCore/Memory/StackAllocator.h>
#include <HydraCore/Memory/PoolAllocator.h>
#include <HydraCore/Memory/FreeListAllocator.h>
#include <HydraCore/Memory/ArenaAllocator.h>
#include <HydraCore/Memory/AllocationRecord.h>
#include <HydraCore/Memory/MemoryStatistics.h>
#include <HydraCore/Memory/IMemoryProfilingHook.h>
#include <HydraCore/Memory/ILeakDetectionHook.h>
#include <HydraCore/Memory/MemoryTracker.h>
#include <HydraCore/Memory/MemoryManager.h>
#include <HydraCore/Memory/MemoryMacros.h>

// ---------------------------------------------------------------------------
// Module 5: Events
// Typed pub/sub: synchronous dispatch, async queueing, priority ordering.
// ---------------------------------------------------------------------------
#include <HydraCore/Events/Event.h>
#include <HydraCore/Events/EventListener.h>
#include <HydraCore/Events/Subscription.h>
#include <HydraCore/Events/EventDispatcher.h>
#include <HydraCore/Events/EventQueue.h>
#include <HydraCore/Events/EventBus.h>

// ---------------------------------------------------------------------------
// Module 6: Threading
// ThreadPool/WorkerThread/Future/Promise, Mutex/ConditionVariable/Semaphore
// wrappers, thread utilities, and the ParallelFor CPU-parallelism helper.
// ---------------------------------------------------------------------------
#include <HydraCore/Threading/ThreadUtils.h>
#include <HydraCore/Threading/Mutex.h>
#include <HydraCore/Threading/ConditionVariable.h>
#include <HydraCore/Threading/Semaphore.h>
#include <HydraCore/Threading/Future.h>
#include <HydraCore/Threading/Promise.h>
#include <HydraCore/Threading/WorkerThread.h>
#include <HydraCore/Threading/ThreadPool.h>
#include <HydraCore/Threading/ParallelFor.h>

// ---------------------------------------------------------------------------
// Module 7: Job System
// Job/JobHandle, DependencyGraph, WorkerQueue, Task/TaskGraph, and
// JobScheduler — parallel execution with dependency scheduling, and a hook
// reserved for future GPU job dispatch.
// ---------------------------------------------------------------------------
#include <HydraCore/Jobs/JobTypes.h>
#include <HydraCore/Jobs/Job.h>
#include <HydraCore/Jobs/JobHandle.h>
#include <HydraCore/Jobs/DependencyGraph.h>
#include <HydraCore/Jobs/WorkerQueue.h>
#include <HydraCore/Jobs/Task.h>
#include <HydraCore/Jobs/TaskGraph.h>
#include <HydraCore/Jobs/JobScheduler.h>

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
