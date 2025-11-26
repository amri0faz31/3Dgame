// render/Shader.h
// Minimal shader abstraction for compiling vertex/fragment programs and setting uniforms.
#pragma once
#include <string>
#include <glm/glm.hpp>

class Shader {
public:
    Shader() = default;
    bool compile(const std::string& vertexSrc, const std::string& fragmentSrc);
    bool compileWithGeometry(const std::string& vertexSrc, const std::string& geometrySrc, const std::string& fragmentSrc);
    void bind() const;
    void setMat4(const std::string& name, const glm::mat4& m);
    void setVec3(const std::string& name, const glm::vec3& v);
    void setVec2(const std::string& name, const glm::vec2& v);
    void setFloat(const std::string& name, float v);
    void setInt(const std::string& name, int v);
    void setBool(const std::string& name, bool v);
private:
    int m_program = 0;
    int uniformLocation(const std::string& name) const;
};