#pragma once

#include <cstdint>
#include <atomic>

namespace arke::platform {

struct ArkeShaderState {
    std::atomic<bool> shaderReloadRequested{true};
    bool shadersEnabled{true};
    int presetMode{0}; // 0 = Vibrant ACES Filmic, 1 = Warm Golden Hour & Soft Shadows, 2 = Cinematic High-Contrast & Bloom, 3 = Custom Shader Pack from Disk
    float saturation{1.35f};    // range 0.5f .. 2.5f
    float contrast{1.18f};      // range 0.8f .. 1.8f
    float exposure{1.12f};      // range 0.5f .. 2.0f
    float bloomStrength{0.35f}; // range 0.0f .. 1.0f
    float warmth{0.25f};        // range -0.5f .. 0.8f

    // Custom Shader Pack toggles (MakeUp-UltraFast & disk packs)
    bool customGodrays{true};
    bool customWaterReflect{true};
    bool customDynamicShadows{true};
    bool customBloom{true};
    bool customChromaAber{false};
    bool customDof{false};

    void applyPreset(int mode) {
        presetMode = mode;
        if (mode == 0) { // Vibrant ACES Filmic
            saturation = 1.35f;
            contrast = 1.18f;
            exposure = 1.12f;
            bloomStrength = 0.35f;
            warmth = 0.25f;
        } else if (mode == 1) { // Warm Golden Hour & Soft Shadows
            saturation = 1.45f;
            contrast = 1.25f;
            exposure = 1.05f;
            bloomStrength = 0.45f;
            warmth = 0.55f;
        } else if (mode == 2) { // Cinematic High-Contrast & Bloom
            saturation = 1.20f;
            contrast = 1.40f;
            exposure = 1.15f;
            bloomStrength = 0.60f;
            warmth = 0.05f;
        }
        shaderReloadRequested.store(true);
    }
};

ArkeShaderState& getArkeShaderState();

// EGL and GLES hook callbacks
void onEglSwapBuffersPost(void* dpy = nullptr, void* surface = nullptr);
void onGlShaderSourcePre(uint32_t shader, int32_t count, const char* const* string, const int32_t* length, const char** outCustomShader);
void onGlCompileShaderPost(uint32_t shader);
void onGlGetUniformLocationPost(uint32_t program, const char* name, int32_t location);
void onGlUniformMatrix4fvPre(int32_t location, int32_t count, uint8_t transpose, const float* value);
void onGlUniform4fvPre(int32_t location, int32_t count, const float* value);

} // namespace arke::platform
