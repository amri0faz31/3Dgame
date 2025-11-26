// platform/Window.cpp
// Placeholder implementations; no actual OS window yet. Serves as scaffolding
// so higher-level architecture can compile while we layer in real GLFW usage later.

#include "Window.h"
#include <iostream>
#include <stdexcept>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

static void glfwErrorCallback(int code, const char* desc){
    std::cerr << "[GLFW Error] (" << code << "): " << desc << std::endl;
}

Window::Window(int w, int h, const std::string& title)
: m_width(w), m_height(h), m_title(title) {}

Window::~Window() {}

bool Window::init(){
    glfwSetErrorCallback(glfwErrorCallback);
    if(!glfwInit()){
        std::cerr << "[Window] Failed to init GLFW" << std::endl;
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    m_handle = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
    if(!m_handle){
        std::cerr << "[Window] Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(m_handle);
    glfwSwapInterval(1); // vsync

    // callbacks & cursor - start with cursor VISIBLE for UI interaction
    glfwSetWindowUserPointer(m_handle, this);
    glfwSetFramebufferSizeCallback(m_handle, framebufferSizeCallback);
    glfwSetInputMode(m_handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    m_mouseCaptured = false;  // Start with cursor visible

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cerr << "[Window] Failed to init GLAD" << std::endl;
        return false;
    }
    std::cout << "[Window] OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "[Window] Renderer: " << glGetString(GL_RENDERER) << std::endl;
    return true;
}

void Window::pollEvents(){
    glfwPollEvents();
}

void Window::swapBuffers(){
    glfwSwapBuffers(m_handle);
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_handle);
}

void Window::shutdown(){
    if(m_handle){
        glfwDestroyWindow(m_handle);
        m_handle = nullptr;
    }
    glfwTerminate();
}

void Window::onFramebufferResized(int w, int h){
    m_width = w; m_height = h;
}

void Window::setMouseCaptured(bool captured){
    m_mouseCaptured = captured;
    if(!m_handle) return;
    glfwSetInputMode(m_handle, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void Window::framebufferSizeCallback(GLFWwindow* win, int w, int h){
    if(auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win))){
        self->onFramebufferResized(w,h);
    }
}
