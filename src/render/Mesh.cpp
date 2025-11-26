// render/Mesh.cpp
#include "Mesh.h"
#include <glad/glad.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>

static void processMesh(aiMesh* mesh, std::vector<Vertex>& vertices, std::vector<unsigned>& indices) {
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        if (mesh->HasNormals()) {
            vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        }
        if (mesh->mTextureCoords[0]) {
            vertex.uv = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        } else {
            vertex.uv = glm::vec2(0.0f, 0.0f);
        }
        if (mesh->HasTangentsAndBitangents()) {
            vertex.tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
        }
        vertices.push_back(vertex);
    }
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
}

static void processNode(aiNode* node, const aiScene* scene, std::vector<Vertex>& vertices, std::vector<unsigned>& indices) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        processMesh(mesh, vertices, indices);
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene, vertices, indices);
    }
}

Mesh Mesh::loadFromFile(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return Mesh(); // return empty mesh
    }
    std::vector<Vertex> vertices;
    std::vector<unsigned> indices;
    processNode(scene->mRootNode, scene, vertices, indices);
    Mesh mesh;
    mesh.setData(vertices, indices);
    return mesh;
}

void Mesh::setData(const std::vector<Vertex>& vertices, const std::vector<unsigned>& indices){
    m_indexCount = (int)indices.size();
    if(m_vao==0){
        glGenVertexArrays(1,&m_vao);
        glGenBuffers(1,&m_vbo);
        glGenBuffers(1,&m_ebo);
    }
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER,m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned), indices.data(), GL_STATIC_DRAW);
    // position (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0);
    // normal (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(sizeof(glm::vec3)));
    // uv (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(sizeof(glm::vec3)*2));
    // tangent (location 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(sizeof(glm::vec3)*2 + sizeof(glm::vec2)));
    glBindVertexArray(0);
}

void Mesh::bind() const { glBindVertexArray(m_vao); }