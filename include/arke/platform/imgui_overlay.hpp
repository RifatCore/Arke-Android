#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <utility>

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
    void toggleSettings() { m_showSettings = !m_showSettings; }

    void scanShaderPacks();

private:
    ImGuiOverlay() = default;

    void drawLauncherButton();
    void drawSettingsWindow();
    void drawDetailsWindow();

    bool m_initialized{false};
    bool m_showSettings{false};
    bool m_showDetails{false};
    float m_displayWidth{1920.0f};
    float m_displayHeight{1080.0f};

    // Discovered shader packs: pair of (packName, fullPath)
    std::vector<std::pair<std::string, std::string>> m_discoveredPacks;
    std::string m_selectedPackName;
    std::string m_selectedPackPath;
};

} // namespace arke::platform
