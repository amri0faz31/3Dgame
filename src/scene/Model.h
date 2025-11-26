#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include "../render/Mesh.h"

// Simple Assimp-based model loader that converts meshes into the project's Mesh
// representation so you can drop .obj/.fbx files into `assets/models/` and load them.
// Usage:
//   Model model;
//   if (model.loadFromFile("assets/models/my_model.obj")) {
//       // retrieve Mesh* objects via model.meshes() and render them with Renderer
//   }

class Model {
public:
    Model();
    ~Model();

    // Load model using Assimp; returns true on success
    bool loadFromFile(const std::string& filepath);

    // Mesh ownership: Model owns the Mesh objects and will delete them on destruction
    const std::vector<Mesh*>& meshes() const { return m_meshes; }

private:
    // Convert Assimp mesh to engine Mesh
    Mesh* convertMesh(struct aiMesh* aMesh, const struct aiScene* scene);

    std::vector<Mesh*> m_meshes;
};
