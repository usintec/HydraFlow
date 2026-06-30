#include <HydraCore/Config/ConfigManager.h>
#include <HydraCore/Logging/Logger.h>

#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace Hydra {

namespace {

/// Recursively converts a YAML::Node into nlohmann::json.
nlohmann::json YamlToJson(const YAML::Node& node)
{
    switch (node.Type()) {
        case YAML::NodeType::Null:
            return nullptr;

        case YAML::NodeType::Scalar: {
            // Try integer, then double, then bool, fall back to string
            try { return node.as<i64>(); }  catch (...) {}
            try { return node.as<f64>(); }  catch (...) {}
            try { return node.as<bool>(); } catch (...) {}
            return node.as<std::string>();
        }

        case YAML::NodeType::Sequence: {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& child : node)
                arr.push_back(YamlToJson(child));
            return arr;
        }

        case YAML::NodeType::Map: {
            nlohmann::json obj = nlohmann::json::object();
            for (const auto& kv : node)
                obj[kv.first.as<std::string>()] = YamlToJson(kv.second);
            return obj;
        }

        default:
            return nullptr;
    }
}

} // anonymous namespace

bool ConfigManager::LoadFile(const std::filesystem::path& path)
{
    const std::string ext = path.extension().string();

    try {
        if (ext == ".yaml" || ext == ".yml") {
            YAML::Node root = YAML::LoadFile(path.string());
            m_Root   = YamlToJson(root);
            m_Loaded = true;
        } else if (ext == ".json") {
            std::ifstream file(path);
            if (!file.is_open()) {
                fprintf(stderr, "[ConfigManager] Cannot open file: %s\n", path.c_str());
                return false;
            }
            file >> m_Root;
            m_Loaded = true;
        } else {
            fprintf(stderr, "[ConfigManager] Unsupported config format: %s\n", ext.c_str());
            return false;
        }
    } catch (const std::exception& ex) {
        fprintf(stderr, "[ConfigManager] Failed to parse '%s': %s\n",
                path.c_str(), ex.what());
        return false;
    }

    return m_Loaded;
}

bool ConfigManager::IsLoaded() const noexcept
{
    return m_Loaded;
}

String ConfigManager::Dump(int indent) const
{
    return m_Root.dump(indent);
}

const nlohmann::json* ConfigManager::NavigateTo(StringView keyPath) const
{
    const nlohmann::json* current = &m_Root;
    std::string           segment;
    std::string           path(keyPath);

    std::istringstream stream(path);
    while (std::getline(stream, segment, '.')) {
        if (!current->is_object())
            return nullptr;
        auto it = current->find(segment);
        if (it == current->end())
            return nullptr;
        current = &(*it);
    }
    return current;
}

nlohmann::json* ConfigManager::NavigateTo(StringView keyPath)
{
    return const_cast<nlohmann::json*>(
        static_cast<const ConfigManager*>(this)->NavigateTo(keyPath));
}

} // namespace Hydra
