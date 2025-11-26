#pragma once
#include <glm/glm.hpp>
#include <vector>

struct PatrolPoint {
    glm::vec3 position;
    float waitTime = 0.0f;  // Seconds to wait at this point
};

class MonsterAI {
public:
    void setPatrolPath(const std::vector<PatrolPoint>& path);
    void update(float dt);
    
    glm::vec3 getPosition() const { return m_position; }
    glm::vec3 getForward() const { return m_forward; }
    float getYaw() const { return m_yaw; }
    
    void setMoveSpeed(float speed) { m_moveSpeed = speed; }
    void setTurnSpeed(float speed) { m_turnSpeed = speed; }
    
    // Check if player is caught (within detection radius)
    bool detectsPlayer(const glm::vec3& playerPos, float detectionRadius = 10.0f) const;
    
    void reset();

private:
    std::vector<PatrolPoint> m_patrolPath;
    int m_currentWaypoint = 0;
    glm::vec3 m_position{0.0f};
    glm::vec3 m_forward{0.0f, 0.0f, -1.0f};
    float m_yaw = 180.0f;
    float m_moveSpeed = 3.0f;
    float m_turnSpeed = 90.0f;  // degrees per second
    float m_waitTimer = 0.0f;
    bool m_waiting = false;
};
