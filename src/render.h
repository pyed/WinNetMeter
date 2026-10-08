#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Notification-area icon size the shell uses at a DPI (SM_CXSMICON): 16 px at
// 100%, 48 px at 300%. An icon of any other size is rescaled and blurs.
int GetTrayIconSizeForDpi(UINT dpi);

// Two-line speed icon (top line over bottom line) on a transparent background,
// using the largest bold font that fits. Caller owns the HICON; nullptr on failure.
HICON CreateMeterIcon(int size, const wchar_t* topText, const wchar_t* bottomText,
                      COLORREF topColor, COLORREF bottomColor);
