// ==========================
// OpenGL STL Viewer
// Binary + ASCII STL
// ==========================

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cfloat>

// ==========================
// SETTINGS
// ==========================
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// ==========================
// CAMERA
// ==========================
glm::vec3 cameraPos(0,0,3);
glm::vec3 cameraFront(0,0,-1);
glm::vec3 cameraUp(0,1,0);

float yaw = -90.0f, pitch = 0.0f;
float lastX = 640, lastY = 360;
bool firstMouse = true;
float fov = 45.0f;

float deltaTime = 0, lastFrame = 0;
float speed = 5.0f;

// ==========================
// INPUT
// ==========================
void mouse_callback(GLFWwindow*, double xpos, double ypos)
{
    if (firstMouse){ lastX = xpos; lastY = ypos; firstMouse = false; }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos; lastY = ypos;

    float sens = 0.1f;
    xoffset *= sens; yoffset *= sens;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89) pitch = 89;
    if (pitch < -89) pitch = -89;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw))*cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw))*cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void scroll_callback(GLFWwindow*, double, double yoffset)
{
    fov -= yoffset;
    if (fov < 1) fov = 1;
    if (fov > 90) fov = 90;
}

void processInput(GLFWwindow* window)
{   
    // thoát chương trình
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float v = speed * deltaTime;
    glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

    // WASD
    if (glfwGetKey(window, GLFW_KEY_W)==GLFW_PRESS) cameraPos += v*cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S)==GLFW_PRESS) cameraPos -= v*cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A)==GLFW_PRESS) cameraPos -= right*v;
    if (glfwGetKey(window, GLFW_KEY_D)==GLFW_PRESS) cameraPos += right*v;

     // Q/E lên xuống
    if (glfwGetKey(window, GLFW_KEY_Q)==GLFW_PRESS) cameraPos += v*worldUp;
    if (glfwGetKey(window, GLFW_KEY_E)==GLFW_PRESS) cameraPos -= v*worldUp;
}

// ==========================
// SHADER
// ==========================
unsigned int createShader()
{
    const char* vs = R"(#version 330 core
    layout (location=0) in vec3 aPos;
    layout (location=1) in vec3 aNormal;

    out vec3 FragPos;
    out vec3 Normal;

    uniform mat4 model, view, projection;

    void main(){
        FragPos = vec3(model*vec4(aPos,1));
        Normal = mat3(transpose(inverse(model)))*aNormal;
        gl_Position = projection*view*vec4(FragPos,1);
    })";

    const char* fs = R"(#version 330 core
    out vec4 FragColor;
    in vec3 FragPos;
    in vec3 Normal;

    uniform vec3 lightPos;
    uniform vec3 viewPos;

    void main(){
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);

        float diff = max(dot(norm, lightDir), 0.0);

        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 halfway = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfway), 0.0), 32);

        vec3 color = (0.2 + diff + 0.5*spec) * vec3(0.8,0.6,0.3);
        FragColor = vec4(color,1);
    })";

    auto compile = [](GLenum type, const char* src){
        unsigned int s = glCreateShader(type);
        glShaderSource(s,1,&src,nullptr);
        glCompileShader(s);
        return s;
    };

    unsigned int v = compile(GL_VERTEX_SHADER, vs);
    unsigned int f = compile(GL_FRAGMENT_SHADER, fs);

    unsigned int p = glCreateProgram();
    glAttachShader(p,v);
    glAttachShader(p,f);
    glLinkProgram(p);

    glDeleteShader(v); glDeleteShader(f);
    return p;
}

// ==========================
// LOAD BINARY STL
// ==========================
bool loadBinarySTL(const std::string& path, std::vector<float>& data)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    char header[80];
    file.read(header, 80);

    uint32_t triCount;
    file.read(reinterpret_cast<char*>(&triCount), 4);

    for (uint32_t i=0;i<triCount;i++)
    {
        float nx,ny,nz;
        file.read((char*)&nx,4);
        file.read((char*)&ny,4);
        file.read((char*)&nz,4);

        glm::vec3 v[3];
        for (int j=0;j<3;j++)
            file.read((char*)&v[j],12);

        file.ignore(2);

        glm::vec3 normal = glm::normalize(glm::cross(v[1]-v[0], v[2]-v[0]));

        for (int j=0;j<3;j++)
        {
            data.insert(data.end(), {v[j].x,v[j].y,v[j].z, normal.x,normal.y,normal.z});
        }
    }
    return true;
}

// ==========================
// AUTO CENTER + SCALE
// ==========================
void normalizeModel(std::vector<float>& v)
{
    glm::vec3 min(FLT_MAX), max(-FLT_MAX);

    for (int i=0;i<v.size();i+=6)
    {
        glm::vec3 p(v[i],v[i+1],v[i+2]);
        min = glm::min(min,p);
        max = glm::max(max,p);
    }

    glm::vec3 center = (min+max)*0.5f;
    float scale = 1.5f / glm::length(max-min);

    for (int i=0;i<v.size();i+=6)
    {
        v[i]   = (v[i]-center.x)*scale;
        v[i+1] = (v[i+1]-center.y)*scale;
        v[i+2] = (v[i+2]-center.z)*scale;
    }
}

// ==========================
// MAIN
// ==========================
int main()
{
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH,SCR_HEIGHT,"STL Viewer",NULL,NULL);
    glfwMakeContextCurrent(window);

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST);

    unsigned int shader = createShader();

    std::vector<float> vertices;

    if (!loadBinarySTL("assets/models/L room-BodyPocket001.stl", vertices))
    {
        std::cout << "Load STL failed!\n";
        return -1;
    }

    normalizeModel(vertices);

    std::cout << "Triangles: " << vertices.size()/18 << std::endl;

    unsigned int VAO,VBO;
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,vertices.size()*4,vertices.data(),GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    while (!glfwWindowShouldClose(window))
    {
        float time = glfwGetTime();
        deltaTime = time - lastFrame;
        lastFrame = time;

        processInput(window);

        glClearColor(0.1,0.1,0.15,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader);

        glm::mat4 model(20.0f);
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1,0,0));
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos+cameraFront, cameraUp);
        glm::mat4 proj = glm::perspective(glm::radians(fov), (float)SCR_WIDTH/SCR_HEIGHT, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(shader,"model"),1,0,glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shader,"view"),1,0,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shader,"projection"),1,0,glm::value_ptr(proj));

        glUniform3f(glGetUniformLocation(shader,"lightPos"),5,5,5);
        glUniform3fv(glGetUniformLocation(shader,"viewPos"),1,glm::value_ptr(cameraPos));

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES,0,vertices.size()/6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
}