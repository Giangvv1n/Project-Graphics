#include "weeks/Lighting.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
#include <limits>
#include <cstdint>
#include <GLFW/glfw3.h>
#include <cmath>

namespace {
#pragma pack(push, 1)
    struct STLTriangle {
        float normal[3];
        float v1[3];
        float v2[3];
        float v3[3];
        uint16_t attrByteCount;
    };
#pragma pack(pop)
}

bool Lighting::init() {
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPointSize(6.0f);
    std::cout << "Lighting + STL model\n";

    glEnable(GL_DEPTH_TEST);

    GLFWwindow* window = glfwGetCurrentContext();
    if (window) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    if (!m_shader.loadFromFiles(
        "assets/shaders/model.vert",
        "assets/shaders/model.frag")) {
        std::cerr << "Failed to load week05 shaders.\n";
        return false;
    }

    if (!loadBinarySTL("assets/models/L room-BodyPocket001.stl")) {
        std::cerr << "Failed to load STL model.\n";
        return false;
    }

    setupMesh();
    return true;
}

void Lighting::update(float dt) {
    m_time += dt;

    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) return;

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    m_cameraDistance -= 3.0f * dt;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        m_cameraDistance += 3.0f * dt;

    if (m_cameraDistance < 1.0f)  m_cameraDistance = 1.0f;
    if (m_cameraDistance > 30.0f) m_cameraDistance = 30.0f;
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if (m_firstMouse) {
        m_lastX = static_cast<float>(xpos);
        m_lastY = static_cast<float>(ypos);
        m_firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos) - m_lastX;
    float yoffset = m_lastY - static_cast<float>(ypos); // đảo chiều Y cho tự nhiên
    m_lastX = static_cast<float>(xpos);
    m_lastY = static_cast<float>(ypos);

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    m_yaw += xoffset;
    m_pitch += yoffset;

    if (m_pitch > 89.0f) m_pitch = 89.0f;
    if (m_pitch < -89.0f) m_pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_cameraFront = glm::normalize(front);
    // camera chuột
    float cameraSpeed = 3.0f * dt;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        m_cameraPos += cameraSpeed * m_cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        m_cameraPos -= cameraSpeed * m_cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        m_cameraPos -= glm::normalize(glm::cross(m_cameraFront, m_cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        m_cameraPos += glm::normalize(glm::cross(m_cameraFront, m_cameraUp)) * cameraSpeed;
}

void Lighting::render() {
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, 800, 800);

    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_shader.use();

    glm::mat4 translateMat =
        glm::translate(glm::mat4(1.0f), glm::vec3(-m_centerX, -m_centerY, -m_centerZ));

    glm::mat4 scaleMat =
        glm::scale(glm::mat4(1.0f), glm::vec3(m_modelScale));

    glm::mat4 rotateY =
        glm::rotate(glm::mat4(1.0f), m_time * 0.8f, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 rotateX =
        glm::rotate(glm::mat4(1.0f), glm::radians(-25.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    glm::mat4 model = scaleMat * translateMat;

    m_cameraPos = -m_cameraFront * m_cameraDistance;

    glm::vec3 target(0.0f, 0.0f, 0.0f);
    m_cameraPos = target - m_cameraFront * m_cameraDistance;

    glm::mat4 view = glm::lookAt(
        m_cameraPos,
        target,
        m_cameraUp
    );

    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        1.0f,
        0.1f,
        100.0f  

);

   
    GLint modelLoc = glGetUniformLocation(m_shader.id(), "uModel");
    GLint viewLoc  = glGetUniformLocation(m_shader.id(), "uView");
    GLint projLoc  = glGetUniformLocation(m_shader.id(), "uProjection");

    if (modelLoc >= 0) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    if (viewLoc  >= 0) glUniformMatrix4fv(viewLoc,  1, GL_FALSE, glm::value_ptr(view));
    if (projLoc  >= 0) glUniformMatrix4fv(projLoc,  1, GL_FALSE, glm::value_ptr(projection));

    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
    GLint normalLoc = glGetUniformLocation(m_shader.id(), "uNormalMatrix");
    if (normalLoc >= 0) {
        glUniformMatrix3fv(normalLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }

    GLint lightDirLoc   = glGetUniformLocation(m_shader.id(), "uLightDir");
    GLint viewPosLoc    = glGetUniformLocation(m_shader.id(), "uViewPos");
    GLint objColorLoc   = glGetUniformLocation(m_shader.id(), "uObjectColor");
    GLint lightColorLoc = glGetUniformLocation(m_shader.id(), "uLightColor");

    if (lightDirLoc >= 0)   glUniform3f(lightDirLoc, -0.6f, -1.0f, -0.4f);
    if (viewPosLoc >= 0) glUniform3f(viewPosLoc, m_cameraPos.x, m_cameraPos.y, m_cameraPos.z);
    if (objColorLoc >= 0)   glUniform3f(objColorLoc, 0.82f, 0.82f, 0.86f);
    if (lightColorLoc >= 0) glUniform3f(lightColorLoc, 1.0f, 1.0f, 1.0f);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
    glBindVertexArray(0);
}
bool Lighting::loadBinarySTL(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open STL file: " << path << "\n";
        return false;
    }

    char header[80];
    file.read(header, 80);

    uint32_t triangleCount = 0;
    file.read(reinterpret_cast<char*>(&triangleCount), sizeof(uint32_t));

    if (!file || triangleCount == 0) {
        std::cerr << "Invalid STL or empty STL: " << path << "\n";
        return false;
    }

    m_vertices.clear();
    m_vertices.reserve(static_cast<size_t>(triangleCount) * 3);

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();

    for (uint32_t i = 0; i < triangleCount; ++i) {
        STLTriangle tri{};
        file.read(reinterpret_cast<char*>(&tri), sizeof(STLTriangle));
        if (!file) {
            std::cerr << "Error while reading STL triangle #" << i << "\n";
            return false;
        }

        Vertex a{ tri.v1[0], tri.v1[1], tri.v1[2], tri.normal[0], tri.normal[1], tri.normal[2] };
        Vertex b{ tri.v2[0], tri.v2[1], tri.v2[2], tri.normal[0], tri.normal[1], tri.normal[2] };
        Vertex c{ tri.v3[0], tri.v3[1], tri.v3[2], tri.normal[0], tri.normal[1], tri.normal[2] };

        m_vertices.push_back(a);
        m_vertices.push_back(b);
        m_vertices.push_back(c);

        const Vertex verts[3] = { a, b, c };
        for (const auto& v : verts) {
            if (v.px < minX) minX = v.px;
            if (v.py < minY) minY = v.py;
            if (v.pz < minZ) minZ = v.pz;
            if (v.px > maxX) maxX = v.px;
            if (v.py > maxY) maxY = v.py;
            if (v.pz > maxZ) maxZ = v.pz;
        }
    }

    // Tính tâm model
    m_centerX = (minX + maxX) * 0.5f;
    m_centerY = (minY + maxY) * 0.5f;
    m_centerZ = (minZ + maxZ) * 0.5f;

    // Scale để kích thước lớn nhất ~ 2 đơn vị
    float sizeX = maxX - minX;
    float sizeY = maxY - minY;
    float sizeZ = maxZ - minZ;
    float maxSize = std::max(sizeX, std::max(sizeY, sizeZ));

    if (maxSize > 0.00001f) {
        m_modelScale = 2.0f / maxSize;
    } else {
        m_modelScale = 1.0f;
    }

    std::cout << "Loaded binary STL: " << path << "\n";
    std::cout << "Triangles: " << triangleCount << "\n";
    std::cout << "Vertices: " << m_vertices.size() << "\n";

        std::cout << "min: " << minX << ", " << minY << ", " << minZ << "\n";
    std::cout << "max: " << maxX << ", " << maxY << ", " << maxZ << "\n";
    std::cout << "size: " << sizeX << ", " << sizeY << ", " << sizeZ << "\n";
    std::cout << "maxSize: " << maxSize << "\n";
    std::cout << "modelScale: " << m_modelScale << "\n";
    std::cout << "center: " << m_centerX << ", " << m_centerY << ", " << m_centerZ << "\n";

    return true;
}

void Lighting::setupMesh() {
    if (m_vao) {
        cleanup();
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)),
        m_vertices.data(),
        GL_STATIC_DRAW
    );

    // location = 0 -> position
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, px))
    );
    glEnableVertexAttribArray(0);

    // location = 1 -> normal
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, nx))
    );
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Lighting::cleanup() {
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
}