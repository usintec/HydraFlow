#include <HydraCore/Configuration/ConfigSchema.h>
#include <HydraCore/Configuration/Configuration.h>

#include <format>

namespace Hydra {

// =============================================================================
// ValidationResult
// =============================================================================

StringView ValidationResult::FirstError() const noexcept
{
    if (m_Errors.empty()) return {};
    return m_Errors.front();
}

void ValidationResult::AddError(String message)
{
    m_Errors.push_back(std::move(message));
}

void ValidationResult::Merge(const ValidationResult& other)
{
    for (const auto& e : other.m_Errors)
        m_Errors.push_back(e);
}

// =============================================================================
// ConfigSchema — fluent builder
// =============================================================================

ConfigSchema& ConfigSchema::RequireKey(String dotPath)
{
    m_Required.push_back({ std::move(dotPath) });
    return *this;
}

ConfigSchema& ConfigSchema::RequireType(String dotPath, ConfigValue::Type expectedType)
{
    m_Types.push_back({ std::move(dotPath), expectedType });
    return *this;
}

ConfigSchema& ConfigSchema::RequireRange(String dotPath,
                                         Optional<f64> min,
                                         Optional<f64> max)
{
    m_Ranges.push_back({ std::move(dotPath), min, max });
    return *this;
}

ConfigSchema& ConfigSchema::AllowValues(String dotPath, Vector<String> allowed)
{
    m_Allowed.push_back({ std::move(dotPath), std::move(allowed) });
    return *this;
}

bool ConfigSchema::IsEmpty() const noexcept
{
    return m_Required.empty() && m_Types.empty()
        && m_Ranges.empty()   && m_Allowed.empty();
}

// =============================================================================
// Validate
// =============================================================================

namespace {

const char* TypeName(ConfigValue::Type t)
{
    switch (t) {
        case ConfigValue::Type::Null:    return "Null";
        case ConfigValue::Type::Bool:    return "Bool";
        case ConfigValue::Type::Integer: return "Integer";
        case ConfigValue::Type::Float:   return "Float";
        case ConfigValue::Type::String:  return "String";
        case ConfigValue::Type::Array:   return "Array";
        case ConfigValue::Type::Object:  return "Object";
    }
    return "Unknown";
}

} // anonymous namespace

ValidationResult ConfigSchema::Validate(const Configuration& config) const
{
    ValidationResult result;

    // ---- RequireKey ---------------------------------------------------------
    for (const auto& rule : m_Required) {
        if (!config.Has(rule.path))
            result.AddError(std::format("Required key '{}' is missing.", rule.path));
    }

    // ---- RequireType --------------------------------------------------------
    for (const auto& rule : m_Types) {
        const ConfigNode node = config.Get(rule.path);
        if (!node.IsValid()) {
            result.AddError(std::format(
                "Key '{}' is missing (expected type {}).", rule.path, TypeName(rule.type)));
            continue;
        }
        if (node.GetType() != rule.type) {
            result.AddError(std::format(
                "Key '{}' has type {} but {} was expected.",
                rule.path, TypeName(node.GetType()), TypeName(rule.type)));
        }
    }

    // ---- RequireRange -------------------------------------------------------
    for (const auto& rule : m_Ranges) {
        const ConfigNode node = config.Get(rule.path);
        if (!node.IsValid()) {
            result.AddError(std::format("Key '{}' is missing (range check).", rule.path));
            continue;
        }
        const auto num = node.AsFloat();
        if (!num) {
            result.AddError(std::format(
                "Key '{}' is not numeric (range check requires a number).", rule.path));
            continue;
        }
        if (rule.min && *num < *rule.min)
            result.AddError(std::format(
                "Key '{}' value {} is below minimum {}.", rule.path, *num, *rule.min));
        if (rule.max && *num > *rule.max)
            result.AddError(std::format(
                "Key '{}' value {} exceeds maximum {}.", rule.path, *num, *rule.max));
    }

    // ---- AllowValues --------------------------------------------------------
    for (const auto& rule : m_Allowed) {
        const ConfigNode node = config.Get(rule.path);
        if (!node.IsValid()) {
            result.AddError(std::format(
                "Key '{}' is missing (allowed-values check).", rule.path));
            continue;
        }
        const auto str = node.AsString();
        if (!str) {
            result.AddError(std::format(
                "Key '{}' is not a string (allowed-values check).", rule.path));
            continue;
        }
        bool found = false;
        for (const auto& allowed : rule.allowed) {
            if (*str == allowed) { found = true; break; }
        }
        if (!found) {
            result.AddError(std::format(
                "Key '{}' value '{}' is not in the allowed set.", rule.path, *str));
        }
    }

    return result;
}

} // namespace Hydra
