#include "arke/shader/properties_parser.hpp"
#include <sstream>
#include <algorithm>

namespace arke::shader {

static std::string trimString(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

void PropertiesParser::parseLine(const std::string& rawLine) {
    std::string line = trimString(rawLine);
    if (line.empty() || line.front() == '#') return;

    // Check for comments trailing behind
    auto commentPos = line.find('#');
    if (commentPos != std::string::npos) {
        line = trimString(line.substr(0, commentPos));
    }

    auto eqPos = line.find('=');
    if (eqPos == std::string::npos) {
        // Feature flag declaration without '=' (e.g. CUSTOM_IMAGES, COMPUTE_SHADERS)
        m_features.insert(line);
        return;
    }

    std::string key = trimString(line.substr(0, eqPos));
    std::string val = trimString(line.substr(eqPos + 1));
    m_properties[key] = val;

    // Check block mappings: block.42 = minecraft:iron_bars
    if (key.rfind("block.", 0) == 0) {
        try {
            int32_t blockId = std::stoi(key.substr(6));
            m_blockMappings[blockId] = val;
        } catch (...) {}
    }
    // Check buffer formats: colortex0.format = RGBA16F
    else if (key.find(".format") != std::string::npos) {
        std::string bufName = key.substr(0, key.find(".format"));
        m_bufferFormats[bufName].target = bufName;
        m_bufferFormats[bufName].format = val;
    }
    // Check slider definitions: slider.SHADOW_RES = 512 4096 512
    else if (key.rfind("slider.", 0) == 0) {
        std::string optName = key.substr(7);
        std::istringstream iss(val);
        float minV = 0.f, maxV = 1.f, stepV = 0.1f;
        iss >> minV >> maxV >> stepV;
        ShaderOption opt;
        opt.name = optName;
        opt.isSlider = true;
        opt.minValue = minV;
        opt.maxValue = maxV;
        opt.step = stepV;
        m_options[optName] = opt;
    }
}

bool PropertiesParser::parseProperties(const std::string& content) {
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        parseLine(line);
    }
    return true;
}

bool PropertiesParser::parseDimensionProperties(const std::string& content) {
    return parseProperties(content);
}

bool PropertiesParser::parseEntityProperties(const std::string& content) {
    return parseProperties(content);
}

bool PropertiesParser::parseConfigGlsl(const std::string& content) {
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        std::string trimmed = trimString(line);
        if (trimmed.rfind("#define", 0) == 0) {
            std::istringstream iss(trimmed.substr(7));
            std::string name, val;
            iss >> name >> val;
            if (!name.empty()) {
                m_features.insert(name);
                m_properties[name] = val.empty() ? "1" : val;
            }
        }
    }
    return true;
}

std::optional<std::string> PropertiesParser::getProperty(const std::string& key) const {
    auto it = m_properties.find(key);
    if (it != m_properties.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool PropertiesParser::hasFeature(const std::string& feature) const {
    return m_features.find(feature) != m_features.end();
}

} // namespace arke::shader
