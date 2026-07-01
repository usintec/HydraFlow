#include <HydraCore/Configuration/EnvironmentSettings.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace Hydra {

// =============================================================================
// Constructors
// =============================================================================

EnvironmentSettings::EnvironmentSettings(Environment env)
    : m_Env(env)
{}

EnvironmentSettings::EnvironmentSettings(Environment env, String customName)
    : m_Env(env)
    , m_CustomName(std::move(customName))
{}

// =============================================================================
// Factory
// =============================================================================

EnvironmentSettings EnvironmentSettings::FromEnvironmentVariable()
{
    const char* raw = std::getenv("HYDRA_ENV");
    if (!raw || *raw == '\0')
        return EnvironmentSettings{};   // default: Development

    const std::string value(raw);
    Environment env = ParseEnvString(value);

    if (env == Environment::Custom)
        return EnvironmentSettings{ Environment::Custom, value };

    return EnvironmentSettings{ env };
}

// =============================================================================
// Mutation
// =============================================================================

void EnvironmentSettings::Set(Environment env)
{
    m_Env        = env;
    m_CustomName.clear();
}

void EnvironmentSettings::SetCustom(StringView name)
{
    m_Env        = Environment::Custom;
    m_CustomName = String(name);
}

// =============================================================================
// Query
// =============================================================================

StringView EnvironmentSettings::GetEnvironmentName() const noexcept
{
    switch (m_Env) {
        case Environment::Development: return "development";
        case Environment::Testing:     return "testing";
        case Environment::Staging:     return "staging";
        case Environment::Production:  return "production";
        case Environment::Custom:      return m_CustomName;
    }
    return "development";
}

bool EnvironmentSettings::operator==(const EnvironmentSettings& other) const noexcept
{
    if (m_Env != other.m_Env) return false;
    if (m_Env == Environment::Custom) return m_CustomName == other.m_CustomName;
    return true;
}

bool EnvironmentSettings::operator!=(const EnvironmentSettings& other) const noexcept
{
    return !(*this == other);
}

// =============================================================================
// Private helpers
// =============================================================================

Environment EnvironmentSettings::ParseEnvString(StringView raw)
{
    // Lowercase copy for case-insensitive comparison
    std::string lower(raw);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "development" || lower == "dev")    return Environment::Development;
    if (lower == "testing"     || lower == "test")   return Environment::Testing;
    if (lower == "staging")                           return Environment::Staging;
    if (lower == "production"  || lower == "prod")   return Environment::Production;

    return Environment::Custom;
}

} // namespace Hydra
