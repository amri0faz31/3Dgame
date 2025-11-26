cmake_minimum_required(VERSION 3.16)
project(LighthouseWorld LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_IMGUI "Build ImGui integration" ON)

# Export compile commands for IDE IntelliSense (VS Code etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Fetch GLFW
FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG        3.3.8
)
FetchContent_MakeAvailable(glfw)

# Fetch GLAD (OpenGL loader)
FetchContent_Declare(
	glad
	GIT_REPOSITORY https://github.com/Dav1dde/glad.git
	GIT_TAG        v0.1.36
)
FetchContent_MakeAvailable(glad)

# Fetch GLM (header-only math library)
FetchContent_Declare(
	glm
	URL https://github.com/g-truc/glm/archive/refs/tags/0.9.9.8.zip
)
FetchContent_MakeAvailable(glm)

# Fetch Assimp (for loading 3D models)
FetchContent_Declare(
	assimp
	GIT_REPOSITORY https://github.com/assimp/assimp.git
	GIT_TAG        v5.3.1
)
FetchContent_MakeAvailable(assimp)

# GLAD: For simplicity user should add generated glad sources under src/vendor/glad.
# (Alternatively FetchContent could grab a repo; here we expect a local glad.c and glad headers.)

add_subdirectory(src)
