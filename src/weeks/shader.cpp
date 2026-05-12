#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <string>
#include <cmath>

// ---------------------------
// Shader helpers
// ---------------------------
static GLuint compileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0; glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(sh, len, nullptr, log.data());
        std::cerr << "Shader compile error:\n" << log << "\n";
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint linkProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0; glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cerr << "Program link error:\n" << log << "\n";
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// ---------------------------
// Simple orbit camera
// ---------------------------
struct OrbitCamera {
    float yaw = 45.0f;     // degrees
    float pitch = 20.0f;   // degrees
    float radius = 5.0f;
    glm::vec3 target = glm::vec3(0.0f);

    glm::mat4 viewMatrix() const {
        float cy = std::cos(glm::radians(yaw));
        float sy = std::sin(glm::radians(yaw));
        float cp = std::cos(glm::radians(pitch));
        float sp = std::sin(glm::radians(pitch));
        glm::vec3 pos = target + glm::vec3(radius * cp * cy, radius * sp, radius * cp * sy);
        return glm::lookAt(pos, target, glm::vec3(0,1,0));
    }
    glm::vec3 position() const {
        float cy = std::cos(glm::radians(yaw));
        float sy = std::sin(glm::radians(yaw));
        float cp = std::cos(glm::radians(pitch));
        float sp = std::sin(glm::radians(pitch));
        return target + glm::vec3(radius * cp * cy, radius * sp, radius * cp * sy);
    }
};

static OrbitCamera gCam;
static bool gLeftMouseDown = false;
static double gLastX = 0.0, gLastY = 0.0;
// toggles
static bool gUseBlinn = true;
static bool gDebugNormals = false;
static bool lastB=false, lastN=false;

static void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
}

static void mouse_button_callback(GLFWwindow*, int button, int action, int /*mods*/) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        gLeftMouseDown = (action == GLFW_PRESS);
    }
}

static void cursor_pos_callback(GLFWwindow* window, double x, double y) {
    if (!gLeftMouseDown) { gLastX = x; gLastY = y; return; }
    double dx = x - gLastX;
    double dy = y - gLastY;
    gLastX = x; gLastY = y;

    gCam.yaw   += static_cast<float>(dx) * 0.2f;
    gCam.pitch -= static_cast<float>(dy) * 0.2f;
    gCam.pitch = glm::clamp(gCam.pitch, -89.0f, 89.0f);

    if (glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_FALSE) gLeftMouseDown = false;
}

static void scroll_callback(GLFWwindow*, double /*xoff*/, double yoff) {
    gCam.radius *= (yoff > 0.0 ? 0.9f : 1.1f);
    gCam.radius = glm::clamp(gCam.radius, 1.5f, 25.0f);
}

static void processInput(GLFWwindow* win, float dt) {
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(win, 1);

    // optional keyboard orbit adjust
    float speed = 3.0f;
    if (glfwGetKey(win, GLFW_KEY_LEFT)  == GLFW_PRESS) gCam.yaw   -= speed * dt * 60.0f;
    if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) gCam.yaw   += speed * dt * 60.0f;
    if (glfwGetKey(win, GLFW_KEY_UP)    == GLFW_PRESS) gCam.pitch += speed * dt * 60.0f;
    if (glfwGetKey(win, GLFW_KEY_DOWN)  == GLFW_PRESS) gCam.pitch -= speed * dt * 60.0f;

    // toggles with edge detection
    static bool lastB = false, lastN = false;
    bool b = glfwGetKey(win, GLFW_KEY_B) == GLFW_PRESS;
    bool n = glfwGetKey(win, GLFW_KEY_N) == GLFW_PRESS;
    if (b && !lastB) gUseBlinn = !gUseBlinn;
    if (n && !lastN) gDebugNormals = !gDebugNormals;
    lastB = b; lastN = n;
}

// ---------------------------
// Cube mesh: position + normal (normal unused in Part 1, but keep for continuity)
// ---------------------------
static GLuint createCubeVAO() {
    float v[] = {
        // pos                 // normal
        // +X
        1, -1, -1,  1,0,0,   1,  1, -1,  1,0,0,   1,  1,  1,  1,0,0,
        1, -1, -1,  1,0,0,   1,  1,  1,  1,0,0,   1, -1,  1,  1,0,0,
        // -X
       -1, -1,  1, -1,0,0,  -1,  1,  1, -1,0,0,  -1,  1, -1, -1,0,0,
       -1, -1,  1, -1,0,0,  -1,  1, -1, -1,0,0,  -1, -1, -1, -1,0,0,
        // +Y
       -1,  1, -1, 0,1,0,    1,  1, -1, 0,1,0,    1,  1,  1, 0,1,0,
       -1,  1, -1, 0,1,0,    1,  1,  1, 0,1,0,   -1,  1,  1, 0,1,0,
        // -Y
       -1, -1,  1, 0,-1,0,   1, -1,  1, 0,-1,0,   1, -1, -1, 0,-1,0,
       -1, -1,  1, 0,-1,0,   1, -1, -1, 0,-1,0,  -1, -1, -1, 0,-1,0,
        // +Z
        1, -1,  1, 0,0,1,    1,  1,  1, 0,0,1,   -1,  1,  1, 0,0,1,
        1, -1,  1, 0,0,1,   -1,  1,  1, 0,0,1,   -1, -1,  1, 0,0,1,
        // -Z
       -1, -1, -1, 0,0,-1,  -1,  1, -1, 0,0,-1,   1,  1, -1, 0,0,-1,
       -1, -1, -1, 0,0,-1,   1,  1, -1, 0,0,-1,   1, -1, -1, 0,0,-1,
    };

    GLuint vao=0, vbo=0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return vao;
}

// ---------------------------
// Part 1 shaders: transform only + constant color
// ---------------------------
static const char* kVS_part1 = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal; // unused in Part 1

uniform mat4 uM;
uniform mat4 uV;
uniform mat4 uP;
uniform mat3 uNormalMat;
out vec3 vWorldPos;
out vec3 vWorldNormal;

void main() {
    vec4 wp = uM * vec4(aPos,1.0);
    vWorldPos = wp.xyz;
    vWorldNormal = normalize(uNormalMat * aNormal);
    gl_Position = uP * uV * wp;

}
)GLSL";

static const char* kFS_part1 = R"GLSL(
#version 330 core
in vec3 vWorldPos;
in vec3 vWorldNormal;
out vec4 FragColor;

uniform vec3 uViewPos;
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
uniform vec3 uKa;
uniform vec3 uKd;
uniform vec3 uKs;   // specular reflectance
uniform float uShininess;
uniform bool uUseBlinn;
uniform bool uDebugNormals;

void main() {
    vec3 N = normalize(vWorldNormal);
    // Debug normals visualization
    if (uDebugNormals) {
        FragColor = vec4(N * 0.5 + 0.5, 1.0);
        return;
    }
    vec3 L = normalize(uLightPos - vWorldPos);
    vec3 V = normalize(uViewPos - vWorldPos);
    float spec = 0.0;

    vec3 ambient = uKa * uAmbientColor;
    float ndotl = max(dot(N,L), 0.0);
    vec3 diffuse = uKd * ndotl * uLightColor;
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
    FragColor = vec4(ambient + diffuse + specular, 1.0);

}
)GLSL";

int main() {
    // GLFW init
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(800, 800, "Practice Part 1 - Scene & Controls", nullptr, nullptr);
    if (!win) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);

    glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);
    glfwSetMouseButtonCallback(win, mouse_button_callback);
    glfwSetCursorPosCallback(win, cursor_pos_callback);
    glfwSetScrollCallback(win, scroll_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n";
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    GLuint prog = linkProgram(kVS_part1, kFS_part1);
    if (!prog) return -1;

    GLuint cubeVAO = createCubeVAO();

    glUseProgram(prog);
    GLint uM = glGetUniformLocation(prog, "uM");
    GLint uV = glGetUniformLocation(prog, "uV");
    GLint uP = glGetUniformLocation(prog, "uP");
    GLint uNormalMat = glGetUniformLocation(prog,"uNormalMat");
    GLint uColor = glGetUniformLocation(prog, "uColor");

    GLint uViewPos = glGetUniformLocation(prog, "uViewPos");
    GLint uLightPos = glGetUniformLocation(prog, "uLightPos");
    GLint uLightColor = glGetUniformLocation(prog, "uLightColor");
    GLint uAmbientColor = glGetUniformLocation(prog, "uAmbientColor");
    GLint uShininess = glGetUniformLocation(prog, "uShininess");
    GLint uKs = glGetUniformLocation(prog, "uKs");
    GLint uKa = glGetUniformLocation(prog, "uKa");
    GLint uKd = glGetUniformLocation(prog, "uKd");
    GLint uSh = glGetUniformLocation(prog, "uSh");

    GLint uUseBlinn = glGetUniformLocation(prog, "uUseBlinn");
    GLint uDebugNormals = glGetUniformLocation(prog, "uDebugNormals");
    float lastTime = (float)glfwGetTime();
    //materials defaults
    glm::vec3 Ka(0.15f);
    glm::vec3 Kd(0.9f, 0.6f, 0.3f);
    glm::vec3 Ks(0.7f);
    float shininess = 64.0f;
    //light default
    glm::vec3 lightColor(1.0f);
    glm::vec3 ambientColor(0.2f);    

    while (!glfwWindowShouldClose(win)) {
        float t = (float)glfwGetTime();
        float dt = t - lastTime;
        lastTime = t;

        processInput(win, dt);

        int w,h; glfwGetFramebufferSize(win, &w, &h);

        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //Matrices
        glm::mat4 M(1.0f);
        M = glm::rotate(M, t * glm::radians(30.0f), glm::vec3(0,1,0));
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(M)));

        glm::mat4 V = gCam.viewMatrix();
        glm::vec3 viewPos = gCam.position();

        glm::mat4 P = glm::perspective(glm::radians(60.0f), (float)w/(float)h, 0.1f, 100.0f);
        
        glm::vec3 lightPos=glm::vec3(2.5f * std::cos(t), 1.8f, 2.5f * std::sin(t));
        // animate (optional):
        // lightPos = {2.5f*cos(t), 1.8f, 2.5f*sin(t)};

        glUseProgram(prog);
        glUniformMatrix4fv(uM, 1, GL_FALSE, glm::value_ptr(M));
        glUniformMatrix4fv(uV, 1, GL_FALSE, glm::value_ptr(V));
        glUniformMatrix4fv(uP, 1, GL_FALSE, glm::value_ptr(P));
        glUniformMatrix3fv(uNormalMat,1,GL_FALSE,glm::value_ptr(normalMat));

        glUniform3fv(uLightPos,1,value_ptr(lightPos));
        glUniform3fv(uLightColor,1,value_ptr(lightColor));
        glUniform3fv(uAmbientColor,1,value_ptr(ambientColor));
        glUniform3fv(uKa,1,value_ptr(Ka));
        glUniform3fv(uKd,1,value_ptr(Kd));
        glUniform3fv(uViewPos,1,value_ptr(viewPos));
        glUniform3fv(uKs,1,value_ptr(Ks));
        glUniform1f(uShininess, shininess);
        glUniform1i(uUseBlinn, gUseBlinn ? 1 : 0);
        glUniform1i(uDebugNormals, gDebugNormals ? 1 : 0);
        // constant color (no lighting yet)
        //glUniform3f(uColor, 0.9f, 0.6f, 0.3f);

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
