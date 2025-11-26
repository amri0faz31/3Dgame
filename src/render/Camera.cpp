// render/Camera.cpp
#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

Camera::Camera(){}

void Camera::setViewport(int w, int h){ m_width=w; m_height=h; }

void Camera::update(float dt, void* windowHandle){
    GLFWwindow* win = static_cast<GLFWwindow*>(windowHandle);
    const float speed = 50.f;
    glm::vec3 forward{
        cos(glm::radians(m_yaw))*cos(glm::radians(m_pitch)),
        sin(glm::radians(m_pitch)),
        sin(glm::radians(m_yaw))*cos(glm::radians(m_pitch))
    };
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));
    if(glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) m_pos += forward * speed * dt;
    if(glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) m_pos -= forward * speed * dt;
    if(glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) m_pos -= right * speed * dt;
    if(glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) m_pos += right * speed * dt;
    if(glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS) m_pos += glm::vec3(0,1,0) * speed * dt;
    if(glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) m_pos -= glm::vec3(0,1,0) * speed * dt;

    // Mouse look
    double xpos, ypos; glfwGetCursorPos(win, &xpos, &ypos);
    if(m_firstMouse){ m_lastX = xpos; m_lastY = ypos; m_firstMouse=false; }
    double xoffset = xpos - m_lastX;
    double yoffset = m_lastY - ypos; // invert y
    m_lastX = xpos; m_lastY = ypos;
    m_yaw += (float)xoffset * m_sensitivity;
    m_pitch += (float)yoffset * m_sensitivity;
    if(m_pitch > 89.f) m_pitch = 89.f;
    if(m_pitch < -89.f) m_pitch = -89.f;
}

glm::mat4 Camera::viewMatrix() const {
    glm::vec3 forward{
        cos(glm::radians(m_yaw))*cos(glm::radians(m_pitch)),
        sin(glm::radians(m_pitch)),
        sin(glm::radians(m_yaw))*cos(glm::radians(m_pitch))
    };
    return glm::lookAt(m_pos, m_pos + forward, glm::vec3(0,1,0));
}

glm::mat4 Camera::projectionMatrix() const {
    return glm::perspective(glm::radians(m_fov), (float)m_width/(float)m_height, 0.1f, 2000.f);
}