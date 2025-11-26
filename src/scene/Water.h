#pragma once
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../render/Mesh.h"

class Water {
public:
    Water();
    ~Water();

    // Generate a flat water plane mesh
    // widthQuads: number of quads along X
    // worldSize: total size in world units
    // height: water level
    void generate(int widthQuads, float worldSize, float height);

    Mesh* mesh() const { return m_mesh; }

private:
    void buildMesh(int widthQuads, float worldSize, float height);

    Mesh* m_mesh = nullptr;
};