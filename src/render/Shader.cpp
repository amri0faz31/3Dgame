// render/Shader.cpp
#include "Shader.h"
#include <glad/glad.h>
#include <iostream>
#include <vector>

static bool compileStage(GLenum type, const std::string& src, GLuint& out){
    out = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(out,1,&c,nullptr);
    glCompileShader(out);
    GLint ok; glGetShaderiv(out, GL_COMPILE_STATUS, &ok);
    if(!ok){
        char log[1024]; glGetShaderInfoLog(out,1024,nullptr,log);
        std::cerr << "[Shader] Compile error: " << log << std::endl;
        return false;
    }
    return true;
}

bool Shader::compile(const std::string& vertexSrc, const std::string& fragmentSrc){
    GLuint vs = 0, fs = 0;
    if(!compileStage(GL_VERTEX_SHADER, vertexSrc, vs)) return false;
    if(!compileStage(GL_FRAGMENT_SHADER, fragmentSrc, fs)) { glDeleteShader(vs); return false; }
    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);
    GLint ok; glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    glDeleteShader(vs); glDeleteShader(fs);
    if(!ok){
        char log[1024]; glGetProgramInfoLog(m_program,1024,nullptr,log);
        std::cerr << "[Shader] Link error: " << log << std::endl;
        return false;
    }
    return true;
}

bool Shader::compileWithGeometry(const std::string& vertexSrc, const std::string& geometrySrc, const std::string& fragmentSrc){
    GLuint vs = 0, gs = 0, fs = 0;
    if(!compileStage(GL_VERTEX_SHADER, vertexSrc, vs)) return false;
    if(!compileStage(GL_GEOMETRY_SHADER, geometrySrc, gs)) { glDeleteShader(vs); return false; }
    if(!compileStage(GL_FRAGMENT_SHADER, fragmentSrc, fs)) { glDeleteShader(vs); glDeleteShader(gs); return false; }
    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, gs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);
    GLint ok; glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    glDeleteShader(vs); glDeleteShader(gs); glDeleteShader(fs);
    if(!ok){
        char log[1024]; glGetProgramInfoLog(m_program,1024,nullptr,log);
        std::cerr << "[Shader] Link error (geom): " << log << std::endl;
        return false;
    }
    return true;
}

void Shader::bind() const { glUseProgram(m_program); }

int Shader::uniformLocation(const std::string& name) const {
    return glGetUniformLocation(m_program, name.c_str());
}

void Shader::setMat4(const std::string& name, const glm::mat4& m){
    glUniformMatrix4fv(uniformLocation(name),1,GL_FALSE,&m[0][0]);
}
void Shader::setVec3(const std::string& name, const glm::vec3& v){
    glUniform3fv(uniformLocation(name),1,&v[0]);
}
void Shader::setVec2(const std::string& name, const glm::vec2& v){
    glUniform2fv(uniformLocation(name),1,&v[0]);
}
void Shader::setFloat(const std::string& name, float v){
    glUniform1f(uniformLocation(name), v);
}
void Shader::setInt(const std::string& name, int v){
    glUniform1i(uniformLocation(name), v);
}
void Shader::setBool(const std::string& name, bool v){
    glUniform1i(uniformLocation(name), v ? 1 : 0);
}