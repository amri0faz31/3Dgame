#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <functional>

// Simple collision shapes for character and future 3D objects
enum class CollisionShape {
    Sphere,
    Capsule,
    Box
};

struct CollisionBody {
    CollisionShape shape = CollisionShape::Capsule;
    glm::vec3 position{0.0f};
    float radius = 0.5f;      // For sphere/capsule
    float height = 1.0f;      // For capsule
    glm::vec3 halfExtents{0.5f}; // For box
    bool isStatic = false;
    void* userData = nullptr;  // Back-reference to game object
};

// Simple collision detection and response
class CollisionSystem {
public:
    // Register a collision body (returns index)
    size_t addBody(const CollisionBody& body);
    
    // Update body position
    void updateBodyPosition(size_t index, const glm::vec3& position);
    
    // Check if moving from 'from' to 'to' would collide
    // Returns adjusted position (slides along obstacles)
    glm::vec3 resolveMovement(size_t bodyIndex, const glm::vec3& from, const glm::vec3& to);
    
    // Query all bodies within radius of point
    std::vector<size_t> queryRadius(const glm::vec3& center, float radius);
    
    // Remove a body
    void removeBody(size_t index);
    
    // Clear all bodies
    void clear();
    
    // Get body by index
    CollisionBody* getBody(size_t index);

private:
    std::vector<CollisionBody> m_bodies;
    std::vector<bool> m_activeSlots;
    
    // Collision detection helpers
    bool sphereVsSphere(const glm::vec3& pos1, float r1, const glm::vec3& pos2, float r2);
    bool capsuleVsCapsule(const CollisionBody& a, const CollisionBody& b);
    glm::vec3 slideAlongSurface(const glm::vec3& velocity, const glm::vec3& normal);
};
