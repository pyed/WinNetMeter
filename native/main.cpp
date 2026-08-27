// NetworkMonitorLite - Native Windows x64 rewrite
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
#include "settings.h"

// Window message constants & IDs
enum {
    ID_TIMER = 1,
    WM_TRAYICON = WM_APP + 1,

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
    ID_SET_BG_BTN = 2001,
    ID_SET_DOWN_BTN = 2002,
    ID_SET_UP_BTN = 2003,
    ID_SET_FONT_BTN = 2004,
    ID_SET_SAVE_BTN = 2005,
    ID_SET_CANCEL_BTN = 2006,
    ID_SET_PREVIEW = 2007,
};

// Colors
static const COLORREF CLR_BG = RGB(20, 20, 20);
static const COLORREF CLR_DOWN = RGB(0, 200, 83);     // #00C853 (Green)
static const COLORREF CLR_UP = RGB(255, 185, 0);      // #FFB900 (Orange)
static const COLORREF CLR_LABEL = RGB(211, 211, 211); // LightGray
static const COLORREF CLR_WHITE = RGB(255, 255, 255);
static const COLORREF CLR_GRAY = RGB(128, 128, 128);

// Global application state
static HINSTANCE g_hInst = nullptr;
static HWND g_hwndMain = nullptr;
static HWND g_hwndOverlay = nullptr;
static HWND g_combo = nullptr;
static HWND g_hwndSpeedDown = nullptr, g_hwndSpeedUp = nullptr;
static HWND g_hwndTotalDown = nullptr, g_hwndTotalUp = nullptr;

static HFONT g_fontLabel = nullptr;
static HFONT g_fontValue = nullptr;
static HFONT g_fontCombo = nullptr;
static HFONT g_fontAuthor = nullptr;
static HFONT g_fontOverlay = nullptr;

static HBRUSH g_brushBg = nullptr;
static HBRUSH g_brushOverlayBg = nullptr;
static HICON g_hCurrentTrayIcon = nullptr;

static UINT g_uTaskbarCreatedMsg = 0;
static NetSampler g_sampler;
static AppSettings g_settings;
static DWORD g_selectedIfIndex = 0;
static int g_currentDpi = 96;

// Overlay speed strings
static wchar_t g_szDownSpeed[64] = L"0.00 KB/s";
static wchar_t g_szUpSpeed[64] = L"0.00 KB/s";
static wchar_t g_szDownCompact[32] = L"0B";
static wchar_t g_szUpCompact[32] = L"0B";

// Overlay dragging state
static bool g_overlayDragging = false;
static POINT g_overlayDragOffset = { 0, 0 };

// Forward declarations
static void ShowMainWindow();
static void OpenSettingsDialog();
static void CreateOrUpdateOverlay();
static void PositionTaskbarOverlay();
static void UpdateTrayIcon();
static void RefreshFonts(int dpi);
static void PopulateAdapters();
static HFONT CreateOverlayFontFromSettings(const AppSettings& s, int dpi);

static int ScaleDpi(int val, int dpi) {
    return MulDiv(val, dpi, 96);
}

static HFONT MakeFont(const wchar_t* family, double pt, bool bold, bool italic, int dpi) {
    int height = -MulDiv(static_cast<int>(pt * 96.0 / 72.0 + 0.5), dpi, 96);
    return CreateFontW(height, 0, 0, 0, bold ? FW_BOLD : FW_REGULAR,
                       italic ? TRUE : FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, family);
}

static void RefreshFonts(int dpi) {
    if (g_fontLabel) DeleteObject(g_fontLabel);
    if (g_fontValue) DeleteObject(g_fontValue);
    if (g_fontCombo) DeleteObject(g_fontCombo);
    if (g_fontAuthor) DeleteObject(g_fontAuthor);
    if (g_fontOverlay) DeleteObject(g_fontOverlay);

    g_currentDpi = dpi;
    g_fontLabel = MakeFont(L"Segoe UI", 9.0, false, false, dpi);
    g_fontValue = MakeFont(L"Segoe UI", 10.0, true, false, dpi);
    g_fontCombo = MakeFont(L"Segoe UI", 9.0, false, false, dpi);
    g_fontAuthor = MakeFont(L"Segoe UI", 8.0, false, true, dpi);
    g_fontOverlay = CreateOverlayFontFromSettings(g_settings, dpi);

    if (g_brushBg) DeleteObject(g_brushBg);
    g_brushBg = CreateSolidBrush(CLR_BG);

    if (g_brushOverlayBg) DeleteObject(g_brushOverlayBg);
    g_brushOverlayBg = CreateSolidBrush(g_settings.bg);
}

static HFONT CreateOverlayFontFromSettings(const AppSettings& s, int dpi) {
    return MakeFont(s.fontFamily, s.fontSize, s.fontStyle != 0, false, dpi);
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

    // Fill background
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

    // 1-bit mask bitmap
    HBITMAP hbmpMask = CreateBitmap(w, h, 1, 1, nullptr);

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
    swprintf_s(nid.szTip, _countof(nid.szTip), L"Network Monitor\n\u2193 %s\n\u2191 %s", g_szDownSpeed, g_szUpSpeed);

    Shell_NotifyIconW(NIM_MODIFY, &nid);

    if (g_hCurrentTrayIcon) {
        DestroyIcon(g_hCurrentTrayIcon);
    }
    g_hCurrentTrayIcon = hNewIcon;
}

static void SetupTrayIcon() {
    g_hCurrentTrayIcon = CreateSpeedTrayIcon(L"0", L"0");

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwndMain;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = g_hCurrentTrayIcon;
    swprintf_s(nid.szTip, _countof(nid.szTip), L"Network Monitor\n\u2193 0 KB/s\n\u2191 0 KB/s");

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
static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Fill background with settings color
        HBRUSH hbg = CreateSolidBrush(g_settings.bg);
        FillRect(hdc, &rc, hbg);
        DeleteObject(hbg);

        // Draw 1px border RGB(60,60,60)
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
        HPEN hOldPen = static_cast<HPEN>(SelectObject(hdc, hPen));
        HBRUSH hOldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, hOldBrush);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        // Draw text: Download (top half) & Upload (bottom half)
        HFONT hOldFont = static_cast<HFONT>(SelectObject(hdc, g_fontOverlay));
        SetBkMode(hdc, TRANSPARENT);

        int midY = (rc.bottom - rc.top) / 2;

        // Down row: arrow + speed
        wchar_t downText[128];
        swprintf_s(downText, _countof(downText), L"\u2193  %s", g_szDownSpeed);
        SetTextColor(hdc, g_settings.down);
        RECT rcDown = { rc.left + ScaleDpi(5, g_currentDpi), rc.top + ScaleDpi(2, g_currentDpi),
                        rc.right - ScaleDpi(5, g_currentDpi), midY };
        DrawTextW(hdc, downText, -1, &rcDown, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Up row: arrow + speed
        wchar_t upText[128];
        swprintf_s(upText, _countof(upText), L"\u2191  %s", g_szUpSpeed);
        SetTextColor(hdc, g_settings.up);
        RECT rcUp = { rc.left + ScaleDpi(5, g_currentDpi), midY,
                      rc.right - ScaleDpi(5, g_currentDpi), rc.bottom - ScaleDpi(2, g_currentDpi) };
        DrawTextW(hdc, upText, -1, &rcUp, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, hOldFont);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        SetCapture(hwnd);
        g_overlayDragging = true;
        GetCursorPos(&g_overlayDragOffset);
        RECT rc;
        GetWindowRect(hwnd, &rc);
        g_overlayDragOffset.x -= rc.left;
        g_overlayDragOffset.y -= rc.top;
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (g_overlayDragging) {
            POINT pt;
            GetCursorPos(&pt);
            SetWindowPos(hwnd, HWND_TOPMOST, pt.x - g_overlayDragOffset.x, pt.y - g_overlayDragOffset.y,
                         0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (g_overlayDragging) {
            ReleaseCapture();
            g_overlayDragging = false;
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    }
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

    HWND hwndTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    RECT rcTaskbar = {};
    if (hwndTaskbar && GetWindowRect(hwndTaskbar, &rcTaskbar)) {
        int w = ScaleDpi(120, g_currentDpi);
        int h = ScaleDpi(40, g_currentDpi);
        int x = 0, y = 0;

        // Determine taskbar edge
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);

        if (rcTaskbar.top > 0 && rcTaskbar.right >= screenW) {
            // Taskbar at bottom
            x = rcTaskbar.right - w - ScaleDpi(350, g_currentDpi);
            y = rcTaskbar.top + ScaleDpi(3, g_currentDpi);
        } else if (rcTaskbar.top == 0 && rcTaskbar.bottom < screenH) {
            // Taskbar at top
            x = rcTaskbar.right - w - ScaleDpi(350, g_currentDpi);
            y = rcTaskbar.top + ScaleDpi(3, g_currentDpi);
        } else if (rcTaskbar.left > 0) {
            // Taskbar on right
            x = rcTaskbar.left - w - ScaleDpi(5, g_currentDpi);
            y = screenH - h - ScaleDpi(50, g_currentDpi);
        } else {
            // Taskbar on left
            x = rcTaskbar.right + ScaleDpi(5, g_currentDpi);
            y = screenH - h - ScaleDpi(50, g_currentDpi);
        }

        SetWindowPos(g_hwndOverlay, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    } else {
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int w = ScaleDpi(120, g_currentDpi);
        int h = ScaleDpi(40, g_currentDpi);
        SetWindowPos(g_hwndOverlay, HWND_TOPMOST, screenW - w - 350, screenH - h - 5, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
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
        wchar_t cls[] = L"NetworkMonitorLiteOverlay";
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = OverlayWndProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = cls;
        RegisterClassExW(&wc);

        g_hwndOverlay = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
            cls, nullptr, WS_POPUP,
            0, 0, ScaleDpi(120, g_currentDpi), ScaleDpi(40, g_currentDpi),
            nullptr, nullptr, g_hInst, nullptr);

        if (g_hwndOverlay) {
            SetLayeredWindowAttributes(g_hwndOverlay, 0, static_cast<BYTE>(0.85 * 255), LWA_ALPHA);
            PositionTaskbarOverlay();
            ShowWindow(g_hwndOverlay, SW_SHOWNOACTIVATE);
        }
    } else {
        InvalidateRect(g_hwndOverlay, nullptr, TRUE);
    }
}

// ---- Sampling & UI Update ---------------------------------------------------
static void OnTimerTick() {
    if (!g_selectedIfIndex) {
        return;
    }

    ULONGLONG downBps = 0, upBps = 0;
    if (g_sampler.Sample(g_selectedIfIndex, &downBps, &upBps)) {
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
        wcscpy_s(g_szDownSpeed, L"0.00 KB/s");
        wcscpy_s(g_szUpSpeed, L"0.00 KB/s");
        wcscpy_s(g_szDownCompact, L"0B");
        wcscpy_s(g_szUpCompact, L"0B");
        if (g_hwndSpeedDown) SetWindowTextW(g_hwndSpeedDown, L"0.00 KB/s");
        if (g_hwndSpeedUp) SetWindowTextW(g_hwndSpeedUp, L"0.00 KB/s");
    }

    UpdateTrayIcon();

    if (g_hwndOverlay && g_settings.showWidget) {
        InvalidateRect(g_hwndOverlay, nullptr, FALSE);
    }
}

static void OnComboSelectionChanged() {
    int sel = static_cast<int>(SendMessageW(g_combo, CB_GETCURSEL, 0, 0));
    if (sel >= 0) {
        DWORD ifIndex = static_cast<DWORD>(SendMessageW(g_combo, CB_GETITEMDATA, sel, 0));
        if (ifIndex != g_selectedIfIndex) {
            g_selectedIfIndex = ifIndex;
            g_sampler.Reset();
            SetWindowTextW(g_hwndSpeedDown, L"0.00 KB/s");
            SetWindowTextW(g_hwndSpeedUp, L"0.00 KB/s");
            SetWindowTextW(g_hwndTotalDown, L"0 B");
            SetWindowTextW(g_hwndTotalUp, L"0 B");
        }
    }
}

static void PopulateAdapters() {
    SendMessageW(g_combo, CB_RESETCONTENT, 0, 0);

    AdapterInfo list[64];
    int count = GetAdapters(list, 64);

    int selectedIdx = -1;
    for (int i = 0; i < count; ++i) {
        int item = static_cast<int>(SendMessageW(g_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(list[i].name)));
        SendMessageW(g_combo, CB_SETITEMDATA, item, static_cast<LPARAM>(list[i].index));
        if (list[i].index == g_selectedIfIndex || (g_selectedIfIndex == 0 && i == 0)) {
            selectedIdx = item;
            g_selectedIfIndex = list[i].index;
        }
    }

    if (selectedIdx >= 0) {
        SendMessageW(g_combo, CB_SETCURSEL, selectedIdx, 0);
    }
}

static void ShowMainWindow() {
    if (g_hwndMain) {
        ShowWindow(g_hwndMain, SW_RESTORE);
        SetForegroundWindow(g_hwndMain);
    }
}

// ---- Settings Dialog ---------------------------------------------------------
struct SettingsDialogState {
    AppSettings tempSettings;
    HWND hwndDlg;
    HWND hwndPreview;
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

static void PickFont(HWND hwndOwner, AppSettings* s) {
    LOGFONTW lf = {};
    lf.lfHeight = -MulDiv(static_cast<int>(s->fontSize * 96.0 / 72.0 + 0.5), g_currentDpi, 96);
    lf.lfWeight = (s->fontStyle != 0) ? FW_BOLD : FW_REGULAR;
    wcsncpy_s(lf.lfFaceName, _countof(lf.lfFaceName), s->fontFamily, _TRUNCATE);

    CHOOSEFONTW cf = {};
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = hwndOwner;
    cf.lpLogFont = &lf;
    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
    if (ChooseFontW(&cf)) {
        wcsncpy_s(s->fontFamily, _countof(s->fontFamily), lf.lfFaceName, _TRUNCATE);
        s->fontSize = cf.iPointSize / 10.0;
        s->fontStyle = (lf.lfWeight >= FW_BOLD) ? 1 : 0;
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

        COLORREF bg = state ? state->tempSettings.bg : RGB(30, 30, 30);
        COLORREF down = state ? state->tempSettings.down : RGB(0, 255, 100);
        COLORREF up = state ? state->tempSettings.up : RGB(255, 180, 0);

        HBRUSH hbg = CreateSolidBrush(bg);
        FillRect(hdc, &rc, hbg);
        DeleteObject(hbg);

        // Draw border
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(128, 128, 128));
        HPEN hOldPen = static_cast<HPEN>(SelectObject(hdc, hPen));
        HBRUSH hOldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, hOldBrush);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        HFONT hFont = state ? CreateOverlayFontFromSettings(state->tempSettings, g_currentDpi) : nullptr;
        HFONT hOldFont = hFont ? static_cast<HFONT>(SelectObject(hdc, hFont)) : nullptr;
        SetBkMode(hdc, TRANSPARENT);

        RECT rcDown = { rc.left + 5, rc.top + 8, rc.right - 5, rc.top + 32 };
        SetTextColor(hdc, down);
        DrawTextW(hdc, L"\u2193  0.00 KB/s", -1, &rcDown, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT rcUp = { rc.left + 5, rc.top + 40, rc.right - 5, rc.top + 64 };
        SetTextColor(hdc, up);
        DrawTextW(hdc, L"\u2191  0.00 KB/s", -1, &rcUp, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

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

        auto mkBtn = [&](const wchar_t* txt, int x, int y, int w, int h, int id) {
            HWND b = CreateWindowExW(0, L"BUTTON", txt, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     x, y, w, h, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_hInst, nullptr);
            SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
            return b;
        };
        auto mkLbl = [&](const wchar_t* txt, int x, int y, int w, int h) {
            HWND l = CreateWindowExW(0, L"STATIC", txt, WS_CHILD | WS_VISIBLE | SS_LEFT,
                                     x, y, w, h, hwnd, nullptr, g_hInst, nullptr);
            SendMessageW(l, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontLabel), TRUE);
            return l;
        };

        mkLbl(L"Overlay Background:", 20, 20, 140, 25);
        mkBtn(L"Select", 170, 16, 80, 25, ID_SET_BG_BTN);

        mkLbl(L"Download Color:", 20, 55, 140, 25);
        mkBtn(L"Select", 170, 51, 80, 25, ID_SET_DOWN_BTN);

        mkLbl(L"Upload Color:", 20, 90, 140, 25);
        mkBtn(L"Select", 170, 86, 80, 25, ID_SET_UP_BTN);

        mkLbl(L"Overlay Font:", 20, 125, 140, 25);
        mkBtn(L"Choose", 170, 121, 80, 25, ID_SET_FONT_BTN);

        // Preview panel
        wchar_t prevCls[] = L"SettingsPreviewPanel";
        WNDCLASSEXW pwc = { sizeof(pwc) };
        pwc.lpfnWndProc = SettingsPreviewWndProc;
        pwc.hInstance = g_hInst;
        pwc.lpszClassName = prevCls;
        RegisterClassExW(&pwc);

        state->hwndPreview = CreateWindowExW(0, prevCls, nullptr, WS_CHILD | WS_VISIBLE,
                                             270, 16, 120, 80, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SET_PREVIEW)), g_hInst, nullptr);
        SetWindowLongPtrW(state->hwndPreview, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        mkBtn(L"Save", 230, 170, 75, 28, ID_SET_SAVE_BTN);
        mkBtn(L"Cancel", 315, 170, 75, 28, ID_SET_CANCEL_BTN);
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
        if (id == ID_SET_BG_BTN && state) {
            state->tempSettings.bg = PickColor(hwnd, state->tempSettings.bg);
            InvalidateRect(state->hwndPreview, nullptr, TRUE);
        } else if (id == ID_SET_DOWN_BTN && state) {
            state->tempSettings.down = PickColor(hwnd, state->tempSettings.down);
            InvalidateRect(state->hwndPreview, nullptr, TRUE);
        } else if (id == ID_SET_UP_BTN && state) {
            state->tempSettings.up = PickColor(hwnd, state->tempSettings.up);
            InvalidateRect(state->hwndPreview, nullptr, TRUE);
        } else if (id == ID_SET_FONT_BTN && state) {
            PickFont(hwnd, &state->tempSettings);
            InvalidateRect(state->hwndPreview, nullptr, TRUE);
        } else if (id == ID_SET_SAVE_BTN && state) {
            g_settings = state->tempSettings;
            SaveSettings(&g_settings);
            RefreshFonts(g_currentDpi);
            CreateOrUpdateOverlay();
            DestroyWindow(hwnd);
        } else if (id == ID_SET_CANCEL_BTN) {
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
    case WM_CLOSE:
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

    wchar_t cls[] = L"NetworkMonitorLiteSettings";
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = cls;
    RegisterClassExW(&wc);

    int w = ScaleDpi(420, g_currentDpi);
    int h = ScaleDpi(260, g_currentDpi);
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

    MSG msg;
    while (IsWindow(hwndDlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hwndDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(g_hwndMain, TRUE);
    SetForegroundWindow(g_hwndMain);
}

// ---- Main Window Procedure ---------------------------------------------------
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == g_uTaskbarCreatedMsg && g_uTaskbarCreatedMsg != 0) {
        SetupTrayIcon();
        if (g_hwndOverlay && g_settings.showWidget) {
            PositionTaskbarOverlay();
        }
        return 0;
    }

    switch (msg) {
    case WM_CREATE: {
        g_hwndMain = hwnd;
        RefreshFonts(g_currentDpi);

        auto mkLabel = [&](const wchar_t* text, int x, int y, int w, int h, int id,
                           HFONT font, UINT ss) {
            HWND lbl = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | ss,
                                       ScaleDpi(x, g_currentDpi), ScaleDpi(y, g_currentDpi),
                                       ScaleDpi(w, g_currentDpi), ScaleDpi(h, g_currentDpi),
                                       hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_hInst, nullptr);
            SendMessageW(lbl, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            return lbl;
        };

        mkLabel(L"Network Interface:", 20, 20, 120, 20, ID_IFACE_LABEL, g_fontLabel, SS_LEFT);

        g_combo = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                                  ScaleDpi(150, g_currentDpi), ScaleDpi(18, g_currentDpi),
                                  ScaleDpi(260, g_currentDpi), ScaleDpi(250, g_currentDpi),
                                  hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_COMBO_IF)), g_hInst, nullptr);
        SendMessageW(g_combo, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontCombo), TRUE);

        PopulateAdapters();

        mkLabel(L"Download Speed:", 20, 70, 150, 25, ID_DOWN_TITLE, g_fontValue, SS_LEFT);
        g_hwndSpeedDown = mkLabel(L"0.00 KB/s", 180, 70, 230, 25, ID_SPEED_DOWN, g_fontValue, SS_RIGHT);

        mkLabel(L"Upload Speed:", 20, 105, 150, 25, ID_UP_TITLE, g_fontValue, SS_LEFT);
        g_hwndSpeedUp = mkLabel(L"0.00 KB/s", 180, 105, 230, 25, ID_SPEED_UP, g_fontValue, SS_RIGHT);

        mkLabel(L"Total Downloaded:", 20, 165, 150, 25, ID_TOTD_TITLE, g_fontLabel, SS_LEFT);
        g_hwndTotalDown = mkLabel(L"0.00 MB", 180, 165, 230, 25, ID_TOTAL_DOWN, g_fontLabel, SS_RIGHT);

        mkLabel(L"Total Uploaded:", 20, 195, 150, 25, ID_TOTU_TITLE, g_fontLabel, SS_LEFT);
        g_hwndTotalUp = mkLabel(L"0.00 MB", 180, 195, 230, 25, ID_TOTAL_UP, g_fontLabel, SS_RIGHT);

        // Author link at bottom
        mkLabel(L"networkMonitorLite\u2122 by mcagriaksoy - 2025\nFor support, visit: github.com/mcagriaksoy/NetworkMonitorLite",
                20, 228, 390, 32, ID_AUTHOR_LINK, g_fontAuthor, SS_RIGHT | SS_NOTIFY);

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
        if (code == CBN_SELCHANGE && id == ID_COMBO_IF) {
            OnComboSelectionChanged();
        } else if (id == ID_AUTHOR_LINK) {
            ShellExecuteW(nullptr, L"open", L"https://github.com/mcagriaksoy/NetworkMonitorLite", nullptr, nullptr, SW_SHOWNORMAL);
        }
        return 0;
    }
    case WM_TIMER:
        if (wp == ID_TIMER) {
            OnTimerTick();
        }
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
                KillTimer(hwnd, ID_TIMER);
                RemoveTrayIcon();
                if (g_hwndOverlay) {
                    DestroyWindow(g_hwndOverlay);
                    g_hwndOverlay = nullptr;
                }
                DestroyWindow(hwnd);
            }
        } else if (lp == WM_LBUTTONDBLCLK) {
            ShowMainWindow();
        }
        return 0;
    }
    case WM_DPICHANGED: {
        int newDpi = HIWORD(wp);
        RefreshFonts(newDpi);
        RECT* prc = reinterpret_cast<RECT*>(lp);
        SetWindowPos(hwnd, nullptr, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        CreateOrUpdateOverlay();
        return 0;
    }
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
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
    g_hInst = hInst;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    InitCommonControls();

    g_uTaskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    LoadSettings(&g_settings);

    wchar_t cls[] = L"NetworkMonitorLiteMain";
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = cls;
    RegisterClassExW(&wc);

    HDC hdcScreen = GetDC(nullptr);
    g_currentDpi = GetDeviceCaps(hdcScreen, LOGPIXELSX);
    ReleaseDC(nullptr, hdcScreen);

    int w = ScaleDpi(450, g_currentDpi);
    int h = ScaleDpi(300, g_currentDpi);
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hwnd = CreateWindowExW(0, cls, L"Network Monitor Lite\u2122",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                x, y, w, h, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup resources
    if (g_fontLabel) DeleteObject(g_fontLabel);
    if (g_fontValue) DeleteObject(g_fontValue);
    if (g_fontCombo) DeleteObject(g_fontCombo);
    if (g_fontAuthor) DeleteObject(g_fontAuthor);
    if (g_fontOverlay) DeleteObject(g_fontOverlay);
    if (g_brushBg) DeleteObject(g_brushBg);
    if (g_brushOverlayBg) DeleteObject(g_brushOverlayBg);

    return static_cast<int>(msg.wParam);
}
