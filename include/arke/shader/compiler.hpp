#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "arke/types.hpp"

namespace arke::shader {

enum class ShaderTarget {
    OpenGLES_320,  // ESSL #version 320 es
    Vulkan_SPIRV   // Raw SPIR-V bytecode for vkCreateShaderModule
};

enum class ShaderStage {
    Vertex,
    Fragment,
    Geometry,
    Compute
};

struct CompileResult {
    bool success{false};
    std::string errorMessage;
    std::string esslSource;             // Target source code for OpenGL ES 3.2
    std::vector<uint32_t> spirvWords;   // Target binary bytecode for Vulkan
};

class CrossCompiler {
public:
    CrossCompiler() = default;

    // Cross-compiles preprocessed Java GLSL to OpenGL ES 3.2 ESSL or Vulkan SPIR-V
    CompileResult compile(
        const std::string& preprocessedGLSL,
        ShaderStage stage,
        ShaderTarget target = ShaderTarget::OpenGLES_320,
        const std::string& shaderName = "unknown"
    );

    // Cross-compiles raw SPIR-V binary words directly to OpenGL ES 3.2 ESSL via SPIRV-Cross
    std::string compileSpirvToESSL(const std::vector<uint32_t>& spirvWords);

private:
    // Transforms legacy OpenGL / OptiFine constructs to modern ESSL 3.2 / SPIR-V compliant GLSL
    std::string modernizeOptiFineGLSL(const std::string& src, ShaderStage stage);
};

} // namespace arke::shader
