#include "Water.h"
#include <cmath>
#include <iostream>
#include <algorithm>

Water::Water() {}

Water::~Water() {
    delete m_mesh;
}

void Water::generate(int widthQuads, float worldSize, float height) {
    buildMesh(widthQuads, worldSize, height);
}

void Water::buildMesh(int widthQuads, float worldSize, float height) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    float step = worldSize / (float)widthQuads;
    float offset = worldSize / 2.0f;

    // Generate vertices
    for (int z = 0; z <= widthQuads; ++z) {
        for (int x = 0; x <= widthQuads; ++x) {
            Vertex v;
            // Flat grid on XZ plane at height, centered
            v.position = glm::vec3(x * step - offset, height, z * step - offset);
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            v.uv = glm::vec2((float)x / widthQuads, (float)z / widthQuads);
            vertices.push_back(v);
        }
    }

    // Generate indices (same as terrain)
    for (int z = 0; z < widthQuads; ++z) {
        for (int x = 0; x < widthQuads; ++x) {
            int topLeft = z * (widthQuads + 1) + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * (widthQuads + 1) + x;
            int bottomRight = bottomLeft + 1;

            // Triangle 1: TL -> TR -> BL
            indices.push_back(topLeft);
            indices.push_back(topRight);
            indices.push_back(bottomLeft);

            // Triangle 2: TR -> BR -> BL
            indices.push_back(topRight);
            indices.push_back(bottomRight);
            indices.push_back(bottomLeft);
        }
    }

    if (m_mesh) delete m_mesh;
    m_mesh = new Mesh();
    m_mesh->setData(vertices, indices);
}