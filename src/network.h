#pragma once
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <string>
#include "settings.h"

// Interface types
enum {
    ADAPTER_TYPE_ETHERNET = 6,      // IF_TYPE_ETHERNET_CSMACD
    ADAPTER_TYPE_WIFI = 71,         // IF_TYPE_IEEE80211
    ADAPTER_TYPE_GIGABIT = 117,     // IF_TYPE_GIGABITETHERNET (NDIS 117)
    ADAPTER_TYPE_PPP = 23,          // IF_TYPE_PPP
    ADAPTER_TYPE_VIRTUAL = 53,      // IF_TYPE_PROP_VIRTUAL (WireGuard / VPN)
    ADAPTER_TYPE_TUNNEL = 131,      // IF_TYPE_TUNNEL
    ADAPTER_TYPE_WWANPP = 243,      // IF_TYPE_WWANPP (mobile broadband, GSM)
    ADAPTER_TYPE_WWANPP2 = 244      // IF_TYPE_WWANPP2 (mobile broadband, CDMA)
};

struct AdapterInfo {
    NET_LUID luid;              // Stable 64-bit interface LUID
    DWORD ifIndex;              // Current InterfaceIndex (may change dynamically)
    wchar_t name[128];          // Alias (display name)
    IF_OPER_STATUS status;      // Operational status (e.g. IfOperStatusUp)
    DWORD type;                 // Interface type
};

// Speed formatting. Bytes use binary multiples (1 KB/s = 1024 B/s). With bits,
// rates use decimal multiples (1 Mbps = 1,000,000 bit/s), as ISPs and speed
// tests quote them; minimumUnit then floors at Kbps/Mbps/Gbps.
void FormatSpeed(ULONGLONG bytesPerSecond, MinimumSpeedUnit minimumUnit,
                 int decimalPlaces, wchar_t* out, size_t maxLen, bool bits = false);
void FormatBytes(ULONGLONG bytes, wchar_t* out, size_t maxLen);
void FormatCompact(ULONGLONG bytesPerSecond, wchar_t* out, size_t maxLen, bool bits = false);
void FormatPrefixedSpeed(const wchar_t* prefix, const wchar_t* speed, wchar_t* out, size_t maxLen);
// One direction of the stacked meter (narrow vertical taskbars): prefix and
// value over the unit ("↑ 1.50" over "MB/s"), plus the widest the first line can
// get with this prefix and these decimal places ("↑ 8888.88"), to size the font.
void FormatStackedSpeed(const wchar_t* prefix, const wchar_t* speed, int decimalPlaces,
                        std::wstring* head, std::wstring* unit, std::wstring* widestHead);

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

// The interface Windows would use to reach the internet (default route), IPv4
// first, then IPv6. A routing-table lookup only; nothing is sent.
bool GetDefaultRouteLuid(NET_LUID* luid);

// Picks which listed adapter to meter. Automatic mode follows defaultRoute. A
// manual choice matches by LUID, then by alias when exactly one adapter has it
// (a VPN or virtual adapter can come back with a new LUID). Never falls back to
// an unrelated adapter. Returns the list index, or -1.
int ChooseAdapter(const AdapterInfo* list, int count, bool automatic, NET_LUID defaultRoute,
                  NET_LUID savedLuid, const wchar_t* savedAlias);
