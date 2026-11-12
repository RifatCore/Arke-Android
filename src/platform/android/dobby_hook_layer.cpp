#include "arke/platform/android_hook.hpp"
#include "arke/platform/gles_hook.hpp"
#include "arke/platform/vulkan_hook.hpp"
#include "arke/platform/imgui_overlay.hpp"
#include "arke/logger.hpp"
#include "dobby/dobby.h"
#include "shadowhook/shadowhook.h"

#include <android/input.h>
#include <dlfcn.h>
#include <cstring>
#include <string>

namespace arke::platform {

// --- Android AGDK GameActivity Structures ---
struct GameActivityPointerAxes {
    int32_t id;
    float axisValues[48];
    float rawX;
    float rawY;
};

struct GameActivityMotionEvent {
    int32_t deviceId;
    int32_t source;
    int32_t action;
    int64_t eventTime;
    int64_t downTime;
    int32_t flags;
    int32_t metaState;
    int32_t buttonState;
    int32_t classification;
    int32_t edgeFlags;
    uint32_t precisionX;
    uint32_t precisionY;
    uint32_t pointerCount;
    GameActivityPointerAxes pointers[8];
};

struct android_input_buffer {
    GameActivityMotionEvent* motionEvents;
    uint64_t motionEventsCount;
    uint64_t motionEventsBufferSize;
    void* keyEvents;
    uint64_t keyEventsCount;
    uint64_t keyEventsBufferSize;
};

// --- Hook trampolines & function pointers ---
static void* (*orig_eglSwapBuffers)(void* dpy, void* surface) = nullptr;
static void (*orig_glShaderSource)(uint32_t shader, int32_t count, const char* const* string, const int32_t* length) = nullptr;
static void (*orig_glCompileShader)(uint32_t shader) = nullptr;
static int32_t (*orig_glGetUniformLocation)(uint32_t program, const char* name) = nullptr;
static void (*orig_glUniformMatrix4fv)(int32_t location, int32_t count, uint8_t transpose, const float* value) = nullptr;
static void (*orig_glUniform4fv)(int32_t location, int32_t count, const float* value) = nullptr;

// Input hooks
static int32_t (*orig_AInputQueue_getEvent)(AInputQueue* queue, AInputEvent** outEvent) = nullptr;
static void (*orig_AInputQueue_finishEvent)(AInputQueue* queue, AInputEvent* event, int handled) = nullptr;
static struct android_input_buffer* (*orig_android_app_swap_input_buffers)(void* app) = nullptr;

static void* hook_eglSwapBuffers(void* dpy, void* surface) {
    onEglSwapBuffersPost(dpy, surface);
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

// 1. Hook AInputQueue_getEvent (for NDK NativeActivity / AInputQueue consumers)
static int32_t hook_AInputQueue_getEvent(AInputQueue* queue, AInputEvent** outEvent) {
    while (true) {
        int32_t res = -1;
        if (orig_AInputQueue_getEvent) {
            res = orig_AInputQueue_getEvent(queue, outEvent);
        } else {
            res = AInputQueue_getEvent(queue, outEvent);
        }

        if (res != 0 || !outEvent || !*outEvent) {
            return res;
        }

        AInputEvent* event = *outEvent;
        int32_t eventType = AInputEvent_getType(event);
        if (eventType == AINPUT_EVENT_TYPE_MOTION) {
            int32_t action = AMotionEvent_getAction(event);
            int32_t actionMasked = action & AMOTION_EVENT_ACTION_MASK;
            float x = AMotionEvent_getX(event, 0);
            float y = AMotionEvent_getY(event, 0);

            bool consumed = ImGuiOverlay::instance().handleTouchEvent(actionMasked, x, y);
            if (consumed) {
                if (orig_AInputQueue_finishEvent) {
                    orig_AInputQueue_finishEvent(queue, event, 1);
                } else {
                    AInputQueue_finishEvent(queue, event, 1);
                }
                *outEvent = nullptr;
                continue; // Fetch next event
            }
        }
        return res;
    }
}

// 2. Hook android_app_swap_input_buffers (for AGDK GameActivity in Minecraft Bedrock)
static struct android_input_buffer* hook_android_app_swap_input_buffers(void* app) {
    struct android_input_buffer* buf = nullptr;
    if (orig_android_app_swap_input_buffers) {
        buf = orig_android_app_swap_input_buffers(app);
    }
    if (buf && buf->motionEvents && buf->motionEventsCount > 0) {
        uint64_t writeIdx = 0;
        for (uint64_t i = 0; i < buf->motionEventsCount; i++) {
            GameActivityMotionEvent* ev = &buf->motionEvents[i];
            int actionMasked = ev->action & AMOTION_EVENT_ACTION_MASK;
            float x = 0.0f, y = 0.0f;
            if (ev->pointerCount > 0) {
                x = ev->pointers[0].axisValues[0];
                y = ev->pointers[0].axisValues[1];
            }

            bool consumed = ImGuiOverlay::instance().handleTouchEvent(actionMasked, x, y);
            if (!consumed) {
                if (writeIdx != i) {
                    buf->motionEvents[writeIdx] = *ev;
                }
                writeIdx++;
            }
        }
        buf->motionEventsCount = writeIdx;
    }
    return buf;
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
    (void)target;
    (void)replacement;
    (void)original;
    return false;
}

bool AndroidHookLayer::hookEGL() {
    return true;
}

bool AndroidHookLayer::hookGLES() {
    return true;
}

bool AndroidHookLayer::hookVulkan() {
    void* vkPresent = resolveSymbol("libvulkan.so", "vkQueuePresentKHR");
    if (vkPresent) {
        Logger::instance().info("[AndroidHookLayer] Vulkan runtime detected on Android host.");
    }
    return true;
}

bool AndroidHookLayer::hookRenderDragon() {
    void* mcHandle = dlopen("libminecraftpe.so", RTLD_LAZY | RTLD_NOLOAD);
    if (mcHandle) {
        Logger::instance().info("[AndroidHookLayer] Bound to target process: libminecraftpe.so");
    }

    // Hook libminecraftpe.so Graphics GOT entries via ShadowHook
    shadowhook_hook_sym_name("libminecraftpe.so", "eglSwapBuffers", (void*)hook_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
    shadowhook_hook_sym_name("libminecraftpe.so", "glShaderSource", (void*)hook_glShaderSource, (void**)&orig_glShaderSource);
    shadowhook_hook_sym_name("libminecraftpe.so", "glCompileShader", (void*)hook_glCompileShader, (void**)&orig_glCompileShader);
    shadowhook_hook_sym_name("libminecraftpe.so", "glGetUniformLocation", (void*)hook_glGetUniformLocation, (void**)&orig_glGetUniformLocation);
    shadowhook_hook_sym_name("libminecraftpe.so", "glUniformMatrix4fv", (void*)hook_glUniformMatrix4fv, (void**)&orig_glUniformMatrix4fv);
    shadowhook_hook_sym_name("libminecraftpe.so", "glUniform4fv", (void*)hook_glUniform4fv, (void**)&orig_glUniform4fv);

    // Hook Input Dispatch GOT entries (AInputQueue and GameActivity)
    shadowhook_hook_sym_name("libminecraftpe.so", "AInputQueue_getEvent", (void*)hook_AInputQueue_getEvent, (void**)&orig_AInputQueue_getEvent);
    shadowhook_hook_sym_name("libandroid.so", "AInputQueue_getEvent", (void*)hook_AInputQueue_getEvent, (void**)&orig_AInputQueue_getEvent);
    shadowhook_hook_sym_name(NULL, "AInputQueue_getEvent", (void*)hook_AInputQueue_getEvent, (void**)&orig_AInputQueue_getEvent);

    orig_AInputQueue_finishEvent = (void(*)(AInputQueue*, AInputEvent*, int))resolveSymbol("libandroid.so", "AInputQueue_finishEvent");

    shadowhook_hook_sym_name("libminecraftpe.so", "android_app_swap_input_buffers", (void*)hook_android_app_swap_input_buffers, (void**)&orig_android_app_swap_input_buffers);
    if (!orig_android_app_swap_input_buffers && mcHandle) {
        orig_android_app_swap_input_buffers = (struct android_input_buffer*(*)(void*))dlsym(mcHandle, "android_app_swap_input_buffers");
    }

    Logger::instance().info("[AndroidHookLayer] Installed RenderDragon graphics & touch input GOT hooks on libminecraftpe.so");
    return true;
}

bool AndroidHookLayer::initialize() {
    if (m_hooked) return true;
    Logger::instance().info("[AndroidHookLayer] Initializing Android ARM64 Safe PLT/GOT Hook Layer...");

    bool mcpeOk = hookRenderDragon();
    hookVulkan();

    m_hooked = mcpeOk;
    Logger::instance().info("[AndroidHookLayer] Hook layer status: " + std::string(m_hooked ? "ACTIVE" : "INACTIVE"));
    return m_hooked;
}

void AndroidHookLayer::shutdown() {
    m_hooked = false;
    Logger::instance().info("[AndroidHookLayer] Shut down Android ARM64 Hook Layer.");
}

} // namespace arke::platform
