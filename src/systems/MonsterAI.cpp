#include "MonsterAI.h"
#include <glm/gtx/norm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

void MonsterAI::setPatrolPath(const std::vector<PatrolPoint>& path){
    m_patrolPath = path;
    m_currentWaypoint = 0;
    if(!m_patrolPath.empty()){
        m_position = m_patrolPath[0].position;
    }
}

void MonsterAI::update(float dt){
    if(m_patrolPath.empty()) return;
    
    const auto& target = m_patrolPath[m_currentWaypoint];
    
    // Waiting at waypoint
    if(m_waiting){
        m_waitTimer += dt;
        if(m_waitTimer >= target.waitTime){
            m_waiting = false;
            m_waitTimer = 0.0f;
            m_currentWaypoint = (m_currentWaypoint + 1) % m_patrolPath.size();
        }
        return;
    }
    
    // Move toward target
    glm::vec3 toTarget = target.position - m_position;
    float distSq = glm::length2(toTarget);
    
    if(distSq < 0.5f){
        // Reached waypoint
        m_waiting = true;
        return;
    }
    
    toTarget = glm::normalize(toTarget);
    
    // Calculate target yaw
    float targetYaw = glm::degrees(std::atan2(toTarget.x, toTarget.z));
    
    // Smooth rotation
    float angleDiff = targetYaw - m_yaw;
    while(angleDiff > 180.0f) angleDiff -= 360.0f;
    while(angleDiff < -180.0f) angleDiff += 360.0f;
    
    float maxTurn = m_turnSpeed * dt;
    if(std::abs(angleDiff) < maxTurn){
        m_yaw = targetYaw;
    } else {
        m_yaw += (angleDiff > 0.0f ? maxTurn : -maxTurn);
    }
    
    // Update forward vector
    float yawRad = glm::radians(m_yaw);
    m_forward = glm::vec3(std::sin(yawRad), 0.0f, std::cos(yawRad));
    
    // Move forward
    m_position += m_forward * m_moveSpeed * dt;
}

bool MonsterAI::detectsPlayer(const glm::vec3& playerPos, float detectionRadius) const{
    float distSq = glm::distance2(m_position, playerPos);
    return distSq < (detectionRadius * detectionRadius);
}

void MonsterAI::reset(){
    m_currentWaypoint = 0;
    m_waitTimer = 0.0f;
    m_waiting = false;
    if(!m_patrolPath.empty()){
        m_position = m_patrolPath[0].position;
    }
}
