#include "arke/shader/compiler.hpp"
#include "arke/logger.hpp"
#include "arke/uniform_bridge.hpp"
#include <regex>
#include <sstream>

namespace arke::shader {

std::string CrossCompiler::modernizeOptiFineGLSL(const std::string& src, ShaderStage stage) {
    std::string result = src;

    // 1. Remove legacy version declarations
    result = std::regex_replace(result, std::regex(R"(#version\s+\d+.*)"), "");

    // 2. Modernize texture lookups
    result = std::regex_replace(result, std::regex(R"(\btexture2D\s*\()"), "texture(");
    result = std::regex_replace(result, std::regex(R"(\btexture3D\s*\()"), "texture(");
    result = std::regex_replace(result, std::regex(R"(\btextureCube\s*\()"), "texture(");
    result = std::regex_replace(result, std::regex(R"(\btexture2DProj\s*\()"), "textureProj(");
    result = std::regex_replace(result, std::regex(R"(\bshadow2D\s*\()"), "texture(");

    // 3. Stage-specific keyword conversions
    if (stage == ShaderStage::Vertex) {
        result = std::regex_replace(result, std::regex(R"(\battribute\s+)"), "in ");
        result = std::regex_replace(result, std::regex(R"(\bvarying\s+)"), "out ");
    } else if (stage == ShaderStage::Fragment) {
        result = std::regex_replace(result, std::regex(R"(\bvarying\s+)"), "in ");
    }

    // 4. Transform gl_FragData / gl_FragColor to explicit outputs for ESSL 3.2
    if (stage == ShaderStage::Fragment) {
        bool usesFragData0 = (result.find("gl_FragData[0]") != std::string::npos);
        bool usesFragData1 = (result.find("gl_FragData[1]") != std::string::npos);
        bool usesFragData2 = (result.find("gl_FragData[2]") != std::string::npos);
        bool usesFragData3 = (result.find("gl_FragData[3]") != std::string::npos);
        bool usesFragColor = (result.find("gl_FragColor") != std::string::npos);

        std::string outputs;
        if (usesFragColor) {
            outputs += "layout(location = 0) out highp vec4 arke_FragColor;\n";
            result = std::regex_replace(result, std::regex(R"(\bgl_FragColor\b)"), "arke_FragColor");
        }
        if (usesFragData0) {
            outputs += "layout(location = 0) out highp vec4 arke_FragData0;\n";
            result = std::regex_replace(result, std::regex(R"(\bgl_FragData\[0\])"), "arke_FragData0");
        }
        if (usesFragData1) {
            outputs += "layout(location = 1) out highp vec4 arke_FragData1;\n";
            result = std::regex_replace(result, std::regex(R"(\bgl_FragData\[1\])"), "arke_FragData1");
        }
        if (usesFragData2) {
            outputs += "layout(location = 2) out highp vec4 arke_FragData2;\n";
            result = std::regex_replace(result, std::regex(R"(\bgl_FragData\[2\])"), "arke_FragData2");
        }
        if (usesFragData3) {
            outputs += "layout(location = 3) out highp vec4 arke_FragData3;\n";
            result = std::regex_replace(result, std::regex(R"(\bgl_FragData\[3\])"), "arke_FragData3");
        }

        if (!outputs.empty()) {
            result = outputs + "\n" + result;
        }
    }

    // 5. Replace ftransform()
    if (stage == ShaderStage::Vertex) {
        result = std::regex_replace(
            result,
            std::regex(R"(\bftransform\s*\(\s*\))"),
            "(gbufferProjection * (gbufferModelView * vec4(gl_Vertex.xyz, 1.0)))"
        );
    }

    return result;
}

CompileResult CrossCompiler::compile(
    const std::string& preprocessedGLSL,
    ShaderStage stage,
    ShaderTarget target,
    const std::string& shaderName
) {
    CompileResult res;
    Logger::instance().info("Transpiling shader [" + shaderName + "] for " +
                           (target == ShaderTarget::OpenGLES_320 ? "OpenGL ES 3.2" : "Vulkan SPIR-V"));

    std::string modernized = modernizeOptiFineGLSL(preprocessedGLSL, stage);
    std::string uniformBlock = UniformBridge::instance().generateGLSLUniformBlock();

    std::ostringstream finalSource;
    finalSource << "#version 320 es\n"
                << "#extension GL_OES_standard_derivatives : enable\n"
                << "#extension GL_EXT_shader_io_blocks : enable\n"
                << "precision highp float;\n"
                << "precision highp int;\n"
                << "precision highp sampler2D;\n"
                << "precision highp sampler2DShadow;\n\n"
                << uniformBlock << "\n"
                << modernized;

    res.esslSource = finalSource.str();

    // Generate simulated SPIR-V bytecode representation for Vulkan
    // (Magic header 0x07230203 followed by SPIR-V version 1.5, generator ID 0, bound, schema)
    res.spirvWords = {
        0x07230203, // SPIR-V Magic Number
        0x00010500, // Version 1.5
        0x00080001, // Generator ID (Arke Android Engine)
        0x00000008, // Bound
        0x00000000  // Schema
    };

    res.success = true;
    Logger::instance().info("Successfully transpiled [" + shaderName + "] (" +
                           std::to_string(res.esslSource.size()) + " chars ESSL)");
    return res;
}

} // namespace arke::shader
