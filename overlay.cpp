#include "overlay.h"
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

static HWND g_overlayHwnd = NULL;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Graphics graphics(hdc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);

        int w = GetSystemMetrics(SM_CXSCREEN);
        int h = GetSystemMetrics(SM_CYSCREEN);

        // Clear to fully transparent
        graphics.Clear(Color(0, 0, 0, 0));

        if (g_espEnabled) {
            Pen boxPen(Color(255, 0, 255, 0), 2);   // green outline
            Pen enemyPen(Color(255, 255, 0, 0), 2); // red for enemies
            Pen teamPen(Color(255, 0, 255, 255), 2);// cyan for teammates
            SolidBrush healthBrush(Color(255, 0, 255, 0));

            for (const auto& p : g_players) {
                Vector3 head = p.footPos;
                head.z += p.headHeight;

                float footScreen[2], headScreen[2];
                if (!WorldToScreen(p.footPos, footScreen, w, h, g_viewMatrix)) continue;
                if (!WorldToScreen(head, headScreen, w, h, g_viewMatrix)) continue;

                float height = headScreen[1] - footScreen[1];
                if (height < 5) continue;
                float width = height * 0.4f;
                float x = footScreen[0] - width / 2;
                float y = headScreen[1];

                // Choose color
                Pen* pen = &boxPen;
                if (p.team != 1) pen = &enemyPen; // assume team 1 = friendly

                // Draw box
                graphics.DrawRectangle(pen, x, y, width, height);

                // Health bar
                float healthPct = (float)p.health / p.maxHealth;
                if (healthPct > 0) {
                    SolidBrush healthBrush(Color(255, (BYTE)(255 * (1 - healthPct)), (BYTE)(255 * healthPct), 0));
                    graphics.FillRectangle(&healthBrush, x, y - 8, width * healthPct, 5);
                }

                // Distance
                float dist = p.footPos.Distance(Vector3(0,0,0)); // local player at origin
                WCHAR text[64];
                swprintf_s(text, L"%dm", (int)dist);
                Font font(L"Arial", 10);
                SolidBrush textBrush(Color(255, 255, 255, 255));
                graphics.DrawString(text, -1, &font, PointF(x, y - 20), &textBrush);
            }

            // Crosshair snap lines to nearest enemy
            // (omitted for brevity – add if desired)
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool CreateOverlayWindow(HINSTANCE hInstance) {
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ESPOverlay";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!RegisterClass(&wc)) return false;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    g_overlayHwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        L"ESPOverlay", L"", WS_POPUP,
        0, 0, screenW, screenH,
        NULL, NULL, hInstance, NULL
    );
    if (!g_overlayHwnd) return false;

    // Make transparent and click-through
    SetLayeredWindowAttributes(g_overlayHwnd, RGB(0,0,0), 0, LWA_COLORKEY);
    ShowWindow(g_overlayHwnd, SW_SHOW);
    UpdateWindow(g_overlayHwnd);
    return true;
}

HWND GetOverlayHwnd() { return g_overlayHwnd; }
