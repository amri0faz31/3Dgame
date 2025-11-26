# Lighthouse World (OpenGL C++ Skeleton)

## Overview
Initial scaffold for an interactive storyworld rendering engine. This is a minimal starting point: window, game loop, renderer stubs.

## Build Requirements
- CMake >= 3.16
- A C++20 compiler (gcc 11+/clang 13+ recommended)
- Internet access for CMake FetchContent (it will download GLFW, GLAD, GLM)

Install tools (Ubuntu/Debian):
```bash
sudo apt update
sudo apt install -y build-essential cmake xorg-dev libxrandr-dev libxi-dev libxxf86vm-dev libxcursor-dev libgl1-mesa-dev
```

Fedora/RHEL:
```bash
sudo dnf install -y @development-tools cmake libXrandr-devel libXi-devel libXxf86vm-devel libXcursor-devel mesa-libGL-devel
```

Arch:
```bash
sudo pacman -S --needed base-devel cmake libxrandr libxi libxxf86vm libxcursor mesa
```

## Build
```bash
mkdir build
cd build
cmake ..
cmake --build . -j
./lighthouse
```

## Current Features
- Real GLFW window + OpenGL context via GLAD
- Animated clear color
- Simple Shader, Mesh, Camera abstractions
- Procedurally generated terrain grid (200x200) with a basic Lambert-like color
- WASD camera movement (mouse look TBD)

## Next Steps
- Editor IntelliSense note: After first CMake configure, copy or symlink `build/compile_commands.json` into the project root if VS Code doesn’t auto-detect:
```bash
ln -s build/compile_commands.json .
```
VS Code will then resolve `<glad/glad.h>` includes without squiggles.
- Mouse look & cursor capture
- Basic OBJ model loading (Assimp) and material support
- UI (ImGui) to tweak lighting and terrain params
- Physics placeholders for later interactions
