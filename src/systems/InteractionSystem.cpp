#include "InteractionSystem.h"
#include <glm/gtx/norm.hpp>
#include <algorithm>

int InteractionSystem::addInteractable(const Interactable& obj){
    m_interactables.push_back(obj);
    return static_cast<int>(m_interactables.size() - 1);
}

int InteractionSystem::findNearestInteractable(const glm::vec3& playerPos, float maxDistance){
    int nearestID = -1;
    float nearestDist = maxDistance * maxDistance;
    
    for(size_t i = 0; i < m_interactables.size(); ++i){
        if(!m_interactables[i].isActive) continue;
        
        float distSq = glm::distance2(playerPos, m_interactables[i].position);
        if(distSq < nearestDist && distSq < (m_interactables[i].radius * m_interactables[i].radius)){
            nearestDist = distSq;
            nearestID = static_cast<int>(i);
        }
    }
    
    return nearestID;
}

void InteractionSystem::interact(int interactableID){
    if(interactableID < 0 || interactableID >= static_cast<int>(m_interactables.size()))
        return;
    
    auto& obj = m_interactables[interactableID];
    if(!obj.isActive) return;
    
    if(obj.onInteract){
        obj.onInteract();
    }
}

void InteractionSystem::removeInteractable(int interactableID){
    if(interactableID < 0 || interactableID >= static_cast<int>(m_interactables.size()))
        return;
    
    m_interactables[interactableID].isActive = false;
}

const Interactable* InteractionSystem::getInteractable(int id) const{
    if(id < 0 || id >= static_cast<int>(m_interactables.size()))
        return nullptr;
    return &m_interactables[id];
}

void InteractionSystem::clear(){
    m_interactables.clear();
}
