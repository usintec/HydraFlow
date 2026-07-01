#include <HydraCore/Configuration/ConfigurationManager.h>

#include <algorithm>
#include <filesystem>

namespace Hydra {

// =============================================================================
// Construction
// =============================================================================

ConfigurationManager::ConfigurationManager(EnvironmentSettings env)
    : m_Env(std::move(env))
{}

// =============================================================================
// Loading
// =============================================================================

bool ConfigurationManager::LoadFile(const std::filesystem::path& path)
{
    return LoadFileInternal(path);
}

bool ConfigurationManager::LoadEnvironmentOverlay(const std::filesystem::path& baseDir,
                                                   StringView baseName)
{
    const std::string envName(m_Env.GetEnvironmentName());
    const std::string base(baseName);

    // Try each extension in order
    const char* extensions[] = { ".yaml", ".yml", ".json" };
    for (const char* ext : extensions) {
        auto candidate = baseDir / (base + '.' + envName + ext);
        if (std::filesystem::exists(candidate)) {
            return LoadFileInternal(candidate);
        }
    }

    // No overlay file found — silently succeed (optional overlay)
    return true;
}

void ConfigurationManager::SetDefaults(Configuration defaults)
{
    m_Defaults = std::move(defaults);
    RebuildMerged();
}

// =============================================================================
// Access
// =============================================================================

ConfigNode ConfigurationManager::Get(StringView dotPath) const
{
    return m_Merged.Get(dotPath);
}

bool ConfigurationManager::Has(StringView dotPath) const
{
    return m_Merged.Has(dotPath);
}

// =============================================================================
// Validation
// =============================================================================

void ConfigurationManager::SetSchema(ConfigSchema schema)
{
    m_Schema = std::move(schema);
}

ValidationResult ConfigurationManager::Validate() const
{
    if (!m_Schema) return ValidationResult{};
    return m_Schema->Validate(m_Merged);
}

bool ConfigurationManager::IsValid() const
{
    return Validate().IsValid();
}

// =============================================================================
// Hot reload
// =============================================================================

void ConfigurationManager::AddReloadListener(IHotReloadListener* listener)
{
    if (!listener) return;
    auto it = std::find(m_Listeners.begin(), m_Listeners.end(), listener);
    if (it == m_Listeners.end())
        m_Listeners.push_back(listener);
}

void ConfigurationManager::RemoveReloadListener(IHotReloadListener* listener)
{
    auto it = std::find(m_Listeners.begin(), m_Listeners.end(), listener);
    if (it != m_Listeners.end())
        m_Listeners.erase(it);
}

bool ConfigurationManager::Reload()
{
    if (m_LoadedFiles.empty()) return true;

    // Re-read all files in the same order into fresh layers
    Vector<Configuration> freshLayers;
    freshLayers.reserve(m_LoadedFiles.size());
    for (const auto& path : m_LoadedFiles) {
        Configuration cfg = LoadConfigurationFromFile(path);
        if (!cfg.IsValid()) {
            fprintf(stderr, "[ConfigurationManager] Reload failed: cannot parse '%s'\n",
                    path.c_str());
            return false;
        }
        freshLayers.push_back(std::move(cfg));
    }

    // Build a candidate merged config from fresh layers
    Configuration candidate = m_Defaults;
    for (const auto& layer : freshLayers)
        candidate.MergeOver(layer);

    // Run schema validation on the candidate
    if (m_Schema) {
        ValidationResult vr = m_Schema->Validate(candidate);
        if (!vr.IsValid()) {
            const std::string src = m_LoadedFiles.empty()
                                  ? "<unknown>"
                                  : m_LoadedFiles.front().string();
            for (auto* l : m_Listeners)
                l->OnConfigReloadFailed(src, vr);
            return false;
        }
    }

    // Notify listeners — all must accept
    bool accepted = NotifyListeners(candidate);
    if (!accepted) return false;

    // Commit
    m_Layers  = std::move(freshLayers);
    m_Merged  = candidate;
    m_Loaded  = true;
    return true;
}

// =============================================================================
// State
// =============================================================================

String ConfigurationManager::Dump(int indent) const
{
    return m_Merged.Dump(indent);
}

// =============================================================================
// Private helpers
// =============================================================================

void ConfigurationManager::RebuildMerged()
{
    // Start from defaults (lowest priority)
    m_Merged = m_Defaults;

    // Layer each loaded file on top
    for (const auto& layer : m_Layers)
        m_Merged.MergeOver(layer);

    // If still invalid (no defaults and no files) produce an empty object root
    if (!m_Merged.IsValid())
        m_Merged = Configuration{ ConfigValue::MakeObject() };
}

bool ConfigurationManager::LoadFileInternal(const std::filesystem::path& path)
{
    Configuration cfg = LoadConfigurationFromFile(path);
    if (!cfg.IsValid()) return false;

    m_LoadedFiles.push_back(path);
    m_Layers.push_back(std::move(cfg));
    RebuildMerged();
    m_Loaded = true;
    return true;
}

bool ConfigurationManager::NotifyListeners(const Configuration& newConfig)
{
    if (m_Listeners.empty()) return true;

    const std::string src = m_LoadedFiles.empty()
                          ? "<unknown>"
                          : m_LoadedFiles.front().string();

    for (auto* listener : m_Listeners) {
        if (!listener->OnConfigReloaded(src, newConfig)) {
            listener->OnConfigReloadRejected(src);
            return false;
        }
    }
    return true;
}

} // namespace Hydra
