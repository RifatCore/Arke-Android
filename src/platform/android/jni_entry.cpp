#include "arke/api.hpp"
#include "arke/logger.hpp"
#include "arke/config.hpp"
#include "arke/shader/pipeline.hpp"
#include "arke/platform/android_hook.hpp"
#include "arke/platform/imgui_overlay.hpp"

#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>

#if defined(__ANDROID__)
#include <jni.h>
#include <android/log.h>
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
}

ARKE_EXPORT void ArkeSetFrameClock(double clock) {
    (void)clock;
}

ARKE_EXPORT void ArkeReleaseFrameClock(void) {
}

ARKE_EXPORT void ArkeSetFrameTime(double time) {
    (void)time;
}

ARKE_EXPORT void ArkeReleaseFrameTime(void) {
}

ARKE_EXPORT void ArkeSetFrameCounter(int32_t counter) {
    (void)counter;
}

ARKE_EXPORT void ArkeReleaseFrameCounter(void) {
}

ARKE_EXPORT void ArkeRunFrameCounter(void) {
}

ARKE_EXPORT void ArkeRenderedFrameCounter(void) {
}

} // extern "C"

// --- Lifecycle & Initialization Logic ---

namespace {

static std::atomic<bool> g_arkeInitialized{false};

void initialize_arke() {
    bool expected = false;
    if (!g_arkeInitialized.compare_exchange_strong(expected, true)) {
        return; // Already initialized
    }

#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_INFO, "Arke", "=== Arke-Android (v0.1.1) Initializing on Android 16 ARM64 ===");
#endif

    // Initialize logger with primary and fallback directories
    const std::vector<std::string> logCandidates = {
        "/sdcard/games/com.mojang/mods/arke/logs",
        "/sdcard/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke/logs",
        "/sdcard/Download",
        "/data/data/com.mojang.minecraftpe/files"
    };

    for (const auto& dir : logCandidates) {
        arke::Logger::instance().init(dir);
        if (arke::Logger::instance().isInitialized()) {
            break;
        }
    }

    arke::Logger::instance().info("=== Arke-Android Core Startup ===");

    // Load configuration with fallback candidates
    const std::vector<std::string> configCandidates = {
        "/sdcard/games/com.mojang/mods/arke/config.json",
        "/sdcard/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke/config.json",
        "/sdcard/Download/arke_config.json"
    };

    for (const auto& path : configCandidates) {
        if (arke::ConfigManager::instance().load(path)) {
            break;
        }
    }

    // Initialize Android safe PLT/GOT hooking layer
    arke::platform::AndroidHookLayer::instance().initialize();

    // Initialize Dear ImGui overlay context
    arke::platform::ImGuiOverlay::instance().init();

    // Check if initial shader pack is defined
    const auto& conf = arke::ConfigManager::instance().get();
    if (conf.enabled && !conf.shaderpack.empty()) {
        const std::vector<std::string> packSearchDirs = {
            "/sdcard/games/com.mojang/mods/arke/shaderpacks/",
            "/sdcard/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke/shaderpacks/",
            "/sdcard/Download/shaderpacks/"
        };
        for (const auto& searchDir : packSearchDirs) {
            std::string fullPackPath = searchDir + conf.shaderpack;
            if (arke::shader::ShaderPipeline::instance().loadPack(fullPackPath)) {
                arke::Logger::instance().info("Loaded default shader pack from: " + fullPackPath);
                break;
            }
        }
    }

    arke::Logger::instance().info("=== Arke-Android Initialization Complete ===");
}

} // anonymous namespace

// --- Automatic Constructor Entry Point (for DT_NEEDED injection) ---

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void arke_ctor_entry() {
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_INFO, "Arke", "arke_ctor_entry: spawned DT_NEEDED background initializer");
#endif
    // Spawn detached worker thread so linker64 is never blocked during dlopen/DT_NEEDED loading
    std::thread initThread([]() {
        // Sleep 5 full seconds so libminecraftpe.so and Mali-G57 GPU driver (libGLES_mali.so) complete startup
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Validate that libminecraftpe.so is mapped in /proc/self/maps before touching GOT
        bool mcpeMapped = false;
        for (int retry = 0; retry < 15; retry++) {
            FILE* maps = fopen("/proc/self/maps", "r");
            if (maps) {
                char line[512];
                while (fgets(line, sizeof(line), maps)) {
                    if (strstr(line, "libminecraftpe.so")) {
                        mcpeMapped = true;
                        break;
                    }
                }
                fclose(maps);
            }

            if (mcpeMapped) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }

        initialize_arke();
    });
    initThread.detach();
}
#endif

// --- JNI Entry Points (for Mod Loader invocation) ---

extern "C" {

#if defined(__ANDROID__)

ARKE_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)vm;
    (void)reserved;
    initialize_arke();
    return JNI_VERSION_1_6;
}

ARKE_EXPORT void JNI_OnUnload(JavaVM* vm, void* reserved) {
    (void)vm;
    (void)reserved;
    arke::Logger::instance().info("=== Arke-Android unloading ===");
    arke::platform::AndroidHookLayer::instance().shutdown();
    arke::platform::ImGuiOverlay::instance().shutdown();
}

#endif

} // extern "C"
