#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Configuration/ConfigValue.h>

namespace Hydra {

class Configuration;

// =============================================================================
// ValidationResult
//
// Returned by ConfigSchema::Validate().  Collects all errors found during a
// single validation pass so the caller can report all problems at once.
// =============================================================================

class HYDRA_API ValidationResult
{
public:
    ValidationResult()  = default;
    ~ValidationResult() = default;

    ValidationResult(const ValidationResult&)            = default;
    ValidationResult& operator=(const ValidationResult&) = default;
    ValidationResult(ValidationResult&&)                 = default;
    ValidationResult& operator=(ValidationResult&&)      = default;

    // -------------------------------------------------------------------------
    // Result query
    // -------------------------------------------------------------------------

    /// True when no errors were recorded.
    [[nodiscard]] bool IsValid() const noexcept { return m_Errors.empty(); }
    [[nodiscard]] explicit operator bool() const noexcept { return IsValid(); }

    [[nodiscard]] usize                   ErrorCount() const noexcept { return m_Errors.size(); }
    [[nodiscard]] const Vector<String>&   GetErrors()  const noexcept { return m_Errors; }

    /// First error message, or an empty string when valid.
    [[nodiscard]] StringView              FirstError() const noexcept;

    // -------------------------------------------------------------------------
    // Error accumulation (used internally by ConfigSchema)
    // -------------------------------------------------------------------------

    void AddError(String message);
    void Merge(const ValidationResult& other);

private:
    Vector<String> m_Errors;
};

// =============================================================================
// ConfigSchema
//
// Declarative validation rules applied to a Configuration.  Rules are
// accumulated with fluent builder calls and applied together via Validate().
//
// Supported constraints:
//   RequireKey     – the dot-path must exist (any type)
//   RequireType    – the dot-path must be a specific ConfigValue::Type
//   RequireRange   – the numeric value at the path must lie within [min, max]
//   AllowValues    – the string value at the path must be in an allowed set
// =============================================================================

class HYDRA_API ConfigSchema
{
public:
    ConfigSchema()  = default;
    ~ConfigSchema() = default;

    ConfigSchema(const ConfigSchema&)            = default;
    ConfigSchema& operator=(const ConfigSchema&) = default;
    ConfigSchema(ConfigSchema&&)                 = default;
    ConfigSchema& operator=(ConfigSchema&&)      = default;

    // -------------------------------------------------------------------------
    // Fluent builder — each call returns *this for chaining
    // -------------------------------------------------------------------------

    /// The dot-path must exist in the configuration (any type is acceptable).
    ConfigSchema& RequireKey(String dotPath);

    /// The dot-path must exist AND the value must be exactly the given type.
    ConfigSchema& RequireType(String dotPath, ConfigValue::Type expectedType);

    /// The numeric value at the path must satisfy  min <= value <= max.
    /// Pass nullopt to skip either bound.
    ConfigSchema& RequireRange(String dotPath,
                               Optional<f64> min,
                               Optional<f64> max);

    /// The string value at the path must be one of the listed literals.
    /// Comparison is case-sensitive.
    ConfigSchema& AllowValues(String dotPath, Vector<String> allowed);

    // -------------------------------------------------------------------------
    // Validation
    // -------------------------------------------------------------------------

    /// Validate a Configuration against all registered constraints.
    /// All failing constraints are reported — the pass does not stop early.
    [[nodiscard]] ValidationResult Validate(const Configuration& config) const;

    [[nodiscard]] bool IsEmpty() const noexcept;

private:
    // ---- Constraint records ------------------------------------------------

    struct RequiredRule   { String path; };
    struct TypeRule       { String path; ConfigValue::Type type; };
    struct RangeRule      { String path; Optional<f64> min; Optional<f64> max; };
    struct AllowedRule    { String path; Vector<String> allowed; };

    Vector<RequiredRule>  m_Required;
    Vector<TypeRule>      m_Types;
    Vector<RangeRule>     m_Ranges;
    Vector<AllowedRule>   m_Allowed;
};

} // namespace Hydra
