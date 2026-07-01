#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>

namespace Hydra {

// =============================================================================
// Environment
//
// Logical deployment tier.  Used by ConfigurationManager to locate and merge
// environment-specific configuration overlays (e.g. hydra.production.yaml).
// =============================================================================

enum class Environment
{
    Development,   ///< Local developer machine (default)
    Testing,       ///< CI / automated test runs
    Staging,       ///< Pre-production integration environment
    Production,    ///< Live production deployment
    Custom,        ///< Caller-supplied name via SetCustom()
};

// =============================================================================
// EnvironmentSettings
//
// Thin value-type that wraps an Environment enum and an optional custom name.
//
// Detection from process environment:
//   EnvironmentSettings::FromEnvironmentVariable() reads HYDRA_ENV and maps:
//     "development" | "dev"         → Development
//     "testing"     | "test"        → Testing
//     "staging"                     → Staging
//     "production"  | "prod"        → Production
//     <anything else>               → Custom (name == the raw env-var value)
//   Comparison is case-insensitive.  If HYDRA_ENV is not set the default
//   (Development) is returned.
// =============================================================================

class HYDRA_API EnvironmentSettings
{
public:
    // -------------------------------------------------------------------------
    // Constructors
    // -------------------------------------------------------------------------

    EnvironmentSettings() = default;   ///< Default: Development
    explicit EnvironmentSettings(Environment env);
    EnvironmentSettings(Environment env, String customName);

    EnvironmentSettings(const EnvironmentSettings&)            = default;
    EnvironmentSettings& operator=(const EnvironmentSettings&) = default;
    EnvironmentSettings(EnvironmentSettings&&)                 = default;
    EnvironmentSettings& operator=(EnvironmentSettings&&)      = default;

    // -------------------------------------------------------------------------
    // Factory
    // -------------------------------------------------------------------------

    /// Detect the environment from the HYDRA_ENV process variable.
    /// Returns Development when the variable is unset or empty.
    [[nodiscard]] static EnvironmentSettings FromEnvironmentVariable();

    // -------------------------------------------------------------------------
    // Mutation
    // -------------------------------------------------------------------------

    void Set(Environment env);
    void SetCustom(StringView name);

    // -------------------------------------------------------------------------
    // Query
    // -------------------------------------------------------------------------

    [[nodiscard]] Environment GetEnvironment()    const noexcept { return m_Env; }

    /// Human-readable lowercase name: "development", "testing", "staging",
    /// "production", or the custom string supplied via SetCustom().
    [[nodiscard]] StringView  GetEnvironmentName() const noexcept;

    [[nodiscard]] bool IsDevelopment() const noexcept { return m_Env == Environment::Development; }
    [[nodiscard]] bool IsTesting()     const noexcept { return m_Env == Environment::Testing;     }
    [[nodiscard]] bool IsStaging()     const noexcept { return m_Env == Environment::Staging;     }
    [[nodiscard]] bool IsProduction()  const noexcept { return m_Env == Environment::Production;  }
    [[nodiscard]] bool IsCustom()      const noexcept { return m_Env == Environment::Custom;      }

    [[nodiscard]] bool operator==(const EnvironmentSettings& other) const noexcept;
    [[nodiscard]] bool operator!=(const EnvironmentSettings& other) const noexcept;

private:
    Environment m_Env        = Environment::Development;
    String      m_CustomName;                          ///< Only used when m_Env == Custom

    static Environment  ParseEnvString(StringView raw);
};

} // namespace Hydra
