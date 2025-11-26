// core/Game.h
// Responsibility: High-level orchestration of subsystems (renderer, scene, input, etc.).
// For now it only demonstrates lifecycle hooks: init, update, render, shutdown.
// Later expansions: references to Scene, ResourceManager, Systems (Lighting, Physics).

#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <array>
#include <memory>
#include <vector>
#include <random>
#include "character/CharacterImporter.h"
#include "character/Animator.h"
#include "character/ThirdPersonCamera.h"
#include "systems/CollisionSystem.h"
#include "systems/InteractionSystem.h"
#include "systems/MonsterAI.h"
#include "audio/AudioSystem.h"
class Game {
public:
    // init(): Set up subsystems & load initial assets.
    bool init();
    // update(): Advance simulation state each frame (physics, animations, timers).
    void update();
    // render(): Issue draw commands via Renderer.
    void render();
    // shutdown(): Cleanly release resources.
    void shutdown();
private:
    class Renderer* m_renderer = nullptr;
    class Shader* m_shader = nullptr;
    class Shader* m_waterShader = nullptr;
    class Shader* m_grassShader = nullptr;
    class Camera* m_camera = nullptr;
    class Camera* m_freeCamera = nullptr;
    class Terrain* m_terrain = nullptr;
    class Water* m_water = nullptr;
    class Sky* m_sky = nullptr;
    // Shadow mapping resources
    class Shader* m_depthShader = nullptr;
    class Shader* m_skinnedDepthShader = nullptr;
    unsigned int m_shadowFBO = 0;
    unsigned int m_shadowTex = 0;
    int m_shadowMapSize = 4096;  // Higher quality shadows
    // LightParams: simple directional light used for terrain shading.
    // direction: points FROM light toward scene (so fragment uses -direction).
    // color: radiance multiplier for diffuse & specular components.
    // ambient: constant low-frequency light.
    // specularStrength: scales highlight intensity.
    // shininess: Phong exponent controlling tightness of highlight.
    struct LightParams {
        glm::vec3 direction{ -0.4f, -1.0f, -0.3f }; // downward/diagonal
        glm::vec3 color{ 1.0f, 0.95f, 0.85f };
        float ambient = 0.25f;
        float specularStrength = 0.5f;
        float shininess = 32.f;
    } m_light;
    struct PointLight {
        glm::vec3 position{0.0f};
        glm::vec3 color{1.0f};
        float baseIntensity = 1.0f;
        float intensity = 1.0f;
        float radius = 20.0f;
        bool enabled = false;
        float flickerTimer = 0.0f;
    } m_campfireLight;
    // Procedural grass texture (GL handle)
    unsigned int m_grassTexture = 0;
    unsigned int m_texFungus = 0;
    unsigned int m_texSandgrass = 0;
    unsigned int m_texRocks = 0;
    unsigned int m_waveHeightTex[2] = {0, 0};
    unsigned int m_waveNormalTex[2] = {0, 0};
    unsigned int m_envCubemap = 0;
    unsigned int m_grassVAO = 0;
    unsigned int m_grassVBO = 0;
    unsigned int m_grassBillboardTex = 0;
    int m_grassPatchCount = 0;
    class Shader* m_fireShader = nullptr;
    unsigned int m_fireVAO = 0;
    unsigned int m_fireQuadVBO = 0;
    unsigned int m_fireInstanceVBO = 0;
    unsigned int m_fireTexture = 0;
    struct FireParticle {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f, 1.0f, 0.0f};
        float life = 0.0f;
        float maxLife = 1.0f;
        float size = 1.0f;
        float seed = 0.0f;
    };
    std::vector<FireParticle> m_fireParticles;
    std::mt19937 m_fireRng{12345};
    glm::vec3 m_campfireEmitterPos{0.0f};
    bool m_fireFXReady = false;
    void initCampfireFireFX();
    void respawnFireParticle(FireParticle& particle);
    void updateFireParticles(float dt);
    void uploadFireParticlesToGPU();
    void renderFireParticles(const glm::mat4& viewProj, const glm::mat4& viewMatrix);
    void updateCampfireLight(float dt);

    float m_waterLevel = 10.0f;
    float m_grassWaterGap = 6.0f; // keep grass at least 6 units above water plane

    class Shader* m_characterShader = nullptr;
    SkinnedMesh m_characterMesh;
    std::unique_ptr<Animator> m_animator;
    CharacterController m_characterController;
    ThirdPersonCamera m_thirdPersonCamera;
    unsigned int m_characterAlbedoTex = 0;
    unsigned int m_boneUBO = 0;
    bool m_characterReady = false;
    std::array<glm::mat4, 128> m_bonePalette{};
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    bool m_firstMouseSample = true;
    bool m_useThirdPersonCamera = true;
    bool m_cameraToggleHeld = false;
    glm::vec3 m_characterAimPoint{0.0f};
    
    // Day/Night cycle
    bool m_isNightMode = false;
    bool m_nightToggleHeld = false;
    float m_characterScale = 1.0f;
    float m_characterHeight = 1.0f;
    float m_characterFeetOffset = 0.0f;
    glm::mat4 m_characterModelMatrix{1.0f};
    
    // Collision system
    CollisionSystem m_collisionSystem;
    size_t m_characterCollisionBody = 0;
    
    // Terrain Region System
    struct TerrainRegion {
        std::string name;
        glm::vec2 minXZ;  // Minimum X,Z bounds
        glm::vec2 maxXZ;  // Maximum X,Z bounds
        float minY;       // Minimum Y (height)
        float maxY;       // Maximum Y (height)
        std::string description;
    };
    std::vector<TerrainRegion> m_terrainRegions;
    bool m_showRegions = false;
    bool m_regionToggleHeld = false;
    
    // Static mesh structure (for non-animated models like lighthouse)
    struct StaticMesh {
        struct Part {
            unsigned int vao = 0;
            unsigned int vbo = 0;
            unsigned int ibo = 0;
            unsigned int vertexCount = 0;
            unsigned int indexCount = 0;
            unsigned int albedoTex = 0;
            glm::vec3 minBounds{0.0f};
            glm::vec3 maxBounds{0.0f};
        };
        std::vector<Part> parts;
        glm::vec3 minBounds{0.0f};
        glm::vec3 maxBounds{0.0f};
        unsigned int totalVertexCount = 0;
        unsigned int totalIndexCount = 0;
    };
    
    // Lighthouse model
    StaticMesh m_lighthouseMesh;
    glm::vec3 m_lighthousePosition{0.0f};
    float m_lighthouseScale = 1.0f;
    bool m_lighthouseReady = false;
    
    struct TreeInstance {
        glm::vec3 position{0.0f};
        float scale = 1.0f;
    };

    // Tree model
    StaticMesh m_treeMesh;
    std::vector<TreeInstance> m_treeInstances;
    bool m_treeReady = false;

    // Campfire model
    StaticMesh m_campfireMesh;
    glm::vec3 m_campfirePosition{0.0f};
    float m_campfireScale = 1.0f;
    bool m_campfireReady = false;

    // Forest hut model
    StaticMesh m_forestHutMesh;
    glm::vec3 m_forestHutPosition{0.0f};
    float m_forestHutScale = 1.0f;
    float m_forestHutYawDegrees = 0.0f;
    float m_forestHutPitchDegrees = 0.0f;
    bool m_forestHutReady = false;

    // New systems for gameplay
    AudioSystem m_audio;
    InteractionSystem m_interactions;
    MonsterAI m_monster;
    
    // Game state
    bool m_hasNote = false;
    bool m_hasTorch = false;
    bool m_torchLit = false;
    bool m_reachedLighthouse = false;
    bool m_gameLost = false;
    float m_beaconRotation = 0.0f;
    int m_nearestInteractableID = -1;
    
    // Audio sources
    int m_soundOcean = -1;
    int m_soundFire = -1;
    int m_soundWind = -1;
    int m_soundPickup = -1;
    int m_soundMonster = -1;
    
    // Monster model
    StaticMesh m_monsterMesh;
    bool m_monsterReady = false;

    void setupCampfireLight();
    void setupGameplayElements();
    void updateGameplay(float dt);
    void checkInteractions();
    void onNotePickup();
    void onTorchPickup();
    void onTorchLight();
    void checkMonsterDetection();
    
    void initTerrainRegions();
    void renderRegionOverlay();
    void renderUI();
    std::string getRegionAtPosition(const glm::vec3& pos) const;
    bool loadStaticModel(const std::string& path, StaticMesh& outMesh);
};
