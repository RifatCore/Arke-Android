#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <vector>

namespace arke {

// --- Macros & Constants matching upstream Arke v0.1.1 ---
constexpr uint32_t ARKE_VERSION_NUMBER = 100; // 0.1.0/0.1.1
constexpr const char* ARKE_VERSION_STRING = "0.1.1-android";

constexpr uint32_t ARKE_CAMERA_MAIN = 0;
constexpr uint32_t ARKE_CAMERA_HAND = 1;
constexpr uint32_t ARKE_CAMERA_SHADOW = 2;

constexpr uint32_t ARKE_DIMENSION_OVERWORLD = 0;
constexpr uint32_t ARKE_DIMENSION_NETHER = 1;
constexpr uint32_t ARKE_DIMENSION_END = 2;

constexpr uint32_t ARKE_GEOMETRY_TERRAIN = 1;
constexpr uint32_t ARKE_GEOMETRY_ENTITY = 2;
constexpr uint32_t ARKE_GEOMETRY_BLOCK_ENTITY = 3;
constexpr uint32_t ARKE_GEOMETRY_HAND = 4;
constexpr uint32_t ARKE_GEOMETRY_PARTICLE = 5;
constexpr uint32_t ARKE_GEOMETRY_SKY = 6;
constexpr uint32_t ARKE_GEOMETRY_MESH = 7;

constexpr uint32_t ARKE_MATERIAL_PLAIN = 0u;
constexpr uint32_t ARKE_MATERIAL_MULTITEXTURE = 1u;
constexpr uint32_t ARKE_MATERIAL_TINT = 2u;
constexpr uint32_t ARKE_MATERIAL_BANNER_LAYER = 3u;

constexpr uint32_t ARKE_MAX_BONES = 8;
constexpr uint32_t ARKE_MAX_SHADOWS = 4;

constexpr uint32_t ARKE_MEDIUM_AIR = 0;
constexpr uint32_t ARKE_MEDIUM_WATER = 1;
constexpr uint32_t ARKE_MEDIUM_LAVA = 2;
constexpr uint32_t ARKE_MEDIUM_POWDER_SNOW = 3;

constexpr uint32_t ARKE_PLAYER_ON_GROUND = 1u;
constexpr uint32_t ARKE_PLAYER_SNEAKING = 2u;
constexpr uint32_t ARKE_PLAYER_SPRINTING = 4u;
constexpr uint32_t ARKE_PLAYER_ON_FIRE = 8u;
constexpr uint32_t ARKE_PLAYER_SPECTATOR = 16u;
constexpr uint32_t ARKE_PLAYER_FIRST_PERSON = 32u;
constexpr uint32_t ARKE_PLAYER_INVISIBLE = 64u;
constexpr uint32_t ARKE_PLAYER_HURT = 128u;

constexpr uint32_t ARKE_VARIANT_CHANGE_COLOR = 1u;
constexpr uint32_t ARKE_VARIANT_CHANGE_COLOR_MULTI = 2u;
constexpr uint32_t ARKE_VARIANT_MASKED_MULTITEXTURE = 16u;
constexpr uint32_t ARKE_VARIANT_COLOR_SECOND_TEXTURE = 32u;
constexpr uint32_t ARKE_VARIANT_TINTED_ALPHA_TEST = 128u;

// Vertex attribute flags
constexpr uint32_t ARKE_VERTEX_ALPHA_TEST = 1u;
constexpr uint32_t ARKE_VERTEX_BACK_FACE = 2u;
constexpr uint32_t ARKE_VERTEX_DOUBLE_SIDED = 4u;
constexpr uint32_t ARKE_VERTEX_COLOR_BASED = 8u;
constexpr uint32_t ARKE_VERTEX_PACKED_LIGHT = 16u;
constexpr uint32_t ARKE_VERTEX_TEXT = 32u;

// Color spaces
enum class ColorSpace : uint32_t {
    sRGB = 0,
    DCI_P3 = 1,
    Display_P3 = 2,
    Rec2020 = 3,
    AdobeRGB = 4
};

// Shader pass classifications
enum class PassStage {
    GBuffers,
    Shadow,
    Deferred,
    Composite,
    Final,
    Compute
};

struct Vec2 { float x{0.f}, y{0.f}; };
struct Vec3 { float x{0.f}, y{0.f}, z{0.f}; };
struct Vec4 { float x{0.f}, y{0.f}, z{0.f}, w{0.f}; };
struct IVec3 { int32_t x{0}, y{0}, z{0}; };
struct UVec2 { uint32_t x{0}, y{0}; };
struct UVec3 { uint32_t x{0}, y{0}, z{0}; };
struct UVec4 { uint32_t x{0}, y{0}, z{0}, w{0}; };
struct Mat4 { std::array<float, 16> m{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; };

// --- Uniform Buffer Structures (Exact Bedrock -> Java mapping recovered from Arke.dll) ---

struct ArkeCamera {
    Mat4 view;
    Mat4 projection;
    Mat4 viewProjection;
    Mat4 inverseView;
    Mat4 inverseProjection;
    Mat4 inverseViewProjection;
    Mat4 previousViewProjection;
    Vec3 position;
    float nearPlane{0.1f};
    IVec3 blockPosition;
    float farPlane{256.0f};
    Vec3 fraction;
    float fov{70.0f};
    Vec3 previousPosition;
    float aspect{1.777f};
    Vec3 motion;
    float pitch{0.0f};
    Vec3 forward{0.0f, 0.0f, 1.0f};
    float yaw{0.0f};
};

struct ArkeHand {
    Mat4 projection;
    Mat4 inverseProjection;
    float depthScale{1.0f};
    float fov{70.0f};
    Vec2 depthRange{0.01f, 10.0f};
};

struct ArkeShadow {
    Mat4 view;
    Mat4 projection;
    Mat4 viewProjection;
    Vec3 direction;
    float distance{128.0f};
    float resolution{2048.0f};
    float depth{0.0f};
    float texel{1.0f / 2048.0f};
    uint32_t enabled{1};
};

struct ArkeTime {
    float seconds{0.0f};
    float delta{0.016f};
    uint32_t frame{0};
    uint32_t ticks{0};
    float dayTime{0.0f};
    uint32_t day{0};
    uint32_t moonPhase{0};
    float partialTick{0.0f};
};

struct ArkeSky {
    Vec3 sunDirection{0.0f, 1.0f, 0.0f};
    float sunAngle{0.0f};
    Vec3 moonDirection{0.0f, -1.0f, 0.0f};
    float moonBrightness{0.0f};
    Vec3 lightDirection{0.0f, 1.0f, 0.0f};
    float daylight{1.0f};
    Vec3 skyColor{0.5f, 0.7f, 1.0f};
    float rain{0.0f};
    Vec3 fogColor{0.7f, 0.8f, 1.0f};
    float thunder{0.0f};
    float fogStart{64.0f};
    float fogEnd{256.0f};
    float cloudHeight{192.0f};
    float wetness{0.0f};
};

struct ArkeWorld {
    uint32_t dimension{ARKE_DIMENSION_OVERWORLD};
    uint32_t biome{1};
    uint32_t biomeTag{0};
    uint32_t flags{0};
    float temperature{0.8f};
    float downfall{0.4f};
    float renderDistance{12.0f};
    float gamma{1.0f};
    Vec3 lightning{0.0f, 0.0f, 0.0f};
    float lightningAge{0.0f};
    float ambient{0.1f};
    float minHeight{-64.0f};
    float maxHeight{320.0f};
    float mood{0.0f};
};

struct ArkeVoxels {
    IVec3 origin{0, 0, 0};
    uint32_t enabled{0};
    UVec3 size{0, 0, 0};
    uint32_t updated{0};
};

struct ArkePlayer {
    Vec3 eyePosition;
    uint32_t medium{ARKE_MEDIUM_AIR};
    Vec2 eyeLight{1.0f, 1.0f};
    Vec2 eyeLightSmooth{1.0f, 1.0f};
    float nightVision{0.0f};
    float blindness{0.0f};
    float darkness{0.0f};
    float health{20.0f};
    float hunger{20.0f};
    float air{300.0f};
    float armor{0.0f};
    uint32_t flags{ARKE_PLAYER_FIRST_PERSON | ARKE_PLAYER_ON_GROUND};
    uint32_t mainHandTag{0};
    uint32_t offHandTag{0};
    float mainHandLight{0.0f};
    float offHandLight{0.0f};
};

struct ArkeScreen {
    Vec2 size{1920.0f, 1080.0f};
    Vec2 texel{1.0f / 1920.0f, 1.0f / 1080.0f};
    UVec2 pixels{1920, 1080};
    float aspect{1920.0f / 1080.0f};
    float inverseAspect{1080.0f / 1920.0f};
};

struct ArkeSelection {
    IVec3 block{0, 0, 0};
    uint32_t tag{0};
    Vec3 boxMin{0.0f, 0.0f, 0.0f};
    uint32_t hasBlock{0};
    Vec3 boxMax{1.0f, 1.0f, 1.0f};
    uint32_t hasBox{0};
};

// Slot 0 (b0): Frame ConstantBuffer
struct alignas(16) ArkeFrame {
    ArkeCamera camera;
    ArkeHand hand;
    std::array<ArkeShadow, ARKE_MAX_SHADOWS> shadows;
    ArkeTime time;
    ArkeSky sky;
    ArkeWorld world;
    ArkePlayer player;
    ArkeScreen screen;
    ArkeSelection selection;
    ArkeVoxels voxels;
};

// Slot 1 (b1): Mesh Draw ConstantBuffer
struct alignas(16) ArkeMeshDraw {
    std::array<Mat4, ARKE_MAX_BONES> bones;
    Mat4 model;
    Vec4 uvTransform;
    Vec4 uvMatrix;
    Vec4 uvOffset;
    Vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 overlay{0.0f, 0.0f, 0.0f, 0.0f};
    Vec2 light{1.0f, 1.0f};
    uint32_t tag{0};
    uint32_t entity{0};
    UVec4 layout{0, 0, 0, 0};
    uint32_t flags{0};
    uint32_t geometry{ARKE_GEOMETRY_MESH};
    UVec2 textureSize{1024, 1024};
    Vec4 materialColor{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 changeColor{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 multiplicativeTint{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 bannerBase{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 bannerPattern{1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t material{ARKE_MATERIAL_PLAIN};
    uint32_t variant{0};
    float cutoff{0.5f};
    float alphaRewrite{0.0f};
    Mat4 previousModel;
    std::array<Mat4, ARKE_MAX_BONES> previousBones;
};

// Slot 1 (b1 alt): Terrain Chunk ConstantBuffer
struct alignas(16) ArkeChunk {
    Vec4 origin;
    Vec4 region;
};

// Slot 2 (b2): Pass ConstantBuffer
struct alignas(16) ArkePass {
    Mat4 view;
    Mat4 projection;
    Mat4 viewProjection;
    Mat4 inverseViewProjection;
    Vec2 size{1920.0f, 1080.0f};
    Vec2 texel{1.0f / 1920.0f, 1.0f / 1080.0f};
    uint32_t iteration{0};
    uint32_t index{0};
    uint32_t camera{ARKE_CAMERA_MAIN};
    uint32_t geometry{ARKE_GEOMETRY_TERRAIN};
};

// Slot 3 (b3): Shader Pack Dynamic Options
struct alignas(16) ArkeOptions {
    std::array<float, 64> floatOptions{};
    std::array<int32_t, 64> intOptions{};
};

} // namespace arke
