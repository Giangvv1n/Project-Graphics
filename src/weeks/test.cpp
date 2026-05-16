// Thêm thư viện tải ảnh và định nghĩa macro Anisotropic [cite: 354]
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
#endif

// Hàm tải ảnh và áp dụng các cấu hình kết cấu [cite: 359, 365, 444, 449]
static GLuint loadTexture2D(const char* path) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // --- Cấu hình Wrapping (Lặp lại kết cấu) --- [cite: 359]
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // --- Cấu hình Filtering (Lấy mẫu kết cấu) --- [cite: 444]
    // Thu nhỏ (Minification): Dùng Trilinear (Nội suy song tuyến tính kết hợp nội suy tuyến tính giữa các Mipmap)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    // Phóng to (Magnification): Dùng Bilinear (Nội suy song tuyến tính)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // --- Cấu hình Anisotropic Filtering --- [cite: 449]
    float maxAniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso); // Lấy giới hạn tối đa của phần cứng
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, maxAniso);

    // --- Xử lý tải dữ liệu ảnh --- [cite: 365]
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true); // Lật gốc UV sang góc dưới bên trái cho đúng hệ tọa độ OpenGL
    unsigned char* data = stbi_load(path, &width, &height, &channels, 0);

    if (!data) {
        std::cerr << "Failed to load texture: " << path << "\n";
        return 0;
    }

    GLenum fmt = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, width, height, 0, fmt, GL_UNSIGNED_BYTE, data);

    // Tự động sinh ra các cấp độ Mipmap --- [cite: 365]
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}
#version 330 core
layout(location = 0) in vec3 aPos;           // [cite: 385]
layout(location = 1) in vec3 aNormal;        // [cite: 386]
layout(location = 2) in vec2 aTexCoord;      // [cite: 387] Bổ sung UV

out vec2 vTexCoord;                          // [cite: 399]
// ... các khai báo khác ...

void main() {
    // ...
    vTexCoord = aTexCoord;                   // [cite: 406] Chuyển tiếp tọa độ UV sang Fragment Shader
}
#version 330 core
in vec2 vTexCoord;                           // [cite: 415]
out vec4 FragColor;                          // [cite: 415]

uniform sampler2D uTexture;                  // [cite: 415] Kết cấu 2D

// ... các khai báo ánh sáng ...

void main() {
    // Lấy mẫu điểm ảnh từ kết cấu [cite: 420]
    vec3 texColor = texture(uTexture, vTexCoord).rgb; 

    // Các thành phần màu sắc được điều chỉnh (modulate) bởi ảnh kết cấu [cite: 420]
    // vec3 ambient = uKa * uAmbientColor * texColor;
    // vec3 diffuse = uKd * ndotl * uLightColor * texColor;
    // ...
}
// Khởi tạo trước vòng lặp render [cite: 423]
GLuint texture = loadTexture2D("your_image.png");
GLint uTexture = glGetUniformLocation(prog, "uTexture");

// Bên trong vòng lặp render, trước khi gọi lệnh vẽ [cite: 423]
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, texture);
glUniform1i(uTexture, 0);


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
#endif

static GLuint loadTexture2D(const char* path) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // --- Wrapping ---
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // --- Filtering (Trilinear từ Task C) ---
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // --- Anisotropic filtering (Từ Task D) ---
    float maxAniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso); 
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, maxAniso);

    // --- Load image ---
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true); 
    unsigned char* data = stbi_load(path, &width, &height, &channels, 0);
    
    if (!data) {
        std::cerr << "Failed to load texture: " << path << "\n";
        return 0;
    }

    GLenum fmt = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, width, height, 0, fmt, GL_UNSIGNED_BYTE, data);

    // --- Auto-generate mipmaps ---
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    // location 0: position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // location 1: normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // location 2: UV <-- NEW
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    return vao;
}

static const char* kVS_part1 = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord; // NEW

uniform mat4 uM;
uniform mat4 uV;
uniform mat4 uP;
uniform mat3 uNormalMat;

out vec3 vWorldPos;
out vec3 vWorldNormal;
out vec2 vTexCoord; // NEW

void main() {
    vec4 wp = uM * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vWorldNormal = normalize(uNormalMat * aNormal);
    vTexCoord = aTexCoord; // NEW
    gl_Position = uP * uV * wp;
}
)GLSL";


static const char* kFS_part1 = R"GLSL(
#version 330 core
in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vTexCoord; // NEW

out vec4 FragColor;

uniform sampler2D uTexture; // NEW
uniform vec3 uViewPos;
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
uniform vec3 uKa;
uniform vec3 uKd;
uniform vec3 uKs;
uniform float uShininess;
uniform bool uUseBlinn;
uniform bool uDebugNormals;

void main() {
    vec3 N = normalize(vWorldNormal);
    if (uDebugNormals) {
        FragColor = vec4(N * 0.5 + 0.5, 1.0);
        return; 
    }

    vec3 texColor = texture(uTexture, vTexCoord).rgb; // NEW: sample texture
    vec3 L = normalize(uLightPos - vWorldPos);
    vec3 V = normalize(uViewPos - vWorldPos);
    
    vec3 ambient = uKa * uAmbientColor * texColor; // texture modulates ambient (MOD)
    
    float ndotl = max(dot(N, L), 0.0);
    vec3 diffuse = uKd * ndotl * uLightColor * texColor; // texture modulates diffuse (MOD)
    
    float spec = 0.0;
    if (ndotl > 0.0) {
        if (uUseBlinn) {
            vec3 H = normalize(L + V);
            spec = pow(max(dot(N, H), 0.0), uShininess);
        } else {
            vec3 R = reflect(-L, N);
            spec = pow(max(dot(R, V), 0.0), uShininess); 
        }
    }
    vec3 specular = uKs * spec * uLightColor; // specular stays white
    
    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
)GLSL";

// 1. Sau khi gọi createCubeVAO():
GLuint texture = loadTexture2D("your_image.png"); // Thay bằng đường dẫn ảnh của bạn

// 2. Cùng chỗ với các uniform lookups khác (sau glUseProgram):
GLint locTexture = glGetUniformLocation(prog, "uTexture");

// 3. Trong vòng lặp render, TRƯỚC KHI gọi glDrawArrays:
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, texture);
glUniform1i(locTexture, 0); // Báo cho shader dùng texture unit 0

// Độ phân giải của Shadow Map (Càng cao bóng càng nét, nhưng nặng hơn)
const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

GLuint depthMapFBO;
glGenFramebuffers(1, &depthMapFBO);

// Tạo 2D texture để lưu độ sâu
GLuint depthMap;
glGenTextures(1, &depthMap);
glBindTexture(GL_TEXTURE_2D, depthMap);
glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

// Cấu hình lọc và bọc (Wrap) cho shadow map
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
// Quan trọng: Dùng CLAMP_TO_BORDER để tránh bóng đổ bị lặp lại ở rìa
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

// Gắn texture độ sâu vào Framebuffer
glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
glDrawBuffer(GL_NONE); // Chúng ta không cần ghi ra màu
glReadBuffer(GL_NONE);
glBindFramebuffer(GL_FRAMEBUFFER, 0);

#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main() {
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}

#version 330 core
void main() {
    // Để trống, OpenGL sẽ tự động ghi dữ liệu vào depth buffer
}

#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec3 vWorldPos;
out vec3 vWorldNormal;
out vec2 vTexCoord;
out vec4 FragPosLightSpace; // NEW: Vị trí trong không gian đèn

uniform mat4 uM;
uniform mat4 uV;
uniform mat4 uP;
uniform mat3 uNormalMat;
uniform mat4 lightSpaceMatrix; // NEW

void main() {
    vec4 wp = uM * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vWorldNormal = normalize(uNormalMat * aNormal);
    vTexCoord = aTexCoord;
    
    // Đưa vị trí thế giới sang không gian của đèn
    FragPosLightSpace = lightSpaceMatrix * vec4(vWorldPos, 1.0);
    
    gl_Position = uP * uV * wp;
}

#version 330 core
in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vTexCoord;
in vec4 FragPosLightSpace; // NEW

out vec4 FragColor;

uniform sampler2D uTexture;
uniform sampler2D shadowMap; // NEW: Texture độ sâu
uniform vec3 uLightPos;
uniform vec3 uViewPos;
uniform vec3 uLightColor;

// Hàm tính bóng đổ (Trả về 1.0 nếu bị khuất bóng, 0.0 nếu được chiếu sáng)
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // Biến đổi tọa độ phối cảnh
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Đưa về dải [0, 1] để đọc texture
    projCoords = projCoords * 0.5 + 0.5;
    
    // Lấy độ sâu gần nhất từ shadow map
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    // Độ sâu hiện tại của fragment
    float currentDepth = projCoords.z;
    
    // Tính toán bias để tránh lỗi Shadow Acne (các sọc đen nứt nẻ trên bề mặt)
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    
    // Kiểm tra xem fragment có nằm sau bề mặt gần nhất không
    float shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;
    
    // Bỏ bóng đổ nếu tọa độ nằm ngoài phạm vi vùng nhìn của đèn (Z > 1.0)
    if(projCoords.z > 1.0) shadow = 0.0;
        
    return shadow;
}

void main() {
    vec3 color = texture(uTexture, vTexCoord).rgb;
    vec3 normal = normalize(vWorldNormal);
    vec3 lightDir = normalize(uLightPos - vWorldPos);
    
    // Ambient
    vec3 ambient = 0.15 * color;
    
    // Diffuse
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * uLightColor;
    
    // Tính toán vùng bị che khuất
    float shadow = ShadowCalculation(FragPosLightSpace, normal, lightDir);                      
    
    // Màu cuối = Ambient + (Vùng không bị che khuất) * (Diffuse + Specular)
    vec3 lighting = (ambient + (1.0 - shadow) * diffuse) * color;    
    
    FragColor = vec4(lighting, 1.0);
}

// 1. Tính toán ma trận không gian đèn (Light Space Matrix)
// Gỉa sử đèn là dạng Directional Light (dùng phối cảnh Orthographic)
glm::mat4 lightProjection, lightView;
glm::mat4 lightSpaceMatrix;
float near_plane = 1.0f, far_plane = 7.5f;
lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
lightSpaceMatrix = lightProjection * lightView;

// ==========================================
// PASS 1: Render vào Shadow Map
// ==========================================
glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
glClear(GL_DEPTH_BUFFER_BIT);

// Kích hoạt shader Pass 1
glUseProgram(simpleDepthShader);
glUniformMatrix4fv(glGetUniformLocation(simpleDepthShader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

// [VẼ KHỐI LẬP PHƯƠNG VÀ MẶT PHẲNG Ở ĐÂY DÙNG CHƯƠNG TRÌNH simpleDepthShader]
renderScene(simpleDepthShader); 

glBindFramebuffer(GL_FRAMEBUFFER, 0);

// ==========================================
// PASS 2: Render cảnh bình thường ra màn hình
// ==========================================
glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT); // Trả lại kích thước cửa sổ
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

// Kích hoạt shader chính
glUseProgram(mainShader);

// Gửi các ma trận cơ bản cho Camera
glUniformMatrix4fv(glGetUniformLocation(mainShader, "uV"), 1, GL_FALSE, glm::value_ptr(cameraView));
glUniformMatrix4fv(glGetUniformLocation(mainShader, "uP"), 1, GL_FALSE, glm::value_ptr(cameraProjection));

// Cung cấp vị trí đèn và Light Space Matrix
glUniform3fv(glGetUniformLocation(mainShader, "uLightPos"), 1, &lightPos[0]);
glUniformMatrix4fv(glGetUniformLocation(mainShader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

// Bind kết cấu màu (Diff map) vào unit 0
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, cubeTexture);
glUniform1i(glGetUniformLocation(mainShader, "uTexture"), 0);

// Bind kết cấu độ sâu (Shadow map) vào unit 1
glActiveTexture(GL_TEXTURE1);
glBindTexture(GL_TEXTURE_2D, depthMap);
glUniform1i(glGetUniformLocation(mainShader, "shadowMap"), 1);

// [VẼ KHỐI LẬP PHƯƠNG VÀ MẶT PHẲNG Ở ĐÂY LẦN 2 DÙNG mainShader]
renderScene(mainShader);