// ==========================
// OpenGL STL Viewer
// Binary STL + Ground + Door-friendly collision
// Collision by horizontal slice at body height
// ==========================

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cfloat>
#include <cmath>
#include <algorithm>

// ==========================
// SETTINGS
// ==========================
const unsigned int SCR_WIDTH  = 1280;
const unsigned int SCR_HEIGHT = 720;

// ==========================
// CAMERA
// ==========================
glm::vec3 cameraPos(0.0f, 0.0f, 0.0f);
glm::vec3 cameraFront(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);

float yaw = -90.0f;
float pitch = 0.0f;
float lastX = SCR_WIDTH * 0.5f;
float lastY = SCR_HEIGHT * 0.5f;
bool firstMouse = true;
float fov = 45.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;
float speed = 2.3f;

// ==========================
// WORLD PARAMETERS
// ==========================
float GROUND_Y = -0.5f;

// mắt cao hơn mặt đất một chút
const float EYE_HEIGHT = 0.12f;

// độ cao dùng để cắt mesh lấy collision
const float COLLISION_SLICE_HEIGHT = 0.12f;

// bán kính va chạm
const float CAMERA_RADIUS = 0.03f;

// lọc gần-đứng
const float WALL_NORMAL_Y_LIMIT = 0.65f;

// ==========================
// COLLISION DATA
// ==========================
struct Segment2D
{
    glm::vec2 a;
    glm::vec2 b;
};

std::vector<Segment2D> g_collisionSegments;

// ==========================
// INPUT
// ==========================
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    (void)window;

    if (firstMouse)
    {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;
    lastX = (float)xpos;
    lastY = (float)ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw   += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    (void)window;
    (void)xoffset;

    fov -= (float)yoffset;
    if (fov < 1.0f)  fov = 1.0f;
    if (fov > 90.0f) fov = 90.0f;
}

// ==========================
// SHADER
// ==========================
unsigned int createShaderProgram()
{
    const char* vertexShaderSource = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * worldPos;
}
)";

    const char* fragmentShaderSource = R"(#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 objectColor;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);

    float diff = max(dot(norm, lightDir), 0.0);

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);

    vec3 ambient  = 0.20 * objectColor;
    vec3 diffuse  = diff * objectColor;
    vec3 specular = 0.45 * spec * vec3(1.0);

    vec3 color = ambient + diffuse + specular;
    FragColor = vec4(color, 1.0);
}
)";

    auto compileShader = [](GLenum type, const char* source) -> unsigned int
    {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char infoLog[1024];
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            std::cout << "Shader compile error:\n" << infoLog << std::endl;
        }

        return shader;
    };

    unsigned int vertexShader   = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        std::cout << "Program link error:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

// ==========================
// LOAD BINARY STL
// x y z nx ny nz
// ==========================
bool loadBinarySTL(const std::string& path, std::vector<float>& data)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;

    char header[80];
    file.read(header, 80);

    uint32_t triangleCount = 0;
    file.read(reinterpret_cast<char*>(&triangleCount), 4);

    for (uint32_t i = 0; i < triangleCount; ++i)
    {
        float nx, ny, nz;
        file.read(reinterpret_cast<char*>(&nx), 4);
        file.read(reinterpret_cast<char*>(&ny), 4);
        file.read(reinterpret_cast<char*>(&nz), 4);

        glm::vec3 v[3];
        for (int j = 0; j < 3; ++j)
            file.read(reinterpret_cast<char*>(&v[j]), 12);

        file.ignore(2);

        glm::vec3 normal = glm::normalize(glm::cross(v[1] - v[0], v[2] - v[0]));

        for (int j = 0; j < 3; ++j)
        {
            data.push_back(v[j].x);
            data.push_back(v[j].y);
            data.push_back(v[j].z);
            data.push_back(normal.x);
            data.push_back(normal.y);
            data.push_back(normal.z);
        }
    }

    return true;
}

// ==========================
// NORMALIZE MODEL
// ==========================
void normalizeModel(std::vector<float>& vertices)
{
    glm::vec3 minP( FLT_MAX,  FLT_MAX,  FLT_MAX);
    glm::vec3 maxP(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (size_t i = 0; i < vertices.size(); i += 6)
    {
        glm::vec3 p(vertices[i], vertices[i + 1], vertices[i + 2]);
        minP = glm::min(minP, p);
        maxP = glm::max(maxP, p);
    }

    glm::vec3 center = (minP + maxP) * 0.5f;
    float diagonal = glm::length(maxP - minP);

    float scale = 1.0f;
    if (diagonal > 0.00001f)
        scale = 1.5f / diagonal;

    for (size_t i = 0; i < vertices.size(); i += 6)
    {
        vertices[i]     = (vertices[i]     - center.x) * scale;
        vertices[i + 1] = (vertices[i + 1] - center.y) * scale;
        vertices[i + 2] = (vertices[i + 2] - center.z) * scale;
    }
}

// ==========================
// GROUND / BOUNDS
// ==========================
float getModelMinYAfterTransform(const std::vector<float>& vertices, const glm::mat4& transform)
{
    float minY = FLT_MAX;

    for (size_t i = 0; i < vertices.size(); i += 6)
    {
        glm::vec4 p(vertices[i], vertices[i + 1], vertices[i + 2], 1.0f);
        glm::vec4 tp = transform * p;
        minY = std::min(minY, tp.y);
    }

    return minY;
}

void getTransformedBounds(
    const std::vector<float>& vertices,
    const glm::mat4& transform,
    glm::vec3& outMin,
    glm::vec3& outMax)
{
    outMin = glm::vec3( FLT_MAX,  FLT_MAX,  FLT_MAX);
    outMax = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (size_t i = 0; i < vertices.size(); i += 6)
    {
        glm::vec4 p(vertices[i], vertices[i + 1], vertices[i + 2], 1.0f);
        glm::vec3 tp = glm::vec3(transform * p);

        outMin = glm::min(outMin, tp);
        outMax = glm::max(outMax, tp);
    }
}

// ==========================
// GROUND
// ==========================
void createGround(std::vector<float>& groundVertices)
{
    float y = GROUND_Y;
    float s = 8.0f;
    glm::vec3 n(0.0f, 1.0f, 0.0f);

    glm::vec3 p1(-s, y, -s);
    glm::vec3 p2( s, y, -s);
    glm::vec3 p3( s, y,  s);
    glm::vec3 p4(-s, y,  s);

    groundVertices = {
        p1.x, p1.y, p1.z, n.x, n.y, n.z,
        p2.x, p2.y, p2.z, n.x, n.y, n.z,
        p3.x, p3.y, p3.z, n.x, n.y, n.z,

        p1.x, p1.y, p1.z, n.x, n.y, n.z,
        p3.x, p3.y, p3.z, n.x, n.y, n.z,
        p4.x, p4.y, p4.z, n.x, n.y, n.z
    };
}

// ==========================
// BUFFERS
// ==========================
void createMeshBuffers(const std::vector<float>& vertices, unsigned int& VAO, unsigned int& VBO)
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

// ==========================
// 2D GEOMETRY HELPERS
// ==========================
float cross2(const glm::vec2& a, const glm::vec2& b)
{
    return a.x * b.y - a.y * b.x;
}

glm::vec2 closestPointOnSegment2D(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b)
{
    glm::vec2 ab = b - a;
    float denom = glm::dot(ab, ab);

    if (denom <= 1e-8f)
        return a;

    float t = glm::dot(p - a, ab) / denom;
    t = std::clamp(t, 0.0f, 1.0f);
    return a + t * ab;
}

bool pointNearSegment2D(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, float radius)
{
    glm::vec2 c = closestPointOnSegment2D(p, a, b);
    return glm::length(p - c) < radius;
}

// ==========================
// SLICE-BASED COLLISION
// ==========================
bool intersectEdgeWithPlaneY(const glm::vec3& p0, const glm::vec3& p1, float y, glm::vec3& outPoint)
{
    float d0 = p0.y - y;
    float d1 = p1.y - y;

    // cùng phía, không cắt
    if ((d0 > 0.0f && d1 > 0.0f) || (d0 < 0.0f && d1 < 0.0f))
        return false;

    // trùng gần như hoàn toàn với mặt phẳng: bỏ qua cạnh này
    if (std::abs(d0) < 1e-6f && std::abs(d1) < 1e-6f)
        return false;

    float denom = (p1.y - p0.y);
    if (std::abs(denom) < 1e-8f)
        return false;

    float t = (y - p0.y) / denom;
    if (t < 0.0f || t > 1.0f)
        return false;

    outPoint = p0 + t * (p1 - p0);
    return true;
}

void buildCollisionSegmentsFromSlice(const std::vector<float>& vertices, const glm::mat4& modelMatrix)
{
    g_collisionSegments.clear();

    float sliceY = GROUND_Y + COLLISION_SLICE_HEIGHT;

    for (size_t i = 0; i + 17 < vertices.size(); i += 18)
    {
        glm::vec3 p0(vertices[i + 0],  vertices[i + 1],  vertices[i + 2]);
        glm::vec3 p1(vertices[i + 6],  vertices[i + 7],  vertices[i + 8]);
        glm::vec3 p2(vertices[i + 12], vertices[i + 13], vertices[i + 14]);

        glm::vec3 tp0 = glm::vec3(modelMatrix * glm::vec4(p0, 1.0f));
        glm::vec3 tp1 = glm::vec3(modelMatrix * glm::vec4(p1, 1.0f));
        glm::vec3 tp2 = glm::vec3(modelMatrix * glm::vec4(p2, 1.0f));

        glm::vec3 normal = glm::normalize(glm::cross(tp1 - tp0, tp2 - tp0));
        if (std::abs(normal.y) >= WALL_NORMAL_Y_LIMIT)
            continue;

        std::vector<glm::vec3> hits;
        glm::vec3 hit;

        if (intersectEdgeWithPlaneY(tp0, tp1, sliceY, hit)) hits.push_back(hit);
        if (intersectEdgeWithPlaneY(tp1, tp2, sliceY, hit)) hits.push_back(hit);
        if (intersectEdgeWithPlaneY(tp2, tp0, sliceY, hit)) hits.push_back(hit);

        // bỏ trùng gần nhau
        std::vector<glm::vec3> uniqueHits;
        for (const auto& h : hits)
        {
            bool dup = false;
            for (const auto& u : uniqueHits)
            {
                if (glm::length(h - u) < 1e-4f)
                {
                    dup = true;
                    break;
                }
            }
            if (!dup) uniqueHits.push_back(h);
        }

        if (uniqueHits.size() == 2)
        {
            glm::vec2 a(uniqueHits[0].x, uniqueHits[0].z);
            glm::vec2 b(uniqueHits[1].x, uniqueHits[1].z);

            if (glm::length(a - b) > 1e-4f)
                g_collisionSegments.push_back({a, b});
        }
    }
}

bool collidesWithWorld(const glm::vec3& position)
{
    glm::vec2 p(position.x, position.z);

    for (const auto& seg : g_collisionSegments)
    {
        if (pointNearSegment2D(p, seg.a, seg.b, CAMERA_RADIUS))
            return true;
    }
    return false;
}

glm::vec3 moveWithCollision(const glm::vec3& currentPos, const glm::vec3& delta)
{
    glm::vec3 result = currentPos;

    glm::vec3 tryX = result + glm::vec3(delta.x, 0.0f, 0.0f);
    if (!collidesWithWorld(tryX))
        result.x = tryX.x;

    glm::vec3 tryZ = result + glm::vec3(0.0f, 0.0f, delta.z);
    if (!collidesWithWorld(tryZ))
        result.z = tryZ.z;

    return result;
}

glm::vec3 findSpawnInsideRoom(const glm::vec3& worldMin, const glm::vec3& worldMax)
{
    float y = GROUND_Y + EYE_HEIGHT;

    std::vector<glm::vec3> candidates = {
        glm::vec3((worldMin.x + worldMax.x) * 0.5f, y, (worldMin.z + worldMax.z) * 0.5f),
        glm::vec3((worldMin.x + worldMax.x) * 0.5f, y, worldMin.z + (worldMax.z - worldMin.z) * 0.30f),
        glm::vec3((worldMin.x + worldMax.x) * 0.5f, y, worldMin.z + (worldMax.z - worldMin.z) * 0.70f),
        glm::vec3(worldMin.x + (worldMax.x - worldMin.x) * 0.30f, y, (worldMin.z + worldMax.z) * 0.5f),
        glm::vec3(worldMin.x + (worldMax.x - worldMin.x) * 0.70f, y, (worldMin.z + worldMax.z) * 0.5f)
    };

    for (const auto& p : candidates)
    {
        if (!collidesWithWorld(p))
            return p;
    }

    const int GRID = 30;
    for (int iz = 0; iz < GRID; ++iz)
    {
        for (int ix = 0; ix < GRID; ++ix)
        {
            float tx = (float)ix / (GRID - 1);
            float tz = (float)iz / (GRID - 1);

            glm::vec3 p(
                worldMin.x + tx * (worldMax.x - worldMin.x),
                y,
                worldMin.z + tz * (worldMax.z - worldMin.z)
            );

            if (!collidesWithWorld(p))
                return p;
        }
    }

    return glm::vec3((worldMin.x + worldMax.x) * 0.5f, y, (worldMin.z + worldMax.z) * 0.5f);
}

// ==========================
// INPUT WITH COLLISION
// ==========================
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float velocity = speed * deltaTime;

    glm::vec3 flatFront = glm::vec3(cameraFront.x, 0.0f, cameraFront.z);
    if (glm::length(flatFront) < 0.0001f)
        flatFront = glm::vec3(0.0f, 0.0f, -1.0f);
    flatFront = glm::normalize(flatFront);

    glm::vec3 right = glm::normalize(glm::cross(flatFront, glm::vec3(0.0f, 1.0f, 0.0f)));

    glm::vec3 delta(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) delta += flatFront * velocity;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) delta -= flatFront * velocity;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) delta -= right * velocity;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) delta += right * velocity;

    cameraPos = moveWithCollision(cameraPos, delta);
    cameraPos.y = GROUND_Y + EYE_HEIGHT;
}

// ==========================
// MAIN
// ==========================
int main()
{
    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "STL Viewer", nullptr, nullptr);
    if (!window)
    {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD\n";
        glfwTerminate();
        return -1;
    }

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glEnable(GL_DEPTH_TEST);

    unsigned int shaderProgram = createShaderProgram();

    std::vector<float> modelVertices;
    if (!loadBinarySTL("assets/models/L room-BodyPocket001.stl", modelVertices))
    {
        std::cout << "Load STL failed!\n";
        glfwTerminate();
        return -1;
    }

    normalizeModel(modelVertices);

    glm::mat4 modelMatrix(1.0f);
    modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    GROUND_Y = getModelMinYAfterTransform(modelVertices, modelMatrix) - 0.01f;

    glm::vec3 worldMin, worldMax;
    getTransformedBounds(modelVertices, modelMatrix, worldMin, worldMax);

    buildCollisionSegmentsFromSlice(modelVertices, modelMatrix);

    std::cout << "Triangles: " << modelVertices.size() / 18 << std::endl;
    std::cout << "Ground Y: " << GROUND_Y << std::endl;
    std::cout << "Collision segments: " << g_collisionSegments.size() << std::endl;

    std::vector<float> groundVertices;
    createGround(groundVertices);

    unsigned int modelVAO = 0, modelVBO = 0;
    unsigned int groundVAO = 0, groundVBO = 0;

    createMeshBuffers(modelVertices, modelVAO, modelVBO);
    createMeshBuffers(groundVertices, groundVAO, groundVBO);

    cameraPos = findSpawnInsideRoom(worldMin, worldMax);
    cameraPos.y = GROUND_Y + EYE_HEIGHT;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.10f, 0.10f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 groundMatrix(1.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(
            glm::radians(fov),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.01f,
            100.0f
        );

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), 5.0f, 5.0f, 5.0f);
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(groundMatrix));
        glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 0.35f, 0.35f, 0.35f);
        glBindVertexArray(groundVAO);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(groundVertices.size() / 6));

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
        glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 0.8f, 0.6f, 0.3f);
        glBindVertexArray(modelVAO);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(modelVertices.size() / 6));

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &modelVAO);
    glDeleteBuffers(1, &modelVBO);

    glDeleteVertexArrays(1, &groundVAO);
    glDeleteBuffers(1, &groundVBO);

    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}   