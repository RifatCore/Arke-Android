#include "arke/platform/imgui_overlay.hpp"
#include "arke/shader/pipeline.hpp"
#include "arke/config.hpp"
#include "arke/logger.hpp"
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"

namespace arke::platform {

ImGuiOverlay& ImGuiOverlay::instance() {
    static ImGuiOverlay s_instance;
    return s_instance;
}

bool ImGuiOverlay::init() {
    if (m_initialized) return true;
    Logger::instance().info("Initializing Android Dear ImGui Overlay...");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(m_displayWidth, m_displayHeight);
    io.IniFilename = nullptr; // Don't write imgui.ini on Android

    ImGui::StyleColorsDark();

    // Scale UI slightly for mobile high-DPI screens
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.5f);

    // Initialize OpenGL ES 3.0+ backend
    if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
        Logger::instance().error("Failed to initialize ImGui_ImplOpenGL3 backend");
        return false;
    }

    m_initialized = true;
    Logger::instance().info("Dear ImGui Overlay initialized successfully.");
    return true;
}

void ImGuiOverlay::shutdown() {
    if (!m_initialized) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
    Logger::instance().info("Shut down Android Dear ImGui Overlay.");
}

void ImGuiOverlay::handleTouchEvent(int action, float x, float y) {
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(x, y);

    // Action: 0 = DOWN, 1 = UP, 2 = MOVE
    if (action == 0) {
        io.MouseDown[0] = true;
    } else if (action == 1) {
        io.MouseDown[0] = false;
    }

    // Direct touch hot-zone: top-left corner toggle
    if (action == 0 && x >= 0.0f && x <= 220.0f && y >= 0.0f && y <= 110.0f) {
        toggleSettings();
        Logger::instance().info("Toggled Arke settings menu via touch zone.");
    }
}

void ImGuiOverlay::newFrame(float width, float height) {
    m_displayWidth = width;
    m_displayHeight = height;

    if (!m_initialized) {
        if (!init()) return;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(width, height);
    io.DeltaTime = 1.0f / 60.0f;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiOverlay::drawLauncherButton() {
    // Top-left launcher button ##arke_launcher (matching Arke.dll)
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("##arke_launcher_panel", nullptr, flags)) {
        if (ImGui::Button("Arke##arke_launcher", ImVec2(120.0f, 44.0f))) {
            toggleSettings();
        }
    }
    ImGui::End();
}

void ImGuiOverlay::drawSettingsWindow() {
    ImGui::SetNextWindowSize(ImVec2(480.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Arke Settings###arke_settings", &m_showSettings)) {
        ImGui::Text("Arke Shader Engine (v0.1.1 Android arm64)");
        ImGui::Text("Minecraft Bedrock 26.60 (libminecraftpe.so)");
        ImGui::Separator();

        std::string currentPack = shader::ShaderPipeline::instance().getCurrentPackName();
        ImGui::Text("Active Shader Pack: %s", currentPack.c_str());

        if (ImGui::Button("Reload Shaders", ImVec2(150.0f, 38.0f))) {
            shader::ShaderPipeline::instance().reload();
        }
        ImGui::SameLine();
        if (ImGui::Button("Pack Details...", ImVec2(150.0f, 38.0f))) {
            m_showDetails = !m_showDetails;
        }

        ImGui::Separator();
        auto& cfg = arke::ConfigManager::instance().get();
        if (ImGui::Checkbox("Enable Custom Shaders", &cfg.enabled)) {
            arke::ConfigManager::instance().save();
        }

        if (ImGui::SliderFloat("Shadow Distance", &cfg.shadowDistance, 32.0f, 512.0f)) {
            arke::ConfigManager::instance().save();
        }
    }
    ImGui::End();
}

void ImGuiOverlay::drawDetailsWindow() {
    ImGui::SetNextWindowSize(ImVec2(480.0f, 260.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Arke Details###arke_details", &m_showDetails)) {
        ImGui::Text("Target: Minecraft Bedrock 26.60 (arm64-v8a)");
        ImGui::Text("Backend: OpenGL ES 3.2 (RenderDragon / BGFX)");
        ImGui::Separator();
        ImGui::TextWrapped("RenderDragon uniform interception: u_view, u_proj, ViewPositionAndTime, FogColor, FogAndDistanceControl bound to slot b0/b2.");
        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(100.0f, 32.0f))) {
            m_showDetails = false;
        }
    }
    ImGui::End();
}

void ImGuiOverlay::render() {
    if (!m_initialized) return;

    drawLauncherButton();
    if (m_showSettings) {
        drawSettingsWindow();
    }
    if (m_showDetails) {
        drawDetailsWindow();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace arke::platform
