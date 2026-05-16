// ==========================
// OpenGL STL Viewer
// Binary STL + Ground + Door-friendly collision
// Collision by horizontal slice at body height
// ==========================

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
extern "C" {
// Ép driver sử dụng card rời NVIDIA thay vì card onboard tích hợp
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
// Ép driver sử dụng card rời AMD thay vì card onboard
__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 1;
}
#endif

// ==========================
// SETTINGS
// ==========================
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// ==========================
// CAMERA & PLAYER
// ==========================
glm::vec3 playerPos(0.0f, 0.0f, 0.0f);
float playerHeightOffset = 0.0f;
glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);

bool isCCTV = false;
bool cKeyPressed = false;

glm::vec3 cctvPos(0.0f, 0.0f, 0.0f);
float cctvYaw = -90.0f;
float cctvPitch = -60.0f; // Nhìn chếch xuống từ trần nhà
glm::vec3 cctvFront(0.0f, -0.866f, -0.5f);

float fpYaw = -90.0f;
float fpPitch = 0.0f;
glm::vec3 fpFront(0.0f, 0.0f, -1.0f);
float lastX = SCR_WIDTH * 0.5f;
float lastY = SCR_HEIGHT * 0.5f;
bool firstMouse = true;
float fov = 45.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;
float speed = 0.575f; // Giảm 75% tốc độ so với 2.3f ban đầu

// ==========================
// WORLD PARAMETERS
// ==========================
float GROUND_Y = -0.5f;
float CEIL_Y = 0.5f;

// mắt cao hơn mặt đất một chút
const float EYE_HEIGHT = 0.12f;

// độ cao dùng để cắt mesh lấy collision
const float COLLISION_SLICE_HEIGHT = 0.12f;

// bán kính va chạm
const float CAMERA_RADIUS = 0.06f;

// lọc gần-đứng
const float WALL_NORMAL_Y_LIMIT = 0.65f;

// ==========================
// COLLISION DATA
// ==========================
struct Segment2D {
  glm::vec2 a;
  glm::vec2 b;
};

std::vector<Segment2D> g_collisionSegments;

// ==========================
// INPUT
// ==========================
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
  (void)window;

  if (firstMouse) {
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

  if (isCCTV) {
    cctvYaw += xoffset;
    cctvPitch += yoffset;
    if (cctvPitch > 89.0f)
      cctvPitch = 89.0f;
    if (cctvPitch < -89.0f)
      cctvPitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(cctvYaw)) * cos(glm::radians(cctvPitch));
    front.y = sin(glm::radians(cctvPitch));
    front.z = sin(glm::radians(cctvYaw)) * cos(glm::radians(cctvPitch));
    cctvFront = glm::normalize(front);
  } else {
    fpYaw += xoffset;
    fpPitch += yoffset;
    if (fpPitch > 89.0f)
      fpPitch = 89.0f;
    if (fpPitch < -89.0f)
      fpPitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(fpYaw)) * cos(glm::radians(fpPitch));
    front.y = sin(glm::radians(fpPitch));
    front.z = sin(glm::radians(fpYaw)) * cos(glm::radians(fpPitch));
    fpFront = glm::normalize(front);
  }
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
  fov -= (float)yoffset;
  if (fov < 1.0f)
    fov = 1.0f;
  if (fov > 45.0f)
    fov = 45.0f;
}

unsigned int loadTexture(char const *path) {
  unsigned int textureID;
  glGenTextures(1, &textureID);

  int width, height, nrComponents;
  // Lật ảnh theo trục Y khi load để map đúng tọa độ
  stbi_set_flip_vertically_on_load(true);
  unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
  if (data) {
    GLenum format = GL_RGB;
    if (nrComponents == 1)
      format = GL_RED;
    else if (nrComponents == 3)
      format = GL_RGB;
    else if (nrComponents == 4)
      format = GL_RGBA;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    float maxAniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, maxAniso);

    stbi_image_free(data);
  } else {
    std::cout << "Texture failed to load at path: " << path << std::endl;
    stbi_image_free(data);
  }

  return textureID;
}

// ==========================
// SHADER
// ==========================
unsigned int createRayTracingProgram() {
  const char *vertexShaderSource = R"(#version 330 core
layout (location = 0) in vec2 aPos;
out vec2 TexCoords;
void main()
{
    TexCoords = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}
)";

  const char *fragmentShaderSource = R"(#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform vec3 cameraPos;
uniform vec3 cameraFront;
uniform vec3 cameraUp;

uniform vec3 lightPos;
uniform bool isCCTV;

uniform sampler2D floorTexture;

uniform float fov;
uniform float aspectRatio;
uniform vec2 resolution;

uniform int numSegments;
uniform vec4 segments[200]; 

uniform float groundY;
uniform float ceilY;

uniform vec3 playerPos;
uniform float playerRadius;

uniform mat4 view;
uniform mat4 projection;

#define MAX_BOUNCES 12

float hitSegment(vec2 o, vec2 d, vec2 a, vec2 b, out vec3 normal) {
    vec2 v = b - a;
    vec2 w = a - o;
    float det = v.x * d.y - v.y * d.x;
    if (abs(det) < 1e-6) return -1.0;
    
    float t = (v.x * w.y - v.y * w.x) / det;
    float u = (d.x * w.y - d.y * w.x) / det;
    
    if (t > 1e-4 && u >= 0.0 && u <= 1.0) {
        vec2 n2 = normalize(vec2(-v.y, v.x));
        if (dot(n2, d) > 0.0) n2 = -n2; 
        normal = vec3(n2.x, 0.0, n2.y);
        return t;
    }
    return -1.0;
}

float hitSphere(vec3 o, vec3 d, vec3 center, float radius, out vec3 normal) {
    vec3 oc = o - center;
    float b = dot(oc, d);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) return -1.0;
    h = sqrt(h);
    float t = -b - h;
    if (t > 1e-4) {
        normal = normalize((o + t * d) - center);
        return t;
    }
    t = -b + h;
    if (t > 1e-4) {
        normal = normalize((o + t * d) - center);
        return t;
    }
    return -1.0;
}

struct Hit {
    float t;
    vec3 normal;
    int type; 
    int id; // segment ID
};

Hit map(vec3 ro, vec3 rd, int rayType, vec3 pPos1) {
    Hit hit;
    hit.t = 1e20;
    hit.type = 0;
    hit.id = -1;
    
    if (rd.y < -1e-6) {
        float t = (groundY - ro.y) / rd.y;
        if (t > 1e-4 && t < hit.t) {
            hit.t = t;
            hit.normal = vec3(0, 1, 0);
            hit.type = 1;
        }
    }
    
    if (rd.y > 1e-6) {
        float t = (ceilY - ro.y) / rd.y;
        if (t > 1e-4 && t < hit.t) {
            if (rayType != 0) {
                hit.t = t;
                hit.normal = vec3(0, -1, 0);
                hit.type = 2;
            }
        }
    }
    
    vec2 ro2 = vec2(ro.x, ro.z);
    vec2 rd2 = vec2(rd.x, rd.z);
    for (int i = 0; i < numSegments; ++i) {
        vec2 a = segments[i].xy;
        vec2 b = segments[i].zw;
        vec3 n;
        float t = hitSegment(ro2, rd2, a, b, n);
        if (t > 1e-4 && t < hit.t) {
            float yHit = ro.y + t * rd.y;
            if (yHit >= groundY && yHit <= ceilY) {
                hit.t = t;
                hit.normal = n;
                hit.id = i;
                hit.type = 3; // Tất cả 6 bức tường đều làm gương
            }
        }
    }
    
    bool hitBall = true;
    if (rayType == 0 && !isCCTV) {
        hitBall = false; // Ẩn quả bóng ở góc nhìn FPS để không che camera
    }
    // Không ẩn bóng cho rayType == 2 nữa, quả bóng sẽ đổ bóng chuẩn vật lý lên mọi nơi (cả sàn và trần)
    
    if (hitBall) {
        vec3 n1;
        float t1 = hitSphere(ro, rd, pPos1, playerRadius, n1);
        
        if (t1 > 1e-4 && t1 < hit.t) {
            hit.t = t1;
            hit.normal = n1;
            hit.type = 4;
        }
    }
    
    return hit;
}

vec3 getRayDirection(vec2 uv) {
    vec3 w = normalize(cameraFront);
    vec3 u = normalize(cross(w, cameraUp));
    vec3 v = cross(u, w);
    
    float fovRad = radians(fov);
    float h = tan(fovRad * 0.5);
    
    vec3 d = w + u * uv.x * h * aspectRatio + v * uv.y * h;
    return normalize(d);
}

vec3 render(vec3 ro, vec3 rd, out Hit primaryHit, out vec3 primaryHitPos) {
    vec3 color = vec3(0.0);
    vec3 attenuation = vec3(1.0);
    float totalDist = 0.0;

    for (int i = 0; i <= MAX_BOUNCES; ++i) {
        bool isPrimary = (i == 0);
        Hit hit = map(ro, rd, isPrimary ? 0 : 1, playerPos);
        
        if (isPrimary) {
            primaryHit = hit;
            primaryHitPos = ro + rd * hit.t;
        }
        
        if (hit.type == 0) {
            color += attenuation * vec3(0.02, 0.02, 0.03); 
            break;
        }
        
        vec3 hitPos = ro + rd * hit.t;
        totalDist += hit.t;
        
        if (hit.type == 3) {
            if (i == MAX_BOUNCES) {
                color += attenuation * vec3(0.01, 0.01, 0.015);
                break;
            } else {
                vec3 N = hit.normal;
                vec3 V = -rd;
                vec3 mirrorDirect = vec3(0.0);
                
                vec3 L = normalize(lightPos - hitPos);
                float currentDist = length(lightPos - hitPos);
                
                float shadowFactor = 1.0;
                vec3 shadowRo = hitPos + N * 1e-3; 
                Hit shadowHit = map(shadowRo, L, 2, playerPos);
                if (shadowHit.type != 0 && shadowHit.type != 3 && shadowHit.t < currentDist) {
                    shadowFactor = 0.0;
                }
                
                vec3 reflectDir = reflect(-L, N);
                float spec = pow(max(dot(V, reflectDir), 0.0), 256.0); 
                float lightAtten = 1.0 / (1.0 + 0.1 * currentDist + 0.1 * (currentDist * currentDist));
                
                mirrorDirect += spec * vec3(1.0) * shadowFactor * lightAtten;
                
                color += attenuation * mirrorDirect;
                
                ro = hitPos + hit.normal * 1e-3;
                rd = reflect(rd, hit.normal);
                attenuation *= 0.85; 
                continue;
            }
        }
        
        if (hit.type != 3 && hit.type != 0) {
            vec3 objColor;
            float shininess = 32.0;
            float specStrength = 0.5;

            if (hit.type == 1) {
                vec2 uvTex = vec2(hitPos.x, hitPos.z) * 0.5;
                objColor = texture(floorTexture, uvTex).rgb;
                specStrength = 0.1;
            }
            else if (hit.type == 2) {
                objColor = vec3(0.6, 0.6, 0.6); 
                specStrength = 0.1;
            }
            else if (hit.type == 4) {
                objColor = vec3(1.0, 0.2, 0.2); 
                shininess = 64.0;
            }
            else if (hit.type == 5) {
                objColor = vec3(0.3, 0.4, 0.5); 
                shininess = 128.0; 
                specStrength = 0.8;
            }
            
            float dPlayer = length(hitPos - playerPos);
            float aoRadius = playerRadius * 4.5; 
            if (dPlayer < aoRadius) {
                float shadow = smoothstep(playerRadius * 0.9, aoRadius, dPlayer);
                objColor *= mix(0.15, 1.0, shadow);
            }
            
            vec3 N = hit.normal;
            vec3 V = -rd; 
            vec3 texColor = objColor;
            
            vec3 L = normalize(lightPos - hitPos);
            float currentDist = length(lightPos - hitPos);

            vec3 uAmbientColor = vec3(0.15); // Khôi phục vật lý chuẩn (không fake)
            vec3 uLightColor   = vec3(1.0, 1.0, 1.0);
            
            vec3 uKa = vec3(1.0); 
            vec3 uKd = vec3(1.0);
            vec3 uKs = vec3(specStrength);
            float uShininess = shininess;

            if (hit.type == 4) {
                uKa = vec3(0.15); 
                uKd = vec3(1.0);  
                uKs = vec3(0.7);  
                uShininess = 64.0; 
            }

            float ao = 0.0;
            vec3 aoDirs[4] = vec3[](
                vec3(0.5, 0.5, 0.5), vec3(-0.5, 0.5, -0.5),
                vec3(0.5, 0.5, -0.5), vec3(-0.5, 0.5, 0.5)
            );
            for(int k=0; k<4; k++) {
                vec3 aoDir = normalize(N + aoDirs[k]*0.5); 
                Hit aoHit = map(hitPos + N * 1e-3, aoDir, 1, playerPos);
                if(aoHit.type != 0 && aoHit.t < 0.6) {
                    ao += 1.0;
                }
            }
            float aoFactor = 1.0 - (ao / 4.0) * 0.6; 
            
            // === TRUE GLOBAL ILLUMINATION (1-Bounce Diffuse Color Bleeding) ===
            vec3 indirectLight = vec3(0.0);
            int NUM_GI_SAMPLES = 16; // Chỉnh về 16 tia để cân bằng giữa độ mượt và hiệu năng (FPS)
            for(int gi = 0; gi < NUM_GI_SAMPLES; gi++) {
                // Tạo hướng ngẫu nhiên trên bán cầu (Cosine-weighted hemisphere)
                float r1 = fract(sin(dot(gl_FragCoord.xy + vec2(gi*13.0, gi*3.0), vec2(12.9898, 78.233))) * 43758.5453);
                float r2 = fract(sin(dot(gl_FragCoord.xy + vec2(gi*7.0, gi*17.0), vec2(12.9898, 78.233))) * 43758.5453);
                float theta = acos(sqrt(1.0 - r1));
                float phi = 2.0 * 3.14159265 * r2;
                
                vec3 giDir = vec3(sin(theta)*cos(phi), cos(theta), sin(theta)*sin(phi));
                
                vec3 up = vec3(0.0, 1.0, 0.0);
                if (abs(N.y) > 0.99) up = vec3(1.0, 0.0, 0.0);
                vec3 right = normalize(cross(N, up));
                vec3 fwd = cross(right, N);
                vec3 sampleDir = right * giDir.x + N * giDir.y + fwd * giDir.z;
                
                vec3 giRo = hitPos + N * 1e-3;
                Hit giHit = map(giRo, sampleDir, 1, playerPos);
                
                if (giHit.type != 0 && giHit.type != 3) {
                    vec3 giHitPos = giRo + sampleDir * giHit.t;
                    vec3 giL = normalize(lightPos - giHitPos);
                    float giDist = length(lightPos - giHitPos);
                    
                    // Kiểm tra xem điểm hắt sáng có đang được đèn chiếu trực tiếp không
                    Hit giShadow = map(giHitPos + giHit.normal * 1e-3, giL, 2, playerPos);
                    if (!(giShadow.type != 0 && giShadow.type != 3 && giShadow.t < giDist)) {
                        float giNdotL = max(dot(giHit.normal, giL), 0.0);
                        float giAtten = 1.0 / (1.0 + 0.1 * giDist + 0.1 * giDist * giDist);
                        
                        vec3 giColor = vec3(0.6); 
                        if (giHit.type == 4) giColor = vec3(1.0, 0.2, 0.2); // Quả bóng đỏ hắt ánh sáng đỏ (Color Bleeding)
                        if (giHit.type == 5) giColor = vec3(0.3, 0.4, 0.5); 
                        if (giHit.type == 1) giColor = vec3(0.5); // Sàn nhà
                        
                        indirectLight += giColor * giNdotL * giAtten;
                    }
                }
            }
            indirectLight /= float(NUM_GI_SAMPLES);
            
            // Kết hợp ánh sáng nền cơ bản (0.02) và Ánh sáng nảy vật lý (GI)
            vec3 ambient = uKa * (vec3(0.02) + indirectLight * 0.8) * texColor * aoFactor;
            vec3 direct = ambient;
            
            
            // THẬT NHẤT: Area Light Soft Shadows (Bóng mềm vật lý chuẩn)
            // Thay vì coi đèn là 1 điểm vô hình (gây ra bóng sắc lẹm), ta coi đèn là 1 khối cầu có bán kính.
            // Bắn 9 tia phân bố đều lên bề mặt bóng đèn để lấy bóng mờ (Penumbra) tuyệt đối vật lý.
            float shadowFactor = 0.0;
            float lightRadius = 0.04; // Giảm kích thước đèn để bớt vỡ hạt
            int numSamples = 16;      // Chỉnh về 16 tia để cân bằng hiệu năng
            
            vec3 lightUp = vec3(0.0, 1.0, 0.0);
            if (abs(L.y) > 0.99) lightUp = vec3(1.0, 0.0, 0.0);
            vec3 lightRight = normalize(cross(L, lightUp));
            vec3 lightUpReal = cross(lightRight, L);
            
            // Hàm sinh số ngẫu nhiên dựa trên tọa độ pixel trên màn hình
            float noise = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
            float angleOffset = noise * 6.283185; // Quay góc ngẫu nhiên từ 0 đến 2PI
            
            vec3 shadowRo = hitPos + N * 1e-3; 
            for(int s = 0; s < numSamples; s++) {
                // Thuật toán xoắn ốc Fibonacci (Vogel's Disk)
                float r = sqrt(float(s) + 0.5) / sqrt(float(numSamples));
                float theta = float(s) * 2.3999632 + angleOffset; 
                vec3 offset = (lightRight * cos(theta) + lightUpReal * sin(theta)) * r * lightRadius;
                
                vec3 targetLight = lightPos + offset;
                vec3 dirLight = normalize(targetLight - hitPos);
                float distLight = length(targetLight - hitPos);
                
                Hit shadowHit = map(shadowRo, dirLight, 2, playerPos);
                if (!(shadowHit.type != 0 && shadowHit.type != 3 && shadowHit.t < distLight)) {
                    shadowFactor += 1.0;
                }
            }
            shadowFactor /= float(numSamples);
            
            float lightAttenuation = 1.0 / (1.0 + 0.1 * currentDist + 0.1 * (currentDist * currentDist));
            float ndotl = max(dot(N, L), 0.0);
            vec3 diffuse = uKd * ndotl * uLightColor * texColor * lightAttenuation;
            
            float spec = 0.0;
            if (ndotl > 0.0) {
                vec3 H = normalize(L + V + vec3(1e-5)); 
                spec = pow(max(dot(N, H), 0.0), uShininess);
            }
            vec3 specular = uKs * spec * uLightColor * lightAttenuation;
            
            direct += (diffuse + specular) * shadowFactor;
            
            color += attenuation * direct;
            break; 
        }
    }
    return color;
}

void main() {
    vec3 ro = cameraPos;
    vec3 totalColor = vec3(0.0);
    vec2 invRes = 1.0 / resolution;
    
    Hit primaryHitCenter;
    vec3 primaryHitPosCenter;
    
    // Xử lý Anti-aliasing (SSAA 2x2 = 4 samples)
    for(int m=0; m<2; m++) {
        for(int n=0; n<2; n++) {
            vec2 offset = (vec2(float(m), float(n)) - vec2(0.5)) * invRes;
            vec2 uv = (TexCoords + offset) * 2.0 - 1.0;
            vec3 rd = getRayDirection(uv);
            
            Hit pHit;
            vec3 pPos;
            vec3 col = render(ro, rd, pHit, pPos);
            totalColor += col;
            
            if (m == 0 && n == 0) {
                primaryHitCenter = pHit;
                primaryHitPosCenter = pPos;
            }
        }
    }
    totalColor *= 0.25; // Trung bình cộng 4 samples
    
    totalColor = totalColor / (totalColor + vec3(1.0)); // Tone mapping
    totalColor = pow(totalColor, vec3(1.0/2.2)); // Gamma correction
    
    FragColor = vec4(totalColor, 1.0);

    if (primaryHitCenter.type != 0) {
        vec4 clipSpace = projection * view * vec4(primaryHitPosCenter, 1.0);
        float ndcDepth = clipSpace.z / clipSpace.w;
        gl_FragDepth = (ndcDepth * 0.5) + 0.5;
    } else {
        gl_FragDepth = 1.0;
    }
}
)";

  auto compileShader = [](GLenum type, const char *source) -> unsigned int {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
      char infoLog[1024];
      glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
      std::cout << "Shader compile error:\n" << infoLog << std::endl;
    }

    return shader;
  };

  unsigned int vertexShader =
      compileShader(GL_VERTEX_SHADER, vertexShaderSource);
  unsigned int fragmentShader =
      compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

  unsigned int program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);

  int success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
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
bool loadBinarySTL(const std::string &path, std::vector<float> &data) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return false;

  char header[80];
  file.read(header, 80);

  uint32_t triangleCount = 0;
  file.read(reinterpret_cast<char *>(&triangleCount), 4);

  for (uint32_t i = 0; i < triangleCount; ++i) {
    float nx, ny, nz;
    file.read(reinterpret_cast<char *>(&nx), 4);
    file.read(reinterpret_cast<char *>(&ny), 4);
    file.read(reinterpret_cast<char *>(&nz), 4);

    glm::vec3 v[3];
    for (int j = 0; j < 3; ++j)
      file.read(reinterpret_cast<char *>(&v[j]), 12);

    file.ignore(2);

    glm::vec3 normal = glm::normalize(glm::cross(v[1] - v[0], v[2] - v[0]));

    for (int j = 0; j < 3; ++j) {
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
void normalizeModel(std::vector<float> &vertices) {
  glm::vec3 minP(FLT_MAX, FLT_MAX, FLT_MAX);
  glm::vec3 maxP(-FLT_MAX, -FLT_MAX, -FLT_MAX);

  for (size_t i = 0; i < vertices.size(); i += 6) {
    glm::vec3 p(vertices[i], vertices[i + 1], vertices[i + 2]);
    minP = glm::min(minP, p);
    maxP = glm::max(maxP, p);
  }

  glm::vec3 center = (minP + maxP) * 0.5f;
  float diagonal = glm::length(maxP - minP);

  float scale = 1.0f;
  if (diagonal > 0.00001f)
    scale = 1.5f / diagonal;

  for (size_t i = 0; i < vertices.size(); i += 6) {
    vertices[i] = (vertices[i] - center.x) * scale;
    vertices[i + 1] = (vertices[i + 1] - center.y) * scale;
    vertices[i + 2] = (vertices[i + 2] - center.z) * scale;
  }
}

// ==========================
// GROUND / BOUNDS
// ==========================
float getModelMinYAfterTransform(const std::vector<float> &vertices,
                                 const glm::mat4 &transform) {
  float minY = FLT_MAX;

  for (size_t i = 0; i < vertices.size(); i += 6) {
    glm::vec4 p(vertices[i], vertices[i + 1], vertices[i + 2], 1.0f);
    glm::vec4 tp = transform * p;
    minY = std::min(minY, tp.y);
  }

  return minY;
}

void getTransformedBounds(const std::vector<float> &vertices,
                          const glm::mat4 &transform, glm::vec3 &outMin,
                          glm::vec3 &outMax) {
  outMin = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
  outMax = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

  for (size_t i = 0; i < vertices.size(); i += 6) {
    glm::vec4 p(vertices[i], vertices[i + 1], vertices[i + 2], 1.0f);
    glm::vec3 tp = glm::vec3(transform * p);

    outMin = glm::min(outMin, tp);
    outMax = glm::max(outMax, tp);
  }
}

// ==========================
// GROUND
// ==========================
void createGround(std::vector<float> &groundVertices) {
  float y = GROUND_Y;
  float s = 8.0f;
  glm::vec3 n(0.0f, 1.0f, 0.0f);

  glm::vec3 p1(-s, y, -s);
  glm::vec3 p2(s, y, -s);
  glm::vec3 p3(s, y, s);
  glm::vec3 p4(-s, y, s);

  groundVertices = {p1.x, p1.y, p1.z, n.x,  n.y,  n.z,  p2.x, p2.y, p2.z,
                    n.x,  n.y,  n.z,  p3.x, p3.y, p3.z, n.x,  n.y,  n.z,

                    p1.x, p1.y, p1.z, n.x,  n.y,  n.z,  p3.x, p3.y, p3.z,
                    n.x,  n.y,  n.z,  p4.x, p4.y, p4.z, n.x,  n.y,  n.z};
}

// ==========================
// SPHERE (BALL)
// ==========================
void createSphere(std::vector<float> &vertices, float radius, int sectorCount,
                  int stackCount) {
  float sectorStep = 2.0f * glm::pi<float>() / sectorCount;
  float stackStep = glm::pi<float>() / stackCount;

  auto getPos = [&](int i, int j) -> glm::vec3 {
    float stackAngle = glm::pi<float>() / 2.0f - i * stackStep;
    float sectorAngle = j * sectorStep;
    float xy = radius * cosf(stackAngle);
    float z = radius * sinf(stackAngle);
    float x = xy * cosf(sectorAngle);
    float realZ = xy * sinf(sectorAngle);
    return glm::vec3(x, z, realZ);
  };

  for (int i = 0; i < stackCount; ++i) {
    for (int j = 0; j < sectorCount; ++j) {
      glm::vec3 p1 = getPos(i, j);
      glm::vec3 p2 = getPos(i + 1, j);
      glm::vec3 p3 = getPos(i + 1, j + 1);
      glm::vec3 p4 = getPos(i, j + 1);

      glm::vec3 n1 = glm::normalize(p1);
      glm::vec3 n2 = glm::normalize(p2);
      glm::vec3 n3 = glm::normalize(p3);
      glm::vec3 n4 = glm::normalize(p4);

      vertices.push_back(p1.x);
      vertices.push_back(p1.y);
      vertices.push_back(p1.z);
      vertices.push_back(n1.x);
      vertices.push_back(n1.y);
      vertices.push_back(n1.z);

      vertices.push_back(p2.x);
      vertices.push_back(p2.y);
      vertices.push_back(p2.z);
      vertices.push_back(n2.x);
      vertices.push_back(n2.y);
      vertices.push_back(n2.z);

      vertices.push_back(p3.x);
      vertices.push_back(p3.y);
      vertices.push_back(p3.z);
      vertices.push_back(n3.x);
      vertices.push_back(n3.y);
      vertices.push_back(n3.z);

      vertices.push_back(p1.x);
      vertices.push_back(p1.y);
      vertices.push_back(p1.z);
      vertices.push_back(n1.x);
      vertices.push_back(n1.y);
      vertices.push_back(n1.z);

      vertices.push_back(p3.x);
      vertices.push_back(p3.y);
      vertices.push_back(p3.z);
      vertices.push_back(n3.x);
      vertices.push_back(n3.y);
      vertices.push_back(n3.z);

      vertices.push_back(p4.x);
      vertices.push_back(p4.y);
      vertices.push_back(p4.z);
      vertices.push_back(n4.x);
      vertices.push_back(n4.y);
      vertices.push_back(n4.z);
    }
  }
}

// ==========================
// BUFFERS
// ==========================
void createMeshBuffers(const std::vector<float> &vertices, unsigned int &VAO,
                       unsigned int &VBO) {
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
               vertices.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);
}

// ==========================
// 2D GEOMETRY HELPERS
// ==========================
float cross2(const glm::vec2 &a, const glm::vec2 &b) {
  return a.x * b.y - a.y * b.x;
}

glm::vec2 closestPointOnSegment2D(const glm::vec2 &p, const glm::vec2 &a,
                                  const glm::vec2 &b) {
  glm::vec2 ab = b - a;
  float denom = glm::dot(ab, ab);

  if (denom <= 1e-8f)
    return a;

  float t = glm::dot(p - a, ab) / denom;
  t = std::clamp(t, 0.0f, 1.0f);
  return a + t * ab;
}

bool pointNearSegment2D(const glm::vec2 &p, const glm::vec2 &a,
                        const glm::vec2 &b, float radius) {
  glm::vec2 c = closestPointOnSegment2D(p, a, b);
  return glm::length(p - c) < radius;
}

// ==========================
// SLICE-BASED COLLISION
// ==========================
bool intersectEdgeWithPlaneY(const glm::vec3 &p0, const glm::vec3 &p1, float y,
                             glm::vec3 &outPoint) {
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

void buildCollisionSegmentsFromSlice(const std::vector<float> &vertices,
                                     const glm::mat4 &modelMatrix,
                                     float sliceY) {
  g_collisionSegments.clear();

  for (size_t i = 0; i + 17 < vertices.size(); i += 18) {
    glm::vec3 p0(vertices[i + 0], vertices[i + 1], vertices[i + 2]);
    glm::vec3 p1(vertices[i + 6], vertices[i + 7], vertices[i + 8]);
    glm::vec3 p2(vertices[i + 12], vertices[i + 13], vertices[i + 14]);

    glm::vec3 tp0 = glm::vec3(modelMatrix * glm::vec4(p0, 1.0f));
    glm::vec3 tp1 = glm::vec3(modelMatrix * glm::vec4(p1, 1.0f));
    glm::vec3 tp2 = glm::vec3(modelMatrix * glm::vec4(p2, 1.0f));

    glm::vec3 normal = glm::normalize(glm::cross(tp1 - tp0, tp2 - tp0));
    if (std::abs(normal.y) >= WALL_NORMAL_Y_LIMIT)
      continue;

    std::vector<glm::vec3> hits;
    glm::vec3 hit;

    if (intersectEdgeWithPlaneY(tp0, tp1, sliceY, hit))
      hits.push_back(hit);
    if (intersectEdgeWithPlaneY(tp1, tp2, sliceY, hit))
      hits.push_back(hit);
    if (intersectEdgeWithPlaneY(tp2, tp0, sliceY, hit))
      hits.push_back(hit);

    // bỏ trùng gần nhau
    std::vector<glm::vec3> uniqueHits;
    for (const auto &h : hits) {
      bool dup = false;
      for (const auto &u : uniqueHits) {
        if (glm::length(h - u) < 1e-4f) {
          dup = true;
          break;
        }
      }
      if (!dup)
        uniqueHits.push_back(h);
    }

    if (uniqueHits.size() == 2) {
      glm::vec2 a(uniqueHits[0].x, uniqueHits[0].z);
      glm::vec2 b(uniqueHits[1].x, uniqueHits[1].z);

      if (glm::length(a - b) > 1e-4f)
        g_collisionSegments.push_back({a, b});
    }
  }
}

bool collidesWithWorld(const glm::vec3 &position) {
  glm::vec2 p(position.x, position.z);

  for (const auto &seg : g_collisionSegments) {
    if (pointNearSegment2D(p, seg.a, seg.b, CAMERA_RADIUS))
      return true;
  }
  return false;
}

glm::vec3 moveWithCollision(const glm::vec3 &currentPos,
                            const glm::vec3 &delta) {
  glm::vec3 result = currentPos;

  glm::vec3 tryX = result + glm::vec3(delta.x, 0.0f, 0.0f);
  if (!collidesWithWorld(tryX))
    result.x = tryX.x;

  glm::vec3 tryZ = result + glm::vec3(0.0f, 0.0f, delta.z);
  if (!collidesWithWorld(tryZ))
    result.z = tryZ.z;

  return result;
}

glm::vec3 findSpawnInsideRoom(const glm::vec3 &worldMin,
                              const glm::vec3 &worldMax) {
  float y = GROUND_Y + EYE_HEIGHT;

  std::vector<glm::vec3> candidates = {
      glm::vec3((worldMin.x + worldMax.x) * 0.5f, y,
                (worldMin.z + worldMax.z) * 0.5f),
      glm::vec3((worldMin.x + worldMax.x) * 0.5f, y,
                worldMin.z + (worldMax.z - worldMin.z) * 0.30f),
      glm::vec3((worldMin.x + worldMax.x) * 0.5f, y,
                worldMin.z + (worldMax.z - worldMin.z) * 0.70f),
      glm::vec3(worldMin.x + (worldMax.x - worldMin.x) * 0.30f, y,
                (worldMin.z + worldMax.z) * 0.5f),
      glm::vec3(worldMin.x + (worldMax.x - worldMin.x) * 0.70f, y,
                (worldMin.z + worldMax.z) * 0.5f)};

  for (const auto &p : candidates) {
    if (!collidesWithWorld(p))
      return p;
  }

  const int GRID = 30;
  for (int iz = 0; iz < GRID; ++iz) {
    for (int ix = 0; ix < GRID; ++ix) {
      float tx = (float)ix / (GRID - 1);
      float tz = (float)iz / (GRID - 1);

      glm::vec3 p(worldMin.x + tx * (worldMax.x - worldMin.x), y,
                  worldMin.z + tz * (worldMax.z - worldMin.z));

      if (!collidesWithWorld(p))
        return p;
    }
  }

  return glm::vec3((worldMin.x + worldMax.x) * 0.5f, y,
                   (worldMin.z + worldMax.z) * 0.5f);
}

// ==========================
// INPUT WITH COLLISION
// ==========================
void processInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
    if (!cKeyPressed) {
      isCCTV = !isCCTV;
      cKeyPressed = true;
    }
  } else {
    cKeyPressed = false;
  }

  float velocity = speed * deltaTime;

  glm::vec3 flatFront = glm::vec3(fpFront.x, 0.0f, fpFront.z);
  if (glm::length(flatFront) < 0.0001f)
    flatFront = glm::vec3(0.0f, 0.0f, -1.0f);
  flatFront = glm::normalize(flatFront);

  glm::vec3 right =
      glm::normalize(glm::cross(flatFront, glm::vec3(0.0f, 1.0f, 0.0f)));

  glm::vec3 delta(0.0f);
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    delta += flatFront * velocity;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    delta -= flatFront * velocity;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    delta -= right * velocity;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    delta += right * velocity;

  if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS)
    playerHeightOffset += speed * deltaTime;
  if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)
    playerHeightOffset -= speed * deltaTime;

  playerPos = moveWithCollision(playerPos, delta);
  
  float newY = GROUND_Y + EYE_HEIGHT + playerHeightOffset;
  // Giữ khoảng cách an toàn với trần nhà (tránh camera chọc thủng trần)
  // Bán kính camera = 0.06. Giói hạn bay cách trần 1 chút
  if (newY > CEIL_Y - 0.1f) {
    newY = CEIL_Y - 0.1f; // độ cao giới hạn khi bay
    playerHeightOffset = newY - (GROUND_Y + EYE_HEIGHT);
  }
  if (newY < GROUND_Y + CAMERA_RADIUS) {
    newY = GROUND_Y + CAMERA_RADIUS;
    playerHeightOffset = newY - (GROUND_Y + EYE_HEIGHT);
  }
  playerPos.y = newY;
}

// ==========================
// STL SHADER
// ==========================
unsigned int createSTLProgram() {
  const char *vShaderSource = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 FragPos;
out vec3 Normal;
void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

  const char *fShaderSource = R"(#version 330 core
out vec4 FragColor;
in vec3 FragPos;
in vec3 Normal;
uniform vec3 viewPos;
uniform vec3 lightPos;
uniform float ceilY;

void main() {
    if (FragPos.y < ceilY - 0.5) {
        discard;
    }
    vec3 color = vec3(0.5, 0.5, 0.5); 
    vec3 lightColor = vec3(1.0); 
    
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * color;
    
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
    vec3 specular = 0.1 * spec * lightColor;
    
    float dist = length(lightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.05 * dist + 0.05 * (dist * dist)); 
    
    vec3 ambient = 0.01 * lightColor * color;
    vec3 result = ambient + (diffuse + specular) * attenuation;
    
    result = result / (result + vec3(1.0));
    result = pow(result, vec3(1.0/2.2));
    FragColor = vec4(result, 1.0);
}
)";

  auto compile = [](GLenum type, const char *src) {
    unsigned int s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int success;
    glGetShaderiv(s, GL_COMPILE_STATUS, &success);
    if (!success) {
      char log[512];
      glGetShaderInfoLog(s, 512, nullptr, log);
      std::cout << "STL Shader error: " << log << "\n";
    }
    return s;
  };
  unsigned int vs = compile(GL_VERTEX_SHADER, vShaderSource);
  unsigned int fs = compile(GL_FRAGMENT_SHADER, fShaderSource);
  unsigned int p = glCreateProgram();
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glLinkProgram(p);
  glDeleteShader(vs);
  glDeleteShader(fs);
  return p;
}

// ==========================
// MAIN
// ==========================
int main() {
  if (!glfwInit()) {
    std::cout << "Failed to initialize GLFW\n";
    return -1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window =
      glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "STL Viewer", nullptr, nullptr);
  if (!window) {
    std::cout << "Failed to create GLFW window\n";
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD\n";
    glfwTerminate();
    return -1;
  }

  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetScrollCallback(window, scroll_callback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
  glEnable(GL_DEPTH_TEST);

  unsigned int rtProgram = createRayTracingProgram();
  unsigned int stlProgram = createSTLProgram();

  unsigned int floorTex = loadTexture("assets/skybox/a.png");

  float quadVertices[] = {-1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f,

                          -1.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 1.0f};
  unsigned int quadVAO, quadVBO;
  glGenVertexArrays(1, &quadVAO);
  glGenBuffers(1, &quadVBO);
  glBindVertexArray(quadVAO);
  glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices,
               GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);

  std::vector<float> modelVertices;
  if (!loadBinarySTL("assets/models/Mirror Maze.stl", modelVertices)) {
    std::cout << "Load STL failed!\n";
    glfwTerminate();
    return -1;
  }

  normalizeModel(modelVertices);

  unsigned int stlVAO, stlVBO;
  createMeshBuffers(modelVertices, stlVAO, stlVBO);

  glm::mat4 modelMatrix(1.0f);
  // modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f),
  // glm::vec3(1.0f, 0.0f, 0.0f));

  GROUND_Y = getModelMinYAfterTransform(modelVertices, modelMatrix) - 0.01f;

  glm::vec3 worldMin, worldMax;
  getTransformedBounds(modelVertices, modelMatrix, worldMin, worldMax);

  CEIL_Y = worldMax.y;
  float ceilY = CEIL_Y;
  float sliceY = GROUND_Y + COLLISION_SLICE_HEIGHT;

  buildCollisionSegmentsFromSlice(modelVertices, modelMatrix, sliceY);

  std::cout << "Triangles: " << modelVertices.size() / 18 << std::endl;
  std::cout << "Ground Y: " << GROUND_Y << std::endl;
  std::cout << "Collision segments: " << g_collisionSegments.size()
            << std::endl;

  playerPos = findSpawnInsideRoom(worldMin, worldMax);
  playerPos.y = GROUND_Y + EYE_HEIGHT + playerHeightOffset;

  // Tìm góc phòng sâu nhất có thể nhìn thấy từ playerPos
  glm::vec2 startP(playerPos.x, playerPos.z);
  glm::vec2 cornersBox[4] = {
      glm::vec2(worldMin.x, worldMin.z), glm::vec2(worldMax.x, worldMax.z),
      glm::vec2(worldMin.x, worldMax.z), glm::vec2(worldMax.x, worldMin.z)};

  float bestDist = -1.0f;
  glm::vec2 bestCornerP = startP;

  for (int i = 0; i < 4; ++i) {
    glm::vec2 target = cornersBox[i];
    glm::vec2 dir = glm::normalize(target - startP);
    float maxDist = glm::length(target - startP);
    float dist = 0.0f;
    while (dist < maxDist) {
      glm::vec2 nextP = startP + dir * (dist + 0.05f);
      if (collidesWithWorld(glm::vec3(nextP.x, 0.0f, nextP.y))) {
        break;
      }
      dist += 0.05f;
    }
    if (dist > bestDist) {
      bestDist = dist;
      bestCornerP = startP + dir * std::max(0.0f, dist - 0.2f);
    }
  }

  cctvPos = glm::vec3(bestCornerP.x, ceilY - 0.1f, bestCornerP.y);
  cctvFront =
      glm::normalize(glm::vec3(playerPos.x, GROUND_Y, playerPos.z) - cctvPos);
  cctvPitch = glm::degrees(asin(cctvFront.y));
  cctvYaw = glm::degrees(atan2(cctvFront.z, cctvFront.x));

  while (!glfwWindowShouldClose(window)) {
    float currentFrame = (float)glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    processInput(window);

    glClearColor(0.10f, 0.10f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);

    glUseProgram(rtProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, floorTex);
    glUniform1i(glGetUniformLocation(rtProgram, "floorTexture"), 0);

    glm::vec3 currentCamPos =
        isCCTV ? cctvPos : playerPos + glm::vec3(0.0f, CAMERA_RADIUS, 0.0f);
    glm::vec3 currentCamFront = isCCTV ? cctvFront : fpFront;

    glm::mat4 projection = glm::perspective(
        glm::radians(fov), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.01f, 100.0f);
    glm::mat4 view =
        glm::lookAt(currentCamPos, currentCamPos + currentCamFront, cameraUp);
    glUniformMatrix4fv(glGetUniformLocation(rtProgram, "projection"), 1,
                       GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(rtProgram, "view"), 1, GL_FALSE,
                       glm::value_ptr(view));

    glUniform3fv(glGetUniformLocation(rtProgram, "cameraPos"), 1,
                 glm::value_ptr(currentCamPos));
    glUniform3fv(glGetUniformLocation(rtProgram, "cameraFront"), 1,
                 glm::value_ptr(currentCamFront));
    glUniform3fv(glGetUniformLocation(rtProgram, "cameraUp"), 1,
                 glm::value_ptr(cameraUp));

    glm::vec3 headPos = playerPos + glm::vec3(0.0f, CAMERA_RADIUS, 0.0f);

    // Nguồn sáng ở trên đầu người chơi
    glm::vec3 overheadLightPos = playerPos + glm::vec3(0.0f, 0.4f, 0.0f);
    if (overheadLightPos.y > ceilY - 0.05f) {
        overheadLightPos.y = ceilY - 0.05f; // Không cho đèn lọt lên trên trần nhà
    }

    glUniform3fv(glGetUniformLocation(rtProgram, "lightPos"), 1,
                 glm::value_ptr(overheadLightPos));
    glUniform1i(glGetUniformLocation(rtProgram, "isCCTV"), isCCTV ? 1 : 0);

    glUniform1f(glGetUniformLocation(rtProgram, "fov"), fov);
    glUniform1f(glGetUniformLocation(rtProgram, "aspectRatio"),
                (float)SCR_WIDTH / (float)SCR_HEIGHT);
    glUniform2f(glGetUniformLocation(rtProgram, "resolution"),
                (float)SCR_WIDTH, (float)SCR_HEIGHT);

    glUniform1i(glGetUniformLocation(rtProgram, "numSegments"),
                g_collisionSegments.size());
    for (size_t i = 0; i < g_collisionSegments.size() && i < 200; ++i) {
      std::string name = "segments[" + std::to_string(i) + "]";
      glUniform4f(glGetUniformLocation(rtProgram, name.c_str()),
                  g_collisionSegments[i].a.x, g_collisionSegments[i].a.y,
                  g_collisionSegments[i].b.x, g_collisionSegments[i].b.y);
    }

    glUniform1f(glGetUniformLocation(rtProgram, "groundY"), GROUND_Y);
    glUniform1f(glGetUniformLocation(rtProgram, "ceilY"), ceilY);
    glUniform3fv(glGetUniformLocation(rtProgram, "playerPos"), 1,
                 glm::value_ptr(playerPos));
    glUniform1f(glGetUniformLocation(rtProgram, "playerRadius"), CAMERA_RADIUS);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Draw STL Ceiling
    glDepthFunc(GL_LESS);
    glUseProgram(stlProgram);
    glUniformMatrix4fv(glGetUniformLocation(stlProgram, "projection"), 1,
                       GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(stlProgram, "view"), 1, GL_FALSE,
                       glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(stlProgram, "model"), 1, GL_FALSE,
                       glm::value_ptr(modelMatrix));
    glUniform3fv(glGetUniformLocation(stlProgram, "viewPos"), 1,
                 glm::value_ptr(currentCamPos));
    glUniform3fv(glGetUniformLocation(stlProgram, "lightPos"), 1,
                 glm::value_ptr(overheadLightPos));
    glUniform1f(glGetUniformLocation(stlProgram, "ceilY"), ceilY);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 2.0f);

    glBindVertexArray(stlVAO);
    glDrawArrays(GL_TRIANGLES, 0, modelVertices.size() / 6);

    glDisable(GL_POLYGON_OFFSET_FILL);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glDeleteVertexArrays(1, &quadVAO);
  glDeleteBuffers(1, &quadVBO);

  glDeleteProgram(rtProgram);
  glDeleteProgram(stlProgram);
  glDeleteVertexArrays(1, &stlVAO);
  glDeleteBuffers(1, &stlVBO);

  glfwTerminate();
  return 0;
}