// render/Mesh.h
// Encapsulates a vertex/index buffer pair for drawing.
#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 tangent; // placeholder tangent (orthonormal with normal)
};

class Mesh {
public:
    Mesh() = default;
    void setData(const std::vector<Vertex>& vertices, const std::vector<unsigned>& indices);
    static Mesh loadFromFile(const std::string& path);
    void bind() const;
    int indexCount() const { return m_indexCount; }
private:
    unsigned m_vao = 0;
    unsigned m_vbo = 0;
    unsigned m_ebo = 0;
    int m_indexCount = 0;
};