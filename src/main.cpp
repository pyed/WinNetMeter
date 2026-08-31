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
#include "network.h"
#include "overlay.h"
#include "settings.h"
#include "version.h"

// Window message constants & IDs
enum {
    ID_TIMER = 1,
    WM_TRAYICON = WM_APP + 1,
    WM_OVERLAY_REFRESH = WM_APP + 2,
    WM_OVERLAY_ENSURE_TOPMOST = WM_APP + 3,
    WM_CHECK_FULLSCREEN = WM_APP + 4,

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
};

static const wchar_t MAIN_WINDOW_CLASS[] = L"WinNetMeterMain";
static const wchar_t TEST_WINDOW_CLASS[] = L"WinNetMeterMainTest";
static const wchar_t SINGLE_INSTANCE_MUTEX[] = L"Local\\WinNetMeter.SingleInstance";
static const wchar_t TEST_INSTANCE_MUTEX[] = L"Local\\WinNetMeter.IntegrationTest.SingleInstance";

// Global application state
static HINSTANCE g_hInst = nullptr;
static const wchar_t* g_mainWindowClass = MAIN_WINDOW_CLASS;
static HWND g_hwndMain = nullptr;
static HWND g_hwndOverlay = nullptr;
static HWINEVENTHOOK g_foregroundHook = nullptr;
static HWINEVENTHOOK g_locationHook = nullptr;

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
    HWND hwndLblDown = nullptr, hwndBtnDown = nullptr;
    HWND hwndLblUp = nullptr, hwndBtnUp = nullptr;
    HWND hwndLblFont = nullptr, hwndBtnFont = nullptr;
    HWND hwndLblOffset = nullptr, hwndEditOffset = nullptr, hwndSpinOffset = nullptr;
    HWND hwndLblOffsetUnit = nullptr, hwndBtnOffsetReset = nullptr;
    HWND hwndLblUnit = nullptr, hwndComboUnit = nullptr;
    HWND hwndLblDecimals = nullptr, hwndComboDecimals = nullptr;
    HWND hwndCheckWidget = nullptr, hwndCheckTray = nullptr, hwndCheckStartup = nullptr;
    HWND hwndBtnApply = nullptr, hwndBtnExit = nullptr;
    int dpi = 96;
    bool refreshing = false;
};

static SettingsUiState g_settingsUi;

// Fonts & Brushes
static HFONT g_fontLabel = nullptr;
static HFONT g_fontValue = nullptr;
static HFONT g_fontCombo = nullptr;
static HFONT g_fontAuthor = nullptr;
static HFONT g_fontOverlay = nullptr;

static HICON g_hCurrentTrayIcon = nullptr;

static UINT g_uTaskbarCreatedMsg = 0;
static NetSampler g_sampler;
static AppSettings g_settings;
static NET_LUID g_selectedLuid = {};
static wchar_t g_selectedAlias[128] = L"";
static NET_LUID g_comboLuids[64] = {};
static int g_comboLuidCount = 0;
static int g_currentDpi = 96;
static int g_overlayFontDpi = 0;

// Overlay speed strings
static wchar_t g_szDownSpeed[64] = L"";
static wchar_t g_szUpSpeed[64] = L"";
static wchar_t g_szDownCompact[32] = L"0B";
static wchar_t g_szUpCompact[32] = L"0B";
static ULONGLONG g_currentDownBps = 0;
static ULONGLONG g_currentUpBps = 0;
static ULONGLONG g_lastTotalsSaveTick = 0;
static bool g_totalsDirty = false;

// Forward declarations
static void ShowMainWindow();
static void CreateOrUpdateOverlay();
static void PositionTaskbarOverlay();
static void EnsureTaskbarOverlayTopmost();
static void UpdateTrayIcon();
static void SetupTrayIcon();
static void RemoveTrayIcon();
static void RefreshFontsAndRelayout(int dpi);
static void PopulateAdapters(bool isInitialStartup = false);
static HFONT CreateOverlayFontFromSettings(const AppSettings& s, int dpi);
static void RefreshSettingsControls();
static void UpdateTotalValues();

static int ScaleDpi(int val, int dpi) {
    return MulDiv(val, dpi, 96);
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

static HFONT CreateOverlayFontFromSettings(const AppSettings& s, int dpi) {
    return MakeFont(s.fontFamily, s.fontSize, s.fontStyle, dpi, ANTIALIASED_QUALITY);
}

static HFONT GetOverlayFont(int dpi) {
    if (!g_fontOverlay || g_overlayFontDpi != dpi) {
        if (g_fontOverlay) DeleteObject(g_fontOverlay);
        g_fontOverlay = CreateOverlayFontFromSettings(g_settings, dpi);
        g_overlayFontDpi = dpi;
    }
    return g_fontOverlay;
}

static void RelayoutMainControls(int dpi) {
    g_settingsUi.dpi = dpi;
    struct ItemPos {
        HWND hwnd;
        int x, y, w, h;
    } items[] = {
        { g_hwndStatusGroup,               12,  12, 325, 375 },
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
        { g_hwndAuthor,                     28, 405, 300,  38 },
        { g_settingsUi.hwndGroup,          350,  12, 365, 375 },
        { g_settingsUi.hwndLblUpPrefix,    370,  42, 125,  25 },
        { g_settingsUi.hwndEditUpPrefix,   500,  38, 125,  25 },
        { g_settingsUi.hwndLblDownPrefix,  370,  77, 125,  25 },
        { g_settingsUi.hwndEditDownPrefix, 500,  73, 125,  25 },
        { g_settingsUi.hwndLblUp,          370, 112, 125,  25 },
        { g_settingsUi.hwndBtnUp,          500, 108,  80,  25 },
        { g_settingsUi.hwndLblDown,        370, 147, 125,  25 },
        { g_settingsUi.hwndBtnDown,        500, 143,  80,  25 },
        { g_settingsUi.hwndLblFont,        370, 182, 125,  25 },
        { g_settingsUi.hwndBtnFont,        500, 178,  80,  25 },
        { g_settingsUi.hwndLblOffset,      370, 222, 125,  25 },
        { g_settingsUi.hwndEditOffset,     500, 218,  58,  25 },
        { g_settingsUi.hwndSpinOffset,     558, 218,  18,  25 },
        { g_settingsUi.hwndLblOffsetUnit,  580, 222,  25,  25 },
        { g_settingsUi.hwndBtnOffsetReset, 610, 218,  90,  25 },
        { g_settingsUi.hwndLblUnit,        370, 257, 125,  25 },
        { g_settingsUi.hwndComboUnit,      500, 253, 110, 120 },
        { g_settingsUi.hwndLblDecimals,    370, 292, 125,  25 },
        { g_settingsUi.hwndComboDecimals, 500, 288,  80, 120 },
        { g_settingsUi.hwndCheckWidget,    370, 327, 155,  22 },
        { g_settingsUi.hwndCheckTray,      535, 327, 170,  22 },
        { g_settingsUi.hwndCheckStartup,   370, 357, 170,  22 },
        { g_settingsUi.hwndBtnApply,       540, 405,  75,  28 },
        { g_settingsUi.hwndBtnExit,        625, 405,  75,  28 },
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
    HFONT oldFontCombo = g_fontCombo;
    HFONT oldFontAuthor = g_fontAuthor;
    HFONT oldFontOverlay = g_fontOverlay;

    g_currentDpi = dpi;
    g_fontLabel = MakeFont(L"Segoe UI", 9.0, 0, dpi);
    g_fontValue = MakeFont(L"Segoe UI", 10.0, 1, dpi);
    g_fontCombo = MakeFont(L"Segoe UI", 9.0, 0, dpi);
    g_fontAuthor = MakeFont(L"Segoe UI", 8.0, 2, dpi);
    g_fontOverlay = nullptr;
    g_overlayFontDpi = 0;

    if (g_hwndIfaceLbl)   SendMessageW(g_hwndIfaceLbl, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
    if (g_combo)          SendMessageW(g_combo, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontCombo), TRUE);
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
        g_settingsUi.hwndLblDown, g_settingsUi.hwndBtnDown,
        g_settingsUi.hwndLblUp, g_settingsUi.hwndBtnUp,
        g_settingsUi.hwndLblFont, g_settingsUi.hwndBtnFont,
        g_settingsUi.hwndLblOffset, g_settingsUi.hwndEditOffset,
        g_settingsUi.hwndSpinOffset, g_settingsUi.hwndLblOffsetUnit,
        g_settingsUi.hwndBtnOffsetReset, g_settingsUi.hwndLblUnit,
        g_settingsUi.hwndComboUnit, g_settingsUi.hwndLblDecimals,
        g_settingsUi.hwndComboDecimals, g_settingsUi.hwndCheckWidget,
        g_settingsUi.hwndCheckTray, g_settingsUi.hwndCheckStartup,
        g_settingsUi.hwndBtnApply, g_settingsUi.hwndBtnExit,
    };
    for (HWND control : settingsControls) {
        if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
    }

    RelayoutMainControls(dpi);

    if (oldFontLabel)   DeleteObject(oldFontLabel);
    if (oldFontValue)   DeleteObject(oldFontValue);
    if (oldFontCombo)   DeleteObject(oldFontCombo);
    if (oldFontAuthor)  DeleteObject(oldFontAuthor);
    if (oldFontOverlay) DeleteObject(oldFontOverlay);

}

static void UpdateSpeedValues(ULONGLONG downBps, ULONGLONG upBps) {
    g_currentDownBps = downBps;
    g_currentUpBps = upBps;
    FormatSpeed(downBps, g_settings.minimumSpeedUnit, g_settings.decimalPlaces,
                g_szDownSpeed, _countof(g_szDownSpeed));
    FormatSpeed(upBps, g_settings.minimumSpeedUnit, g_settings.decimalPlaces,
                g_szUpSpeed, _countof(g_szUpSpeed));
    FormatCompact(downBps, g_szDownCompact, _countof(g_szDownCompact));
    FormatCompact(upBps, g_szUpCompact, _countof(g_szUpCompact));

    if (g_hwndSpeedDown) SetWindowTextW(g_hwndSpeedDown, g_szDownSpeed);
    if (g_hwndSpeedUp) SetWindowTextW(g_hwndSpeedUp, g_szUpSpeed);
}

// ---- Tray Icon Generation ----------------------------------------------------
static HICON CreateSpeedTrayIcon(const wchar_t* downSpeed, const wchar_t* upSpeed) {
    const int w = 16;
    const int h = 16;

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hbmpColor = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    HBITMAP hbmpOld = static_cast<HBITMAP>(SelectObject(hdcMem, hbmpColor));

    // Fill background with dark gray
    RECT rc = { 0, 0, w, h };
    HBRUSH hbg = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdcMem, &rc, hbg);
    DeleteObject(hbg);

    // Render speed text with Arial 6pt Bold
    HFONT hFont = CreateFontW(-8, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    HFONT hOldFont = static_cast<HFONT>(SelectObject(hdcMem, hFont));

    SetBkMode(hdcMem, TRANSPARENT);

    // Download speed (top half) - configured color
    SetTextColor(hdcMem, g_settings.down);
    RECT rcDown = { 0, 0, w, h / 2 };
    DrawTextW(hdcMem, downSpeed, -1, &rcDown, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Upload speed (bottom half) - configured color
    SetTextColor(hdcMem, g_settings.up);
    RECT rcUp = { 0, h / 2, w, h };
    DrawTextW(hdcMem, upSpeed, -1, &rcUp, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdcMem, hOldFont);
    DeleteObject(hFont);

    // Initialize 1-bit monochrome mask (all 0s = fully opaque color bitmap)
    BYTE maskBits[16 * 2] = { 0 };
    HBITMAP hbmpMask = CreateBitmap(w, h, 1, 1, maskBits);

    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmMask = hbmpMask;
    ii.hbmColor = hbmpColor;

    HICON hIcon = CreateIconIndirect(&ii);

    SelectObject(hdcMem, hbmpOld);
    DeleteObject(hbmpColor);
    DeleteObject(hbmpMask);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    return hIcon;
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

// ---- Taskbar Overlay Widget --------------------------------------------------
static bool GetTaskbarPosition(RECT* rect, UINT* edge) {
    APPBARDATA data = {};
    data.cbSize = sizeof(data);
    if (!SHAppBarMessage(ABM_GETTASKBARPOS, &data) || IsRectEmpty(&data.rc)) {
        return false;
    }
    if (data.uEdge != ABE_LEFT && data.uEdge != ABE_TOP &&
        data.uEdge != ABE_RIGHT && data.uEdge != ABE_BOTTOM) {
        return false;
    }
    *rect = data.rc;
    *edge = data.uEdge;
    return true;
}

static bool IsTaskbarShown(const RECT& expected, UINT edge) {
    APPBARDATA state = {};
    state.cbSize = sizeof(state);
    if ((SHAppBarMessage(ABM_GETSTATE, &state) & ABS_AUTOHIDE) == 0) {
        return true;
    }

    APPBARDATA query = {};
    query.cbSize = sizeof(query);
    query.uEdge = edge;
    HWND taskbar = reinterpret_cast<HWND>(SHAppBarMessage(ABM_GETAUTOHIDEBAR, &query));
    RECT actual = {};
    RECT visible = {};
    if (!taskbar || !GetWindowRect(taskbar, &actual) || !IntersectRect(&visible, &actual, &expected)) {
        return false;
    }

    int expectedThickness = (edge == ABE_TOP || edge == ABE_BOTTOM)
        ? expected.bottom - expected.top
        : expected.right - expected.left;
    int visibleThickness = (edge == ABE_TOP || edge == ABE_BOTTOM)
        ? visible.bottom - visible.top
        : visible.right - visible.left;
    return visibleThickness * 2 >= expectedThickness;
}

// ---- Fullscreen Detection ----------------------------------------------------
static bool IsShellOrDesktopWindow(HWND hwnd) {
    if (!hwnd) return false;
    if (hwnd == GetDesktopWindow() || hwnd == GetShellWindow()) return true;

    wchar_t cls[64] = {};
    if (GetClassNameW(hwnd, cls, _countof(cls)) > 0) {
        if (wcscmp(cls, L"Progman") == 0 ||
            wcscmp(cls, L"WorkerW") == 0 ||
            wcscmp(cls, L"Shell_TrayWnd") == 0 ||
            wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0) {
            return true;
        }
    }
    return false;
}

static bool GetVisibleWindowBounds(HWND hwnd, RECT* out) {
    // DwmGetWindowAttribute(DWMWA_EXTENDED_FRAME_BOUNDS) returns the actual visible
    // bounds, excluding invisible resize borders that GetWindowRect includes on Win10/11.
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, out, sizeof(*out)))) {
        return true;
    }
    return GetWindowRect(hwnd, out) != FALSE;
}

static bool IsForegroundFullscreenOnMonitor(HMONITOR targetMonitor) {
    if (!targetMonitor) return false;

    HWND fg = GetForegroundWindow();
    if (!fg) return false;

    // A hidden or minimized window is never fullscreen application content
    if (!IsWindowVisible(fg)) return false;
    if (IsIconic(fg) || (GetWindowLongPtrW(fg, GWL_STYLE) & WS_MINIMIZE) != 0) return false;

    // Normalize to root window to avoid classifying child controls/tooltips/menus
    HWND root = GetAncestor(fg, GA_ROOT);
    if (root) fg = root;

    // Don't classify our own windows or shell/desktop surfaces as fullscreen
    if (fg == g_hwndOverlay || fg == g_hwndMain) return false;
    if (IsShellOrDesktopWindow(fg)) return false;

    // Check which monitor the foreground window is on
    HMONITOR fgMonitor = MonitorFromWindow(fg, MONITOR_DEFAULTTONULL);
    if (!fgMonitor || fgMonitor != targetMonitor) return false;

    MONITORINFO mi = { sizeof(mi) };
    if (!GetMonitorInfoW(fgMonitor, &mi)) return false;

    RECT wndRect = {};
    if (!GetVisibleWindowBounds(fg, &wndRect)) return false;

    if (!IsWindowRectFullscreen(wndRect, mi.rcMonitor)) return false;

    // Normal maximized window under taskbar auto-hide:
    // Has WS_CAPTION and is zoomed (maximized), occupying rcWork.
    // Genuine fullscreen apps (F11, games, video) remove WS_CAPTION or use WS_POPUP.
    LONG_PTR style = GetWindowLongPtrW(fg, GWL_STYLE);
    if ((style & WS_CAPTION) == WS_CAPTION && (style & WS_MAXIMIZE) != 0) {
        return false;
    }

    return true;
}

static bool ShouldShowTaskbarMeter(RECT* outTaskbar = nullptr, UINT* outEdge = nullptr) {
    if (!g_settings.showWidget) return false;

    RECT taskbar = {};
    UINT edge = ABE_BOTTOM;
    if (!GetTaskbarPosition(&taskbar, &edge)) return false;
    if (!IsTaskbarShown(taskbar, edge)) return false;

    HMONITOR taskbarMon = MonitorFromRect(&taskbar, MONITOR_DEFAULTTOPRIMARY);
    if (IsForegroundFullscreenOnMonitor(taskbarMon)) return false;

    if (outTaskbar) *outTaskbar = taskbar;
    if (outEdge) *outEdge = edge;
    return true;
}

static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        PositionTaskbarOverlay();
        return 0;
    }
    case WM_DPICHANGED:
        PostMessageW(hwnd, WM_OVERLAY_REFRESH, 0, 0);
        return 0;
    case WM_OVERLAY_REFRESH:
        PositionTaskbarOverlay();
        return 0;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_LBUTTONDBLCLK:
        ShowMainWindow();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void PositionTaskbarOverlay() {
    if (!g_hwndOverlay) return;

    RECT taskbar = {};
    UINT edge = ABE_BOTTOM;
    if (!ShouldShowTaskbarMeter(&taskbar, &edge)) {
        if (IsWindowVisible(g_hwndOverlay)) {
            ShowWindow(g_hwndOverlay, SW_HIDE);
        }
        return;
    }

    int dpi = static_cast<int>(GetDpiForWindow(g_hwndOverlay));
    if (dpi == 0) dpi = g_currentDpi;
    RECT target = CalculateTaskbarOverlayRect(taskbar, edge, static_cast<UINT>(dpi),
                                              g_settings.taskbarOffset);
    int width = target.right - target.left;
    int height = target.bottom - target.top;
    int stride = width * 4;

    HDC screen = GetDC(nullptr);
    if (!screen) return;
    HDC memory = CreateCompatibleDC(screen);
    if (!memory) {
        ReleaseDC(nullptr, screen);
        return;
    }

    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(screen, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) {
        if (bitmap) DeleteObject(bitmap);
        DeleteDC(memory);
        ReleaseDC(nullptr, screen);
        return;
    }

    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memory, bitmap));
    memset(bits, 0, static_cast<size_t>(stride) * static_cast<size_t>(height));
    HFONT font = GetOverlayFont(dpi);
    HFONT oldFont = font ? static_cast<HFONT>(SelectObject(memory, font)) : nullptr;
    SetBkMode(memory, TRANSPARENT);
    SetTextColor(memory, RGB(255, 255, 255));

    const int middle = height / 2;
    int padding = ScaleDpi(4, dpi);
    if (padding * 2 >= width) padding = 0;
    wchar_t upText[128] = {};
    wchar_t downText[128] = {};
    FormatPrefixedSpeed(g_settings.upPrefix, g_szUpSpeed, upText, _countof(upText));
    FormatPrefixedSpeed(g_settings.downPrefix, g_szDownSpeed, downText, _countof(downText));
    RECT upRect = { padding, 0, width - padding, middle };
    RECT downRect = { padding, middle, width - padding, height };
    const UINT textFlags = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
    DrawTextW(memory, upText, -1, &upRect, textFlags);
    DrawTextW(memory, downText, -1, &downRect, textFlags);
    GdiFlush();
    ApplyOverlayAlpha(static_cast<BYTE*>(bits), width, height, stride, middle,
                      g_settings.up, g_settings.down);

    POINT destination = { target.left, target.top };
    POINT source = { 0, 0 };
    SIZE size = { width, height };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    BOOL updated = UpdateLayeredWindow(g_hwndOverlay, screen, &destination, &size,
                                       memory, &source, 0, &blend, ULW_ALPHA);

    if (oldFont) SelectObject(memory, oldFont);
    SelectObject(memory, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);

    if (updated) {
        SetWindowPos(g_hwndOverlay, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}

static void EnsureTaskbarOverlayTopmost() {
    if (!g_hwndOverlay || !IsWindowVisible(g_hwndOverlay)) return;
    if (!ShouldShowTaskbarMeter(nullptr, nullptr)) {
        ShowWindow(g_hwndOverlay, SW_HIDE);
        return;
    }

    SetWindowPos(g_hwndOverlay, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

static void CALLBACK OnForegroundChanged(HWINEVENTHOOK, DWORD event, HWND hwnd,
                                         LONG, LONG, DWORD, DWORD) {
    if (event == EVENT_SYSTEM_FOREGROUND && hwnd && g_hwndMain && hwnd != g_hwndOverlay) {
        PostMessageW(g_hwndMain, WM_CHECK_FULLSCREEN, 0, 0);
        PostMessageW(g_hwndMain, WM_OVERLAY_ENSURE_TOPMOST, 0, 0);
    }
}

static void CALLBACK OnLocationChanged(HWINEVENTHOOK, DWORD event, HWND hwnd,
                                       LONG idObject, LONG idChild, DWORD, DWORD) {
    // Filter to top-level window moves only (not child controls, not caret, etc.)
    if (event != EVENT_OBJECT_LOCATIONCHANGE) return;
    if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) return;
    if (!hwnd || !g_hwndMain || hwnd == g_hwndOverlay || hwnd == g_hwndMain) return;

    // Only care about the foreground window's geometry changes
    HWND fg = GetForegroundWindow();
    if (!fg) return;
    HWND root = GetAncestor(fg, GA_ROOT);
    if (root) fg = root;
    HWND hwndRoot = GetAncestor(hwnd, GA_ROOT);
    if (hwndRoot) hwnd = hwndRoot;
    if (hwnd != fg) return;

    PostMessageW(g_hwndMain, WM_CHECK_FULLSCREEN, 0, 0);
}

static void CreateOrUpdateOverlay() {
    if (!g_settings.showWidget) {
        if (g_hwndOverlay) {
            DestroyWindow(g_hwndOverlay);
            g_hwndOverlay = nullptr;
        }
        return;
    }

    if (!g_hwndOverlay) {
        RECT taskbar = {};
        UINT edge = ABE_BOTTOM;
        if (!GetTaskbarPosition(&taskbar, &edge)) return;

        wchar_t cls[] = L"WinNetMeterOverlay";
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = OverlayWndProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = cls;
        RegisterClassExW(&wc);

        g_hwndOverlay = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
            cls, nullptr, WS_POPUP,
            taskbar.left, taskbar.top, 1, 1,
            nullptr, nullptr, g_hInst, nullptr);

        if (g_hwndOverlay) {
            PositionTaskbarOverlay();
        }
    } else {
        PositionTaskbarOverlay();
    }
}

static ULONGLONG g_lastFailTick = 0;
static void OnTimerTick() {
    if (g_selectedLuid.Value == 0) {
        // Disconnected / waiting for reconnect: rate-limited refresh at most every 3s
        ULONGLONG now = GetTickCount64();
        if (now - g_lastFailTick >= 3000) {
            g_lastFailTick = now;
            PopulateAdapters(false);
        }
    }

    if (g_selectedLuid.Value == 0) {
        UpdateSpeedValues(0, 0);
        UpdateTrayIcon();
        CreateOrUpdateOverlay();
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
            SaveSettings(&g_settings);
            g_lastTotalsSaveTick = now;
            g_totalsDirty = false;
        }
    } else {
        // Sampling failed (adapter disconnected/disabled): rate-limited refresh at most every 3s
        ULONGLONG now = GetTickCount64();
        if (now - g_lastFailTick >= 3000) {
            g_lastFailTick = now;
            PopulateAdapters(false);
        }

        UpdateSpeedValues(0, 0);
    }

    UpdateTrayIcon();
    CreateOrUpdateOverlay();
}

static void OnComboSelectionChanged() {
    int sel = static_cast<int>(SendMessageW(g_combo, CB_GETCURSEL, 0, 0));
    if (sel >= 0 && sel < g_comboLuidCount) {
        NET_LUID luid = g_comboLuids[sel];
        SendMessageW(g_combo, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(g_selectedAlias));
        if (luid.Value != g_selectedLuid.Value) {
            g_selectedLuid = luid;
            g_sampler.Reset(luid);
            UpdateSpeedValues(0, 0);
            UpdateTotalValues();
            UpdateTrayIcon();
            CreateOrUpdateOverlay();
        }
    }
}

static void PopulateAdapters(bool isInitialStartup) {
    SendMessageW(g_combo, CB_RESETCONTENT, 0, 0);

    AdapterInfo list[64];
    int count = GetAdapters(list, 64);
    g_comboLuidCount = 0;

    int exactLuidIdx = -1;
    int aliasMatchIdx = -1;
    int aliasMatchCount = 0;
    int initialUpIdx = -1;

    for (int i = 0; i < count; ++i) {
        int item = static_cast<int>(SendMessageW(g_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(list[i].name)));
        if (item >= 0 && item < 64) {
            g_comboLuids[item] = list[i].luid;
            g_comboLuidCount = max(g_comboLuidCount, item + 1);

            if (isInitialStartup) {
                if (list[i].status == IfOperStatusUp && initialUpIdx < 0) {
                    initialUpIdx = item;
                }
            } else {
                if (list[i].luid.Value == g_selectedLuid.Value && g_selectedLuid.Value != 0) {
                    exactLuidIdx = item;
                }
                if (g_selectedAlias[0] != L'\0' && wcscmp(list[i].name, g_selectedAlias) == 0) {
                    aliasMatchIdx = item;
                    ++aliasMatchCount;
                }
            }
        }
    }

    if (isInitialStartup) {
        int chosen = (initialUpIdx >= 0) ? initialUpIdx : ((count > 0) ? 0 : -1);
        if (chosen >= 0) {
            SendMessageW(g_combo, CB_SETCURSEL, chosen, 0);
            g_selectedLuid = g_comboLuids[chosen];
            SendMessageW(g_combo, CB_GETLBTEXT, chosen, reinterpret_cast<LPARAM>(g_selectedAlias));
            g_sampler.Reset(g_selectedLuid);
        }
    } else {
        if (exactLuidIdx >= 0) {
            SendMessageW(g_combo, CB_SETCURSEL, exactLuidIdx, 0);
        } else if (aliasMatchCount == 1 && aliasMatchIdx >= 0) {
            // Unambiguous reconnect on new LUID: rebind & rebaseline sampler without losing totals
            SendMessageW(g_combo, CB_SETCURSEL, aliasMatchIdx, 0);
            g_selectedLuid = g_comboLuids[aliasMatchIdx];
            g_sampler.Rebind(g_selectedLuid);
        } else {
            // Disappeared or ambiguous match: do NOT fall back to an unrelated adapter!
            // Keep g_selectedAlias intact for future recovery, set LUID to 0 (disconnected).
            SendMessageW(g_combo, CB_SETCURSEL, static_cast<WPARAM>(-1), 0);
            g_selectedLuid.Value = 0;
        }
    }
}

static void ShowMainWindow() {
    if (g_hwndMain) {
        if (!IsWindowVisible(g_hwndMain)) RefreshSettingsControls();
        ShowWindow(g_hwndMain, IsIconic(g_hwndMain) ? SW_RESTORE : SW_SHOW);
        SetForegroundWindow(g_hwndMain);
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

    wchar_t date[16] = {};
    wchar_t title[64] = L"Total data";
    if (FormatLifetimeSinceDate(g_settings.lifetimeSince, date, _countof(date))) {
        swprintf_s(title, L"Total data since %s", date);
    }
    if (g_hwndLifetimeTitle) SetWindowTextW(g_hwndLifetimeTitle, title);
}

static COLORREF PickColor(HWND hwndOwner, COLORREF initColor) {
    static COLORREF customColors[16] = {};
    CHOOSECOLORW cc = {};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hwndOwner;
    cc.rgbResult = initColor;
    cc.lpCustColors = customColors;
    cc.Flags = CC_RGBINIT | CC_FULLOPEN;
    if (ChooseColorW(&cc)) {
        return cc.rgbResult;
    }
    return initColor;
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
    state.hwndLblDown = CreateMainLabel(hwnd, L"Download color:", ID_SET_DOWN_LBL);
    state.hwndBtnDown = CreateMainButton(hwnd, L"Select", ID_SET_DOWN_BTN);
    state.hwndLblFont = CreateMainLabel(hwnd, L"Taskbar meter font:", ID_SET_FONT_LBL);
    state.hwndBtnFont = CreateMainButton(hwnd, L"Choose", ID_SET_FONT_BTN);
    state.hwndLblOffset = CreateMainLabel(hwnd, L"Taskbar meter offset:", ID_SET_OFFSET_LBL);
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

    state.hwndLblUnit = CreateMainLabel(hwnd, L"Minimum speed unit:", ID_SET_UNIT_LBL);
    state.hwndComboUnit = CreateWindowExW(
        0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SET_UNIT_COMBO)), g_hInst, nullptr);
    const wchar_t* unitChoices[] = { L"Auto", L"KB/s", L"MB/s", L"GB/s" };
    for (const wchar_t* choice : unitChoices) {
        SendMessageW(state.hwndComboUnit, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choice));
    }

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
    SendMessageW(state.hwndComboUnit, CB_SETCURSEL, static_cast<WPARAM>(g_settings.minimumSpeedUnit), 0);
    SendMessageW(state.hwndComboDecimals, CB_SETCURSEL, g_settings.decimalPlaces, 0);
    SendMessageW(state.hwndCheckWidget, BM_SETCHECK, g_settings.showWidget ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(state.hwndCheckTray, BM_SETCHECK, g_settings.showTrayIcon ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(state.hwndCheckStartup, BM_SETCHECK, g_settings.startWithWindows ? BST_CHECKED : BST_UNCHECKED, 0);
    state.refreshing = false;
}

static void ApplyLiveMeterSettings(bool fontChanged = false) {
    SettingsUiState& state = g_settingsUi;
    g_settings.down = state.tempSettings.down;
    g_settings.up = state.tempSettings.up;
    wcscpy_s(g_settings.downPrefix, _countof(g_settings.downPrefix), state.tempSettings.downPrefix);
    wcscpy_s(g_settings.upPrefix, _countof(g_settings.upPrefix), state.tempSettings.upPrefix);
    wcscpy_s(g_settings.fontFamily, _countof(g_settings.fontFamily), state.tempSettings.fontFamily);
    g_settings.fontSize = state.tempSettings.fontSize;
    g_settings.fontStyle = state.tempSettings.fontStyle;
    g_settings.taskbarOffset = state.tempSettings.taskbarOffset;
    g_settings.minimumSpeedUnit = state.tempSettings.minimumSpeedUnit;
    g_settings.decimalPlaces = state.tempSettings.decimalPlaces;
    SaveSettings(&g_settings);
    UpdateSpeedValues(g_currentDownBps, g_currentUpBps);
    if (fontChanged) RefreshFontsAndRelayout(g_currentDpi);
    UpdateTrayIcon();
    CreateOrUpdateOverlay();
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
    int unit = static_cast<int>(SendMessageW(state.hwndComboUnit, CB_GETCURSEL, 0, 0));
    int decimals = static_cast<int>(SendMessageW(state.hwndComboDecimals, CB_GETCURSEL, 0, 0));
    if (unit >= 0 && unit <= 3) state.tempSettings.minimumSpeedUnit = static_cast<MinimumSpeedUnit>(unit);
    if (decimals >= SPEED_DECIMAL_PLACES_MIN && decimals <= SPEED_DECIMAL_PLACES_MAX) {
        state.tempSettings.decimalPlaces = decimals;
    }
    state.tempSettings.showWidget = SendMessageW(state.hwndCheckWidget, BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.tempSettings.showTrayIcon = SendMessageW(state.hwndCheckTray, BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.tempSettings.startWithWindows = SendMessageW(state.hwndCheckStartup, BM_GETCHECK, 0, 0) == BST_CHECKED;
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

    state.tempSettings.lifetimeDownloaded = g_settings.lifetimeDownloaded;
    state.tempSettings.lifetimeUploaded = g_settings.lifetimeUploaded;
    wcscpy_s(state.tempSettings.lifetimeSince, _countof(state.tempSettings.lifetimeSince),
             g_settings.lifetimeSince);
    g_settings = state.tempSettings;
    SaveSettings(&g_settings);
    UpdateSpeedValues(g_currentDownBps, g_currentUpBps);
    RefreshFontsAndRelayout(g_currentDpi);
    SetupTrayIcon();
    CreateOrUpdateOverlay();
    InvalidateRect(g_hwndMain, nullptr, TRUE);
    RefreshSettingsControls();
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
        ApplyLiveMeterSettings();
    } else if (id == ID_SET_OFFSET_EDIT && code == EN_CHANGE) {
        if (state.refreshing) return true;
        wchar_t text[32] = {};
        GetWindowTextW(state.hwndEditOffset, text, _countof(text));
        int offset = 0;
        if (ParseTaskbarMeterOffset(text, &offset)) {
            state.tempSettings.taskbarOffset = offset;
            ApplyLiveMeterSettings();
        }
    } else if (id == ID_SET_OFFSET_RESET) {
        AppSettings defaults;
        state.tempSettings.down = defaults.down;
        state.tempSettings.up = defaults.up;
        wcscpy_s(state.tempSettings.downPrefix, _countof(state.tempSettings.downPrefix), defaults.downPrefix);
        wcscpy_s(state.tempSettings.upPrefix, _countof(state.tempSettings.upPrefix), defaults.upPrefix);
        wcscpy_s(state.tempSettings.fontFamily, _countof(state.tempSettings.fontFamily), defaults.fontFamily);
        state.tempSettings.fontSize = defaults.fontSize;
        state.tempSettings.fontStyle = defaults.fontStyle;
        state.tempSettings.taskbarOffset = defaults.taskbarOffset;
        state.tempSettings.minimumSpeedUnit = defaults.minimumSpeedUnit;
        state.tempSettings.decimalPlaces = defaults.decimalPlaces;
        ApplyLiveMeterSettings(true);
        RefreshSettingsControls();
    } else if (id == ID_SET_UNIT_COMBO && code == CBN_SELCHANGE) {
        int selection = static_cast<int>(SendMessageW(state.hwndComboUnit, CB_GETCURSEL, 0, 0));
        if (selection >= 0 && selection <= 3) {
            state.tempSettings.minimumSpeedUnit = static_cast<MinimumSpeedUnit>(selection);
            ApplyLiveMeterSettings();
        }
    } else if (id == ID_SET_DECIMALS_COMBO && code == CBN_SELCHANGE) {
        int selection = static_cast<int>(SendMessageW(state.hwndComboDecimals, CB_GETCURSEL, 0, 0));
        if (selection >= SPEED_DECIMAL_PLACES_MIN && selection <= SPEED_DECIMAL_PLACES_MAX) {
            state.tempSettings.decimalPlaces = selection;
            ApplyLiveMeterSettings();
        }
    } else if (id == ID_SET_DOWN_BTN) {
        state.tempSettings.down = PickColor(hwnd, state.tempSettings.down);
        ApplyLiveMeterSettings();
    } else if (id == ID_SET_UP_BTN) {
        state.tempSettings.up = PickColor(hwnd, state.tempSettings.up);
        ApplyLiveMeterSettings();
    } else if (id == ID_SET_FONT_BTN) {
        PickFont(hwnd, &state.tempSettings, state.dpi);
        ApplyLiveMeterSettings(true);
    } else if (id == ID_SET_SAVE_BTN) {
        ApplySettings(hwnd);
    } else if (id == ID_LIFETIME_RESET) {
        if (MessageBoxW(hwnd,
                        L"Reset the saved download and upload totals?\n\nThis cannot be undone.",
                        L"Reset total data", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES) {
            ResetLifetimeTotals(&g_settings);
            SaveSettings(&g_settings);
            g_lastTotalsSaveTick = GetTickCount64();
            g_totalsDirty = false;
            UpdateTotalValues();
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
        CreateOrUpdateOverlay();
        return 0;
    }

    switch (msg) {
    case WM_CREATE: {
        g_hwndMain = hwnd;
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

        PopulateAdapters(true);

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
        CreateOrUpdateOverlay();
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
        }
        return 0;
    case WM_OVERLAY_ENSURE_TOPMOST:
        EnsureTaskbarOverlayTopmost();
        return 0;
    case WM_CHECK_FULLSCREEN:
        PositionTaskbarOverlay();
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
        CreateOrUpdateOverlay();
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
                SaveSettings(&g_settings);
                CreateOrUpdateOverlay();
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
        CreateOrUpdateOverlay();
        return 0;
    }
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) {
            ShowWindow(hwnd, SW_HIDE);
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER);
        SaveSettings(&g_settings);
        RemoveTrayIcon();
        if (g_hwndOverlay) {
            DestroyWindow(g_hwndOverlay);
            g_hwndOverlay = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR commandLine, int) {
    bool isIntegrationTest = wcsstr(commandLine, L"--integration-test") != nullptr;
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

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    InitCommonControls();

    g_uTaskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    LoadSettings(&g_settings);
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
    int h = ScaleDpi(490, g_currentDpi);
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

    g_foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                       nullptr, OnForegroundChanged, 0, 0,
                                       WINEVENT_OUTOFCONTEXT);
    g_locationHook = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE,
                                     nullptr, OnLocationChanged, 0, 0,
                                     WINEVENT_OUTOFCONTEXT);

    MSG msg = {};
    BOOL result = 0;
    while ((result = GetMessageW(&msg, nullptr, 0, 0)) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (result == -1 && IsWindow(hwnd)) DestroyWindow(hwnd);

    if (g_foregroundHook) {
        UnhookWinEvent(g_foregroundHook);
        g_foregroundHook = nullptr;
    }
    if (g_locationHook) {
        UnhookWinEvent(g_locationHook);
        g_locationHook = nullptr;
    }

    // Cleanup resources
    if (g_fontLabel) DeleteObject(g_fontLabel);
    if (g_fontValue) DeleteObject(g_fontValue);
    if (g_fontCombo) DeleteObject(g_fontCombo);
    if (g_fontAuthor) DeleteObject(g_fontAuthor);
    if (g_fontOverlay) DeleteObject(g_fontOverlay);
    ReleaseMutex(singleInstance);
    CloseHandle(singleInstance);

    return result == -1 ? 1 : static_cast<int>(msg.wParam);
}
