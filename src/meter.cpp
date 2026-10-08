#include "meter.h"
#include "overlay.h"
#include "render.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace {

enum : UINT {
    WM_METER_STATE = WM_APP + 1,     // lParam: MeterState*, ownership passes to the meter thread
    WM_METER_REFRESH = WM_APP + 2,   // taskbar, display or settings changed
    WM_METER_CHECK = WM_APP + 3,     // the foreground window or its geometry changed
    WM_METER_TOPMOST = WM_APP + 4,   // re-assert z-order after a foreground change
    WM_METER_QUIT = WM_APP + 5,
    WM_METER_APPBAR = WM_APP + 6,    // appbar notifications from the shell (ABN_*)
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

// One meter per taskbar. The primary taskbar's comes first; secondary taskbars
// (other monitors) get one each when all taskbars are wanted.
struct MeterSlot {
    bool primary = false;
    HWND taskbar = nullptr;   // Shell_TrayWnd (may be null for the primary) or a Shell_SecondaryTrayWnd
    HWND window = nullptr;    // the meter
    bool embedded = false;    // window is a child of taskbar, else a topmost overlay
};

struct CachedFont {
    UINT dpi;
    HFONT font;
};

// The stacked meter's font, fitted to a box width. One entry: it is rebuilt
// when the width, DPI, font or widest lines change, which settings changes do.
struct StackedFont {
    UINT dpi = 0;
    int width = 0;
    std::wstring key;
    HFONT font = nullptr;
    int rowHeight = 0;
};

// Set during start-up and shutdown by the UI thread; everything else here is
// touched only by the meter thread.
HANDLE g_thread = nullptr;
HANDLE g_ready = nullptr;
HWND g_host = nullptr;
HWND g_notify = nullptr;
UINT g_openMessage = 0;

std::vector<MeterSlot> g_slots;
int g_raisesLeft = 0;
HWND g_appbarShell = nullptr;          // the Shell_TrayWnd our appbar is registered with
HWND g_shellFullscreen = nullptr;      // the app the shell last reported as fullscreen
HMONITOR g_shellFullscreenMonitor = nullptr;
HWINEVENTHOOK g_foregroundHook = nullptr;
HWINEVENTHOOK g_locationHook = nullptr;
MeterState g_state;
ULONGLONG g_lastRender = 0;
bool g_renderPending = false;
std::vector<CachedFont> g_fonts;   // one per DPI in use, for the font below
std::wstring g_fontFamily;
double g_fontSize = 0.0;
int g_fontStyle = -1;
StackedFont g_stackedFont;

bool IsMeterWindow(HWND hwnd) {
    if (!hwnd) return false;
    for (const MeterSlot& slot : g_slots) {
        if (slot.window == hwnd) return true;
    }
    return false;
}

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

// A secondary taskbar's parts. Windows 10 gives them a clock window and a task
// list (class names as used by common taskbar tools; not verified here, a
// single-monitor machine); Windows 11 gives them neither. Without a clock
// window, the primary's notification area is mirrored (see MirrorTrayArea).
TaskbarLayout GetSecondaryLayout(HWND taskbarWindow, const RECT& taskbar, UINT edge) {
    TaskbarLayout layout = {};
    layout.taskbar = taskbar;
    layout.edge = edge;
    if (!GetVisibleChildRect(taskbarWindow, L"ClockButton", &layout.tray) ||
        !IsUsableTaskbarPart(layout.tray, taskbar)) {
        layout.tray = {};
        HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr);
        RECT primaryWindow = {};
        RECT docked = {};
        UINT primaryEdge = ABE_BOTTOM;
        // The primary's window rect, not its docked one: the notification area
        // moves with the window when an auto-hidden taskbar slides away.
        if (primary && GetWindowRect(primary, &primaryWindow) && GetTaskbarPosition(&docked, &primaryEdge)) {
            layout.tray = MirrorTrayArea(GetTaskbarLayout(primaryWindow, primaryEdge), GetDpiForWindow(primary),
                                         taskbar, edge, GetDpiForWindow(taskbarWindow));
        }
    }
    HWND worker = FindWindowExW(taskbarWindow, nullptr, L"WorkerW", nullptr);
    GetVisibleChildRect(worker, L"MSTaskListWClass", &layout.apps);
    return layout;
}

TaskbarLayout GetSlotLayout(const MeterSlot& slot, const RECT& taskbar, UINT edge) {
    return slot.primary ? GetTaskbarLayout(taskbar, edge) : GetSecondaryLayout(slot.taskbar, taskbar, edge);
}

// Secondary taskbars have no appbar messages: read the window and its monitor.
// shown is false while an auto-hidden one has slid away.
bool GetSecondaryTaskbar(HWND taskbarWindow, RECT* taskbar, UINT* edge, HMONITOR* monitor, bool* shown) {
    if (!taskbarWindow || !IsWindowVisible(taskbarWindow) || !GetWindowRect(taskbarWindow, taskbar)) {
        return false;
    }
    *monitor = MonitorFromWindow(taskbarWindow, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = { sizeof(info) };
    if (!GetMonitorInfoW(*monitor, &info)) return false;
    *edge = TaskbarEdgeOnMonitor(*taskbar, info.rcMonitor);
    *shown = IsTaskbarMostlyOnMonitor(*taskbar, *edge, info.rcMonitor);
    return true;
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

bool IsWindowFullscreenOnMonitor(HWND fg, HMONITOR targetMonitor) {
    if (!fg) return false;

    // A hidden or minimized window is never fullscreen application content
    if (!IsWindowVisible(fg)) return false;
    if (IsIconic(fg) || (GetWindowLongPtrW(fg, GWL_STYLE) & WS_MINIMIZE) != 0) return false;

    // Normalize to root window to avoid classifying child controls/tooltips/menus
    HWND root = GetAncestor(fg, GA_ROOT);
    if (root) fg = root;

    // Don't classify our own windows or shell/desktop surfaces as fullscreen
    if (IsMeterWindow(fg) || fg == g_notify) return false;
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

// ---- Shell fullscreen signal --------------------------------------------------
// The shell tells appbars when a fullscreen app opens or closes
// (ABN_FULLSCREENAPP); that is how the taskbar knows to step behind it. The host
// window registers as an appbar that reserves no space (no ABM_SETPOS) only to
// hear that. Measured on Windows 11: OPEN/CLOSE arrive within ~20 ms of every
// transition, for a message-only window too, and the work area is unchanged.
// Registrations die with Explorer, so a new taskbar window re-registers.
void UpdateAppBarRegistration() {
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!taskbar || taskbar == g_appbarShell) return;
    APPBARDATA data = {};
    data.cbSize = sizeof(data);
    data.hWnd = g_host;
    data.uCallbackMessage = WM_METER_APPBAR;
    if (g_appbarShell) SHAppBarMessage(ABM_REMOVE, &data);
    g_appbarShell = SHAppBarMessage(ABM_NEW, &data) ? taskbar : nullptr;
    g_shellFullscreen = nullptr;
}

void RemoveAppBarRegistration() {
    if (!g_appbarShell) return;
    APPBARDATA data = {};
    data.cbSize = sizeof(data);
    data.hWnd = g_host;
    SHAppBarMessage(ABM_REMOVE, &data);
    g_appbarShell = nullptr;
}

// OPEN names no window: it is the foreground one, which the shell has just
// found to be fullscreen.
void OnShellFullscreen(bool open) {
    g_shellFullscreen = nullptr;
    g_shellFullscreenMonitor = nullptr;
    HWND app = open ? GetForegroundWindow() : nullptr;
    HWND root = app ? GetAncestor(app, GA_ROOT) : nullptr;
    if (root) app = root;
    if (app && !IsMeterWindow(app) && app != g_notify && !IsShellOrDesktopWindow(app)) {
        g_shellFullscreen = app;
        g_shellFullscreenMonitor = MonitorFromWindow(app, MONITOR_DEFAULTTONULL);
    }
}

// The app the shell reported, while it still looks like one: visible, not
// minimized, on the same monitor. Anything else means a missed CLOSE; forget it.
bool IsShellFullscreenOnMonitor(HMONITOR targetMonitor) {
    HWND app = g_shellFullscreen;
    if (!app) return false;
    if (!IsWindow(app) || !IsWindowVisible(app) || IsIconic(app) ||
        (GetWindowLongPtrW(app, GWL_STYLE) & WS_MINIMIZE) != 0 ||
        MonitorFromWindow(app, MONITOR_DEFAULTTONULL) != g_shellFullscreenMonitor) {
        g_shellFullscreen = nullptr;
        return false;
    }
    return g_shellFullscreenMonitor == targetMonitor;
}

bool IsForegroundFullscreenOnMonitor(HMONITOR targetMonitor) {
    if (!targetMonitor) return false;
    HWND fg = GetForegroundWindow();
    if (IsWindowFullscreenOnMonitor(fg, targetMonitor)) return true;
    // A smaller window in front of a fullscreen app leaves the shell in
    // fullscreen mode and the taskbar behind the app (measured), so keep the
    // meter hidden too; unless the shell's own UI (taskbar, desktop) is in front.
    HWND root = fg ? GetAncestor(fg, GA_ROOT) : nullptr;
    if (root) fg = root;
    if (fg && IsShellOrDesktopWindow(fg)) return false;
    return IsShellFullscreenOnMonitor(targetMonitor);
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

// Overlay meters: whether the slot's taskbar is up and not under fullscreen
// content, and where it is.
bool ShouldShowOverlay(const MeterSlot& slot, RECT* taskbar, UINT* edge) {
    if (slot.primary) return ShouldShowMeter(taskbar, edge);
    HMONITOR monitor = nullptr;
    bool shown = false;
    return g_state.show && GetSecondaryTaskbar(slot.taskbar, taskbar, edge, &monitor, &shown) && shown &&
           !IsForegroundFullscreenOnMonitor(monitor);
}

// ---- Rendering ----------------------------------------------------------------
void ClearFonts() {
    for (const CachedFont& cached : g_fonts) DeleteObject(cached.font);
    g_fonts.clear();
}

HFONT GetMeterFont(UINT dpi) {
    if (g_fontFamily != g_state.fontFamily || g_fontSize != g_state.fontSize ||
        g_fontStyle != g_state.fontStyle || g_fonts.size() >= 8) {
        ClearFonts();
        g_fontFamily = g_state.fontFamily;
        g_fontSize = g_state.fontSize;
        g_fontStyle = g_state.fontStyle;
    }
    for (const CachedFont& cached : g_fonts) {
        if (cached.dpi == dpi) return cached.font;
    }
    const int style = g_state.fontStyle;
    const int height = -MulDiv(static_cast<int>(g_state.fontSize * 96.0 / 72.0 + 0.5), static_cast<int>(dpi), 96);
    HFONT font = CreateFontW(height, 0, 0, 0, (style & 1) ? FW_BOLD : FW_REGULAR,
                             (style & 2) ? TRUE : FALSE, (style & 4) ? TRUE : FALSE, (style & 8) ? TRUE : FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, g_state.fontFamily.c_str());
    if (font) g_fonts.push_back({ dpi, font });
    return font;
}

void ClearStackedFont() {
    if (g_stackedFont.font) DeleteObject(g_stackedFont.font);
    g_stackedFont = StackedFont();
}

// The user's font, shrunk if needed so that the widest value line and every
// unit fit across the box. Sized for the widest possible lines rather than the
// current ones, so it does not change size as the speed changes.
bool GetStackedFont(UINT dpi, int boxWidth, HFONT* font, int* rowHeight) {
    const std::wstring key = g_state.fontFamily + L'|' + std::to_wstring(g_state.fontSize) + L'|' +
                             std::to_wstring(g_state.fontStyle) + L'|' + g_state.upHeadWidest + L'|' +
                             g_state.downHeadWidest;
    if (!g_stackedFont.font || g_stackedFont.dpi != dpi || g_stackedFont.width != boxWidth ||
        g_stackedFont.key != key) {
        ClearStackedFont();
        const int style = g_state.fontStyle;
        LOGFONTW base = {};
        base.lfHeight = -MulDiv(static_cast<int>(g_state.fontSize * 96.0 / 72.0 + 0.5), static_cast<int>(dpi), 96);
        base.lfWeight = (style & 1) ? FW_BOLD : FW_REGULAR;
        base.lfItalic = (style & 2) ? TRUE : FALSE;
        base.lfUnderline = (style & 4) ? TRUE : FALSE;
        base.lfStrikeOut = (style & 8) ? TRUE : FALSE;
        base.lfCharSet = DEFAULT_CHARSET;
        base.lfQuality = ANTIALIASED_QUALITY;
        wcsncpy_s(base.lfFaceName, g_state.fontFamily.c_str(), _TRUNCATE);
        const std::wstring lines[] = { g_state.upHeadWidest, g_state.downHeadWidest,
                                       L"B/s", L"KB/s", L"MB/s", L"GB/s", L"bps", L"Kbps", L"Mbps", L"Gbps" };
        // Not below about 6 pt: smaller is unreadable, so clip instead.
        g_stackedFont.font = CreateFittingFont(base, MulDiv(8, static_cast<int>(dpi), 96), lines, _countof(lines),
                                               boxWidth - 2 * ScaleOverlay(2, dpi), &g_stackedFont.rowHeight);
        g_stackedFont.dpi = dpi;
        g_stackedFont.width = boxWidth;
        g_stackedFont.key = key;
    }
    *font = g_stackedFont.font;
    *rowHeight = g_stackedFont.rowHeight;
    return g_stackedFont.font != nullptr;
}

// Where a slot's meter goes on its taskbar, and how its text is laid out:
// two lines, or (stackedFont set) four stacked lines on a narrow vertical taskbar.
struct MeterPlacement {
    RECT target;
    HFONT stackedFont;
    int rowHeight;
};

MeterPlacement PlaceMeter(const MeterSlot& slot, const RECT& taskbar, UINT edge, UINT dpi) {
    MeterPlacement placement = {};
    int stackedHeight = 0;
    if (IsStackedMeterTaskbar(taskbar, edge, dpi)) {
        int width = 0, height = 0, padding = 0;
        CalculateMeterBox(taskbar, dpi, &width, &height, &padding, 1);   // the stacked box's width
        if (GetStackedFont(dpi, width, &placement.stackedFont, &placement.rowHeight)) {
            stackedHeight = 4 * placement.rowHeight + 2 * ScaleOverlay(2, dpi);
        }
    }
    placement.target = CalculateAnchoredMeterRect(GetSlotLayout(slot, taskbar, edge), dpi,
                                                  static_cast<MeterAnchor>(g_state.anchor), g_state.taskbarOffset,
                                                  stackedHeight);
    return placement;
}

// Draws the meter into a layered window of width x height at destination
// (screen coordinates for a top-level window, parent client coordinates for a
// child): upload over download, as two lines or, with stackedFont, four.
// Returns whether the window was updated.
bool PaintMeter(HWND window, POINT destination, int width, int height, UINT dpi,
                HFONT stackedFont = nullptr, int rowHeight = 0) {
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
    HFONT font = stackedFont ? stackedFont : GetMeterFont(dpi);
    HGDIOBJ oldFont = font ? SelectObject(memory, font) : nullptr;
    SetBkMode(memory, TRANSPARENT);
    SetTextColor(memory, RGB(255, 255, 255));

    int split = height / 2;   // upload colour above, download colour below
    if (stackedFont) {
        // Value over unit for each speed, centred; the font was fitted to the width.
        const int top = (height - 4 * rowHeight) / 2;
        const std::wstring* lines[] = { &g_state.upHead, &g_state.upUnit, &g_state.downHead, &g_state.downUnit };
        for (int i = 0; i < 4; ++i) {
            RECT row = { 0, top + i * rowHeight, width, top + (i + 1) * rowHeight };
            DrawTextW(memory, lines[i]->c_str(), -1, &row, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        split = top + 2 * rowHeight;
    } else {
        int padding = ScaleOverlay(4, dpi);
        if (padding * 2 >= width) padding = 0;
        RECT upRect = { padding, 0, width - padding, split };
        RECT downRect = { padding, split, width - padding, height };
        const UINT textFlags = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
        DrawTextW(memory, g_state.upText.c_str(), -1, &upRect, textFlags);
        DrawTextW(memory, g_state.downText.c_str(), -1, &downRect, textFlags);
    }
    GdiFlush();
    ApplyOverlayAlpha(static_cast<BYTE*>(bits), width, height, stride, split,
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

void RenderOverlay(const MeterSlot& slot) {
    RECT taskbar = {};
    UINT edge = ABE_BOTTOM;
    if (!ShouldShowOverlay(slot, &taskbar, &edge)) {
        if (IsWindowVisible(slot.window)) ShowWindow(slot.window, SW_HIDE);
        return;
    }

    UINT dpi = GetDpiForWindow(slot.window);
    if (dpi == 0) dpi = GetDpiForSystem();
    const MeterPlacement placement = PlaceMeter(slot, taskbar, edge, dpi);
    const RECT& target = placement.target;
    POINT destination = { target.left, target.top };
    if (PaintMeter(slot.window, destination, target.right - target.left, target.bottom - target.top, dpi,
                   placement.stackedFont, placement.rowHeight)) {
        const UINT show = IsWindowVisible(slot.window) ? 0 : SWP_SHOWWINDOW;
        SetWindowPos(slot.window, HWND_TOPMOST, 0, 0, 0, 0,
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
void RenderEmbedded(const MeterSlot& slot) {
    // Lay out against the taskbar window's own rectangle: the child's position is
    // relative to it, so an auto-hiding taskbar carries the meter as it slides.
    RECT taskbar = {};
    UINT edge = ABE_BOTTOM;
    bool placed = false;
    if (slot.primary) {
        RECT docked = {};
        placed = GetTaskbarPosition(&docked, &edge) && GetWindowRect(slot.taskbar, &taskbar);
    } else {
        // A slid-away (auto-hidden) taskbar still places its child; it slides too.
        HMONITOR monitor = nullptr;
        bool shown = false;
        placed = GetSecondaryTaskbar(slot.taskbar, &taskbar, &edge, &monitor, &shown);
    }
    if (!placed) {
        ShowWindow(slot.window, SW_HIDE);
        return;
    }
    UINT dpi = GetDpiForWindow(slot.taskbar);
    if (dpi == 0) dpi = GetDpiForSystem();
    const MeterPlacement placement = PlaceMeter(slot, taskbar, edge, dpi);
    RECT target = placement.target;
    const int width = target.right - target.left;
    const int height = target.bottom - target.top;
    // Two points are mapped as a rectangle, which keeps left < right if the
    // taskbar is mirrored (right-to-left layouts).
    MapWindowPoints(nullptr, slot.taskbar, reinterpret_cast<POINT*>(&target), 2);
    POINT destination = { target.left, target.top };
    if (PaintMeter(slot.window, destination, width, height, dpi, placement.stackedFont, placement.rowHeight)) {
        // Above the taskbar's own child windows (its XAML host covers the whole bar).
        const UINT show = IsWindowVisible(slot.window) ? 0 : SWP_SHOWWINDOW;
        SetWindowPos(slot.window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | show);
    }
}

void RenderSlot(const MeterSlot& slot) {
    if (!slot.window) return;
    if (slot.embedded) {
        RenderEmbedded(slot);
    } else {
        RenderOverlay(slot);
    }
}

// Fullscreen tracking only matters to overlays; embedded meters hide with
// their taskbar.
void RenderOverlays() {
    for (const MeterSlot& slot : g_slots) {
        if (!slot.embedded) RenderSlot(slot);
    }
}

bool HasOverlay() {
    for (const MeterSlot& slot : g_slots) {
        if (slot.window && !slot.embedded) return true;
    }
    return false;
}

void EnsureOverlaysTopmost() {
    for (const MeterSlot& slot : g_slots) {
        if (slot.embedded || !slot.window || !IsWindowVisible(slot.window)) continue;
        RECT taskbar = {};
        UINT edge = ABE_BOTTOM;
        if (!ShouldShowOverlay(slot, &taskbar, &edge)) {
            ShowWindow(slot.window, SW_HIDE);
            continue;
        }
        SetWindowPos(slot.window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

// Serves both kinds of meter window. An embedded one is a child of another
// process's window, and DefWindowProc forwards some messages from a child to its
// parent with SendMessage; those are handled here so the meter thread never
// waits on Explorer.
LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        for (const MeterSlot& slot : g_slots) {
            if (slot.window == hwnd) {
                RenderSlot(slot);
                break;
            }
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

void DestroySlotWindow(MeterSlot& slot) {
    if (slot.window && IsWindow(slot.window)) DestroyWindow(slot.window);
    slot.window = nullptr;
    slot.embedded = false;
}

HWND CreateOverlayWindow(const MeterSlot& slot) {
    // Created on its taskbar's monitor, so it starts with that monitor's DPI.
    RECT taskbar = {};
    UINT edge = ABE_BOTTOM;
    const bool found = slot.primary ? GetTaskbarPosition(&taskbar, &edge)
                                    : (slot.taskbar && GetWindowRect(slot.taskbar, &taskbar));
    if (!found) return nullptr;
    return CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
                           OVERLAY_CLASS, nullptr, WS_POPUP, taskbar.left, taskbar.top, 1, 1,
                           nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
}

// Gives a slot the window the state asks for: a child of its taskbar in
// embedded mode, if that can be created; otherwise a topmost overlay.
void EnsureSlotWindow(MeterSlot& slot) {
    if (slot.window && !IsWindow(slot.window)) {
        // Destroyed along with its taskbar (Explorer restarted).
        slot.window = nullptr;
        slot.embedded = false;
    }
    if (slot.window && slot.embedded && (!g_state.embedded || GetParent(slot.window) != slot.taskbar)) {
        DestroySlotWindow(slot);
    }
    if (g_state.embedded && slot.taskbar && !(slot.window && slot.embedded)) {
        HWND child = CreateWindowExW(WS_EX_LAYERED | WS_EX_NOPARENTNOTIFY, EMBEDDED_CLASS, nullptr,
                                     WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 1, 1, slot.taskbar, nullptr,
                                     GetModuleHandleW(nullptr), nullptr);
        if (child) {
            DestroySlotWindow(slot);
            slot.window = child;
            slot.embedded = true;
        }
    }
    if (!slot.window) slot.window = CreateOverlayWindow(slot);
}

// Creates, swaps or destroys meter windows to match the state and the taskbars
// that exist now, then renders them.
void SyncMeters() {
    if (!g_state.show) {
        for (MeterSlot& slot : g_slots) DestroySlotWindow(slot);
        g_slots.clear();
        return;
    }
    UpdateAppBarRegistration();

    std::vector<HWND> secondaries;
    if (g_state.allTaskbars) {
        for (HWND taskbar = FindWindowExW(nullptr, nullptr, L"Shell_SecondaryTrayWnd", nullptr); taskbar;
             taskbar = FindWindowExW(nullptr, taskbar, L"Shell_SecondaryTrayWnd", nullptr)) {
            if (IsWindowVisible(taskbar)) secondaries.push_back(taskbar);
        }
    }
    for (std::size_t i = 0; i < g_slots.size();) {
        const bool wanted = g_slots[i].primary ||
            std::find(secondaries.begin(), secondaries.end(), g_slots[i].taskbar) != secondaries.end();
        if (wanted) {
            ++i;
        } else {
            DestroySlotWindow(g_slots[i]);
            g_slots.erase(g_slots.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
    if (g_slots.empty() || !g_slots.front().primary) {
        MeterSlot primary;
        primary.primary = true;
        g_slots.insert(g_slots.begin(), primary);
    }
    g_slots.front().taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    for (HWND taskbar : secondaries) {
        const bool known = std::any_of(g_slots.begin(), g_slots.end(), [taskbar](const MeterSlot& slot) {
            return !slot.primary && slot.taskbar == taskbar;
        });
        if (!known) {
            MeterSlot slot;
            slot.taskbar = taskbar;
            g_slots.push_back(slot);
        }
    }

    for (MeterSlot& slot : g_slots) {
        EnsureSlotWindow(slot);
        RenderSlot(slot);
    }
}

// Leading-edge throttle for state-driven renders.
void ScheduleSync() {
    const ULONGLONG now = GetTickCount64();
    const ULONGLONG elapsed = now - g_lastRender;
    if (!g_renderPending && elapsed >= RENDER_INTERVAL_MS) {
        g_lastRender = now;
        SyncMeters();
        return;
    }
    if (!g_renderPending) {
        g_renderPending = true;
        const UINT wait = elapsed >= RENDER_INTERVAL_MS ? 1 : static_cast<UINT>(RENDER_INTERVAL_MS - elapsed);
        SetTimer(g_host, RENDER_TIMER, wait, nullptr);
    }
}

void CALLBACK OnForegroundChanged(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG, LONG, DWORD, DWORD) {
    if (event == EVENT_SYSTEM_FOREGROUND && hwnd && !IsMeterWindow(hwnd)) {
        PostMessageW(g_host, WM_METER_CHECK, 0, 0);
        PostMessageW(g_host, WM_METER_TOPMOST, 0, 0);
    }
}

void CALLBACK OnLocationChanged(HWINEVENTHOOK, DWORD event, HWND hwnd,
                                LONG idObject, LONG idChild, DWORD, DWORD) {
    // Top-level window moves only (not child controls, carets, ...)
    if (event != EVENT_OBJECT_LOCATIONCHANGE) return;
    if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) return;
    if (!hwnd || IsMeterWindow(hwnd) || hwnd == g_notify) return;

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
    RemoveAppBarRegistration();
    for (MeterSlot& slot : g_slots) DestroySlotWindow(slot);
    g_slots.clear();
    ClearFonts();
    ClearStackedFont();
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
            SyncMeters();
        } else if (wp == RAISE_TIMER) {
            EnsureOverlaysTopmost();
            if (--g_raisesLeft <= 0) KillTimer(hwnd, RAISE_TIMER);
        }
        return 0;
    case WM_METER_REFRESH:
        SyncMeters();
        return 0;
    case WM_METER_CHECK:
        RenderOverlays();
        return 0;
    case WM_METER_APPBAR:
        if (wp == ABN_FULLSCREENAPP) {
            OnShellFullscreen(lp != 0);
            RenderOverlays();
        }
        return 0;
    case WM_METER_TOPMOST:
        if (HasOverlay()) {
            EnsureOverlaysTopmost();
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
