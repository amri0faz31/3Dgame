#include "CollisionSystem.h"
#include <algorithm>
#include <cmath>

size_t CollisionSystem::addBody(const CollisionBody& body) {
    // Find empty slot or add new one
    for(size_t i = 0; i < m_activeSlots.size(); ++i) {
        if(!m_activeSlots[i]) {
            m_bodies[i] = body;
            m_activeSlots[i] = true;
            return i;
        }
    }
    m_bodies.push_back(body);
    m_activeSlots.push_back(true);
    return m_bodies.size() - 1;
}

void CollisionSystem::updateBodyPosition(size_t index, const glm::vec3& position) {
    if(index < m_bodies.size() && m_activeSlots[index]) {
        m_bodies[index].position = position;
    }
}

glm::vec3 CollisionSystem::resolveMovement(size_t bodyIndex, const glm::vec3& from, const glm::vec3& to) {
    if(bodyIndex >= m_bodies.size() || !m_activeSlots[bodyIndex]) {
        return to;
    }
    
    CollisionBody& movingBody = m_bodies[bodyIndex];
    glm::vec3 movement = to - from;
    float moveLength = glm::length(movement);
    
    if(moveLength < 1e-6f) {
        return to;
    }
    
    glm::vec3 moveDir = movement / moveLength;
    glm::vec3 resolvedPos = to;
    
    // Check collision with all other bodies
    for(size_t i = 0; i < m_bodies.size(); ++i) {
        if(i == bodyIndex || !m_activeSlots[i]) continue;
        
        const CollisionBody& other = m_bodies[i];
        
        // Simple sphere-based collision for now (works for capsules too)
        float combinedRadius = movingBody.radius + other.radius;
        glm::vec3 toOther = other.position - resolvedPos;
        float dist = glm::length(toOther);
        
        if(dist < combinedRadius) {
            // Collision detected - push out
            if(dist > 1e-6f) {
                glm::vec3 pushDir = toOther / dist;
                float penetration = combinedRadius - dist;
                resolvedPos -= pushDir * (penetration + 0.01f);
                
                // Slide along surface if moving
                if(!other.isStatic) {
                    resolvedPos = slideAlongSurface(movement, -pushDir);
                }
            }
        }
    }
    
    return resolvedPos;
}

std::vector<size_t> CollisionSystem::queryRadius(const glm::vec3& center, float radius) {
    std::vector<size_t> results;
    for(size_t i = 0; i < m_bodies.size(); ++i) {
        if(!m_activeSlots[i]) continue;
        
        float dist = glm::length(m_bodies[i].position - center);
        if(dist <= radius + m_bodies[i].radius) {
            results.push_back(i);
        }
    }
    return results;
}

void CollisionSystem::removeBody(size_t index) {
    if(index < m_activeSlots.size()) {
        m_activeSlots[index] = false;
    }
}

void CollisionSystem::clear() {
    m_bodies.clear();
    m_activeSlots.clear();
}

CollisionBody* CollisionSystem::getBody(size_t index) {
    if(index < m_bodies.size() && m_activeSlots[index]) {
        return &m_bodies[index];
    }
    return nullptr;
}

bool CollisionSystem::sphereVsSphere(const glm::vec3& pos1, float r1, const glm::vec3& pos2, float r2) {
    float dist = glm::length(pos1 - pos2);
    return dist < (r1 + r2);
}

bool CollisionSystem::capsuleVsCapsule(const CollisionBody& a, const CollisionBody& b) {
    // Simplified: treat as sphere at center for now
    return sphereVsSphere(a.position, a.radius, b.position, b.radius);
}

glm::vec3 CollisionSystem::slideAlongSurface(const glm::vec3& velocity, const glm::vec3& normal) {
    // Project velocity onto surface (remove component along normal)
    return velocity - normal * glm::dot(velocity, normal);
}
