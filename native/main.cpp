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
#include <stdio.h>
#include <stdlib.h>
#include <cwchar>
#include "network.h"
#include "overlay.h"
#include "settings.h"

// Window message constants & IDs
enum {
    ID_TIMER = 1,
    WM_TRAYICON = WM_APP + 1,
    WM_OVERLAY_REFRESH = WM_APP + 2,
    WM_OVERLAY_ENSURE_TOPMOST = WM_APP + 3,

    // Main window control IDs
    ID_COMBO_IF = 101,
    ID_SPEED_DOWN = 102,
    ID_SPEED_UP = 103,
    ID_TOTAL_DOWN = 104,
    ID_TOTAL_UP = 105,
    ID_AUTHOR_LINK = 106,
    ID_IFACE_LABEL = 201,
    ID_DOWN_TITLE = 202,
    ID_UP_TITLE = 203,
    ID_TOTD_TITLE = 204,
    ID_TOTU_TITLE = 205,

    // Tray menu command IDs
    ID_TRAY_SHOW = 1001,
    ID_TRAY_SETTINGS = 1002,
    ID_TRAY_TOGGLE_WIDGET = 1003,
    ID_TRAY_EXIT = 1004,

    // Settings dialog control IDs
    ID_SET_DOWN_BTN = 2002,
    ID_SET_UP_BTN = 2003,
    ID_SET_FONT_BTN = 2004,
    ID_SET_SAVE_BTN = 2005,
    ID_SET_CANCEL_BTN = 2006,
    ID_SET_PREVIEW = 2007,
    ID_SET_DOWN_LBL = 2009,
    ID_SET_UP_LBL = 2010,
    ID_SET_FONT_LBL = 2011,
    ID_SET_OFFSET_LBL = 2012,
    ID_SET_OFFSET_EDIT = 2013,
    ID_SET_OFFSET_SPIN = 2014,
    ID_SET_OFFSET_UNIT = 2015,
    ID_SET_OFFSET_RESET = 2016,
};

// Colors
static const COLORREF CLR_BG = RGB(20, 20, 20);
static const COLORREF CLR_DOWN = RGB(0, 200, 83);     // #00C853 (Green)
static const COLORREF CLR_UP = RGB(255, 185, 0);      // #FFB900 (Orange)
static const COLORREF CLR_LABEL = RGB(211, 211, 211); // LightGray
static const COLORREF CLR_WHITE = RGB(255, 255, 255);
static const COLORREF CLR_GRAY = RGB(128, 128, 128);
static const wchar_t MAIN_WINDOW_CLASS[] = L"WinNetMeterMain";
static const wchar_t SINGLE_INSTANCE_MUTEX[] = L"Local\\WinNetMeter.SingleInstance";

// Global application state
static HINSTANCE g_hInst = nullptr;
static HWND g_hwndMain = nullptr;
static HWND g_hwndOverlay = nullptr;
static HWINEVENTHOOK g_foregroundHook = nullptr;

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
static HWND g_hwndAuthor = nullptr;

// Fonts & Brushes
static HFONT g_fontLabel = nullptr;
static HFONT g_fontValue = nullptr;
static HFONT g_fontCombo = nullptr;
static HFONT g_fontAuthor = nullptr;
static HFONT g_fontOverlay = nullptr;

static HBRUSH g_brushBg = nullptr;
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
static wchar_t g_szDownSpeed[64] = L"0.00 KB/s";
static wchar_t g_szUpSpeed[64] = L"0.00 KB/s";
static wchar_t g_szDownCompact[32] = L"0B";
static wchar_t g_szUpCompact[32] = L"0B";

// Forward declarations
static void ShowMainWindow();
static void OpenSettingsDialog();
static void CreateOrUpdateOverlay();
static void PositionTaskbarOverlay();
static void EnsureTaskbarOverlayTopmost();
static void UpdateTrayIcon();
static void SetupTrayIcon();
static void RemoveTrayIcon();
static void RefreshFontsAndRelayout(int dpi);
static void PopulateAdapters(bool isInitialStartup = false);
static HFONT CreateOverlayFontFromSettings(const AppSettings& s, int dpi);

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
    struct ItemPos {
        HWND hwnd;
        int x, y, w, h;
    } items[] = {
        { g_hwndIfaceLbl,   20,  20, 120,  20 },
        { g_combo,         150,  18, 260, 250 },
        { g_hwndDownTitle,  20,  70, 150,  25 },
        { g_hwndSpeedDown, 180,  70, 230,  25 },
        { g_hwndUpTitle,    20, 105, 150,  25 },
        { g_hwndSpeedUp,   180, 105, 230,  25 },
        { g_hwndTotdTitle,  20, 165, 150,  25 },
        { g_hwndTotalDown, 180, 165, 230,  25 },
        { g_hwndTotuTitle,  20, 195, 150,  25 },
        { g_hwndTotalUp,   180, 195, 230,  25 },
        { g_hwndAuthor,     20, 228, 390,  32 },
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
    if (g_hwndAuthor)     SendMessageW(g_hwndAuthor, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontAuthor), TRUE);

    RelayoutMainControls(dpi);

    if (oldFontLabel)   DeleteObject(oldFontLabel);
    if (oldFontValue)   DeleteObject(oldFontValue);
    if (oldFontCombo)   DeleteObject(oldFontCombo);
    if (oldFontAuthor)  DeleteObject(oldFontAuthor);
    if (oldFontOverlay) DeleteObject(oldFontOverlay);

    if (g_brushBg) DeleteObject(g_brushBg);
    g_brushBg = CreateSolidBrush(CLR_BG);
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

    // Download speed (top half) - green
    SetTextColor(hdcMem, g_settings.down);
    RECT rcDown = { 0, 0, w, h / 2 };
    DrawTextW(hdcMem, downSpeed, -1, &rcDown, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Upload speed (bottom half) - orange
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

static void UpdateTrayIcon() {
    HICON hNewIcon = CreateSpeedTrayIcon(g_szDownCompact, g_szUpCompact);

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwndMain;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hNewIcon;
    swprintf_s(nid.szTip, _countof(nid.szTip), L"WinNetMeter\n\u2193 %s\n\u2191 %s", g_szDownSpeed, g_szUpSpeed);

    Shell_NotifyIconW(NIM_MODIFY, &nid);

    if (g_hCurrentTrayIcon) {
        DestroyIcon(g_hCurrentTrayIcon);
    }
    g_hCurrentTrayIcon = hNewIcon;
}

static void SetupTrayIcon() {
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
    swprintf_s(nid.szTip, _countof(nid.szTip), L"WinNetMeter\n\u2193 %s\n\u2191 %s", g_szDownSpeed, g_szUpSpeed);

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
    if (!g_settings.showWidget) {
        ShowWindow(g_hwndOverlay, SW_HIDE);
        return;
    }

    RECT taskbar = {};
    UINT edge = ABE_BOTTOM;
    if (!GetTaskbarPosition(&taskbar, &edge)) {
        ShowWindow(g_hwndOverlay, SW_HIDE);
        return;
    }
    if (!IsTaskbarShown(taskbar, edge)) {
        ShowWindow(g_hwndOverlay, SW_HIDE);
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
    swprintf_s(upText, L"\u2191  %s", g_szUpSpeed);
    swprintf_s(downText, L"\u2193  %s", g_szDownSpeed);
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
    if (!g_hwndOverlay || !g_settings.showWidget || !IsWindowVisible(g_hwndOverlay)) return;

    RECT taskbar = {};
    UINT edge = ABE_BOTTOM;
    if (!GetTaskbarPosition(&taskbar, &edge) || !IsTaskbarShown(taskbar, edge)) return;

    SetWindowPos(g_hwndOverlay, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

static void CALLBACK OnForegroundChanged(HWINEVENTHOOK, DWORD event, HWND hwnd,
                                         LONG, LONG, DWORD, DWORD) {
    if (event == EVENT_SYSTEM_FOREGROUND && hwnd && g_hwndMain && hwnd != g_hwndOverlay) {
        PostMessageW(g_hwndMain, WM_OVERLAY_ENSURE_TOPMOST, 0, 0);
    }
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
        wcscpy_s(g_szDownSpeed, L"0.00 KB/s");
        wcscpy_s(g_szUpSpeed, L"0.00 KB/s");
        wcscpy_s(g_szDownCompact, L"0B");
        wcscpy_s(g_szUpCompact, L"0B");
        if (g_hwndSpeedDown) SetWindowTextW(g_hwndSpeedDown, L"0.00 KB/s");
        if (g_hwndSpeedUp) SetWindowTextW(g_hwndSpeedUp, L"0.00 KB/s");
        UpdateTrayIcon();
        CreateOrUpdateOverlay();
        return;
    }

    ULONGLONG downBps = 0, upBps = 0;
    if (g_sampler.Sample(g_selectedLuid, &downBps, &upBps)) {
        FormatSpeed(downBps, g_szDownSpeed, _countof(g_szDownSpeed));
        FormatSpeed(upBps, g_szUpSpeed, _countof(g_szUpSpeed));
        FormatCompact(downBps, g_szDownCompact, _countof(g_szDownCompact));
        FormatCompact(upBps, g_szUpCompact, _countof(g_szUpCompact));

        if (g_hwndSpeedDown) SetWindowTextW(g_hwndSpeedDown, g_szDownSpeed);
        if (g_hwndSpeedUp) SetWindowTextW(g_hwndSpeedUp, g_szUpSpeed);

        wchar_t totalBuf[64];
        FormatBytes(g_sampler.totalIn, totalBuf, _countof(totalBuf));
        if (g_hwndTotalDown) SetWindowTextW(g_hwndTotalDown, totalBuf);

        FormatBytes(g_sampler.totalOut, totalBuf, _countof(totalBuf));
        if (g_hwndTotalUp) SetWindowTextW(g_hwndTotalUp, totalBuf);
    } else {
        // Sampling failed (adapter disconnected/disabled): rate-limited refresh at most every 3s
        ULONGLONG now = GetTickCount64();
        if (now - g_lastFailTick >= 3000) {
            g_lastFailTick = now;
            PopulateAdapters(false);
        }

        wcscpy_s(g_szDownSpeed, L"0.00 KB/s");
        wcscpy_s(g_szUpSpeed, L"0.00 KB/s");
        wcscpy_s(g_szDownCompact, L"0B");
        wcscpy_s(g_szUpCompact, L"0B");
        if (g_hwndSpeedDown) SetWindowTextW(g_hwndSpeedDown, L"0.00 KB/s");
        if (g_hwndSpeedUp) SetWindowTextW(g_hwndSpeedUp, L"0.00 KB/s");
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
            SetWindowTextW(g_hwndSpeedDown, L"0.00 KB/s");
            SetWindowTextW(g_hwndSpeedUp, L"0.00 KB/s");
            SetWindowTextW(g_hwndTotalDown, L"0 B");
            SetWindowTextW(g_hwndTotalUp, L"0 B");
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
        ShowWindow(g_hwndMain, IsIconic(g_hwndMain) ? SW_RESTORE : SW_SHOW);
        SetForegroundWindow(g_hwndMain);
    }
}

// ---- Settings Dialog ---------------------------------------------------------
struct SettingsDialogState {
    AppSettings tempSettings;
    HWND hwndDlg = nullptr;
    HWND hwndPreview = nullptr;
    HWND hwndLblDown = nullptr, hwndBtnDown = nullptr;
    HWND hwndLblUp = nullptr, hwndBtnUp = nullptr;
    HWND hwndLblFont = nullptr, hwndBtnFont = nullptr;
    HWND hwndLblOffset = nullptr, hwndEditOffset = nullptr, hwndSpinOffset = nullptr;
    HWND hwndLblOffsetUnit = nullptr, hwndBtnOffsetReset = nullptr;
    HWND hwndBtnSave = nullptr, hwndBtnCancel = nullptr;
    HFONT hFontDlg = nullptr;
    int dpi = 96;
    int originalOffset = 0;
};

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

static void RelayoutSettingsDialog(SettingsDialogState* state, int dpi) {
    state->dpi = dpi;
    HFONT oldFont = state->hFontDlg;
    state->hFontDlg = MakeFont(L"Segoe UI", 9.0, 0, dpi);

    struct SetItem {
        HWND hwnd;
        int x, y, w, h;
    } items[] = {
        { state->hwndLblDown,   20,  20, 140, 25 },
        { state->hwndBtnDown,  170,  16,  80, 25 },
        { state->hwndLblUp,     20,  55, 140, 25 },
        { state->hwndBtnUp,    170,  51,  80, 25 },
        { state->hwndLblFont,   20,  90, 140, 25 },
        { state->hwndBtnFont,  170,  86,  80, 25 },
        { state->hwndPreview,  270,  16, 120, 80 },
        { state->hwndLblOffset, 20, 125, 145, 25 },
        { state->hwndEditOffset,170,121,  65, 25 },
        { state->hwndSpinOffset,235,121,  18, 25 },
        { state->hwndLblOffsetUnit,258,125,  25, 25 },
        { state->hwndBtnOffsetReset,300,121,90, 25 },
        { state->hwndBtnSave,  230, 175,  75, 28 },
        { state->hwndBtnCancel,315, 175,  75, 28 },
    };

    for (const auto& it : items) {
        if (it.hwnd) {
            SendMessageW(it.hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(state->hFontDlg), TRUE);
            SetWindowPos(it.hwnd, nullptr,
                         ScaleDpi(it.x, dpi), ScaleDpi(it.y, dpi),
                         ScaleDpi(it.w, dpi), ScaleDpi(it.h, dpi),
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    if (oldFont) {
        DeleteObject(oldFont);
    }
}

static LRESULT CALLBACK SettingsPreviewWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    SettingsDialogState* state = reinterpret_cast<SettingsDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        int dpi = state ? state->dpi : 96;

        COLORREF down = state ? state->tempSettings.down : RGB(0, 255, 100);
        COLORREF up = state ? state->tempSettings.up : RGB(255, 180, 0);

        FillRect(hdc, &rc, g_brushBg);

        // Draw border
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(128, 128, 128));
        HPEN hOldPen = static_cast<HPEN>(SelectObject(hdc, hPen));
        HBRUSH hOldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, hOldBrush);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        HFONT hFont = state ? CreateOverlayFontFromSettings(state->tempSettings, dpi) : nullptr;
        HFONT hOldFont = hFont ? static_cast<HFONT>(SelectObject(hdc, hFont)) : nullptr;
        SetBkMode(hdc, TRANSPARENT);

        RECT rcUp = { rc.left + ScaleDpi(5, dpi), rc.top + ScaleDpi(8, dpi),
                      rc.right - ScaleDpi(5, dpi), rc.top + ScaleDpi(32, dpi) };
        SetTextColor(hdc, up);
        DrawTextW(hdc, L"\u2191  0.00 KB/s", -1, &rcUp, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT rcDown = { rc.left + ScaleDpi(5, dpi), rc.top + ScaleDpi(40, dpi),
                        rc.right - ScaleDpi(5, dpi), rc.top + ScaleDpi(64, dpi) };
        SetTextColor(hdc, down);
        DrawTextW(hdc, L"\u2193  0.00 KB/s", -1, &rcDown, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        if (hFont) {
            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    SettingsDialogState* state = reinterpret_cast<SettingsDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        state = reinterpret_cast<SettingsDialogState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        state->hwndDlg = hwnd;

        int dpi = static_cast<int>(GetDpiForWindow(hwnd));
        if (dpi == 0) dpi = g_currentDpi;
        state->dpi = dpi;

        auto mkBtn = [&](const wchar_t* txt, int id) {
            return CreateWindowExW(0, L"BUTTON", txt, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_hInst, nullptr);
        };
        auto mkLbl = [&](const wchar_t* txt, int id) {
            return CreateWindowExW(0, L"STATIC", txt, WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_hInst, nullptr);
        };

        state->hwndLblDown = mkLbl(L"Download Color:", ID_SET_DOWN_LBL);
        state->hwndBtnDown = mkBtn(L"Select", ID_SET_DOWN_BTN);

        state->hwndLblUp = mkLbl(L"Upload Color:", ID_SET_UP_LBL);
        state->hwndBtnUp = mkBtn(L"Select", ID_SET_UP_BTN);

        state->hwndLblFont = mkLbl(L"Overlay Font:", ID_SET_FONT_LBL);
        state->hwndBtnFont = mkBtn(L"Choose", ID_SET_FONT_BTN);

        state->hwndLblOffset = mkLbl(L"Taskbar meter offset:", ID_SET_OFFSET_LBL);
        state->hwndEditOffset = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_RIGHT | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SET_OFFSET_EDIT)), g_hInst, nullptr);
        state->hwndSpinOffset = CreateWindowExW(
            0, UPDOWN_CLASSW, nullptr,
            WS_CHILD | WS_VISIBLE | UDS_ARROWKEYS | UDS_SETBUDDYINT | UDS_NOTHOUSANDS,
            0, 0, 0, 0, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SET_OFFSET_SPIN)), g_hInst, nullptr);
        state->hwndLblOffsetUnit = mkLbl(L"px", ID_SET_OFFSET_UNIT);
        state->hwndBtnOffsetReset = mkBtn(L"Reset", ID_SET_OFFSET_RESET);
        SendMessageW(state->hwndSpinOffset, UDM_SETRANGE32,
                     static_cast<WPARAM>(static_cast<INT_PTR>(TASKBAR_METER_OFFSET_MIN)),
                     static_cast<LPARAM>(TASKBAR_METER_OFFSET_MAX));
        SendMessageW(state->hwndSpinOffset, UDM_SETBUDDY,
                     reinterpret_cast<WPARAM>(state->hwndEditOffset), 0);
        SendMessageW(state->hwndSpinOffset, UDM_SETPOS32, 0,
                     static_cast<LPARAM>(state->tempSettings.taskbarOffset));

        wchar_t prevCls[] = L"SettingsPreviewPanel";
        WNDCLASSEXW pwc = { sizeof(pwc) };
        pwc.lpfnWndProc = SettingsPreviewWndProc;
        pwc.hInstance = g_hInst;
        pwc.lpszClassName = prevCls;
        RegisterClassExW(&pwc);

        state->hwndPreview = CreateWindowExW(0, prevCls, nullptr, WS_CHILD | WS_VISIBLE,
                                             0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SET_PREVIEW)), g_hInst, nullptr);
        SetWindowLongPtrW(state->hwndPreview, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        state->hwndBtnSave = mkBtn(L"Save", ID_SET_SAVE_BTN);
        state->hwndBtnCancel = mkBtn(L"Cancel", ID_SET_CANCEL_BTN);

        RelayoutSettingsDialog(state, dpi);
        return 0;
    }
    case WM_DPICHANGED: {
        int newDpi = HIWORD(wp);
        RECT* prc = reinterpret_cast<RECT*>(lp);
        SetWindowPos(hwnd, nullptr, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        if (state) {
            RelayoutSettingsDialog(state, newDpi);
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wp);
        SetTextColor(hdc, CLR_WHITE);
        SetBkColor(hdc, CLR_BG);
        return reinterpret_cast<LRESULT>(g_brushBg);
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        int code = HIWORD(wp);
        if (id == ID_SET_OFFSET_EDIT && code == EN_CHANGE && state) {
            wchar_t text[32] = {};
            GetWindowTextW(state->hwndEditOffset, text, _countof(text));
            int offset = 0;
            if (ParseTaskbarMeterOffset(text, &offset)) {
                state->tempSettings.taskbarOffset = offset;
                g_settings.taskbarOffset = offset;
                PositionTaskbarOverlay();
            }
        } else if (id == ID_SET_OFFSET_RESET && state) {
            SetWindowTextW(state->hwndEditOffset, L"0");
        } else if (id == ID_SET_DOWN_BTN && state) {
            state->tempSettings.down = PickColor(hwnd, state->tempSettings.down);
            InvalidateRect(state->hwndPreview, nullptr, TRUE);
        } else if (id == ID_SET_UP_BTN && state) {
            state->tempSettings.up = PickColor(hwnd, state->tempSettings.up);
            InvalidateRect(state->hwndPreview, nullptr, TRUE);
        } else if (id == ID_SET_FONT_BTN && state) {
            PickFont(hwnd, &state->tempSettings, state->dpi);
            InvalidateRect(state->hwndPreview, nullptr, TRUE);
        } else if (id == ID_SET_SAVE_BTN && state) {
            wchar_t text[32] = {};
            GetWindowTextW(state->hwndEditOffset, text, _countof(text));
            int offset = 0;
            if (!ParseTaskbarMeterOffset(text, &offset)) {
                MessageBoxW(hwnd, L"Enter a whole number from -4096 to 4096.",
                            L"WinNetMeter", MB_OK | MB_ICONWARNING);
                SetFocus(state->hwndEditOffset);
                return 0;
            }
            state->tempSettings.taskbarOffset = offset;
            g_settings = state->tempSettings;
            SaveSettings(&g_settings);
            RefreshFontsAndRelayout(g_currentDpi);
            CreateOrUpdateOverlay();
            DestroyWindow(hwnd);
        } else if (id == ID_SET_CANCEL_BTN) {
            if (state) {
                g_settings.taskbarOffset = state->originalOffset;
                PositionTaskbarOverlay();
            }
            DestroyWindow(hwnd);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_brushBg);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        if (state && state->hFontDlg) {
            DeleteObject(state->hFontDlg);
            state->hFontDlg = nullptr;
        }
        return 0;
    case WM_CLOSE:
        if (state) {
            g_settings.taskbarOffset = state->originalOffset;
            PositionTaskbarOverlay();
        }
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void OpenSettingsDialog() {
    SettingsDialogState state;
    state.tempSettings = g_settings;
    state.hwndDlg = nullptr;
    state.hwndPreview = nullptr;
    state.hFontDlg = nullptr;
    state.originalOffset = g_settings.taskbarOffset;

    wchar_t cls[] = L"WinNetMeterSettings";
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = cls;
    RegisterClassExW(&wc);

    int w = ScaleDpi(420, g_currentDpi);
    int h = ScaleDpi(265, g_currentDpi);
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hwndDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        cls, L"Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, w, h,
        g_hwndMain, nullptr, g_hInst, &state);

    if (!hwndDlg) return;

    EnableWindow(g_hwndMain, FALSE);

    MSG msg = {};
    while (IsWindow(hwndDlg)) {
        BOOL result = GetMessageW(&msg, nullptr, 0, 0);
        if (result <= 0) {
            PostQuitMessage(result == 0 ? static_cast<int>(msg.wParam) : 1);
            break;
        }
        if (!IsDialogMessageW(hwndDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (IsWindow(g_hwndMain)) {
        EnableWindow(g_hwndMain, TRUE);
        if (IsWindowVisible(g_hwndMain)) SetForegroundWindow(g_hwndMain);
    }
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

        auto mkLabel = [&](const wchar_t* text, int id, UINT ss) {
            return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | ss,
                                  0, 0, 0, 0,
                                  hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_hInst, nullptr);
        };

        g_hwndIfaceLbl = mkLabel(L"Network Interface:", ID_IFACE_LABEL, SS_LEFT);

        g_combo = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                                  0, 0, 0, 0,
                                  hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_COMBO_IF)), g_hInst, nullptr);

        PopulateAdapters(true);

        g_hwndDownTitle = mkLabel(L"Download Speed:", ID_DOWN_TITLE, SS_LEFT);
        g_hwndSpeedDown = mkLabel(L"0.00 KB/s", ID_SPEED_DOWN, SS_RIGHT);

        g_hwndUpTitle = mkLabel(L"Upload Speed:", ID_UP_TITLE, SS_LEFT);
        g_hwndSpeedUp = mkLabel(L"0.00 KB/s", ID_SPEED_UP, SS_RIGHT);

        g_hwndTotdTitle = mkLabel(L"Total Downloaded:", ID_TOTD_TITLE, SS_LEFT);
        g_hwndTotalDown = mkLabel(L"0.00 MB", ID_TOTAL_DOWN, SS_RIGHT);

        g_hwndTotuTitle = mkLabel(L"Total Uploaded:", ID_TOTU_TITLE, SS_LEFT);
        g_hwndTotalUp = mkLabel(L"0.00 MB", ID_TOTAL_UP, SS_RIGHT);

        g_hwndAuthor = mkLabel(L"WinNetMeter v0.1.0\nFor support, visit: github.com/pyed/WinNetMeter",
                               ID_AUTHOR_LINK, SS_RIGHT | SS_NOTIFY);

        RefreshFontsAndRelayout(g_currentDpi);

        SetTimer(hwnd, ID_TIMER, 1000, nullptr);
        SetupTrayIcon();
        CreateOrUpdateOverlay();
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wp);
        int id = GetDlgCtrlID(reinterpret_cast<HWND>(lp));
        COLORREF c = CLR_LABEL;
        if (id == ID_SPEED_DOWN) c = CLR_DOWN;
        else if (id == ID_SPEED_UP) c = CLR_UP;
        else if (id == ID_TOTAL_DOWN || id == ID_TOTAL_UP || id == ID_IFACE_LABEL) c = CLR_WHITE;
        else if (id == ID_AUTHOR_LINK) c = CLR_GRAY;
        SetTextColor(hdc, c);
        SetBkColor(hdc, CLR_BG);
        return reinterpret_cast<LRESULT>(g_brushBg);
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
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_brushBg);

        // Separator line at y=145
        RECT line = { ScaleDpi(20, g_currentDpi), ScaleDpi(145, g_currentDpi),
                      ScaleDpi(20 + 390, g_currentDpi), ScaleDpi(147, g_currentDpi) };
        HBRUSH b = CreateSolidBrush(CLR_GRAY);
        FillRect(hdc, &line, b);
        DeleteObject(b);

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
            InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_SHOW, L"Show Window");
            InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
            InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING, ID_TRAY_SETTINGS, L"Settings...");
            InsertMenuW(hMenu, 3, MF_BYPOSITION | MF_STRING | (g_settings.showWidget ? MF_CHECKED : MF_UNCHECKED),
                        ID_TRAY_TOGGLE_WIDGET, L"Show Taskbar Widget");
            InsertMenuW(hMenu, 4, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
            InsertMenuW(hMenu, 5, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit");

            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(hMenu);

            if (cmd == ID_TRAY_SHOW) {
                ShowMainWindow();
            } else if (cmd == ID_TRAY_SETTINGS) {
                OpenSettingsDialog();
            } else if (cmd == ID_TRAY_TOGGLE_WIDGET) {
                g_settings.showWidget = !g_settings.showWidget;
                SaveSettings(&g_settings);
                CreateOrUpdateOverlay();
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

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    HANDLE singleInstance = CreateMutexW(nullptr, TRUE, SINGLE_INSTANCE_MUTEX);
    if (!singleInstance) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(singleInstance);
        HWND existing = FindWindowW(MAIN_WINDOW_CLASS, nullptr);
        if (existing && IsWindowVisible(existing)) {
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

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = MAIN_WINDOW_CLASS;
    RegisterClassExW(&wc);

    HDC hdcScreen = GetDC(nullptr);
    g_currentDpi = GetDeviceCaps(hdcScreen, LOGPIXELSX);
    ReleaseDC(nullptr, hdcScreen);

    int w = ScaleDpi(450, g_currentDpi);
    int h = ScaleDpi(300, g_currentDpi);
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, MAIN_WINDOW_CLASS, L"WinNetMeter",
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

    MSG msg = {};
    BOOL result = 0;
    while ((result = GetMessageW(&msg, nullptr, 0, 0)) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (result == -1 && IsWindow(hwnd)) DestroyWindow(hwnd);

    if (g_foregroundHook) {
        UnhookWinEvent(g_foregroundHook);
        g_foregroundHook = nullptr;
    }

    // Cleanup resources
    if (g_fontLabel) DeleteObject(g_fontLabel);
    if (g_fontValue) DeleteObject(g_fontValue);
    if (g_fontCombo) DeleteObject(g_fontCombo);
    if (g_fontAuthor) DeleteObject(g_fontAuthor);
    if (g_fontOverlay) DeleteObject(g_fontOverlay);
    if (g_brushBg) DeleteObject(g_brushBg);
    ReleaseMutex(singleInstance);
    CloseHandle(singleInstance);

    return result == -1 ? 1 : static_cast<int>(msg.wParam);
}
