#include "arke/config.hpp"
#include "arke/logger.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace arke {

ConfigManager& ConfigManager::instance() {
    static ConfigManager s_instance;
    return s_instance;
}

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool ConfigManager::load(const std::string& configFilePath) {
    m_config.configPath = configFilePath;
    std::ifstream file(configFilePath);
    if (!file.is_open()) {
        Logger::instance().info("config.json not found, using default configuration.");
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '{' || line.front() == '}') continue;

        auto colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = trim(line.substr(0, colonPos));
        std::string val = trim(line.substr(colonPos + 1));

        if (!val.empty() && val.back() == ',') val.pop_back();
        val = trim(val);

        // Strip quotes from key
        if (key.size() >= 2 && key.front() == '"' && key.back() == '"') {
            key = key.substr(1, key.size() - 2);
        }

        if (key == "enabled") {
            m_config.enabled = (val == "true");
        } else if (key == "shaderpack") {
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                val = val.substr(1, val.size() - 2);
            }
            m_config.shaderpack = val;
        } else if (key == "strictMath") {
            m_config.strictMath = (val == "true");
        } else if (key == "shadowDistance") {
            try { m_config.shadowDistance = std::stof(val); } catch (...) {}
        } else if (key == "perfStats") {
            m_config.perfStats = (val == "true");
        } else if (key == "colorSpace") {
            try { m_config.colorSpace = static_cast<ColorSpace>(std::stoi(val)); } catch (...) {}
        } else if (key == "graphicsDebug" || key == "d3d12Debug") {
            m_config.graphicsDebug = (val == "true");
        }
    }

    Logger::instance().info("Loaded configuration: shaderpack='" + m_config.shaderpack +
                           "', enabled=" + (m_config.enabled ? "true" : "false"));
    return true;
}

bool ConfigManager::save(const std::string& configFilePath) {
    try {
        std::filesystem::path p(configFilePath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream file(configFilePath);
        if (!file.is_open()) return false;

        file << "{\n";
        file << "  \"enabled\": " << (m_config.enabled ? "true" : "false") << ",\n";
        file << "  \"shaderpack\": \"" << m_config.shaderpack << "\",\n";
        file << "  \"strictMath\": " << (m_config.strictMath ? "true" : "false") << ",\n";
        file << "  \"shadowDistance\": " << m_config.shadowDistance << ",\n";
        file << "  \"colorSpace\": " << static_cast<uint32_t>(m_config.colorSpace) << ",\n";
        file << "  \"perfStats\": " << (m_config.perfStats ? "true" : "false") << ",\n";
        file << "  \"graphicsDebug\": " << (m_config.graphicsDebug ? "true" : "false") << "\n";
        file << "}\n";

        Logger::instance().info("Saved configuration to " + configFilePath);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace arke
