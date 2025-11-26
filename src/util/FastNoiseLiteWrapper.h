#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>

// Lightweight placeholder wrapper mimicking a FastNoiseLite-style interface.
// Provides fractal noise combining Perlin + Simplex + domain warp for macro/meso/micro detail.
class FastNoiseLiteWrapper {
public:
    void setSeed(int s){ m_seed = s; }
    float getFractalNoise(float x, float z) const {
        // Macro structure (low freq simplex) - increased frequency for closer noise
        float macro = glm::simplex(glm::vec2(x * 0.0032f + m_seed, z * 0.0032f + m_seed));
        // Domain warp for meso layer - increased frequency
        glm::vec2 warp(
            glm::perlin(glm::vec2(x * 0.02f + 17.0f, z * 0.02f + 3.1f)),
            glm::perlin(glm::vec2(x * 0.02f - 6.4f, z * 0.02f + 11.7f))
        );
        float meso = glm::perlin(glm::vec2(x * 0.01f, z * 0.01f) + warp * 15.0f);
        // Micro detail (higher freq fractal sum) - increased base frequency
        float micro = 0.0f;
        float amp = 1.0f;
        float freq = 0.08f;
        for(int i=0;i<4;++i){
            micro += amp * glm::perlin(glm::vec2(x * freq, z * freq));
            amp *= 0.5f;
            freq *= 2.0f;
        }
        return macro * 0.55f + meso * 0.35f + micro * 0.10f; // Weighted blend
    }
private:
    int m_seed = 1337;
};
