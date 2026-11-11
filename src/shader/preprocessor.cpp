#include "arke/shader/preprocessor.hpp"
#include "arke/logger.hpp"
#include <sstream>
#include <regex>

namespace arke::shader {

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

Preprocessor::Preprocessor() {
    // Injected Arke defines
    m_macros["IS_ARKE"] = "1";
    m_macros["ARKE_VERSION"] = "100";
    m_macros["ARKE_API"] = "1";

    // Injected Iris / Minecraft defines
    m_macros["MC_VERSION"] = "12000";
    m_macros["MC_GL_VERSION"] = "320";
    m_macros["MC_GLSL_VERSION"] = "320";
    m_macros["MC_RENDER_STAGE_TERRAIN_SOLID"] = "1";
    m_macros["MC_RENDER_STAGE_TERRAIN_CUTOUT"] = "2";
    m_macros["MC_RENDER_STAGE_WATER"] = "3";
    m_macros["MC_RENDER_STAGE_ENTITIES"] = "4";
    m_macros["MC_RENDER_STAGE_HAND"] = "5";

    // Recovered Arke architecture defines
    m_macros["ARKE_CAMERA_MAIN"] = "0";
    m_macros["ARKE_CAMERA_HAND"] = "1";
    m_macros["ARKE_CAMERA_SHADOW"] = "2";

    m_macros["ARKE_DIMENSION_OVERWORLD"] = "0";
    m_macros["ARKE_DIMENSION_NETHER"] = "1";
    m_macros["ARKE_DIMENSION_END"] = "2";

    m_macros["ARKE_GEOMETRY_TERRAIN"] = "1";
    m_macros["ARKE_GEOMETRY_ENTITY"] = "1";
    m_macros["ARKE_GEOMETRY_BLOCK_ENTITY"] = "1";
    m_macros["ARKE_GEOMETRY_HAND"] = "1";
    m_macros["ARKE_GEOMETRY_PARTICLE"] = "1";
    m_macros["ARKE_GEOMETRY_SKY"] = "1";
    m_macros["ARKE_GEOMETRY_MESH"] = "1";

    m_macros["ARKE_MATERIAL_PLAIN"] = "0u";
    m_macros["ARKE_MATERIAL_MULTITEXTURE"] = "1u";
    m_macros["ARKE_MATERIAL_TINT"] = "2u";
    m_macros["ARKE_MATERIAL_BANNER_LAYER"] = "3u";

    m_macros["ARKE_PLAYER_ON_GROUND"] = "1u";
    m_macros["ARKE_PLAYER_SNEAKING"] = "2u";
    m_macros["ARKE_PLAYER_SPRINTING"] = "4u";
    m_macros["ARKE_PLAYER_ON_FIRE"] = "8u";
    m_macros["ARKE_PLAYER_FIRST_PERSON"] = "32u";
}

void Preprocessor::setMacro(const std::string& name, const std::string& value) {
    m_macros[name] = value;
}

void Preprocessor::removeMacro(const std::string& name) {
    m_macros.erase(name);
}

std::string Preprocessor::processInternal(const std::string& source, const std::string& filename, std::set<std::string>& includeStack) {
    std::istringstream stream(source);
    std::ostringstream output;
    std::string line;

    std::vector<bool> conditionStack;
    conditionStack.push_back(true);

    auto shouldEmit = [&conditionStack]() {
        for (bool b : conditionStack) {
            if (!b) return false;
        }
        return true;
    };

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);

        if (trimmed.rfind("#ifdef", 0) == 0) {
            std::string macro = trim(trimmed.substr(6));
            bool defined = m_macros.find(macro) != m_macros.end();
            conditionStack.push_back(defined);
            continue;
        } else if (trimmed.rfind("#ifndef", 0) == 0) {
            std::string macro = trim(trimmed.substr(7));
            bool defined = m_macros.find(macro) != m_macros.end();
            conditionStack.push_back(!defined);
            continue;
        } else if (trimmed.rfind("#if", 0) == 0) {
            std::string expr = trim(trimmed.substr(3));
            bool defined = (m_macros.find(expr) != m_macros.end() && m_macros[expr] != "0");
            conditionStack.push_back(defined);
            continue;
        } else if (trimmed.rfind("#elif", 0) == 0) {
            if (conditionStack.size() > 1) {
                conditionStack.pop_back();
                std::string expr = trim(trimmed.substr(5));
                bool defined = (m_macros.find(expr) != m_macros.end() && m_macros[expr] != "0");
                conditionStack.push_back(defined);
            }
            continue;
        } else if (trimmed.rfind("#else", 0) == 0) {
            if (conditionStack.size() > 1) {
                bool last = conditionStack.back();
                conditionStack.pop_back();
                conditionStack.push_back(!last);
            }
            continue;
        } else if (trimmed.rfind("#endif", 0) == 0) {
            if (conditionStack.size() > 1) {
                conditionStack.pop_back();
            }
            continue;
        }

        if (!shouldEmit()) {
            continue;
        }

        // Handle #define dynamically
        if (trimmed.rfind("#define", 0) == 0) {
            std::istringstream iss(trimmed.substr(7));
            std::string name, val;
            iss >> name >> val;
            if (!name.empty()) {
                m_macros[name] = val.empty() ? "1" : val;
            }
            output << line << "\n";
            continue;
        }

        // Handle #include
        if (trimmed.rfind("#include", 0) == 0) {
            std::string incPath = trim(trimmed.substr(8));
            if (!incPath.empty() && (incPath.front() == '"' || incPath.front() == '<')) {
                incPath = incPath.substr(1, incPath.size() - 2);
            }

            if (includeStack.count(incPath)) {
                Logger::instance().warn("Cyclic include detected for: " + incPath);
                continue;
            }

            if (m_includeCallback) {
                includeStack.insert(incPath);
                std::string includedSource = m_includeCallback(incPath);
                output << "// --- Begin include: " << incPath << " ---\n";
                output << processInternal(includedSource, incPath, includeStack);
                output << "// --- End include: " << incPath << " ---\n";
                includeStack.erase(incPath);
            } else {
                output << "// [unresolved include " << incPath << "]\n";
            }
            continue;
        }

        output << line << "\n";
    }

    return output.str();
}

std::string Preprocessor::preprocess(const std::string& source, const std::string& filename) {
    std::ostringstream header;
    header << "// Preprocessed by Arke-Android (v0.1.1)\n"
           << "#version 320 es\n"
           << "precision highp float;\n"
           << "precision highp int;\n\n";

    // Inject base macros
    for (const auto& [k, v] : m_macros) {
        header << "#define " << k << " " << v << "\n";
    }
    header << "\n";

    std::set<std::string> includeStack;
    std::string processed = processInternal(source, filename, includeStack);
    return header.str() + processed;
}

} // namespace arke::shader
