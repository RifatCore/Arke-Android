#include "arke/platform/imgui_overlay.hpp"
#include "arke/platform/gles_hook.hpp"
#include "arke/shader/pipeline.hpp"
#include "arke/config.hpp"
#include "arke/logger.hpp"
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <unordered_set>

extern "C" void ArkeReload(void);

namespace arke::platform {

static constexpr float LAUNCHER_X = 16.0f;
static constexpr float LAUNCHER_Y = 16.0f;
static constexpr float LAUNCHER_W = 120.0f;
static constexpr float LAUNCHER_H = 48.0f;

// Touch bounds for launcher button: [12, 142] x [12, 66]
static constexpr float LAUNCHER_TOUCH_MIN_X = 12.0f;
static constexpr float LAUNCHER_TOUCH_MAX_X = 142.0f;
static constexpr float LAUNCHER_TOUCH_MIN_Y = 12.0f;
static constexpr float LAUNCHER_TOUCH_MAX_Y = 66.0f;

static constexpr float SETTINGS_X = 16.0f;
static constexpr float SETTINGS_Y = 76.0f;
static constexpr float SETTINGS_W = 560.0f;
static constexpr float SETTINGS_H = 480.0f;

// Touch bounds for settings window: [16, 576] x [76, 556]
static constexpr float SETTINGS_TOUCH_MIN_X = SETTINGS_X;
static constexpr float SETTINGS_TOUCH_MAX_X = SETTINGS_X + SETTINGS_W;
static constexpr float SETTINGS_TOUCH_MIN_Y = SETTINGS_Y;
static constexpr float SETTINGS_TOUCH_MAX_Y = SETTINGS_Y + SETTINGS_H;

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

    // CRITICAL SAFEGUARD:
    // If x <= 4.0f && y <= 4.0f (invalid/zero coordinates) or isnan(x) or isnan(y) or x > 4096.0f or y > 4096.0f,
    // NEVER toggle the menu and NEVER consume the event (return false)!
    if (std::isnan(x) || std::isnan(y) || (x <= 4.0f && y <= 4.0f) || x > 4096.0f || y > 4096.0f) {
        static int s_safeguardLogCount = 0;
        if (s_safeguardLogCount < 10) {
            s_safeguardLogCount++;
            char logBuf[128];
            snprintf(logBuf, sizeof(logBuf), "[Touch] action=%d x=%.1f y=%.1f menuOpen=%d consumed=0 (IGNORED_SAFEGUARD)",
                     action, x, y, m_showSettings ? 1 : 0);
            Logger::instance().info(logBuf);
        }
        return false;
    }

    // Android AMotionEvent action definitions:
    // 0 = ACTION_DOWN, 1 = ACTION_UP, 2 = ACTION_MOVE, 3 = ACTION_CANCEL, 5 = ACTION_POINTER_DOWN, 6 = ACTION_POINTER_UP
    bool isDown = (action == 0 || action == 5);
    bool isUp   = (action == 1 || action == 6 || action == 3);
    bool isMove = (action == 2);

    // Hitbox definitions:
    // Top-left Arke launcher button: pos (16, 16), size (120, 48) -> strictly [12, 142] x [12, 66] (NEVER includes (0, 0)!)
    bool insideLauncher = (x >= LAUNCHER_TOUCH_MIN_X && x <= LAUNCHER_TOUCH_MAX_X &&
                           y >= LAUNCHER_TOUCH_MIN_Y && y <= LAUNCHER_TOUCH_MAX_Y);

    // Arke Settings window: pos (16, 76), size (560, 480) -> strictly [16, 576] x [76, 556]
    bool insideSettings = (x >= SETTINGS_TOUCH_MIN_X && x <= SETTINGS_TOUCH_MAX_X &&
                           y >= SETTINGS_TOUCH_MIN_Y && y <= SETTINGS_TOUCH_MAX_Y);

    bool consumed = false;

    if (!m_showSettings) {
        // --- MENU IS CLOSED ---
        // ONLY consume the touch event if inside launcher button.
        // On ACTION_DOWN inside [12, 142] x [12, 66], call tryToggleSettings().
        // For ALL other coordinates on screen, do NOT consume the event—let Minecraft receive 100% of touches!
        if (insideLauncher) {
            if (isDown) {
                tryToggleSettings();
            }
            consumed = true;
        } else {
            consumed = false;
        }
    } else {
        // --- MENU IS OPEN ---
        // 1. If ACTION_DOWN is inside launcher button, toggle m_showSettings to false (close menu) and consume.
        if (insideLauncher) {
            if (isDown) {
                m_showSettings = false;
                m_showDetails = false;
                Logger::instance().info("[ImGuiOverlay] Settings closed via launcher button tap");
            }
            consumed = true;
        }
        // 2. If touch is inside Arke Settings window:
        // Do NOT toggle m_showSettings! Forward the real (x, y) to ImGui so widgets work, and consume.
        else if (insideSettings) {
            io.AddMousePosEvent(x, y);
            if (isDown) {
                io.AddMouseButtonEvent(0, true);
            } else if (isMove) {
                // Dragging in ImGui - pos is updated above
            } else if (isUp) {
                m_pendingMouseUp = true;
                m_pendingMouseUpX = x;
                m_pendingMouseUpY = y;
                io.AddMouseButtonEvent(0, false);
            }
            consumed = true;
        }
        // 3. If touch is OUTSIDE both hitboxes:
        // Let it pass through to the game (do NOT toggle unless user taps Arke button or Close Window).
        else {
            if (isUp) {
                io.AddMousePosEvent(x, y);
                io.AddMouseButtonEvent(0, false);
            }
            consumed = false;
        }
    }

    // Log the first 20 touch events to arke.log
    static int s_loggedTouchCount = 0;
    if (s_loggedTouchCount < 20) {
        s_loggedTouchCount++;
        char logBuf[128];
        snprintf(logBuf, sizeof(logBuf), "[Touch] action=%d x=%.1f y=%.1f menuOpen=%d consumed=%d",
                 action, x, y, m_showSettings ? 1 : 0, consumed ? 1 : 0);
        Logger::instance().info(logBuf);
    }

    return consumed;
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
    ImGui::SetNextWindowPos(ImVec2(LAUNCHER_X, LAUNCHER_Y), ImGuiCond_Always);
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

        if (ImGui::Button("Arke##arke_launcher", ImVec2(LAUNCHER_W, LAUNCHER_H))) {
            tryToggleSettings();
        }

        ImGui::PopStyleColor(3);
    }
    ImGui::End();
}

void ImGuiOverlay::drawSettingsWindow() {
    ImGui::SetNextWindowPos(ImVec2(SETTINGS_X, SETTINGS_Y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(SETTINGS_W, SETTINGS_H), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Arke Settings###arke_settings", &m_showSettings, flags)) {
        ImGui::TextColored(ImVec4(0.2f, 0.7f, 1.0f, 1.0f), "Arke Shader Engine (v0.1.1 Android ARM64)");
        ImGui::Text("Minecraft Bedrock 26.60 | Mali-G57 GLES 3.2");
        ImGui::Separator();

        auto& shaderState = getArkeShaderState();
        auto& cfg = ConfigManager::instance().get();

        // Checkbox: Enable Shaders
        bool enabled = shaderState.shadersEnabled && cfg.enabled;
        if (ImGui::Checkbox("Enable Shaders", &enabled)) {
            shaderState.shadersEnabled = enabled;
            cfg.enabled = enabled;
            ConfigManager::instance().save();
            Logger::instance().info("[Settings] Shaders toggled: " + std::string(enabled ? "ON" : "OFF"));
        }

        // Shader Mode Preset Selector
        const char* const presetNames[] = {
            "Vibrant ACES Filmic",
            "Warm Golden Hour & Soft Shadows",
            "Cinematic High-Contrast & Bloom",
            "Custom Shader Pack from Disk"
        };
        int currentPreset = shaderState.presetMode;
        if (ImGui::Combo("Shader Mode", &currentPreset, presetNames, IM_ARRAYSIZE(presetNames))) {
            shaderState.applyPreset(currentPreset);
            Logger::instance().info("[Settings] Preset mode changed to: " + std::string(presetNames[currentPreset]));
        }

        // Live Real-Time Uniform Sliders
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "Live Shader Grading:");
        ImGui::SliderFloat("Saturation", &shaderState.saturation, 0.5f, 2.5f, "%.2f");
        ImGui::SliderFloat("Contrast", &shaderState.contrast, 0.8f, 1.8f, "%.2f");
        ImGui::SliderFloat("Exposure", &shaderState.exposure, 0.5f, 2.0f, "%.2f");
        ImGui::SliderFloat("Bloom Strength", &shaderState.bloomStrength, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Sunlight Warmth", &shaderState.warmth, -0.5f, 0.8f, "%.2f");

        if (ImGui::SliderFloat("Shadow Distance", &cfg.shadowDistance, 32.0f, 512.0f, "%.0f blocks")) {
            ConfigManager::instance().save();
        }

        // Custom Pack selector (if in Custom Pack mode)
        if (shaderState.presetMode == 3) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.5f, 1.0f), "Custom Shader Pack:");

            std::string currentPack = shader::ShaderPipeline::instance().getCurrentPackName();
            if (currentPack.empty()) currentPack = "None";
            ImGui::Text("Active: %s (%zu passes)", currentPack.c_str(), shader::ShaderPipeline::instance().getPasses().size());

            if (m_discoveredPacks.empty()) {
                scanShaderPacks();
            }

            if (ImGui::BeginCombo("Pack", m_selectedPackName.c_str())) {
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

            if (ImGui::Button("Rescan", ImVec2(100.0f, 36.0f))) {
                scanShaderPacks();
            }
            ImGui::SameLine();
            if (ImGui::Button("Apply & Load Pack", ImVec2(170.0f, 36.0f))) {
                if (!m_selectedPackPath.empty()) {
                    shader::ShaderPipeline::instance().loadPack(m_selectedPackPath);
                    cfg.shaderpack = m_selectedPackName;
                    ConfigManager::instance().save();
                    shaderState.shaderReloadRequested.store(true);
                    Logger::instance().info("[Settings] Applied shader pack: " + m_selectedPackName);
                }
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Reload Shaders", ImVec2(150.0f, 38.0f))) {
            ArkeReload();
        }
        ImGui::SameLine();
        if (ImGui::Button("Pack Details...", ImVec2(150.0f, 38.0f))) {
            m_showDetails = !m_showDetails;
        }
        ImGui::SameLine();
        if (ImGui::Button("Close Window", ImVec2(140.0f, 38.0f))) {
            m_showSettings = false;
            m_showDetails = false;
            Logger::instance().info("[ImGuiOverlay] Settings closed via Close Window button");
        }
    }
    ImGui::End();
}

void ImGuiOverlay::drawDetailsWindow() {
    ImGui::SetNextWindowPos(ImVec2(16.0f, 76.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(540.0f, 320.0f), ImGuiCond_Always);

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
