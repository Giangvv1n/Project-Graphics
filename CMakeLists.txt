
cmake_minimum_required(VERSION 3.20)
project(OpenGL_Course_Template LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Which weekly demo to run (1..10)
set(COURSE_DEMO "1" CACHE STRING "Select which weekly demo to run (1..10)")

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

add_executable(opengl_course
    # src/main.cpp
    # src/App.cpp
    # src/core/Shader.cpp
    # src/weeks/Lighting.cpp
    # extern/glad/src/glad.c
    src/weeks/mirror_room.cpp
)

target_include_directories(opengl_course PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/glad/include/
     ${CMAKE_CURRENT_SOURCE_DIR}/extern/stb
    )

# GLAD C source (must exist once you generate GLAD)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/glad/src/glad.c")
    target_sources(opengl_course PRIVATE extern/glad/src/glad.c)
else()
    message(WARNING "GLAD source not found at extern/glad/src/glad.c. Generate GLAD and place it there.")
endif()

target_compile_definitions(opengl_course PRIVATE
    COURSE_DEMO=${COURSE_DEMO}
    GLFW_INCLUDE_NONE=1
)

target_link_libraries(opengl_course PRIVATE
    glfw
    glm::glm
)

if (WIN32)
    target_link_libraries(opengl_course PRIVATE opengl32)
endif()

# Copy assets next to the executable
add_custom_command(TARGET opengl_course POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:opengl_course>/assets
)
