#include "App.h"
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

App::App(int width, int height, const std::string& title)
    : m_width(width), m_height(height), m_title(title) {}

App::~App() {
    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

void App::setDemo(std::unique_ptr<IDemo> demo) { m_demo = std::move(demo); }

bool App::initWindow() {
    if (!glfwInit()) { std::cerr << "Failed to init GLFW\n"; return false; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
    if (!m_window) { std::cerr << "Failed to create GLFW window\n"; glfwTerminate(); return false; }

    glfwMakeContextCurrent(m_window);
    glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);
    glfwSwapInterval(1);
    return true;
}

bool App::initGLAD() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n";
        return false;
    }
    return true;
}

void App::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
}

int App::run() {
    if (!initWindow()) return 1;
    if (!initGLAD()) return 1;

    if (!m_demo) { std::cerr << "No demo set.\n"; return 1; }
    if (!m_demo->init()) { std::cerr << "Demo init failed.\n"; return 1; }

    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(m_window)) {
        float now = (float)glfwGetTime();
        float dt = now - lastTime;
        lastTime = now;

        if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);

        m_demo->processInput(m_window, dt);
        m_demo->update(dt);
        m_demo->render();

        glfwSwapBuffers(m_window);
        glfwPollEvents();
    }
    return 0;
}
