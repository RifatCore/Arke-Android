#pragma once

#include <cstdint>

#if defined(_WIN32)
  #define ARKE_EXPORT __declspec(dllexport)
#else
  #define ARKE_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Reconstructed public exports from Arke.dll
ARKE_EXPORT void ArkeReload(void);
ARKE_EXPORT int32_t ArkeState(void);
ARKE_EXPORT void ArkeTerrainChunks(void);
ARKE_EXPORT void ArkeSetFrameClock(double clock);
ARKE_EXPORT void ArkeReleaseFrameClock(void);
ARKE_EXPORT void ArkeSetFrameTime(double time);
ARKE_EXPORT void ArkeReleaseFrameTime(void);
ARKE_EXPORT void ArkeSetFrameCounter(int32_t counter);
ARKE_EXPORT void ArkeReleaseFrameCounter(void);
ARKE_EXPORT void ArkeRunFrameCounter(void);
ARKE_EXPORT void ArkeRenderedFrameCounter(void);

#if defined(__ANDROID__)
#include <jni.h>
ARKE_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved);
ARKE_EXPORT void JNI_OnUnload(JavaVM* vm, void* reserved);
#endif

#ifdef __cplusplus
}
#endif
