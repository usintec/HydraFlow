#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Configuration/Configuration.h>
#include <HydraCore/Configuration/ConfigSchema.h>
#include <HydraCore/Configuration/EnvironmentSettings.h>
#include <HydraCore/Configuration/IHotReloadListener.h>

#include <filesystem>

namespace Hydra {

// =============================================================================
// ConfigurationManager
//
// Central façade for the Module 3 Configuration system.
//
// Priority layers (lowest → highest):
//   1. Programmatic defaults (SetDefaults)
//   2. Base file (LoadFile)
//   3. Environment overlay (LoadEnvironmentOverlay)
//
// Each LoadFile / LoadEnvironmentOverlay call appends a new layer; layers are
// deep-merged left-to-right so that later calls win on conflict.  The merged
// result is cached in m_Merged and rebuilt whenever the layer stack changes or
// Reload() is called.
//
// Hot reload:
//   Reload() re-reads all previously loaded files in the same order, rebuilds
//   the merged config, runs schema validation (if set), and notifies listeners.
//   File-system watching is intentionally out of scope for this module; callers
//   drive reload timing explicitly.
// =============================================================================

class HYDRA_API ConfigurationManager final : private NonCopyable
{
public:
    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    ConfigurationManager() = default;
    explicit ConfigurationManager(EnvironmentSettings env);
    ~ConfigurationManager() = default;

    // -------------------------------------------------------------------------
    // Loading
    // -------------------------------------------------------------------------

    /// Load the primary configuration file (YAML or JSON).
    /// May be called multiple times; each call adds a new layer.
    /// Returns false if the file cannot be read or parsed.
    bool LoadFile(const std::filesystem::path& path);

    /// Attempt to load an environment-specific overlay and merge it over the
    /// existing layers.
    ///
    /// The overlay filename is derived as:
    ///   <baseDir>/<baseName>.<envName>.<ext>
    ///
    /// where <ext> is tried in order: .yaml, .yml, .json.
    ///
    /// If no matching file exists this is silently ignored and true is returned.
    /// Returns false only when a matching file is found but cannot be parsed.
    bool LoadEnvironmentOverlay(const std::filesystem::path& baseDir,
                                StringView                   baseName = "hydra");

    /// Set programmatic defaults (lowest-priority layer).
    /// Replaces any previously supplied defaults.
    void SetDefaults(Configuration defaults);

    // -------------------------------------------------------------------------
    // Active configuration access
    // -------------------------------------------------------------------------

    [[nodiscard]] const Configuration& GetConfig() const noexcept { return m_Merged; }

    [[nodiscard]] ConfigNode Get(StringView dotPath) const;

    template<typename T>
    [[nodiscard]] Optional<T> Get(StringView dotPath) const
    {
        return m_Merged.Get<T>(dotPath);
    }

    template<typename T>
    [[nodiscard]] T GetOrDefault(StringView dotPath, T defaultValue) const
    {
        return m_Merged.GetOrDefault(dotPath, std::move(defaultValue));
    }

    [[nodiscard]] bool Has(StringView dotPath) const;

    // -------------------------------------------------------------------------
    // Validation
    // -------------------------------------------------------------------------

    /// Replace the validation schema.  If set, Reload() runs it automatically.
    void SetSchema(ConfigSchema schema);

    /// Validate the current merged configuration against the schema (if any).
    /// Returns a valid result when no schema is set.
    [[nodiscard]] ValidationResult Validate() const;

    /// Convenience: Validate().IsValid()
    [[nodiscard]] bool IsValid() const;

    // -------------------------------------------------------------------------
    // Environment
    // -------------------------------------------------------------------------

    [[nodiscard]] const EnvironmentSettings& GetEnvironment() const noexcept { return m_Env; }

    // -------------------------------------------------------------------------
    // Hot reload
    // -------------------------------------------------------------------------

    /// Register a listener.  Does not transfer ownership.
    /// Adding the same pointer twice is idempotent.
    void AddReloadListener(IHotReloadListener* listener);

    /// Unregister a listener.  Safe to call with a pointer that was never added.
    void RemoveReloadListener(IHotReloadListener* listener);

    /// Re-read all loaded files, rebuild layers, and notify listeners.
    /// If a schema is set, runs validation; if it fails, calls
    /// OnConfigReloadFailed on all listeners and keeps the old config.
    /// Returns true when the reload was accepted by all listeners.
    bool Reload();

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    [[nodiscard]] bool   IsLoaded() const noexcept { return m_Loaded; }
    [[nodiscard]] String Dump(int indent = 2) const;

private:
    // Layer stack
    Configuration                 m_Defaults;       ///< Programmatic defaults (lowest)
    Vector<Configuration>         m_Layers;          ///< Files in load order
    Configuration                 m_Merged;          ///< Rebuilt merged result

    // Metadata
    EnvironmentSettings           m_Env;
    Vector<std::filesystem::path> m_LoadedFiles;     ///< Tracks files for Reload()
    Optional<ConfigSchema>        m_Schema;

    // Hot reload
    Vector<IHotReloadListener*>   m_Listeners;

    bool m_Loaded = false;

    // ---- Internal helpers --------------------------------------------------
    void RebuildMerged();
    bool LoadFileInternal(const std::filesystem::path& path);
    bool NotifyListeners(const Configuration& newConfig);
};

} // namespace Hydra
