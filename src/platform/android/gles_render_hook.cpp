#include "arke/platform/gles_hook.hpp"
#include "arke/platform/imgui_overlay.hpp"
#include "arke/uniform_bridge.hpp"
#include "arke/shader/pipeline.hpp"
#include "arke/logger.hpp"
#include <chrono>

namespace arke::platform {

static auto g_lastFrameTime = std::chrono::high_resolution_clock::now();
static double g_totalSeconds = 0.0;
static uint32_t g_frameCount = 0;

void onEglSwapBuffersPost() {
    auto now = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(now - g_lastFrameTime).count();
    g_lastFrameTime = now;
    g_totalSeconds += dt;
    g_frameCount++;

    // Update uniform bridge for the next frame
    UniformBridge::instance().updateFrame(dt, g_totalSeconds, g_frameCount);

    // Render in-game Dear ImGui overlay
    ImGuiOverlay::instance().newFrame(1920.0f, 1080.0f);
    ImGuiOverlay::instance().render();
}

void onGlShaderSourcePre(
    uint32_t shader,
    int32_t count,
    const char* const* string,
    const int32_t* length,
    const char** outCustomShader
) {
    if (!outCustomShader || !string || count <= 0) return;

    // Check if an Arke shader pack is loaded
    if (!shader::ShaderPipeline::instance().isPackLoaded()) {
        *outCustomShader = nullptr;
        return;
    }

    // Inspect shader source to detect RenderDragon stage
    std::string srcText = "";
    for (int i = 0; i < count; i++) {
        if (string[i]) {
            srcText += (length && length[i] > 0) ? std::string(string[i], length[i]) : std::string(string[i]);
        }
    }

    // Check if terrain or entity shader
    const auto& passes = shader::ShaderPipeline::instance().getPasses();
    for (const auto& pass : passes) {
        if (srcText.find("POSITION") != std::string::npos && !pass.vertexESSL.empty()) {
            *outCustomShader = pass.vertexESSL.c_str();
            Logger::instance().info("Substituted RenderDragon vertex shader with pass: " + pass.name);
            return;
        }
    }

    *outCustomShader = nullptr;
}

void onGlCompileShaderPost(uint32_t shader) {
    // Post-compile validation hook
}

} // namespace arke::platform
