// NetworkMonitorLite - native Win32 rewrite. Slice 1: main window + network sampling.
// Layout mirrors the C# original's client coordinates (450x300, 96-dpi points).
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <commctrl.h>
#include <shellapi.h>
#include "network.h"
#include "settings.h"

// ---- IDs / colors (mirror C# original) ----------------------------------------
enum {
    ID_TIMER = 1,
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
};

static const COLORREF CLR_BG    = RGB(20, 20, 20);
static const COLORREF CLR_DOWN  = RGB(0, 200, 83);    // #00C853
static const COLORREF CLR_UP    = RGB(255, 185, 0);   // #FFB900
static const COLORREF CLR_LABEL = RGB(211, 211, 211); // LightGray
static const COLORREF CLR_WHITE = RGB(255, 255, 255);
static const COLORREF CLR_GRAY  = RGB(128, 128, 128);

// ---- formatting (mirrors C# Formatting) ----------------------------------------
static void FormatSpeed(ULONGLONG bps, wchar_t* out, size_t n) {
    if (bps < 1024)
        swprintf_s(out, n, L"%llu B/s", static_cast<unsigned long long>(bps));
    else if (bps < 1024ULL * 1024)
        swprintf_s(out, n, L"%.2f KB/s", bps / 1024.0);
    else if (bps < 1024ULL * 1024 * 1024)
        swprintf_s(out, n, L"%.2f MB/s", bps / (1024.0 * 1024.0));
    else
        swprintf_s(out, n, L"%.2f GB/s", bps / (1024.0 * 1024.0 * 1024.0));
}

static void FormatBytes(ULONGLONG b, wchar_t* out, size_t n) {
    if (b < 1024)
        swprintf_s(out, n, L"%llu B", static_cast<unsigned long long>(b));
    else if (b < 1024ULL * 1024)
        swprintf_s(out, n, L"%.2f KB", b / 1024.0);
    else if (b < 1024ULL * 1024 * 1024)
        swprintf_s(out, n, L"%.2f MB", b / (1024.0 * 1024.0));
    else
        swprintf_s(out, n, L"%.2f GB", b / (1024.0 * 1024.0 * 1024.0));
}

// ---- app state -----------------------------------------------------------------
static HWND g_main = nullptr;
static HWND g_combo = nullptr;
static HWND g_hwndSpeedDown = nullptr, g_hwndSpeedUp = nullptr;
static HWND g_hwndTotalDown = nullptr, g_hwndTotalUp = nullptr;
static HFONT g_fontLabel = nullptr, g_fontValue = nullptr, g_fontCombo = nullptr, g_fontAuthor = nullptr;
static HBRUSH g_brushBg = nullptr;
static NetSampler g_sampler;
static AppSettings g_settings;

static int PtToPx(double pt) { // slice 1: 96-dpi only; WM_DPICHANGED scaling is a later slice
    return static_cast<int>(pt * 96.0 / 72.0);
}

static HFONT MakeFont(double pt, bool bold, bool italic) {
    return CreateFontW(PtToPx(pt), 0, 0, 0, bold ? FW_BOLD : FW_REGULAR,
                       italic ? TRUE : FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

// ---- main window ----------------------------------------------------------------
static void OnTimer(HWND) {
    if (!g_combo)
        return;
    AdapterInfo list[64];
    int n = GetAdapters(list, 64);
    int sel = static_cast<int>(SendMessageW(g_combo, CB_GETCURSEL, 0, 0));
    if (sel < 0 || sel >= n)
        return;
    ULONGLONG d = 0, u = 0;
    if (g_sampler.Sample(list[sel].index, &d, &u)) {
        g_settings.lastInterfaceIndex = list[sel].index; // persist selection like C#
        wchar_t buf[64];
        FormatSpeed(d, buf, _countof(buf));
        SetWindowTextW(g_hwndSpeedDown, buf);
        FormatSpeed(u, buf, _countof(buf));
        SetWindowTextW(g_hwndSpeedUp, buf);
        FormatBytes(g_sampler.totalIn, buf, _countof(buf));
        SetWindowTextW(g_hwndTotalDown, buf);
        FormatBytes(g_sampler.totalOut, buf, _countof(buf));
        SetWindowTextW(g_hwndTotalUp, buf);
    } else {
        SetWindowTextW(g_hwndSpeedDown, L"0.00 KB/s");
        SetWindowTextW(g_hwndSpeedUp, L"0.00 KB/s");
    }
}

static void OnComboSelect() {
    g_sampler.Reset();
    wchar_t buf[64];
    FormatSpeed(0, buf, _countof(buf));
    SetWindowTextW(g_hwndSpeedDown, buf);
    SetWindowTextW(g_hwndSpeedUp, buf);
    SetWindowTextW(g_hwndTotalDown, L"0 B");
    SetWindowTextW(g_hwndTotalUp, L"0 B");
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_main = hwnd;
        g_brushBg = CreateSolidBrush(CLR_BG);
        g_fontLabel  = MakeFont(9.0, false, false);
        g_fontValue  = MakeFont(10.0, true, false);
        g_fontCombo  = MakeFont(9.0, false, false);
        g_fontAuthor = MakeFont(8.0, false, true);

        auto mkLabel = [&](const wchar_t* text, int x, int y, int w, int h, int id,
                           HFONT font, UINT ss = SS_LEFT) {
            HWND h = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | ss,
                                     x, y, w, h, hwnd, reinterpret_cast<HMENU>(id), nullptr, nullptr);
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            return h;
        };

        // iface (20,20,120,20), combo (150,18,260,25)
        mkLabel(L"Network Interface:", 20, 20, 120, 20, ID_IFACE_LABEL, g_fontLabel);
        g_combo = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                                  150, 18, 260, 250, hwnd, reinterpret_cast<HMENU>(ID_COMBO_IF),
                                  nullptr, nullptr);
        SendMessageW(g_combo, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontCombo), TRUE);
        {
            AdapterInfo list[64];
            int n = GetAdapters(list, 64);
            for (int i = 0; i < n; ++i)
                SendMessageW(g_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(list[i].name));
            if (n > 0)
                SendMessageW(g_combo, CB_SETCURSEL, 0, 0);
        }

        // speeds: titles 150x25 10pt bold; values 230x25 right-aligned 10pt bold
        mkLabel(L"Download Speed:", 20, 70, 150, 25, ID_DOWN_TITLE, g_fontValue);
        g_hwndSpeedDown = mkLabel(L"0.00 KB/s", 180, 70, 230, 25, ID_SPEED_DOWN, g_fontValue, SS_RIGHT);
        mkLabel(L"Upload Speed:", 20, 105, 150, 25, ID_UP_TITLE, g_fontValue);
        g_hwndSpeedUp = mkLabel(L"0.00 KB/s", 180, 105, 230, 25, ID_SPEED_UP, g_fontValue, SS_RIGHT);

        // totals: titles 150x25 9pt; values 230x25 9pt
        mkLabel(L"Total Downloaded:", 20, 165, 150, 25, ID_TOTD_TITLE, g_fontLabel);
        g_hwndTotalDown = mkLabel(L"0 B", 180, 165, 230, 25, ID_TOTAL_DOWN, g_fontLabel, SS_RIGHT);
        mkLabel(L"Total Uploaded:", 20, 195, 150, 25, ID_TOTU_TITLE, g_fontLabel);
        g_hwndTotalUp = mkLabel(L"0 B", 180, 195, 230, 25, ID_TOTAL_UP, g_fontLabel, SS_RIGHT);

        // author: bottom 36px band, 8pt italic gray, right-aligned, both lines clickable
        mkLabel(L"networkMonitorLite\u2122 by mcagriaksoy - 2025", 2, 266, 446, 14, ID_AUTHOR_LINK, g_fontAuthor, SS_RIGHT);
        mkLabel(L"For support, visit: github.com/mcagriaksoy/NetworkMonitorLite", 2, 281, 446, 14, ID_AUTHOR_LINK, g_fontAuthor, SS_RIGHT);

        SetTimer(hwnd, ID_TIMER, 1000, nullptr);
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
    case WM_COMMAND:
        if (HIWORD(wp) == CBN_SELCHANGE && LOWORD(wp) == ID_COMBO_IF)
            OnComboSelect();
        else if (LOWORD(wp) == ID_AUTHOR_LINK) // original: LinkLabel click
            ShellExecuteW(nullptr, L"open", L"https://github.com/mcagriaksoy/NetworkMonitorLite", nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case WM_TIMER:
        if (wp == ID_TIMER)
            OnTimer(hwnd);
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_brushBg);
        RECT line{ 20, 145, 20 + 390, 147 }; // original: (20,145) 390x2 Gray
        HBRUSH b = CreateSolidBrush(CLR_GRAY);
        FillRect(hdc, &line, b);
        DeleteObject(b);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE: // original: X hides to tray
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    InitCommonControls();

    wchar_t cls[] = L"NetworkMonitorLiteMain";
    WNDCLASSEXW wc{ sizeof wc };
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = cls;
    RegisterClassExW(&wc);

    LoadSettings(&g_settings);

    // C# StartPosition = CenterScreen: full-screen center
    int w = 450, h = 300;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    HWND hwnd = CreateWindowExW(0, cls, L"Network Monitor Lite\u2122",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                x, y, w, h, nullptr, nullptr, hInst, nullptr);
    if (!hwnd)
        return 1;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    if (g_main)
        KillTimer(g_main, ID_TIMER);
    if (g_fontLabel)  DeleteObject(g_fontLabel);
    if (g_fontValue)  DeleteObject(g_fontValue);
    if (g_fontCombo)  DeleteObject(g_fontCombo);
    if (g_fontAuthor) DeleteObject(g_fontAuthor);
    if (g_brushBg)    DeleteObject(g_brushBg);
    SaveSettings(&g_settings);
    return static_cast<int>(m.wParam);
}
