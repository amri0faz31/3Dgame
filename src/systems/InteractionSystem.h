#pragma once
#include <glm/glm.hpp>
#include <string>
#include <functional>

enum class InteractableType {
    Note,
    TorchPickup,
    FireSource
};

struct Interactable {
    InteractableType type;
    glm::vec3 position;
    float radius = 2.0f;
    std::string message;
    bool isActive = true;
    std::function<void()> onInteract;
};

class InteractionSystem {
public:
    // Add interactable object
    int addInteractable(const Interactable& obj);
    
    // Check if player is near any interactable
    int findNearestInteractable(const glm::vec3& playerPos, float maxDistance = 3.0f);
    
    // Trigger interaction
    void interact(int interactableID);
    
    // Remove interactable (after pickup)
    void removeInteractable(int interactableID);
    
    // Get interactable data
    const Interactable* getInteractable(int id) const;
    
    // Clear all interactables
    void clear();

private:
    std::vector<Interactable> m_interactables;
};
