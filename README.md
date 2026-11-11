# Arke-Android: Next-Generation Shader Runtime for Minecraft Bedrock (Android ARM64)

<div align="center">
  <h3>Bringing Java Edition (Iris & OptiFine) Shaders to Minecraft Bedrock Android (arm64-v8a)</h3>
  <p><b>Port & Complete Reconstruction of <a href="https://github.com/LiteLDev/Arke">LiteLDev/Arke</a> (v0.1.1)</b></p>
</div>

---

## 🌟 Overview

**Arke-Android** is a clean, modular C++20 port and architectural reconstruction of **LiteLDev/Arke** for **Minecraft Bedrock Edition on Android (`arm64-v8a`)**.

Upstream Arke runs Java Edition shader packs on Windows using Direct3D 12 and `dxcompiler.dll`. **Arke-Android** replaces the DirectX 12 pipeline with a mobile GPU backend targeting **RenderDragon / BGFX** on **OpenGL ES 3.2 (ESSL `#version 320 es`)** and **Vulkan 1.2+ SPIR-V**, natively accelerating shader packs with zero performance loss on Qualcomm Adreno and ARM Mali GPUs.

For the comprehensive binary analysis of the upstream Windows release (`arke.dll`), see:
📄 **[REA Reverse Engineering Evidence Report](docs/REA_REVERSE_REPORT.md)**.

---

## 🏗️ Architecture & Pipeline Mapping

```
      Java Edition Shader Pack (.zip / folder)
                         │
                         ▼
        Iris / OptiFine shaders.properties
        (Custom textures, buffer formats, dimensions)
                         │
                         ▼
             JCPP Preprocessor Wrapper
     Injects #include, IS_ARKE, ARKE_VERSION 100,
             Iris MC_* & Geometry defines
                         │
                         ▼
             Cross-Compilation Engine
       [glslang] -> [SPIRV-Tools] -> [SPIRV-Cross]
                         │
        ┌────────────────┴────────────────┐
        ▼                                 ▼
   OpenGL ES 3.2 ESSL               Vulkan 1.2+
   (#version 320 es)              (SPIR-V Bytecode)
        │                                 │
        ▼                                 ▼
   Adreno / Mali GPU             Adreno / Mali GPU
                         │
                         ▼
       Android ARM64 Hook Layer (Dobby / ShadowHook)
  Intercepts libminecraftpe.so & libEGL.so (eglSwapBuffers, glShaderSource)
                         │
                         ▼
     Dear ImGui In-Game Touch Overlay (##arke_launcher)
```

---

## 🔧 Subsystem Comparison: Windows x64 vs. Android arm64-v8a

| Subsystem | Upstream Arke (`Arke.dll`) | Arke-Android (`libarke_android.so`) |
| :--- | :--- | :--- |
| **Target OS / Arch** | Windows 10/11 (`x86_64`) | Android 8.0+ (`arm64-v8a`) |
| **Host Binary** | `Minecraft.Windows.exe` | `libminecraftpe.so` (RenderDragon/BGFX) |
| **Graphics API** | Direct3D 12 Feature Level 12_0 | OpenGL ES 3.2 / Vulkan 1.2+ |
| **Shader Backend** | DXIL via `dxcompiler.dll` | ESSL 3.2 (`#version 320 es`) via `SPIRV-Cross` |
| **Hook Mechanism** | `ll::memory::hookEx` / MinHook | **Dobby** (`DobbyHook`) & **ShadowHook** |
| **Present Hook** | `IDXGISwapChain::Present` | `eglSwapBuffers` (libEGL.so) / `vkQueuePresentKHR` |
| **UI Framework** | Dear ImGui (`imgui_impl_dx12`) | Dear ImGui (`imgui_impl_opengl3` + `imgui_impl_android`) |
| **Input Routing** | Win32 `WndProc` / `HIDController` | Android Touch Events (`ANativeWindow` touch dispatch) |
| **Mod Loader Entry** | `ll_mod_load` (LeviLamina Windows) | `JNI_OnLoad` (Android mod loaders / Zygisk / LeviLamina Android) |

---

## 📂 Repository Structure

```
Arke-Android/
├── CMakeLists.txt                # CMake build script supporting Android NDK
├── xmake.lua                     # xmake project configuration
├── build_android.sh              # Local Android ARM64 compilation script
├── README.md                     # Project documentation
├── docs/
│   └── REA_REVERSE_REPORT.md     # Full Reverse Engineering & RTTI report
├── .github/workflows/
│   └── android-build.yml         # CI workflow compiling libarke_android.so
├── include/arke/
│   ├── api.hpp                   # Public exported C/C++ API (ArkeReload, ArkeState)
│   ├── config.hpp                # config.json manager
│   ├── logger.hpp                # Logger targeting /mods/arke/logs/arke.log and logcat
│   ├── types.hpp                 # Uniform buffer structs (ArkeFrame, ArkePass, ArkeMeshDraw)
│   ├── uniform_bridge.hpp        # RenderDragon -> OptiFine/Iris uniform bridge
│   ├── shader/
│   │   ├── properties_parser.hpp # shaders.properties & dimension.properties parser
│   │   ├── preprocessor.hpp      # JCPP preprocessor with IS_ARKE macro injection
│   │   ├── compiler.hpp          # CrossCompiler emitting ESSL 3.2 & Vulkan SPIR-V
│   │   └── pipeline.hpp          # Multi-pass shader pack orchestration pipeline
│   └── platform/
│       ├── android_hook.hpp      # Dobby / ShadowHook inline hook layer
│       ├── gles_hook.hpp         # eglSwapBuffers & glShaderSource hooks
│       ├── vulkan_hook.hpp       # Vulkan pipeline hooks
│       └── imgui_overlay.hpp     # Android touch-enabled Dear ImGui launcher
├── src/
│   ├── core/                     # Platform-independent core
│   ├── shader/                   # Shader parsing, preprocessing & translation
│   └── platform/android/         # Android JNI and graphics hooking
├── third_party/                  # Bundled third-party interfaces (Dobby, ShadowHook, ImGui)
└── tests/
    └── test_shader_transpile.cpp # Unit tests verifying parser, preprocessor & compiler
```

---

## 🚀 Building & Packaging

### Option 1: Using CMake with Android NDK
```bash
export ANDROID_NDK_ROOT=/path/to/android-ndk-r26d

cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-26 \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release -j$(nproc)
```

The resulting library will be located at:
`build/libarke_android.so`

### Option 2: Using the automated build script
```bash
./build_android.sh
```

### Option 3: Using xmake
```bash
xmake f -p android -a arm64-v8a -m release
xmake
```

---

## 🎮 Deployment on Android Device

1. Copy `libarke_android.so` to your mod loader library folder (e.g. LeviLamina Android mod folder or inject via Zygisk/root).
2. Place your desired Iris/OptiFine shader packs in:
   `/sdcard/Android/data/com.mojang.minecraftpe/files/games/com.mojang/mods/arke/shaderpacks/`
3. Launch Minecraft Bedrock Edition.
4. Tap the **`Arke`** button in the upper-left corner of the screen to select shader packs, adjust options, or inspect logs.

---

## 📜 License

This reconstruction project is licensed under Apache-2.0 or MIT, respecting upstream component licenses (glslang, SPIRV-Cross, Dear ImGui, LeviLamina).
