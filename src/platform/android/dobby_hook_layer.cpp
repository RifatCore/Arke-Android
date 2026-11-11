#include "arke/platform/android_hook.hpp"
#include "arke/platform/gles_hook.hpp"
#include "arke/platform/vulkan_hook.hpp"
#include "arke/logger.hpp"
#include "dobby/dobby.h"
#include "shadowhook/shadowhook.h"
#include <dlfcn.h>

namespace arke::platform {

// --- Hook trampolines & function pointers ---
static void* (*orig_eglSwapBuffers)(void* dpy, void* surface) = nullptr;
static void (*orig_glShaderSource)(uint32_t shader, int32_t count, const char* const* string, const int32_t* length) = nullptr;
static void (*orig_glCompileShader)(uint32_t shader) = nullptr;
static int32_t (*orig_glGetUniformLocation)(uint32_t program, const char* name) = nullptr;
static void (*orig_glUniformMatrix4fv)(int32_t location, int32_t count, uint8_t transpose, const float* value) = nullptr;
static void (*orig_glUniform4fv)(int32_t location, int32_t count, const float* value) = nullptr;

static void* hook_eglSwapBuffers(void* dpy, void* surface) {
    onEglSwapBuffersPost();
    if (orig_eglSwapBuffers) {
        return orig_eglSwapBuffers(dpy, surface);
    }
    return nullptr;
}

static void hook_glShaderSource(uint32_t shader, int32_t count, const char* const* string, const int32_t* length) {
    const char* customShader = nullptr;
    onGlShaderSourcePre(shader, count, string, length, &customShader);
    if (customShader != nullptr) {
        const char* const customStrings[] = { customShader };
        if (orig_glShaderSource) {
            orig_glShaderSource(shader, 1, customStrings, nullptr);
            return;
        }
    }
    if (orig_glShaderSource) {
        orig_glShaderSource(shader, count, string, length);
    }
}

static void hook_glCompileShader(uint32_t shader) {
    if (orig_glCompileShader) {
        orig_glCompileShader(shader);
    }
    onGlCompileShaderPost(shader);
}

static int32_t hook_glGetUniformLocation(uint32_t program, const char* name) {
    int32_t loc = -1;
    if (orig_glGetUniformLocation) {
        loc = orig_glGetUniformLocation(program, name);
    }
    if (loc >= 0 && name) {
        onGlGetUniformLocationPost(program, name, loc);
    }
    return loc;
}

static void hook_glUniformMatrix4fv(int32_t location, int32_t count, uint8_t transpose, const float* value) {
    onGlUniformMatrix4fvPre(location, count, transpose, value);
    if (orig_glUniformMatrix4fv) {
        orig_glUniformMatrix4fv(location, count, transpose, value);
    }
}

static void hook_glUniform4fv(int32_t location, int32_t count, const float* value) {
    onGlUniform4fvPre(location, count, value);
    if (orig_glUniform4fv) {
        orig_glUniform4fv(location, count, value);
    }
}

AndroidHookLayer& AndroidHookLayer::instance() {
    static AndroidHookLayer s_instance;
    return s_instance;
}

void* AndroidHookLayer::resolveSymbol(const char* libName, const char* symName) {
    void* handle = dlopen(libName, RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) {
        handle = dlopen(libName, RTLD_LAZY);
    }
    if (!handle) return nullptr;
    return dlsym(handle, symName);
}

bool AndroidHookLayer::hookFunction(void* target, void* replacement, void** original) {
    if (!target || !replacement) return false;
    int ret = DobbyHook(target, replacement, original);
    if (ret == 0) return true;

    // Fallback to ShadowHook
    void* stub = shadowhook_hook_func_addr(target, replacement, original);
    return (stub != nullptr);
}

bool AndroidHookLayer::hookEGL() {
    void* eglSwap = resolveSymbol("libEGL.so", "eglSwapBuffers");
    if (!eglSwap) {
        Logger::instance().warn("Failed to locate eglSwapBuffers in libEGL.so");
        return false;
    }

    bool success = hookFunction(eglSwap, (void*)hook_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
    if (success) {
        Logger::instance().info("Hooked eglSwapBuffers in libEGL.so successfully!");
    } else {
        Logger::instance().error("Failed to install hook on eglSwapBuffers");
    }
    return success;
}

bool AndroidHookLayer::hookGLES() {
    void* glSS = resolveSymbol("libGLESv3.so", "glShaderSource");
    void* glCS = resolveSymbol("libGLESv3.so", "glCompileShader");
    void* glGUL = resolveSymbol("libGLESv3.so", "glGetUniformLocation");
    void* glUM4 = resolveSymbol("libGLESv3.so", "glUniformMatrix4fv");
    void* glU4 = resolveSymbol("libGLESv3.so", "glUniform4fv");

    bool s1 = false, s2 = false, s3 = false, s4 = false, s5 = false;
    if (glSS) s1 = hookFunction(glSS, (void*)hook_glShaderSource, (void**)&orig_glShaderSource);
    if (glCS) s2 = hookFunction(glCS, (void*)hook_glCompileShader, (void**)&orig_glCompileShader);
    if (glGUL) s3 = hookFunction(glGUL, (void*)hook_glGetUniformLocation, (void**)&orig_glGetUniformLocation);
    if (glUM4) s4 = hookFunction(glUM4, (void*)hook_glUniformMatrix4fv, (void**)&orig_glUniformMatrix4fv);
    if (glU4) s5 = hookFunction(glU4, (void*)hook_glUniform4fv, (void**)&orig_glUniform4fv);

    Logger::instance().info("Hooked libGLESv3 exports: glShaderSource(" + std::string(s1 ? "OK" : "FAIL") +
                           ") glCompileShader(" + std::string(s2 ? "OK" : "FAIL") +
                           ") glUniform(" + std::string(s4 ? "OK" : "FAIL") + ")");
    return s1 || s2 || s4;
}

bool AndroidHookLayer::hookVulkan() {
    // Scaffolding for devices utilizing Vulkan RenderDragon backend
    void* vkPresent = resolveSymbol("libvulkan.so", "vkQueuePresentKHR");
    if (vkPresent) {
        Logger::instance().info("Vulkan runtime detected on Android host.");
    }
    return true;
}

bool AndroidHookLayer::hookRenderDragon() {
    // Intercept libminecraftpe.so RenderDragon internal hooks & PLT imports
    void* mcHandle = dlopen("libminecraftpe.so", RTLD_LAZY | RTLD_NOLOAD);
    if (mcHandle) {
        Logger::instance().info("Bound to target process: libminecraftpe.so");
    }

    // Hook libminecraftpe.so GOT entries via ShadowHook
    shadowhook_hook_sym_name("libminecraftpe.so", "eglSwapBuffers", (void*)hook_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
    shadowhook_hook_sym_name("libminecraftpe.so", "glShaderSource", (void*)hook_glShaderSource, (void**)&orig_glShaderSource);
    shadowhook_hook_sym_name("libminecraftpe.so", "glCompileShader", (void*)hook_glCompileShader, (void**)&orig_glCompileShader);
    shadowhook_hook_sym_name("libminecraftpe.so", "glGetUniformLocation", (void*)hook_glGetUniformLocation, (void**)&orig_glGetUniformLocation);
    shadowhook_hook_sym_name("libminecraftpe.so", "glUniformMatrix4fv", (void*)hook_glUniformMatrix4fv, (void**)&orig_glUniformMatrix4fv);
    shadowhook_hook_sym_name("libminecraftpe.so", "glUniform4fv", (void*)hook_glUniform4fv, (void**)&orig_glUniform4fv);

    Logger::instance().info("Installed RenderDragon GOT hooks on libminecraftpe.so (26.60)");
    return true;
}

bool AndroidHookLayer::initialize() {
    if (m_hooked) return true;
    Logger::instance().info("Initializing Android ARM64 Hook Layer...");

    bool eglOk = hookEGL();
    bool glesOk = hookGLES();
    hookVulkan();
    hookRenderDragon();

    m_hooked = eglOk || glesOk;
    Logger::instance().info("Android Hook Layer status: " + std::string(m_hooked ? "ACTIVE" : "INACTIVE"));
    return m_hooked;
}

void AndroidHookLayer::shutdown() {
    m_hooked = false;
    Logger::instance().info("Shut down Android ARM64 Hook Layer.");
}

} // namespace arke::platform
