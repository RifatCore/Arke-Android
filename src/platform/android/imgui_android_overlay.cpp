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
    Logger::instance().info("[ImGuiOverlay] Initializing Android Dear ImGui Overlay (Mobile DPI 1600x720)...");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(m_displayWidth, m_displayHeight);
    io.IniFilename = nullptr; // Don't write imgui.ini on Android

    // Mobile font & touch DPI scaling
    io.FontGlobalScale = 1.65f;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.5f);
    style.FrameRounding = 6.0f;
    style.WindowRounding = 8.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.TouchExtraPadding = ImVec2(4.0f, 4.0f);

    // Initialize OpenGL ES 3.0+ backend
    if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
        Logger::instance().error("[ImGuiOverlay] Failed to initialize ImGui_ImplOpenGL3 backend");
        return false;
    }

    m_initialized = true;
    scanShaderPacks();
    Logger::instance().info("[ImGuiOverlay] Dear ImGui Overlay initialized successfully with mobile DPI scaling.");
    return true;
}

void ImGuiOverlay::shutdown() {
    if (!m_initialized) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
    Logger::instance().info("[ImGuiOverlay] Shut down Android Dear ImGui Overlay.");
}

bool ImGuiOverlay::tryToggleSettings() {
    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastToggleTime).count();
    if (elapsedMs < 250) {
        return false; // Debounce rejection
    }
    m_lastToggleTime = now;
    m_showSettings = !m_showSettings;
    Logger::instance().info("[ImGuiOverlay] Settings window toggled: " + std::string(m_showSettings ? "OPEN" : "CLOSED"));
    return true;
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

    // Android AMotionEvent action definitions:
    // 0 = ACTION_DOWN, 1 = ACTION_UP, 2 = ACTION_MOVE, 3 = ACTION_CANCEL, 5 = ACTION_POINTER_DOWN, 6 = ACTION_POINTER_UP
    bool isDown = (action == 0 || action == 5);
    bool isUp = (action == 1 || action == 6 || action == 3);
    bool isMove = (action == 2);

    // 1. If Settings window is open, check if touch is inside Settings window bounds
    // Settings window: pos (16, 74), size (520, 420) -> rect: x in [16, 540], y in [70, 500]
    if (m_showSettings) {
        bool insideSettings = (x >= 16.0f && x <= 540.0f && y >= 70.0f && y <= 500.0f);
        if (insideSettings) {
            io.AddMousePosEvent(x, y);
            if (isDown || isMove) {
                io.AddMouseButtonEvent(0, true);
            } else if (isUp) {
                m_pendingMouseUp = true;
                m_pendingMouseUpX = x;
                m_pendingMouseUpY = y;
            }
            return true; // Completely consumed by Arke Settings window
        }
    }

    // 2. If Details window is open, check bounds: pos (16, 74), size (520, 320)
    if (m_showDetails) {
        bool insideDetails = (x >= 16.0f && x <= 540.0f && y >= 70.0f && y <= 400.0f);
        if (insideDetails) {
            io.AddMousePosEvent(x, y);
            if (isDown || isMove) {
                io.AddMouseButtonEvent(0, true);
            } else if (isUp) {
                m_pendingMouseUp = true;
                m_pendingMouseUpX = x;
                m_pendingMouseUpY = y;
            }
            return true; // Completely consumed by Details window
        }
    }

    // 3. Check Arke Launcher Button hitbox: pos (16, 16), size (120, 48)
    // Hitbox: x in [0, 160], y in [0, 68]. Notice y strictly <= 68 so it NEVER overlaps with Settings (y >= 70)
    bool insideLauncher = (x >= 0.0f && x <= 160.0f && y >= 0.0f && y <= 68.0f);
    if (insideLauncher) {
        if (isDown) {
            tryToggleSettings();
        }
        return true; // Consumed by launcher button
    }

    // 4. Check if ImGui wants mouse capture anywhere else
    if (io.WantCaptureMouse) {
        io.AddMousePosEvent(x, y);
        if (isDown || isMove) {
            io.AddMouseButtonEvent(0, true);
        } else if (isUp) {
            m_pendingMouseUp = true;
            m_pendingMouseUpX = x;
            m_pendingMouseUpY = y;
        }
        return true;
    }

    // 5. Unconsumed touch: forward position and release to ImGui, let Minecraft PE handle gameplay touch
    if (isUp) {
        io.AddMousePosEvent(x, y);
        io.AddMouseButtonEvent(0, false);
    }
    return false;
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

    // Dispatch delayed mouse release on frame boundary for 100% reliable touch button clicks
    if (m_pendingMouseUp) {
        io.AddMousePosEvent(m_pendingMouseUpX, m_pendingMouseUpY);
        io.AddMouseButtonEvent(0, false);
        m_pendingMouseUp = false;
    }

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

        if (ImGui::Button("Arke##arke_launcher", ImVec2(120.0f, 48.0f))) {
            tryToggleSettings();
        }

        ImGui::PopStyleColor(3);
    }
    ImGui::End();
}

void ImGuiOverlay::drawSettingsWindow() {
    ImGui::SetNextWindowPos(ImVec2(16.0f, 74.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(520.0f, 420.0f), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Arke Settings###arke_settings", &m_showSettings, flags)) {
        ImGui::TextColored(ImVec4(0.2f, 0.7f, 1.0f, 1.0f), "Arke Shader Engine (v0.1.1 Android ARM64)");
        ImGui::Text("Minecraft Bedrock 26.60 | Mali-G57 GLES 3.2");
        ImGui::Separator();

        std::string currentPack = shader::ShaderPipeline::instance().getCurrentPackName();
        if (currentPack.empty()) currentPack = "None";
        ImGui::Text("Active Pack: %s", currentPack.c_str());
        ImGui::Text("Loaded Passes: %zu", shader::ShaderPipeline::instance().getPasses().size());

        if (ImGui::Button("Rescan Packs", ImVec2(140.0f, 36.0f))) {
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

        if (ImGui::Button("Apply & Load Pack", ImVec2(170.0f, 38.0f))) {
            if (!m_selectedPackPath.empty()) {
                shader::ShaderPipeline::instance().loadPack(m_selectedPackPath);
                auto& cfg = ConfigManager::instance().get();
                cfg.shaderpack = m_selectedPackName;
                ConfigManager::instance().save();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload Shaders", ImVec2(160.0f, 38.0f))) {
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
        if (ImGui::Button("Close Window", ImVec2(150.0f, 36.0f))) {
            tryToggleSettings();
        }
    }
    ImGui::End();
}

void ImGuiOverlay::drawDetailsWindow() {
    ImGui::SetNextWindowPos(ImVec2(16.0f, 74.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(520.0f, 320.0f), ImGuiCond_Always);

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
        if (ImGui::Button("Close Details", ImVec2(140.0f, 34.0f))) {
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
