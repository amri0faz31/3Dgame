// render/Camera.h
// Simple free-fly camera with yaw/pitch + perspective projection.
#pragma once
#include <glm/glm.hpp>

class Camera {
public:
    Camera();
    void setViewport(int w, int h);
    void update(float dt, void* windowHandle); // polls input
    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix() const;
    glm::vec3 position() const { return m_pos; }
    void setPosition(const glm::vec3& pos) { m_pos = pos; }
    void setPitch(float p) { m_pitch = p; }
    void setYaw(float y) { m_yaw = y; }
    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }
private:
    glm::vec3 m_pos{0.f,2.f,5.f};
    float m_yaw = -90.f; // facing -Z
    float m_pitch = -20.f;
    int m_width = 1280;
    int m_height = 720;
    float m_fov = 60.f;
    bool m_firstMouse = true;
    double m_lastX = 0.0;
    double m_lastY = 0.0;
    float m_sensitivity = 0.15f;
};