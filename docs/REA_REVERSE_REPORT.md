# Reverse Engineering Evidence & Architectural Reconstruction Report: LiteLDev/Arke (v0.1.1)

## 1. Executive Summary

`LiteLDev/Arke` is a next-generation client-side shader runtime designed for Minecraft Bedrock Edition (specifically built as a LeviLamina native mod on Windows x64). Its core breakthrough is enabling unmodified or minimally adapted Java Edition shader packs—authored for **Iris** and **OptiFine**—to run directly on Minecraft Bedrock's modern **RenderDragon** graphics engine with **zero performance loss**.

This report documents the exhaustive binary analysis of the shipped release binary `Arke.dll` (v0.1.1 / internal 0.1.0, SHA-256 `8b6df43d9cdccfecabb59d77501a77cff4967e461be19568470f2a8d16986f8c`) conducted using the `rea` (Reverse Engineer Anything) suite and native inspection tools (`llvm-objdump`, `readelf`, `llvm-undname`). The report covers archive ingestion, shader preprocessing and macro injection, cross-compilation mechanics, uniform buffer topologies, Direct3D 12 render loop interception, and the architectural translation required for an **Android (`arm64-v8a`)** port targeting RenderDragon / BGFX on OpenGL ES 3.2 and Vulkan.

---

## 2. Binary Metadata & Layout Inspection

### 2.1 File Characteristics
- **Binary Name**: `arke.dll`
- **Container Format**: PE32+ (64-bit AMD64)
- **Machine Type**: `0x8664` (`IMAGE_FILE_MACHINE_AMD64`)
- **File Size**: `11,230,208` bytes (~10.71 MiB)
- **SHA-256 Digest**: `8b6df43d9cdccfecabb59d77501a77cff4967e461be19568470f2a8d16986f8c`
- **Companion DLL**: `dxcompiler.dll` (DirectX Shader Compiler, 22,274,872 bytes)
- **Subsystem**: Windows CUI / GUI (DLL)
- **C++ Standard**: C++20 / C++23 (compiled with MSVC 19.38+, STL ABI 14.38)

### 2.2 PE Section Breakdown
| Section | Raw Offset | Raw Size | Virtual Address (VMA) | Virtual Size | Characteristics |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `.text` | `0x00000400` | `0x007DA400` | `0x00001000` (`0x180001000`) | `0x007DA327` | Code, Executable, Readable |
| `.rdata` | `0x007DA800` | `0x00265800` | `0x007DC000` (`0x1807DC000`) | `0x002657B0` | Read-only Data, RTTI, Embedded HLSL |
| `.data` | `0x00A40000` | `0x00024600` | `0x00A42000` (`0x180A42000`) | `0x0003BBC9` | Initialized Globals, Type Descriptors |
| `.pdata` | `0x00A64600` | `0x00048200` | `0x00A7E000` (`0x180A7E000`) | `0x000481C8` | Exception Handling Tables (x64) |
| `.reloc` | `0x00AAC800` | `0x00009400` | `0x00AC7000` (`0x180AC7000`) | `0x00009320` | Base Relocation Table |

### 2.3 Exported Symbols
The DLL exports 13 public routines providing host lifecycle and timing control:
```c
ArkeReleaseFrameClock    @ 0x180148540 (Ordinal 1)
ArkeReleaseFrameCounter  @ 0x1801485a0 (Ordinal 2)
ArkeReleaseFrameTime     @ 0x180148570 (Ordinal 3)
ArkeReload               @ 0x1801484d0 (Ordinal 4)  // Triggers hot-reload of active shader pack
ArkeRenderedFrameCounter @ 0x1801485c0 (Ordinal 5)
ArkeRunFrameCounter      @ 0x1801485b0 (Ordinal 6)
ArkeSetFrameClock        @ 0x180148520 (Ordinal 7)
ArkeSetFrameCounter      @ 0x180148580 (Ordinal 8)
ArkeSetFrameTime         @ 0x180148550 (Ordinal 9)
ArkeState                @ 0x1801484f0 (Ordinal 10) // Queries current runtime & frontend status
ArkeTerrainChunks        @ 0x180148510 (Ordinal 11)
ll_memory_operator_overrided @ 0x1807f295c (Ordinal 12)
ll_mod_load              @ 0x180147e40 (Ordinal 13) // LeviLamina mod registration entry
```

### 2.4 Import Dependencies
- **LeviLamina Core**: `ll::memory::hookEx`, `ll::mod::Mod::onEnable`, `ll::mod::Mod::onDisable`, `ll::mod::Mod::getModDir`, `ll::io::Logger`, `ll::io::FileSink`.
- **Bedrock Engine Entities & Render Context**: `Actor::getInterpolatedPosition`, `Actor::getDimension`, `BlockSource::getBiome`, `Biome::getTemperature`, `ItemStackBase::getBlockForRendering`, `LevelRendererPlayer::_renderOutlineSelection`, `LightningBolt::$normalTick`, `HIDControllerGameCoreDesktop::$onKeyDown`.
- **Direct3D 12 & DXGI**: `d3d12.dll` (`D3D12SerializeRootSignature`), `dxgi.dll` (`CreateDXGIFactory1`), `D3DCOMPILER_47.dll` (`D3DCompile`).
- **Dynamic Linking**: Dynamically loads `dxcompiler.dll` via `LoadLibraryW` / `GetProcAddress("DxcCreateInstance")`.

---

## 3. Shader Pack Loader & Archive Handling

### 3.1 Architecture Overview
The loader subsystem is organized into distinct classes identified in the binary RTTI and symbol tables:
- `arke::frontend::Frontend`: Root controller singleton managing configuration and UI interaction.
- `arke::frontend::LoadedPack`: State container representing the currently bound shader pack.
- `arke::frontend::PackFiles`: Virtual file system abstraction allowing transparent reading from folders or archives.
- `arke::iris::pack::ShaderPack`: Core parser for Iris/OptiFine shader packages.
- `DirectoryPack` & `ZipPack`: Concretions of `PackFiles`.
  - `ZipPack` uses **miniz** (`mz_zip_*`) to index and stream compressed `.zip` shader packs in memory.
  - `DirectoryPack` reads standard filesystem directory structures from `shaderpacks/<pack_name>/`.
  - Embedded image handling (`CUSTOM_IMAGES`) uses **stb_image** (`stbi_load`) to decode PNG, JPG, and TGA textures.
  - JSON metadata and client state are handled via **nlohmann::json** (ABI v3.12.0) reading/writing `config.json`.

### 3.2 Property File Parsing & Feature Support
The parser searches for the following configuration files:
1. `shaders/shaders.properties` (or root `shaders.properties`)
2. `shaders/dimension.properties`
3. `shaders/entity.properties`
4. Dimension overrides in `shaders/world0/`, `shaders/world-1/` (Nether), and `shaders/world1/` (The End)

The parser extracts key directives:
- **Feature Flags**:
  - `CUSTOM_IMAGES`: Custom pack textures
  - `PER_BUFFER_BLENDING`: Explicit blend state per color attachment
  - `COMPUTE_SHADERS`: Post-processing compute kernels (`.csh`)
  - `TESSELLATION_SHADERS`: Hardware tessellation (`.tcs`, `.tes`)
  - `ENTITY_TRANSLUCENT`: Translucent entity depth sorting
  - `REVERSED_CULLING`: Geometry culling inversions
  - `BLOCK_EMISSION_ATTRIBUTE`: Dynamic per-block light emission values
  - `CAN_DISABLE_WEATHER`: Selective rain/snow disabling
  - `SSBO`: Shader Storage Buffer Object support
- **Color Spaces**:
  - `COLOR_SPACE_SRGB` (0)
  - `COLOR_SPACE_DCI_P3` (1)
  - `COLOR_SPACE_DISPLAY_P3` (2)
  - `COLOR_SPACE_REC2020` (3)
  - `COLOR_SPACE_ADOBE_RGB` (4)
- **Block & Entity ID Mapping**:
  Parses `block.<id> = <namespace>:<block_name>` definitions (e.g. `block.42 = minecraft:iron_bars`, `block.12 = minecraft:glow_frame`) to populate `arke::engine::ContentMap` and `arke::api::TagMap`.

### 3.3 Shader Pass Lifecycle & Stage Identification
Arke classifies shader source files matching Iris / OptiFine naming conventions:
- **Geometry Buffers (`gbuffers_*`)**:
  - `gbuffers_basic`: Untextured color geometry
  - `gbuffers_textured`, `gbuffers_textured_lit`: Standard UI / blocks
  - `gbuffers_terrain`, `gbuffers_terrain_solid`, `gbuffers_terrain_cutout`: Terrain geometry
  - `gbuffers_water`: Translucent water and fluids
  - `gbuffers_entities`, `gbuffers_entities_translucent`, `gbuffers_entities_glowing`: Entity rendering
  - `gbuffers_hand`, `gbuffers_hand_water`: First-person player hands/held items
  - `gbuffers_clouds`, `gbuffers_skybasic`, `gbuffers_skytextured`: Sky dome and atmospheric layers
  - `gbuffers_particles`, `gbuffers_particles_translucent`: Particle effects
  - `gbuffers_weather`: Rain and snow
  - `gbuffers_beaconbeam`, `gbuffers_armor_glint`, `gbuffers_lightning`, `gbuffers_line`, `gbuffers_spidereyes`
- **Shadow Passes (`shadow*`)**:
  - `shadow.vsh`, `shadow.fsh`, `shadow.gsh`, `shadow.csh`
- **Deferred Composite Stages (`deferred*` & `composite*`)**:
  - `deferred`, `deferred1`..`deferred99`
  - `composite`, `composite1`..`composite99`
  - `final.vsh`, `final.fsh`

### 3.4 Framebuffer & Render Target Mapping
OptiFine color and depth attachments are mapped directly:
- `colortex0` through `colortex15` (`RGBA8`, `RGBA16F`, `RGBA32F`, `R11G11B10F`)
- `depthtex0`, `depthtex1`, `depthtex2` (`DEPTH24_STENCIL8` or `DEPTH32F`)
- `shadowcolor0` through `shadowcolor3`
- `shadowtex0`, `shadowtex1` (Shadow depth maps, nearest/linear samplers)

---

## 4. GLSL Preprocessor & Macro Injection

### 4.1 JCPP Preprocessor Architecture
Java Edition shaders heavily utilize C preprocessor syntax with OptiFine `#include` extensions. Arke embeds a C++ port of **JCPP** to perform:
1. Recursive resolution of `#include "/lib/..."` directives.
2. Macro replacement and conditional compilation (`#ifdef`, `#ifndef`, `#if`, `#elif`, `#else`, `#endif`).
3. Mathematical expression evaluation for configuration profiles.

### 4.2 Iris & Minecraft Emulation Macros
The preprocessor automatically injects the canonical Iris runtime environment defines:
```glsl
#define MC_VERSION 12000
#define MC_GL_VERSION 320
#define MC_GLSL_VERSION 320
#define MC_RENDER_STAGE_TERRAIN_SOLID 1
#define MC_RENDER_STAGE_TERRAIN_CUTOUT 2
#define MC_RENDER_STAGE_WATER 3
#define MC_RENDER_STAGE_ENTITIES 4
#define MC_RENDER_STAGE_HAND 5
#define MC_GL_ARB_shader_image_load_store 1
#define MC_GL_ARB_texture_storage 1
```

### 4.3 Arke-Specific Macro Environment
To allow pack authors to handle differences between Java Edition and Bedrock Edition RenderDragon, Arke defines:
```glsl
#define IS_ARKE 1
#define ARKE_VERSION 100
#define ARKE_API 1

// Camera Types
#define ARKE_CAMERA_MAIN 0
#define ARKE_CAMERA_HAND 1
#define ARKE_CAMERA_SHADOW 2

// Dimension IDs
#define ARKE_DIMENSION_OVERWORLD 0
#define ARKE_DIMENSION_NETHER 1
#define ARKE_DIMENSION_END 2

// Geometry Classes
#define ARKE_GEOMETRY_TERRAIN 1
#define ARKE_GEOMETRY_ENTITY 1
#define ARKE_GEOMETRY_BLOCK_ENTITY 1
#define ARKE_GEOMETRY_HAND 1
#define ARKE_GEOMETRY_PARTICLE 1
#define ARKE_GEOMETRY_SKY 1
#define ARKE_GEOMETRY_MESH 1

// Material Categories
#define ARKE_MATERIAL_PLAIN 0u
#define ARKE_MATERIAL_MULTITEXTURE 1u
#define ARKE_MATERIAL_TINT 2u
#define ARKE_MATERIAL_BANNER_LAYER 3u

// Submerged Medium
#define ARKE_MEDIUM_AIR 0
#define ARKE_MEDIUM_WATER 1
#define ARKE_MEDIUM_LAVA 2
#define ARKE_MEDIUM_POWDER_SNOW 3

// Player State Bitflags
#define ARKE_PLAYER_ON_GROUND 1u
#define ARKE_PLAYER_SNEAKING 2u
#define ARKE_PLAYER_SPRINTING 4u
#define ARKE_PLAYER_ON_FIRE 8u
#define ARKE_PLAYER_SPECTATOR 16u
#define ARKE_PLAYER_FIRST_PERSON 32u
#define ARKE_PLAYER_INVISIBLE 64u
#define ARKE_PLAYER_HURT 128u

// Material Variants
#define ARKE_VARIANT_CHANGE_COLOR 1u
#define ARKE_VARIANT_CHANGE_COLOR_MULTI 2u
#define ARKE_VARIANT_MASKED_MULTITEXTURE 16u
#define ARKE_VARIANT_COLOR_SECOND_TEXTURE 32u
#define ARKE_VARIANT_TINTED_ALPHA_TEST 128u
```

---

## 5. Cross-Compilation Pipeline

### 5.1 Compilation Flowchart
```
Java GLSL (.vsh/.fsh/.csh)
         │
         ▼
    JCPP Preprocessor (Inject #include, IS_ARKE, Iris Macros)
         │
         ▼
    glslang Frontend (Compile GLSL to SPIR-V Intermediate AST)
         │
         ▼
    SPIRV-Tools Optimizer (spvtools::Optimizer dead-code & legalization passes)
         │
         ▼
    SPIRV-Cross Backend
    ├─► Windows: CompilerHLSL ──► DXIL (via dxcompiler.dll) ──► Direct3D 12
    └─► Android: CompilerGLSL ──► OpenGL ES 3.2 ESSL (#version 320 es) / Vulkan SPIR-V
```

### 5.2 Uniform Buffer Structures (Bedrock -> Java Mapping)
Bedrock's RenderDragon uniform stream is gathered per frame and bound into 4 primary ConstantBuffer slots:

#### Slot 0: `ConstantBuffer<ArkeFrame> frame : register(b0)`
```cpp
struct ArkeCamera {
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float4x4 inverseView;
    float4x4 inverseProjection;
    float4x4 inverseViewProjection;
    float4x4 previousViewProjection;
    float3 position;
    float nearPlane;
    int3 blockPosition;
    float farPlane;
    float3 fraction;
    float fov;
    float3 previousPosition;
    float aspect;
    float3 motion;
    float pitch;
    float3 forward;
    float yaw;
};

struct ArkeHand {
    float4x4 projection;
    float4x4 inverseProjection;
    float depthScale;
    float fov;
    float2 depthRange;
};

struct ArkeShadow {
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float3 direction;
    float distance;
    float resolution;
    float depth;
    float texel;
    uint enabled;
};

struct ArkeTime {
    float seconds;
    float delta;
    uint frame;
    uint ticks;
    float dayTime;
    uint day;
    uint moonPhase;
    float partialTick;
};

struct ArkeSky {
    float3 sunDirection;
    float sunAngle;
    float3 moonDirection;
    float moonBrightness;
    float3 lightDirection;
    float daylight;
    float3 skyColor;
    float rain;
    float3 fogColor;
    float thunder;
    float fogStart;
    float fogEnd;
    float cloudHeight;
    float wetness;
};

struct ArkeWorld {
    uint dimension;
    uint biome;
    uint biomeTag;
    uint flags;
    float temperature;
    float downfall;
    float renderDistance;
    float gamma;
    float3 lightning;
    float lightningAge;
    float ambient;
    float minHeight;
    float maxHeight;
    float mood;
};

struct ArkeVoxels {
    int3 origin;
    uint enabled;
    uint3 size;
    uint updated;
};

struct ArkePlayer {
    float3 eyePosition;
    uint medium;
    float2 eyeLight;
    float2 eyeLightSmooth;
    float nightVision;
    float blindness;
    float darkness;
    float health;
    float hunger;
    float air;
    float armor;
    uint flags;
    uint mainHandTag;
    uint offHandTag;
    float mainHandLight;
    float offHandLight;
};

struct ArkeScreen {
    float2 size;
    float2 texel;
    uint2 pixels;
    float aspect;
    float inverseAspect;
};

struct ArkeSelection {
    int3 block;
    uint tag;
    float3 boxMin;
    uint hasBlock;
    float3 boxMax;
    uint hasBox;
};

struct ArkeFrame {
    ArkeCamera camera;
    ArkeHand hand;
    ArkeShadow shadows[4];
    ArkeTime time;
    ArkeSky sky;
    ArkeWorld world;
    ArkePlayer player;
    ArkeScreen screen;
    ArkeSelection selection;
    ArkeVoxels voxels;
};
```

#### Slot 1: `ConstantBuffer<ArkeMeshDraw> draw / ArkeChunk chunk : register(b1)`
Provides per-draw or per-chunk matrices, bone palettes (up to 8 bones for skinned entities), and UV transforms.

#### Slot 2: `ConstantBuffer<ArkePass> pass : register(b2)`
```cpp
struct ArkePass {
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float4x4 inverseViewProjection;
    float2 size;
    float2 texel;
    uint iteration;
    uint index;
    uint camera;
    uint geometry;
};
```

#### Slot 3: `ConstantBuffer<ArkeOptions> options : register(b3)`
Dynamic shader pack user options mapped from `shaders.properties`.

---

## 6. Renderer & UI Hooks (Windows Analysis)

### 6.1 Direct3D 12 SwapChain Hooking
On Windows, `Arke.dll` leverages LeviLamina's `ll::memory::hookEx` to intercept:
- `IDXGISwapChain::Present`
- `ID3D12CommandQueue::ExecuteCommandLists`
- RenderDragon pass submission routines in `LevelRendererPlayer`

When `Present` is invoked, Arke binds the render target backbuffer, executes composite/final passes, and renders the Dear ImGui interface.

### 6.2 Dear ImGui In-Game Overlay
Arke initializes Dear ImGui using the Win32 and DX12 backends (`imgui_impl_win32`, `imgui_impl_dx12`).
The UI consists of:
- **`##arke_launcher`**: A lightweight overlay button docked at the top-left screen margin.
- **`###arke_settings`**: The primary modal settings window with sub-tabs:
  - `##packs`: Lists discovered shader packs in `shaderpacks/`.
  - `##settings`: Sliders and dropdowns parsed from the active pack's options.
  - `##details`: Diagnostics, log viewer, and compile error reporting.
- **Logging**: Mod activity is logged to `mods/arke/logs/arke.log` using LeviLamina's `ll::io::Logger` and `FileSink`.

---

## 7. Android (`arm64-v8a`) Porting Strategy

To port Arke to Android Minecraft Bedrock, the Windows-specific subsystems must be replaced:

| Subsystem | Windows x64 Implementation | Android `arm64-v8a` Target Implementation |
| :--- | :--- | :--- |
| **Graphics API** | Direct3D 12 / DXGI | OpenGL ES 3.2 (ESSL) & Vulkan 1.2+ |
| **Shader Compilation** | `dxcompiler.dll` (SPIR-V -> HLSL -> DXIL) | `SPIRV-Cross` -> ESSL 3.2 (`#version 320 es`) & Vulkan SPIR-V |
| **Hooking Engine** | `ll::memory::hookEx` / MinHook | **Dobby** (`DobbyHook`) or **ShadowHook** |
| **Target Binary** | `Minecraft.Windows.exe` | `libminecraftpe.so` (BGFX / RenderDragon) |
| **Swapchain Hook** | `IDXGISwapChain::Present` | `eglSwapBuffers` (libEGL.so) or `vkQueuePresentKHR` |
| **Shader Interception**| D3D12 PSO Creation | `glShaderSource`, `glCompileShader`, or `vkCreateShaderModule` |
| **UI Rendering** | `imgui_impl_dx12` + `imgui_impl_win32` | `imgui_impl_opengl3` + `imgui_impl_android` |
| **Input Capture** | Win32 `WndProc` / `HIDController` | Android `ANativeWindow` touch dispatch / JNI input events |
| **Mod Loader Entry** | `ll_mod_load` (LeviLamina Windows) | `JNI_OnLoad` (`libarke_android.so` / LeviLamina Android mod) |

---

## 8. Minecraft Bedrock 26.60 (arm64-v8a) RenderDragon & BGFX Analysis

### 8.1 Target Binary Identity
- **Binary Path**: `lib/arm64-v8a/libminecraftpe.so` (extracted from `minecraft-26-60-music.apk`)
- **File Size**: `342,231,392` bytes (~326.37 MiB)
- **ELF Class**: ELF64, Little Endian, AArch64 (ARMv8-A)
- **Android NDK Ident**: NDK r28c (Android API 26+)
- **GNU Build ID**: `eb291b883dfdfaff70c4999c2eabd61885ae3e15`
- **Dynamic Symbol Count**: 90,670 exported symbols, 102 dynamic imported graphics symbols

### 8.2 RenderDragon Material Architecture (`.material.bin`)
On Android, Minecraft Bedrock 26.60 stores precompiled shader packages in `assets/assets/renderer/materials/*.material.bin`:
- **Package Header**: Magic `\x1a\xda\x11\n` (RenderDragon Compiled Material Package), version 0.
- **Embedded Target Shaders**: Contains high-performance ESSL 3.10 / ESSL 3.20 shaders (`#version 310 es` / `#version 320 es`).
- **Pass Structure**: Core materials include `RenderChunk.material.bin` (terrain geometry), `Entity.material.bin` (mob & player rendering), `Sky.material.bin` (celestial & atmosphere), `DeferredShadingBase.material.bin` (PBR lighting passes), and `BlockSelectionOverlay.material.bin` (selection box outline).

### 8.3 Exact PLT/GOT Relocation Offsets (libminecraftpe.so 26.60)
The following functions are imported via PLT relocations (`R_AARCH64_JUMP_SLOT`) and can be intercepted via PLT hooking or inline hook trampolines:

| Function Name | GOT Offset (AArch64) | Relocation Type | Target Library |
| :--- | :--- | :--- | :--- |
| `eglSwapBuffers` | `0x0000000013feb528` | `R_AARCH64_JUMP_SLOT` | `libEGL.so` |
| `glShaderSource` | `0x0000000013fac9c8` | `R_AARCH64_JUMP_SLOT` | `libGLESv3.so` |
| `glCompileShader`| `0x0000000013faca38` | `R_AARCH64_JUMP_SLOT` | `libGLESv3.so` |
| `glLinkProgram`  | `0x0000000013fac940` | `R_AARCH64_JUMP_SLOT` | `libGLESv3.so` |
| `glUseProgram`   | `0x0000000013facb28` | `R_AARCH64_JUMP_SLOT` | `libGLESv3.so` |
| `glUniformMatrix4fv` | `0x0000000013fac920` | `R_AARCH64_JUMP_SLOT` | `libGLESv3.so` |
| `glUniformMatrix3fv` | `0x0000000013fac8b8` | `R_AARCH64_JUMP_SLOT` | `libGLESv3.so` |
| `glUniform4fv`   | `0x0000000013fac9d0` | `R_AARCH64_JUMP_SLOT` | `libGLESv3.so` |
| `glDrawArrays`   | `0x0000000013fac910` | `R_AARCH64_JUMP_SLOT` | `libGLESv3.so` |
| `glDrawElements` | `0x0000000013facae8` | `R_AARCH64_JUMP_SLOT` | `libGLESv3.so` |

### 8.4 RenderDragon to Iris/OptiFine Uniform Mapping Table
Recovered from decompiled `.material.bin` shaders and matched directly with Arke's `ArkeFrame` (b0) and `ArkePass` (b2) structures:

| RenderDragon / BGFX Uniform | Type | Semantic / Description | Mapped Iris/Arke Field |
| :--- | :--- | :--- | :--- |
| `u_view` | `mat4` | View transformation matrix | `camera.view` / `gbufferModelView` |
| `u_proj` | `mat4` | Camera projection matrix | `camera.projection` / `gbufferProjection` |
| `u_viewProj` | `mat4` | Combined View-Projection | `camera.viewProjection` |
| `u_invView` | `mat4` | Inverse View matrix | `camera.inverseView` / `gbufferModelViewInverse` |
| `u_invProj` | `mat4` | Inverse Projection matrix | `camera.inverseProjection` / `gbufferProjectionInverse` |
| `u_prevViewProj` | `mat4` | Previous frame View-Proj | `camera.previousViewProjection` |
| `u_model[4]` | `mat4[4]`| Per-instance world matrix | `draw.model` / `bones` |
| `ViewPositionAndTime` | `vec4` | xyz: Camera Pos, w: GameTime | `camera.position.xyz`, `time.seconds` |
| `FogColor` | `vec4` | Atmospheric fog color RGBA | `sky.fogColor` / `fogColor` |
| `FogAndDistanceControl` | `vec4` | x: start, y: end, z: chunk dist | `sky.fogStart`, `sky.fogEnd`, `world.renderDistance` |
| `RenderChunkFogAlpha` | `vec4` | Terrain fog alpha blending | `sky.wetness` / `rain` factor |
| `LightDiffuseColorAndIlluminance`| `vec4` | Sunlight/moonlight color + lux | `sky.daylight`, `sky.sunAngle` |
| `LightWorldSpaceDirection` | `vec4` | Sun/Moon light vector | `sky.sunDirection` / `sunPosition` |
| `SkyColor` | `vec4` | Clear sky ambient color | `sky.skyColor` / `skyColor` |
| `OverlayColor` | `vec4` | Actor damage/tint overlay | `draw.overlay` / `changeColor` |
| `TileLightColor` | `vec4` | Local voxel block/sky lighting | `player.eyeLight` (`gl_MultiTexCoord1`) |
| `u_alphaRef4` | `vec4` | Cutout alpha test threshold | `draw.cutoff` / `alphaTest` |
| `LightMapTexture` | `sampler2D` | Minecraft lightmap texture | Sampler slot 6 (`atlasSampler`) |

