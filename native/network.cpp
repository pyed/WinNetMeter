#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <cwchar>

void FormatSpeed(ULONGLONG bytesPerSecond, MinimumSpeedUnit minimumUnit,
                 int decimalPlaces, wchar_t* out, size_t maxLen) {
    int unit = 0;
    if (bytesPerSecond >= 1024ULL * 1024 * 1024) {
        unit = 3;
    } else if (bytesPerSecond >= 1024ULL * 1024) {
        unit = 2;
    } else if (bytesPerSecond >= 1024) {
        unit = 1;
    }

    int floor = static_cast<int>(minimumUnit);
    if (floor < 0 || floor > 3) floor = 0;
    if (floor > unit) unit = floor;

    if (unit == 0) {
        swprintf_s(out, maxLen, L"%llu B/s", static_cast<unsigned long long>(bytesPerSecond));
        return;
    }

    static constexpr double divisors[] = { 1.0, 1024.0, 1024.0 * 1024.0,
                                           1024.0 * 1024.0 * 1024.0 };
    static constexpr const wchar_t* suffixes[] = { L"B/s", L"KB/s", L"MB/s", L"GB/s" };
    int precision = ClampSpeedDecimalPlaces(decimalPlaces);
    double scale = precision == 0 ? 1.0 : (precision == 1 ? 10.0 : 100.0);
    double value = static_cast<double>(bytesPerSecond) / divisors[unit];
    value = std::round(value * scale) / scale;
    swprintf_s(out, maxLen, L"%.*f %s", precision, value, suffixes[unit]);
}

void FormatBytes(ULONGLONG bytes, wchar_t* out, size_t maxLen) {
    if (bytes < 1024) {
        swprintf_s(out, maxLen, L"%llu B", static_cast<unsigned long long>(bytes));
    } else if (bytes < 1024ULL * 1024) {
        swprintf_s(out, maxLen, L"%.2f KB", static_cast<double>(bytes) / 1024.0);
    } else if (bytes < 1024ULL * 1024 * 1024) {
        swprintf_s(out, maxLen, L"%.2f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else {
        swprintf_s(out, maxLen, L"%.2f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    }
}

void FormatCompact(ULONGLONG bytesPerSecond, wchar_t* out, size_t maxLen) {
    if (bytesPerSecond < 1024) {
        swprintf_s(out, maxLen, L"%lluB", static_cast<unsigned long long>(bytesPerSecond));
    } else if (bytesPerSecond < 1024ULL * 1024) {
        swprintf_s(out, maxLen, L"%lluK", static_cast<unsigned long long>(bytesPerSecond / 1024ULL));
    } else if (bytesPerSecond < 1024ULL * 1024 * 1024) {
        swprintf_s(out, maxLen, L"%lluM", static_cast<unsigned long long>(bytesPerSecond / (1024ULL * 1024ULL)));
    } else {
        swprintf_s(out, maxLen, L"%lluG", static_cast<unsigned long long>(bytesPerSecond / (1024ULL * 1024ULL * 1024ULL)));
    }
}

// Retrieves live row by stable NET_LUID
static bool FetchRowByLuid(NET_LUID luid, MIB_IF_ROW2* row) {
    if (luid.Value == 0) return false;
    memset(row, 0, sizeof(MIB_IF_ROW2));
    row->InterfaceLuid = luid;
    if (GetIfEntry2(row) == NO_ERROR) {
        return !row->InterfaceAndOperStatusFlags.FilterInterface;
    }
    return false;
}

int GetAdapters(AdapterInfo* out, int maxCount) {
    MIB_IF_TABLE2* table = nullptr;
    int count = 0;
    if (GetIfTable2(&table) == NO_ERROR && table) {
        for (DWORD i = 0; i < table->NumEntries && count < maxCount; ++i) {
            const auto& r = table->Table[i];
            if (r.InterfaceAndOperStatusFlags.FilterInterface) {
                continue;
            }
            bool isSupported = (r.Type == ADAPTER_TYPE_ETHERNET ||
                                r.Type == ADAPTER_TYPE_WIFI ||
                                r.Type == ADAPTER_TYPE_GIGABIT ||
                                r.Type == ADAPTER_TYPE_PPP ||
                                r.Type == ADAPTER_TYPE_VIRTUAL ||
                                r.Type == ADAPTER_TYPE_TUNNEL);
            if (isSupported) {
                out[count].luid = r.InterfaceLuid;
                out[count].ifIndex = r.InterfaceIndex;
                out[count].status = r.OperStatus;
                out[count].type = r.Type;
                wcsncpy_s(out[count].name, _countof(out[count].name), r.Alias, _TRUNCATE);
                ++count;
            }
        }
        FreeMibTable(table);
    }
    return count;
}

static LARGE_INTEGER GetQpcFrequency() {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return f;
}

void NetSampler::UpdateMock(ULONGLONG inBytes, ULONGLONG outBytes, LARGE_INTEGER now,
                            LARGE_INTEGER freq, IF_OPER_STATUS operStatus,
                            ULONGLONG* outDownBps, ULONGLONG* outUpBps) {
    *outDownBps = 0;
    *outUpBps = 0;

    if (operStatus != IfOperStatusUp) {
        return;
    }

    if (!valid) {
        lastIn = inBytes;
        lastOut = outBytes;
        lastQpc = now;
        valid = true;
        return;
    }

    double secs = static_cast<double>(now.QuadPart - lastQpc.QuadPart) / static_cast<double>(freq.QuadPart);
    if (secs < 0.001) {
        secs = 0.001;
    }

    ULONGLONG dIn = (inBytes >= lastIn) ? (inBytes - lastIn) : 0;
    ULONGLONG dOut = (outBytes >= lastOut) ? (outBytes - lastOut) : 0;

    lastIn = inBytes;
    lastOut = outBytes;
    lastQpc = now;

    totalIn += dIn;
    totalOut += dOut;

    *outDownBps = static_cast<ULONGLONG>(static_cast<double>(dIn) / secs);
    *outUpBps = static_cast<ULONGLONG>(static_cast<double>(dOut) / secs);
}

bool NetSampler::Sample(NET_LUID luid, ULONGLONG* outDownBps, ULONGLONG* outUpBps) {
    MIB_IF_ROW2 row;
    if (!FetchRowByLuid(luid, &row)) {
        *outDownBps = 0;
        *outUpBps = 0;
        return false;
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    LARGE_INTEGER freq = GetQpcFrequency();

    UpdateMock(row.InOctets, row.OutOctets, now, freq, row.OperStatus, outDownBps, outUpBps);
    return true;
}
