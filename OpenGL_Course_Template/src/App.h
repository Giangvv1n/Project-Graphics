#pragma once
#include <memory>
#include <string>
#include <GLFW/glfw3.h>
#include "IDemo.h"

class App {
public:
    App(int width, int height, const std::string& title);
    ~App();

    void setDemo(std::unique_ptr<IDemo> demo);
    int run();

private:
    int m_width;
    int m_height;
    std::string m_title;

    GLFWwindow* m_window = nullptr;
    std::unique_ptr<IDemo> m_demo;

    bool initWindow();
    bool initGLAD();
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
};
