#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

// Everything the taskbar meter draws, prepared by the UI thread.
struct MeterState {
    bool show = false;
    std::wstring upText;          // prefixed and formatted, e.g. L"↑  1.50 MB/s"
    std::wstring downText;
    // The stacked meter of narrow vertical taskbars (FormatStackedSpeed):
    // L"↑ 1.50" over L"MB/s", and the widest first line, which sizes the font.
    std::wstring upHead, upUnit, upHeadWidest;
    std::wstring downHead, downUnit, downHeadWidest;
    COLORREF upColor = RGB(255, 255, 255);     // already resolved (no METER_COLOR_AUTO)
    COLORREF downColor = RGB(255, 255, 255);
    std::wstring fontFamily = L"Segoe UI";
    double fontSize = 8.0;
    int fontStyle = 1;
    int anchor = 1;               // MeterAnchor (overlay.h)
    int taskbarOffset = 0;        // logical px from the anchor
    bool embedded = false;        // a child of the taskbar instead of a topmost overlay
    bool allTaskbars = false;     // also on secondary taskbars (other monitors)
};

// The taskbar meter runs on its own thread: it owns the meter windows, renders
// them and follows foreground and fullscreen changes. The UI thread only posts
// state snapshots and never waits on it, so a stall on one side cannot freeze
// the other. Once a meter window is parented to the taskbar, its thread also
// shares input processing with Explorer, so that thread must never block.
//
// openMessage is posted to notifyWindow when the user double-clicks the meter.
bool StartMeterHost(HWND notifyWindow, UINT openMessage);
void PushMeterState(const MeterState& state);   // asynchronous; the latest state wins
void RequestMeterRefresh();                     // taskbar, display or settings changed
void StopMeterHost();
