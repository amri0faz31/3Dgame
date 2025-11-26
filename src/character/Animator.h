#pragma once

#include "CharacterImporter.h"
#include <glm/glm.hpp>
#include <vector>

enum class CharacterState { Idle, Run };

class Animator {
public:
    Animator(const SkinnedMesh& mesh);

    void play(CharacterState state, bool immediate = false);
    void setPlaybackSpeed(float speed) { m_playbackSpeed = speed; }
    void update(double dt);

    const std::vector<glm::mat4>& boneMatrices() const { return m_finalMatrices; }
    const std::vector<glm::mat4>& globalPose() const { return m_globalPose; }

private:
    const SkinnedMesh& m_mesh;
    const AnimationClip* m_currentClip = nullptr;
    const AnimationClip* m_nextClip = nullptr;
    CharacterState m_state = CharacterState::Idle;
    CharacterState m_previousState = CharacterState::Idle;
    double m_currentTime = 0.0;
    double m_blendTime = 0.0;
    double m_blendDuration = 0.05;  // Very fast, near-instant
    bool m_blending = false;
    float m_playbackSpeed = 1.0f;  // Animation speed multiplier

    std::vector<glm::mat4> m_finalMatrices;
    std::vector<glm::mat4> m_globalPose;

    const AnimationClip* clipForState(CharacterState state) const;
    void readNodeHierarchy(int boneIndex,
                           const glm::mat4& parentTransform,
                           const AnimationClip* clip,
                           double time,
                           std::vector<glm::mat4>& outGlobals,
                           std::vector<glm::mat4>& outFinals);
};

struct CharacterController {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    float moveSpeed = 6.0f;  // Run speed when W is pressed

    CharacterState update(double dt, bool forward, const glm::vec3& moveDirection);
};
