#include "core/Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

Shader::~Shader() {
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

std::string Shader::readTextFile(const std::string& path) {
    std::ifstream file(path, std::ios::in);
    if (!file) return {};
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint Shader::compile(GLenum type, const std::string& src) {
    const char* csrc = src.c_str();
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &csrc, nullptr);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log((size_t)len + 1);
        glGetShaderInfoLog(s, len, nullptr, log.data());
        std::cerr << "Shader compile error:\n" << log.data() << "\n";
    }
    return s;
}

GLuint Shader::link(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);

    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log((size_t)len + 1);
        glGetProgramInfoLog(p, len, nullptr, log.data());
        std::cerr << "Program link error:\n" << log.data() << "\n";
    }
    return p;
}

bool Shader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vsSrc = readTextFile(vertexPath);
    std::string fsSrc = readTextFile(fragmentPath);
    if (vsSrc.empty() || fsSrc.empty()) {
        std::cerr << "Failed to read shader files:\n  " << vertexPath << "\n  " << fragmentPath << "\n";
        return false;
    }

    GLuint vs = compile(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc);
    GLuint prog = link(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    if (m_program) glDeleteProgram(m_program);
    m_program = prog;
    return m_program != 0;
}

void Shader::use() const { glUseProgram(m_program); }

void Shader::setFloat(const std::string& name, float v) const {
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    if (loc >= 0) glUniform1f(loc, v);
}

void Shader::setMat4(const std::string& name, const glm::mat4& m) const {
    glUniformMatrix4fv(
        glGetUniformLocation(m_program, name.c_str()),
        1,
        GL_FALSE,
        &m[0][0]
    );
}
void Shader :: setMat3(const std :: string & name, const glm :: mat3& mat )const{
    glUniformMatrix3fv(
        glGetUniformLocation(m_program,name.c_str()),
        1,
        GL_FALSE,
        &mat[0][0]
    );
}