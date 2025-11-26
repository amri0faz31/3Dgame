// main.cpp
// Entry point: constructs Window & Game, runs the primary loop.
// Current behavior: placeholder window & empty update/render cycle.
// Next steps: integrate GLFW, OpenGL context, real rendering calls and Time::tick().

#include "core/Game.h"
#include "platform/Window.h"
#include "core/Time.h"
#include <iostream>

Window* g_windowPtr = nullptr; // global access for simplicity (refactor later)

int main(){
    Window window(1280, 720, "Lighthouse World");
    g_windowPtr = &window;
    if(!window.init()){
        std::cerr << "Failed to initialize window" << std::endl;
        return 1;
    }

    Game game;
    if(!game.init()){
        std::cerr << "Failed to initialize game" << std::endl;
        return 2;
    }

    while(!window.shouldClose()){
        window.pollEvents();
        Time::tick();
        game.update();
        game.render();
        window.swapBuffers();
    }

    game.shutdown();
    window.shutdown();
    return 0;
}
