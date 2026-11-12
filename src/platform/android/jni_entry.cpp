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
#include <sys/stat.h>

#if defined(__ANDROID__)
#include <jni.h>
#include <android/log.h>
#endif

// --- Exported C ABI Functions (Identical to Arke.dll) ---

extern "C" {

ARKE_EXPORT void ArkeReload(void) {
    arke::Logger::instance().info("[API] ArkeReload called from host/UI.");
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

static void posix_mkdir_p(const std::string& path) {
    std::string current;
    for (size_t i = 0; i < path.size(); ++i) {
        current += path[i];
        if (path[i] == '/' || i == path.size() - 1) {
            if (current != "/" && !current.empty()) {
                struct stat st;
                if (stat(current.c_str(), &st) != 0) {
                    mkdir(current.c_str(), 0777);
                }
            }
        }
    }
}

static void ensure_default_assets(const std::string& baseDir) {
    posix_mkdir_p(baseDir + "/logs");
    posix_mkdir_p(baseDir + "/shaderpacks/Arke_Vibrant_Default/shaders");

    // 1. Ensure config.json
    std::string configPath = baseDir + "/config.json";
    if (access(configPath.c_str(), F_OK) != 0) {
        FILE* fp = fopen(configPath.c_str(), "w");
        if (fp) {
            fprintf(fp, "{\n"
                        "  \"enabled\": true,\n"
                        "  \"shaderpack\": \"Arke_Vibrant_Default\",\n"
                        "  \"strictMath\": false,\n"
                        "  \"shadowDistance\": 128.0,\n"
                        "  \"perfStats\": true,\n"
                        "  \"colorSpace\": 0,\n"
                        "  \"graphicsDebug\": false\n"
                        "}\n");
            fclose(fp);
            arke::Logger::instance().info("[Setup] Created default config.json at " + configPath);
        }
    }

    // 2. Ensure built-in default shader preset: Arke_Vibrant_Default
    std::string shadersDir = baseDir + "/shaderpacks/Arke_Vibrant_Default/shaders";

    std::string propsPath = shadersDir + "/shaders.properties";
    if (access(propsPath.c_str(), F_OK) != 0) {
        FILE* fp = fopen(propsPath.c_str(), "w");
        if (fp) {
            fprintf(fp, "# Arke Vibrant Default Shader Pack (Mali-G57 GLES 3.2)\n"
                        "version=1.0.0\n"
                        "author=Arke-Team\n"
                        "profile.default=HIGH\n"
                        "sliders=SHADOW_DISTANCE BLOOM_INTENSITY SATURATION_BOOST\n"
                        "slider.SHADOW_DISTANCE=128.0\n"
                        "slider.BLOOM_INTENSITY=1.25\n"
                        "slider.SATURATION_BOOST=1.22\n");
            fclose(fp);
            arke::Logger::instance().info("[Setup] Created default shaders.properties");
        }
    }

    std::string vshPath = shadersDir + "/final.vsh";
    if (access(vshPath.c_str(), F_OK) != 0) {
        FILE* fp = fopen(vshPath.c_str(), "w");
        if (fp) {
            fprintf(fp, "#version 300 es\n"
                        "precision highp float;\n"
                        "layout(location = 0) in vec2 a_position;\n"
                        "layout(location = 1) in vec2 a_texcoord;\n"
                        "out vec2 texCoord;\n"
                        "void main() {\n"
                        "    texCoord = a_texcoord;\n"
                        "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                        "}\n");
            fclose(fp);
            arke::Logger::instance().info("[Setup] Created default final.vsh");
        }
    }

    std::string fshPath = shadersDir + "/final.fsh";
    if (access(fshPath.c_str(), F_OK) != 0) {
        FILE* fp = fopen(fshPath.c_str(), "w");
        if (fp) {
            fprintf(fp, "#version 300 es\n"
                        "precision highp float;\n"
                        "in vec2 texCoord;\n"
                        "out vec4 fragColor;\n"
                        "uniform sampler2D colortex0;\n"
                        "uniform float viewWidth;\n"
                        "uniform float viewHeight;\n"
                        "uniform float frameTimeCounter;\n"
                        "void main() {\n"
                        "    vec4 color = texture(colortex0, texCoord);\n"
                        "    vec3 rgb = color.rgb;\n"
                        "    float luma = dot(rgb, vec3(0.2126, 0.7152, 0.0722));\n"
                        "    rgb = mix(vec3(luma), rgb, 1.22);\n"
                        "    rgb = pow(rgb, vec3(0.96));\n"
                        "    rgb = (rgb - 0.5) * 1.06 + 0.5;\n"
                        "    rgb += vec3(0.015, 0.008, 0.0) * luma;\n"
                        "    fragColor = vec4(clamp(rgb, 0.0, 1.0), color.a);\n"
                        "}\n");
            fclose(fp);
            arke::Logger::instance().info("[Setup] Created default final.fsh");
        }
    }

    std::string compPath = shadersDir + "/composite.fsh";
    if (access(compPath.c_str(), F_OK) != 0) {
        FILE* fp = fopen(compPath.c_str(), "w");
        if (fp) {
            fprintf(fp, "#version 300 es\n"
                        "precision highp float;\n"
                        "in vec2 texCoord;\n"
                        "out vec4 fragColor;\n"
                        "uniform sampler2D colortex0;\n"
                        "void main() {\n"
                        "    fragColor = texture(colortex0, texCoord);\n"
                        "}\n");
            fclose(fp);
            arke::Logger::instance().info("[Setup] Created default composite.fsh");
        }
    }
}

void initialize_arke() {
    bool expected = false;
    if (!g_arkeInitialized.compare_exchange_strong(expected, true)) {
        return; // Already initialized
    }

#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_INFO, "Arke", "=== Arke-Android (v0.1.1) Phase 8 Initializing on Android 16 ARM64 ===");
#endif

    // Initialize logger with primary and fallback directories
    const std::vector<std::string> logCandidates = {
        "/storage/emulated/0/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke/logs",
        "/sdcard/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke/logs",
        "/sdcard/games/com.mojang/mods/arke/logs",
        "/sdcard/Download",
        "/data/data/com.mojang.minecraftpe/files"
    };

    for (const auto& dir : logCandidates) {
        arke::Logger::instance().init(dir);
        if (arke::Logger::instance().isInitialized()) {
            break;
        }
    }

    arke::Logger::instance().info("=== Arke-Android Phase 8 Core Startup ===");

    // Auto-create default directories, config.json and Arke_Vibrant_Default preset
    const std::vector<std::string> baseCandidates = {
        "/storage/emulated/0/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke",
        "/sdcard/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke",
        "/sdcard/games/com.mojang/mods/arke",
        "/sdcard/Download/arke"
    };

    for (const auto& base : baseCandidates) {
        ensure_default_assets(base);
    }

    // Load configuration
    const std::vector<std::string> configCandidates = {
        "/storage/emulated/0/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke/config.json",
        "/sdcard/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke/config.json",
        "/sdcard/games/com.mojang/mods/arke/config.json",
        "/sdcard/Download/arke_config.json"
    };

    for (const auto& path : configCandidates) {
        if (arke::ConfigManager::instance().load(path)) {
            break;
        }
    }

    // Initialize Android safe PLT/GOT hooking layer (graphics & touch input)
    arke::platform::AndroidHookLayer::instance().initialize();

    // Initialize Dear ImGui overlay context
    arke::platform::ImGuiOverlay::instance().init();

    // Load active shader pack
    const auto& conf = arke::ConfigManager::instance().get();
    if (conf.enabled && !conf.shaderpack.empty()) {
        const std::vector<std::string> packSearchDirs = {
            "/storage/emulated/0/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke/shaderpacks/",
            "/sdcard/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke/shaderpacks/",
            "/sdcard/games/com.mojang/mods/arke/shaderpacks/",
            "/sdcard/Download/shaderpacks/",
            "/storage/emulated/0/Download/shaderpacks/"
        };
        for (const auto& searchDir : packSearchDirs) {
            std::string fullPackPath = searchDir + conf.shaderpack;
            if (access(fullPackPath.c_str(), F_OK) == 0) {
                if (arke::shader::ShaderPipeline::instance().loadPack(fullPackPath)) {
                    arke::Logger::instance().info("[Init] Loaded active shader pack from: " + fullPackPath);
                    break;
                }
            }
        }
    }

    arke::Logger::instance().info("=== Arke-Android Phase 8 Initialization Complete ===");
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
