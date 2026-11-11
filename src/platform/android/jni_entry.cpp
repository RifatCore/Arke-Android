#include "arke/api.hpp"
#include "arke/logger.hpp"
#include "arke/config.hpp"
#include "arke/shader/pipeline.hpp"
#include "arke/platform/android_hook.hpp"
#include "arke/platform/imgui_overlay.hpp"

#if defined(__ANDROID__)
#include <jni.h>
#endif

// --- Exported C ABI Functions (Identical to Arke.dll) ---

extern "C" {

ARKE_EXPORT void ArkeReload(void) {
    arke::Logger::instance().info("ArkeReload called from host.");
    arke::shader::ShaderPipeline::instance().reload();
}

ARKE_EXPORT int32_t ArkeState(void) {
    return arke::shader::ShaderPipeline::instance().isPackLoaded() ? 1 : 0;
}

ARKE_EXPORT void ArkeTerrainChunks(void) {
    // Terrain chunk voxel / geometry callback
}

ARKE_EXPORT void ArkeSetFrameClock(double clock) {
    // Timing override
}

ARKE_EXPORT void ArkeReleaseFrameClock(void) {
}

ARKE_EXPORT void ArkeSetFrameTime(double time) {
}

ARKE_EXPORT void ArkeReleaseFrameTime(void) {
}

ARKE_EXPORT void ArkeSetFrameCounter(int32_t counter) {
}

ARKE_EXPORT void ArkeReleaseFrameCounter(void) {
}

ARKE_EXPORT void ArkeRunFrameCounter(void) {
}

ARKE_EXPORT void ArkeRenderedFrameCounter(void) {
}

#if defined(__ANDROID__)

ARKE_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    // Common Bedrock Android data directory
    const char* logDir = "/sdcard/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke/logs";
    arke::Logger::instance().init(logDir);
    arke::Logger::instance().info("=== Arke-Android (v0.1.1) loaded into process ===");

    // Load configuration
    arke::ConfigManager::instance().load("/sdcard/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke/config.json");

    // Initialize Android inline hooking layer
    arke::platform::AndroidHookLayer::instance().initialize();

    // Initialize Dear ImGui overlay
    arke::platform::ImGuiOverlay::instance().init();

    // Check if initial shader pack is defined
    const auto& conf = arke::ConfigManager::instance().get();
    if (conf.enabled && !conf.shaderpack.empty()) {
        std::string packPath = "/sdcard/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke/shaderpacks/" + conf.shaderpack;
        arke::shader::ShaderPipeline::instance().loadPack(packPath);
    }

    return JNI_VERSION_1_6;
}

ARKE_EXPORT void JNI_OnUnload(JavaVM* vm, void* reserved) {
    arke::Logger::instance().info("=== Arke-Android unloading ===");
    arke::platform::AndroidHookLayer::instance().shutdown();
    arke::platform::ImGuiOverlay::instance().shutdown();
}

#endif

} // extern "C"
