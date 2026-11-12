#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include "arke/types.hpp"

namespace arke::shader {

struct ShaderOption {
    std::string name;
    std::string value;
    std::string defaultValue;
    std::vector<std::string> allowedValues;
    std::string description;
    float minValue{0.0f};
    float maxValue{1.0f};
    float step{0.01f};
    bool isSlider{false};
};

struct BufferFormat {
    std::string target{"colortex0"};
    std::string format{"RGBA8"}; // RGBA8, RGBA16F, RGBA32F, R11G11B10F
    bool mipmap{false};
    bool nearest{false};
};

struct DimensionConfig {
    std::string dimensionFolder;
    std::string cloud;
    std::string sky;
    std::string fog;
};

class PropertiesParser {
public:
    PropertiesParser() = default;

    bool parseProperties(const std::string& content);
    bool parseDimensionProperties(const std::string& content);
    bool parseEntityProperties(const std::string& content);
    bool parseConfigGlsl(const std::string& content);

    const std::map<std::string, std::string>& getProperties() const { return m_properties; }
    const std::map<int32_t, std::string>& getBlockMappings() const { return m_blockMappings; }
    const std::map<std::string, ShaderOption>& getOptions() const { return m_options; }
    const std::map<std::string, BufferFormat>& getBufferFormats() const { return m_bufferFormats; }
    const std::set<std::string>& getSupportedFeatures() const { return m_features; }

    std::optional<std::string> getProperty(const std::string& key) const;
    bool hasFeature(const std::string& feature) const;

private:
    void parseLine(const std::string& rawLine);

    std::map<std::string, std::string> m_properties;
    std::map<int32_t, std::string> m_blockMappings;
    std::map<std::string, ShaderOption> m_options;
    std::map<std::string, BufferFormat> m_bufferFormats;
    std::set<std::string> m_features;
};

} // namespace arke::shader
