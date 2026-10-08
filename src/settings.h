#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

constexpr int TASKBAR_METER_OFFSET_MIN = -4096;
constexpr int TASKBAR_METER_OFFSET_MAX = 4096;
constexpr int SPEED_DECIMAL_PLACES_MIN = 0;
constexpr int SPEED_DECIMAL_PLACES_MAX = 2;
constexpr size_t METER_PREFIX_CAPACITY = 32;

// Settings files written by 0.2.0+ carry [General] SettingsVersion=2.
constexpr int SETTINGS_VERSION = 2;

// AppSettings::meterAnchor values (MeterAnchor in overlay.h).
constexpr int METER_ANCHOR_LEGACY = 0;   // 0.1.x fixed point; only for migrated files
constexpr int METER_ANCHOR_TRAY = 1;
constexpr int METER_ANCHOR_APPS = 2;
constexpr int METER_ANCHOR_LEFT = 3;

// A meter colour that follows the taskbar theme: white on a dark taskbar,
// near-black on a light one. ChooseColor never returns it (high byte is 0).
constexpr COLORREF METER_COLOR_AUTO = 0xFF000000;
COLORREF ResolveMeterColor(COLORREF color, bool lightTaskbar);
// True when Windows uses the light theme for the taskbar ("default Windows mode").
bool IsSystemThemeLight();

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
    COLORREF down = METER_COLOR_AUTO;   // Download speed color
    COLORREF up = METER_COLOR_AUTO;     // Upload speed color
    wchar_t fontFamily[64] = L"Segoe UI";
    wchar_t downPrefix[METER_PREFIX_CAPACITY] = L"\u2193";
    wchar_t upPrefix[METER_PREFIX_CAPACITY] = L"\u2191";
    double fontSize = 8.0;             // Font size in points
    int fontStyle = 1;                 // 1 = bold, 0 = regular
    int showWidget = 1;                // 1 = show overlay widget, 0 = hide
    int showTrayIcon = 1;              // 1 = show notification-area icon, 0 = hide
    int startWithWindows = 0;          // Mirrors the current user's Run registry entry
    int meterAnchor = METER_ANCHOR_TRAY;   // what taskbarOffset is measured from
    int taskbarOffset = 0;             // Logical pixels from the anchor
    MinimumSpeedUnit minimumSpeedUnit = MinimumSpeedUnit::Auto;
    int decimalPlaces = 2;
    int speedBits = 0;                 // 0 = bytes (KB/s, binary), 1 = bits (Mbps, decimal)
    ULONGLONG lifetimeDownloaded = 0;
    ULONGLONG lifetimeUploaded = 0;
    wchar_t lifetimeSince[11] = L""; // YYYY-MM-DD
    int adapterAuto = 1;               // 1 = meter whichever adapter carries the default route
    ULONGLONG adapterLuid = 0;         // manual choice (NET_LUID value) when adapterAuto == 0
    wchar_t adapterAlias[128] = L"";   // manual choice's name, to recover it if its LUID changes
};

inline bool HasUiEntryPoint(const AppSettings& s) {
    return s.showWidget || s.showTrayIcon;
}

void LoadSettings(AppSettings* s);
// Returns false if the settings could not be persisted (read-only file, full
// disk, locked profile). The on-disk file is left untouched on failure.
bool SaveSettings(const AppSettings* s);
void GetSettingsPath(wchar_t* buf, size_t maxLen);
// Integration-test isolation: redirect the settings file and the Run-key value
// name so tests never touch a real installation. nullptr restores the defaults.
void SetSettingsPathOverride(const wchar_t* path);
void SetStartupValueName(const wchar_t* name);
bool IsStartWithWindowsEnabled();
bool SetStartWithWindowsEnabled(bool enabled);
void AddLifetimeTraffic(AppSettings* s, ULONGLONG downloaded, ULONGLONG uploaded);
void ResetLifetimeTotals(AppSettings* s);
// Formats a YYYY-MM-DD date as the locale's short date; nullptr = the user's locale.
bool FormatLifetimeSinceDate(const wchar_t* isoDate, wchar_t* out, size_t maxLen,
                             const wchar_t* locale = nullptr);

void LoadSettingsCustom(AppSettings* s, const wchar_t* filePath);
bool SaveSettingsCustom(const AppSettings* s, const wchar_t* filePath);
