#pragma once
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>

// Adapter types mirrored from .NET NetworkInterfaceType
enum { ADAPTER_ETHERNET = 6, ADAPTER_80211 = 71, ADAPTER_GIGABIT = 32 };

struct AdapterInfo {
    DWORD index;
    wchar_t name[128]; // Alias (display name)
};

// Formatting helpers matching original C# Formatting.cs
void FormatSpeed(ULONGLONG bytesPerSecond, wchar_t* out, size_t maxLen);
void FormatBytes(ULONGLONG bytes, wchar_t* out, size_t maxLen);
void FormatCompact(ULONGLONG bytesPerSecond, wchar_t* out, size_t maxLen);

// Live counter sampler for one adapter index.
// Totals count traffic since Reset(); negative deltas (counter reset) add 0.
struct NetSampler {
    bool valid = false;
    ULONGLONG lastIn = 0, lastOut = 0;
    LARGE_INTEGER lastQpc{};
    ULONGLONG totalIn = 0, totalOut = 0;

    void Reset() {
        valid = false;
        lastIn = 0;
        lastOut = 0;
        lastQpc.QuadPart = 0;
        totalIn = 0;
        totalOut = 0;
    }

    // Live sampling via GetIfTable2
    bool Sample(DWORD index, ULONGLONG* outDownBps, ULONGLONG* outUpBps);

    // Mock update logic for deterministic testing
    void UpdateMock(ULONGLONG inBytes, ULONGLONG outBytes, LARGE_INTEGER now,
                    LARGE_INTEGER freq, IF_OPER_STATUS operStatus,
                    ULONGLONG* outDownBps, ULONGLONG* outUpBps);
};

// Fills adapters with physical-type interfaces (Ethernet/802.11/GigE).
int GetAdapters(AdapterInfo* out, int maxCount);
