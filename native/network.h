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

// Live counter sampler for one adapter index.
// Totals count traffic since Reset(); negative deltas (counter reset) add 0.
struct NetSampler {
    bool valid = false;
    ULONGLONG lastIn = 0, lastOut = 0;
    LARGE_INTEGER lastQpc{};

    ULONGLONG totalIn = 0, totalOut = 0;

    void Reset() { valid = false; totalIn = 0; totalOut = 0; }

    // true if adapter found. outBps = 0 when adapter is down.
    bool Sample(DWORD index, ULONGLONG* outDownBps, ULONGLONG* outUpBps);
};

// Fills adapters with physical-type interfaces (Ethernet/802.11/GigE), any status.
int GetAdapters(AdapterInfo* out, int maxCount);
