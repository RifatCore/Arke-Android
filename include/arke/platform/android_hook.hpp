#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace arke::platform {

enum class GraphicsBackend {
    OpenGLES,
    Vulkan
};

class AndroidHookLayer {
public:
    static AndroidHookLayer& instance();

    bool initialize();
    void shutdown();

    GraphicsBackend getBackend() const { return m_backend; }
    bool isHooked() const { return m_hooked; }

    // Resolves a symbol from a loaded Android library or libminecraftpe.so
    void* resolveSymbol(const char* libName, const char* symName);

    // Hooks an explicit function pointer using Dobby/ShadowHook
    bool hookFunction(void* target, void* replacement, void** original);

private:
    AndroidHookLayer() = default;

    bool hookEGL();
    bool hookGLES();
    bool hookVulkan();
    bool hookRenderDragon();

    bool m_hooked{false};
    GraphicsBackend m_backend{GraphicsBackend::OpenGLES};
};

} // namespace arke::platform
