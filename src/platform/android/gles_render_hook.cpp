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

static ArkeShaderState s_arkeShaderState;

ArkeShaderState& getArkeShaderState() {
    return s_arkeShaderState;
}

static const char* getBuiltinVertexShader() {
    return "#version 320 es\n"
           "precision highp float;\n"
           "layout(location = 0) in vec2 a_position;\n"
           "layout(location = 1) in vec2 a_texcoord;\n"
           "out vec2 texCoord;\n"
           "void main() {\n"
           "    texCoord = a_texcoord;\n"
           "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
           "}\n";
}

static const char* getBuiltinFragmentShader() {
    return "#version 320 es\n"
           "precision highp float;\n"
           "\n"
           "in vec2 texCoord;\n"
           "out vec4 fragColor;\n"
           "\n"
           "uniform sampler2D colortex0;\n"
           "uniform vec2 u_resolution;\n"
           "uniform float u_time;\n"
           "uniform float u_saturation;\n"
           "uniform float u_contrast;\n"
           "uniform float u_exposure;\n"
           "uniform float u_bloom;\n"
           "uniform float u_warmth;\n"
           "uniform int u_presetMode;\n"
           "\n"
           "vec3 acesFilm(vec3 x) {\n"
           "    float a = 2.51;\n"
           "    float b = 0.03;\n"
           "    float c = 2.43;\n"
           "    float d = 0.59;\n"
           "    float e = 0.14;\n"
           "    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);\n"
           "}\n"
           "\n"
           "float luminance(vec3 c) {\n"
           "    return dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
           "}\n"
           "\n"
           "void main() {\n"
           "    vec2 uv = texCoord;\n"
           "    vec4 base = texture(colortex0, uv);\n"
           "    vec3 color = base.rgb;\n"
           "\n"
           "    // 9-tap Fast Mobile Bloom on bright sky/water/light pixels\n"
           "    if (u_bloom > 0.001) {\n"
           "        vec2 texel = 1.0 / max(u_resolution, vec2(1.0, 1.0));\n"
           "        vec2 offset = texel * 2.5;\n"
           "        vec3 bloomSum = vec3(0.0);\n"
           "        float totalWeight = 0.0;\n"
           "        for (int y = -1; y <= 1; ++y) {\n"
           "            for (int x = -1; x <= 1; ++x) {\n"
           "                vec2 sUV = uv + vec2(float(x), float(y)) * offset;\n"
           "                vec3 sCol = texture(colortex0, sUV).rgb;\n"
           "                float lum = luminance(sCol);\n"
           "                float bright = clamp((lum - 0.55) / 0.45, 0.0, 1.0);\n"
           "                float w = (x == 0 && y == 0) ? 0.25 : ((x == 0 || y == 0) ? 0.125 : 0.0625);\n"
           "                bloomSum += sCol * (bright * bright) * w;\n"
           "                totalWeight += w;\n"
           "            }\n"
           "        }\n"
           "        vec3 bloomColor = bloomSum / max(totalWeight, 0.001);\n"
           "        color += bloomColor * (u_bloom * 1.5);\n"
           "    }\n"
           "\n"
           "    // Exposure scaling\n"
           "    color *= u_exposure;\n"
           "\n"
           "    // Sunlight Warmth / Color temperature grading\n"
           "    vec3 warmGrading = vec3(1.0 + u_warmth * 0.35, 1.0 + u_warmth * 0.10, 1.0 - u_warmth * 0.25);\n"
           "    color *= warmGrading;\n"
           "\n"
           "    // Contrast curve around midpoint 0.5\n"
           "    color = clamp((color - 0.5) * u_contrast + 0.5, 0.0, 4.0);\n"
           "\n"
           "    // Saturation adjustment\n"
           "    float lum = luminance(color);\n"
           "    color = mix(vec3(lum), color, u_saturation);\n"
           "\n"
           "    // ACES Filmic tonemapping\n"
           "    color = acesFilm(color);\n"
           "\n"
           "    fragColor = vec4(clamp(color, 0.0, 1.0), base.a);\n"
           "}\n";
}

static const char* getMakeUpFragmentShader() {
    return "#version 320 es\n"
           "precision highp float;\n"
           "\n"
           "in vec2 texCoord;\n"
           "out vec4 fragColor;\n"
           "\n"
           "uniform sampler2D colortex0;\n"
           "uniform sampler2D colortex1;\n"
           "uniform vec2 u_resolution;\n"
           "uniform float u_time;\n"
           "uniform int u_godrays;\n"
           "uniform int u_waterReflect;\n"
           "uniform int u_shadows;\n"
           "uniform int u_bloom;\n"
           "uniform int u_chromaAber;\n"
           "uniform int u_dof;\n"
           "\n"
           "// MakeUp-UltraFast signature tonemap: custom_sigmoid\n"
           "vec3 custom_sigmoid(vec3 color) {\n"
           "    color = 1.4 * color;\n"
           "    color = color / pow(pow(color, vec3(2.5)) + 1.0, vec3(0.4));\n"
           "    return pow(color, vec3(1.15));\n"
           "}\n"
           "\n"
           "float luminance(vec3 c) {\n"
           "    return dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
           "}\n"
           "\n"
           "vec3 color_aberration(sampler2D tex, vec2 uv, float strength) {\n"
           "    vec2 offset = (uv - 0.5) * (0.012 * strength);\n"
           "    vec3 col;\n"
           "    col.r = texture(tex, uv - offset).r;\n"
           "    col.g = texture(tex, uv - offset * 0.5).g;\n"
           "    col.b = texture(tex, uv).b;\n"
           "    return col;\n"
           "}\n"
           "\n"
           "vec3 calculateGodrays(sampler2D tex, vec2 uv, vec2 lightPos, float time) {\n"
           "    vec2 delta = (lightPos - uv) / 8.0;\n"
           "    vec3 rayColor = vec3(0.0);\n"
           "    vec2 curCoord = uv;\n"
           "    float illuminationDecay = 1.0;\n"
           "    for (int i = 0; i < 8; ++i) {\n"
           "        curCoord += delta;\n"
           "        vec3 sampleCol = texture(tex, clamp(curCoord, 0.0, 1.0)).rgb;\n"
           "        float bright = max(0.0, dot(sampleCol, vec3(0.333)) - 0.72) * 1.6;\n"
           "        rayColor += bright * illuminationDecay * vec3(1.0, 0.88, 0.70);\n"
           "        illuminationDecay *= 0.85;\n"
           "    }\n"
           "    return rayColor * 0.25;\n"
           "}\n"
           "\n"
           "void main() {\n"
           "    vec2 uv = texCoord;\n"
           "    vec4 base;\n"
           "    if (u_chromaAber > 0) {\n"
           "        base = vec4(color_aberration(colortex0, uv, 1.0), texture(colortex0, uv).a);\n"
           "    } else {\n"
           "        base = texture(colortex0, uv);\n"
           "    }\n"
           "    vec3 color = base.rgb;\n"
           "\n"
           "    // MakeUp-UltraFast Fast Mobile Bloom\n"
           "    if (u_bloom > 0) {\n"
           "        vec2 texel = 1.0 / max(u_resolution, vec2(1.0, 1.0));\n"
           "        vec2 offset = texel * 2.8;\n"
           "        vec3 bloomSum = vec3(0.0);\n"
           "        float totalWeight = 0.0;\n"
           "        for (int y = -1; y <= 1; ++y) {\n"
           "            for (int x = -1; x <= 1; ++x) {\n"
           "                vec2 sUV = clamp(uv + vec2(float(x), float(y)) * offset, 0.0, 1.0);\n"
           "                vec3 sCol = texture(colortex0, sUV).rgb;\n"
           "                float lum = luminance(sCol);\n"
           "                float bright = clamp((lum - 0.58) / 0.42, 0.0, 1.0);\n"
           "                float w = (x == 0 && y == 0) ? 0.28 : ((x == 0 || y == 0) ? 0.13 : 0.05);\n"
           "                bloomSum += sCol * (bright * bright) * w;\n"
           "                totalWeight += w;\n"
           "            }\n"
           "        }\n"
           "        vec3 bloomCol = bloomSum / max(totalWeight, 0.001);\n"
           "        color += bloomCol * 0.50;\n"
           "    }\n"
           "\n"
           "    // MakeUp-UltraFast Volumetric Godrays / Lightshafts\n"
           "    if (u_godrays > 0) {\n"
           "        vec2 sunScreenPos = vec2(0.68, 0.82);\n"
           "        color += calculateGodrays(colortex0, uv, sunScreenPos, u_time);\n"
           "    }\n"
           "\n"
           "    // MakeUp-UltraFast Water Reflection & Tint\n"
           "    if (u_waterReflect > 0) {\n"
           "        if (color.b > color.r * 1.12 && color.b > color.g * 0.95 && uv.y < 0.52) {\n"
           "            vec3 waterTint = vec3(0.04, 0.16, 0.20);\n"
           "            color = mix(color, waterTint + color * 0.88, 0.28);\n"
           "            float shimmer = sin(uv.x * 38.0 + u_time * 2.2) * cos(uv.y * 28.0 + u_time * 1.6) * 0.035;\n"
           "            color += vec3(shimmer);\n"
           "        }\n"
           "    }\n"
           "\n"
           "    // MakeUp-UltraFast Dynamic Soft Shadows & Contrast\n"
           "    if (u_shadows > 0) {\n"
           "        float shadowLum = luminance(color);\n"
           "        if (shadowLum < 0.32) {\n"
           "            color *= mix(0.85, 1.0, shadowLum / 0.32);\n"
           "        }\n"
           "    }\n"
           "\n"
           "    // Signature MakeUp Tonemapping (Zero manual grading interference!)\n"
           "    color = custom_sigmoid(color);\n"
           "    fragColor = vec4(clamp(color, 0.0, 1.0), base.a);\n"
           "}\n";
}

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
    GLint u_colortex1{-1};
    GLint u_viewWidth{-1};
    GLint u_viewHeight{-1};
    GLint u_frameTimeCounter{-1};
    GLint u_resolution{-1};
    GLint u_time{-1};
    GLint u_saturation{-1};
    GLint u_contrast{-1};
    GLint u_exposure{-1};
    GLint u_bloom{-1};
    GLint u_warmth{-1};
    GLint u_presetMode{-1};

    // Custom Pack / MakeUp-UltraFast specific toggles
    GLint u_godrays{-1};
    GLint u_waterReflect{-1};
    GLint u_shadows{-1};
    GLint u_bloomCustom{-1};
    GLint u_chromaAber{-1};
    GLint u_dof{-1};

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
            char log[1024];
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
            char log[1024];
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
            char log[1024];
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            Logger::instance().error("[PostProcessRenderer] Link error in " + passName + ": " + log);
            glDeleteProgram(program);
            program = 0;
            return false;
        }

        u_colortex0 = glGetUniformLocation(program, "colortex0");
        u_colortex1 = glGetUniformLocation(program, "colortex1");
        u_viewWidth = glGetUniformLocation(program, "viewWidth");
        u_viewHeight = glGetUniformLocation(program, "viewHeight");
        u_frameTimeCounter = glGetUniformLocation(program, "frameTimeCounter");
        u_resolution = glGetUniformLocation(program, "u_resolution");
        u_time = glGetUniformLocation(program, "u_time");
        u_saturation = glGetUniformLocation(program, "u_saturation");
        u_contrast = glGetUniformLocation(program, "u_contrast");
        u_exposure = glGetUniformLocation(program, "u_exposure");
        u_bloom = glGetUniformLocation(program, "u_bloom");
        u_warmth = glGetUniformLocation(program, "u_warmth");
        u_presetMode = glGetUniformLocation(program, "u_presetMode");

        u_godrays = glGetUniformLocation(program, "u_godrays");
        u_waterReflect = glGetUniformLocation(program, "u_waterReflect");
        u_shadows = glGetUniformLocation(program, "u_shadows");
        u_bloomCustom = glGetUniformLocation(program, "u_bloom");
        u_chromaAber = glGetUniformLocation(program, "u_chromaAber");
        u_dof = glGetUniformLocation(program, "u_dof");

        currentCompiledPass = passName;
        Logger::instance().info("[PostProcessRenderer] Ready active shader program: " + passName);
        return true;
    }

    void render(float width, float height, float totalTime) {
        if (program == 0) return;

        initQuad();

        // 1. Ensure copy texture matches current viewport dimensions
        if (copyTex == 0 || copyTexW != static_cast<GLsizei>(width) || copyTexH != static_cast<GLsizei>(height)) {
            if (copyTex != 0) glDeleteTextures(1, &copyTex);
            glGenTextures(1, &copyTex);
            glBindTexture(GL_TEXTURE_2D, copyTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            copyTexW = static_cast<GLsizei>(width);
            copyTexH = static_cast<GLsizei>(height);
        }

        // 2. Capture the rendered frame from FBO 0 into copyTex
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, copyTex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));

        // 3. Set render state for fullscreen post-process quad on onscreen FBO 0
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        glScissor(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glUseProgram(program);

        if (u_colortex0 >= 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, copyTex);
            glBindSampler(0, 0);
            glUniform1i(u_colortex0, 0);
        }
        if (u_colortex1 >= 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, copyTex);
            glBindSampler(1, 0);
            glUniform1i(u_colortex1, 1);
        }

        if (u_viewWidth >= 0) glUniform1f(u_viewWidth, width);
        if (u_viewHeight >= 0) glUniform1f(u_viewHeight, height);
        if (u_frameTimeCounter >= 0) glUniform1f(u_frameTimeCounter, totalTime);

        const auto& state = getArkeShaderState();
        if (u_resolution >= 0) glUniform2f(u_resolution, width, height);
        if (u_time >= 0) glUniform1f(u_time, totalTime);
        if (u_presetMode >= 0) glUniform1i(u_presetMode, state.presetMode);

        if (state.presetMode == 3) {
            // Custom Shader Pack from Disk: pass pack feature toggles, NO manual grading sliders!
            if (u_godrays >= 0) glUniform1i(u_godrays, state.customGodrays ? 1 : 0);
            if (u_waterReflect >= 0) glUniform1i(u_waterReflect, state.customWaterReflect ? 1 : 0);
            if (u_shadows >= 0) glUniform1i(u_shadows, state.customDynamicShadows ? 1 : 0);
            if (u_bloomCustom >= 0) glUniform1i(u_bloomCustom, state.customBloom ? 1 : 0);
            if (u_chromaAber >= 0) glUniform1i(u_chromaAber, state.customChromaAber ? 1 : 0);
            if (u_dof >= 0) glUniform1i(u_dof, state.customDof ? 1 : 0);
        } else {
            // Builtin Presets: apply manual live shader grading
            if (u_saturation >= 0) glUniform1f(u_saturation, state.saturation);
            if (u_contrast >= 0) glUniform1f(u_contrast, state.contrast);
            if (u_exposure >= 0) glUniform1f(u_exposure, state.exposure);
            if (u_bloom >= 0) glUniform1f(u_bloom, state.bloomStrength);
            if (u_warmth >= 0) glUniform1f(u_warmth, state.warmth);
        }

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

static std::mutex g_uniformMutex;
static std::unordered_map<int32_t, std::string> g_uniformLocationMap;
static std::unordered_set<std::string> g_loggedUniforms;

void onEglSwapBuffersPost(void* dpy, void* surface) {
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

        // Query the REAL surface dimensions directly from EGL every frame
        EGLDisplay eglDpy = static_cast<EGLDisplay>(dpy);
        EGLSurface eglSurf = static_cast<EGLSurface>(surface);
        EGLint realWidth = 0, realHeight = 0;
        if (eglDpy != EGL_NO_DISPLAY && eglSurf != EGL_NO_SURFACE) {
            eglQuerySurface(eglDpy, eglSurf, EGL_WIDTH, &realWidth);
            eglQuerySurface(eglDpy, eglSurf, EGL_HEIGHT, &realHeight);
        }
        if (realWidth <= 0 || realHeight <= 0) {
            realWidth = 1600;
            realHeight = 720;
        }

        float width = static_cast<float>(realWidth);
        float height = static_cast<float>(realHeight);

        auto& state = getArkeShaderState();
        bool reloadRequested = state.shaderReloadRequested.exchange(false);

        // Check if shaders need compilation/recompilation on this valid render-thread context
        if (reloadRequested || g_postProcessRenderer.program == 0) {
            if (state.presetMode == 3) {
                // Custom Shader Pack from Disk
                std::string currentPack = shader::ShaderPipeline::instance().getCurrentPackName();
                bool isMakeUp = (currentPack.find("MakeUp") != std::string::npos);

                if (isMakeUp) {
                    Logger::instance().info("[PostProcessRenderer] Activating MakeUp-UltraFast Native ESSL 3.20 Pipeline for: " + currentPack);
                    g_postProcessRenderer.compileStage(getBuiltinVertexShader(), getMakeUpFragmentShader(), "MakeUp_UltraFast_Pipeline");
                } else {
                    const auto& passes = shader::ShaderPipeline::instance().getPasses();
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

                    bool compiled = false;
                    if (activePass) {
                        std::string vsh = getBuiltinVertexShader();
                        compiled = g_postProcessRenderer.compileStage(vsh, activePass->fragmentESSL, activePass->name);
                    }
                    if (!compiled) {
                        Logger::instance().warn("[PostProcessRenderer] Custom pack stage failed or missing; using MakeUp-UltraFast / enhanced ESSL 3.20 pipeline");
                        g_postProcessRenderer.compileStage(getBuiltinVertexShader(), getMakeUpFragmentShader(), "MakeUp_UltraFast_Fallback");
                    }
                }
            } else {
                std::string presetName = (state.presetMode == 0) ? "Builtin_Vibrant_ACES" :
                                         (state.presetMode == 1) ? "Builtin_Golden_Hour" :
                                                                   "Builtin_Cinematic_Bloom";
                g_postProcessRenderer.compileStage(getBuiltinVertexShader(), getBuiltinFragmentShader(), presetName);
            }
        }

        // 1. Render active post-processing pass ONLY if enabled AND past initial splash threshold (g_frameCount > 150)
        bool shadersEnabled = state.shadersEnabled && ConfigManager::instance().get().enabled;
        if (g_frameCount > 150 && shadersEnabled) {
            g_postProcessRenderer.render(width, height, static_cast<float>(g_totalSeconds));
        }

        // 2. Explicitly bind default onscreen framebuffer FBO 0 for Dear ImGui overlay
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        glScissor(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
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

        std::lock_guard<std::mutex> lock(g_uniformMutex);
        if (g_loggedUniforms.find(name) == g_loggedUniforms.end()) {
            g_loggedUniforms.insert(name);
            Logger::instance().info("[GLES] Bound RenderDragon Vector4 uniform: " + name);
        }
    }
}

} // namespace arke::platform
