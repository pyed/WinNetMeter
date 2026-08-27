#pragma once
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>

// Interface types
enum {
    ADAPTER_TYPE_ETHERNET = 6,      // IF_TYPE_ETHERNET_CSMACD
    ADAPTER_TYPE_WIFI = 71,         // IF_TYPE_IEEE80211
    ADAPTER_TYPE_GIGABIT = 117,     // IF_TYPE_GIGABITETHERNET (NDIS 117)
    ADAPTER_TYPE_PPP = 23,          // IF_TYPE_PPP
    ADAPTER_TYPE_VIRTUAL = 53,      // IF_TYPE_PROP_VIRTUAL (WireGuard / VPN)
    ADAPTER_TYPE_TUNNEL = 131       // IF_TYPE_TUNNEL
};

struct AdapterInfo {
    NET_LUID luid;              // Stable 64-bit interface LUID
    DWORD ifIndex;              // Current InterfaceIndex (may change dynamically)
    wchar_t name[128];          // Alias (display name)
    IF_OPER_STATUS status;      // Operational status (e.g. IfOperStatusUp)
    DWORD type;                 // Interface type
};

// Formatting utilities matching C# Formatting.cs
void FormatSpeed(ULONGLONG bytesPerSecond, wchar_t* out, size_t maxLen);
void FormatBytes(ULONGLONG bytes, wchar_t* out, size_t maxLen);
void FormatCompact(ULONGLONG bytesPerSecond, wchar_t* out, size_t maxLen);

// Live counter sampler keyed by stable NET_LUID.
struct NetSampler {
    bool valid = false;
    NET_LUID trackedLuid{};
    ULONGLONG lastIn = 0, lastOut = 0;
    LARGE_INTEGER lastQpc{};
    ULONGLONG totalIn = 0, totalOut = 0;

    void Reset(NET_LUID luid) {
        valid = false;
        trackedLuid = luid;
        lastIn = 0;
        lastOut = 0;
        lastQpc.QuadPart = 0;
        totalIn = 0;
        totalOut = 0;
    }

    void Rebind(NET_LUID luid) {
        // Rebaseline for new LUID while preserving accumulated totals across reconnect
        valid = false;
        trackedLuid = luid;
        lastIn = 0;
        lastOut = 0;
        lastQpc.QuadPart = 0;
    }

    void Clear() {
        valid = false;
        trackedLuid.Value = 0;
        lastIn = 0;
        lastOut = 0;
        lastQpc.QuadPart = 0;
        totalIn = 0;
        totalOut = 0;
    }

    // Samples live interface via stable NET_LUID
    bool Sample(NET_LUID luid, ULONGLONG* outDownBps, ULONGLONG* outUpBps);

    // Mock update logic for deterministic testing
    void UpdateMock(ULONGLONG inBytes, ULONGLONG outBytes, LARGE_INTEGER now,
                    LARGE_INTEGER freq, IF_OPER_STATUS operStatus,
                    ULONGLONG* outDownBps, ULONGLONG* outUpBps);
};

// Enumerates physical and VPN/virtual interfaces
int GetAdapters(AdapterInfo* out, int maxCount);
