#include "arke/uniform_bridge.hpp"
#include <cmath>
#include <sstream>

namespace arke {

UniformBridge& UniformBridge::instance() {
    static UniformBridge s_instance;
    return s_instance;
}

UniformBridge::UniformBridge() {
    m_frame = {};
    m_pass = {};
    m_frame.screen.size = {1920.0f, 1080.0f};
    m_frame.screen.texel = {1.0f / 1920.0f, 1.0f / 1080.0f};
    m_frame.screen.aspect = 1920.0f / 1080.0f;
    m_frame.screen.inverseAspect = 1080.0f / 1920.0f;
    m_frame.world.dimension = ARKE_DIMENSION_OVERWORLD;
}

static Mat4 computeInverse(const Mat4& in) {
    // Simple 4x4 inverse calculation
    Mat4 inv;
    const float* m = in.m.data();
    float* out = inv.m.data();

    out[0] = m[5]  * m[10] * m[15] - 
             m[5]  * m[11] * m[14] - 
             m[9]  * m[6]  * m[15] + 
             m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] - 
             m[13] * m[7]  * m[10];

    out[4] = -m[4]  * m[10] * m[15] + 
              m[4]  * m[11] * m[14] + 
              m[8]  * m[6]  * m[15] - 
              m[8]  * m[7]  * m[14] - 
              m[12] * m[6]  * m[11] + 
              m[12] * m[7]  * m[10];

    out[8] = m[4]  * m[9] * m[15] - 
             m[4]  * m[11] * m[13] - 
             m[8]  * m[5] * m[15] + 
             m[8]  * m[7] * m[13] + 
             m[12] * m[5] * m[11] - 
             m[12] * m[7] * m[9];

    out[12] = -m[4]  * m[9] * m[14] + 
               m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] - 
               m[8]  * m[6] * m[13] - 
               m[12] * m[5] * m[10] + 
               m[12] * m[6] * m[9];

    float det = m[0] * out[0] + m[1] * out[4] + m[2] * out[8] + m[3] * out[12];
    if (std::abs(det) < 1e-6f) return in;

    float invDet = 1.0f / det;
    for (int i = 0; i < 16; i++) {
        out[i] *= invDet;
    }
    return inv;
}

void UniformBridge::updateFrame(float deltaTime, double totalSeconds, uint32_t tickCount) {
    m_frame.time.delta = deltaTime;
    m_frame.time.seconds = static_cast<float>(totalSeconds);
    m_frame.time.frame++;
    m_frame.time.ticks = tickCount;
    m_frame.time.dayTime = static_cast<float>(std::fmod(totalSeconds, 1200.0) / 1200.0);
}

void UniformBridge::setCamera(const Vec3& pos, float pitch, float yaw, float fov, float nearPlane, float farPlane, float aspect) {
    m_frame.camera.previousPosition = m_frame.camera.position;
    m_frame.camera.position = pos;
    m_frame.camera.pitch = pitch;
    m_frame.camera.yaw = yaw;
    m_frame.camera.fov = fov;
    m_frame.camera.nearPlane = nearPlane;
    m_frame.camera.farPlane = farPlane;
    m_frame.camera.aspect = aspect;

    float radPitch = pitch * 0.0174532925f;
    float radYaw = yaw * 0.0174532925f;
    m_frame.camera.forward.x = -std::sin(radYaw) * std::cos(radPitch);
    m_frame.camera.forward.y = std::sin(radPitch);
    m_frame.camera.forward.z = std::cos(radYaw) * std::cos(radPitch);
}

void UniformBridge::setMatrices(const Mat4& view, const Mat4& proj) {
    m_frame.camera.view = view;
    m_frame.camera.projection = proj;
    m_frame.camera.inverseView = computeInverse(view);
    m_frame.camera.inverseProjection = computeInverse(proj);

    m_pass.view = view;
    m_pass.projection = proj;
    m_pass.inverseViewProjection = computeInverse(proj); // approx
}

void UniformBridge::setPlayerState(uint32_t flags, uint32_t medium, float health, float hunger) {
    m_frame.player.flags = flags;
    m_frame.player.medium = medium;
    m_frame.player.health = health;
    m_frame.player.hunger = hunger;
}

void UniformBridge::setEnvironment(uint32_t dimension, float sunAngle, float rain, float thunder, const Vec3& skyColor, const Vec3& fogColor) {
    m_frame.world.dimension = dimension;
    m_frame.sky.sunAngle = sunAngle;
    m_frame.sky.rain = rain;
    m_frame.sky.thunder = thunder;
    m_frame.sky.skyColor = skyColor;
    m_frame.sky.fogColor = fogColor;

    float radSun = sunAngle * 6.2831853f;
    m_frame.sky.sunDirection = {std::cos(radSun), std::sin(radSun), 0.0f};
    m_frame.sky.moonDirection = {-std::cos(radSun), -std::sin(radSun), 0.0f};
}

std::string UniformBridge::generateGLSLUniformBlock() const {
    std::ostringstream ss;
    ss << "// Auto-generated Arke Uniform Block for OpenGL ES 3.2 ESSL\n"
       << "layout(std140, binding = 0) uniform ArkeFrameBlock {\n"
       << "    mat4 gbufferModelView;\n"
       << "    mat4 gbufferProjection;\n"
       << "    mat4 gbufferModelViewInverse;\n"
       << "    mat4 gbufferProjectionInverse;\n"
       << "    vec3 cameraPosition;\n"
       << "    vec3 previousCameraPosition;\n"
       << "    vec3 sunPosition;\n"
       << "    vec3 moonPosition;\n"
       << "    vec3 upPosition;\n"
       << "    vec3 skyColor;\n"
       << "    vec3 fogColor;\n"
       << "    vec2 viewWidthHeight;\n"
       << "    float frameTimeCounter;\n"
       << "    float frameTime;\n"
       << "    float rainStrength;\n"
       << "    float wetness;\n"
       << "    int worldTime;\n"
       << "    int worldDay;\n"
       << "    int isEyeInWater;\n"
       << "    float nightVision;\n"
       << "    float blindness;\n"
       << "};\n\n"
       << "// Direct aliases for OptiFine/Iris compatibility\n"
       << "#define modelViewMatrix gbufferModelView\n"
       << "#define projectionMatrix gbufferProjection\n"
       << "#define modelViewMatrixInverse gbufferModelViewInverse\n"
       << "#define projectionMatrixInverse gbufferProjectionInverse\n"
       << "#define viewWidth (viewWidthHeight.x)\n"
       << "#define viewHeight (viewWidthHeight.y)\n"
       << "#define aspectRatio (viewWidthHeight.x / viewWidthHeight.y)\n";
    return ss.str();
}

} // namespace arke
