#include "memory.h"
#include <TlHelp32.h>
#include <Psapi.h>
#include <cstring>
#include <iostream>

HANDLE g_hProcess = NULL;
uintptr_t g_moduleBase = 0;

// ---- Find Overwatch process ----
bool FindOverwatchProcess() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (wcsstr(pe.szExeFile, L"Overwatch.exe")) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    if (!pid) return false;

    g_hProcess = OpenProcess(PROCESS_VIRTUAL_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!g_hProcess) return false;

    // Get module base
    HMODULE mods[1024];
    DWORD needed;
    if (EnumProcessModules(g_hProcess, mods, sizeof(mods), &needed)) {
        g_moduleBase = (uintptr_t)mods[0];
    }
    return true;
}

HANDLE GetProcessHandle() { return g_hProcess; }

// ---- Pattern scan helper ----
uintptr_t PatternScan(HANDLE hProc, uintptr_t start, size_t size, const BYTE* pattern, const char* mask) {
    BYTE* buffer = (BYTE*)VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
    if (!buffer) return 0;
    SIZE_T read;
    if (!ReadProcessMemory(hProc, (LPCVOID)start, buffer, size, &read)) {
        VirtualFree(buffer, 0, MEM_RELEASE);
        return 0;
    }

    size_t patternLen = strlen(mask);
    for (size_t i = 0; i < size - patternLen; i++) {
        bool found = true;
        for (size_t j = 0; j < patternLen; j++) {
            if (mask[j] == 'x' && buffer[i + j] != pattern[j]) {
                found = false;
                break;
            }
        }
        if (found) {
            uintptr_t addr = start + i;
            VirtualFree(buffer, 0, MEM_RELEASE);
            return addr;
        }
    }
    VirtualFree(buffer, 0, MEM_RELEASE);
    return 0;
}

// ---- Offset discovery (signatures) ----
bool FindAllOffsets(HANDLE hProc, uintptr_t& entityList, uintptr_t& viewMatrix, uintptr_t& localPlayer) {
    // Get module info
    MODULEINFO modInfo = { 0 };
    if (!GetModuleInformation(hProc, (HMODULE)g_moduleBase, &modInfo, sizeof(modInfo)))
        return false;

    uintptr_t base = (uintptr_t)modInfo.lpBaseOfDll;
    size_t size = modInfo.SizeOfImage;

    // Signature for entity list: "48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01"
    BYTE sigEntity[] = { 0x48, 0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC9, 0x74, 0x00, 0x48, 0x8B, 0x01 };
    const char* maskEntity = "xxx????xxxx?xxx";
    uintptr_t addr = PatternScan(hProc, base, size, sigEntity, maskEntity);
    if (!addr) return false;
    // offset is relative RIP: read 4 bytes after opcode
    int32_t relOffset = 0;
    ReadProcessMemory(hProc, (LPCVOID)(addr + 3), &relOffset, 4, NULL);
    entityList = base + relOffset + (addr + 7 - base);

    // Signature for view matrix: "48 8B 0D ? ? ? ? 48 8B 01" (similar pattern)
    BYTE sigView[] = { 0x48, 0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x01 };
    const char* maskView = "xxx????xxx";
    uintptr_t addrView = PatternScan(hProc, base, size, sigView, maskView);
    if (!addrView) return false;
    int32_t relView = 0;
    ReadProcessMemory(hProc, (LPCVOID)(addrView + 3), &relView, 4, NULL);
    viewMatrix = base + relView + (addrView + 7 - base);

    // Local player: often entity list[0] or separate pointer
    // We'll just read from entity list
    localPlayer = entityList; // first slot

    return (entityList && viewMatrix);
}

// ---- Read entities ----
void ReadEntityList(HANDLE hProc, uintptr_t entityList, uintptr_t localPlayer, std::vector<Player>& out) {
    out.clear();
    uintptr_t listPtr = 0;
    ReadProcessMemory(hProc, (LPCVOID)entityList, &listPtr, sizeof(uintptr_t), NULL);
    if (!listPtr) return;

    for (int i = 0; i < 64; i++) {
        uintptr_t entityAddr = 0;
        ReadProcessMemory(hProc, (LPCVOID)(listPtr + i * 8), &entityAddr, sizeof(uintptr_t), NULL);
        if (!entityAddr) continue;

        // Check if local player (skip)
        if (entityAddr == localPlayer) continue;

        Player p;
        p.address = entityAddr;

        // Read position (offsets: common 0x128, 0x12C, 0x130 for x,y,z)
        ReadProcessMemory(hProc, (LPCVOID)(entityAddr + 0x128), &p.footPos.x, sizeof(float), NULL);
        ReadProcessMemory(hProc, (LPCVOID)(entityAddr + 0x12C), &p.footPos.y, sizeof(float), NULL);
        ReadProcessMemory(hProc, (LPCVOID)(entityAddr + 0x130), &p.footPos.z, sizeof(float), NULL);

        // Head height offset (approx 1.7m)
        p.headHeight = 1.7f;

        // Health (offset 0xEC or 0xF0)
        ReadProcessMemory(hProc, (LPCVOID)(entityAddr + 0xEC), &p.health, sizeof(int), NULL);
        ReadProcessMemory(hProc, (LPCVOID)(entityAddr + 0xF0), &p.maxHealth, sizeof(int), NULL);

        // Team ID (offset 0x30 or 0x34)
        ReadProcessMemory(hProc, (LPCVOID)(entityAddr + 0x30), &p.team, sizeof(int), NULL);

        // Alive flag (offset 0xE8)
        BYTE alive = 0;
        ReadProcessMemory(hProc, (LPCVOID)(entityAddr + 0xE8), &alive, 1, NULL);
        p.alive = (alive == 1);

        if (p.health > 0 && p.alive) {
            out.push_back(p);
        }
    }
}

// ---- Read view matrix ----
float g_viewMatrix[4][4];

void ReadViewMatrix(HANDLE hProc, uintptr_t viewMatrix) {
    ReadProcessMemory(hProc, (LPCVOID)viewMatrix, g_viewMatrix, sizeof(g_viewMatrix), NULL);
}
