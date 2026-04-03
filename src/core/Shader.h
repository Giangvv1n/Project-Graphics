#pragma once
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>
class Shader {
public:
    Shader() = default;
    ~Shader();

    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    void use() const;
    GLuint id() const { return m_program; }

    void setFloat(const std::string& name, float v) const;
    void setMat4(const std::string& name, const glm::mat4& m) const;

    //
    void setMat3(const std :: string & name,const glm :: mat3 & mat) const;

private:
    GLuint m_program = 0;

    static std::string readTextFile(const std::string& path);
    static GLuint compile(GLenum type, const std::string& src);
    static GLuint link(GLuint vs, GLuint fs);
};
