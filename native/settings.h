#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

struct AppSettings {
    COLORREF bg = RGB(20, 20, 20);     // opaque
    COLORREF down = RGB(0, 255, 100);
    COLORREF up = RGB(255, 180, 0);
    wchar_t fontFamily[64] = L"Segoe UI";
    double fontSize = 8.0;             // points
    int fontStyle = 1;                 // 1 = bold, 0 = regular
};

void LoadSettings(AppSettings* s);
void SaveSettings(const AppSettings* s);
