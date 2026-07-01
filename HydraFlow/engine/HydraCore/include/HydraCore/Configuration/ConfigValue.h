#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>

#include <nlohmann/json.hpp>

namespace Hydra {

// =============================================================================
// ConfigValue
//
// Dynamically-typed, copy-by-value node in a configuration tree.
// Backed internally by nlohmann::json for zero-overhead YAML/JSON
// round-tripping; the public API is entirely Hydra-native (no nlohmann types
// appear in member signatures visible to callers).
//
// Supported types:
//   Null | Bool | Integer (i64) | Float (f64) | String
//   | Array (ordered list of ConfigValue)
//   | Object (string-keyed map of ConfigValue, ordered by insertion)
// =============================================================================

class HYDRA_API ConfigValue
{
public:
    // -------------------------------------------------------------------------
    // Type tag
    // -------------------------------------------------------------------------
    enum class Type
    {
        Null,
        Bool,
        Integer,
        Float,
        String,
        Array,
        Object,
    };

    // -------------------------------------------------------------------------
    // Constructors
    // -------------------------------------------------------------------------
    ConfigValue()  = default;           ///< Null value
    ~ConfigValue() = default;

    ConfigValue(const ConfigValue&)            = default;
    ConfigValue& operator=(const ConfigValue&) = default;
    ConfigValue(ConfigValue&&)                 = default;
    ConfigValue& operator=(ConfigValue&&)      = default;

    explicit ConfigValue(bool       value);
    explicit ConfigValue(i32        value);
    explicit ConfigValue(i64        value);
    explicit ConfigValue(u32        value);
    explicit ConfigValue(u64        value);
    explicit ConfigValue(f32        value);
    explicit ConfigValue(f64        value);
    explicit ConfigValue(const char*   value);
    explicit ConfigValue(StringView    value);
    explicit ConfigValue(const String& value);

    /// Construct an Array from an initializer list of ConfigValues.
    explicit ConfigValue(std::initializer_list<ConfigValue> values);

    // -------------------------------------------------------------------------
    // Type inspection
    // -------------------------------------------------------------------------
    [[nodiscard]] Type GetType()   const noexcept;

    [[nodiscard]] bool IsNull()    const noexcept;
    [[nodiscard]] bool IsBool()    const noexcept;
    [[nodiscard]] bool IsInteger() const noexcept;
    [[nodiscard]] bool IsFloat()   const noexcept;
    [[nodiscard]] bool IsNumber()  const noexcept;   ///< Integer OR Float
    [[nodiscard]] bool IsString()  const noexcept;
    [[nodiscard]] bool IsArray()   const noexcept;
    [[nodiscard]] bool IsObject()  const noexcept;

    // -------------------------------------------------------------------------
    // Typed extraction — return nullopt when the value cannot be converted
    // -------------------------------------------------------------------------
    [[nodiscard]] Optional<bool>   AsBool()   const;
    [[nodiscard]] Optional<i64>    AsInt()    const;
    [[nodiscard]] Optional<f64>    AsFloat()  const;
    [[nodiscard]] Optional<String> AsString() const;

    /// Generic extraction.  Specialisations for bool/i32/i64/f32/f64/String
    /// are defined inline below; the primary template falls through to the
    /// nlohmann type-deduction machinery.
    template<typename T>
    [[nodiscard]] Optional<T> As() const;

    /// Extract with a fallback value when conversion fails.
    template<typename T>
    [[nodiscard]] T AsOr(T defaultValue) const;

    // -------------------------------------------------------------------------
    // Array access — only valid when IsArray()
    // -------------------------------------------------------------------------
    [[nodiscard]] usize       Size()  const;
    [[nodiscard]] bool        Empty() const;

    /// Returns a Null ConfigValue when the index is out of range.
    [[nodiscard]] ConfigValue At(usize index) const;

    // -------------------------------------------------------------------------
    // Object access — only valid when IsObject()
    // -------------------------------------------------------------------------
    [[nodiscard]] bool        HasKey(StringView key) const;
    [[nodiscard]] usize       KeyCount() const;

    /// Returns a Null ConfigValue when the key is absent.
    [[nodiscard]] ConfigValue Get(StringView key) const;

    // -------------------------------------------------------------------------
    // Mutation helpers (used by loaders and merge logic)
    // -------------------------------------------------------------------------
    [[nodiscard]] static ConfigValue MakeArray();
    [[nodiscard]] static ConfigValue MakeObject();

    /// Append to an Array.  No-op if not an Array.
    void Append(ConfigValue value);

    /// Insert or replace a key in an Object.  No-op if not an Object.
    void Set(StringView key, ConfigValue value);

    // -------------------------------------------------------------------------
    // Round-tripping with nlohmann::json
    // -------------------------------------------------------------------------
    [[nodiscard]] static ConfigValue FromJson(const nlohmann::json& json);
    [[nodiscard]] nlohmann::json     ToJson()                          const;

    /// Human-readable representation.
    ///   indent == -1  → single-line compact form
    ///   indent >= 0   → pretty-printed with that many spaces
    [[nodiscard]] String ToString(int indent = -1) const;

    // -------------------------------------------------------------------------
    // Equality
    // -------------------------------------------------------------------------
    [[nodiscard]] bool operator==(const ConfigValue& other) const noexcept;
    [[nodiscard]] bool operator!=(const ConfigValue& other) const noexcept;

private:
    nlohmann::json m_Data;  ///< nlohmann::json is the backing store

    // Grant friends direct access to m_Data for efficient merging / export.
    friend class ConfigNode;
    friend class Configuration;
};

// =============================================================================
// Template specialisations (inline in the header)
// =============================================================================

template<typename T>
Optional<T> ConfigValue::As() const
{
    try { return m_Data.get<T>(); } catch (...) { return std::nullopt; }
}

template<>
inline Optional<bool> ConfigValue::As<bool>() const { return AsBool(); }

template<>
inline Optional<i64> ConfigValue::As<i64>() const { return AsInt(); }

template<>
inline Optional<i32> ConfigValue::As<i32>() const
{
    if (auto v = AsInt()) return static_cast<i32>(*v);
    return std::nullopt;
}

template<>
inline Optional<u32> ConfigValue::As<u32>() const
{
    if (auto v = AsInt()) return static_cast<u32>(*v);
    return std::nullopt;
}

template<>
inline Optional<u64> ConfigValue::As<u64>() const
{
    if (auto v = AsInt()) return static_cast<u64>(*v);
    return std::nullopt;
}

template<>
inline Optional<f64> ConfigValue::As<f64>() const { return AsFloat(); }

template<>
inline Optional<f32> ConfigValue::As<f32>() const
{
    if (auto v = AsFloat()) return static_cast<f32>(*v);
    return std::nullopt;
}

template<>
inline Optional<String> ConfigValue::As<String>() const { return AsString(); }

template<typename T>
T ConfigValue::AsOr(T defaultValue) const
{
    return As<T>().value_or(std::move(defaultValue));
}

} // namespace Hydra
