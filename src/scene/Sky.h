#pragma once
#include "render/Shader.h"
#include "render/Camera.h"
#include <glm/glm.hpp>

class Sky {
public:
    bool init();
    void render(const Camera& cam, const glm::vec3& sunDir, bool isNight = false);
private:
    Shader m_shader;
};
