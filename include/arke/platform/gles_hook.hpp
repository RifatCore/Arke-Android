#pragma once

#include <cstdint>

namespace arke::platform {

// EGL and GLES hook callbacks
void onEglSwapBuffersPost();
void onGlShaderSourcePre(uint32_t shader, int32_t count, const char* const* string, const int32_t* length, const char** outCustomShader);
void onGlCompileShaderPost(uint32_t shader);

} // namespace arke::platform
