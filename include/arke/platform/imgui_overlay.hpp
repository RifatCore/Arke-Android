#pragma once

#include <cstdint>
#include <string>

namespace arke::platform {

class ImGuiOverlay {
public:
    static ImGuiOverlay& instance();

    bool init();
    void shutdown();

    void newFrame(float width, float height);
    void render();

    // Android touch event injection
    void handleTouchEvent(int action, float x, float y);

    bool isSettingsOpen() const { return m_showSettings; }
    void toggleSettings() { m_showSettings = !m_showSettings; }

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
};

} // namespace arke::platform
