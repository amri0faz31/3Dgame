#include "Animator.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <iostream>

namespace {
static glm::vec3 interpolate(const std::vector<std::pair<double, glm::vec3>>& keys, double time){
    if(keys.empty()) return glm::vec3(0.0f);
    if(keys.size() == 1) return keys[0].second;
    for(size_t i=0; i<keys.size() - 1; ++i){
        if(time < keys[i+1].first){
            double span = keys[i+1].first - keys[i].first;
            double factor = span > 0.0 ? (time - keys[i].first) / span : 0.0;
            return glm::mix(keys[i].second, keys[i+1].second, static_cast<float>(factor));
        }
    }
    return keys.back().second;
}

static glm::quat interpolate(const std::vector<std::pair<double, glm::quat>>& keys, double time){
    if(keys.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if(keys.size() == 1) return keys[0].second;
    for(size_t i=0; i<keys.size() - 1; ++i){
        if(time < keys[i+1].first){
            double span = keys[i+1].first - keys[i].first;
            double factor = span > 0.0 ? (time - keys[i].first) / span : 0.0;
            return glm::slerp(keys[i].second, keys[i+1].second, static_cast<float>(factor));
        }
    }
    return keys.back().second;
}
}

Animator::Animator(const SkinnedMesh& mesh)
        : m_mesh(mesh),
            m_finalMatrices(mesh.bones.size(), glm::mat4(1.0f)),
            m_globalPose(mesh.bones.size(), glm::mat4(1.0f)) {
    play(CharacterState::Idle, true);
}

const AnimationClip* Animator::clipForState(CharacterState state) const {
    for(const auto& clip : m_mesh.clips){
        if(state == CharacterState::Idle && clip.name.find("Idle") != std::string::npos) return &clip;
        if(state == CharacterState::Run && clip.name.find("Run") != std::string::npos) return &clip;
    }
    return m_mesh.clips.empty() ? nullptr : &m_mesh.clips[0];
}

void Animator::play(CharacterState state, bool immediate){
    const AnimationClip* requested = clipForState(state);
    if(!requested) return;
    
    // Immediate state changes - no complex blending logic needed
    if(immediate || !m_currentClip){
        m_previousState = m_state;
        m_currentClip = requested;
        m_state = state;
        m_currentTime = 0.0;
        m_blending = false;
        return;
    }
    
    // Already playing this animation
    if(requested == m_currentClip) return;
    
    // Quick transition to new state
    m_previousState = m_state;
    m_nextClip = requested;
    m_blending = true;
    m_blendTime = 0.0;
    m_blendDuration = 0.05;  // Fast response
    m_state = state;
}

void Animator::update(double dt){
    if(!m_currentClip) return;
    double duration = m_currentClip->duration;
    double ticksPerSecond = m_currentClip->ticksPerSecond;
    double timeAdvance = dt * ticksPerSecond * m_playbackSpeed;  // Apply speed multiplier
    m_currentTime = std::fmod(m_currentTime + timeAdvance, duration);

    std::vector<glm::mat4> currentFinal(m_mesh.bones.size(), glm::mat4(1.0f));
    std::vector<glm::mat4> currentGlobals(m_mesh.bones.size(), glm::mat4(1.0f));
    readNodeHierarchy(-1, glm::mat4(1.0f), m_currentClip, m_currentTime, currentGlobals, currentFinal);

    if(m_blending && m_nextClip){
        double nextDuration = m_nextClip->duration;
        double nextTicks = m_nextClip->ticksPerSecond;
        double nextTime = std::fmod(m_currentTime, nextDuration);
        std::vector<glm::mat4> nextFinal(m_mesh.bones.size(), glm::mat4(1.0f));
        std::vector<glm::mat4> nextGlobals(m_mesh.bones.size(), glm::mat4(1.0f));
        readNodeHierarchy(-1, glm::mat4(1.0f), m_nextClip, nextTime, nextGlobals, nextFinal);

        float alpha = glm::clamp(static_cast<float>(m_blendTime / m_blendDuration), 0.0f, 1.0f);
        for(size_t i=0; i<m_finalMatrices.size(); ++i){
            glm::quat q0 = glm::quat_cast(currentFinal[i]);
            glm::quat q1 = glm::quat_cast(nextFinal[i]);
            glm::vec3 t0 = glm::vec3(currentFinal[i][3]);
            glm::vec3 t1 = glm::vec3(nextFinal[i][3]);
            glm::quat blendedRot = glm::slerp(q0, q1, alpha);
            glm::vec3 blendedPos = glm::mix(t0, t1, alpha);
            m_finalMatrices[i] = glm::translate(glm::mat4(1.0f), blendedPos) * glm::mat4_cast(blendedRot);

            glm::quat gq0 = glm::quat_cast(currentGlobals[i]);
            glm::quat gq1 = glm::quat_cast(nextGlobals[i]);
            glm::vec3 gp0 = glm::vec3(currentGlobals[i][3]);
            glm::vec3 gp1 = glm::vec3(nextGlobals[i][3]);
            glm::quat blendedGlobalRot = glm::slerp(gq0, gq1, alpha);
            glm::vec3 blendedGlobalPos = glm::mix(gp0, gp1, alpha);
            m_globalPose[i] = glm::translate(glm::mat4(1.0f), blendedGlobalPos) * glm::mat4_cast(blendedGlobalRot);
        }
        m_blendTime += dt;
        if(m_blendTime >= m_blendDuration){
            m_currentClip = m_nextClip;
            m_nextClip = nullptr;
            m_blending = false;
        }
    } else {
        m_finalMatrices = std::move(currentFinal);
        m_globalPose = std::move(currentGlobals);
    }
}

void Animator::readNodeHierarchy(int boneIndex,
                                 const glm::mat4& parentTransform,
                                 const AnimationClip* clip,
                                 double time,
                                 std::vector<glm::mat4>& outGlobals,
                                 std::vector<glm::mat4>& outFinals){
    for(size_t idx=0; idx<m_mesh.bones.size(); ++idx){
        const BoneInfo& bone = m_mesh.bones[idx];
        if(bone.parentIndex == boneIndex){
            glm::mat4 localTransform(1.0f);
            for(const auto& pair : clip->channels){
                auto lookup = m_mesh.boneLookup.find(pair.first);
                if(lookup != m_mesh.boneLookup.end() && lookup->second == static_cast<int>(idx)){
                    const AnimationChannel& channel = pair.second;
                    glm::vec3 translation = ::interpolate(channel.positionKeys, time);
                    glm::quat rotation = ::interpolate(channel.rotationKeys, time);
                    glm::vec3 scale = ::interpolate(channel.scaleKeys, time);
                    localTransform = glm::translate(glm::mat4(1.0f), translation) *
                                     glm::mat4_cast(rotation) *
                                     glm::scale(glm::mat4(1.0f), scale);
                    break;
                }
            }
            glm::mat4 globalTransform = parentTransform * localTransform;
            outGlobals[idx] = globalTransform;
            outFinals[idx] = globalTransform * bone.offset;
            readNodeHierarchy(static_cast<int>(idx), globalTransform, clip, time, outGlobals, outFinals);
        }
    }
}

CharacterState CharacterController::update(double dt, bool forward, const glm::vec3& moveDirection){
    if(forward){
        // W pressed: Movement with animation
        glm::vec3 moveVec = moveDirection * moveSpeed * static_cast<float>(dt);
        position += moveVec;
        return CharacterState::Idle;
    }
    // No keys: No movement
    return CharacterState::Run;
}
