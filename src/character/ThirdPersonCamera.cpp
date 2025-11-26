#include "ThirdPersonCamera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <cmath>

void ThirdPersonCamera::setTarget(glm::vec3* characterPosition){
    m_targetPos = characterPosition;
}

void ThirdPersonCamera::setFollowConfig(float pivotHeight, float verticalOffset, float followDistance){
    m_pivotHeight = pivotHeight;
    m_verticalOffset = verticalOffset;
    m_followDistance = followDistance;
}

void ThirdPersonCamera::update(double dt, float mouseDX, float mouseDY,
                               const std::function<float(float, float)>& terrainHeightFn){
    if(!m_targetPos) return;
    if(std::abs(mouseDX) > 0.0001f){
        m_yawDegrees += mouseDX * m_sensitivity;
    }
    if(std::abs(mouseDY) > 0.0001f){
        m_pitchDegrees = glm::clamp(m_pitchDegrees + mouseDY * m_sensitivity, -70.0f, 60.0f);
    }

    float yawRad = glm::radians(m_yawDegrees);
    float pitchRad = glm::radians(m_pitchDegrees);
    glm::vec3 forward;
    forward.x = std::sin(yawRad) * std::cos(pitchRad);
    forward.y = std::sin(pitchRad);
    forward.z = std::cos(yawRad) * std::cos(pitchRad);
    forward = glm::normalize(forward);

    glm::vec3 pivot = *m_targetPos;
    glm::vec3 desiredPos = pivot - forward * m_followDistance;
    desiredPos.y += m_verticalOffset;
    if(terrainHeightFn){
        float terrainY = terrainHeightFn(desiredPos.x, desiredPos.z);
        desiredPos.y = std::max(desiredPos.y, terrainY + 1.0f);
    }
    m_cameraPos = smoothPos(desiredPos, dt);
    glm::vec3 look = pivot - m_cameraPos;
    if(glm::length2(look) < 1e-6f){
        look = forward;
    }
    m_forward = glm::normalize(look);
}

glm::vec3 ThirdPersonCamera::smoothPos(glm::vec3 desired, double dt){
    const float stiffness = 8.0f;
    float t = glm::clamp(static_cast<float>(dt) * stiffness, 0.0f, 1.0f);
    return glm::mix(m_cameraPos, desired, t);
}

glm::mat4 ThirdPersonCamera::viewMatrix() const{
    return glm::lookAt(m_cameraPos,
                       m_cameraPos + m_forward,
                       glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 ThirdPersonCamera::projectionMatrix(float fovDeg, float aspect,
                                              float nearPlane, float farPlane) const{
    return glm::perspective(glm::radians(fovDeg), aspect, nearPlane, farPlane);
}

glm::vec3 ThirdPersonCamera::predictForward(float mouseDX, float mouseDY) const{
    float yawAdvance = m_yawDegrees + mouseDX * m_sensitivity;
    float pitchAdvance = glm::clamp(m_pitchDegrees + mouseDY * m_sensitivity, -70.0f, 60.0f);
    float yawRad = glm::radians(yawAdvance);
    float pitchRad = glm::radians(pitchAdvance);
    glm::vec3 fwd;
    fwd.x = std::sin(yawRad) * std::cos(pitchRad);
    fwd.y = std::sin(pitchRad);
    fwd.z = std::cos(yawRad) * std::cos(pitchRad);
    if(glm::length2(fwd) < 1e-6f){
        return glm::vec3(0.0f, 0.0f, -1.0f);
    }
    return glm::normalize(fwd);
}
