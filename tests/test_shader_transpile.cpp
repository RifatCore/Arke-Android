#include <iostream>
#include <cassert>
#include "arke/types.hpp"
#include "arke/logger.hpp"
#include "arke/uniform_bridge.hpp"
#include "arke/shader/properties_parser.hpp"
#include "arke/shader/preprocessor.hpp"
#include "arke/shader/compiler.hpp"

void testPropertiesParser() {
    std::cout << "[Test] Running PropertiesParser..." << std::endl;
    std::string sampleProps = R"(
# Sample shaders.properties
CUSTOM_IMAGES
COMPUTE_SHADERS
block.42 = minecraft:iron_bars
block.101 = minecraft:glow_frame
colortex0.format = RGBA16F
colortex1.format = RGBA8
slider.SHADOW_RES = 512 4096 512
profile.ULTRA = SHADOW_RES:4096
)";

    arke::shader::PropertiesParser parser;
    parser.parseProperties(sampleProps);

    assert(parser.hasFeature("CUSTOM_IMAGES"));
    assert(parser.hasFeature("COMPUTE_SHADERS"));
    assert(parser.getBlockMappings().at(42) == "minecraft:iron_bars");
    assert(parser.getBlockMappings().at(101) == "minecraft:glow_frame");
    assert(parser.getBufferFormats().at("colortex0").format == "RGBA16F");
    assert(parser.getOptions().at("SHADOW_RES").isSlider == true);
    assert(parser.getOptions().at("SHADOW_RES").maxValue == 4096.0f);
    std::cout << "  -> PropertiesParser PASSED!" << std::endl;
}

void testPreprocessor() {
    std::cout << "[Test] Running Preprocessor..." << std::endl;
    std::string sampleGLSL = R"(
#ifdef IS_ARKE
// Bedrock Arke specific logic
#define TEST_OK 1
#else
// Java only logic
#define TEST_FAIL 1
#endif

#if ARKE_VERSION >= 100
vec4 getArkeColor() { return vec4(1.0, 0.5, 0.2, 1.0); }
#endif
)";

    arke::shader::Preprocessor pp;
    std::string processed = pp.preprocess(sampleGLSL, "test.glsl");

    assert(processed.find("#define IS_ARKE 1") != std::string::npos);
    assert(processed.find("#define ARKE_VERSION 100") != std::string::npos);
    assert(processed.find("TEST_OK") != std::string::npos);
    assert(processed.find("TEST_FAIL") == std::string::npos);
    assert(processed.find("getArkeColor") != std::string::npos);
    std::cout << "  -> Preprocessor PASSED!" << std::endl;
}

void testCrossCompiler() {
    std::cout << "[Test] Running CrossCompiler (OptiFine GLSL -> ESSL 3.2)..." << std::endl;
    std::string sampleVertexGLSL = R"(
attribute vec4 gl_Vertex;
attribute vec4 gl_Color;
varying vec4 color;
varying vec2 texCoord;

void main() {
    color = gl_Color;
    texCoord = gl_MultiTexCoord0.xy;
    gl_Position = ftransform();
}
)";

    std::string sampleFragmentGLSL = R"(
varying vec4 color;
varying vec2 texCoord;
uniform sampler2D texture;

void main() {
    vec4 albedo = texture2D(texture, texCoord) * color;
    gl_FragData[0] = albedo;
}
)";

    arke::shader::CrossCompiler compiler;

    auto vRes = compiler.compile(sampleVertexGLSL, arke::shader::ShaderStage::Vertex, arke::shader::ShaderTarget::OpenGLES_320, "gbuffers_terrain.vsh");
    assert(vRes.success);
    assert(vRes.esslSource.find("#version 320 es") != std::string::npos);
    assert(vRes.esslSource.find("in vec4 gl_Vertex;") != std::string::npos);
    assert(vRes.esslSource.find("out vec4 color;") != std::string::npos);
    assert(vRes.esslSource.find("gbufferProjection *") != std::string::npos);

    auto fRes = compiler.compile(sampleFragmentGLSL, arke::shader::ShaderStage::Fragment, arke::shader::ShaderTarget::OpenGLES_320, "gbuffers_terrain.fsh");
    assert(fRes.success);
    assert(fRes.esslSource.find("#version 320 es") != std::string::npos);
    assert(fRes.esslSource.find("in vec4 color;") != std::string::npos);
    assert(fRes.esslSource.find("texture(") != std::string::npos);
    assert(fRes.esslSource.find("layout(location = 0) out highp vec4 arke_FragData0;") != std::string::npos);
    assert(fRes.esslSource.find("arke_FragData0 = albedo;") != std::string::npos);

    std::cout << "  -> CrossCompiler PASSED!" << std::endl;
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  Arke-Android Unit & Transpile Test Suite" << std::endl;
    std::cout << "==========================================" << std::endl;

    testPropertiesParser();
    testPreprocessor();
    testCrossCompiler();

    std::cout << "\nALL ARKE-ANDROID TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
