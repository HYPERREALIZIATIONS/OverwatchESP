#include <Windows.h>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include "memory.h"
#include "overlay.h"
#include "math.h"

// Global state
bool g_running = true;
bool g_espEnabled = true;
std::vector<Player> g_players;
CRITICAL_SECTION g_cs;

// ---- Hotkey ----
void CheckHotkey() {
    if (GetAsyncKeyState(VK_F1) & 0x8000) {
        g_espEnabled = !g_espEnabled;
        Sleep(200); // debounce
    }
}

// ---- Worker thread: read memory, update players ----
DWORD WINAPI WorkerThread(LPVOID) {
    HANDLE hProcess = GetProcessHandle();
    if (!hProcess) return 1;

    uintptr_t entityList = 0, viewMatrix = 0, localPlayer = 0;
    if (!FindAllOffsets(hProcess, entityList, viewMatrix, localPlayer)) {
        MessageBox(NULL, L"Pattern scan failed. Game update?", L"ESP", MB_ICONERROR);
        return 1;
    }

    while (g_running) {
        CheckHotkey();

        if (g_espEnabled && hProcess) {
            std::vector<Player> newPlayers;
            ReadEntityList(hProcess, entityList, localPlayer, newPlayers);
            ReadViewMatrix(hProcess, viewMatrix);

            EnterCriticalSection(&g_cs);
            g_players = newPlayers;
            LeaveCriticalSection(&g_cs);

            InvalidateRect(GetOverlayHwnd(), NULL, TRUE);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    return 0;
}

// ---- WinMain ----
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    InitializeCriticalSection(&g_cs);

    if (!FindOverwatchProcess()) {
        MessageBox(NULL, L"Overwatch.exe not found.", L"ESP", MB_ICONERROR);
        return 1;
    }

    if (!CreateOverlayWindow(hInstance)) {
        MessageBox(NULL, L"Failed to create overlay.", L"ESP", MB_ICONERROR);
        return 1;
    }

    HANDLE hThread = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_running = false;
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    DeleteCriticalSection(&g_cs);
    return 0;
}
