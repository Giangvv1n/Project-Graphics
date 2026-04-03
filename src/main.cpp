#include <memory>
#include <iostream>

#include "App.h"
#include "weeks/Lighting.h"

int main() {
    std::cout << "Running Lighting demo" << std::endl;

    App app(800, 800, "OpenGL Course Template");
    app.setDemo(std::make_unique<Lighting>());
    return app.run();
}