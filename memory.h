#pragma once
#include <Windows.h>
#include <vector>
#include <cstdint>

struct Player {
    uintptr_t address;
    Vector3 footPos;      // world position at feet
    float headHeight;     // offset from foot to top of head
    int health, maxHealth;
    int team;
    bool alive;
};

extern HANDLE g_hProcess;
extern uintptr_t g_moduleBase;

bool FindOverwatchProcess();
HANDLE GetProcessHandle();
bool FindAllOffsets(HANDLE hProc, uintptr_t& entityList, uintptr_t& viewMatrix, uintptr_t& localPlayer);
void ReadEntityList(HANDLE hProc, uintptr_t entityList, uintptr_t localPlayer, std::vector<Player>& out);
void ReadViewMatrix(HANDLE hProc, uintptr_t viewMatrix);
