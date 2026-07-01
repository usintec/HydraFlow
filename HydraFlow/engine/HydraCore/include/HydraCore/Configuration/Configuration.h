#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Configuration/ConfigValue.h>
#include <HydraCore/Configuration/ConfigNode.h>

#include <filesystem>

namespace Hydra {

// =============================================================================
// Configuration
//
// Represents one fully-loaded configuration document: a root ConfigValue
// (which may be an Object, Array, or scalar) together with the path from
// which it was loaded.
//
// Merge semantics:
//   MergeOver(overlay) performs a deep merge.  For each key in overlay:
//     – If both sides are Objects, recurse.
//     – Otherwise the overlay value overwrites the base value.
//   This is the "overlay wins" rule used to layer environment-specific files
//   on top of a base file.
//
// Access:
//   All Get() / Has() calls are non-throwing.  Missing paths return nullopt
//   or the provided default.
// =============================================================================

class HYDRA_API Configuration
{
public:
    // -------------------------------------------------------------------------
    // Constructors
    // -------------------------------------------------------------------------

    Configuration()  = default;           ///< Empty / invalid configuration
    ~Configuration() = default;

    Configuration(const Configuration&)            = default;
    Configuration& operator=(const Configuration&) = default;
    Configuration(Configuration&&)                 = default;
    Configuration& operator=(Configuration&&)      = default;

    /// Construct from a root ConfigValue and an optional source path.
    explicit Configuration(ConfigValue root, String sourcePath = {});

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    /// True when a non-empty root value has been supplied.
    [[nodiscard]] bool IsValid()    const noexcept { return m_Valid;       }
    [[nodiscard]] explicit operator bool() const noexcept { return m_Valid; }

    [[nodiscard]] StringView GetSourcePath() const noexcept { return m_SourcePath; }

    // -------------------------------------------------------------------------
    // Node access
    // -------------------------------------------------------------------------

    /// The root node of this configuration document.
    [[nodiscard]] ConfigNode GetRoot() const;

    /// Navigate to a dot-separated path from the root.
    /// Returns an invalid ConfigNode when the path does not exist.
    [[nodiscard]] ConfigNode Get(StringView dotPath) const;

    // -------------------------------------------------------------------------
    // Typed access shortcuts
    // -------------------------------------------------------------------------

    template<typename T>
    [[nodiscard]] Optional<T> Get(StringView dotPath) const
    {
        return GetRoot().Navigate(dotPath).As<T>();
    }

    template<typename T>
    [[nodiscard]] T GetOrDefault(StringView dotPath, T defaultValue) const
    {
        return Get<T>(dotPath).value_or(std::move(defaultValue));
    }

    [[nodiscard]] bool Has(StringView dotPath) const;

    // -------------------------------------------------------------------------
    // Merge
    // -------------------------------------------------------------------------

    /// Deep-merge overlay on top of this configuration (overlay wins).
    /// This configuration must be a valid Object root for merging to have
    /// effect; otherwise the overlay simply replaces this configuration.
    void MergeOver(const Configuration& overlay);

    // -------------------------------------------------------------------------
    // Serialisation / diagnostics
    // -------------------------------------------------------------------------

    [[nodiscard]] String Dump(int indent = 2) const;

    // -------------------------------------------------------------------------
    // Raw value access (used internally by ConfigurationManager / ConfigSchema)
    // -------------------------------------------------------------------------

    [[nodiscard]] const ConfigValue& GetRootValue() const noexcept { return m_Root; }

private:
    ConfigValue m_Root;
    String      m_SourcePath;
    bool        m_Valid = false;
};

// =============================================================================
// Free function — load a Configuration from a YAML or JSON file.
// Returns an invalid Configuration on any error.
// =============================================================================

[[nodiscard]] HYDRA_API
Configuration LoadConfigurationFromFile(const std::filesystem::path& path);

} // namespace Hydra
