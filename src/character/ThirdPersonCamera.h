#pragma once
// Third-person follow camera with gentle smoothing.

#include <functional>
#include <glm/glm.hpp>

class ThirdPersonCamera {
public:
    void setTarget(glm::vec3* pivotPosition);
    void setFollowConfig(float pivotHeight, float verticalOffset, float followDistance);

    void update(double dt, float mouseDeltaX, float mouseDeltaY,
                const std::function<float(float, float)>& terrainHeightFn);

    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float fovDeg, float aspect, float nearPlane, float farPlane) const;

    glm::vec3 forward() const { return m_forward; }
    glm::vec3 position() const { return m_cameraPos; }
    glm::vec3 predictForward(float mouseDX, float mouseDY) const;

private:
    glm::vec3* m_targetPos = nullptr;

    float m_followDistance = 4.0f;
    float m_verticalOffset = 0.5f;
    float m_pivotHeight = 1.0f;
    float m_yawDegrees = 0.0f;
    float m_pitchDegrees = -15.0f;
    float m_sensitivity = 0.1f;

    glm::vec3 m_cameraPos{0.0f};
    glm::vec3 m_forward{0.0f};

    glm::vec3 smoothPos(glm::vec3 desired, double dt);
};
