# OpenGL Course Template (VS Code + CMake, Windows)

This template is designed for a 10-week OpenGL course where students primarily edit **one file per week**:
`src/weeks/WeekXX_*.cpp`.

## What this template provides
- Stable CMake build (C++17)
- GLFW + GLAD + GLM integration
- Simple app framework (`App`) with render loop, delta time, and basic input
- `IDemo` interface and weekly demo stubs (Week01–Week10)
- Shader loading from `assets/shaders/`
- Automatic copying of the `assets/` folder next to the built executable

## Dependencies
This project expects:
- GLFW (via vcpkg)
- GLM  (via vcpkg)
- GLAD (included in repo under `extern/glad/`)

### IMPORTANT: Add GLAD files
Place generated GLAD files here:

- `extern/glad/include/glad/glad.h`
- `extern/glad/include/KHR/khrplatform.h`
- `extern/glad/src/glad.c`

Recommended GLAD generator settings:
- OpenGL **3.3**, Core profile
- Language: C/C++
- Loader enabled

## Build (vcpkg + VS Code)
1. Install deps (PowerShell):
   - `vcpkg install glfw3 glm --triplet x64-mingw-dynamic`
2. In VS Code, ensure `.vscode/settings.json` points to your vcpkg toolchain:
   - `C:/tools/vcpkg/scripts/buildsystems/vcpkg.cmake`
3. VS Code:
   - CMake: Configure
   - CMake: Build
   - CMake: Run Without Debugging

## Select which week to run
Configure with:
- `-DCOURSE_DEMO=5` (runs Week05)
Valid values: 1..10
