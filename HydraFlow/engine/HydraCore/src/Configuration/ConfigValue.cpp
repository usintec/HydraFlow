#include <HydraCore/Configuration/ConfigValue.h>

#include <stdexcept>

namespace Hydra {

// =============================================================================
// Constructors
// =============================================================================

ConfigValue::ConfigValue(bool value)         : m_Data(value)                      {}
ConfigValue::ConfigValue(i32  value)         : m_Data(static_cast<i64>(value))    {}
ConfigValue::ConfigValue(i64  value)         : m_Data(value)                      {}
ConfigValue::ConfigValue(u32  value)         : m_Data(static_cast<u64>(value))    {}
ConfigValue::ConfigValue(u64  value)         : m_Data(value)                      {}
ConfigValue::ConfigValue(f32  value)         : m_Data(static_cast<f64>(value))    {}
ConfigValue::ConfigValue(f64  value)         : m_Data(value)                      {}
ConfigValue::ConfigValue(const char*   value): m_Data(value ? value : "")         {}
ConfigValue::ConfigValue(StringView    value): m_Data(std::string(value))         {}
ConfigValue::ConfigValue(const String& value): m_Data(value)                      {}

ConfigValue::ConfigValue(std::initializer_list<ConfigValue> values)
{
    m_Data = nlohmann::json::array();
    for (const auto& v : values)
        m_Data.push_back(v.m_Data);
}

// =============================================================================
// Type inspection
// =============================================================================

ConfigValue::Type ConfigValue::GetType() const noexcept
{
    switch (m_Data.type()) {
        case nlohmann::json::value_t::null:             return Type::Null;
        case nlohmann::json::value_t::boolean:          return Type::Bool;
        case nlohmann::json::value_t::number_integer:   return Type::Integer;
        case nlohmann::json::value_t::number_unsigned:  return Type::Integer;
        case nlohmann::json::value_t::number_float:     return Type::Float;
        case nlohmann::json::value_t::string:           return Type::String;
        case nlohmann::json::value_t::array:            return Type::Array;
        case nlohmann::json::value_t::object:           return Type::Object;
        default:                                        return Type::Null;
    }
}

bool ConfigValue::IsNull()    const noexcept { return m_Data.is_null();             }
bool ConfigValue::IsBool()    const noexcept { return m_Data.is_boolean();          }
bool ConfigValue::IsInteger() const noexcept { return m_Data.is_number_integer() || m_Data.is_number_unsigned(); }
bool ConfigValue::IsFloat()   const noexcept { return m_Data.is_number_float();     }
bool ConfigValue::IsNumber()  const noexcept { return m_Data.is_number();           }
bool ConfigValue::IsString()  const noexcept { return m_Data.is_string();           }
bool ConfigValue::IsArray()   const noexcept { return m_Data.is_array();            }
bool ConfigValue::IsObject()  const noexcept { return m_Data.is_object();           }

// =============================================================================
// Typed extraction
// =============================================================================

Optional<bool> ConfigValue::AsBool() const
{
    if (m_Data.is_boolean()) return m_Data.get<bool>();
    return std::nullopt;
}

Optional<i64> ConfigValue::AsInt() const
{
    if (m_Data.is_number_integer())  return m_Data.get<i64>();
    if (m_Data.is_number_unsigned()) return static_cast<i64>(m_Data.get<u64>());
    if (m_Data.is_number_float())    return static_cast<i64>(m_Data.get<f64>());
    return std::nullopt;
}

Optional<f64> ConfigValue::AsFloat() const
{
    if (m_Data.is_number_float())    return m_Data.get<f64>();
    if (m_Data.is_number_integer())  return static_cast<f64>(m_Data.get<i64>());
    if (m_Data.is_number_unsigned()) return static_cast<f64>(m_Data.get<u64>());
    return std::nullopt;
}

Optional<String> ConfigValue::AsString() const
{
    if (m_Data.is_string()) return m_Data.get<String>();
    return std::nullopt;
}

// =============================================================================
// Array access
// =============================================================================

usize ConfigValue::Size() const
{
    if (m_Data.is_array() || m_Data.is_object())
        return m_Data.size();
    return 0;
}

bool ConfigValue::Empty() const
{
    return Size() == 0;
}

ConfigValue ConfigValue::At(usize index) const
{
    if (!m_Data.is_array() || index >= m_Data.size())
        return ConfigValue{};   // Null
    ConfigValue out;
    out.m_Data = m_Data[index];
    return out;
}

// =============================================================================
// Object access
// =============================================================================

bool ConfigValue::HasKey(StringView key) const
{
    if (!m_Data.is_object()) return false;
    return m_Data.contains(std::string(key));
}

usize ConfigValue::KeyCount() const
{
    if (!m_Data.is_object()) return 0;
    return m_Data.size();
}

ConfigValue ConfigValue::Get(StringView key) const
{
    if (!m_Data.is_object()) return ConfigValue{};
    auto it = m_Data.find(std::string(key));
    if (it == m_Data.end()) return ConfigValue{};
    ConfigValue out;
    out.m_Data = *it;
    return out;
}

// =============================================================================
// Mutation helpers
// =============================================================================

ConfigValue ConfigValue::MakeArray()
{
    ConfigValue v;
    v.m_Data = nlohmann::json::array();
    return v;
}

ConfigValue ConfigValue::MakeObject()
{
    ConfigValue v;
    v.m_Data = nlohmann::json::object();
    return v;
}

void ConfigValue::Append(ConfigValue value)
{
    if (!m_Data.is_array()) return;
    m_Data.push_back(std::move(value.m_Data));
}

void ConfigValue::Set(StringView key, ConfigValue value)
{
    if (!m_Data.is_object()) return;
    m_Data[std::string(key)] = std::move(value.m_Data);
}

// =============================================================================
// JSON round-tripping
// =============================================================================

ConfigValue ConfigValue::FromJson(const nlohmann::json& json)
{
    ConfigValue v;
    v.m_Data = json;
    return v;
}

nlohmann::json ConfigValue::ToJson() const
{
    return m_Data;
}

String ConfigValue::ToString(int indent) const
{
    return m_Data.dump(indent);
}

// =============================================================================
// Equality
// =============================================================================

bool ConfigValue::operator==(const ConfigValue& other) const noexcept
{
    return m_Data == other.m_Data;
}

bool ConfigValue::operator!=(const ConfigValue& other) const noexcept
{
    return m_Data != other.m_Data;
}

} // namespace Hydra
