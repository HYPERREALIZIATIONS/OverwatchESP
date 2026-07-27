#pragma once
#include <Windows.h>
#include <vector>
#include "math.h"
#include "memory.h"

extern std::vector<Player> g_players;
extern bool g_espEnabled;
extern float g_viewMatrix[4][4];

bool CreateOverlayWindow(HINSTANCE hInstance);
HWND GetOverlayHwnd();
