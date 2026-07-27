#include "math.h"

bool WorldToScreen(const Vector3& world, float screenPos[2], int screenW, int screenH, float viewMatrix[4][4]) {
    float clip[4] = {
        viewMatrix[0][0] * world.x + viewMatrix[0][1] * world.y + viewMatrix[0][2] * world.z + viewMatrix[0][3],
        viewMatrix[1][0] * world.x + viewMatrix[1][1] * world.y + viewMatrix[1][2] * world.z + viewMatrix[1][3],
        viewMatrix[2][0] * world.x + viewMatrix[2][1] * world.y + viewMatrix[2][2] * world.z + viewMatrix[2][3],
        viewMatrix[3][0] * world.x + viewMatrix[3][1] * world.y + viewMatrix[3][2] * world.z + viewMatrix[3][3]
    };
    if (clip[3] < 0.001f) return false;

    float ndcX = clip[0] / clip[3];
    float ndcY = clip[1] / clip[3];
    screenPos[0] = (ndcX + 1.0f) * 0.5f * screenW;
    screenPos[1] = (1.0f - ndcY) * 0.5f * screenH;
    return true;
}
