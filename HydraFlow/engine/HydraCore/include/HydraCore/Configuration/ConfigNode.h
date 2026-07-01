#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Configuration/ConfigValue.h>

namespace Hydra {

// =============================================================================
// ConfigNode
//
// A path-aware cursor into a ConfigValue tree.  Navigation never throws:
// accessing a missing key or out-of-range index returns an invalid node
// (IsValid() == false).  Typed extraction methods return nullopt / the
// provided default when the node is invalid or the type does not match.
//
// Nodes are cheap to copy because ConfigValue is copy-by-value.
// =============================================================================

class HYDRA_API ConfigNode
{
public:
    // -------------------------------------------------------------------------
    // Constructors
    // -------------------------------------------------------------------------

    /// Construct an invalid (missing) node — IsValid() returns false.
    ConfigNode() = default;

    /// Construct a valid node wrapping the given value at the given path.
    ConfigNode(ConfigValue value, String path);

    ConfigNode(const ConfigNode&)            = default;
    ConfigNode& operator=(const ConfigNode&) = default;
    ConfigNode(ConfigNode&&)                 = default;
    ConfigNode& operator=(ConfigNode&&)      = default;

    // -------------------------------------------------------------------------
    // Validity
    // -------------------------------------------------------------------------

    /// True when this node was produced by a successful navigation step
    /// (the path existed in the parent ConfigValue).
    [[nodiscard]] bool IsValid() const noexcept { return m_Valid; }
    [[nodiscard]] explicit operator bool() const noexcept { return m_Valid; }

    // -------------------------------------------------------------------------
    // Inspection
    // -------------------------------------------------------------------------

    [[nodiscard]] const ConfigValue& GetValue() const noexcept { return m_Value; }
    [[nodiscard]] StringView         GetPath()  const noexcept { return m_Path;  }
    [[nodiscard]] ConfigValue::Type  GetType()  const noexcept;

    // -------------------------------------------------------------------------
    // Child navigation (Object)
    //
    // Returns an invalid node when:
    //   – this node is not an Object
    //   – the requested key does not exist
    // -------------------------------------------------------------------------

    [[nodiscard]] ConfigNode Child(StringView key) const;
    [[nodiscard]] ConfigNode operator[](StringView key) const { return Child(key); }

    // -------------------------------------------------------------------------
    // Index navigation (Array)
    //
    // Returns an invalid node when:
    //   – this node is not an Array
    //   – index is out of range
    // -------------------------------------------------------------------------

    [[nodiscard]] ConfigNode At(usize index) const;

    // -------------------------------------------------------------------------
    // Dot-path navigation  ("a.b.c" traverses a → b → c)
    //
    // Empty segments (consecutive dots) are skipped.
    // Returns an invalid node if any segment along the path is missing.
    // -------------------------------------------------------------------------

    [[nodiscard]] ConfigNode Navigate(StringView dotPath) const;

    // -------------------------------------------------------------------------
    // Typed extraction
    // -------------------------------------------------------------------------

    [[nodiscard]] Optional<bool>   AsBool()   const;
    [[nodiscard]] Optional<i64>    AsInt()    const;
    [[nodiscard]] Optional<f64>    AsFloat()  const;
    [[nodiscard]] Optional<String> AsString() const;

    template<typename T>
    [[nodiscard]] Optional<T> As() const
    {
        if (!m_Valid) return std::nullopt;
        return m_Value.As<T>();
    }

    template<typename T>
    [[nodiscard]] T AsOr(T defaultValue) const
    {
        return As<T>().value_or(std::move(defaultValue));
    }

    // -------------------------------------------------------------------------
    // Convenience
    // -------------------------------------------------------------------------

    [[nodiscard]] usize Size()   const;    ///< 0 for non-Array/Object nodes
    [[nodiscard]] bool  Empty()  const;
    [[nodiscard]] bool  HasKey(StringView key) const;

private:
    ConfigValue m_Value;           ///< Copy of the value at this node
    String      m_Path;            ///< Dot-separated path from root
    bool        m_Valid = false;   ///< Whether the path existed
};

} // namespace Hydra
