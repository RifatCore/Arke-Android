#pragma once

#include <string>
#include "arke/types.hpp"

namespace arke {

struct ArkeConfig {
    bool enabled{true};
    std::string shaderpack{""};
    bool strictMath{false};
    float shadowDistance{128.0f};
    ColorSpace colorSpace{ColorSpace::sRGB};
    bool perfStats{false};
    bool graphicsDebug{false};
    std::string shaderpacksDir{"mods/arke/shaderpacks"};
    std::string configPath{"mods/arke/config.json"};
};

class ConfigManager {
public:
    static ConfigManager& instance();

    bool load(const std::string& configFilePath = "mods/arke/config.json");
    bool save(const std::string& configFilePath = "mods/arke/config.json");

    ArkeConfig& get() { return m_config; }
    const ArkeConfig& get() const { return m_config; }

private:
    ConfigManager() = default;
    ArkeConfig m_config;
};

} // namespace arke
