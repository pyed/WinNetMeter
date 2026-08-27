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

inline RECT CalculateTaskbarOverlayRect(const RECT& taskbar, UINT edge, UINT dpi) {
    const int barWidth = taskbar.right - taskbar.left;
    const int barHeight = taskbar.bottom - taskbar.top;
    const int padding = ScaleOverlay(2, dpi) > 0 ? ScaleOverlay(2, dpi) : 1;
    int width = ScaleOverlay(132, dpi);
    int height = ScaleOverlay(40, dpi);
    if (width > barWidth - 2 * padding) width = barWidth - 2 * padding;
    if (height > barHeight - 2 * padding) height = barHeight - 2 * padding;
    if (width < 1) width = 1;
    if (height < 1) height = 1;

    int x = taskbar.left + (barWidth - width) / 2;
    int y = taskbar.top + (barHeight - height) / 2;
    if (edge == ABE_TOP || edge == ABE_BOTTOM) {
        // ponytail: reserve the notification area without inspecting Explorer children;
        // add a persisted offset only if real taskbar layouts need tuning.
        x = taskbar.right - ScaleOverlay(350, dpi) - width;
        x = ClampOverlay(x, taskbar.left + padding, taskbar.right - padding - width);
    } else {
        y = taskbar.bottom - ScaleOverlay(50, dpi) - height;
        y = ClampOverlay(y, taskbar.top + padding, taskbar.bottom - padding - height);
    }

    return { x, y, x + width, y + height };
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
