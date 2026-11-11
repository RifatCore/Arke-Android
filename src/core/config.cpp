#include "arke/config.hpp"
#include "arke/logger.hpp"
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
    FILE* fp = fopen(configFilePath.c_str(), "r");
    if (!fp) {
        Logger::instance().info("config.json not found at " + configFilePath + ", using defaults.");
        return false;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp)) {
        std::string line = trim(buffer);
        if (line.empty() || line.front() == '{' || line.front() == '}') continue;

        auto colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = trim(line.substr(0, colonPos));
        std::string val = trim(line.substr(colonPos + 1));

        if (!val.empty() && val.back() == ',') val.pop_back();
        val = trim(val);

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
    fclose(fp);

    Logger::instance().info("Loaded configuration: shaderpack='" + m_config.shaderpack +
                           "', enabled=" + (m_config.enabled ? "true" : "false"));
    return true;
}

bool ConfigManager::save(const std::string& configFilePath) {
    size_t lastSlash = configFilePath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        std::string parentDir = configFilePath.substr(0, lastSlash);
        std::string current;
        for (size_t i = 0; i < parentDir.size(); ++i) {
            current += parentDir[i];
            if (parentDir[i] == '/' || i == parentDir.size() - 1) {
                if (current != "/" && !current.empty()) {
                    struct stat st;
                    if (stat(current.c_str(), &st) != 0) {
                        mkdir(current.c_str(), 0755);
                    }
                }
            }
        }
    }

    FILE* fp = fopen(configFilePath.c_str(), "w");
    if (!fp) return false;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"enabled\": %s,\n", m_config.enabled ? "true" : "false");
    fprintf(fp, "  \"shaderpack\": \"%s\",\n", m_config.shaderpack.c_str());
    fprintf(fp, "  \"strictMath\": %s,\n", m_config.strictMath ? "true" : "false");
    fprintf(fp, "  \"shadowDistance\": %.1f,\n", m_config.shadowDistance);
    fprintf(fp, "  \"colorSpace\": %u,\n", static_cast<uint32_t>(m_config.colorSpace));
    fprintf(fp, "  \"perfStats\": %s,\n", m_config.perfStats ? "true" : "false");
    fprintf(fp, "  \"graphicsDebug\": %s\n", m_config.graphicsDebug ? "true" : "false");
    fprintf(fp, "}\n");
    fclose(fp);

    Logger::instance().info("Saved configuration to " + configFilePath);
    return true;
}

} // namespace arke
