// render/Renderer.h
// Responsibility: Encapsulate GPU frame lifecycle and draw submission.
// Future additions: shader management, uniform buffers, culling, batching.
// beginFrame(): prepare GL state for new frame (clear, set viewport).
// endFrame(): finalize state (in multi-pass setups, would resolve buffers).

#pragma once
#include <glm/glm.hpp>
class Renderer {
public:
    bool init();
    void beginFrame(int fbWidth, int fbHeight);
    void endFrame();
    void drawMesh(class Mesh& mesh, class Shader& shader, const class Camera& cam, const glm::mat4& model);
};
