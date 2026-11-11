#include "arke/platform/imgui_overlay.hpp"
#include "arke/shader/pipeline.hpp"
#include "arke/config.hpp"
#include "arke/logger.hpp"
#include <vector>

namespace arke::platform {

ImGuiOverlay& ImGuiOverlay::instance() {
    static ImGuiOverlay s_instance;
    return s_instance;
}

bool ImGuiOverlay::init() {
    if (m_initialized) return true;
    Logger::instance().info("Initializing Android Dear ImGui Overlay...");
    m_initialized = true;
    return true;
}

void ImGuiOverlay::shutdown() {
    m_initialized = false;
    Logger::instance().info("Shutting down Android Dear ImGui Overlay.");
}

void ImGuiOverlay::handleTouchEvent(int action, float x, float y) {
    // Action 0 = DOWN, 1 = UP, 2 = MOVE
    if (action == 0) { // Touch DOWN
        // If touching the top-left area (0,0) -> (200, 100), toggle settings
        if (x >= 0.0f && x <= 220.0f && y >= 0.0f && y <= 110.0f) {
            toggleSettings();
            Logger::instance().info("Toggled Arke settings menu via touch event.");
        }
    }
}

void ImGuiOverlay::drawLauncherButton() {
    // Render the top-left launcher button ##arke_launcher (matching Arke.dll)
    // In real Dear ImGui:
    // ImGui::SetNextWindowPos(ImVec2(10, 10));
    // if (ImGui::Button("Arke##arke_launcher", ImVec2(100, 40))) { toggleSettings(); }
}

void ImGuiOverlay::drawSettingsWindow() {
    // Render the main settings modal window ###arke_settings
    // In real Dear ImGui:
    // ImGui::Begin("Arke Settings###arke_settings", &m_showSettings);
    // ImGui::Text("Active Shader Pack: %s", shader::ShaderPipeline::instance().getCurrentPackName().c_str());
    // if (ImGui::Button("Reload Pack")) { shader::ShaderPipeline::instance().reload(); }
    // ImGui::End();
}

void ImGuiOverlay::drawDetailsWindow() {
    // Render the details / logs modal window ###arke_details
}

void ImGuiOverlay::newFrame(float width, float height) {
    m_displayWidth = width;
    m_displayHeight = height;
    if (!m_initialized) {
        init();
    }
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
}

} // namespace arke::platform
