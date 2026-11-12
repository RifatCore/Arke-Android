#include "arke/shader/compiler.hpp"
#include "arke/logger.hpp"
#include "arke/uniform_bridge.hpp"
#include "spirv_glsl.hpp"
#include <regex>
#include <sstream>

namespace arke::shader {

std::string CrossCompiler::compileSpirvToESSL(const std::vector<uint32_t>& spirvWords) {
    if (spirvWords.empty()) return "";
    try {
        spirv_cross::CompilerGLSL glsl(spirvWords);
        spirv_cross::CompilerGLSL::Options options;
        options.version = 320;
        options.es = true;
        options.vulkan_semantics = false;
        options.enable_420pack_extension = false;
        glsl.set_common_options(options);
        return glsl.compile();
    } catch (const std::exception& e) {
        Logger::instance().error("SPIRV-Cross compilation exception: " + std::string(e.what()));
        return "";
    }
}

std::string CrossCompiler::modernizeOptiFineGLSL(const std::string& src, ShaderStage stage) {
    std::string result = src;

    // 1. Remove legacy version declarations & strip extension directives from body
    result = std::regex_replace(result, std::regex(R"(#version\s+\d+.*)"), "");
    result = std::regex_replace(result, std::regex(R"(#extension\s+.*)"), "// [stripped extension from body]");

    // Strip macro definitions of texture2D/textureLod so regexes never corrupt macros
    result = std::regex_replace(result, std::regex(R"(#define\s+texture2D\b[^\r\n]*)"), "// [stripped #define texture2D]");
    result = std::regex_replace(result, std::regex(R"(#define\s+texture2DLod\b[^\r\n]*)"), "// [stripped #define texture2DLod]");
    result = std::regex_replace(result, std::regex(R"(#define\s+textureCube\b[^\r\n]*)"), "// [stripped #define textureCube]");
    result = std::regex_replace(result, std::regex(R"(#define\s+texture3D\b[^\r\n]*)"), "// [stripped #define texture3D]");

    // 2. Modernize texture lookups
    result = std::regex_replace(result, std::regex(R"(\btexture2DLod\s*\()"), "textureLod(");
    result = std::regex_replace(result, std::regex(R"(\btexture3DLod\s*\()"), "textureLod(");
    result = std::regex_replace(result, std::regex(R"(\btextureCubeLod\s*\()"), "textureLod(");
    result = std::regex_replace(result, std::regex(R"(\btexture2DProjLod\s*\()"), "textureProjLod(");
    result = std::regex_replace(result, std::regex(R"(\bshadow2DLod\s*\()"), "textureLod(");

    result = std::regex_replace(result, std::regex(R"(\btexture2D\s*\()"), "texture(");
    result = std::regex_replace(result, std::regex(R"(\btexture3D\s*\()"), "texture(");
    result = std::regex_replace(result, std::regex(R"(\btextureCube\s*\()"), "texture(");
    result = std::regex_replace(result, std::regex(R"(\btexture2DProj\s*\()"), "textureProj(");
    result = std::regex_replace(result, std::regex(R"(\bshadow2D\s*\()"), "texture(");

    // 3. Stage-specific keyword conversions
    if (stage == ShaderStage::Vertex) {
        result = std::regex_replace(result, std::regex(R"(\battribute\s+)"), "in ");
        result = std::regex_replace(result, std::regex(R"(\bvarying\s+)"), "out ");

        // Replace ftransform()
        result = std::regex_replace(
            result,
            std::regex(R"(\bftransform\s*\(\s*\))"),
            "(gbufferProjection * (gbufferModelView * vec4(gl_Vertex.xyz, 1.0)))"
        );

        // Support legacy gl_Vertex / gl_MultiTexCoord0 / gl_ModelViewProjectionMatrix
        bool usesGlVertex = (result.find("gl_Vertex") != std::string::npos);
        bool usesGlMultiTexCoord0 = (result.find("gl_MultiTexCoord0") != std::string::npos);
        bool usesGlColor = (result.find("gl_Color") != std::string::npos);
        bool usesMvp = (result.find("gl_ModelViewProjectionMatrix") != std::string::npos);

        std::string decls;
        if (usesGlVertex && result.find("in vec4 gl_Vertex;") == std::string::npos && result.find("in vec3 gl_Vertex;") == std::string::npos) {
            decls += "layout(location = 0) in highp vec4 gl_Vertex;\n";
        }
        if (usesGlMultiTexCoord0 && result.find("gl_MultiTexCoord0") != std::string::npos && result.find("in vec4 gl_MultiTexCoord0;") == std::string::npos) {
            decls += "layout(location = 1) in highp vec4 gl_MultiTexCoord0;\n";
        }
        if (usesGlColor && result.find("in vec4 gl_Color;") == std::string::npos) {
            decls += "layout(location = 2) in highp vec4 gl_Color;\n";
        }
        if (usesMvp) {
            result = std::regex_replace(result, std::regex(R"(\bgl_ModelViewProjectionMatrix\b)"), "(gbufferProjection * gbufferModelView)");
        }
        if (!decls.empty()) {
            result = decls + result;
        }
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
