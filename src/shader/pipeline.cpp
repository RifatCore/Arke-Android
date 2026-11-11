#include "arke/shader/pipeline.hpp"
#include "arke/logger.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace arke::shader {

ShaderPipeline& ShaderPipeline::instance() {
    static ShaderPipeline s_instance;
    return s_instance;
}

ShaderPipeline::ShaderPipeline() {
    // Setup include resolution relative to shaders/ directory
    m_preprocessor.setIncludeCallback([this](const std::string& includePath) -> std::string {
        std::string fullPath = m_currentPackPath + "/shaders" + includePath;
        if (!std::filesystem::exists(fullPath)) {
            fullPath = m_currentPackPath + "/" + includePath;
        }
        std::ifstream file(fullPath);
        if (!file.is_open()) {
            Logger::instance().warn("Unable to open include file: " + fullPath);
            return "";
        }
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    });
}

bool ShaderPipeline::loadShaderStage(
    const std::string& baseName,
    PassStage stage,
    const std::string& vshPath,
    const std::string& fshPath
) {
    std::string vshSource, fshSource;

    if (std::filesystem::exists(vshPath)) {
        std::ifstream vf(vshPath);
        std::stringstream ss;
        ss << vf.rdbuf();
        vshSource = ss.str();
    }
    if (std::filesystem::exists(fshPath)) {
        std::ifstream ff(fshPath);
        std::stringstream ss;
        ss << ff.rdbuf();
        fshSource = ss.str();
    }

    if (vshSource.empty() && fshSource.empty()) {
        return false;
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
    Logger::instance().info("Pipeline loaded and compiled pass: " + baseName);
    return true;
}

bool ShaderPipeline::loadPack(const std::string& packPath) {
    m_currentPackPath = packPath;
    m_compiledPasses.clear();

    std::filesystem::path p(packPath);
    m_currentPackName = p.filename().string();

    Logger::instance().info("Initializing shader pack: " + m_currentPackName + " from " + packPath);

    // 1. Parse shaders.properties
    std::string propsPath = packPath + "/shaders/shaders.properties";
    if (!std::filesystem::exists(propsPath)) {
        propsPath = packPath + "/shaders.properties";
    }
    if (std::filesystem::exists(propsPath)) {
        std::ifstream pf(propsPath);
        std::stringstream ss;
        ss << pf.rdbuf();
        m_properties.parseProperties(ss.str());
        Logger::instance().info("Parsed shaders.properties (" +
                               std::to_string(m_properties.getProperties().size()) + " properties)");
    }

    // 2. Discover and compile passes
    std::string shadersDir = packPath + "/shaders";
    if (!std::filesystem::exists(shadersDir)) {
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
    Logger::instance().info("Shader pack pipeline loaded with " +
                           std::to_string(m_compiledPasses.size()) + " active passes.");
    return m_loaded;
}

void ShaderPipeline::reload() {
    if (!m_currentPackPath.empty()) {
        loadPack(m_currentPackPath);
    }
}

} // namespace arke::shader
