# Character System Integration

The new character subsystem is split into a few focused classes located under
`src/character/`:

- `CharacterImporter` loads the GLB via Assimp, builds the skinned mesh VAO,
  extracts the skeleton hierarchy, and caches every animation clip.
- `Animator` evaluates an animation clip on the CPU and writes one bone matrix
  per joint (`Bones` UBO binding 0). Idle/Walk/Run switching is provided with a
  0.1 s crossfade.
- `CharacterController` reads player intent (W / Shift+W) and keeps the
  character aligned with the sampled terrain height.
- `ThirdPersonCamera` keeps the camera behind/above the character with a damped
  spring arm so it feels responsive but stable.

`scene/TerrainSampler` exposes two helpers:

```cpp
void setActiveTerrain(Terrain* terrain);
float getTerrainHeightAt(float worldX, float worldZ);
```

Call `setActiveTerrain(m_terrain)` after terrain generation (Game.cpp already
does this) so everyone can request heights.

## Typical Usage

```cpp
CharacterImporter importer;
SkinnedMesh heroMesh = importer.load("assets/models/sponge.glb");
Animator animator(heroMesh);
CharacterController controller;
controller.setTerrainSampler(getTerrainHeightAt);
ThirdPersonCamera camera;
camera.attach(&controller.position, &controller.yaw);

// UBO for Bones
GLuint boneUBO = 0;
glGenBuffers(1, &boneUBO);
glBindBuffer(GL_UNIFORM_BUFFER, boneUBO);
glBufferData(GL_UNIFORM_BUFFER, heroMesh.bones.size() * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
glBindBufferBase(GL_UNIFORM_BUFFER, 0, boneUBO);

while(running){
    double dt = Time::delta();
    bool forward = input.isKeyDown(GLFW_KEY_W);
    bool run = forward && input.isKeyDown(GLFW_KEY_LEFT_SHIFT);

    CharacterState desired = controller.update(dt, forward, run, camera.forward());
    animator.play(desired);
    animator.update(dt);

    glBindBuffer(GL_UNIFORM_BUFFER, boneUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0,
                    animator.boneMatrices().size() * sizeof(glm::mat4),
                    animator.boneMatrices().data());

    camera.update(dt, input.mouseDeltaX(), input.mouseDeltaY());

    renderer.beginFrame(w, h);
    renderTerrain();
    renderCharacter(heroMesh, characterShader, controller.position, controller.yaw);
    renderWater();
    renderer.endFrame();
}
```

`renderCharacter` simply binds `heroMesh.vao`, sets `uModel/uView/uProj`, binds
`Bones` UBO at binding 0, and issues `glDrawElements`. The GLSL pair lives under
`assets/shaders/skinned.vert` and `skinned.frag`.
