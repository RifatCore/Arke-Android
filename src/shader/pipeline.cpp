#include "arke/shader/pipeline.hpp"
#include "arke/logger.hpp"

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <string>

namespace arke::shader {

static bool posix_file_exists(const std::string& path) {
    return (access(path.c_str(), F_OK) == 0);
}

static std::string posix_read_file(const std::string& path) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return "";
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(fp);
        return "";
    }
    std::string content(sz, '\0');
    fread(&content[0], 1, sz, fp);
    fclose(fp);
    return content;
}

ShaderPipeline& ShaderPipeline::instance() {
    static ShaderPipeline s_instance;
    return s_instance;
}

ShaderPipeline::ShaderPipeline() {
    m_preprocessor.setIncludeCallback([this](const std::string& includePath) -> std::string {
        std::string fullPath = m_currentPackPath + "/shaders" + includePath;
        if (!posix_file_exists(fullPath)) {
            fullPath = m_currentPackPath + "/" + includePath;
        }
        std::string content = posix_read_file(fullPath);
        if (content.empty()) {
            Logger::instance().warn("[ShaderPipeline] Unable to read include file: " + fullPath);
        }
        return content;
    });
}

bool ShaderPipeline::loadShaderStage(
    const std::string& baseName,
    PassStage stage,
    const std::string& vshPath,
    const std::string& fshPath
) {
    std::string vshSource = posix_read_file(vshPath);
    std::string fshSource = posix_read_file(fshPath);

    if (vshSource.empty() && fshSource.empty()) {
        return false;
    }

    if (!vshSource.empty()) {
        Logger::instance().info("[ShaderPipeline] Loaded vertex shader: " + vshPath + " (" + std::to_string(vshSource.size()) + " bytes)");
    }
    if (!fshSource.empty()) {
        Logger::instance().info("[ShaderPipeline] Loaded fragment shader: " + fshPath + " (" + std::to_string(fshSource.size()) + " bytes)");
    }

    CompiledPass pass;
    pass.name = baseName;
    pass.stage = stage;

    if (!vshSource.empty()) {
        std::string preprocessedV = m_preprocessor.preprocess(vshSource, baseName + ".vsh");
        auto vRes = m_compiler.compile(preprocessedV, ShaderStage::Vertex, ShaderTarget::OpenGLES_320, baseName + ".vsh");
        pass.vertexESSL = vRes.esslSource;
        pass.vertexSPIRV = vRes.spirvWords;
    }

    if (!fshSource.empty()) {
        std::string preprocessedF = m_preprocessor.preprocess(fshSource, baseName + ".fsh");
        auto fRes = m_compiler.compile(preprocessedF, ShaderStage::Fragment, ShaderTarget::OpenGLES_320, baseName + ".fsh");
        pass.fragmentESSL = fRes.esslSource;
        pass.fragmentSPIRV = fRes.spirvWords;
    }

    m_compiledPasses.push_back(std::move(pass));
    Logger::instance().info("[ShaderPipeline] Compiled pipeline stage: " + baseName);
    return true;
}

bool ShaderPipeline::loadPack(const std::string& packPath) {
    m_currentPackPath = packPath;
    m_compiledPasses.clear();

    // Extract pack name
    size_t lastSlash = packPath.find_last_of('/');
    if (lastSlash != std::string::npos && lastSlash + 1 < packPath.size()) {
        m_currentPackName = packPath.substr(lastSlash + 1);
    } else {
        m_currentPackName = packPath;
    }

    Logger::instance().info("[ShaderPipeline] Initializing shader pack: " + m_currentPackName + " from " + packPath);

    // 1. Parse shaders.properties
    std::string propsPath = packPath + "/shaders/shaders.properties";
    if (!posix_file_exists(propsPath)) {
        propsPath = packPath + "/shaders.properties";
    }
    if (posix_file_exists(propsPath)) {
        std::string propsContent = posix_read_file(propsPath);
        m_properties.parseProperties(propsContent);
        Logger::instance().info("[ShaderPipeline] Parsed shaders.properties (" +
                               std::to_string(m_properties.getProperties().size()) + " properties)");
    }

    // 2. Discover and compile passes
    std::string shadersDir = packPath + "/shaders";
    if (!posix_file_exists(shadersDir)) {
        shadersDir = packPath;
    }

    const std::vector<std::pair<std::string, PassStage>> targetPasses = {
        {"gbuffers_basic", PassStage::GBuffers},
        {"gbuffers_textured", PassStage::GBuffers},
        {"gbuffers_terrain", PassStage::GBuffers},
        {"gbuffers_water", PassStage::GBuffers},
        {"gbuffers_entities", PassStage::GBuffers},
        {"gbuffers_hand", PassStage::GBuffers},
        {"gbuffers_skybasic", PassStage::GBuffers},
        {"gbuffers_clouds", PassStage::GBuffers},
        {"gbuffers_weather", PassStage::GBuffers},
        {"shadow", PassStage::Shadow},
        {"deferred", PassStage::Deferred},
        {"composite", PassStage::Composite},
        {"composite1", PassStage::Composite},
        {"final", PassStage::Final}
    };

    for (const auto& [name, stage] : targetPasses) {
        std::string vsh = shadersDir + "/" + name + ".vsh";
        std::string fsh = shadersDir + "/" + name + ".fsh";
        loadShaderStage(name, stage, vsh, fsh);
    }

    m_loaded = !m_compiledPasses.empty();
    Logger::instance().info("[ShaderPipeline] Shader pack loaded with " +
                           std::to_string(m_compiledPasses.size()) + " active passes.");
    return m_loaded;
}

void ShaderPipeline::reload() {
    if (!m_currentPackPath.empty()) {
        loadPack(m_currentPackPath);
    }
}

} // namespace arke::shader
