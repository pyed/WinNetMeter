#include "settings.h"
#include <shlobj.h>
#include <stdio.h>
#include <cerrno>
#include <cwchar>
#include <climits>
#include <string>

static const wchar_t RUN_KEY[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t RUN_VALUE[] = L"WinNetMeter";

static std::wstring GetStartupCommand() {
    std::wstring path(32768, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length == path.size()) return {};
    path.resize(length);
    return L"\"" + path + L"\"";
}

static std::wstring GetDefaultSettingsPath() {
    wchar_t appdata[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata) == S_OK) {
        return std::wstring(appdata) + L"\\WinNetMeter\\settings.ini";
    }
    return L"settings.ini";
}

static bool ParseInteger(const wchar_t* text, long* value) {
    if (!text || !value) return false;
    wchar_t* end = nullptr;
    errno = 0;
    long parsed = wcstol(text, &end, 10);
    while (end && (*end == L' ' || *end == L'\t')) ++end;
    if (end == text || !end || *end != L'\0' || errno == ERANGE) return false;
    *value = parsed;
    return true;
}

static bool ParseUnsigned64(const wchar_t* text, ULONGLONG* value) {
    if (!text || !value) return false;
    while (*text == L' ' || *text == L'\t') ++text;
    if (*text == L'-') return false;
    wchar_t* end = nullptr;
    errno = 0;
    unsigned long long parsed = wcstoull(text, &end, 10);
    while (end && (*end == L' ' || *end == L'\t')) ++end;
    if (end == text || !end || *end != L'\0' || errno == ERANGE) return false;
    *value = static_cast<ULONGLONG>(parsed);
    return true;
}

static int HexDigit(wchar_t c) {
    if (c >= L'0' && c <= L'9') return c - L'0';
    if (c >= L'A' && c <= L'F') return c - L'A' + 10;
    if (c >= L'a' && c <= L'f') return c - L'a' + 10;
    return -1;
}

static bool DecodePrefix(const wchar_t* encoded, wchar_t* out, size_t capacity) {
    if (!encoded || encoded[0] != L'x' || !out || capacity == 0) return false;
    size_t digits = wcslen(encoded + 1);
    if (digits % 4 != 0 || digits / 4 >= capacity) return false;
    for (size_t i = 0; i < digits / 4; ++i) {
        unsigned value = 0;
        for (size_t j = 0; j < 4; ++j) {
            int digit = HexDigit(encoded[1 + i * 4 + j]);
            if (digit < 0) return false;
            value = value * 16 + static_cast<unsigned>(digit);
        }
        out[i] = static_cast<wchar_t>(value);
    }
    out[digits / 4] = L'\0';
    return true;
}

static std::wstring EncodePrefix(const wchar_t* prefix) {
    std::wstring encoded = L"x";
    wchar_t codeUnit[5] = {};
    for (const wchar_t* p = prefix; p && *p; ++p) {
        swprintf_s(codeUnit, L"%04X", static_cast<unsigned>(*p));
        encoded += codeUnit;
    }
    return encoded;
}

static bool ParseIsoDate(const wchar_t* date, SYSTEMTIME* parsed) {
    if (!date || wcslen(date) != 10 || date[4] != L'-' || date[7] != L'-') return false;
    const int positions[] = { 0, 1, 2, 3, 5, 6, 8, 9 };
    for (int position : positions) {
        if (date[position] < L'0' || date[position] > L'9') return false;
    }
    SYSTEMTIME value = {};
    value.wYear = static_cast<WORD>((date[0] - L'0') * 1000 + (date[1] - L'0') * 100 +
                                    (date[2] - L'0') * 10 + date[3] - L'0');
    value.wMonth = static_cast<WORD>((date[5] - L'0') * 10 + date[6] - L'0');
    value.wDay = static_cast<WORD>((date[8] - L'0') * 10 + date[9] - L'0');
    FILETIME fileTime = {};
    if (!SystemTimeToFileTime(&value, &fileTime)) return false;
    if (parsed) *parsed = value;
    return true;
}

static void SetToday(wchar_t* out, size_t maxLen) {
    SYSTEMTIME today = {};
    GetLocalTime(&today);
    swprintf_s(out, maxLen, L"%04u-%02u-%02u", today.wYear, today.wMonth, today.wDay);
}

bool ParseTaskbarMeterOffset(const wchar_t* text, int* value) {
    if (!value) return false;
    long parsed = 0;
    if (!ParseInteger(text, &parsed)) return false;
    *value = ClampTaskbarMeterOffset(static_cast<int>(parsed));
    return true;
}

void LoadSettingsCustom(AppSettings* s, const wchar_t* filePath) {
    *s = AppSettings(); // start with defaults
    ResetLifetimeTotals(s);
    GetPrivateProfileStringW(L"Overlay", L"FontFamily", L"Segoe UI", s->fontFamily, _countof(s->fontFamily), filePath);

    wchar_t encodedPrefix[METER_PREFIX_CAPACITY * 4 + 2] = {};
    wchar_t decodedPrefix[METER_PREFIX_CAPACITY] = {};
    GetPrivateProfileStringW(L"Overlay", L"DownloadPrefix", L"", encodedPrefix, _countof(encodedPrefix), filePath);
    if (encodedPrefix[0] && DecodePrefix(encodedPrefix, decodedPrefix, _countof(decodedPrefix))) {
        wcscpy_s(s->downPrefix, _countof(s->downPrefix), decodedPrefix);
    }
    GetPrivateProfileStringW(L"Overlay", L"UploadPrefix", L"", encodedPrefix, _countof(encodedPrefix), filePath);
    if (encodedPrefix[0] && DecodePrefix(encodedPrefix, decodedPrefix, _countof(decodedPrefix))) {
        wcscpy_s(s->upPrefix, _countof(s->upPrefix), decodedPrefix);
    }

    wchar_t num[32] = {};
    GetPrivateProfileStringW(L"Overlay", L"FontSize", L"8", num, _countof(num), filePath);
    double sz = wcstod(num, nullptr);
    if (sz >= 4.0 && sz <= 72.0) {
        s->fontSize = sz;
    }

    s->fontStyle = GetPrivateProfileIntW(L"Overlay", L"FontStyle", 1, filePath);
    s->showWidget = (GetPrivateProfileIntW(L"Overlay", L"ShowWidget", 1, filePath) != 0) ? 1 : 0;
    s->showTrayIcon = (GetPrivateProfileIntW(L"General", L"ShowTrayIcon", 1, filePath) != 0) ? 1 : 0;

    GetPrivateProfileStringW(L"Overlay", L"TaskbarOffset", L"", num, _countof(num), filePath);
    int taskbarOffset = 0;
    if (ParseTaskbarMeterOffset(num, &taskbarOffset)) s->taskbarOffset = taskbarOffset;

    GetPrivateProfileStringW(L"Overlay", L"MinimumSpeedUnit", L"Auto", num, _countof(num), filePath);
    if (_wcsicmp(num, L"KB/s") == 0) {
        s->minimumSpeedUnit = MinimumSpeedUnit::Kilobytes;
    } else if (_wcsicmp(num, L"MB/s") == 0) {
        s->minimumSpeedUnit = MinimumSpeedUnit::Megabytes;
    } else if (_wcsicmp(num, L"GB/s") == 0) {
        s->minimumSpeedUnit = MinimumSpeedUnit::Gigabytes;
    }

    GetPrivateProfileStringW(L"Overlay", L"DecimalPlaces", L"", num, _countof(num), filePath);
    long decimalPlaces = 0;
    if (ParseInteger(num, &decimalPlaces)) {
        s->decimalPlaces = ClampSpeedDecimalPlaces(static_cast<int>(decimalPlaces));
    }

    s->down = static_cast<COLORREF>(GetPrivateProfileIntW(L"Overlay", L"DownloadColor", static_cast<DWORD>(s->down), filePath));
    s->up = static_cast<COLORREF>(GetPrivateProfileIntW(L"Overlay", L"UploadColor", static_cast<DWORD>(s->up), filePath));

    GetPrivateProfileStringW(L"Totals", L"Downloaded", L"", num, _countof(num), filePath);
    ParseUnsigned64(num, &s->lifetimeDownloaded);
    GetPrivateProfileStringW(L"Totals", L"Uploaded", L"", num, _countof(num), filePath);
    ParseUnsigned64(num, &s->lifetimeUploaded);
    GetPrivateProfileStringW(L"Totals", L"Since", L"", num, _countof(num), filePath);
    if (ParseIsoDate(num, nullptr)) {
        wcscpy_s(s->lifetimeSince, _countof(s->lifetimeSince), num);
    }
}

void SaveSettingsCustom(const AppSettings* s, const wchar_t* filePath) {
    std::wstring path(filePath);
    size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        CreateDirectoryW(path.substr(0, slash).c_str(), nullptr);
    }

    wchar_t num[32] = {};
    WritePrivateProfileStringW(L"Overlay", L"FontFamily", s->fontFamily, filePath);
    std::wstring encodedPrefix = EncodePrefix(s->downPrefix);
    WritePrivateProfileStringW(L"Overlay", L"DownloadPrefix", encodedPrefix.c_str(), filePath);
    encodedPrefix = EncodePrefix(s->upPrefix);
    WritePrivateProfileStringW(L"Overlay", L"UploadPrefix", encodedPrefix.c_str(), filePath);
    swprintf_s(num, L"%.1f", s->fontSize);
    WritePrivateProfileStringW(L"Overlay", L"FontSize", num, filePath);
    swprintf_s(num, L"%d", s->fontStyle);
    WritePrivateProfileStringW(L"Overlay", L"FontStyle", num, filePath);
    WritePrivateProfileStringW(L"Overlay", L"ShowWidget", s->showWidget ? L"1" : L"0", filePath);
    WritePrivateProfileStringW(L"General", L"ShowTrayIcon", s->showTrayIcon ? L"1" : L"0", filePath);
    swprintf_s(num, L"%d", ClampTaskbarMeterOffset(s->taskbarOffset));
    WritePrivateProfileStringW(L"Overlay", L"TaskbarOffset", num, filePath);

    const wchar_t* minimumUnit = L"Auto";
    if (s->minimumSpeedUnit == MinimumSpeedUnit::Kilobytes) minimumUnit = L"KB/s";
    else if (s->minimumSpeedUnit == MinimumSpeedUnit::Megabytes) minimumUnit = L"MB/s";
    else if (s->minimumSpeedUnit == MinimumSpeedUnit::Gigabytes) minimumUnit = L"GB/s";
    WritePrivateProfileStringW(L"Overlay", L"MinimumSpeedUnit", minimumUnit, filePath);
    swprintf_s(num, L"%d", ClampSpeedDecimalPlaces(s->decimalPlaces));
    WritePrivateProfileStringW(L"Overlay", L"DecimalPlaces", num, filePath);

    swprintf_s(num, L"%lu", static_cast<DWORD>(s->down));
    WritePrivateProfileStringW(L"Overlay", L"DownloadColor", num, filePath);
    swprintf_s(num, L"%lu", static_cast<DWORD>(s->up));
    WritePrivateProfileStringW(L"Overlay", L"UploadColor", num, filePath);

    swprintf_s(num, L"%llu", static_cast<unsigned long long>(s->lifetimeDownloaded));
    WritePrivateProfileStringW(L"Totals", L"Downloaded", num, filePath);
    swprintf_s(num, L"%llu", static_cast<unsigned long long>(s->lifetimeUploaded));
    WritePrivateProfileStringW(L"Totals", L"Uploaded", num, filePath);
    wchar_t since[11] = {};
    if (ParseIsoDate(s->lifetimeSince, nullptr)) {
        wcscpy_s(since, s->lifetimeSince);
    } else {
        SetToday(since, _countof(since));
    }
    WritePrivateProfileStringW(L"Totals", L"Since", since, filePath);
}

void LoadSettings(AppSettings* s) {
    std::wstring path = GetDefaultSettingsPath();
    LoadSettingsCustom(s, path.c_str());
    s->startWithWindows = IsStartWithWindowsEnabled() ? 1 : 0;
}

void SaveSettings(const AppSettings* s) {
    std::wstring path = GetDefaultSettingsPath();
    SaveSettingsCustom(s, path.c_str());
}

void GetSettingsPath(wchar_t* buf, size_t maxLen) {
    if (!buf || maxLen == 0) return;
    std::wstring path = GetDefaultSettingsPath();
    wcsncpy_s(buf, maxLen, path.c_str(), _TRUNCATE);
}

bool IsStartWithWindowsEnabled() {
    std::wstring expected = GetStartupCommand();
    if (expected.empty()) return false;

    DWORD bytes = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VALUE, RRF_RT_REG_SZ,
                     nullptr, nullptr, &bytes) != ERROR_SUCCESS || bytes == 0) {
        return false;
    }

    std::wstring actual(bytes / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VALUE, RRF_RT_REG_SZ,
                     nullptr, actual.data(), &bytes) != ERROR_SUCCESS) {
        return false;
    }
    actual.resize(wcsnlen_s(actual.c_str(), actual.size()));
    return actual == expected;
}

bool SetStartWithWindowsEnabled(bool enabled) {
    HKEY key = nullptr;
    LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, nullptr, 0,
                                     KEY_SET_VALUE, nullptr, &key, nullptr);
    if (status != ERROR_SUCCESS) return false;

    if (enabled) {
        std::wstring command = GetStartupCommand();
        status = command.empty()
            ? ERROR_BAD_PATHNAME
            : RegSetValueExW(key, RUN_VALUE, 0, REG_SZ,
                             reinterpret_cast<const BYTE*>(command.c_str()),
                             static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        status = RegDeleteValueW(key, RUN_VALUE);
        if (status == ERROR_FILE_NOT_FOUND) status = ERROR_SUCCESS;
    }

    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

void AddLifetimeTraffic(AppSettings* s, ULONGLONG downloaded, ULONGLONG uploaded) {
    if (!s) return;
    s->lifetimeDownloaded = downloaded > ULLONG_MAX - s->lifetimeDownloaded
        ? ULLONG_MAX : s->lifetimeDownloaded + downloaded;
    s->lifetimeUploaded = uploaded > ULLONG_MAX - s->lifetimeUploaded
        ? ULLONG_MAX : s->lifetimeUploaded + uploaded;
}

void ResetLifetimeTotals(AppSettings* s) {
    if (!s) return;
    s->lifetimeDownloaded = 0;
    s->lifetimeUploaded = 0;
    SetToday(s->lifetimeSince, _countof(s->lifetimeSince));
}

bool FormatLifetimeSinceDate(const wchar_t* isoDate, wchar_t* out, size_t maxLen) {
    SYSTEMTIME date = {};
    if (!out || maxLen == 0 || !ParseIsoDate(isoDate, &date)) return false;
    return swprintf_s(out, maxLen, L"%02u/%02u/%04u", date.wDay, date.wMonth, date.wYear) >= 0;
}
