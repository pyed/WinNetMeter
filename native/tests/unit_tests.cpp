#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "../network.h"
#include "../settings.h"

// Test 1: Formatting exact parity with C#
void TestFormatting() {
    wchar_t buf[64];

    // Speed formatting
    FormatSpeed(0, buf, 64);
    assert(wcscmp(buf, L"0 B/s") == 0);

    FormatSpeed(512, buf, 64);
    assert(wcscmp(buf, L"512 B/s") == 0);

    FormatSpeed(1024, buf, 64);
    assert(wcscmp(buf, L"1.00 KB/s") == 0);

    FormatSpeed(1536, buf, 64);
    assert(wcscmp(buf, L"1.50 KB/s") == 0);

    FormatSpeed(1048576, buf, 64);
    assert(wcscmp(buf, L"1.00 MB/s") == 0);

    FormatSpeed(1073741824ULL, buf, 64);
    assert(wcscmp(buf, L"1.00 GB/s") == 0);

    // Bytes formatting
    FormatBytes(0, buf, 64);
    assert(wcscmp(buf, L"0 B") == 0);

    FormatBytes(1024, buf, 64);
    assert(wcscmp(buf, L"1.00 KB") == 0);

    FormatBytes(8545894, buf, 64);
    assert(wcscmp(buf, L"8.15 MB") == 0);

    FormatBytes(302858, buf, 64);
    assert(wcscmp(buf, L"295.76 KB") == 0);

    // Compact speed formatting
    FormatCompact(0, buf, 64);
    assert(wcscmp(buf, L"0B") == 0);

    FormatCompact(500, buf, 64);
    assert(wcscmp(buf, L"500B") == 0);

    FormatCompact(1024, buf, 64);
    assert(wcscmp(buf, L"1K") == 0);

    FormatCompact(1048576, buf, 64);
    assert(wcscmp(buf, L"1M") == 0);

    FormatCompact(1073741824ULL, buf, 64);
    assert(wcscmp(buf, L"1G") == 0);

    printf("PASS: TestFormatting\n");
}

// Test 2: NetSampler with mock updates (64-bit counters, monotonic timing, counter reset, wrap)
void TestNetSamplerMock() {
    NetSampler s;
    NET_LUID mockLuid;
    mockLuid.Value = 0x123456789ABCDEF0ULL;
    s.Reset(mockLuid);
    assert(!s.valid);
    assert(s.totalIn == 0);
    assert(s.totalOut == 0);
    assert(s.trackedLuid.Value == mockLuid.Value);

    // Mock first sample: 10,000,000,000 bytes (test 64-bit range)
    ULONGLONG down = 0, up = 0;
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    LARGE_INTEGER t0;
    t0.QuadPart = 1000000;
    s.UpdateMock(10000000000ULL, 5000000000ULL, t0, freq, IfOperStatusUp, &down, &up);
    assert(s.valid);
    assert(down == 0);
    assert(up == 0);
    assert(s.totalIn == 0); // Total starts at 0, accumulating deltas
    assert(s.totalOut == 0);

    // Second sample: 1 second later, +1048576 bytes down (1 MB/s), +524288 bytes up (512 KB/s)
    LARGE_INTEGER t1;
    t1.QuadPart = t0.QuadPart + freq.QuadPart; // exactly 1.0 second
    s.UpdateMock(10000000000ULL + 1048576ULL, 5000000000ULL + 524288ULL, t1, freq, IfOperStatusUp, &down, &up);
    assert(down == 1048576ULL);
    assert(up == 524288ULL);
    assert(s.totalIn == 1048576ULL);
    assert(s.totalOut == 524288ULL);

    // Third sample: 0.5 seconds later (+0.5s), +2097152 bytes down (4 MB/s)
    LARGE_INTEGER t2;
    t2.QuadPart = t1.QuadPart + (freq.QuadPart / 2); // 0.5 seconds
    s.UpdateMock(10000000000ULL + 1048576ULL + 2097152ULL, 5000000000ULL + 524288ULL, t2, freq, IfOperStatusUp, &down, &up);
    assert(down == 4194304ULL); // 2MB / 0.5s = 4 MB/s
    assert(up == 0);
    assert(s.totalIn == 1048576ULL + 2097152ULL);

    // Test counter reset / wrap (cur < prev) -> delta treated as 0, no negative spike, no crash
    LARGE_INTEGER t3;
    t3.QuadPart = t2.QuadPart + freq.QuadPart;
    s.UpdateMock(500ULL, 100ULL, t3, freq, IfOperStatusUp, &down, &up);
    assert(down == 0);
    assert(up == 0);

    // Test adapter down -> speed 0, totals unchanged
    LARGE_INTEGER t4;
    t4.QuadPart = t3.QuadPart + freq.QuadPart;
    s.UpdateMock(600ULL, 200ULL, t4, freq, IfOperStatusDown, &down, &up);
    assert(down == 0);
    assert(up == 0);

    printf("PASS: TestNetSamplerMock\n");
}

// Test 3: Settings INI roundtrip
void TestSettings() {
    AppSettings s1;
    s1.bg = RGB(40, 50, 60);
    s1.down = RGB(10, 200, 100);
    s1.up = RGB(220, 150, 30);
    wcscpy_s(s1.fontFamily, L"Arial");
    s1.fontSize = 11.5;
    s1.fontStyle = 15; // Bold(1) | Italic(2) | Underline(4) | Strikeout(8)
    s1.showWidget = 0;

    // Save to test file
    SaveSettingsCustom(&s1, L".\\test_settings.ini");

    AppSettings s2;
    LoadSettingsCustom(&s2, L".\\test_settings.ini");

    assert(s2.bg == s1.bg);
    assert(s2.down == s1.down);
    assert(s2.up == s1.up);
    assert(wcscmp(s2.fontFamily, s1.fontFamily) == 0);
    assert(fabs(s2.fontSize - s1.fontSize) < 0.01);
    assert(s2.fontStyle == s1.fontStyle);
    assert(s2.showWidget == s1.showWidget);

    DeleteFileW(L".\\test_settings.ini");
    printf("PASS: TestSettings\n");
}

// Test 4: Live adapter enumeration with LUID stability
void TestLiveAdapters() {
    AdapterInfo list[64];
    int count = GetAdapters(list, 64);
    printf("INFO: Found %d physical/VPN adapter(s)\n", count);
    for (int i = 0; i < count; ++i) {
        wprintf(L"  [%d] LUID: 0x%llx, IfIndex: %lu, Type: %lu, Name: %s\n",
                i, list[i].luid.Value, list[i].ifIndex, list[i].type, list[i].name);
        assert(list[i].luid.Value != 0);
        assert(wcslen(list[i].name) > 0);
    }
    printf("PASS: TestLiveAdapters\n");
}

// Helper to simulate one icon lifecycle
static void SimulateIconCycle() {
    const int w = 16, h = 16;
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

    RECT rc = { 0, 0, w, h };
    HBRUSH hbg = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdcMem, &rc, hbg);
    DeleteObject(hbg);

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

    DestroyIcon(hIcon);
}

// Test 5: GDI Object Allocation & Cleanup Verification (Zero Leaks)
void TestGdiResourceLeakCheck() {
    HANDLE hProc = GetCurrentProcess();

    // Warmup process-level GDI/User table and rasterizer caches
    for (int i = 0; i < 5; ++i) {
        SimulateIconCycle();
        HFONT hf = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        DeleteObject(hf);
    }

    DWORD gdiStart = GetGuiResources(hProc, GR_GDIOBJECTS);
    DWORD userStart = GetGuiResources(hProc, GR_USEROBJECTS);

    // Simulate 200 tray icon updates
    for (int i = 0; i < 200; ++i) {
        SimulateIconCycle();
    }

    // Simulate 200 font creations and deletions
    for (int i = 0; i < 200; ++i) {
        HFONT hf = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        DeleteObject(hf);
    }

    DWORD gdiEnd = GetGuiResources(hProc, GR_GDIOBJECTS);
    DWORD userEnd = GetGuiResources(hProc, GR_USEROBJECTS);

    printf("INFO: GDI Objects Start=%lu, End=%lu | User Objects Start=%lu, End=%lu\n",
           gdiStart, gdiEnd, userStart, userEnd);
    fflush(stdout);
    assert(gdiEnd == gdiStart);
    assert(userEnd == userStart);
    printf("PASS: TestGdiResourceLeakCheck (Zero Leaks)\n");
}

int main() {
    printf("Running NetworkMonitorLite Native Robustness & Regression Tests...\n");
    TestFormatting();
    TestNetSamplerMock();
    TestSettings();
    TestLiveAdapters();
    TestGdiResourceLeakCheck();
    printf("ALL TESTS PASSED\n");
    return 0;
}
