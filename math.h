#pragma once
#include <cmath>

struct Vector3 {
    float x, y, z;
    Vector3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    Vector3 operator-(const Vector3& o) const { return Vector3(x - o.x, y - o.y, z - o.z); }
    float Distance(const Vector3& o) const {
        float dx = x - o.x, dy = y - o.y, dz = z - o.z;
        return sqrtf(dx*dx + dy*dy + dz*dz);
    }
};

bool WorldToScreen(const Vector3& world, float screenPos[2], int screenW, int screenH, float viewMatrix[4][4]);
