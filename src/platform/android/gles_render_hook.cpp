#include "arke/platform/gles_hook.hpp"
#include "arke/platform/imgui_overlay.hpp"
#include "arke/uniform_bridge.hpp"
#include "arke/shader/pipeline.hpp"
#include "arke/config.hpp"
#include "arke/logger.hpp"

#include <GLES3/gl32.h>
#include <EGL/egl.h>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <mutex>
#include <vector>

namespace arke::platform {

// --- BGFX-Safe Complete OpenGL ES State Guard ---
class GLStateGuard {
public:
    GLint prevDrawFBO{0};
    GLint prevReadFBO{0};
    GLint prevProgram{0};
    GLint prevActiveTexture{GL_TEXTURE0};
    GLint prevTex2D[4]{0, 0, 0, 0};
    GLint prevSampler[4]{0, 0, 0, 0};
    GLint prevVAO{0};
    GLint prevVBO{0};
    GLint prevEBO{0};
    GLint prevViewport[4]{0, 0, 0, 0};
    GLint prevScissor[4]{0, 0, 0, 0};
    GLboolean prevScissorTest{GL_FALSE};
    GLboolean prevDepthTest{GL_FALSE};
    GLboolean prevDepthMask{GL_TRUE};
    GLboolean prevCullFace{GL_FALSE};
    GLint prevCullMode{GL_BACK};
    GLboolean prevBlend{GL_FALSE};
    GLint prevBlendSrcRgb{GL_ONE};
    GLint prevBlendDstRgb{GL_ZERO};
    GLint prevBlendSrcAlpha{GL_ONE};
    GLint prevBlendDstAlpha{GL_ZERO};
    GLint prevBlendEqRgb{GL_FUNC_ADD};
    GLint prevBlendEqAlpha{GL_FUNC_ADD};
    GLboolean prevColorMask[4]{GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLboolean prevStencilTest{GL_FALSE};

    GLStateGuard() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFBO);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFBO);
        glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTexture);

        for (int i = 0; i < 4; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex2D[i]);
            glGetIntegerv(GL_SAMPLER_BINDING, &prevSampler[i]);
        }
        glActiveTexture(prevActiveTexture);

        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevVBO);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &prevEBO);

        glGetIntegerv(GL_VIEWPORT, prevViewport);
        glGetIntegerv(GL_SCISSOR_BOX, prevScissor);
        prevScissorTest = glIsEnabled(GL_SCISSOR_TEST);

        prevDepthTest = glIsEnabled(GL_DEPTH_TEST);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);

        prevCullFace = glIsEnabled(GL_CULL_FACE);
        glGetIntegerv(GL_CULL_FACE_MODE, &prevCullMode);

        prevBlend = glIsEnabled(GL_BLEND);
        glGetIntegerv(GL_BLEND_SRC_RGB, &prevBlendSrcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &prevBlendDstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &prevBlendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &prevBlendDstAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &prevBlendEqRgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &prevBlendEqAlpha);

        glGetBooleanv(GL_COLOR_WRITEMASK, prevColorMask);
        prevStencilTest = glIsEnabled(GL_STENCIL_TEST);
    }

    ~GLStateGuard() {
        if (prevDrawFBO == prevReadFBO) {
            glBindFramebuffer(GL_FRAMEBUFFER, prevDrawFBO);
        } else {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFBO);
        }

        glUseProgram(prevProgram);

        for (int i = 0; i < 4; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, prevTex2D[i]);
            glBindSampler(i, prevSampler[i]);
        }
        glActiveTexture(prevActiveTexture);

        glBindVertexArray(prevVAO);
        glBindBuffer(GL_ARRAY_BUFFER, prevVBO);
        if (prevVAO == 0) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prevEBO);
        }

        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
        glScissor(prevScissor[0], prevScissor[1], prevScissor[2], prevScissor[3]);
        if (prevScissorTest) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);

        glDepthMask(prevDepthMask);
        if (prevDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);

        glCullFace(prevCullMode);
        if (prevCullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);

        glBlendEquationSeparate(prevBlendEqRgb, prevBlendEqAlpha);
        glBlendFuncSeparate(prevBlendSrcRgb, prevBlendDstRgb, prevBlendSrcAlpha, prevBlendDstAlpha);
        if (prevBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);

        glColorMask(prevColorMask[0], prevColorMask[1], prevColorMask[2], prevColorMask[3]);
        if (prevStencilTest) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
    }
};

// --- Post-Processing Full-Screen Pass Renderer for Mali-G57 ---
class PostProcessRenderer {
public:
    GLuint program{0};
    GLuint vao{0};
    GLuint vbo{0};
    GLuint copyTex{0};
    GLsizei copyTexW{0};
    GLsizei copyTexH{0};

    std::string currentCompiledPass;
    GLint u_colortex0{-1};
    GLint u_viewWidth{-1};
    GLint u_viewHeight{-1};
    GLint u_frameTimeCounter{-1};

    void initQuad() {
        if (vao != 0) return;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        // Clip-space quad: pos (x, y), texCoord (u, v)
        static const float quadData[] = {
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadData), quadData, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    bool compileStage(const std::string& vshSrc, const std::string& fshSrc, const std::string& passName) {
        if (program != 0) {
            glDeleteProgram(program);
            program = 0;
        }

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        const char* vPtr = vshSrc.c_str();
        glShaderSource(vs, 1, &vPtr, nullptr);
        glCompileShader(vs);

        GLint vsOk = 0;
        glGetShaderiv(vs, GL_COMPILE_STATUS, &vsOk);
        if (!vsOk) {
            char log[512];
            glGetShaderInfoLog(vs, sizeof(log), nullptr, log);
            Logger::instance().error("[PostProcessRenderer] Vertex compile error in " + passName + ": " + log);
            glDeleteShader(vs);
            return false;
        }

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        const char* fPtr = fshSrc.c_str();
        glShaderSource(fs, 1, &fPtr, nullptr);
        glCompileShader(fs);

        GLint fsOk = 0;
        glGetShaderiv(fs, GL_COMPILE_STATUS, &fsOk);
        if (!fsOk) {
            char log[512];
            glGetShaderInfoLog(fs, sizeof(log), nullptr, log);
            Logger::instance().error("[PostProcessRenderer] Fragment compile error in " + passName + ": " + log);
            glDeleteShader(vs);
            glDeleteShader(fs);
            return false;
        }

        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        glDeleteShader(vs);
        glDeleteShader(fs);

        GLint linkOk = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linkOk);
        if (!linkOk) {
            char log[512];
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            Logger::instance().error("[PostProcessRenderer] Link error in " + passName + ": " + log);
            glDeleteProgram(program);
            program = 0;
            return false;
        }

        u_colortex0 = glGetUniformLocation(program, "colortex0");
        u_viewWidth = glGetUniformLocation(program, "viewWidth");
        u_viewHeight = glGetUniformLocation(program, "viewHeight");
        u_frameTimeCounter = glGetUniformLocation(program, "frameTimeCounter");

        currentCompiledPass = passName;
        Logger::instance().info("[PostProcessRenderer] Ready active shader pass: " + passName);
        return true;
    }

    void render(GLint prevFBO, float width, float height, float totalTime) {
        const auto& passes = shader::ShaderPipeline::instance().getPasses();
        if (passes.empty()) return;

        // Find final pass or composite pass
        const shader::CompiledPass* activePass = nullptr;
        for (const auto& p : passes) {
            if (p.name == "final" && !p.fragmentESSL.empty()) {
                activePass = &p;
                break;
            }
        }
        if (!activePass) {
            for (const auto& p : passes) {
                if ((p.name == "composite" || p.name == "composite1") && !p.fragmentESSL.empty()) {
                    activePass = &p;
                    break;
                }
            }
        }
        if (!activePass) return;

        // Recompile if pass changed
        if (program == 0 || currentCompiledPass != activePass->name) {
            std::string vsh = activePass->vertexESSL;
            if (vsh.empty()) {
                vsh = "#version 300 es\n"
                      "precision highp float;\n"
                      "layout(location = 0) in vec2 a_position;\n"
                      "layout(location = 1) in vec2 a_texcoord;\n"
                      "out vec2 texCoord;\n"
                      "void main() {\n"
                      "    texCoord = a_texcoord;\n"
                      "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                      "}\n";
            }
            if (!compileStage(vsh, activePass->fragmentESSL, activePass->name)) {
                return;
            }
        }

        initQuad();

        // Determine source scene texture
        GLuint srcTex = 0;
        if (prevFBO != 0) {
            GLint objType = 0;
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &objType);
            if (objType == GL_TEXTURE) {
                GLint texName = 0;
                glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &texName);
                srcTex = static_cast<GLuint>(texName);
            }
        }

        if (srcTex == 0) {
            // FBO 0 backbuffer copy
            if (copyTex == 0 || copyTexW != (GLsizei)width || copyTexH != (GLsizei)height) {
                if (copyTex != 0) glDeleteTextures(1, &copyTex);
                glGenTextures(1, &copyTex);
                glBindTexture(GL_TEXTURE_2D, copyTex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                copyTexW = (GLsizei)width;
                copyTexH = (GLsizei)height;
            }
            glBindTexture(GL_TEXTURE_2D, copyTex);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, (GLsizei)width, (GLsizei)height);
            srcTex = copyTex;
        }

        // Render to onscreen default FBO 0
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glViewport(0, 0, (GLsizei)width, (GLsizei)height);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glUseProgram(program);
        if (u_colortex0 >= 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, srcTex);
            glBindSampler(0, 0);
            glUniform1i(u_colortex0, 0);
        }
        if (u_viewWidth >= 0) glUniform1f(u_viewWidth, width);
        if (u_viewHeight >= 0) glUniform1f(u_viewHeight, height);
        if (u_frameTimeCounter >= 0) glUniform1f(u_frameTimeCounter, totalTime);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Immediate cleanup of our resources
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glUseProgram(0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindSampler(0, 0);
    }
};

static PostProcessRenderer g_postProcessRenderer;
static auto g_lastFrameTime = std::chrono::high_resolution_clock::now();
static double g_totalSeconds = 0.0;
static uint32_t g_frameCount = 0;

static bool g_worldRenderedThisFrame = false;

static inline void checkWorldUniformName(const std::string& name) {
    if (g_worldRenderedThisFrame) return;
    if (name == "ViewPositionAndTime" || 
        name == "FogAndDistanceControl" || 
        name == "LightWorldSpaceDirection" || 
        name == "u_view" || 
        name == "u_proj" ||
        name == "u_viewProj" ||
        name == "u_modelView" ||
        name == "u_modelViewProj" ||
        name.find("RenderChunk") != std::string::npos ||
        name.find("Terrain") != std::string::npos ||
        name.find("FogAndDistance") != std::string::npos ||
        name.find("ViewPosition") != std::string::npos ||
        name.find("WorldPos") != std::string::npos ||
        name.find("LightDirection") != std::string::npos) {
        g_worldRenderedThisFrame = true;
    }
}

static std::mutex g_uniformMutex;
static std::unordered_map<int32_t, std::string> g_uniformLocationMap;
static std::unordered_set<std::string> g_loggedUniforms;

void onEglSwapBuffersPost(void* dpy, void* surface) {
    (void)dpy;
    (void)surface;

    auto now = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(now - g_lastFrameTime).count();
    g_lastFrameTime = now;
    g_totalSeconds += dt;
    g_frameCount++;

    // Update uniform bridge
    UniformBridge::instance().updateFrame(dt, g_totalSeconds, g_frameCount);

    {
        // Scope-bound RAII complete GL state preservation
        GLStateGuard stateGuard;

        float width = static_cast<float>(stateGuard.prevViewport[2] > 0 ? stateGuard.prevViewport[2] : 1600);
        float height = static_cast<float>(stateGuard.prevViewport[3] > 0 ? stateGuard.prevViewport[3] : 720);

        // 1. Render active post-processing pass ONLY if pack is active, enabled, AND world geometry was drawn this frame
        if (g_worldRenderedThisFrame && shader::ShaderPipeline::instance().isPackLoaded() && ConfigManager::instance().get().enabled) {
            g_postProcessRenderer.render(stateGuard.prevDrawFBO, width, height, static_cast<float>(g_totalSeconds));
        }

        // 2. Explicitly bind default onscreen framebuffer FBO 0 for Dear ImGui overlay
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        ImGuiOverlay::instance().newFrame(width, height);
        ImGuiOverlay::instance().render();

        // Immediate cleanup before restoring BGFX state
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glUseProgram(0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindSampler(0, 0);
    }

    // Reset world rendered flag for the next frame
    g_worldRenderedThisFrame = false;
}

void onGlShaderSourcePre(
    uint32_t shader,
    int32_t count,
    const char* const* string,
    const int32_t* length,
    const char** outCustomShader
) {
    if (!outCustomShader || !string || count <= 0) return;

    if (!shader::ShaderPipeline::instance().isPackLoaded()) {
        *outCustomShader = nullptr;
        return;
    }

    std::string srcText = "";
    for (int i = 0; i < count; i++) {
        if (string[i]) {
            srcText += (length && length[i] > 0) ? std::string(string[i], length[i]) : std::string(string[i]);
        }
    }

    const auto& passes = shader::ShaderPipeline::instance().getPasses();
    for (const auto& pass : passes) {
        if (srcText.find("POSITION") != std::string::npos && !pass.vertexESSL.empty()) {
            *outCustomShader = pass.vertexESSL.c_str();
            Logger::instance().info("[GLES] Injected RenderDragon vertex shader pass: " + pass.name);
            return;
        }
    }

    *outCustomShader = nullptr;
}

void onGlCompileShaderPost(uint32_t shader) {
    (void)shader;
}

void onGlGetUniformLocationPost(uint32_t program, const char* name, int32_t location) {
    (void)program;
    if (location < 0 || !name) return;

    std::lock_guard<std::mutex> lock(g_uniformMutex);
    g_uniformLocationMap[location] = std::string(name);
}

void onGlUniformMatrix4fvPre(int32_t location, int32_t count, uint8_t transpose, const float* value) {
    (void)count;
    (void)transpose;
    if (location < 0 || !value) return;

    std::string name;
    {
        std::lock_guard<std::mutex> lock(g_uniformMutex);
        auto it = g_uniformLocationMap.find(location);
        if (it != g_uniformLocationMap.end()) {
            name = it->second;
        }
    }

    if (!name.empty()) {
        UniformBridge::instance().setRenderDragonMatrix(name, value);
        checkWorldUniformName(name);

        std::lock_guard<std::mutex> lock(g_uniformMutex);
        if (g_loggedUniforms.find(name) == g_loggedUniforms.end()) {
            g_loggedUniforms.insert(name);
            Logger::instance().info("[GLES] Bound RenderDragon Matrix uniform: " + name);
        }
    }
}

void onGlUniform4fvPre(int32_t location, int32_t count, const float* value) {
    (void)count;
    if (location < 0 || !value) return;

    std::string name;
    {
        std::lock_guard<std::mutex> lock(g_uniformMutex);
        auto it = g_uniformLocationMap.find(location);
        if (it != g_uniformLocationMap.end()) {
            name = it->second;
        }
    }

    if (!name.empty()) {
        UniformBridge::instance().setRenderDragonVector4(name, value);
        checkWorldUniformName(name);

        std::lock_guard<std::mutex> lock(g_uniformMutex);
        if (g_loggedUniforms.find(name) == g_loggedUniforms.end()) {
            g_loggedUniforms.insert(name);
            Logger::instance().info("[GLES] Bound RenderDragon Vector4 uniform: " + name);
        }
    }
}

} // namespace arke::platform
