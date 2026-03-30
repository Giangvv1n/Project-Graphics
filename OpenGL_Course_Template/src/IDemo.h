#pragma once
#include <GLFW/glfw3.h>

class IDemo {
public:
    virtual ~IDemo() = default;
    virtual bool init() = 0;
    virtual void update(float dt) = 0;
    virtual void render() = 0;
    virtual void processInput(GLFWwindow* window, float dt) { (void)window; (void)dt; }
};
