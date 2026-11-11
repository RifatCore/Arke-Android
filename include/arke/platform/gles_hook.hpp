#pragma once

#include <cstdint>

namespace arke::platform {

// EGL and GLES hook callbacks
void onEglSwapBuffersPost();
void onGlShaderSourcePre(uint32_t shader, int32_t count, const char* const* string, const int32_t* length, const char** outCustomShader);
void onGlCompileShaderPost(uint32_t shader);
void onGlGetUniformLocationPost(uint32_t program, const char* name, int32_t location);
void onGlUniformMatrix4fvPre(int32_t location, int32_t count, uint8_t transpose, const float* value);
void onGlUniform4fvPre(int32_t location, int32_t count, const float* value);

} // namespace arke::platform
