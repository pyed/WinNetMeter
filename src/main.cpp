// WinNetMeter - Native Windows x64 network throughput monitor
// Single small dependency-free executable
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <cwchar>
#include <string>
#include "network.h"
#include "meter.h"
#include "render.h"
#include "settings.h"
#include "version.h"

// Window message constants & IDs
enum {
    ID_TIMER = 1,
    ID_SETTINGS_SAVE_TIMER = 2,
    WM_TRAYICON = WM_APP + 1,
    WM_OPEN_FROM_METER = WM_APP + 2,   // posted by the meter thread on double-click

    // Main window control IDs
    ID_COMBO_IF = 101,
    ID_SPEED_DOWN = 102,
    ID_SPEED_UP = 103,
    ID_TOTAL_DOWN = 104,
    ID_TOTAL_UP = 105,
    ID_AUTHOR_LINK = 106,
    ID_STATUS_GROUP = 107,
    ID_SETTINGS_GROUP = 108,
    ID_LIFETIME_TITLE = 109,
    ID_LIFETIME_DOWN = 110,
    ID_LIFETIME_DOWN_VALUE = 111,
    ID_LIFETIME_UP = 112,
    ID_LIFETIME_UP_VALUE = 113,
    ID_LIFETIME_RESET = 114,
    ID_IFACE_LABEL = 201,
    ID_DOWN_TITLE = 202,
    ID_UP_TITLE = 203,
    ID_TOTD_TITLE = 204,
    ID_TOTU_TITLE = 205,

    // Tray menu command IDs
    ID_TRAY_SHOW = 1001,
    ID_TRAY_TOGGLE_WIDGET = 1003,
    ID_TRAY_EXIT = 1004,

    // Settings dialog control IDs
    ID_SET_DOWN_BTN = 2002,
    ID_SET_UP_BTN = 2003,
    ID_SET_FONT_BTN = 2004,
    ID_SET_SAVE_BTN = 2005,
    ID_SET_DOWN_LBL = 2009,
    ID_SET_UP_LBL = 2010,
    ID_SET_FONT_LBL = 2011,
    ID_SET_OFFSET_LBL = 2012,
    ID_SET_OFFSET_EDIT = 2013,
    ID_SET_OFFSET_SPIN = 2014,
    ID_SET_OFFSET_UNIT = 2015,
    ID_SET_OFFSET_RESET = 2016,
    ID_SET_UNIT_LBL = 2017,
    ID_SET_UNIT_COMBO = 2018,
    ID_SET_DECIMALS_LBL = 2019,
    ID_SET_DECIMALS_COMBO = 2020,
    ID_SET_WIDGET_CHECK = 2021,
    ID_SET_TRAY_CHECK = 2022,
    ID_SET_STARTUP_CHECK = 2023,
    ID_EXIT_APP = 2024,
    ID_SET_DOWN_PREFIX_LBL = 2025,
    ID_SET_DOWN_PREFIX_EDIT = 2026,
    ID_SET_UP_PREFIX_LBL = 2027,
    ID_SET_UP_PREFIX_EDIT = 2028,
    ID_SET_UNITS_LBL = 2029,
    ID_SET_UNITS_COMBO = 2030,
    ID_SET_UP_AUTO = 2031,
    ID_SET_DOWN_AUTO = 2032,
    ID_SET_ANCHOR_LBL = 2033,
    ID_SET_ANCHOR_COMBO = 2034,
    ID_SET_EMBED_CHECK = 2035,
};

// Order of the "Meter position" choices, mapped to AppSettings::meterAnchor.
static const int ANCHOR_CHOICES[] = { METER_ANCHOR_TRAY, METER_ANCHOR_APPS, METER_ANCHOR_LEFT, METER_ANCHOR_LEGACY };

// Long enough to swallow a burst of keystrokes, short enough that a settings
// change is on disk well before a normal exit.
static constexpr UINT SETTINGS_SAVE_DEBOUNCE_MS = 750;

static const wchar_t MAIN_WINDOW_CLASS[] = L"WinNetMeterMain";
static const wchar_t TEST_WINDOW_CLASS[] = L"WinNetMeterMainTest";
static const wchar_t SINGLE_INSTANCE_MUTEX[] = L"Local\\WinNetMeter.SingleInstance";
static const wchar_t TEST_INSTANCE_MUTEX[] = L"Local\\WinNetMeter.IntegrationTest.SingleInstance";

// Global application state
static HINSTANCE g_hInst = nullptr;
static const wchar_t* g_mainWindowClass = MAIN_WINDOW_CLASS;
static HWND g_hwndMain = nullptr;

// Main window child controls
static HWND g_hwndIfaceLbl = nullptr;
static HWND g_combo = nullptr;
static HWND g_hwndDownTitle = nullptr;
static HWND g_hwndSpeedDown = nullptr;
static HWND g_hwndUpTitle = nullptr;
static HWND g_hwndSpeedUp = nullptr;
static HWND g_hwndTotdTitle = nullptr;
static HWND g_hwndTotalDown = nullptr;
static HWND g_hwndTotuTitle = nullptr;
static HWND g_hwndTotalUp = nullptr;
static HWND g_hwndLifetimeTitle = nullptr;
static HWND g_hwndLifetimeDown = nullptr;
static HWND g_hwndLifetimeDownValue = nullptr;
static HWND g_hwndLifetimeUp = nullptr;
static HWND g_hwndLifetimeUpValue = nullptr;
static HWND g_hwndLifetimeReset = nullptr;
static HWND g_hwndAuthor = nullptr;
static HWND g_hwndStatusGroup = nullptr;

struct SettingsUiState {
    AppSettings tempSettings;
    HWND hwndGroup = nullptr;
    HWND hwndLblDownPrefix = nullptr, hwndEditDownPrefix = nullptr;
    HWND hwndLblUpPrefix = nullptr, hwndEditUpPrefix = nullptr;
    HWND hwndLblDown = nullptr, hwndBtnDown = nullptr, hwndCheckDownAuto = nullptr;
    HWND hwndLblUp = nullptr, hwndBtnUp = nullptr, hwndCheckUpAuto = nullptr;
    HWND hwndLblFont = nullptr, hwndBtnFont = nullptr;
    HWND hwndLblAnchor = nullptr, hwndComboAnchor = nullptr;
    HWND hwndLblOffset = nullptr, hwndEditOffset = nullptr, hwndSpinOffset = nullptr;
    HWND hwndLblOffsetUnit = nullptr, hwndBtnOffsetReset = nullptr;
    HWND hwndLblUnits = nullptr, hwndComboUnits = nullptr;
    HWND hwndLblUnit = nullptr, hwndComboUnit = nullptr;
    HWND hwndLblDecimals = nullptr, hwndComboDecimals = nullptr;
    HWND hwndCheckWidget = nullptr, hwndCheckTray = nullptr, hwndCheckStartup = nullptr;
    HWND hwndCheckEmbed = nullptr;
    HWND hwndBtnApply = nullptr, hwndBtnExit = nullptr;
    bool refreshing = false;
};

static SettingsUiState g_settingsUi;

// Fonts & Brushes
static HFONT g_fontLabel = nullptr;
static HFONT g_fontValue = nullptr;
static HFONT g_fontAuthor = nullptr;

static HICON g_hCurrentTrayIcon = nullptr;

static UINT g_uTaskbarCreatedMsg = 0;
static NetSampler g_sampler;
static AppSettings g_settings;
static NET_LUID g_selectedLuid = {};     // adapter being metered right now (0 = none)
static NET_LUID g_comboLuids[64] = {};   // adapter behind combo item i + 1 (item 0 is Automatic)
static int g_comboLuidCount = 0;
static int g_currentDpi = 96;
static bool g_taskbarLight = false;   // resolves METER_COLOR_AUTO; refreshed on theme change

// Overlay speed strings
static wchar_t g_szDownSpeed[64] = L"";
static wchar_t g_szUpSpeed[64] = L"";
static wchar_t g_szDownCompact[32] = L"0B";
static wchar_t g_szUpCompact[32] = L"0B";
static ULONGLONG g_currentDownBps = 0;
static ULONGLONG g_currentUpBps = 0;
static ULONGLONG g_lastTotalsSaveTick = 0;
static bool g_totalsDirty = false;
static bool g_settingsSaveFailed = false;
static bool g_settingsSavePending = false;

// Forward declarations
static void ShowMainWindow();
static void UpdateMeter();
static void UpdateTrayIcon();
static void SetupTrayIcon();
static void RemoveTrayIcon();
static void RefreshFontsAndRelayout(int dpi);
static void PopulateAdapters();
static void RefreshSettingsControls();
static void UpdateTotalValues();

static int ScaleDpi(int val, int dpi) {
    return MulDiv(val, dpi, 96);
}

// Writes the settings file now, cancelling any debounced save, and remembers a
// failure so it can be reported the next time the window is opened.
static bool PersistSettingsNow() {
    if (g_hwndMain) KillTimer(g_hwndMain, ID_SETTINGS_SAVE_TIMER);
    g_settingsSavePending = false;
    if (SaveSettings(&g_settings)) {
        g_settingsSaveFailed = false;
        return true;
    }
    g_settingsSaveFailed = true;
    return false;
}

// Live meter edits arrive one per keystroke (and faster from spinner
// auto-repeat). Coalesce them so typing costs one file write, not one per
// character, and keeps the UI thread off the disk.
static void SchedulePersistSettings() {
    if (!g_hwndMain) {
        PersistSettingsNow();
        return;
    }
    g_settingsSavePending = true;
    SetTimer(g_hwndMain, ID_SETTINGS_SAVE_TIMER, SETTINGS_SAVE_DEBOUNCE_MS, nullptr);
}

static void ReportSettingsSaveFailure(HWND hwnd) {
    // The message box runs a modal loop that can re-enter here (a meter
    // double-click, a failing periodic save). Clear the flag before showing it
    // and allow only one box, so a later failure is reported once, afterwards.
    static bool reporting = false;
    g_settingsSaveFailed = false;
    if (reporting) return;
    reporting = true;

    wchar_t path[MAX_PATH] = {};
    GetSettingsPath(path, _countof(path));
    wchar_t message[MAX_PATH + 192] = {};
    _snwprintf_s(message, _countof(message), _TRUNCATE,
                 L"Settings could not be saved to:\n\n%s\n\n"
                 L"Check that the file is not read-only and that the drive has free space. "
                 L"Your changes stay active for this session only.",
                 path);
    MessageBoxW(hwnd, message, L"WinNetMeter", MB_OK | MB_ICONERROR);
    reporting = false;
}

static HFONT MakeFont(const wchar_t* family, double pt, int style, int dpi,
                      DWORD quality = CLEARTYPE_QUALITY) {
    int height = -MulDiv(static_cast<int>(pt * 96.0 / 72.0 + 0.5), dpi, 96);
    bool bold = (style & 1) != 0;
    bool italic = (style & 2) != 0;
    bool underline = (style & 4) != 0;
    bool strikeout = (style & 8) != 0;
    return CreateFontW(height, 0, 0, 0,
                       bold ? FW_BOLD : FW_REGULAR,
                       italic ? TRUE : FALSE,
                       underline ? TRUE : FALSE,
                       strikeout ? TRUE : FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, quality,
                       DEFAULT_PITCH | FF_DONTCARE, family);
}

// Sends the meter thread everything it draws; it renders asynchronously.
static void UpdateMeter() {
    MeterState state;
    state.show = g_settings.showWidget != 0;
    wchar_t text[128] = {};
    FormatPrefixedSpeed(g_settings.upPrefix, g_szUpSpeed, text, _countof(text));
    state.upText = text;
    FormatPrefixedSpeed(g_settings.downPrefix, g_szDownSpeed, text, _countof(text));
    state.downText = text;
    state.upColor = ResolveMeterColor(g_settings.up, g_taskbarLight);
    state.downColor = ResolveMeterColor(g_settings.down, g_taskbarLight);
    state.fontFamily = g_settings.fontFamily;
    state.fontSize = g_settings.fontSize;
    state.fontStyle = g_settings.fontStyle;
    state.anchor = g_settings.meterAnchor;
    state.taskbarOffset = g_settings.taskbarOffset;
    state.embedded = g_settings.embedInTaskbar != 0;
    PushMeterState(state);
}

static void RelayoutMainControls(int dpi) {
    struct ItemPos {
        HWND hwnd;
        int x, y, w, h;
    } items[] = {
        { g_hwndStatusGroup,               12,  12, 325, 480 },
        { g_hwndIfaceLbl,                   28,  42, 110,  20 },
        { g_combo,                         140,  39, 180, 250 },
        { g_hwndDownTitle,                  28,  90, 140,  25 },
        { g_hwndSpeedDown,                 175,  90, 145,  25 },
        { g_hwndUpTitle,                    28, 125, 140,  25 },
        { g_hwndSpeedUp,                   175, 125, 145,  25 },
        { g_hwndTotdTitle,                  28, 175, 145,  25 },
        { g_hwndTotalDown,                 180, 175, 140,  25 },
        { g_hwndTotuTitle,                  28, 205, 145,  25 },
        { g_hwndTotalUp,                   180, 205, 140,  25 },
        { g_hwndLifetimeTitle,              28, 250, 292,  25 },
        { g_hwndLifetimeDown,               28, 280, 145,  25 },
        { g_hwndLifetimeDownValue,         180, 280, 140,  25 },
        { g_hwndLifetimeUp,                 28, 310, 145,  25 },
        { g_hwndLifetimeUpValue,           180, 310, 140,  25 },
        { g_hwndLifetimeReset,             235, 342,  85,  26 },
        { g_hwndAuthor,                     28, 505, 300,  38 },
        { g_settingsUi.hwndGroup,          350,  12, 365, 480 },
        { g_settingsUi.hwndLblUpPrefix,    370,  42, 125,  25 },
        { g_settingsUi.hwndEditUpPrefix,   500,  38, 125,  25 },
        { g_settingsUi.hwndLblDownPrefix,  370,  77, 125,  25 },
        { g_settingsUi.hwndEditDownPrefix, 500,  73, 125,  25 },
        { g_settingsUi.hwndLblUp,          370, 112, 125,  25 },
        { g_settingsUi.hwndBtnUp,          500, 108,  80,  25 },
        { g_settingsUi.hwndCheckUpAuto,    592, 110, 110,  22 },
        { g_settingsUi.hwndLblDown,        370, 147, 125,  25 },
        { g_settingsUi.hwndBtnDown,        500, 143,  80,  25 },
        { g_settingsUi.hwndCheckDownAuto,  592, 145, 110,  22 },
        { g_settingsUi.hwndLblFont,        370, 182, 125,  25 },
        { g_settingsUi.hwndBtnFont,        500, 178,  80,  25 },
        { g_settingsUi.hwndLblUnits,       370, 222, 125,  25 },
        { g_settingsUi.hwndComboUnits,     500, 218, 200, 120 },
        { g_settingsUi.hwndLblUnit,        370, 257, 125,  25 },
        { g_settingsUi.hwndComboUnit,      500, 253, 110, 120 },
        { g_settingsUi.hwndLblDecimals,    370, 292, 125,  25 },
        { g_settingsUi.hwndComboDecimals,  500, 288,  80, 120 },
        { g_settingsUi.hwndLblAnchor,      370, 327, 125,  25 },
        { g_settingsUi.hwndComboAnchor,    500, 323, 200, 140 },
        { g_settingsUi.hwndLblOffset,      370, 362, 125,  25 },
        { g_settingsUi.hwndEditOffset,     500, 358,  58,  25 },
        { g_settingsUi.hwndSpinOffset,     558, 358,  18,  25 },
        { g_settingsUi.hwndLblOffsetUnit,  580, 362,  25,  25 },
        { g_settingsUi.hwndBtnOffsetReset, 610, 358,  90,  25 },
        { g_settingsUi.hwndCheckWidget,    370, 397, 155,  22 },
        { g_settingsUi.hwndCheckTray,      535, 397, 170,  22 },
        { g_settingsUi.hwndCheckStartup,   370, 427, 155,  22 },
        // y=427 right: all-taskbars checkbox
        { g_settingsUi.hwndCheckEmbed,     370, 457, 330,  22 },
        { g_settingsUi.hwndBtnApply,       540, 505,  75,  28 },
        { g_settingsUi.hwndBtnExit,        625, 505,  75,  28 },
    };

    for (const auto& item : items) {
        if (item.hwnd) {
            SetWindowPos(item.hwnd, nullptr,
                         ScaleDpi(item.x, dpi), ScaleDpi(item.y, dpi),
                         ScaleDpi(item.w, dpi), ScaleDpi(item.h, dpi),
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
}

static void RefreshFontsAndRelayout(int dpi) {
    HFONT oldFontLabel = g_fontLabel;
    HFONT oldFontValue = g_fontValue;
    HFONT oldFontAuthor = g_fontAuthor;

    g_currentDpi = dpi;
    g_fontLabel = MakeFont(L"Segoe UI", 9.0, 0, dpi);
    g_fontValue = MakeFont(L"Segoe UI", 10.0, 1, dpi);
    g_fontAuthor = MakeFont(L"Segoe UI", 8.0, 2, dpi);

    if (g_hwndIfaceLbl)   SendMessageW(g_hwndIfaceLbl, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
    if (g_combo)          SendMessageW(g_combo, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
    if (g_hwndDownTitle)  SendMessageW(g_hwndDownTitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontValue), TRUE);
    if (g_hwndSpeedDown)  SendMessageW(g_hwndSpeedDown, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontValue), TRUE);
    if (g_hwndUpTitle)    SendMessageW(g_hwndUpTitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontValue), TRUE);
    if (g_hwndSpeedUp)    SendMessageW(g_hwndSpeedUp, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontValue), TRUE);
    if (g_hwndTotdTitle)  SendMessageW(g_hwndTotdTitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
    if (g_hwndTotalDown)  SendMessageW(g_hwndTotalDown, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
    if (g_hwndTotuTitle)  SendMessageW(g_hwndTotuTitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
    if (g_hwndTotalUp)    SendMessageW(g_hwndTotalUp, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
    if (g_hwndLifetimeTitle) SendMessageW(g_hwndLifetimeTitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontValue), TRUE);
    if (g_hwndLifetimeDown) SendMessageW(g_hwndLifetimeDown, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
    if (g_hwndLifetimeDownValue) SendMessageW(g_hwndLifetimeDownValue, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
    if (g_hwndLifetimeUp) SendMessageW(g_hwndLifetimeUp, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
    if (g_hwndLifetimeUpValue) SendMessageW(g_hwndLifetimeUpValue, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
    if (g_hwndLifetimeReset) SendMessageW(g_hwndLifetimeReset, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
    if (g_hwndAuthor)     SendMessageW(g_hwndAuthor, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontAuthor), TRUE);

    HWND settingsControls[] = {
        g_hwndStatusGroup, g_settingsUi.hwndGroup,
        g_settingsUi.hwndLblDownPrefix, g_settingsUi.hwndEditDownPrefix,
        g_settingsUi.hwndLblUpPrefix, g_settingsUi.hwndEditUpPrefix,
        g_settingsUi.hwndLblDown, g_settingsUi.hwndBtnDown, g_settingsUi.hwndCheckDownAuto,
        g_settingsUi.hwndLblUp, g_settingsUi.hwndBtnUp, g_settingsUi.hwndCheckUpAuto,
        g_settingsUi.hwndLblFont, g_settingsUi.hwndBtnFont,
        g_settingsUi.hwndLblAnchor, g_settingsUi.hwndComboAnchor,
        g_settingsUi.hwndLblOffset, g_settingsUi.hwndEditOffset,
        g_settingsUi.hwndSpinOffset, g_settingsUi.hwndLblOffsetUnit,
        g_settingsUi.hwndBtnOffsetReset, g_settingsUi.hwndLblUnits,
        g_settingsUi.hwndComboUnits, g_settingsUi.hwndLblUnit,
        g_settingsUi.hwndComboUnit, g_settingsUi.hwndLblDecimals,
        g_settingsUi.hwndComboDecimals, g_settingsUi.hwndCheckWidget,
        g_settingsUi.hwndCheckTray, g_settingsUi.hwndCheckStartup,
        g_settingsUi.hwndCheckEmbed, g_settingsUi.hwndBtnApply, g_settingsUi.hwndBtnExit,
    };
    for (HWND control : settingsControls) {
        if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
    }

    RelayoutMainControls(dpi);

    if (oldFontLabel)   DeleteObject(oldFontLabel);
    if (oldFontValue)   DeleteObject(oldFontValue);
    if (oldFontAuthor)  DeleteObject(oldFontAuthor);

}

static void UpdateSpeedValues(ULONGLONG downBps, ULONGLONG upBps) {
    g_currentDownBps = downBps;
    g_currentUpBps = upBps;
    const bool bits = g_settings.speedBits != 0;
    FormatSpeed(downBps, g_settings.minimumSpeedUnit, g_settings.decimalPlaces,
                g_szDownSpeed, _countof(g_szDownSpeed), bits);
    FormatSpeed(upBps, g_settings.minimumSpeedUnit, g_settings.decimalPlaces,
                g_szUpSpeed, _countof(g_szUpSpeed), bits);
    FormatCompact(downBps, g_szDownCompact, _countof(g_szDownCompact), bits);
    FormatCompact(upBps, g_szUpCompact, _countof(g_szUpCompact), bits);

    if (g_hwndSpeedDown) SetWindowTextW(g_hwndSpeedDown, g_szDownSpeed);
    if (g_hwndSpeedUp) SetWindowTextW(g_hwndSpeedUp, g_szUpSpeed);
}

// ---- Tray Icon Generation ----------------------------------------------------
// Drawn at the size the shell uses for the taskbar's DPI, so it is not rescaled
// (a fixed 16 px icon was tripled into a blur at 300%).
static HICON CreateSpeedTrayIcon(const wchar_t* downSpeed, const wchar_t* upSpeed) {
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    UINT dpi = taskbar ? GetDpiForWindow(taskbar) : 0;
    if (dpi == 0) dpi = static_cast<UINT>(g_currentDpi);
    return CreateMeterIcon(GetTrayIconSizeForDpi(dpi), downSpeed, upSpeed,
                           ResolveMeterColor(g_settings.down, g_taskbarLight),
                           ResolveMeterColor(g_settings.up, g_taskbarLight));
}

static void BuildTrayTooltip(wchar_t* out, size_t maxLen) {
    wchar_t down[96] = {};
    wchar_t up[96] = {};
    FormatPrefixedSpeed(g_settings.downPrefix, g_szDownSpeed, down, _countof(down));
    FormatPrefixedSpeed(g_settings.upPrefix, g_szUpSpeed, up, _countof(up));
    _snwprintf_s(out, maxLen, _TRUNCATE, L"WinNetMeter\n%s\n%s", down, up);
}

static void UpdateTrayIcon() {
    if (!g_settings.showTrayIcon) return;

    HICON hNewIcon = CreateSpeedTrayIcon(g_szDownCompact, g_szUpCompact);

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwndMain;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hNewIcon;
    BuildTrayTooltip(nid.szTip, _countof(nid.szTip));

    Shell_NotifyIconW(NIM_MODIFY, &nid);

    if (g_hCurrentTrayIcon) {
        DestroyIcon(g_hCurrentTrayIcon);
    }
    g_hCurrentTrayIcon = hNewIcon;
}

static void SetupTrayIcon() {
    if (!g_settings.showTrayIcon) {
        RemoveTrayIcon();
        return;
    }

    NOTIFYICONDATAW old = {};
    old.cbSize = sizeof(old);
    old.hWnd = g_hwndMain;
    old.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &old);

    if (g_hCurrentTrayIcon) {
        DestroyIcon(g_hCurrentTrayIcon);
        g_hCurrentTrayIcon = nullptr;
    }

    g_hCurrentTrayIcon = CreateSpeedTrayIcon(g_szDownCompact, g_szUpCompact);

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwndMain;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = g_hCurrentTrayIcon;
    BuildTrayTooltip(nid.szTip, _countof(nid.szTip));

    Shell_NotifyIconW(NIM_ADD, &nid);
}

static void RemoveTrayIcon() {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwndMain;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);

    if (g_hCurrentTrayIcon) {
        DestroyIcon(g_hCurrentTrayIcon);
        g_hCurrentTrayIcon = nullptr;
    }
}

// ---- Adapter selection -------------------------------------------------------
// The persisted choice (g_settings.adapterAuto / adapterLuid / adapterAlias) is
// the source of truth; g_selectedLuid is what that choice resolves to now.
static ULONGLONG g_lastFailTick = 0;

static void SetMeteredAdapter(NET_LUID luid, bool newSession) {
    if (luid.Value == g_selectedLuid.Value) return;
    g_selectedLuid = luid;
    if (newSession) {
        g_sampler.Reset(luid);
    } else {
        g_sampler.Rebind(luid);   // same connection on another adapter: keep session totals
    }
}

// Automatic: the adapter carrying the default route (metered even if its type
// is not in the list). Manual: the remembered choice, matched by LUID, then by
// name. Zero when it cannot be found right now.
static NET_LUID ResolveAdapter(const AdapterInfo* list, int count) {
    NET_LUID none = {};
    if (g_settings.adapterAuto) {
        NET_LUID route = {};
        return GetDefaultRouteLuid(&route) ? route : none;
    }
    NET_LUID saved = {};
    saved.Value = g_settings.adapterLuid;
    int index = ChooseAdapter(list, count, false, none, saved, g_settings.adapterAlias);
    if (index < 0) return none;
    g_settings.adapterLuid = list[index].luid.Value;   // may have come back under a new LUID
    return list[index].luid;
}

static void BuildAutomaticLabel(wchar_t* out, size_t maxLen) {
    NET_LUID route = {};
    wchar_t alias[NDIS_IF_MAX_STRING_SIZE + 1] = {};
    if (GetDefaultRouteLuid(&route) &&
        ConvertInterfaceLuidToAlias(&route, alias, _countof(alias)) == NO_ERROR && alias[0]) {
        _snwprintf_s(out, maxLen, _TRUNCATE, L"Automatic (%s)", alias);
    } else {
        wcscpy_s(out, maxLen, L"Automatic");
    }
}

static void SelectCurrentAdapterInCombo() {
    int selection = -1;
    if (g_settings.adapterAuto) {
        selection = 0;
    } else if (g_selectedLuid.Value != 0) {
        for (int i = 0; i < g_comboLuidCount; ++i) {
            if (g_comboLuids[i].Value == g_selectedLuid.Value) {
                selection = i + 1;
                break;
            }
        }
    }
    SendMessageW(g_combo, CB_SETCURSEL, static_cast<WPARAM>(selection), 0);
}

// Relabels the Automatic item when the internet connection moves, without
// disturbing the selection or an open dropdown.
static void RefreshAutomaticItem() {
    if (!g_combo || SendMessageW(g_combo, CB_GETDROPPEDSTATE, 0, 0)) return;
    wchar_t label[NDIS_IF_MAX_STRING_SIZE + 16] = {};
    BuildAutomaticLabel(label, _countof(label));
    wchar_t current[NDIS_IF_MAX_STRING_SIZE + 16] = {};
    if (SendMessageW(g_combo, CB_GETLBTEXTLEN, 0, 0) < static_cast<LRESULT>(_countof(current))) {
        SendMessageW(g_combo, CB_GETLBTEXT, 0, reinterpret_cast<LPARAM>(current));
    }
    if (wcscmp(current, label) == 0) return;
    int selection = static_cast<int>(SendMessageW(g_combo, CB_GETCURSEL, 0, 0));
    SendMessageW(g_combo, CB_DELETESTRING, 0, 0);
    SendMessageW(g_combo, CB_INSERTSTRING, 0, reinterpret_cast<LPARAM>(label));
    SendMessageW(g_combo, CB_SETCURSEL, static_cast<WPARAM>(selection), 0);
}

static void PopulateAdapters() {
    AdapterInfo list[64];
    int count = GetAdapters(list, 64);

    SendMessageW(g_combo, CB_RESETCONTENT, 0, 0);
    wchar_t label[NDIS_IF_MAX_STRING_SIZE + 16] = {};
    BuildAutomaticLabel(label, _countof(label));
    SendMessageW(g_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    g_comboLuidCount = 0;
    for (int i = 0; i < count; ++i) {
        int item = static_cast<int>(SendMessageW(g_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(list[i].name)));
        if (item >= 1 && item <= static_cast<int>(_countof(g_comboLuids))) {
            g_comboLuids[item - 1] = list[i].luid;
            g_comboLuidCount = max(g_comboLuidCount, item);
        }
    }
    SelectCurrentAdapterInCombo();
}

static void InitializeAdapterSelection() {
    AdapterInfo list[64];
    int count = GetAdapters(list, 64);
    g_selectedLuid = ResolveAdapter(list, count);
    g_sampler.Reset(g_selectedLuid);
    PopulateAdapters();
}

// Manual mode only: the chosen adapter vanished or failed to sample; look for
// it again (same LUID, or the same name under a new LUID). Never substitutes a
// different adapter.
static void RecoverManualAdapter() {
    ULONGLONG now = GetTickCount64();
    if (now - g_lastFailTick < 3000) return;
    g_lastFailTick = now;
    AdapterInfo list[64];
    int count = GetAdapters(list, 64);
    NET_LUID luid = ResolveAdapter(list, count);
    if (luid.Value != g_selectedLuid.Value) {
        SetMeteredAdapter(luid, false);
        if (!SendMessageW(g_combo, CB_GETDROPPEDSTATE, 0, 0)) PopulateAdapters();
    }
}

static void OnTimerTick() {
    if (g_settings.adapterAuto) {
        // Follow the internet connection as it moves (Wi-Fi <-> Ethernet, VPN up/down).
        NET_LUID route = {};
        if (GetDefaultRouteLuid(&route) && route.Value != g_selectedLuid.Value) {
            SetMeteredAdapter(route, false);
            RefreshAutomaticItem();
        }
    } else if (g_selectedLuid.Value == 0) {
        RecoverManualAdapter();
    }

    if (g_selectedLuid.Value == 0) {
        UpdateSpeedValues(0, 0);
        UpdateTrayIcon();
        UpdateMeter();
        return;
    }

    ULONGLONG downBps = 0, upBps = 0;
    ULONGLONG previousDown = g_sampler.totalIn;
    ULONGLONG previousUp = g_sampler.totalOut;
    if (g_sampler.Sample(g_selectedLuid, &downBps, &upBps)) {
        UpdateSpeedValues(downBps, upBps);
        ULONGLONG downloaded = g_sampler.totalIn - previousDown;
        ULONGLONG uploaded = g_sampler.totalOut - previousUp;
        AddLifetimeTraffic(&g_settings, downloaded, uploaded);
        UpdateTotalValues();

        ULONGLONG now = GetTickCount64();
        if (downloaded || uploaded) g_totalsDirty = true;
        if (g_totalsDirty && now - g_lastTotalsSaveTick >= 60000) {
            // ponytail: abnormal termination can lose at most 60 seconds; add a journal only if exact crash durability matters.
            // Stay dirty when the write fails so the next window retries instead
            // of silently dropping the accumulated totals.
            if (PersistSettingsNow()) g_totalsDirty = false;
            g_lastTotalsSaveTick = now;
        }
    } else {
        // Sampling failed (adapter removed). Automatic mode re-resolves next tick.
        if (!g_settings.adapterAuto) RecoverManualAdapter();
        UpdateSpeedValues(0, 0);
    }

    UpdateTrayIcon();
    UpdateMeter();
}

static void OnComboSelectionChanged() {
    int sel = static_cast<int>(SendMessageW(g_combo, CB_GETCURSEL, 0, 0));
    NET_LUID target = {};
    if (sel == 0) {
        g_settings.adapterAuto = 1;
        GetDefaultRouteLuid(&target);
    } else if (sel >= 1 && sel <= g_comboLuidCount) {
        target = g_comboLuids[sel - 1];
        g_settings.adapterAuto = 0;
        g_settings.adapterLuid = target.Value;
        // Item text is the adapter alias (at most 127 characters, see AdapterInfo::name).
        SendMessageW(g_combo, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(g_settings.adapterAlias));
    } else {
        return;
    }
    PersistSettingsNow();

    if (target.Value != g_selectedLuid.Value) {
        SetMeteredAdapter(target, true);   // the user picked another adapter: new session
        UpdateSpeedValues(0, 0);
        UpdateTotalValues();
        UpdateTrayIcon();
        UpdateMeter();
    }
}

static void ShowMainWindow() {
    if (g_hwndMain) {
        if (!IsWindowVisible(g_hwndMain)) RefreshSettingsControls();
        ShowWindow(g_hwndMain, IsIconic(g_hwndMain) ? SW_RESTORE : SW_SHOW);
        SetForegroundWindow(g_hwndMain);
        // Surface a background save failure once the user is actually looking.
        if (g_settingsSaveFailed) ReportSettingsSaveFailure(g_hwndMain);
    }
}

static void UpdateTotalValues() {
    wchar_t text[64] = {};
    FormatBytes(g_sampler.totalIn, text, _countof(text));
    if (g_hwndTotalDown) SetWindowTextW(g_hwndTotalDown, text);
    FormatBytes(g_sampler.totalOut, text, _countof(text));
    if (g_hwndTotalUp) SetWindowTextW(g_hwndTotalUp, text);

    FormatBytes(g_settings.lifetimeDownloaded, text, _countof(text));
    if (g_hwndLifetimeDownValue) SetWindowTextW(g_hwndLifetimeDownValue, text);
    FormatBytes(g_settings.lifetimeUploaded, text, _countof(text));
    if (g_hwndLifetimeUpValue) SetWindowTextW(g_hwndLifetimeUpValue, text);

    wchar_t date[80] = {};   // the user's short-date format; some locales are long
    wchar_t title[128] = L"Total data";
    if (FormatLifetimeSinceDate(g_settings.lifetimeSince, date, _countof(date))) {
        _snwprintf_s(title, _countof(title), _TRUNCATE, L"Total data since %s", date);
    }
    if (g_hwndLifetimeTitle) SetWindowTextW(g_hwndLifetimeTitle, title);
}

// Returns false when the dialog is cancelled. The dialog opens on the colour
// currently shown, resolving Automatic for the current theme.
static bool PickColor(HWND hwndOwner, COLORREF* color) {
    static COLORREF customColors[16] = {};
    CHOOSECOLORW cc = {};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hwndOwner;
    cc.rgbResult = ResolveMeterColor(*color, g_taskbarLight);
    cc.lpCustColors = customColors;
    cc.Flags = CC_RGBINIT | CC_FULLOPEN;
    if (!ChooseColorW(&cc)) return false;
    *color = cc.rgbResult;
    return true;
}

static void PickFont(HWND hwndOwner, AppSettings* s, int dpi) {
    LOGFONTW lf = {};
    lf.lfHeight = -MulDiv(static_cast<int>(s->fontSize * 96.0 / 72.0 + 0.5), dpi, 96);
    lf.lfWeight = (s->fontStyle & 1) ? FW_BOLD : FW_REGULAR;
    lf.lfItalic = (s->fontStyle & 2) ? TRUE : FALSE;
    lf.lfUnderline = (s->fontStyle & 4) ? TRUE : FALSE;
    lf.lfStrikeOut = (s->fontStyle & 8) ? TRUE : FALSE;
    wcsncpy_s(lf.lfFaceName, _countof(lf.lfFaceName), s->fontFamily, _TRUNCATE);

    CHOOSEFONTW cf = {};
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = hwndOwner;
    cf.lpLogFont = &lf;
    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_EFFECTS;
    if (ChooseFontW(&cf)) {
        wcsncpy_s(s->fontFamily, _countof(s->fontFamily), lf.lfFaceName, _TRUNCATE);
        s->fontSize = cf.iPointSize / 10.0;
        int style = 0;
        if (lf.lfWeight >= FW_BOLD) style |= 1;
        if (lf.lfItalic) style |= 2;
        if (lf.lfUnderline) style |= 4;
        if (lf.lfStrikeOut) style |= 8;
        s->fontStyle = style;
    }
}

// The minimum-unit choices read in the selected units; the stored value is the
// same index either way (Auto, kilo, mega, giga).
static void RelabelMinimumUnitChoices(bool bits) {
    HWND combo = g_settingsUi.hwndComboUnit;
    if (!combo) return;
    static const wchar_t* const bytesChoices[] = { L"Auto", L"KB/s", L"MB/s", L"GB/s" };
    static const wchar_t* const bitsChoices[] = { L"Auto", L"Kbps", L"Mbps", L"Gbps" };
    const wchar_t* const* choices = bits ? bitsChoices : bytesChoices;
    int selection = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < 4; ++i) {
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choices[i]));
    }
    SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(selection), 0);
}

static HWND CreateMainButton(HWND parent, const wchar_t* text, int id, DWORD style = BS_PUSHBUTTON) {
    DWORD tabStop = style == BS_GROUPBOX ? 0 : WS_TABSTOP;
    return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | tabStop | style,
                           0, 0, 0, 0, parent,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_hInst, nullptr);
}

static HWND CreateMainLabel(HWND parent, const wchar_t* text, int id) {
    return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT,
                           0, 0, 0, 0, parent,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_hInst, nullptr);
}

static void CreateSettingsControls(HWND hwnd) {
    SettingsUiState& state = g_settingsUi;
    state.tempSettings = g_settings;
    state.hwndGroup = CreateMainButton(hwnd, L"Settings", ID_SETTINGS_GROUP, BS_GROUPBOX);
    state.hwndLblUpPrefix = CreateMainLabel(hwnd, L"Upload prefix:", ID_SET_UP_PREFIX_LBL);
    state.hwndEditUpPrefix = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SET_UP_PREFIX_EDIT)), g_hInst, nullptr);
    state.hwndLblDownPrefix = CreateMainLabel(hwnd, L"Download prefix:", ID_SET_DOWN_PREFIX_LBL);
    state.hwndEditDownPrefix = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SET_DOWN_PREFIX_EDIT)), g_hInst, nullptr);
    SendMessageW(state.hwndEditDownPrefix, EM_SETLIMITTEXT, METER_PREFIX_CAPACITY - 1, 0);
    SendMessageW(state.hwndEditUpPrefix, EM_SETLIMITTEXT, METER_PREFIX_CAPACITY - 1, 0);
    state.hwndLblUp = CreateMainLabel(hwnd, L"Upload color:", ID_SET_UP_LBL);
    state.hwndBtnUp = CreateMainButton(hwnd, L"Select", ID_SET_UP_BTN);
    state.hwndCheckUpAuto = CreateMainButton(hwnd, L"Automatic", ID_SET_UP_AUTO, BS_AUTOCHECKBOX);
    state.hwndLblDown = CreateMainLabel(hwnd, L"Download color:", ID_SET_DOWN_LBL);
    state.hwndBtnDown = CreateMainButton(hwnd, L"Select", ID_SET_DOWN_BTN);
    state.hwndCheckDownAuto = CreateMainButton(hwnd, L"Automatic", ID_SET_DOWN_AUTO, BS_AUTOCHECKBOX);
    state.hwndLblFont = CreateMainLabel(hwnd, L"Taskbar meter font:", ID_SET_FONT_LBL);
    state.hwndBtnFont = CreateMainButton(hwnd, L"Choose", ID_SET_FONT_BTN);
    state.hwndLblAnchor = CreateMainLabel(hwnd, L"Meter position:", ID_SET_ANCHOR_LBL);
    state.hwndComboAnchor = CreateWindowExW(
        0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SET_ANCHOR_COMBO)), g_hInst, nullptr);
    const wchar_t* anchorNames[] = { L"Next to the tray", L"After the app buttons", L"Left edge",
                                     L"Classic (fixed point)" };
    for (const wchar_t* name : anchorNames) {
        SendMessageW(state.hwndComboAnchor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
    }
    state.hwndLblOffset = CreateMainLabel(hwnd, L"Offset from there:", ID_SET_OFFSET_LBL);
    state.hwndEditOffset = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_RIGHT | ES_AUTOHSCROLL,
        0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SET_OFFSET_EDIT)), g_hInst, nullptr);
    state.hwndSpinOffset = CreateWindowExW(
        0, UPDOWN_CLASSW, nullptr,
        WS_CHILD | WS_VISIBLE | UDS_ARROWKEYS | UDS_SETBUDDYINT | UDS_NOTHOUSANDS,
        0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SET_OFFSET_SPIN)), g_hInst, nullptr);
    state.hwndLblOffsetUnit = CreateMainLabel(hwnd, L"px", ID_SET_OFFSET_UNIT);
    state.hwndBtnOffsetReset = CreateMainButton(hwnd, L"Reset meter", ID_SET_OFFSET_RESET);
    SendMessageW(state.hwndSpinOffset, UDM_SETRANGE32,
                 static_cast<WPARAM>(static_cast<INT_PTR>(TASKBAR_METER_OFFSET_MIN)),
                 static_cast<LPARAM>(TASKBAR_METER_OFFSET_MAX));
    SendMessageW(state.hwndSpinOffset, UDM_SETBUDDY,
                 reinterpret_cast<WPARAM>(state.hwndEditOffset), 0);

    state.hwndLblUnits = CreateMainLabel(hwnd, L"Show speed in:", ID_SET_UNITS_LBL);
    state.hwndComboUnits = CreateWindowExW(
        0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SET_UNITS_COMBO)), g_hInst, nullptr);
    const wchar_t* unitsChoices[] = { L"Bytes (KB/s, MB/s)", L"Bits (Kbps, Mbps)" };
    for (const wchar_t* choice : unitsChoices) {
        SendMessageW(state.hwndComboUnits, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choice));
    }

    state.hwndLblUnit = CreateMainLabel(hwnd, L"Minimum speed unit:", ID_SET_UNIT_LBL);
    state.hwndComboUnit = CreateWindowExW(
        0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SET_UNIT_COMBO)), g_hInst, nullptr);
    RelabelMinimumUnitChoices(false);

    state.hwndLblDecimals = CreateMainLabel(hwnd, L"Decimal places:", ID_SET_DECIMALS_LBL);
    state.hwndComboDecimals = CreateWindowExW(
        0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SET_DECIMALS_COMBO)), g_hInst, nullptr);
    for (int decimal = SPEED_DECIMAL_PLACES_MIN; decimal <= SPEED_DECIMAL_PLACES_MAX; ++decimal) {
        wchar_t text[2] = { static_cast<wchar_t>(L'0' + decimal), L'\0' };
        SendMessageW(state.hwndComboDecimals, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    }

    state.hwndCheckWidget = CreateMainButton(hwnd, L"Show taskbar meter", ID_SET_WIDGET_CHECK, BS_AUTOCHECKBOX);
    state.hwndCheckTray = CreateMainButton(hwnd, L"Show tray icon", ID_SET_TRAY_CHECK, BS_AUTOCHECKBOX);
    state.hwndCheckStartup = CreateMainButton(hwnd, L"Start with Windows", ID_SET_STARTUP_CHECK, BS_AUTOCHECKBOX);
    state.hwndCheckEmbed = CreateMainButton(hwnd, L"Embed in taskbar (stays visible over Start)",
                                            ID_SET_EMBED_CHECK, BS_AUTOCHECKBOX);
    state.hwndBtnApply = CreateMainButton(hwnd, L"Apply", ID_SET_SAVE_BTN, BS_DEFPUSHBUTTON);
    state.hwndBtnExit = CreateMainButton(hwnd, L"Exit", ID_EXIT_APP);
    RefreshSettingsControls();
}

static void RefreshSettingsControls() {
    SettingsUiState& state = g_settingsUi;
    state.tempSettings = g_settings;
    if (!state.hwndEditOffset) return;
    state.refreshing = true;
    SetWindowTextW(state.hwndEditDownPrefix, g_settings.downPrefix);
    SetWindowTextW(state.hwndEditUpPrefix, g_settings.upPrefix);
    SendMessageW(state.hwndSpinOffset, UDM_SETPOS32, 0, static_cast<LPARAM>(g_settings.taskbarOffset));
    int anchorIndex = 0;
    for (int i = 0; i < static_cast<int>(_countof(ANCHOR_CHOICES)); ++i) {
        if (ANCHOR_CHOICES[i] == g_settings.meterAnchor) anchorIndex = i;
    }
    SendMessageW(state.hwndComboAnchor, CB_SETCURSEL, static_cast<WPARAM>(anchorIndex), 0);
    SendMessageW(state.hwndComboUnits, CB_SETCURSEL, g_settings.speedBits ? 1 : 0, 0);
    RelabelMinimumUnitChoices(g_settings.speedBits != 0);
    SendMessageW(state.hwndComboUnit, CB_SETCURSEL, static_cast<WPARAM>(g_settings.minimumSpeedUnit), 0);
    SendMessageW(state.hwndComboDecimals, CB_SETCURSEL, g_settings.decimalPlaces, 0);
    SendMessageW(state.hwndCheckUpAuto, BM_SETCHECK, g_settings.up == METER_COLOR_AUTO ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(state.hwndCheckDownAuto, BM_SETCHECK, g_settings.down == METER_COLOR_AUTO ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(state.hwndCheckWidget, BM_SETCHECK, g_settings.showWidget ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(state.hwndCheckTray, BM_SETCHECK, g_settings.showTrayIcon ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(state.hwndCheckStartup, BM_SETCHECK, g_settings.startWithWindows ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(state.hwndCheckEmbed, BM_SETCHECK, g_settings.embedInTaskbar ? BST_CHECKED : BST_UNCHECKED, 0);
    state.refreshing = false;
}

// persistImmediately=false is for edit-box changes, which arrive one per
// keystroke; one-shot actions (buttons, combo selections) still write at once
// so the file always matches the UI the moment the user stops interacting.
static void ApplyLiveMeterSettings(bool fontChanged = false, bool persistImmediately = true) {
    SettingsUiState& state = g_settingsUi;
    g_settings.down = state.tempSettings.down;
    g_settings.up = state.tempSettings.up;
    wcscpy_s(g_settings.downPrefix, _countof(g_settings.downPrefix), state.tempSettings.downPrefix);
    wcscpy_s(g_settings.upPrefix, _countof(g_settings.upPrefix), state.tempSettings.upPrefix);
    wcscpy_s(g_settings.fontFamily, _countof(g_settings.fontFamily), state.tempSettings.fontFamily);
    g_settings.fontSize = state.tempSettings.fontSize;
    g_settings.fontStyle = state.tempSettings.fontStyle;
    g_settings.meterAnchor = state.tempSettings.meterAnchor;
    g_settings.taskbarOffset = state.tempSettings.taskbarOffset;
    g_settings.minimumSpeedUnit = state.tempSettings.minimumSpeedUnit;
    g_settings.decimalPlaces = state.tempSettings.decimalPlaces;
    g_settings.speedBits = state.tempSettings.speedBits;
    if (persistImmediately) {
        PersistSettingsNow();
    } else {
        SchedulePersistSettings();
    }
    UpdateSpeedValues(g_currentDownBps, g_currentUpBps);
    if (fontChanged) RefreshFontsAndRelayout(g_currentDpi);
    UpdateTrayIcon();
    UpdateMeter();
}

static bool ApplySettings(HWND hwnd) {
    SettingsUiState& state = g_settingsUi;
    GetWindowTextW(state.hwndEditDownPrefix, state.tempSettings.downPrefix,
                   _countof(state.tempSettings.downPrefix));
    GetWindowTextW(state.hwndEditUpPrefix, state.tempSettings.upPrefix,
                   _countof(state.tempSettings.upPrefix));
    wchar_t text[32] = {};
    GetWindowTextW(state.hwndEditOffset, text, _countof(text));
    int offset = 0;
    if (!ParseTaskbarMeterOffset(text, &offset)) {
        MessageBoxW(hwnd, L"Enter a whole number from -4096 to 4096.",
                    L"WinNetMeter", MB_OK | MB_ICONWARNING);
        SetFocus(state.hwndEditOffset);
        return false;
    }

    state.tempSettings.taskbarOffset = offset;
    int anchor = static_cast<int>(SendMessageW(state.hwndComboAnchor, CB_GETCURSEL, 0, 0));
    if (anchor >= 0 && anchor < static_cast<int>(_countof(ANCHOR_CHOICES))) {
        state.tempSettings.meterAnchor = ANCHOR_CHOICES[anchor];
    }
    int units = static_cast<int>(SendMessageW(state.hwndComboUnits, CB_GETCURSEL, 0, 0));
    int unit = static_cast<int>(SendMessageW(state.hwndComboUnit, CB_GETCURSEL, 0, 0));
    int decimals = static_cast<int>(SendMessageW(state.hwndComboDecimals, CB_GETCURSEL, 0, 0));
    if (units == 0 || units == 1) state.tempSettings.speedBits = units;
    if (unit >= 0 && unit <= 3) state.tempSettings.minimumSpeedUnit = static_cast<MinimumSpeedUnit>(unit);
    if (decimals >= SPEED_DECIMAL_PLACES_MIN && decimals <= SPEED_DECIMAL_PLACES_MAX) {
        state.tempSettings.decimalPlaces = decimals;
    }
    state.tempSettings.showWidget = SendMessageW(state.hwndCheckWidget, BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.tempSettings.showTrayIcon = SendMessageW(state.hwndCheckTray, BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.tempSettings.startWithWindows = SendMessageW(state.hwndCheckStartup, BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.tempSettings.embedInTaskbar = SendMessageW(state.hwndCheckEmbed, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (!HasUiEntryPoint(state.tempSettings)) {
        MessageBoxW(hwnd, L"Keep either the taskbar meter or tray icon enabled so WinNetMeter can be opened.",
                    L"WinNetMeter", MB_OK | MB_ICONWARNING);
        return false;
    }
    if (!SetStartWithWindowsEnabled(state.tempSettings.startWithWindows != 0)) {
        MessageBoxW(hwnd, L"Windows startup registration could not be updated.",
                    L"WinNetMeter", MB_OK | MB_ICONERROR);
        return false;
    }

    // Totals and the adapter choice change outside the settings panel (timer,
    // Status combo) after tempSettings was copied; keep their live values.
    state.tempSettings.lifetimeDownloaded = g_settings.lifetimeDownloaded;
    state.tempSettings.lifetimeUploaded = g_settings.lifetimeUploaded;
    wcscpy_s(state.tempSettings.lifetimeSince, _countof(state.tempSettings.lifetimeSince),
             g_settings.lifetimeSince);
    state.tempSettings.adapterAuto = g_settings.adapterAuto;
    state.tempSettings.adapterLuid = g_settings.adapterLuid;
    wcscpy_s(state.tempSettings.adapterAlias, _countof(state.tempSettings.adapterAlias),
             g_settings.adapterAlias);
    g_settings = state.tempSettings;
    const bool saved = PersistSettingsNow();
    UpdateSpeedValues(g_currentDownBps, g_currentUpBps);
    RefreshFontsAndRelayout(g_currentDpi);
    SetupTrayIcon();
    UpdateMeter();
    InvalidateRect(g_hwndMain, nullptr, TRUE);
    RefreshSettingsControls();
    if (!saved) {
        // Apply is an explicit user action, so report the failure immediately.
        ReportSettingsSaveFailure(hwnd);
        return false;
    }
    return true;
}

static bool HandleSettingsCommand(HWND hwnd, int id, int code) {
    SettingsUiState& state = g_settingsUi;
    if ((id == ID_SET_DOWN_PREFIX_EDIT || id == ID_SET_UP_PREFIX_EDIT) && code == EN_CHANGE) {
        if (state.refreshing) return true;
        GetWindowTextW(state.hwndEditDownPrefix, state.tempSettings.downPrefix,
                       _countof(state.tempSettings.downPrefix));
        GetWindowTextW(state.hwndEditUpPrefix, state.tempSettings.upPrefix,
                       _countof(state.tempSettings.upPrefix));
        ApplyLiveMeterSettings(false, false);
    } else if (id == ID_SET_OFFSET_EDIT && code == EN_CHANGE) {
        if (state.refreshing) return true;
        wchar_t text[32] = {};
        GetWindowTextW(state.hwndEditOffset, text, _countof(text));
        int offset = 0;
        if (ParseTaskbarMeterOffset(text, &offset)) {
            state.tempSettings.taskbarOffset = offset;
            ApplyLiveMeterSettings(false, false);
        }
    } else if (id == ID_SET_ANCHOR_COMBO && code == CBN_SELCHANGE) {
        int selection = static_cast<int>(SendMessageW(state.hwndComboAnchor, CB_GETCURSEL, 0, 0));
        if (selection >= 0 && selection < static_cast<int>(_countof(ANCHOR_CHOICES)) &&
            ANCHOR_CHOICES[selection] != state.tempSettings.meterAnchor) {
            state.tempSettings.meterAnchor = ANCHOR_CHOICES[selection];
            // Offsets are relative to the anchor, so an old one means nothing here.
            state.tempSettings.taskbarOffset = 0;
            state.refreshing = true;
            SendMessageW(state.hwndSpinOffset, UDM_SETPOS32, 0, 0);
            state.refreshing = false;
            ApplyLiveMeterSettings();
        }
    } else if (id == ID_SET_OFFSET_RESET) {
        state.tempSettings = AppSettings();
        ApplyLiveMeterSettings(true);
        RefreshSettingsControls();
    } else if (id == ID_SET_UNIT_COMBO && code == CBN_SELCHANGE) {
        int selection = static_cast<int>(SendMessageW(state.hwndComboUnit, CB_GETCURSEL, 0, 0));
        if (selection >= 0 && selection <= 3) {
            state.tempSettings.minimumSpeedUnit = static_cast<MinimumSpeedUnit>(selection);
            ApplyLiveMeterSettings();
        }
    } else if (id == ID_SET_UNITS_COMBO && code == CBN_SELCHANGE) {
        int selection = static_cast<int>(SendMessageW(state.hwndComboUnits, CB_GETCURSEL, 0, 0));
        if (selection == 0 || selection == 1) {
            state.tempSettings.speedBits = selection;
            RelabelMinimumUnitChoices(selection == 1);
            ApplyLiveMeterSettings();
        }
    } else if (id == ID_SET_DECIMALS_COMBO && code == CBN_SELCHANGE) {
        int selection = static_cast<int>(SendMessageW(state.hwndComboDecimals, CB_GETCURSEL, 0, 0));
        if (selection >= SPEED_DECIMAL_PLACES_MIN && selection <= SPEED_DECIMAL_PLACES_MAX) {
            state.tempSettings.decimalPlaces = selection;
            ApplyLiveMeterSettings();
        }
    } else if (id == ID_SET_DOWN_BTN || id == ID_SET_UP_BTN) {
        const bool down = id == ID_SET_DOWN_BTN;
        COLORREF& color = down ? state.tempSettings.down : state.tempSettings.up;
        COLORREF chosen = color;
        if (PickColor(hwnd, &chosen)) {
            color = chosen;
            SendMessageW(down ? state.hwndCheckDownAuto : state.hwndCheckUpAuto, BM_SETCHECK, BST_UNCHECKED, 0);
            ApplyLiveMeterSettings();
        }
    } else if ((id == ID_SET_DOWN_AUTO || id == ID_SET_UP_AUTO) && code == BN_CLICKED) {
        const bool down = id == ID_SET_DOWN_AUTO;
        COLORREF& color = down ? state.tempSettings.down : state.tempSettings.up;
        const bool automatic = SendMessageW(down ? state.hwndCheckDownAuto : state.hwndCheckUpAuto,
                                            BM_GETCHECK, 0, 0) == BST_CHECKED;
        // Turning Automatic off keeps the colour on screen, now as a fixed choice.
        color = automatic ? METER_COLOR_AUTO : ResolveMeterColor(color, g_taskbarLight);
        ApplyLiveMeterSettings();
    } else if (id == ID_SET_FONT_BTN) {
        PickFont(hwnd, &state.tempSettings, g_currentDpi);
        ApplyLiveMeterSettings(true);
    } else if (id == ID_SET_SAVE_BTN) {
        ApplySettings(hwnd);
    } else if (id == ID_LIFETIME_RESET) {
        if (MessageBoxW(hwnd,
                        L"Reset the saved download and upload totals?\n\nThis cannot be undone.",
                        L"Reset total data", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES) {
            ResetLifetimeTotals(&g_settings);
            const bool saved = PersistSettingsNow();
            g_lastTotalsSaveTick = GetTickCount64();
            g_totalsDirty = !saved;
            UpdateTotalValues();
            if (!saved) ReportSettingsSaveFailure(hwnd);
        }
    } else if (id == ID_EXIT_APP) {
        DestroyWindow(hwnd);
    } else {
        return false;
    }
    return true;
}

// ---- Main Window Procedure ---------------------------------------------------
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == g_uTaskbarCreatedMsg && g_uTaskbarCreatedMsg != 0) {
        SetupTrayIcon();
        UpdateMeter();
        return 0;
    }

    switch (msg) {
    case WM_CREATE: {
        g_hwndMain = hwnd;
        // Without the meter thread the app still runs, reachable from the tray.
        StartMeterHost(hwnd, WM_OPEN_FROM_METER);
        UpdateSpeedValues(0, 0);

        auto mkLabel = [&](const wchar_t* text, int id, UINT ss) {
            return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | ss,
                                  0, 0, 0, 0,
                                  hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_hInst, nullptr);
        };

        g_hwndStatusGroup = CreateMainButton(hwnd, L"Status", ID_STATUS_GROUP, BS_GROUPBOX);
        g_hwndIfaceLbl = mkLabel(L"Network Interface:", ID_IFACE_LABEL, SS_LEFT);

        g_combo = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                                  0, 0, 0, 0,
                                  hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_COMBO_IF)), g_hInst, nullptr);

        InitializeAdapterSelection();

        g_hwndDownTitle = mkLabel(L"Download Speed:", ID_DOWN_TITLE, SS_LEFT);
        g_hwndSpeedDown = mkLabel(g_szDownSpeed, ID_SPEED_DOWN, SS_RIGHT);

        g_hwndUpTitle = mkLabel(L"Upload Speed:", ID_UP_TITLE, SS_LEFT);
        g_hwndSpeedUp = mkLabel(g_szUpSpeed, ID_SPEED_UP, SS_RIGHT);

        g_hwndTotdTitle = mkLabel(L"Session downloaded:", ID_TOTD_TITLE, SS_LEFT);
        g_hwndTotalDown = mkLabel(L"0.00 MB", ID_TOTAL_DOWN, SS_RIGHT);

        g_hwndTotuTitle = mkLabel(L"Session uploaded:", ID_TOTU_TITLE, SS_LEFT);
        g_hwndTotalUp = mkLabel(L"0.00 MB", ID_TOTAL_UP, SS_RIGHT);

        g_hwndLifetimeTitle = mkLabel(L"Total data", ID_LIFETIME_TITLE, SS_LEFT);
        g_hwndLifetimeDown = mkLabel(L"Downloaded:", ID_LIFETIME_DOWN, SS_LEFT);
        g_hwndLifetimeDownValue = mkLabel(L"0 B", ID_LIFETIME_DOWN_VALUE, SS_RIGHT);
        g_hwndLifetimeUp = mkLabel(L"Uploaded:", ID_LIFETIME_UP, SS_LEFT);
        g_hwndLifetimeUpValue = mkLabel(L"0 B", ID_LIFETIME_UP_VALUE, SS_RIGHT);
        g_hwndLifetimeReset = CreateMainButton(hwnd, L"Reset total", ID_LIFETIME_RESET);

        g_hwndAuthor = mkLabel(L"WinNetMeter v" WINNETMETER_VERSION_STRING_W
                               L"\nSupport: github.com/pyed/WinNetMeter",
                               ID_AUTHOR_LINK, SS_RIGHT | SS_NOTIFY);
        CreateSettingsControls(hwnd);
        UpdateTotalValues();

        RefreshFontsAndRelayout(g_currentDpi);

        SetTimer(hwnd, ID_TIMER, 1000, nullptr);
        SetupTrayIcon();
        UpdateMeter();
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wp);
        int id = GetDlgCtrlID(reinterpret_cast<HWND>(lp));
        SetTextColor(hdc, GetSysColor(id == ID_AUTHOR_LINK ? COLOR_HOTLIGHT : COLOR_WINDOWTEXT));
        SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }
    case WM_COMMAND: {
        int code = HIWORD(wp);
        int id = LOWORD(wp);
        if (id == ID_COMBO_IF) {
            if (code == CBN_SELCHANGE) {
                OnComboSelectionChanged();
            } else if (code == CBN_DROPDOWN) {
                PopulateAdapters();
            }
        } else if (id == ID_AUTHOR_LINK) {
            ShellExecuteW(nullptr, L"open", L"https://github.com/pyed/WinNetMeter", nullptr, nullptr, SW_SHOWNORMAL);
        } else {
            HandleSettingsCommand(hwnd, id, code);
        }
        return 0;
    }
    case WM_TIMER:
        if (wp == ID_TIMER) {
            OnTimerTick();
        } else if (wp == ID_SETTINGS_SAVE_TIMER) {
            PersistSettingsNow();
        }
        return 0;
    case WM_OPEN_FROM_METER:
        ShowMainWindow();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE: {
        // Windows broadcasts "ImmersiveColorSet" when light/dark mode changes.
        if (msg == WM_SETTINGCHANGE && lp &&
            wcscmp(reinterpret_cast<const wchar_t*>(lp), L"ImmersiveColorSet") == 0) {
            g_taskbarLight = IsSystemThemeLight();
            UpdateTrayIcon();
        }
        UpdateMeter();
        return 0;
    }
    case WM_TRAYICON: {
        if (lp == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_SHOW, L"Open WinNetMeter");
            InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
            InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING | (g_settings.showWidget ? MF_CHECKED : MF_UNCHECKED),
                        ID_TRAY_TOGGLE_WIDGET, L"Show Taskbar Widget");
            InsertMenuW(hMenu, 3, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
            InsertMenuW(hMenu, 4, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit");

            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(hMenu);

            if (cmd == ID_TRAY_SHOW) {
                ShowMainWindow();
            } else if (cmd == ID_TRAY_TOGGLE_WIDGET) {
                g_settings.showWidget = !g_settings.showWidget;
                PersistSettingsNow();
                UpdateMeter();
                if (IsWindowVisible(hwnd)) RefreshSettingsControls();
            } else if (cmd == ID_TRAY_EXIT) {
                DestroyWindow(hwnd);
            }
        } else if (lp == WM_LBUTTONDBLCLK) {
            ShowMainWindow();
        }
        return 0;
    }
    case WM_DPICHANGED: {
        int newDpi = HIWORD(wp);
        RECT* prc = reinterpret_cast<RECT*>(lp);
        SetWindowPos(hwnd, nullptr, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        RefreshFontsAndRelayout(newDpi);
        UpdateMeter();
        return 0;
    }
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        // Don't leave a debounced edit unwritten while the window is away.
        if (g_settingsSavePending) PersistSettingsNow();
        return 0;
    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) {
            ShowWindow(hwnd, SW_HIDE);
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER);
        // Flush any debounced edit before the process goes away.
        PersistSettingsNow();
        RemoveTrayIcon();
        StopMeterHost();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// --integration-test runs an isolated instance: its own window class and mutex,
// a settings file that is never the real one (--settings <path>, else a fixed
// file under %TEMP%), and its own Run-key value name.
static bool ApplyCommandLine() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool isIntegrationTest = false;
    std::wstring settingsPath;
    for (int i = 1; argv && i < argc; ++i) {
        if (wcscmp(argv[i], L"--integration-test") == 0) {
            isIntegrationTest = true;
        } else if (wcscmp(argv[i], L"--settings") == 0 && i + 1 < argc) {
            settingsPath = argv[++i];
        }
    }
    if (argv) LocalFree(argv);

    if (isIntegrationTest) {
        if (settingsPath.empty()) {
            wchar_t temp[MAX_PATH] = {};
            DWORD length = GetTempPathW(_countof(temp), temp);
            settingsPath = std::wstring(length > 0 && length < _countof(temp) ? temp : L".\\") +
                           L"WinNetMeter-integration-test\\settings.ini";
        }
        SetSettingsPathOverride(settingsPath.c_str());
        SetStartupValueName(L"WinNetMeter.IntegrationTest");
    }
    return isIntegrationTest;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    bool isIntegrationTest = ApplyCommandLine();
    g_mainWindowClass = isIntegrationTest ? TEST_WINDOW_CLASS : MAIN_WINDOW_CLASS;
    HANDLE singleInstance = CreateMutexW(nullptr, TRUE,
                                         isIntegrationTest ? TEST_INSTANCE_MUTEX : SINGLE_INSTANCE_MUTEX);
    if (!singleInstance) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(singleInstance);
        HWND existing = FindWindowW(g_mainWindowClass, nullptr);
        if (existing) {
            ShowWindowAsync(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        return 0;
    }

    g_hInst = hInst;

    // DPI awareness (PerMonitorV2) comes from the embedded manifest. Under Common
    // Controls 6, InitCommonControls() is a no-op, so register explicitly.
    INITCOMMONCONTROLSEX controls = { sizeof(controls), ICC_STANDARD_CLASSES | ICC_UPDOWN_CLASS };
    InitCommonControlsEx(&controls);

    g_uTaskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    LoadSettings(&g_settings);
    g_taskbarLight = IsSystemThemeLight();
    g_lastTotalsSaveTick = GetTickCount64();

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = g_mainWindowClass;
    RegisterClassExW(&wc);

    HDC hdcScreen = GetDC(nullptr);
    g_currentDpi = GetDeviceCaps(hdcScreen, LOGPIXELSX);
    ReleaseDC(nullptr, hdcScreen);

    int w = ScaleDpi(730, g_currentDpi);
    int h = ScaleDpi(590, g_currentDpi);
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hwnd = CreateWindowExW(0, g_mainWindowClass, L"WinNetMeter",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                x, y, w, h, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) {
        ReleaseMutex(singleInstance);
        CloseHandle(singleInstance);
        return 1;
    }

    MSG msg = {};
    BOOL result = 0;
    while ((result = GetMessageW(&msg, nullptr, 0, 0)) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (result == -1 && IsWindow(hwnd)) DestroyWindow(hwnd);
    StopMeterHost();   // no-op after WM_DESTROY; covers a GetMessage failure

    // Cleanup resources
    if (g_fontLabel) DeleteObject(g_fontLabel);
    if (g_fontValue) DeleteObject(g_fontValue);
    if (g_fontAuthor) DeleteObject(g_fontAuthor);
    ReleaseMutex(singleInstance);
    CloseHandle(singleInstance);

    return result == -1 ? 1 : static_cast<int>(msg.wParam);
}
