#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct AppSettings {
    COLORREF down = RGB(0, 255, 100);  // Download speed color
    COLORREF up = RGB(255, 180, 0);    // Upload speed color
    wchar_t fontFamily[64] = L"Segoe UI";
    double fontSize = 8.0;             // Font size in points
    int fontStyle = 1;                 // 1 = bold, 0 = regular
    int showWidget = 1;                // 1 = show overlay widget, 0 = hide
};

void LoadSettings(AppSettings* s);
void SaveSettings(const AppSettings* s);
void GetSettingsPath(wchar_t* buf, size_t maxLen);

void LoadSettingsCustom(AppSettings* s, const wchar_t* filePath);
void SaveSettingsCustom(const AppSettings* s, const wchar_t* filePath);
