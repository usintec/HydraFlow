#include <HydraCore/Configuration/ConfigNode.h>

#include <sstream>

namespace Hydra {

// =============================================================================
// Constructors
// =============================================================================

ConfigNode::ConfigNode(ConfigValue value, String path)
    : m_Value(std::move(value))
    , m_Path(std::move(path))
    , m_Valid(true)
{}

// =============================================================================
// Inspection
// =============================================================================

ConfigValue::Type ConfigNode::GetType() const noexcept
{
    if (!m_Valid) return ConfigValue::Type::Null;
    return m_Value.GetType();
}

// =============================================================================
// Child navigation (Object)
// =============================================================================

ConfigNode ConfigNode::Child(StringView key) const
{
    if (!m_Valid || !m_Value.IsObject())
        return ConfigNode{};

    if (!m_Value.HasKey(key))
        return ConfigNode{};

    String childPath = m_Path.empty()
                     ? String(key)
                     : m_Path + '.' + String(key);

    return ConfigNode{ m_Value.Get(key), std::move(childPath) };
}

// =============================================================================
// Index navigation (Array)
// =============================================================================

ConfigNode ConfigNode::At(usize index) const
{
    if (!m_Valid || !m_Value.IsArray())
        return ConfigNode{};

    if (index >= m_Value.Size())
        return ConfigNode{};

    String childPath = m_Path + '[' + std::to_string(index) + ']';
    return ConfigNode{ m_Value.At(index), std::move(childPath) };
}

// =============================================================================
// Dot-path navigation
// =============================================================================

ConfigNode ConfigNode::Navigate(StringView dotPath) const
{
    if (!m_Valid) return ConfigNode{};
    if (dotPath.empty()) return *this;

    ConfigNode current = *this;

    std::string segment;
    std::istringstream stream{ std::string(dotPath) };

    while (std::getline(stream, segment, '.')) {
        if (segment.empty()) continue;   // skip empty segments from leading/trailing dots
        current = current.Child(segment);
        if (!current.m_Valid) return ConfigNode{};
    }

    return current;
}

// =============================================================================
// Typed extraction
// =============================================================================

Optional<bool>   ConfigNode::AsBool()   const { return m_Valid ? m_Value.AsBool()   : Optional<bool>{};   }
Optional<i64>    ConfigNode::AsInt()    const { return m_Valid ? m_Value.AsInt()    : Optional<i64>{};    }
Optional<f64>    ConfigNode::AsFloat()  const { return m_Valid ? m_Value.AsFloat()  : Optional<f64>{};    }
Optional<String> ConfigNode::AsString() const { return m_Valid ? m_Value.AsString() : Optional<String>{}; }

// =============================================================================
// Convenience
// =============================================================================

usize ConfigNode::Size() const
{
    if (!m_Valid) return 0;
    return m_Value.Size();
}

bool ConfigNode::Empty() const
{
    return Size() == 0;
}

bool ConfigNode::HasKey(StringView key) const
{
    if (!m_Valid) return false;
    return m_Value.HasKey(key);
}

} // namespace Hydra
