#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "arke/types.hpp"
#include "arke/shader/properties_parser.hpp"
#include "arke/shader/preprocessor.hpp"
#include "arke/shader/compiler.hpp"

namespace arke::shader {

struct CompiledPass {
    std::string name;
    PassStage stage;
    std::string vertexESSL;
    std::string fragmentESSL;
    std::vector<uint32_t> vertexSPIRV;
    std::vector<uint32_t> fragmentSPIRV;
    uint32_t programId{0};
};

class ShaderPipeline {
public:
    static ShaderPipeline& instance();

    bool loadPack(const std::string& packPath);
    void reload();

    bool isPackLoaded() const { return m_loaded; }
    const std::string& getCurrentPackName() const { return m_currentPackName; }
    const std::vector<CompiledPass>& getPasses() const { return m_compiledPasses; }
    const PropertiesParser& getProperties() const { return m_properties; }

private:
    ShaderPipeline();

    bool loadShaderStage(const std::string& baseName, PassStage stage, const std::string& vshPath, const std::string& fshPath);

    bool m_loaded{false};
    std::string m_currentPackPath;
    std::string m_currentPackName;

    PropertiesParser m_properties;
    Preprocessor m_preprocessor;
    CrossCompiler m_compiler;
    std::vector<CompiledPass> m_compiledPasses;
};

} // namespace arke::shader
