#pragma once
#include <vector>

struct Vector3 {
    float x, y, z;
};

// Globale Zust�nde f�r RadarWindow
extern std::vector<Vector3> g_enemyPositions;
extern Vector3 g_localPos;
extern float g_viewYaw;
