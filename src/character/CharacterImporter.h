#pragma once

#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <unordered_map>
#include <vector>

struct SkinnedVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::uvec4 boneIds;
    glm::vec4 boneWeights;
};

struct BoneInfo {
    glm::mat4 offset;
    glm::mat4 finalTransform{1.0f};
    int parentIndex = -1;
};

struct AnimationChannel {
    std::string boneName;
    std::vector<std::pair<double, glm::vec3>> positionKeys;
    std::vector<std::pair<double, glm::quat>> rotationKeys;
    std::vector<std::pair<double, glm::vec3>> scaleKeys;
};

struct AnimationClip {
    std::string name;
    double duration = 0.0;
    double ticksPerSecond = 25.0;
    std::unordered_map<std::string, AnimationChannel> channels;
};

struct SkinnedMesh {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ibo = 0;
    unsigned int indexCount = 0;
    unsigned int albedoTex = 0;
    glm::vec3 minBounds{0.0f};
    glm::vec3 maxBounds{0.0f};
    std::vector<BoneInfo> bones;
    std::unordered_map<std::string, int> boneLookup;
    std::vector<int> boneParents;
    std::vector<std::string> boneNames;
    int headBone = -1;
    int leftFootBone = -1;
    int rightFootBone = -1;
    std::vector<AnimationClip> clips;
};

class CharacterImporter {
public:
    SkinnedMesh load(const std::string& path);

private:
    void processMesh(const aiMesh* mesh, SkinnedMesh& outMesh);
    void extractSkeleton(const aiScene* scene, SkinnedMesh& outMesh);
    void extractAnimations(const aiScene* scene, SkinnedMesh& outMesh);
    void extractMaterial(const aiScene* scene,
                         const aiMesh* mesh,
                         const std::string& sourceDir,
                         SkinnedMesh& outMesh);
    void uploadBuffers(SkinnedMesh& mesh,
                       const std::vector<SkinnedVertex>& vertices,
                       const std::vector<uint32_t>& indices);
};
