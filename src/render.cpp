#include "render.h"
#include "overlay.h"
#include <cstring>
#include <cwchar>
#include <vector>

int GetTrayIconSizeForDpi(UINT dpi) {
    int size = GetSystemMetricsForDpi(SM_CXSMICON, dpi ? dpi : 96);
    return (size >= 16 && size <= 256) ? size : 16;
}

static HFONT CreateIconFont(int height) {
    return CreateFontW(-height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static int TextWidth(HDC dc, const wchar_t* text) {
    SIZE extent = {};
    GetTextExtentPoint32W(dc, text, static_cast<int>(wcslen(text)), &extent);
    return extent.cx;
}

HICON CreateMeterIcon(int size, const wchar_t* topText, const wchar_t* bottomText,
                      COLORREF topColor, COLORREF bottomColor) {
    if (size < 8 || size > 256 || !topText || !bottomText) return nullptr;

    HDC memory = CreateCompatibleDC(nullptr);
    if (!memory) return nullptr;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP color = CreateDIBSection(memory, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!color || !bits) {
        if (color) DeleteObject(color);
        DeleteDC(memory);
        return nullptr;
    }
    HGDIOBJ oldBitmap = SelectObject(memory, color);
    memset(bits, 0, static_cast<size_t>(size) * static_cast<size_t>(size) * 4);
    SetBkMode(memory, TRANSPARENT);
    SetTextColor(memory, RGB(255, 255, 255));

    // Largest bold font, up to half the icon height, at which both lines fit.
    // Below a third of the height text stops being legible, so clip instead.
    const int minimum = size / 3 > 6 ? size / 3 : 6;
    HFONT font = nullptr;
    for (int height = size / 2 + 1; height >= minimum && !font; --height) {
        HFONT candidate = CreateIconFont(height);
        if (!candidate) break;
        HGDIOBJ previous = SelectObject(memory, candidate);
        int widest = TextWidth(memory, topText);
        int bottomWidth = TextWidth(memory, bottomText);
        if (bottomWidth > widest) widest = bottomWidth;
        SelectObject(memory, previous);
        if (widest <= size || height == minimum) {
            font = candidate;
        } else {
            DeleteObject(candidate);
        }
    }
    HGDIOBJ oldFont = font ? SelectObject(memory, font) : nullptr;
    const UINT flags = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
    RECT top = { 0, 0, size, size / 2 };
    RECT bottom = { 0, size / 2, size, size };
    DrawTextW(memory, topText, -1, &top, flags);
    DrawTextW(memory, bottomText, -1, &bottom, flags);
    GdiFlush();
    if (oldFont) SelectObject(memory, oldFont);
    if (font) DeleteObject(font);
    SelectObject(memory, oldBitmap);

    ApplyIconAlpha(static_cast<BYTE*>(bits), size, size, size * 4, size / 2, topColor, bottomColor);

    // With a 32-bit alpha colour bitmap the mask is not used for drawing, but
    // CreateIconIndirect requires one of the same size.
    std::vector<BYTE> maskBits(static_cast<size_t>((size + 15) / 16) * 2 * static_cast<size_t>(size), 0);
    HBITMAP mask = CreateBitmap(size, size, 1, 1, maskBits.data());
    HICON icon = nullptr;
    if (mask) {
        ICONINFO info = {};
        info.fIcon = TRUE;
        info.hbmMask = mask;
        info.hbmColor = color;
        icon = CreateIconIndirect(&info);
        DeleteObject(mask);
    }
    DeleteObject(color);
    DeleteDC(memory);
    return icon;
}

HFONT CreateFittingFont(const LOGFONTW& base, int minHeight, const std::wstring* lines, std::size_t count,
                        int width, int* rowHeight) {
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return nullptr;
    LOGFONTW font = base;
    int height = base.lfHeight < 0 ? -base.lfHeight : base.lfHeight;
    if (minHeight > height) minHeight = height;
    HFONT fitted = nullptr;
    while (height > 0) {
        font.lfHeight = -height;
        HFONT candidate = CreateFontIndirectW(&font);
        if (!candidate) break;
        HGDIOBJ previous = SelectObject(dc, candidate);
        int widest = 0;
        for (std::size_t i = 0; i < count; ++i) {
            const int lineWidth = TextWidth(dc, lines[i].c_str());
            if (lineWidth > widest) widest = lineWidth;
        }
        TEXTMETRICW metrics = {};
        GetTextMetricsW(dc, &metrics);
        SelectObject(dc, previous);
        if (widest <= width || height <= minHeight) {
            fitted = candidate;
            *rowHeight = metrics.tmHeight;
            break;
        }
        DeleteObject(candidate);
        // Text width is close to proportional to height: jump near the answer,
        // then step down a pixel at a time (hinting makes it not quite linear).
        const int estimate = MulDiv(height, width, widest);
        height = estimate < height ? estimate : height - 1;
        if (height < minHeight) height = minHeight;
    }
    DeleteDC(dc);
    return fitted;
}
