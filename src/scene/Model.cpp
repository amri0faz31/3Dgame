#include "Model.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>

Model::Model() {}

Model::~Model() {
    for (auto m : m_meshes) delete m;
}

bool Model::loadFromFile(const std::string& filepath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filepath,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices);

    if (!scene) {
        std::cerr << "[Model] Failed to load: " << filepath << " - " << importer.GetErrorString() << std::endl;
        return false;
    }

    // Clean previous meshes
    for (auto m : m_meshes) delete m;
    m_meshes.clear();

    // Convert all meshes
    for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh* aMesh = scene->mMeshes[i];
        Mesh* m = convertMesh(aMesh, scene);
        if (m) m_meshes.push_back(m);
    }

    return !m_meshes.empty();
}

Mesh* Model::convertMesh(aiMesh* aMesh, const aiScene* scene) {
    if (!aMesh) return nullptr;

    std::vector<Vertex> verts;
    std::vector<unsigned> idx;
    verts.reserve(aMesh->mNumVertices);
    idx.reserve(aMesh->mNumFaces * 3);

    for (unsigned v = 0; v < aMesh->mNumVertices; ++v) {
        Vertex vert;
        // Positions
        vert.position = glm::vec3(
            aMesh->mVertices[v].x,
            aMesh->mVertices[v].y,
            aMesh->mVertices[v].z);
        // Normals (if present)
        if (aMesh->HasNormals()) {
            vert.normal = glm::vec3(
                aMesh->mNormals[v].x,
                aMesh->mNormals[v].y,
                aMesh->mNormals[v].z);
        } else {
            vert.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        // UVs (take first set if present)
        if (aMesh->mTextureCoords && aMesh->mTextureCoords[0]) {
            vert.uv = glm::vec2(aMesh->mTextureCoords[0][v].x, aMesh->mTextureCoords[0][v].y);
        } else {
            vert.uv = glm::vec2(0.0f, 0.0f);
        }
        // Tangent (if present)
        if (aMesh->HasTangentsAndBitangents()) {
            vert.tangent = glm::vec3(
                aMesh->mTangents[v].x,
                aMesh->mTangents[v].y,
                aMesh->mTangents[v].z);
        } else {
            vert.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        }
        verts.push_back(vert);
    }

    // Faces / indices
    for (unsigned f = 0; f < aMesh->mNumFaces; ++f) {
        aiFace& face = aMesh->mFaces[f];
        if (face.mNumIndices != 3) continue; // skip non-triangle
        idx.push_back(face.mIndices[0]);
        idx.push_back(face.mIndices[1]);
        idx.push_back(face.mIndices[2]);
    }

    Mesh* mesh = new Mesh();
    mesh->setData(verts, idx);
    return mesh;
}
