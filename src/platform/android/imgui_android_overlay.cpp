#include "arke/platform/imgui_overlay.hpp"
#include "arke/shader/pipeline.hpp"
#include "arke/config.hpp"
#include "arke/logger.hpp"
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <unordered_set>

extern "C" void ArkeReload(void);

namespace arke::platform {

ImGuiOverlay& ImGuiOverlay::instance() {
    static ImGuiOverlay s_instance;
    return s_instance;
}

bool ImGuiOverlay::init() {
    if (m_initialized) return true;
    Logger::instance().info("[ImGuiOverlay] Initializing Android Dear ImGui Overlay...");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(m_displayWidth, m_displayHeight);
    io.IniFilename = nullptr; // Don't write imgui.ini on Android

    ImGui::StyleColorsDark();

    // Scale UI comfortably for mobile touchscreens
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.6f);
    style.WindowRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;

    // Initialize OpenGL ES 3.0+ backend
    if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
        Logger::instance().error("[ImGuiOverlay] Failed to initialize ImGui_ImplOpenGL3 backend");
        return false;
    }

    m_initialized = true;
    scanShaderPacks();
    Logger::instance().info("[ImGuiOverlay] Dear ImGui Overlay initialized successfully.");
    return true;
}

void ImGuiOverlay::shutdown() {
    if (!m_initialized) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
    Logger::instance().info("[ImGuiOverlay] Shut down Android Dear ImGui Overlay.");
}

void ImGuiOverlay::scanShaderPacks() {
    m_discoveredPacks.clear();

    const std::vector<std::string> searchRoots = {
        "/storage/emulated/0/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke/shaderpacks",
        "/sdcard/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke/shaderpacks",
        "/sdcard/games/com.mojang/mods/arke/shaderpacks",
        "/storage/emulated/0/Download/shaderpacks",
        "/sdcard/Download/shaderpacks",
        "/storage/emulated/0/Download",
        "/sdcard/Download"
    };

    std::unordered_set<std::string> seenPacks;

    for (const auto& root : searchRoots) {
        DIR* dir = opendir(root.c_str());
        if (!dir) continue;

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

            std::string name = entry->d_name;
            std::string fullPath = root + "/" + name;

            struct stat st;
            if (stat(fullPath.c_str(), &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                std::string subShaders = fullPath + "/shaders";
                std::string subProps = fullPath + "/shaders.properties";
                std::string subFinal = fullPath + "/shaders/final.fsh";

                struct stat subSt;
                if (stat(subShaders.c_str(), &subSt) == 0 ||
                    stat(subProps.c_str(), &subSt) == 0 ||
                    stat(subFinal.c_str(), &subSt) == 0) {
                    if (seenPacks.find(name) == seenPacks.end()) {
                        seenPacks.insert(name);
                        m_discoveredPacks.push_back({name, fullPath});
                    }
                }
            } else if (S_ISREG(st.st_mode)) {
                if (name.size() > 4 && (name.substr(name.size() - 4) == ".zip" ||
                                        name.substr(name.size() - 7) == ".mcpack")) {
                    if (seenPacks.find(name) == seenPacks.end()) {
                        seenPacks.insert(name);
                        m_discoveredPacks.push_back({name, fullPath});
                    }
                }
            }
        }
        closedir(dir);
    }

    if (m_selectedPackName.empty() && !m_discoveredPacks.empty()) {
        m_selectedPackName = m_discoveredPacks[0].first;
        m_selectedPackPath = m_discoveredPacks[0].second;
    }
}

bool ImGuiOverlay::handleTouchEvent(int action, float x, float y) {
    if (!m_initialized) {
        init();
    }

    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(x, y);

    // Android AMotionEvent action definitions:
    // 0 = ACTION_DOWN, 1 = ACTION_UP, 2 = ACTION_MOVE, 3 = ACTION_CANCEL, 5 = ACTION_POINTER_DOWN, 6 = ACTION_POINTER_UP
    bool isDown = (action == 0 || action == 5);
    bool isUp = (action == 1 || action == 6 || action == 3);

    if (isDown) {
        io.AddMouseButtonEvent(0, true);
    } else if (isUp) {
        io.AddMouseButtonEvent(0, false);
    }

    // Direct touch hot-zone: top-left corner toggle
    bool insideLauncher = (x >= 0.0f && x <= 220.0f && y >= 0.0f && y <= 110.0f);
    if (insideLauncher && isDown) {
        toggleSettings();
        Logger::instance().info("[ImGuiOverlay] Toggled Arke settings menu via touch on launcher button.");
        return true;
    }

    // Check if touch is inside the open Settings window
    if (m_showSettings) {
        if (io.WantCaptureMouse || (x >= 20.0f && x <= 580.0f && y >= 70.0f && y <= 550.0f)) {
            return true;
        }
    }

    // Check if touch is inside the open Details window
    if (m_showDetails) {
        if (io.WantCaptureMouse || (x >= 20.0f && x <= 580.0f && y >= 70.0f && y <= 450.0f)) {
            return true;
        }
    }

    return io.WantCaptureMouse;
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
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("##arke_launcher_panel", nullptr, flags)) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.45f, 0.85f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.55f, 0.95f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.35f, 0.75f, 1.0f));

        if (ImGui::Button("Arke##arke_launcher", ImVec2(140.0f, 50.0f))) {
            toggleSettings();
            Logger::instance().info("[ImGuiOverlay] Toggled Arke settings menu via ImGui Button click.");
        }

        ImGui::PopStyleColor(3);
    }
    ImGui::End();
}

void ImGuiOverlay::drawSettingsWindow() {
    ImGui::SetNextWindowPos(ImVec2(30.0f, 80.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520.0f, 440.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Arke Settings###arke_settings", &m_showSettings)) {
        ImGui::TextColored(ImVec4(0.2f, 0.7f, 1.0f, 1.0f), "Arke Shader Engine (v0.1.1 Android ARM64)");
        ImGui::Text("Minecraft Bedrock 26.60 | Mali-G57 GLES 3.2");
        ImGui::Separator();

        std::string currentPack = shader::ShaderPipeline::instance().getCurrentPackName();
        if (currentPack.empty()) currentPack = "None";
        ImGui::Text("Active Pack: %s", currentPack.c_str());
        ImGui::Text("Loaded Passes: %zu", shader::ShaderPipeline::instance().getPasses().size());

        if (ImGui::Button("Rescan Packs", ImVec2(150.0f, 36.0f))) {
            scanShaderPacks();
        }

        if (m_discoveredPacks.empty()) {
            scanShaderPacks();
        }

        if (ImGui::BeginCombo("Shader Pack", m_selectedPackName.c_str())) {
            for (const auto& pack : m_discoveredPacks) {
                bool isSelected = (m_selectedPackName == pack.first);
                if (ImGui::Selectable(pack.first.c_str(), isSelected)) {
                    m_selectedPackName = pack.first;
                    m_selectedPackPath = pack.second;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Apply & Load Pack", ImVec2(180.0f, 38.0f))) {
            if (!m_selectedPackPath.empty()) {
                shader::ShaderPipeline::instance().loadPack(m_selectedPackPath);
                auto& cfg = ConfigManager::instance().get();
                cfg.shaderpack = m_selectedPackName;
                ConfigManager::instance().save();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload (ArkeReload)", ImVec2(180.0f, 38.0f))) {
            ArkeReload();
        }

        ImGui::Separator();
        auto& cfg = ConfigManager::instance().get();
        if (ImGui::Checkbox("Enable Custom Shaders", &cfg.enabled)) {
            ConfigManager::instance().save();
        }

        if (ImGui::SliderFloat("Shadow Distance", &cfg.shadowDistance, 32.0f, 512.0f, "%.0f blocks")) {
            ConfigManager::instance().save();
        }

        ImGui::Separator();
        if (ImGui::Button("Pack Details...", ImVec2(150.0f, 36.0f))) {
            m_showDetails = !m_showDetails;
        }
        ImGui::SameLine();
        if (ImGui::Button("Close Settings", ImVec2(150.0f, 36.0f))) {
            m_showSettings = false;
        }
    }
    ImGui::End();
}

void ImGuiOverlay::drawDetailsWindow() {
    ImGui::SetNextWindowPos(ImVec2(60.0f, 100.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500.0f, 300.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Arke Details###arke_details", &m_showDetails)) {
        ImGui::Text("Target: Minecraft Bedrock 26.60 (arm64-v8a)");
        ImGui::Text("Backend: OpenGL ES 3.2 (RenderDragon / BGFX)");
        ImGui::Separator();
        ImGui::TextWrapped("RenderDragon uniform interception: u_view, u_proj, ViewPositionAndTime, FogColor, FogAndDistanceControl mapped to slot b0/b2.");
        ImGui::Separator();

        const auto& passes = shader::ShaderPipeline::instance().getPasses();
        ImGui::Text("Compiled Passes (%zu):", passes.size());
        for (const auto& pass : passes) {
            ImGui::BulletText("%s [Stage: %d]", pass.name.c_str(), static_cast<int>(pass.stage));
        }

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(100.0f, 34.0f))) {
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
