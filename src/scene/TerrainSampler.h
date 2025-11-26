#pragma once
// Helper to expose a global terrain height query for gameplay systems. Game
// sets the active terrain, then callers can invoke getTerrainHeightAt.

class Terrain;

void setActiveTerrain(Terrain* terrain);
float getTerrainHeightAt(float worldX, float worldZ);
