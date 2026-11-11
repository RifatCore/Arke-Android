#pragma once

#include "arke/types.hpp"

namespace arke {

class UniformBridge {
public:
    static UniformBridge& instance();

    void updateFrame(float deltaTime, double totalSeconds, uint32_t tickCount);
    void setCamera(const Vec3& pos, float pitch, float yaw, float fov, float nearPlane, float farPlane, float aspect);
    void setMatrices(const Mat4& view, const Mat4& proj);
    void setPlayerState(uint32_t flags, uint32_t medium, float health, float hunger);
    void setEnvironment(uint32_t dimension, float sunAngle, float rain, float thunder, const Vec3& skyColor, const Vec3& fogColor);

    ArkeFrame& getFrame() { return m_frame; }
    const ArkeFrame& getFrame() const { return m_frame; }

    ArkePass& getPass() { return m_pass; }
    const ArkePass& getPass() const { return m_pass; }

    // Generates GLSL uniform block declarations matching OptiFine/Iris
    std::string generateGLSLUniformBlock() const;

private:
    UniformBridge();
    ArkeFrame m_frame;
    ArkePass m_pass;
};

} // namespace arke
