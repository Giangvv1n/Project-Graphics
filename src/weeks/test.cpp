// =============================================================================
//  lecture4practice4_shadows.cpp  –  v2  (fixes + dual-cube GI)
//
//  Controls:
//    TAB          – cycle shadow technique (Shadow Map | Shadow Volume | AO)
//    B            – toggle Blinn / Phong
//    N            – debug normals
//    Mouse drag   – orbit camera
//    Scroll       – zoom
//    Arrow keys   – orbit camera (keyboard)
// =============================================================================
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <cmath>
#include <random>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Shadow mode
// ─────────────────────────────────────────────────────────────────────────────
enum ShadowMode { SHADOW_MAP = 0, SHADOW_VOLUME = 1, PATH_TRACE_AO = 2 };
static ShadowMode gShadowMode = SHADOW_MAP;
static bool       gLastTab    = false;

// ─────────────────────────────────────────────────────────────────────────────
//  Shadow-map resources
// ─────────────────────────────────────────────────────────────────────────────
static const int SHADOW_W = 2048, SHADOW_H = 2048;
static GLuint    gShadowFBO = 0, gShadowTex = 0;

// ─────────────────────────────────────────────────────────────────────────────
//  AO noise texture
// ─────────────────────────────────────────────────────────────────────────────
static GLuint gNoiseTexAO = 0;

// =============================================================================
//  Texture loader
// =============================================================================
static GLuint loadTexture2D(const char* path) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    float maxAniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, maxAniso);
    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &ch, 0);
    if (!data) { std::cerr << "Failed to load: " << path << "\n"; return 0; }
    GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

// =============================================================================
//  Shader helpers
// =============================================================================
static GLuint compileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0; glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(sh, len, nullptr, log.data());
        std::cerr << "Shader error:\n" << log << "\n";
        glDeleteShader(sh); return 0;
    }
    return sh;
}
static GLuint linkProgram(const char* vsSrc, const char* fsSrc,
                          const char* gsSrc = nullptr) {
    GLuint vs = compileShader(GL_VERTEX_SHADER,   vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    GLuint gs = gsSrc ? compileShader(GL_GEOMETRY_SHADER, gsSrc) : 0;
    if (!vs || !fs || (gsSrc && !gs)) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    if (gs) glAttachShader(prog, gs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs); if (gs) glDeleteShader(gs);
    GLint ok = 0; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0; glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cerr << "Link error:\n" << log << "\n";
        glDeleteProgram(prog); return 0;
    }
    return prog;
}

// =============================================================================
//  Orbit camera
// =============================================================================
struct OrbitCamera {
    float yaw = 35.0f, pitch = 22.0f, radius = 7.5f;
    glm::vec3 target{0.5f, 0.0f, 0.0f};   // centre between the two cubes

    glm::mat4 viewMatrix() const {
        return glm::lookAt(position(), target, glm::vec3(0,1,0));
    }
    glm::vec3 position() const {
        float cy = std::cos(glm::radians(yaw)),   sy = std::sin(glm::radians(yaw));
        float cp = std::cos(glm::radians(pitch)), sp = std::sin(glm::radians(pitch));
        return target + glm::vec3(radius*cp*cy, radius*sp, radius*cp*sy);
    }
};

static OrbitCamera gCam;
static bool   gLeftMouseDown = false;
static double gLastX = 0, gLastY = 0;
static bool   gUseBlinn = true, gDebugNormals = false;

static void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
}
static void mouse_button_callback(GLFWwindow*, int btn, int action, int) {
    if (btn == GLFW_MOUSE_BUTTON_LEFT) gLeftMouseDown = (action == GLFW_PRESS);
}
static void cursor_pos_callback(GLFWwindow* win, double x, double y) {
    if (!gLeftMouseDown) { gLastX = x; gLastY = y; return; }
    gCam.yaw   += (float)(x - gLastX) * 0.2f;
    gCam.pitch += (float)(y - gLastY) * 0.2f;
    gCam.pitch  = glm::clamp(gCam.pitch, -89.0f, 89.0f);
    gLastX = x; gLastY = y;
    if (!glfwGetWindowAttrib(win, GLFW_FOCUSED)) gLeftMouseDown = false;
}
static void scroll_callback(GLFWwindow*, double, double yoff) {
    gCam.radius *= (yoff > 0 ? 0.9f : 1.1f);
    gCam.radius  = glm::clamp(gCam.radius, 1.5f, 30.0f);
}
static void processInput(GLFWwindow* win, float dt) {
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(win, 1);

    float spd = 3.0f * dt * 60.0f;
    if (glfwGetKey(win, GLFW_KEY_LEFT)  == GLFW_PRESS) gCam.yaw   -= spd;
    if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) gCam.yaw   += spd;
    if (glfwGetKey(win, GLFW_KEY_UP)    == GLFW_PRESS) gCam.pitch += spd;
    if (glfwGetKey(win, GLFW_KEY_DOWN)  == GLFW_PRESS) gCam.pitch -= spd;

    static bool lastB=false, lastN=false;
    bool b = glfwGetKey(win, GLFW_KEY_B) == GLFW_PRESS;
    bool n = glfwGetKey(win, GLFW_KEY_N) == GLFW_PRESS;
    if (b && !lastB) gUseBlinn     = !gUseBlinn;
    if (n && !lastN) gDebugNormals = !gDebugNormals;
    lastB = b; lastN = n;

    bool tab = glfwGetKey(win, GLFW_KEY_TAB) == GLFW_PRESS;
    if (tab && !gLastTab) {
        gShadowMode = (ShadowMode)((gShadowMode + 1) % 3);
        const char* names[] = {
            "Shadow Mapping (PCF)",
            "Shadow Volume (z-fail stencil)",
            "AO Hemisphere"
        };
        std::cout << "[Shadow] -> " << names[gShadowMode] << "\n";
    }
    gLastTab = tab;
}

// =============================================================================
//  Geometry builders
// =============================================================================

static GLuint createCubeVAO() {
    // pos(3) + normal(3) + uv(2) per vertex, 36 vertices = 12 triangles
    float v[] = {
        // +X face
        1,-1,-1, 1,0,0, 0,0,  1, 1,-1, 1,0,0, 1,0,  1, 1, 1, 1,0,0, 1,1,
        1,-1,-1, 1,0,0, 0,0,  1, 1, 1, 1,0,0, 1,1,  1,-1, 1, 1,0,0, 0,1,
        // -X face
       -1,-1, 1,-1,0,0, 0,0, -1, 1, 1,-1,0,0, 1,0, -1, 1,-1,-1,0,0, 1,1,
       -1,-1, 1,-1,0,0, 0,0, -1, 1,-1,-1,0,0, 1,1, -1,-1,-1,-1,0,0, 0,1,
        // +Y face
       -1, 1,-1, 0,1,0, 0,0,  1, 1,-1, 0,1,0, 1,0,  1, 1, 1, 0,1,0, 1,1,
       -1, 1,-1, 0,1,0, 0,0,  1, 1, 1, 0,1,0, 1,1, -1, 1, 1, 0,1,0, 0,1,
        // -Y face
       -1,-1, 1, 0,-1,0, 0,0,  1,-1, 1, 0,-1,0, 1,0,  1,-1,-1, 0,-1,0, 1,1,
       -1,-1, 1, 0,-1,0, 0,0,  1,-1,-1, 0,-1,0, 1,1, -1,-1,-1, 0,-1,0, 0,1,
        // +Z face
        1,-1, 1, 0,0,1, 0,0,  1, 1, 1, 0,0,1, 1,0, -1, 1, 1, 0,0,1, 1,1,
        1,-1, 1, 0,0,1, 0,0, -1, 1, 1, 0,0,1, 1,1, -1,-1, 1, 0,0,1, 0,1,
        // -Z face
       -1,-1,-1, 0,0,-1, 0,0, -1, 1,-1, 0,0,-1, 1,0,  1, 1,-1, 0,0,-1, 1,1,
       -1,-1,-1, 0,0,-1, 0,0,  1, 1,-1, 0,0,-1, 1,1,  1,-1,-1, 0,0,-1, 0,1,
    };
    GLuint vao=0, vbo=0;
    glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*4,(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*4,(void*)(3*4));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*4,(void*)(6*4));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    return vao;
}

static GLuint createGroundVAO() {
    // Large flat quad at y = -1.01 so it sits just below the unit-cube bottom
    const float Y = -1.01f, S = 10.0f;
    float v[] = {
        -S,Y,-S, 0,1,0, 0,0,
         S,Y,-S, 0,1,0, 4,0,
         S,Y, S, 0,1,0, 4,4,
        -S,Y,-S, 0,1,0, 0,0,
         S,Y, S, 0,1,0, 4,4,
        -S,Y, S, 0,1,0, 0,4,
    };
    GLuint vao=0,vbo=0;
    glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*4,(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*4,(void*)(3*4));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*4,(void*)(6*4));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    return vao;
}

// Full-screen NDC quad for the shadow-volume dark overlay
static GLuint createFSQuadVAO() {
    float v[] = {-1,-1, 1,-1, 1,1, -1,-1, 1,1, -1,1};
    GLuint vao=0,vbo=0;
    glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,(void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return vao;
}

// =============================================================================
//  Resource creators
// =============================================================================
static void createShadowMapFBO() {
    glGenFramebuffers(1,&gShadowFBO);
    glGenTextures(1,&gShadowTex);
    glBindTexture(GL_TEXTURE_2D,gShadowTex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT32F,
                 SHADOW_W,SHADOW_H,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
    // Use LINEAR filter so hardware PCF (sampler2DShadow) bilinearly blends
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
    float border[]={1,1,1,1};
    glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);
    // Enable hardware depth comparison (required for sampler2DShadow)
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_FUNC,GL_LEQUAL);
    glBindFramebuffer(GL_FRAMEBUFFER,gShadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,gShadowTex,0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Shadow FBO incomplete!\n";
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}

static void createNoiseTexture() {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> d(-1.0f,1.0f);
    float noise[16*3];
    for(int i=0;i<16;i++){
        noise[i*3]=d(rng); noise[i*3+1]=d(rng); noise[i*3+2]=0.0f;
    }
    glGenTextures(1,&gNoiseTexAO);
    glBindTexture(GL_TEXTURE_2D,gNoiseTexAO);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB16F,4,4,0,GL_RGB,GL_FLOAT,noise);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
}

static GLuint makeCheckerTex(unsigned char r1=210,unsigned char g1=210,
                              unsigned char b1=210,
                              unsigned char r2=100,unsigned char g2=110,
                              unsigned char b2=140) {
    // 128x128 checkerboard, 16px squares
    const int SZ=128;
    unsigned char buf[SZ*SZ*3];
    for(int y=0;y<SZ;y++) for(int x=0;x<SZ;x++){
        bool light = ((x/16+y/16)%2==0);
        buf[(y*SZ+x)*3+0] = light?r1:r2;
        buf[(y*SZ+x)*3+1] = light?g1:g2;
        buf[(y*SZ+x)*3+2] = light?b1:b2;
    }
    GLuint t; glGenTextures(1,&t);
    glBindTexture(GL_TEXTURE_2D,t);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,SZ,SZ,0,GL_RGB,GL_UNSIGNED_BYTE,buf);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    return t;
}

// =============================================================================
//  SHADERS
// =============================================================================

// ─── Depth-only pass (shadow map generation) ─────────────────────────────────
// FIX v2: uLightVP and uM are separate uniforms.
// Previously the code baked uM into the lightMVP on the CPU, so the VS could
// not support multiple objects with one glUniform call.  Now we pass
// uLightVP once, and uM per object — exactly like the main pass.
static const char* kVS_depth = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uLightVP;  // light projection * view  (no model transform!)
uniform mat4 uM;        // model matrix — set per-object before each draw call
void main() {
    gl_Position = uLightVP * uM * vec4(aPos, 1.0);
}
)GLSL";

static const char* kFS_depth = R"GLSL(
#version 330 core
void main() {}  // depth written automatically by rasteriser
)GLSL";

// ─── Main pass vertex shader ──────────────────────────────────────────────────
// FIX v2: uLightVP is the view-projection only; the model transform is uM.
// The bias matrix (maps NDC [-1,1] to UV+depth [0,1]) is baked here so the
// FS receives a ready-to-use texture coordinate in vShadowCoord.
static const char* kVS_main = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;

uniform mat4 uM, uV, uP;
uniform mat3 uNormalMat;
uniform mat4 uLightVP;   // light view-projection WITHOUT model matrix

out vec3 vWorldPos;
out vec3 vWorldNormal;
out vec2 vTexCoord;
out vec4 vShadowCoord;

void main() {
    vec4 wp      = uM * vec4(aPos, 1.0);
    vWorldPos    = wp.xyz;
    vWorldNormal = normalize(uNormalMat * aNormal);
    vTexCoord    = aTexCoord;

    // Bias matrix: NDC [-1,1]^3 -> [0,1]^3 for shadow texture lookup.
    // GLSL mat4() fills COLUMNS, not rows — each vec4 arg is one column:
    //   col0        col1        col2        col3 (translation)
    mat4 bias = mat4(
        vec4(0.5, 0.0, 0.0, 0.0),
        vec4(0.0, 0.5, 0.0, 0.0),
        vec4(0.0, 0.0, 0.5, 0.0),
        vec4(0.5, 0.5, 0.5, 1.0)
    );
    vShadowCoord = bias * uLightVP * wp;

    gl_Position = uP * uV * wp;
}
)GLSL";

// ─── Main pass fragment shader ────────────────────────────────────────────────
static const char* kFS_main = R"GLSL(
#version 330 core
in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vTexCoord;
in vec4 vShadowCoord;

out vec4 FragColor;

uniform sampler2D       uTexture;
// FIX v2: sampler2DShadow enables hardware PCF depth comparison.
// The driver handles bilinear filtering of the depth comparisons automatically.
// You MUST set GL_TEXTURE_COMPARE_MODE = GL_COMPARE_REF_TO_TEXTURE on the
// texture object, or texture() always returns 1.0 (no shadow).
uniform sampler2DShadow uShadowMap;
uniform sampler2D       uNoiseTex;

uniform vec3  uViewPos, uLightPos, uLightColor, uAmbientColor;
uniform vec3  uKa, uKd, uKs;
uniform float uShininess;
uniform bool  uUseBlinn, uDebugNormals;
uniform int   uShadowMode;
uniform vec2  uNoiseScale;

// ── Global illumination uniforms ─────────────────────────────────────────────
// Single-bounce indirect light: cube B illuminates cube A and the ground.
// The CPU sets uGILightPos to cube B's world-space centre each frame.
uniform vec3  uGILightPos;    // world position of the "bounce light" (cube B)
uniform vec3  uGILightColor;  // colour of the bounce (can be tinted to cube B's albedo)
uniform float uGIStrength;    // overall GI contribution scale [0..1]

// ── Poisson disk offsets for PCF ────────────────────────────────────────────
const vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624,-0.39906216), vec2( 0.94558609,-0.76890725),
    vec2(-0.09418410,-0.92938870), vec2( 0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232,-0.87912464),
    vec2(-0.38277543, 0.27676845), vec2( 0.97484398, 0.75648379),
    vec2( 0.44323325,-0.97511554), vec2( 0.53742981,-0.47373420),
    vec2(-0.26496911,-0.41893023), vec2( 0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
    vec2( 0.19984126, 0.78641367), vec2( 0.14383161,-0.14100790)
);

// ── Shadow-map PCF factor ────────────────────────────────────────────────────
// FIX v2: Uses textureProj() on sampler2DShadow.
// textureProj divides .xy by .w for us, and the hardware comparator
// checks .z/.w against the stored depth.  We add the Poisson offsets in
// texture space (after projection) to get 16-sample soft shadow.
// Slope-scaled bias adapts to grazing-angle shadow acne automatically.
float shadowMapFactor() {
    // Perspective divide to get texture UV + reference depth in [0,1]
    vec3 sc = vShadowCoord.xyz / vShadowCoord.w;

    // Outside the light frustum -> treat as fully lit
    if (sc.x < 0.0 || sc.x > 1.0 ||
        sc.y < 0.0 || sc.y > 1.0 ||
        sc.z > 1.0) return 1.0;

    // Slope-scaled bias: larger bias at grazing angles to prevent shadow acne
    vec3  N    = normalize(vWorldNormal);
    vec3  L    = normalize(uLightPos - vWorldPos);
    float bias = max(0.003 * (1.0 - dot(N, L)), 0.0008);

    float shadow  = 0.0;
    float texel   = 1.0 / 2048.0;
    for (int i = 0; i < 16; i++) {
        // texture() on sampler2DShadow: third component is the reference depth.
        // Returns 1.0 if sc.z - bias <= stored_depth (lit), else 0.0 (shadow).
        shadow += texture(uShadowMap,
                    vec3(sc.xy + poissonDisk[i] * texel * 1.5,
                         sc.z - bias));
    }
    return shadow / 16.0;
}

// ── Hemisphere AO factor ─────────────────────────────────────────────────────
// Approximates ambient occlusion via 8 hemisphere samples.
// Samples that land below the ground plane (y < -1.01) are considered occluded.
float ambientOcclusionFactor(vec3 N, vec3 pos) {
    vec2 noiseUV   = gl_FragCoord.xy * uNoiseScale;
    vec3 randVec   = normalize(texture(uNoiseTex, noiseUV).xyz);
    vec3 tangent   = normalize(randVec - N * dot(randVec, N));
    vec3 bitangent = cross(N, tangent);
    mat3 TBN = mat3(tangent, bitangent, N);

    const vec3 sampleDirs[8] = vec3[](
        vec3( 0.1, 0.1, 0.9), vec3(-0.1, 0.1, 0.9),
        vec3( 0.5, 0.5, 0.7), vec3(-0.5, 0.5, 0.7),
        vec3( 0.7,-0.3, 0.6), vec3(-0.7,-0.3, 0.6),
        vec3( 0.3,-0.8, 0.5), vec3(-0.3,-0.8, 0.5)
    );
    float ao = 0.0;
    float radius = 0.6;
    for (int i = 0; i < 8; i++) {
        vec3 sp = pos + TBN * normalize(sampleDirs[i]) * radius;
        if (sp.y < -1.01) ao += 1.0;
    }
    return 1.0 - (ao / 8.0) * 0.75;
}

void main() {
    vec3 N = normalize(vWorldNormal);
    if (uDebugNormals) { FragColor = vec4(N*0.5+0.5, 1.0); return; }

    vec3 texColor = texture(uTexture, vTexCoord).rgb;
    vec3 L = normalize(uLightPos - vWorldPos);
    vec3 V = normalize(uViewPos  - vWorldPos);

    // ── Phong / Blinn-Phong lighting (primary light) ─────────────────────────
    vec3  ambient = uKa * uAmbientColor * texColor;
    float ndotl   = max(dot(N, L), 0.0);
    vec3  diffuse = uKd * ndotl * uLightColor * texColor;

    float spec = 0.0;
    if (ndotl > 0.0) {
        if (uUseBlinn) {
            vec3 H = normalize(L + V);
            spec = pow(max(dot(N, H), 0.0), uShininess);
        } else {
            vec3 R = reflect(-L, N);
            spec = pow(max(dot(R, V), 0.0), uShininess);
        }
    }
    vec3 specular = uKs * spec * uLightColor;

    // ── Global illumination (single bounce from cube B) ──────────────────────
    // Cube B acts as a large Lambertian area emitter whose centre is uGILightPos.
    // The bounce colour is tinted by uGILightColor (set to cube B's dominant hue).
    // Note: GI bounce is NOT multiplied by the shadow factor — indirect light
    // wraps around occluders, so shadowed areas still receive bounce illumination.
    vec3  Lgi     = normalize(uGILightPos - vWorldPos);
    float ndotgi  = max(dot(N, Lgi), 0.0);
    // Distance falloff (approximate 1/d^2 clamped so it stays visible)
    float dist    = length(uGILightPos - vWorldPos);
    float falloff = 1.0 / max(dist * dist * 0.25, 1.0);
    vec3  giBounce = uKd * ndotgi * uGILightColor * texColor * uGIStrength * falloff;

    // ── Shadow factor ────────────────────────────────────────────────────────
    float shadowFactor = 1.0;
    if      (uShadowMode == 0) shadowFactor = shadowMapFactor();
    else if (uShadowMode == 2) shadowFactor = ambientOcclusionFactor(N, vWorldPos);
    // mode 1: shadow handled externally via stencil; FS sees full lighting.

    // ── Compose ──────────────────────────────────────────────────────────────
    vec3 direct = ambient + (diffuse + specular) * shadowFactor;
    FragColor   = vec4(direct + giBounce, 1.0);
}
)GLSL";

// ─── Shadow-volume vertex shader ─────────────────────────────────────────────
static const char* kVS_svol = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uM;
out vec3 vWorldPos;
void main() { vWorldPos = (uM * vec4(aPos, 1.0)).xyz; }
)GLSL";

// ─── Shadow-volume geometry shader ───────────────────────────────────────────
// z-fail (Carmack's Reverse):
//   Light-facing triangles are identified.  For each such triangle we emit:
//   (a) front cap   – original triangle
//   (b) back cap    – extruded to "infinity", reversed winding
//   (c) 3 side quads connecting silhouette edges from front to back
//   The stencil rules (DECR_WRAP on front-depth-fail, INCR_WRAP on back-depth-fail)
//   guarantee stencil == 0 in lit regions and != 0 in shadow.
static const char* kGS_svol = R"GLSL(
#version 330 core
layout(triangles) in;
layout(triangle_strip, max_vertices=18) out;

in  vec3 vWorldPos[];
uniform vec3 uLightPos;
uniform mat4 uVP;

void emitSideQuad(vec3 a, vec3 b) {
    vec3 da = normalize(a - uLightPos) * 40.0;
    vec3 db = normalize(b - uLightPos) * 40.0;
    gl_Position = uVP * vec4(a,      1.0); EmitVertex();
    gl_Position = uVP * vec4(a + da, 1.0); EmitVertex();
    gl_Position = uVP * vec4(b,      1.0); EmitVertex();
    gl_Position = uVP * vec4(b + db, 1.0); EmitVertex();
    EndPrimitive();
}

void main() {
    vec3 A = vWorldPos[0], B = vWorldPos[1], C = vWorldPos[2];
    if (dot(cross(B-A, C-A), uLightPos - A) <= 0.0) return; // back-facing: skip

    // Front cap
    gl_Position = uVP * vec4(A, 1.0); EmitVertex();
    gl_Position = uVP * vec4(B, 1.0); EmitVertex();
    gl_Position = uVP * vec4(C, 1.0); EmitVertex();
    EndPrimitive();

    // Back cap (reversed winding so it faces outward)
    vec3 Ae = A + normalize(A - uLightPos)*40.0;
    vec3 Be = B + normalize(B - uLightPos)*40.0;
    vec3 Ce = C + normalize(C - uLightPos)*40.0;
    gl_Position = uVP * vec4(Ae, 1.0); EmitVertex();
    gl_Position = uVP * vec4(Ce, 1.0); EmitVertex();
    gl_Position = uVP * vec4(Be, 1.0); EmitVertex();
    EndPrimitive();

    emitSideQuad(A, B);
    emitSideQuad(B, C);
    emitSideQuad(C, A);
}
)GLSL";

static const char* kFS_svol = R"GLSL(
#version 330 core
out vec4 FragColor;
void main() { FragColor = vec4(0.0); } // only writes stencil
)GLSL";

// ─── Shadow-volume dark overlay shaders ──────────────────────────────────────
// FIX v2: gl_Position.z = 0.9999 means the quad is just before the far plane.
// With GL_DEPTH_FUNC = GL_LESS, it loses the depth test against the cubes
// (whose fragments are at z < 0.9999) — so the cube interiors are NEVER
// covered by the dark overlay.  The overlay wins only against the sky/floor
// where nothing closer was drawn.  Combined with GL_STENCIL_FUNC = GL_NOTEQUAL 0,
// only the shadowed portion of the floor/background receives the darkening.
static const char* kVS_dark = R"GLSL(
#version 330 core
layout(location=0) in vec2 aPos;
void main() {
    // z = 0.9999 → loses GL_LESS depth test against any solid geometry
    // but still covers the background where no geometry exists.
    gl_Position = vec4(aPos, 0.9999, 1.0);
}
)GLSL";

static const char* kFS_dark = R"GLSL(
#version 330 core
out vec4 FragColor;
void main() { FragColor = vec4(0.0, 0.0, 0.0, 0.55); }  // semi-transparent shadow tint
)GLSL";

// =============================================================================
//  main()
// =============================================================================
int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8); // required for shadow volume

    GLFWwindow* win = glfwCreateWindow(800, 800,
        "Shadows + GI  [TAB=cycle | B=Blinn | N=normals]", nullptr, nullptr);
    if (!win) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);
    glfwSetMouseButtonCallback(win,    mouse_button_callback);
    glfwSetCursorPosCallback(win,      cursor_pos_callback);
    glfwSetScrollCallback(win,         scroll_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD failed\n"; return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // ── Compile and link programs ─────────────────────────────────────────────
    GLuint progMain  = linkProgram(kVS_main,  kFS_main);
    GLuint progDepth = linkProgram(kVS_depth, kFS_depth);
    GLuint progSvol  = linkProgram(kVS_svol,  kFS_svol, kGS_svol);
    GLuint progDark  = linkProgram(kVS_dark,  kFS_dark);
    if (!progMain || !progDepth || !progSvol || !progDark) return -1;

    // ── Geometry ─────────────────────────────────────────────────────────────
    GLuint cubeVAO   = createCubeVAO();
    GLuint groundVAO = createGroundVAO();
    GLuint fsqVAO    = createFSQuadVAO();

    // ── Textures ─────────────────────────────────────────────────────────────
    // Cube A: load your VNUIS logo (fallback to grey if missing)
    GLuint texCubeA = loadTexture2D("assets/vnuis_logo.png");
    if (!texCubeA) texCubeA = makeCheckerTex(200,200,200, 80,80,80);

    // Cube B: vivid cyan checkerboard — bright so shadow is clearly visible
    GLuint texCubeB  = makeCheckerTex(80, 220, 240, 20, 100, 140);
    // Ground: neutral grey checkerboard
    GLuint texGround = makeCheckerTex(200,200,200, 110,110,110);

    createShadowMapFBO();
    createNoiseTexture();

    // ── Cache uniform locations (progMain) ───────────────────────────────────
    auto U = [&](const char* n){ return glGetUniformLocation(progMain, n); };
    GLint uM_   = U("uM"),     uV_  = U("uV"),   uP_  = U("uP");
    GLint uNM_  = U("uNormalMat"), uLVP_ = U("uLightVP");
    GLint uVPo_ = U("uViewPos"), uLP_ = U("uLightPos");
    GLint uLC_  = U("uLightColor"), uAC_ = U("uAmbientColor");
    GLint uKa_  = U("uKa"), uKd_ = U("uKd"), uKs_ = U("uKs");
    GLint uSh_  = U("uShininess"), uBl_ = U("uUseBlinn"), uDN_ = U("uDebugNormals");
    GLint uTex_ = U("uTexture"), uSM_  = U("uShadowMap");
    GLint uNT_  = U("uNoiseTex"), uNS_ = U("uNoiseScale");
    GLint uSMd_ = U("uShadowMode");
    GLint uGIP_ = U("uGILightPos"), uGIC_ = U("uGILightColor"), uGIS_ = U("uGIStrength");

    // ── Material and light defaults ───────────────────────────────────────────
    glm::vec3 Ka(0.10f), Kd(0.95f), Ks(0.55f*1.2);
    float shininess = 64.0f;
    glm::vec3 lightColor(1.0f*1.2, 0.95f*1.2, 0.85f*1.2);    // warm sunlight
    glm::vec3 ambientColor(0.15f, 0.17f, 0.22f); // cool dark ambient
    glm::vec3 GIColor(0.15f, 0.70f, 0.80f);      // cyan tinted bounce from cube B
    float     GIStrength = 0.35f;

    float lastTime = (float)glfwGetTime();
    std::cout << "TAB=shadow mode | B=Blinn/Phong | N=debug normals\n";
    std::cout << "Cube A rotates.  Cube B is stationary and acts as GI bounce emitter.\n";

    // =========================================================================
    //  RENDER LOOP
    // =========================================================================
    while (!glfwWindowShouldClose(win)) {
        float t  = (float)glfwGetTime();
        float dt = t - lastTime; lastTime = t;
        processInput(win, dt);

        int fbW, fbH;
        glfwGetFramebufferSize(win, &fbW, &fbH);
        if (fbW == 0 || fbH == 0) { glfwPollEvents(); continue; }

        // ── Per-frame transforms ──────────────────────────────────────────────
        // Cube A: rotating on Y-axis, centred at origin
        glm::mat4 MA(1.0f);
        MA = glm::rotate(MA, t * glm::radians(30.0f), glm::vec3(0,1,0));
        glm::mat3 normalMatA = glm::transpose(glm::inverse(glm::mat3(MA)));

        // Cube B: stationary — translate then scale only (no rotation).
        // Build T * S explicitly so the scale does NOT corrupt the translation
        // column that we later extract as cubeB_center.
        // Ground is at y=-1.01. Cube half-extent = scale = 0.85,
        // so bottom face in world = pos.y - 0.85. Set pos.y = -1.01 + 0.85 = -0.16.
        const glm::vec3 cubeB_pos(3.5f, -0.16f, 0.0f);  // resting on ground
        const float     cubeB_scale = 0.85f;             // slightly bigger
        glm::mat4 MB = glm::translate(glm::mat4(1.0f), cubeB_pos)
                     * glm::scale   (glm::mat4(1.0f), glm::vec3(cubeB_scale));
        // Normal matrix: transpose(inverse(M)).  For uniform scale this equals
        // the rotation part (identity here), but we compute it properly anyway.
        glm::mat3 normalMatB = glm::transpose(glm::inverse(glm::mat3(MB)));

        glm::mat4 Mground(1.0f);
        glm::mat3 normalMatG(1.0f);

        // Camera
        glm::mat4 V    = gCam.viewMatrix();
        glm::vec3 VP   = gCam.position();
        glm::mat4 Proj = glm::perspective(glm::radians(55.0f),
                                           (float)fbW/(float)fbH, 0.1f, 100.0f);
        glm::mat4 VPmat = Proj * V;

        // Orbiting key light (slower orbit than cube rotation)
        glm::vec3 lightPos(5.0f*std::cos(t*0.6f) + 2.25f, 4.5f,
                               5.0f*std::sin(t*0.6f));  // orbit around scene centre

        // GI source: cube B's world-space centre.
        // Use the explicit position constant rather than MB[3] (which holds a vec4).
        glm::vec3 cubeB_center = cubeB_pos;

        // ── Light-space matrix ────────────────────────────────────────────────
        // Light looks at the midpoint between the two cubes so both fit in the
        // shadow map.  The ortho frustum is sized to enclose the full scene.
        glm::vec3 lightTarget(2.25f, 0.0f, 0.0f); // midpoint between origin and cube B
        glm::mat4 lightView = glm::lookAt(lightPos, lightTarget, glm::vec3(0,1,0));
        glm::mat4 lightProj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.5f, 40.0f);
        // FIX v2: uLightVP = lightProj * lightView, NO model matrix baked in.
        // Each draw call passes its own uM separately, matching the depth VS.
        glm::mat4 lightVP = lightProj * lightView;

        // =====================================================================
        //  PASS 0 — depth-only render into shadow map
        // =====================================================================
        if (gShadowMode == SHADOW_MAP) {
            glBindFramebuffer(GL_FRAMEBUFFER, gShadowFBO);
            glViewport(0, 0, SHADOW_W, SHADOW_H);
            glClear(GL_DEPTH_BUFFER_BIT);

            // Front-face culling during depth pass eliminates shadow acne on
            // the lit faces of the casters without needing a large manual bias.
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);

            glUseProgram(progDepth);
            GLint dLVP = glGetUniformLocation(progDepth, "uLightVP");
            GLint dM   = glGetUniformLocation(progDepth, "uM");
            glUniformMatrix4fv(dLVP, 1, GL_FALSE, glm::value_ptr(lightVP));

            // Cube A into shadow map
            glUniformMatrix4fv(dM, 1, GL_FALSE, glm::value_ptr(MA));
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);

            // Cube B into shadow map (casts shadow on floor + cube A)
            glUniformMatrix4fv(dM, 1, GL_FALSE, glm::value_ptr(MB));
            glDrawArrays(GL_TRIANGLES, 0, 36);

            // Ground into shadow map
            glUniformMatrix4fv(dM, 1, GL_FALSE, glm::value_ptr(Mground));
            glBindVertexArray(groundVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glCullFace(GL_BACK);
            glDisable(GL_CULL_FACE);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, fbW, fbH);
        }

        // =====================================================================
        //  PASS 1 — full lit render
        // =====================================================================
        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glDisable(GL_STENCIL_TEST);

        glUseProgram(progMain);

        // ── Set shared uniforms once ──────────────────────────────────────────
        glUniformMatrix4fv(uV_,  1, GL_FALSE, glm::value_ptr(V));
        glUniformMatrix4fv(uP_,  1, GL_FALSE, glm::value_ptr(Proj));
        glUniformMatrix4fv(uLVP_,1, GL_FALSE, glm::value_ptr(lightVP));

        glUniform3fv(uLP_,  1, glm::value_ptr(lightPos));
        glUniform3fv(uLC_,  1, glm::value_ptr(lightColor));
        glUniform3fv(uAC_,  1, glm::value_ptr(ambientColor));
        glUniform3fv(uKa_,  1, glm::value_ptr(Ka));
        glUniform3fv(uKd_,  1, glm::value_ptr(Kd));
        glUniform3fv(uKs_,  1, glm::value_ptr(Ks));
        glUniform3fv(uVPo_, 1, glm::value_ptr(VP));
        glUniform1f (uSh_,  shininess);
        glUniform1i (uBl_,  gUseBlinn ? 1 : 0);
        glUniform1i (uDN_,  gDebugNormals ? 1 : 0);
        glUniform1i (uSMd_, (int)gShadowMode);
        glUniform2f (uNS_,  (float)fbW/4.0f, (float)fbH/4.0f);

        // GI uniforms
        glUniform3fv(uGIP_, 1, glm::value_ptr(cubeB_center));
        glUniform3fv(uGIC_, 1, glm::value_ptr(GIColor));
        glUniform1f (uGIS_, GIStrength);

        // Bind shadow map (unit 1) and noise (unit 2) — shared across all objects
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gShadowTex);
        glUniform1i(uSM_, 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gNoiseTexAO);
        glUniform1i(uNT_, 2);

        // ── Draw Cube A ───────────────────────────────────────────────────────
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texCubeA);
        glUniform1i(uTex_, 0);
        glUniformMatrix4fv(uM_,  1, GL_FALSE, glm::value_ptr(MA));
        glUniformMatrix3fv(uNM_, 1, GL_FALSE, glm::value_ptr(normalMatA));
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // ── Draw Cube B ───────────────────────────────────────────────────────
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texCubeB);
        glUniform1i(uTex_, 0);
        glUniformMatrix4fv(uM_,  1, GL_FALSE, glm::value_ptr(MB));
        glUniformMatrix3fv(uNM_, 1, GL_FALSE, glm::value_ptr(normalMatB));
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // ── Draw Ground ───────────────────────────────────────────────────────
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texGround);
        glUniform1i(uTex_, 0);
        glUniformMatrix4fv(uM_,  1, GL_FALSE, glm::value_ptr(Mground));
        glUniformMatrix3fv(uNM_, 1, GL_FALSE, glm::value_ptr(normalMatG));
        glBindVertexArray(groundVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // =====================================================================
        //  PASS 2 — shadow volume stencil + dark overlay  (mode 1 only)
        // =====================================================================
        if (gShadowMode == SHADOW_VOLUME) {

            // ── 2a: rasterise shadow volumes into stencil ─────────────────────
            // All solid geometry must already be in the depth buffer (from pass 1).
            // Colour writes OFF, depth writes OFF, depth test ON (GL_LESS).
            glEnable(GL_STENCIL_TEST);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glDepthMask(GL_FALSE);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);

            // Stencil always passes; the depth test determines +/- counting.
            glStencilFunc(GL_ALWAYS, 0, 0xFF);
            // z-fail: front face depth-fail → DECR_WRAP; back face → INCR_WRAP
            glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
            glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_INCR_WRAP, GL_KEEP);

            glDisable(GL_CULL_FACE); // must render both faces for z-fail
            glUseProgram(progSvol);

            GLint svM  = glGetUniformLocation(progSvol, "uM");
            GLint svVP = glGetUniformLocation(progSvol, "uVP");
            GLint svLP = glGetUniformLocation(progSvol, "uLightPos");
            glUniformMatrix4fv(svVP, 1, GL_FALSE, glm::value_ptr(VPmat));
            glUniform3fv(svLP, 1, glm::value_ptr(lightPos));

            // Shadow volume for cube A
            glUniformMatrix4fv(svM, 1, GL_FALSE, glm::value_ptr(MA));
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);

            // Shadow volume for cube B (B also casts shadow onto A and floor)
            glUniformMatrix4fv(svM, 1, GL_FALSE, glm::value_ptr(MB));
            glDrawArrays(GL_TRIANGLES, 0, 36);

            glEnable(GL_CULL_FACE);

            // ── 2b: dark overlay where stencil != 0 ──────────────────────────
            // FIX v2: Use GL_DEPTH_FUNC = GL_LESS (not GL_ALWAYS).
            // The dark quad has z = 0.9999 (from kVS_dark).  The cubes drew their
            // pixels at z << 0.9999 into the depth buffer in pass 1.  GL_LESS
            // means the quad's z = 0.9999 FAILS the depth test against the cubes,
            // so those pixels are skipped — shadow never appears inside/through
            // the cubes.  The quad only wins against pixels where depth > 0.9999
            // (background) or where no geometry was drawn (also depth = 1.0).
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glDepthMask(GL_FALSE);
            glDepthFunc(GL_LESS);  // FIX: NOT GL_ALWAYS — preserves cube silhouette

            glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUseProgram(progDark);
            glBindVertexArray(fsqVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            // ── Restore state ─────────────────────────────────────────────────
            glDisable(GL_BLEND);
            glDisable(GL_STENCIL_TEST);
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
        }

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}