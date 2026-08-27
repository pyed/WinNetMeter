#include "network.h"
#include <stdlib.h>

// Fills `row` from the live table; false if index not present.
// SDK 10.0.26100 user-mode GetIfTable2: one arg, caller-freeable via FreeMibTable.
static bool FetchRow(DWORD index, MIB_IF_ROW2* row) {
    MIB_IF_TABLE2* table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR || !table)
        return false;
    bool found = false;
    for (DWORD i = 0; i < table->NumEntries; ++i)
        if (table->Table[i].InterfaceIndex == index) {
            *row = table->Table[i];
            found = true;
            break;
        }
    FreeMibTable(table);
    return found;
}

int GetAdapters(AdapterInfo* out, int maxCount) {
    MIB_IF_TABLE2* table = nullptr;
    int count = 0;
    if (GetIfTable2(&table) == NO_ERROR && table) {
        for (DWORD i = 0; i < table->NumEntries && count < maxCount; ++i) {
            const auto& r = table->Table[i];
            if (r.Type == ADAPTER_ETHERNET || r.Type == ADAPTER_80211 || r.Type == ADAPTER_GIGABIT) {
                out[count].index = r.InterfaceIndex;
                wcsncpy_s(out[count].name, _countof(out[count].name), r.Alias, _TRUNCATE);
                ++count;
            }
        }
        FreeMibTable(table);
    }
    return count;
}

static LARGE_INTEGER qpcFreq() {
    static LARGE_INTEGER f = [] { QueryPerformanceFrequency(&f); return f; }();
    return f;
}

bool NetSampler::Sample(DWORD index, ULONGLONG* outDownBps, ULONGLONG* outUpBps) {
    MIB_IF_ROW2 row;
    if (!FetchRow(index, &row))
        return false;
    *outDownBps = *outUpBps = 0;
    if (row.OperStatus != IfOperStatusUp)
        return true; // present but down: zero speed, totals unchanged

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    ULONGLONG inB = row.InOctets, outB = row.OutOctets;

    if (!valid) {
        lastIn = inB;
        lastOut = outB;
        totalIn = inB;   // C# shows the adapter's cumulative totals at selection time
        totalOut = outB;
        lastQpc = now;
        valid = true;
        return true;
    }

    double secs = static_cast<double>(now.QuadPart - lastQpc.QuadPart) / static_cast<double>(qpcFreq().QuadPart);
    if (secs < 0.001)
        secs = 0.001;
    ULONGLONG dIn = inB >= lastIn ? inB - lastIn : 0; // counter reset/wrap -> 0
    ULONGLONG dOut = outB >= lastOut ? outB - lastOut : 0;
    lastIn = inB;
    lastOut = outB;
    lastQpc = now;
    totalIn += dIn;
    totalOut += dOut;
    *outDownBps = static_cast<ULONGLONG>(dIn / secs);
    *outUpBps = static_cast<ULONGLONG>(dOut / secs);
    return true;
}
