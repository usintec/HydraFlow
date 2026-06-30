#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>

namespace Hydra {

/// ===========================================================================
/// ConfigManager
///
/// Manages loading and accessing configuration from YAML/JSON files.
/// Values are stored internally as nlohmann::json for uniform access.
/// ===========================================================================
class HYDRA_API ConfigManager final : private NonCopyable
{
public:
    ConfigManager() = default;
    ~ConfigManager() = default;

    /// Load configuration from a YAML or JSON file.
    /// Returns true on success.
    bool LoadFile(const std::filesystem::path& path);

    /// Returns true if any config has been loaded successfully.
    [[nodiscard]] bool IsLoaded() const noexcept;

    /// Get a value by dot-separated key path (e.g. "app.window.width").
    /// Returns std::nullopt if the key does not exist.
    template<typename T>
    [[nodiscard]] Optional<T> Get(StringView keyPath) const;

    /// Get a value or return the provided default.
    template<typename T>
    [[nodiscard]] T GetOrDefault(StringView keyPath, T defaultValue) const;

    /// Set a value at the given dot-separated key path.
    template<typename T>
    void Set(StringView keyPath, T value);

    /// Dump the entire config as a JSON string (for debugging).
    [[nodiscard]] String Dump(int indent = 2) const;

private:
    nlohmann::json         m_Root;
    bool                   m_Loaded = false;

    [[nodiscard]] const nlohmann::json* NavigateTo(StringView keyPath) const;
    [[nodiscard]] nlohmann::json*       NavigateTo(StringView keyPath);
};

// ===========================================================================
// Template Implementations
// ===========================================================================

template<typename T>
Optional<T> ConfigManager::Get(StringView keyPath) const
{
    const auto* node = NavigateTo(keyPath);
    if (!node || node->is_null())
        return std::nullopt;
    try {
        return node->get<T>();
    } catch (...) {
        return std::nullopt;
    }
}

template<typename T>
T ConfigManager::GetOrDefault(StringView keyPath, T defaultValue) const
{
    return Get<T>(keyPath).value_or(std::move(defaultValue));
}

template<typename T>
void ConfigManager::Set(StringView keyPath, T value)
{
    auto* node = NavigateTo(keyPath);
    if (node)
        *node = std::move(value);
}

} // namespace Hydra
