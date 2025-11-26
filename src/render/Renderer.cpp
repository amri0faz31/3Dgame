// render/Renderer.cpp
// Placeholder implementation; actual OpenGL calls will be added once context exists.

#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"
#include "Camera.h"
#include <iostream>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

static float s_timeAccum = 0.0f; // for color animation

bool Renderer::init(){
    std::cout << "[Renderer] Init" << std::endl;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    return true;
}

void Renderer::beginFrame(int fbWidth, int fbHeight){
    // Sky blue background
    glViewport(0,0,fbWidth,fbHeight);
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::endFrame(){ }

void Renderer::drawMesh(Mesh& mesh, Shader& shader, const Camera& cam, const glm::mat4& model){
    shader.bind();
    shader.setMat4("uModel", model);
    shader.setMat4("uView", cam.viewMatrix());
    shader.setMat4("uProj", cam.projectionMatrix());
    mesh.bind();
    glDrawElements(GL_TRIANGLES, mesh.indexCount(), GL_UNSIGNED_INT, 0);
}
