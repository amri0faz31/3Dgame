set(SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

add_library(engine
  platform/Window.cpp
  core/Game.cpp
  core/Time.cpp
  render/Renderer.cpp
  render/Shader.cpp
  render/Mesh.cpp
  render/Camera.cpp
  character/CharacterImporter.cpp
  character/Animator.cpp
  character/ThirdPersonCamera.cpp
  scene/Terrain.cpp
  scene/TerrainSampler.cpp
  scene/Sky.cpp
  scene/Water.cpp
  scene/Model.cpp
  systems/CollisionSystem.cpp
)

target_include_directories(engine PUBLIC ${SRC_ROOT})
# Provide access to stb_image bundled with GLFW so Game.cpp can load textures
if(DEFINED glfw_SOURCE_DIR)
  target_include_directories(engine PRIVATE ${glfw_SOURCE_DIR}/deps)
endif()
if(DEFINED assimp_SOURCE_DIR)
  target_include_directories(engine PRIVATE ${assimp_SOURCE_DIR}/contrib/stb)
endif()

target_link_libraries(engine PUBLIC glfw glm glad assimp)

if(BUILD_IMGUI)
  include(FetchContent)
  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.89.9
  )
  FetchContent_MakeAvailable(imgui)

  target_include_directories(engine PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_sources(engine PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  )
  target_link_libraries(engine PUBLIC glfw)
endif()

add_executable(lighthouse main.cpp)

target_link_libraries(lighthouse PRIVATE engine)
