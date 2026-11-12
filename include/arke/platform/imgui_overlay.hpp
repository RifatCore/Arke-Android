#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <chrono>

namespace arke::platform {

class ImGuiOverlay {
public:
    static ImGuiOverlay& instance();

    bool init();
    void shutdown();

    void newFrame(float width, float height);
    void render();

    // Android touch event injection - returns true if consumed by Arke UI
    bool handleTouchEvent(int action, float x, float y);

    bool isSettingsOpen() const { return m_showSettings; }
    void toggleSettings() { tryToggleSettings(); }
    bool tryToggleSettings();

    void scanShaderPacks();

private:
    ImGuiOverlay() = default;

    void drawLauncherButton();
    void drawSettingsWindow();
    void drawDetailsWindow();

    bool m_initialized{false};
    bool m_showSettings{false};
    bool m_showDetails{false};
    float m_displayWidth{1600.0f};
    float m_displayHeight{720.0f};

    // Single debounced toggle timer
    std::chrono::steady_clock::time_point m_lastToggleTime{};

    // Delayed mouse release for reliable mobile tap registration
    bool m_pendingMouseUp{false};
    float m_pendingMouseUpX{0.0f};
    float m_pendingMouseUpY{0.0f};

    // Discovered shader packs: pair of (packName, fullPath)
    std::vector<std::pair<std::string, std::string>> m_discoveredPacks;
    std::string m_selectedPackName;
    std::string m_selectedPackPath;
};

} // namespace arke::platform
