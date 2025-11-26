#include "Terrain.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include "../util/FastNoiseLiteWrapper.h"

Terrain::Terrain() {}

Terrain::~Terrain() {
    delete m_mesh;
    if (m_heightMapTex) glDeleteTextures(1, &m_heightMapTex);
}

void Terrain::generate(int widthQuads, float worldSize) {
    m_worldSize = worldSize;
    m_resolution = widthQuads + 1; // Vertex count matches texture pixels roughly

    buildMesh(widthQuads, worldSize);
    generateHeightMap(m_resolution);
    // After heightmap is generated, compute normals and update mesh normals
    updateMeshNormalsFromHeightmap();
}

void Terrain::recomputeNormals() {
    updateMeshNormalsFromHeightmap();
}

void Terrain::buildMesh(int widthQuads, float worldSize) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    float step = worldSize / (float)widthQuads;
    float offset = worldSize / 2.0f;

    // Generate vertices
    for (int z = 0; z <= widthQuads; ++z) {
        for (int x = 0; x <= widthQuads; ++x) {
            Vertex v;
            // Flat grid on XZ plane, centered
            v.position = glm::vec3(x * step - offset, 0.0f, z * step - offset);
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f); // Displaced in shader
            v.uv = glm::vec2((float)x / widthQuads, (float)z / widthQuads);
            vertices.push_back(v);
        }
    }

    // Generate indices
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

void Terrain::generateHeightMap(int resolution) {
    m_heightData.resize(resolution * resolution);
    FastNoiseLiteWrapper noise;
    noise.setSeed(42); // Fixed seed for reproducibility

    float worldScale = m_worldSize; // Map texture coords to world space

    for (int z = 0; z < resolution; ++z) {
        for (int x = 0; x < resolution; ++x) {
            float u = (float)x / (resolution - 1);
            float v = (float)z / (resolution - 1);

            // Map to world coordinates
            float wx = u * worldScale - worldScale / 2.0f;
            float wz = v * worldScale - worldScale / 2.0f;

            // Multi-octave fractal noise for natural terrain variation
            float h = 0.0f;
            
            // Base terrain noise - slightly stronger rolling hills
            float amplitude = 0.020f;  // increased for stronger base
            float frequency = 0.28f;    // slightly higher frequency for more variation
            for (int octave = 0; octave < 2; ++octave) {  // Only 2 octaves
                h += noise.getFractalNoise(wx * frequency, wz * frequency) * amplitude;
                amplitude *= 0.5f;      // Faster falloff
                frequency *= 1.5f;      // Slower frequency increase
            }
            
            // Add large-scale rolling hills (main terrain shape)
            float hills = noise.getFractalNoise(wx * 0.12f, wz * 0.12f) * 0.07f;  // slightly stronger
            h += hills;

            // Valley system - very shallow depression
            float valleyCenter = 0.0f;
            float valleyWidth = worldScale * 0.4f;  // Wider valley
            float distToValley = std::abs(wz - valleyCenter);
            float valleyDepth = std::exp(-distToValley * distToValley / (2.0f * valleyWidth * valleyWidth)) * 0.06f;  // Much shallower
            // Add very subtle erosion variation
            float erosion = noise.getFractalNoise(wx * 0.5f, wz * 0.2f) * 0.008f;
            h -= (valleyDepth + erosion);

            // Plateau with gentle edge
            float plateauEdge = glm::smoothstep(-worldScale * 0.5f, worldScale * 0.1f, wx);
            float plateauNoise = noise.getFractalNoise(wx * 0.6f, wz * 0.6f) * 0.012f;
            float sideElevation = (plateauEdge + plateauNoise) * 0.20f;
            h += sideElevation;

            // Gentle hills instead of mountains
            float hill1 = std::exp(-((wx + worldScale * 0.3f)*(wx + worldScale * 0.3f) + wz*wz) / 
                (2.0f * (worldScale * 0.25f)*(worldScale * 0.25f))) * 0.10f;  // Gentle hill
            float hill2 = std::exp(-((wx - worldScale * 0.25f)*(wx - worldScale * 0.25f) + (wz + worldScale * 0.2f)*(wz + worldScale * 0.2f)) / 
                (2.0f * (worldScale * 0.30f)*(worldScale * 0.30f))) * 0.08f;  // Gentle hill
            h += hill1 + hill2;
            
            // Very subtle surface detail (like small rocks/terrain texture)
            float detail = noise.getFractalNoise(wx * 2.0f, wz * 2.0f) * 0.006f;  // slightly stronger
            h += detail;

            // Add patchy variation to create different biomes/textured areas
            float patchMask = noise.getFractalNoise(wx * 0.07f, wz * 0.07f) * 0.5f + 0.5f;
            h += patchMask * 0.02f; // small patch height variation

            // Clamp to 0-1 range
            h = std::max(0.0f, std::min(1.0f, h));

            m_heightData[z * resolution + x] = h;
        }
    }

    if (m_heightMapTex) glDeleteTextures(1, &m_heightMapTex);
    glGenTextures(1, &m_heightMapTex);
    glBindTexture(GL_TEXTURE_2D, m_heightMapTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, resolution, resolution, 0, GL_RED, GL_FLOAT, m_heightData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

float Terrain::getHeight(float x, float z) const {
    // Map world pos to 0..1
    float offset = m_worldSize / 2.0f;
    float u = (x + offset) / m_worldSize;
    float v = (z + offset) / m_worldSize;

    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return 0.0f;

    int ix = (int)(u * (m_resolution - 1));
    int iz = (int)(v * (m_resolution - 1));
    
    // Clamp
    ix = std::max(0, std::min(ix, m_resolution - 1));
    iz = std::max(0, std::min(iz, m_resolution - 1));

    float normalized = m_heightData[iz * m_resolution + ix];
    return normalized * recommendedHeightScale();
}

// Compute normals from the heightmap and update the mesh vertex normals in-place.
// This keeps the mesh positions as a flat grid (so shader displacement still works),
// but uses accurate CPU-computed normals for lighting.
void Terrain::updateMeshNormalsFromHeightmap() {
    if (!m_mesh) return;
    FastNoiseLiteWrapper noise;
    noise.setSeed(42);

    int widthQuads = m_resolution - 1;
    std::vector<Vertex> vertices;
    std::vector<unsigned> indices;
    vertices.reserve(m_resolution * m_resolution);
    indices.reserve(widthQuads * widthQuads * 6);

    float step = m_worldSize / (float)widthQuads;
    float offset = m_worldSize / 2.0f;

    // Height scale used by shader - read recommended value from Terrain
    float heightScaleForNormals = recommendedHeightScale();

    for (int z = 0; z <= widthQuads; ++z) {
        for (int x = 0; x <= widthQuads; ++x) {
            int idx = z * (widthQuads + 1) + x;
            Vertex v;
            // flat XZ positions (shader will displace them using the heightmap)
            v.position = glm::vec3(x * step - offset, 0.0f, z * step - offset);
            v.uv = glm::vec2((float)x / widthQuads, (float)z / widthQuads);

            // Compute central-difference slope in height (world units)
            float hC = m_heightData[idx] * heightScaleForNormals;
            float hL = (x > 0) ? m_heightData[idx - 1] * heightScaleForNormals : hC;
            float hR = (x < m_resolution - 1) ? m_heightData[idx + 1] * heightScaleForNormals : hC;
            float hD = (z > 0) ? m_heightData[idx - (m_resolution)] * heightScaleForNormals : hC;
            float hU = (z < m_resolution - 1) ? m_heightData[idx + (m_resolution)] * heightScaleForNormals : hC;

            float dx = (hR - hL) / (2.0f * step);
            float dz = (hU - hD) / (2.0f * step);

            // Add micro-detail to normals using small-scale noise (configurable amplitude/frequency)
            float microX = noise.getFractalNoise(v.position.x * m_microFrequency, v.position.z * m_microFrequency) * m_microAmplitude;
            float microZ = noise.getFractalNoise(v.position.x * m_microFrequency + 12.3f, v.position.z * m_microFrequency + 9.8f) * m_microAmplitude;

            glm::vec3 n = glm::normalize(glm::vec3(-dx + microX, 1.0f, -dz + microZ));
            v.normal = n;
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);

            vertices.push_back(v);
        }
    }

    // Rebuild index list (same as buildMesh)
    for (int z = 0; z < widthQuads; ++z) {
        for (int x = 0; x < widthQuads; ++x) {
            int topLeft = z * (widthQuads + 1) + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * (widthQuads + 1) + x;
            int bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            indices.push_back(bottomRight);
            indices.push_back(bottomLeft);
        }
    }

    // Update the mesh VBO with new normals (positions remain flat so shader displacement still applies)
    m_mesh->setData(vertices, indices);
}


