#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

constexpr int TASKBAR_METER_OFFSET_MIN = -4096;
constexpr int TASKBAR_METER_OFFSET_MAX = 4096;
constexpr int SPEED_DECIMAL_PLACES_MIN = 0;
constexpr int SPEED_DECIMAL_PLACES_MAX = 2;

enum class MinimumSpeedUnit {
    Auto,
    Kilobytes,
    Megabytes,
    Gigabytes,
};

inline int ClampTaskbarMeterOffset(int value) {
    if (value < TASKBAR_METER_OFFSET_MIN) return TASKBAR_METER_OFFSET_MIN;
    if (value > TASKBAR_METER_OFFSET_MAX) return TASKBAR_METER_OFFSET_MAX;
    return value;
}

inline int ClampSpeedDecimalPlaces(int value) {
    if (value < SPEED_DECIMAL_PLACES_MIN) return SPEED_DECIMAL_PLACES_MIN;
    if (value > SPEED_DECIMAL_PLACES_MAX) return SPEED_DECIMAL_PLACES_MAX;
    return value;
}

bool ParseTaskbarMeterOffset(const wchar_t* text, int* value);

struct AppSettings {
    COLORREF down = RGB(255, 255, 255); // Download speed color
    COLORREF up = RGB(255, 255, 255);   // Upload speed color
    wchar_t fontFamily[64] = L"Segoe UI";
    double fontSize = 8.0;             // Font size in points
    int fontStyle = 1;                 // 1 = bold, 0 = regular
    int showWidget = 1;                // 1 = show overlay widget, 0 = hide
    int taskbarOffset = 0;             // Logical pixels from the automatic taskbar anchor
    MinimumSpeedUnit minimumSpeedUnit = MinimumSpeedUnit::Auto;
    int decimalPlaces = 2;
};

void LoadSettings(AppSettings* s);
void SaveSettings(const AppSettings* s);
void GetSettingsPath(wchar_t* buf, size_t maxLen);

void LoadSettingsCustom(AppSettings* s, const wchar_t* filePath);
void SaveSettingsCustom(const AppSettings* s, const wchar_t* filePath);
