#include <HydraCore/Configuration/Configuration.h>

#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace Hydra {

// =============================================================================
// Internal helpers
// =============================================================================

namespace {

nlohmann::json YamlNodeToJson(const YAML::Node& node)
{
    switch (node.Type()) {
        case YAML::NodeType::Null:
            return nullptr;

        case YAML::NodeType::Scalar: {
            try { return node.as<i64>();  } catch (...) {}
            try { return node.as<f64>();  } catch (...) {}
            try { return node.as<bool>(); } catch (...) {}
            return node.as<std::string>();
        }

        case YAML::NodeType::Sequence: {
            auto arr = nlohmann::json::array();
            for (const auto& child : node)
                arr.push_back(YamlNodeToJson(child));
            return arr;
        }

        case YAML::NodeType::Map: {
            auto obj = nlohmann::json::object();
            for (const auto& kv : node)
                obj[kv.first.as<std::string>()] = YamlNodeToJson(kv.second);
            return obj;
        }

        default:
            return nullptr;
    }
}

/// Deep-merge src into dst (dst wins on scalar conflict — caller inverts).
/// Both must be JSON objects; if not, src simply replaces dst.
void DeepMerge(nlohmann::json& dst, const nlohmann::json& src)
{
    if (!src.is_object() || !dst.is_object()) {
        dst = src;
        return;
    }
    for (auto it = src.begin(); it != src.end(); ++it) {
        if (dst.contains(it.key()) && dst[it.key()].is_object() && it.value().is_object()) {
            DeepMerge(dst[it.key()], it.value());
        } else {
            dst[it.key()] = it.value();
        }
    }
}

} // anonymous namespace

// =============================================================================
// Configuration
// =============================================================================

Configuration::Configuration(ConfigValue root, String sourcePath)
    : m_Root(std::move(root))
    , m_SourcePath(std::move(sourcePath))
    , m_Valid(!m_Root.IsNull())
{}

// ---- Node access ------------------------------------------------------------

ConfigNode Configuration::GetRoot() const
{
    if (!m_Valid) return ConfigNode{};
    return ConfigNode{ m_Root, {} };
}

ConfigNode Configuration::Get(StringView dotPath) const
{
    return GetRoot().Navigate(dotPath);
}

bool Configuration::Has(StringView dotPath) const
{
    return Get(dotPath).IsValid();
}

// ---- Merge ------------------------------------------------------------------

void Configuration::MergeOver(const Configuration& overlay)
{
    if (!overlay.m_Valid) return;

    if (!m_Valid) {
        // Nothing to merge into — just adopt the overlay
        *this = overlay;
        return;
    }

    // Deep-merge at the JSON level
    nlohmann::json dst = m_Root.ToJson();
    const nlohmann::json src = overlay.m_Root.ToJson();
    DeepMerge(dst, src);
    m_Root  = ConfigValue::FromJson(dst);
    m_Valid = !m_Root.IsNull();
}

// ---- Serialisation ----------------------------------------------------------

String Configuration::Dump(int indent) const
{
    if (!m_Valid) return "null";
    return m_Root.ToString(indent);
}

// =============================================================================
// Free function: LoadConfigurationFromFile
// =============================================================================

Configuration LoadConfigurationFromFile(const std::filesystem::path& path)
{
    const std::string ext = path.extension().string();

    try {
        nlohmann::json json;

        if (ext == ".yaml" || ext == ".yml") {
            YAML::Node root = YAML::LoadFile(path.string());
            json = YamlNodeToJson(root);
        } else if (ext == ".json") {
            std::ifstream file(path);
            if (!file.is_open()) {
                fprintf(stderr, "[Configuration] Cannot open: %s\n", path.c_str());
                return Configuration{};
            }
            file >> json;
        } else {
            fprintf(stderr, "[Configuration] Unsupported extension '%s': %s\n",
                    ext.c_str(), path.c_str());
            return Configuration{};
        }

        return Configuration{ ConfigValue::FromJson(json), path.string() };

    } catch (const std::exception& ex) {
        fprintf(stderr, "[Configuration] Failed to parse '%s': %s\n",
                path.c_str(), ex.what());
        return Configuration{};
    }
}

} // namespace Hydra
