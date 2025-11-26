#include "TerrainSampler.h"

#include "Terrain.h"

namespace {
Terrain* g_activeTerrain = nullptr;
}

void setActiveTerrain(Terrain* terrain){
    g_activeTerrain = terrain;
}

float getTerrainHeightAt(float worldX, float worldZ){
    if(!g_activeTerrain) return 0.0f;
    return g_activeTerrain->getHeight(worldX, worldZ);
}
