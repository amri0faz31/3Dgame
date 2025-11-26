#pragma once
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../render/Mesh.h"

class Terrain {
public:
    Terrain();
    ~Terrain();
 
    // Generate a flat grid mesh and a heightmap texture
    // widthQuads: number of quads along X
    // worldSize: total size in world units
    void generate(int widthQuads, float worldSize);

    Mesh* mesh() const { return m_mesh; }
    GLuint heightTexture() const { return m_heightMapTex; }
    int widthResolution() const { return m_resolution; }
    int lengthResolution() const { return m_resolution; }

    // Helper to get height on CPU (normalized 0..1 value from texture data)
    float getHeight(float x, float z) const;
    // Recommended vertical scale (in world units) to use when displacing vertices in the shader
    float recommendedHeightScale() const { return m_worldSize * m_heightScaleMultiplier; }
    // Configure vertical exaggeration multiplier (default 0.26f)
    void setHeightScaleMultiplier(float m) { m_heightScaleMultiplier = m; }
    float heightScaleMultiplier() const { return m_heightScaleMultiplier; }
    float worldSize() const { return m_worldSize; }
    // Micro-detail parameters (affect normals / small bumps)
    void setMicroAmplitude(float a) { m_microAmplitude = a; }
    void setMicroFrequency(float f) { m_microFrequency = f; }
    float microAmplitude() const { return m_microAmplitude; }
    float microFrequency() const { return m_microFrequency; }
    // Recompute normals from heightmap (call after changing micro-detail or height-scale)
    void recomputeNormals();

private:
    void buildMesh(int widthQuads, float worldSize);
    void generateHeightMap(int resolution);
    void updateMeshNormalsFromHeightmap();

    Mesh* m_mesh = nullptr;
    GLuint m_heightMapTex = 0;
    int m_resolution = 256; 
    std::vector<float> m_heightData;
    float m_worldSize = 100.0f;
    float m_heightScaleMultiplier = 0.26f;
    float m_microAmplitude = 0.035f;
    float m_microFrequency = 0.15f;
};
