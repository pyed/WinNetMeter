#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <cstddef>

inline int ScaleOverlay(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi ? dpi : 96), 96);
}

inline int ClampOverlay(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

inline bool IsWindowRectFullscreen(const RECT& wnd, const RECT& monitor, int tolerance = 1) {
    return wnd.left <= monitor.left + tolerance &&
           wnd.top <= monitor.top + tolerance &&
           wnd.right >= monitor.right - tolerance &&
           wnd.bottom >= monitor.bottom - tolerance;
}

// The meter box: 132 x 40 logical px, shrunk to fit inside the taskbar. A
// stacked meter (stackedHeight > 0, see IsStackedMeterTaskbar) is instead as
// wide as the taskbar allows and stackedHeight physical px tall.
inline void CalculateMeterBox(const RECT& taskbar, UINT dpi, int* width, int* height, int* padding,
                              int stackedHeight = 0) {
    const int barWidth = taskbar.right - taskbar.left;
    const int barHeight = taskbar.bottom - taskbar.top;
    *padding = ScaleOverlay(2, dpi) > 0 ? ScaleOverlay(2, dpi) : 1;
    *width = stackedHeight > 0 ? barWidth : ScaleOverlay(132, dpi);
    *height = stackedHeight > 0 ? stackedHeight : ScaleOverlay(40, dpi);
    if (*width > barWidth - 2 * *padding) *width = barWidth - 2 * *padding;
    if (*height > barHeight - 2 * *padding) *height = barHeight - 2 * *padding;
    if (*width < 1) *width = 1;
    if (*height < 1) *height = 1;
}

// Vertical taskbars narrower than the 132 px meter get a stacked meter: as wide
// as the taskbar, with each speed's value and unit on separate lines.
inline bool IsStackedMeterTaskbar(const RECT& taskbar, UINT edge, UINT dpi) {
    if (edge != ABE_LEFT && edge != ABE_RIGHT) return false;
    int width = 0, height = 0, padding = 0;
    CalculateMeterBox(taskbar, dpi, &width, &height, &padding);
    return width < ScaleOverlay(132, dpi);
}

inline RECT CalculateTaskbarOverlayRect(const RECT& taskbar, UINT edge, UINT dpi, int logicalOffset = 0,
                                        int stackedHeight = 0) {
    const int barWidth = taskbar.right - taskbar.left;
    const int barHeight = taskbar.bottom - taskbar.top;
    int width = 0, height = 0, padding = 0;
    CalculateMeterBox(taskbar, dpi, &width, &height, &padding, stackedHeight);

    int x = taskbar.left + (barWidth - width) / 2;
    int y = taskbar.top + (barHeight - height) / 2;
    if (edge == ABE_TOP || edge == ABE_BOTTOM) {
        // ponytail: documented APIs expose no free taskbar slot; the persisted
        // logical offset is the user-controlled escape hatch for real layouts.
        x = taskbar.right - ScaleOverlay(350, dpi) - width + ScaleOverlay(logicalOffset, dpi);
        x = ClampOverlay(x, taskbar.left + padding, taskbar.right - padding - width);
    } else {
        y = taskbar.bottom - ScaleOverlay(50, dpi) - height + ScaleOverlay(logicalOffset, dpi);
        y = ClampOverlay(y, taskbar.top + padding, taskbar.bottom - padding - height);
    }

    return { x, y, x + width, y + height };
}

// Where the meter sits on the taskbar. Legacy is the 0.1.x fixed point (350
// logical px in from the far end), kept for files written by those releases so
// an upgrade does not move anyone's meter. Values match AppSettings::meterAnchor.
enum class MeterAnchor { Legacy = 0, BesideTray = 1, AfterApps = 2, LeftEdge = 3 };

// Taskbar geometry the anchors refer to. tray and apps are empty when unknown
// (e.g. a secondary taskbar without a notification area).
struct TaskbarLayout {
    RECT taskbar;
    UINT edge;
    RECT tray;   // notification area (TrayNotifyWnd)
    RECT apps;   // task buttons (MSTaskSwWClass)
};

inline bool IsUsableTaskbarPart(const RECT& part, const RECT& taskbar) {
    RECT overlap = {};
    return part.right > part.left && part.bottom > part.top && IntersectRect(&overlap, &part, &taskbar);
}

// Meter rectangle for an anchor. logicalOffset moves it along the taskbar
// (right on horizontal taskbars, down on vertical ones), relative to the anchor.
inline RECT CalculateAnchoredMeterRect(const TaskbarLayout& layout, UINT dpi, MeterAnchor anchor, int logicalOffset,
                                       int stackedHeight = 0) {
    if (anchor == MeterAnchor::Legacy) {
        return CalculateTaskbarOverlayRect(layout.taskbar, layout.edge, dpi, logicalOffset, stackedHeight);
    }
    const RECT& taskbar = layout.taskbar;
    int width = 0, height = 0, padding = 0;
    CalculateMeterBox(taskbar, dpi, &width, &height, &padding, stackedHeight);
    const int gap = ScaleOverlay(4, dpi);
    const int offset = ScaleOverlay(logicalOffset, dpi);
    const bool hasTray = IsUsableTaskbarPart(layout.tray, taskbar);
    const bool hasApps = IsUsableTaskbarPart(layout.apps, taskbar);

    int x = taskbar.left + (taskbar.right - taskbar.left - width) / 2;
    int y = taskbar.top + (taskbar.bottom - taskbar.top - height) / 2;
    if (layout.edge == ABE_TOP || layout.edge == ABE_BOTTOM) {
        if (anchor == MeterAnchor::BesideTray) {
            x = (hasTray ? layout.tray.left : taskbar.right) - gap - width;
        } else if (anchor == MeterAnchor::AfterApps && hasApps) {
            x = layout.apps.right + gap;
        } else {
            x = taskbar.left + gap;
        }
        x = ClampOverlay(x + offset, taskbar.left + padding, taskbar.right - padding - width);
    } else {
        if (anchor == MeterAnchor::BesideTray) {
            y = (hasTray ? layout.tray.top : taskbar.bottom) - gap - height;
        } else if (anchor == MeterAnchor::AfterApps && hasApps) {
            y = layout.apps.bottom + gap;
        } else {
            y = taskbar.top + gap;
        }
        y = ClampOverlay(y + offset, taskbar.top + padding, taskbar.bottom - padding - height);
    }
    return { x, y, x + width, y + height };
}

// The edge a taskbar is docked to, from its rectangle and its monitor's
// (secondary taskbars have no appbar message to ask). An auto-hidden taskbar
// that has slid off its edge still resolves to that edge.
inline UINT TaskbarEdgeOnMonitor(const RECT& taskbar, const RECT& monitor) {
    if (taskbar.right - taskbar.left >= taskbar.bottom - taskbar.top) {
        return taskbar.top - monitor.top < monitor.bottom - taskbar.bottom ? ABE_TOP : ABE_BOTTOM;
    }
    return taskbar.left - monitor.left < monitor.right - taskbar.right ? ABE_LEFT : ABE_RIGHT;
}

// Whether at least half of a taskbar's thickness is on its monitor; an
// auto-hidden one leaves only a sliver.
inline bool IsTaskbarMostlyOnMonitor(const RECT& taskbar, UINT edge, const RECT& monitor) {
    RECT visible = {};
    if (!IntersectRect(&visible, &taskbar, &monitor)) return false;
    const bool horizontal = edge == ABE_TOP || edge == ABE_BOTTOM;
    const int thickness = horizontal ? taskbar.bottom - taskbar.top : taskbar.right - taskbar.left;
    const int shown = horizontal ? visible.bottom - visible.top : visible.right - visible.left;
    return shown * 2 >= thickness;
}

// Secondary taskbars have no notification-area window (on Windows 11 their
// clock has none either), but the meter must not cover their far end. Reserve
// the same logical length there as the primary's notification area takes, so
// "next to the tray" sits the same distance from the end on every taskbar.
// Empty when the primary's notification area is unknown.
inline RECT MirrorTrayArea(const TaskbarLayout& primary, UINT primaryDpi,
                           const RECT& taskbar, UINT edge, UINT dpi) {
    RECT tray = {};
    if (!IsUsableTaskbarPart(primary.tray, primary.taskbar)) return tray;
    const bool primaryHorizontal = primary.edge == ABE_TOP || primary.edge == ABE_BOTTOM;
    const int primaryLength = primaryHorizontal ? primary.taskbar.right - primary.tray.left
                                                : primary.taskbar.bottom - primary.tray.top;
    const int length = MulDiv(primaryLength, static_cast<int>(dpi ? dpi : 96),
                              static_cast<int>(primaryDpi ? primaryDpi : 96));
    if (length <= 0) return tray;
    tray = taskbar;
    if (edge == ABE_TOP || edge == ABE_BOTTOM) {
        tray.left = taskbar.right - length;
    } else {
        tray.top = taskbar.bottom - length;
    }
    return tray;
}

// Icons take straight (not premultiplied) alpha, unlike UpdateLayeredWindow:
// a 50% white pixel drawn over black must come out mid-grey (measured with
// DrawIconEx; premultiplying here would halve every edge pixel again). The
// input is white-on-black text; colour channels get the full text colour and
// alpha carries the coverage.
inline void ApplyIconAlpha(BYTE* pixels, int width, int height, int stride,
                           int splitY, COLORREF topColor, COLORREF bottomColor) {
    for (int y = 0; y < height; ++y) {
        const COLORREF color = y < splitY ? topColor : bottomColor;
        BYTE* row = pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
        for (int x = 0; x < width; ++x) {
            BYTE* pixel = row + x * 4;
            BYTE coverage = pixel[0];
            if (pixel[1] > coverage) coverage = pixel[1];
            if (pixel[2] > coverage) coverage = pixel[2];
            pixel[0] = coverage ? GetBValue(color) : 0;
            pixel[1] = coverage ? GetGValue(color) : 0;
            pixel[2] = coverage ? GetRValue(color) : 0;
            pixel[3] = coverage;
        }
    }
}

inline void ApplyOverlayAlpha(BYTE* pixels, int width, int height, int stride,
                              int splitY, COLORREF topColor, COLORREF bottomColor) {
    for (int y = 0; y < height; ++y) {
        const COLORREF color = y < splitY ? topColor : bottomColor;
        BYTE* row = pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
        for (int x = 0; x < width; ++x) {
            BYTE* pixel = row + x * 4;
            BYTE coverage = pixel[0];
            if (pixel[1] > coverage) coverage = pixel[1];
            if (pixel[2] > coverage) coverage = pixel[2];
            pixel[0] = static_cast<BYTE>((static_cast<unsigned>(GetBValue(color)) * coverage) / 255U);
            pixel[1] = static_cast<BYTE>((static_cast<unsigned>(GetGValue(color)) * coverage) / 255U);
            pixel[2] = static_cast<BYTE>((static_cast<unsigned>(GetRValue(color)) * coverage) / 255U);
            pixel[3] = coverage;
        }
    }
}
