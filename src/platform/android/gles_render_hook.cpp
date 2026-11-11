#include "arke/platform/gles_hook.hpp"
#include "arke/platform/imgui_overlay.hpp"
#include "arke/uniform_bridge.hpp"
#include "arke/shader/pipeline.hpp"
#include "arke/logger.hpp"
#include <GLES3/gl32.h>
#include <chrono>
#include <unordered_map>
#include <string>
#include <mutex>

namespace arke::platform {

static auto g_lastFrameTime = std::chrono::high_resolution_clock::now();
static double g_totalSeconds = 0.0;
static uint32_t g_frameCount = 0;

static std::mutex g_uniformMutex;
// Maps location -> uniform name
static std::unordered_map<int32_t, std::string> g_uniformLocationMap;

void onEglSwapBuffersPost() {
    auto now = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(now - g_lastFrameTime).count();
    g_lastFrameTime = now;
    g_totalSeconds += dt;
    g_frameCount++;

    // Update uniform bridge for the next frame
    UniformBridge::instance().updateFrame(dt, g_totalSeconds, g_frameCount);

    // Query active OpenGL ES viewport
    GLint viewport[4] = {0, 0, 1920, 1080};
    glGetIntegerv(GL_VIEWPORT, viewport);
    float width = static_cast<float>(viewport[2] > 0 ? viewport[2] : 1920);
    float height = static_cast<float>(viewport[3] > 0 ? viewport[3] : 1080);

    // Render in-game Dear ImGui overlay
    ImGuiOverlay::instance().newFrame(width, height);
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
    (void)shader;
    // Post-compile validation hook
}

void onGlGetUniformLocationPost(uint32_t program, const char* name, int32_t location) {
    (void)program;
    if (location < 0 || !name) return;

    std::lock_guard<std::mutex> lock(g_uniformMutex);
    g_uniformLocationMap[location] = std::string(name);
}

void onGlUniformMatrix4fvPre(int32_t location, int32_t count, uint8_t transpose, const float* value) {
    (void)count;
    (void)transpose;
    if (location < 0 || !value) return;

    std::string name;
    {
        std::lock_guard<std::mutex> lock(g_uniformMutex);
        auto it = g_uniformLocationMap.find(location);
        if (it != g_uniformLocationMap.end()) {
            name = it->second;
        }
    }

    if (!name.empty()) {
        UniformBridge::instance().setRenderDragonMatrix(name, value);
    }
}

void onGlUniform4fvPre(int32_t location, int32_t count, const float* value) {
    (void)count;
    if (location < 0 || !value) return;

    std::string name;
    {
        std::lock_guard<std::mutex> lock(g_uniformMutex);
        auto it = g_uniformLocationMap.find(location);
        if (it != g_uniformLocationMap.end()) {
            name = it->second;
        }
    }

    if (!name.empty()) {
        UniformBridge::instance().setRenderDragonVector4(name, value);
    }
}

} // namespace arke::platform
