#pragma once
#include "IDemo.h"
#include "core/Shader.h"
#include <vector>
#include <string>

class Lighting : public IDemo {
public:
    bool init() override;
    void update(float dt) override;
    void render() override;

private:
    struct Vertex {
        float px, py, pz;
        float nx, ny, nz;
    };

    bool loadBinarySTL(const std::string& path);
    void setupMesh();
    void cleanup();

private:
    Shader m_shader;

    std::vector<Vertex> m_vertices;

    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;

    float m_time = 0.0f;

    // model transform helper
    float m_modelScale = 1.0f;
    float m_centerX = 0.0f;
    float m_centerY = 0.0f;
    float m_centerZ = 0.0f;

private:
    float m_yaw = -90.0f;
    float m_pitch = 0.0f;

    float m_lastX = 400.0f;
    float m_lastY = 400.0f;
    bool m_firstMouse = true;

    glm::vec3 m_cameraPos   = glm::vec3(0.0f, 0.0f, 6.0f);
    glm::vec3 m_cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 m_cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
private:
    float m_cameraDistance = 6.0f;
    float m_fov = 45.0f;
    double m_lastScrollY = 0.0;
    };