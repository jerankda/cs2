// RadarWindow.cpp
#include <windows.h>
#include <vector>
#include <cmath>
#include "RadarTypes.h"

const int RADAR_SIZE = 400;
const int RADAR_RANGE = 1500; // Ingame units

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        HBRUSH background = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &ps.rcPaint, background);
        DeleteObject(background);

        int centerX = RADAR_SIZE / 2;
        int centerY = RADAR_SIZE / 2;

        // Draw radar circle
        HBRUSH greenBrush = CreateSolidBrush(RGB(0, 0, 0));
        HPEN greenPen = CreatePen(PS_SOLID, 1, RGB(0, 255, 0));
        SelectObject(hdc, greenBrush);
        SelectObject(hdc, greenPen);
        Ellipse(hdc, centerX - 150, centerY - 150, centerX + 150, centerY + 150);
        DeleteObject(greenBrush);
        DeleteObject(greenPen);

        // Draw field of view cone (fixed up direction)
        float yawRad = -(g_viewYaw - 90.0f) * (3.14159265f / 180.0f);
        float coneLength = 150.0f;
        float coneAngle = 45.0f * (3.14159265f / 180.0f); // ±45°

        float leftX = cosf(coneAngle) * coneLength;
        float leftY = sinf(coneAngle) * coneLength;
        float rightX = cosf(-coneAngle) * coneLength;
        float rightY = sinf(-coneAngle) * coneLength;

        HPEN fovPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
        SelectObject(hdc, fovPen);

        // Player always looking "up" → cone drawn upward
        MoveToEx(hdc, centerX, centerY, NULL);
        LineTo(hdc, centerX + static_cast<int>(leftX), centerY - static_cast<int>(leftY));
        MoveToEx(hdc, centerX, centerY, NULL);
        LineTo(hdc, centerX + static_cast<int>(rightX), centerY - static_cast<int>(rightY));

        DeleteObject(fovPen);

        // Draw enemies (rotated around player)
        for (const auto& enemy : g_enemyPositions) {
            float dx = enemy.x - g_localPos.x;
            float dy = enemy.y - g_localPos.y;

            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > RADAR_RANGE) continue;

            // Rotate world around player (so player is always facing up)
            float rotatedX = dx * cosf(yawRad) - dy * sinf(yawRad);
            float rotatedY = dx * sinf(yawRad) + dy * cosf(yawRad);

            float radarX = rotatedX / RADAR_RANGE * 150;
            float radarY = rotatedY / RADAR_RANGE * 150;

            int px = static_cast<int>(centerX + radarX);
            int py = static_cast<int>(centerY - radarY);

            HBRUSH redBrush = CreateSolidBrush(RGB(255, 0, 0));
            SelectObject(hdc, redBrush);
            Ellipse(hdc, px - 3, py - 3, px + 3, py + 3);
            DeleteObject(redBrush);
        }

        EndPaint(hwnd, &ps);
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void RunRadarWindow() {
    const wchar_t CLASS_NAME[] = L"RadarWindow";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"CS2 Radar",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, RADAR_SIZE, RADAR_SIZE,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    if (hwnd == NULL) return;

    ShowWindow(hwnd, SW_SHOW);

    MSG msg = {};
    while (true) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        InvalidateRect(hwnd, NULL, TRUE);
        Sleep(50);
    }
}
