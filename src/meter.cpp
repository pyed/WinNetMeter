#include "meter.h"
#include "overlay.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <cstring>
#include <memory>
#include <utility>

namespace {

enum : UINT {
    WM_METER_STATE = WM_APP + 1,     // lParam: MeterState*, ownership passes to the meter thread
    WM_METER_REFRESH = WM_APP + 2,   // taskbar, display or settings changed
    WM_METER_CHECK = WM_APP + 3,     // the foreground window or its geometry changed
    WM_METER_TOPMOST = WM_APP + 4,   // re-assert z-order after a foreground change
    WM_METER_QUIT = WM_APP + 5,
};

// Bursts of state updates (typing in a settings field, a flood of timer ticks)
// render at most this often; the first update of a burst renders at once.
constexpr UINT RENDER_INTERVAL_MS = 33;
constexpr UINT_PTR RENDER_TIMER = 1;

const wchar_t HOST_CLASS[] = L"WinNetMeterMeterHost";
const wchar_t OVERLAY_CLASS[] = L"WinNetMeterOverlay";
const wchar_t EMBEDDED_CLASS[] = L"WinNetMeterEmbedded";

// Overlay mode: keep re-raising for a moment after a foreground change. When
// Start closes, the taskbar leaves its higher band a little after the
// foreground moves and lands above the overlay; a re-raise issued before that
// is lost. Measured on Windows 11: with these re-raises the overlay was back
// within ~150 ms of Start losing the foreground; without them, only at the
// next refresh (up to a second later).
constexpr UINT_PTR RAISE_TIMER = 2;
constexpr UINT RAISE_INTERVAL_MS = 100;
constexpr int RAISE_REPEATS = 8;

// Set during start-up and shutdown by the UI thread; everything else here is
// touched only by the meter thread.
HANDLE g_thread = nullptr;
HANDLE g_ready = nullptr;
HWND g_host = nullptr;
HWND g_notify = nullptr;
UINT g_openMessage = 0;

HWND g_overlay = nullptr;          // overlay mode: top-level topmost layered popup
HWND g_embedded = nullptr;         // embedded mode: layered child of the taskbar
HWND g_embeddedParent = nullptr;   // the taskbar window g_embedded was created under
int g_raisesLeft = 0;
HWINEVENTHOOK g_foregroundHook = nullptr;
HWINEVENTHOOK g_locationHook = nullptr;
MeterState g_state;
ULONGLONG g_lastRender = 0;
bool g_renderPending = false;
HFONT g_font = nullptr;
UINT g_fontDpi = 0;
std::wstring g_fontFamily;
double g_fontSize = 0.0;
int g_fontStyle = -1;

// ---- Taskbar geometry ---------------------------------------------------------
bool GetTaskbarPosition(RECT* rect, UINT* edge) {
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

bool IsTaskbarShown(const RECT& expected, UINT edge) {
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

bool GetVisibleChildRect(HWND parent, const wchar_t* className, RECT* rect) {
    HWND child = parent ? FindWindowExW(parent, nullptr, className, nullptr) : nullptr;
    return child && IsWindowVisible(child) && GetWindowRect(child, rect);
}

// The parts of the taskbar the anchors refer to. Both are documented-class
// child windows on Windows 10 and 11 (TrayNotifyWnd; ReBarWindow32 holding
// MSTaskSwWClass); whatever is missing stays empty and the anchor falls back.
TaskbarLayout GetTaskbarLayout(const RECT& taskbar, UINT edge) {
    TaskbarLayout layout = {};
    layout.taskbar = taskbar;
    layout.edge = edge;
    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    GetVisibleChildRect(tray, L"TrayNotifyWnd", &layout.tray);
    HWND rebar = tray ? FindWindowExW(tray, nullptr, L"ReBarWindow32", nullptr) : nullptr;
    GetVisibleChildRect(rebar, L"MSTaskSwWClass", &layout.apps);
    return layout;
}

// ---- Fullscreen detection -----------------------------------------------------
bool IsShellOrDesktopWindow(HWND hwnd) {
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

bool GetVisibleWindowBounds(HWND hwnd, RECT* out) {
    // DWMWA_EXTENDED_FRAME_BOUNDS excludes the invisible resize borders that
    // GetWindowRect includes on Windows 10 and 11.
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, out, sizeof(*out)))) {
        return true;
    }
    return GetWindowRect(hwnd, out) != FALSE;
}

bool IsForegroundFullscreenOnMonitor(HMONITOR targetMonitor) {
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
    if (fg == g_overlay || fg == g_notify) return false;
    if (IsShellOrDesktopWindow(fg)) return false;

    HMONITOR fgMonitor = MonitorFromWindow(fg, MONITOR_DEFAULTTONULL);
    if (!fgMonitor || fgMonitor != targetMonitor) return false;

    MONITORINFO mi = { sizeof(mi) };
    if (!GetMonitorInfoW(fgMonitor, &mi)) return false;

    RECT wndRect = {};
    if (!GetVisibleWindowBounds(fg, &wndRect)) return false;
    if (!IsWindowRectFullscreen(wndRect, mi.rcMonitor)) return false;

    // A normal maximized window under an auto-hide taskbar keeps WS_CAPTION;
    // genuine fullscreen content (F11, games, video) drops it or uses WS_POPUP.
    LONG_PTR style = GetWindowLongPtrW(fg, GWL_STYLE);
    if ((style & WS_CAPTION) == WS_CAPTION && (style & WS_MAXIMIZE) != 0) {
        return false;
    }
    return true;
}

bool ShouldShowMeter(RECT* outTaskbar, UINT* outEdge) {
    if (!g_state.show) return false;

    RECT taskbar = {};
    UINT edge = ABE_BOTTOM;
    if (!GetTaskbarPosition(&taskbar, &edge)) return false;
    if (!IsTaskbarShown(taskbar, edge)) return false;

    HMONITOR taskbarMonitor = MonitorFromRect(&taskbar, MONITOR_DEFAULTTOPRIMARY);
    if (IsForegroundFullscreenOnMonitor(taskbarMonitor)) return false;

    if (outTaskbar) *outTaskbar = taskbar;
    if (outEdge) *outEdge = edge;
    return true;
}

// ---- Rendering ----------------------------------------------------------------
HFONT GetMeterFont(UINT dpi) {
    if (g_font && g_fontDpi == dpi && g_fontFamily == g_state.fontFamily &&
        g_fontSize == g_state.fontSize && g_fontStyle == g_state.fontStyle) {
        return g_font;
    }
    if (g_font) DeleteObject(g_font);
    const int style = g_state.fontStyle;
    const int height = -MulDiv(static_cast<int>(g_state.fontSize * 96.0 / 72.0 + 0.5), static_cast<int>(dpi), 96);
    g_font = CreateFontW(height, 0, 0, 0, (style & 1) ? FW_BOLD : FW_REGULAR,
                         (style & 2) ? TRUE : FALSE, (style & 4) ? TRUE : FALSE, (style & 8) ? TRUE : FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, g_state.fontFamily.c_str());
    g_fontDpi = dpi;
    g_fontFamily = g_state.fontFamily;
    g_fontSize = g_state.fontSize;
    g_fontStyle = g_state.fontStyle;
    return g_font;
}

// Draws the two meter lines into a layered window of width x height at
// destination (screen coordinates for a top-level window, parent client
// coordinates for a child). Returns whether the window was updated.
bool PaintMeter(HWND window, POINT destination, int width, int height, UINT dpi) {
    const int stride = width * 4;
    // No GetDC(NULL): a screen-compatible memory DC and a null destination DC
    // are equivalent for UpdateLayeredWindow and avoid the shared DC cache.
    HDC memory = CreateCompatibleDC(nullptr);
    if (!memory) return false;

    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(memory, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) {
        if (bitmap) DeleteObject(bitmap);
        DeleteDC(memory);
        return false;
    }

    HGDIOBJ oldBitmap = SelectObject(memory, bitmap);
    memset(bits, 0, static_cast<size_t>(stride) * static_cast<size_t>(height));
    HFONT font = GetMeterFont(dpi);
    HGDIOBJ oldFont = font ? SelectObject(memory, font) : nullptr;
    SetBkMode(memory, TRANSPARENT);
    SetTextColor(memory, RGB(255, 255, 255));

    const int middle = height / 2;
    int padding = ScaleOverlay(4, dpi);
    if (padding * 2 >= width) padding = 0;
    RECT upRect = { padding, 0, width - padding, middle };
    RECT downRect = { padding, middle, width - padding, height };
    const UINT textFlags = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
    DrawTextW(memory, g_state.upText.c_str(), -1, &upRect, textFlags);
    DrawTextW(memory, g_state.downText.c_str(), -1, &downRect, textFlags);
    GdiFlush();
    ApplyOverlayAlpha(static_cast<BYTE*>(bits), width, height, stride, middle,
                      g_state.upColor, g_state.downColor);

    POINT source = { 0, 0 };
    SIZE size = { width, height };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    BOOL updated = UpdateLayeredWindow(window, nullptr, &destination, &size,
                                       memory, &source, 0, &blend, ULW_ALPHA);

    if (oldFont) SelectObject(memory, oldFont);
    SelectObject(memory, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
    return updated != FALSE;
}

void RenderOverlay() {
    if (!g_overlay) return;

    RECT taskbar = {};
    UINT edge = ABE_BOTTOM;
    if (!ShouldShowMeter(&taskbar, &edge)) {
        if (IsWindowVisible(g_overlay)) ShowWindow(g_overlay, SW_HIDE);
        return;
    }

    UINT dpi = GetDpiForWindow(g_overlay);
    if (dpi == 0) dpi = GetDpiForSystem();
    RECT target = CalculateAnchoredMeterRect(GetTaskbarLayout(taskbar, edge), dpi,
                                             static_cast<MeterAnchor>(g_state.anchor), g_state.taskbarOffset);
    POINT destination = { target.left, target.top };
    if (PaintMeter(g_overlay, destination, target.right - target.left, target.bottom - target.top, dpi)) {
        const UINT show = IsWindowVisible(g_overlay) ? 0 : SWP_SHOWWINDOW;
        SetWindowPos(g_overlay, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | show);
    }
}

// ---- Embedded mode --------------------------------------------------------------
// The meter as a layered child of the taskbar window. When Start or Search opens,
// the shell lifts the taskbar into a higher z-order band than any app window can
// reach (measured: band 1 -> 6 on Windows 11), so a topmost overlay is covered;
// a child of the taskbar moves with it. It also hides and slides with the
// taskbar, so no fullscreen or auto-hide tracking is needed. Plain child windows
// are hidden under the taskbar's XAML content; layered ones are not. Layered
// child windows require the manifest's Windows 8+ declaration.
bool EnsureEmbedded() {
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!taskbar) return false;
    if (g_embedded && (!IsWindow(g_embedded) || g_embeddedParent != taskbar ||
                       GetParent(g_embedded) != taskbar)) {
        // Explorer restarted: the old taskbar, and with it our child, is gone.
        if (IsWindow(g_embedded)) DestroyWindow(g_embedded);
        g_embedded = nullptr;
    }
    if (!g_embedded) {
        g_embedded = CreateWindowExW(WS_EX_LAYERED | WS_EX_NOPARENTNOTIFY, EMBEDDED_CLASS, nullptr,
                                     WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 1, 1, taskbar, nullptr,
                                     GetModuleHandleW(nullptr), nullptr);
        g_embeddedParent = g_embedded ? taskbar : nullptr;
    }
    return g_embedded != nullptr;
}

void DestroyEmbedded() {
    if (g_embedded && IsWindow(g_embedded)) DestroyWindow(g_embedded);
    g_embedded = nullptr;
    g_embeddedParent = nullptr;
}

void RenderEmbedded() {
    RECT docked = {};
    UINT edge = ABE_BOTTOM;
    RECT taskbar = {};
    if (!GetTaskbarPosition(&docked, &edge) || !GetWindowRect(g_embeddedParent, &taskbar)) {
        ShowWindow(g_embedded, SW_HIDE);
        return;
    }
    // Lay out against the taskbar window's own rectangle: the child's position is
    // relative to it, so an auto-hiding taskbar carries the meter as it slides.
    UINT dpi = GetDpiForWindow(g_embeddedParent);
    if (dpi == 0) dpi = GetDpiForSystem();
    RECT target = CalculateAnchoredMeterRect(GetTaskbarLayout(taskbar, edge), dpi,
                                             static_cast<MeterAnchor>(g_state.anchor), g_state.taskbarOffset);
    const int width = target.right - target.left;
    const int height = target.bottom - target.top;
    // Two points are mapped as a rectangle, which keeps left < right if the
    // taskbar is mirrored (right-to-left layouts).
    MapWindowPoints(nullptr, g_embeddedParent, reinterpret_cast<POINT*>(&target), 2);
    POINT destination = { target.left, target.top };
    if (PaintMeter(g_embedded, destination, width, height, dpi)) {
        // Above the taskbar's own child windows (its XAML host covers the whole bar).
        const UINT show = IsWindowVisible(g_embedded) ? 0 : SWP_SHOWWINDOW;
        SetWindowPos(g_embedded, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | show);
    }
}

void EnsureOverlayTopmost() {
    if (!g_overlay || !IsWindowVisible(g_overlay)) return;
    if (!ShouldShowMeter(nullptr, nullptr)) {
        ShowWindow(g_overlay, SW_HIDE);
        return;
    }
    SetWindowPos(g_overlay, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// Serves both meter windows. The embedded one is a child of another process's
// window, and DefWindowProc forwards some messages from a child to its parent
// with SendMessage; those are handled here so the meter thread never waits on
// Explorer.
LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        if (hwnd == g_embedded) {
            RenderEmbedded();
        } else {
            RenderOverlay();
        }
        return 0;
    }
    case WM_DPICHANGED:
    case WM_DPICHANGED_AFTERPARENT:
        PostMessageW(g_host, WM_METER_REFRESH, 0, 0);
        return 0;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return TRUE;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_CONTEXTMENU:
        return 0;
    case WM_APPCOMMAND:
        return TRUE;
    case WM_LBUTTONDBLCLK:
        // Posted, never sent: the meter thread must not wait on the UI thread.
        PostMessageW(g_notify, g_openMessage, 0, 0);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void DestroyOverlay() {
    if (g_overlay) DestroyWindow(g_overlay);
    g_overlay = nullptr;
}

// Creates, swaps or destroys the meter window to match the state, then renders.
void SyncOverlay() {
    if (!g_state.show) {
        DestroyOverlay();
        DestroyEmbedded();
        return;
    }
    // Embedded when asked and possible; the overlay is the fallback (no taskbar
    // window, or the child could not be created).
    if (g_state.embedded && EnsureEmbedded()) {
        DestroyOverlay();
        RenderEmbedded();
        return;
    }
    DestroyEmbedded();
    if (!g_overlay) {
        RECT taskbar = {};
        UINT edge = ABE_BOTTOM;
        if (!GetTaskbarPosition(&taskbar, &edge)) return;
        g_overlay = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
            OVERLAY_CLASS, nullptr, WS_POPUP, taskbar.left, taskbar.top, 1, 1,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    }
    RenderOverlay();
}

// Leading-edge throttle for state-driven renders.
void ScheduleSync() {
    const ULONGLONG now = GetTickCount64();
    const ULONGLONG elapsed = now - g_lastRender;
    if (!g_renderPending && elapsed >= RENDER_INTERVAL_MS) {
        g_lastRender = now;
        SyncOverlay();
        return;
    }
    if (!g_renderPending) {
        g_renderPending = true;
        const UINT wait = elapsed >= RENDER_INTERVAL_MS ? 1 : static_cast<UINT>(RENDER_INTERVAL_MS - elapsed);
        SetTimer(g_host, RENDER_TIMER, wait, nullptr);
    }
}

void CALLBACK OnForegroundChanged(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG, LONG, DWORD, DWORD) {
    if (event == EVENT_SYSTEM_FOREGROUND && hwnd && hwnd != g_overlay) {
        PostMessageW(g_host, WM_METER_CHECK, 0, 0);
        PostMessageW(g_host, WM_METER_TOPMOST, 0, 0);
    }
}

void CALLBACK OnLocationChanged(HWINEVENTHOOK, DWORD event, HWND hwnd,
                                LONG idObject, LONG idChild, DWORD, DWORD) {
    // Top-level window moves only (not child controls, carets, ...)
    if (event != EVENT_OBJECT_LOCATIONCHANGE) return;
    if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) return;
    if (!hwnd || hwnd == g_overlay || hwnd == g_notify) return;

    // Only the foreground window's geometry matters
    HWND fg = GetForegroundWindow();
    if (!fg) return;
    HWND root = GetAncestor(fg, GA_ROOT);
    if (root) fg = root;
    HWND hwndRoot = GetAncestor(hwnd, GA_ROOT);
    if (hwndRoot) hwnd = hwndRoot;
    if (hwnd != fg) return;

    PostMessageW(g_host, WM_METER_CHECK, 0, 0);
}

void Shutdown() {
    if (g_foregroundHook) UnhookWinEvent(g_foregroundHook);
    if (g_locationHook) UnhookWinEvent(g_locationHook);
    g_foregroundHook = g_locationHook = nullptr;
    KillTimer(g_host, RENDER_TIMER);
    KillTimer(g_host, RAISE_TIMER);
    DestroyOverlay();
    DestroyEmbedded();
    if (g_font) DeleteObject(g_font);
    g_font = nullptr;
    // Free snapshots that were still queued.
    MSG pending;
    while (PeekMessageW(&pending, g_host, WM_METER_STATE, WM_METER_STATE, PM_REMOVE)) {
        delete reinterpret_cast<MeterState*>(pending.lParam);
    }
}

LRESULT CALLBACK HostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_METER_STATE: {
        // Coalesce: only the newest queued snapshot is worth drawing.
        std::unique_ptr<MeterState> latest(reinterpret_cast<MeterState*>(lp));
        MSG pending;
        while (PeekMessageW(&pending, hwnd, WM_METER_STATE, WM_METER_STATE, PM_REMOVE)) {
            latest.reset(reinterpret_cast<MeterState*>(pending.lParam));
        }
        if (latest) g_state = std::move(*latest);
        ScheduleSync();
        return 0;
    }
    case WM_TIMER:
        if (wp == RENDER_TIMER) {
            KillTimer(hwnd, RENDER_TIMER);
            g_renderPending = false;
            g_lastRender = GetTickCount64();
            SyncOverlay();
        } else if (wp == RAISE_TIMER) {
            EnsureOverlayTopmost();
            if (--g_raisesLeft <= 0) KillTimer(hwnd, RAISE_TIMER);
        }
        return 0;
    case WM_METER_REFRESH:
        SyncOverlay();
        return 0;
    case WM_METER_CHECK:
        // Fullscreen tracking only matters to the overlay; the embedded meter
        // hides with the taskbar.
        RenderOverlay();
        return 0;
    case WM_METER_TOPMOST:
        if (g_overlay) {
            EnsureOverlayTopmost();
            g_raisesLeft = RAISE_REPEATS;
            SetTimer(hwnd, RAISE_TIMER, RAISE_INTERVAL_MS, nullptr);
        }
        return 0;
    case WM_METER_QUIT:
        Shutdown();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

DWORD WINAPI MeterThread(void*) {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW host = { sizeof(host) };
    host.lpfnWndProc = HostProc;
    host.hInstance = instance;
    host.lpszClassName = HOST_CLASS;
    RegisterClassExW(&host);

    WNDCLASSEXW overlay = { sizeof(overlay) };
    overlay.style = CS_DBLCLKS;
    overlay.lpfnWndProc = OverlayProc;
    overlay.hInstance = instance;
    overlay.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    overlay.lpszClassName = OVERLAY_CLASS;
    RegisterClassExW(&overlay);
    overlay.lpszClassName = EMBEDDED_CLASS;   // same behaviour, distinct for diagnostics and tests
    RegisterClassExW(&overlay);

    g_host = CreateWindowExW(0, HOST_CLASS, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
    if (g_host) {
        // Out-of-context hooks call back on this thread through its message loop.
        g_foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                           nullptr, OnForegroundChanged, 0, 0, WINEVENT_OUTOFCONTEXT);
        g_locationHook = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE,
                                         nullptr, OnLocationChanged, 0, 0, WINEVENT_OUTOFCONTEXT);
    }
    SetEvent(g_ready);
    if (!g_host) return 1;

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    DestroyWindow(g_host);
    return 0;
}

}  // namespace

bool StartMeterHost(HWND notifyWindow, UINT openMessage) {
    if (g_thread) return g_host != nullptr;
    g_notify = notifyWindow;
    g_openMessage = openMessage;
    g_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_ready) return false;
    g_thread = CreateThread(nullptr, 0, MeterThread, nullptr, 0, nullptr);
    if (g_thread) WaitForSingleObject(g_ready, 5000);
    CloseHandle(g_ready);
    g_ready = nullptr;
    return g_host != nullptr;
}

void PushMeterState(const MeterState& state) {
    if (!g_host) return;
    auto* copy = new MeterState(state);
    if (!PostMessageW(g_host, WM_METER_STATE, 0, reinterpret_cast<LPARAM>(copy))) delete copy;
}

void RequestMeterRefresh() {
    if (g_host) PostMessageW(g_host, WM_METER_REFRESH, 0, 0);
}

void StopMeterHost() {
    if (!g_thread) return;
    if (g_host) PostMessageW(g_host, WM_METER_QUIT, 0, 0);
    // The meter thread never waits on this one, so this cannot deadlock. If it
    // somehow stalls, process exit tears it down anyway.
    WaitForSingleObject(g_thread, 3000);
    CloseHandle(g_thread);
    g_thread = nullptr;
    g_host = nullptr;
}
