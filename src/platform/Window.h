// platform/Window.h
// Responsibility: Owns the native OS window and (later) the OpenGL context.
// Current state: Placeholder implementation that will be replaced by GLFW.
// Why separate? Decouples platform concerns (input events, window lifecycle)
// from higher-level game logic contained in Game.

#pragma once
#include <string>

struct GLFWwindow; // forward declaration

class Window {
public:
    // Construct with desired width/height and title. Does NOT create native window yet.
    Window(int w, int h, const std::string& title);
    ~Window();

    // init(): Create the underlying window & GL context (in future).
    bool init();
    // pollEvents(): Process pending OS events (keyboard/mouse/close). Placeholder for now.
    void pollEvents();
    // swapBuffers(): Present the rendered frame. Placeholder until GLFW integrated.
    void swapBuffers();
    // shouldClose(): Query close flag (set when user requests exit).
    bool shouldClose() const; // queries GLFWwindowShouldClose
    int width() const { return m_width; }
    int height() const { return m_height; }
    GLFWwindow* nativeHandle() const { return m_handle; }
    // shutdown(): Release resources tied to window/context.
    void shutdown();
    void onFramebufferResized(int w, int h); // internal resize hook
    void setMouseCaptured(bool captured);
    bool isMouseCaptured() const { return m_mouseCaptured; }
private:
    int m_width;
    int m_height;
    std::string m_title;
    GLFWwindow* m_handle = nullptr; // native window handle
    bool m_mouseCaptured = false;  // Start with cursor visible for UI
    static void framebufferSizeCallback(GLFWwindow* win, int w, int h);
};
