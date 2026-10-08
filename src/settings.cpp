#include "settings.h"
#include <shlobj.h>
#include <stdio.h>
#include <cerrno>
#include <cwchar>
#include <climits>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

static const wchar_t RUN_KEY[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t DEFAULT_RUN_VALUE[] = L"WinNetMeter";
static std::wstring g_runValueName = DEFAULT_RUN_VALUE;
static std::wstring g_settingsPathOverride;

static std::wstring GetModulePath() {
    std::wstring path(32768, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length == path.size()) return {};
    path.resize(length);
    return path;
}

static std::wstring GetStartupCommand() {
    std::wstring path = GetModulePath();
    if (path.empty()) return {};
    return L"\"" + path + L"\"";
}

static std::wstring GetDefaultSettingsPath() {
    if (!g_settingsPathOverride.empty()) return g_settingsPathOverride;
    wchar_t appdata[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata) == S_OK) {
        return std::wstring(appdata) + L"\\WinNetMeter\\settings.ini";
    }
    // The fallback must be absolute: GetPrivateProfileStringW resolves a bare
    // file name against %WINDIR%, while the writer would use the working directory.
    std::wstring exe = GetModulePath();
    size_t slash = exe.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return L".\\settings.ini";
    return exe.substr(0, slash + 1) + L"settings.ini";
}

void SetSettingsPathOverride(const wchar_t* path) {
    g_settingsPathOverride.clear();
    if (!path || !path[0]) return;
    wchar_t full[MAX_PATH * 4] = {};
    DWORD length = GetFullPathNameW(path, _countof(full), full, nullptr);
    g_settingsPathOverride = (length > 0 && length < _countof(full)) ? full : path;
}

void SetStartupValueName(const wchar_t* name) {
    g_runValueName = (name && name[0]) ? name : DEFAULT_RUN_VALUE;
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

// Strict 1-16 digit hexadecimal, as written for adapter LUIDs.
static bool ParseHex64(const wchar_t* text, ULONGLONG* value) {
    if (!text || !value || !text[0]) return false;
    ULONGLONG parsed = 0;
    size_t digits = 0;
    for (const wchar_t* p = text; *p; ++p, ++digits) {
        int digit = HexDigit(*p);
        if (digit < 0 || digits >= 16) return false;
        parsed = (parsed << 4) | static_cast<ULONGLONG>(digit);
    }
    *value = parsed;
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

// "auto" or a decimal COLORREF. Releases before 0.2.0 (no SettingsVersion)
// always wrote their white default, so white from them means "never chosen"
// and becomes Automatic; on a dark taskbar that still renders white.
static COLORREF LoadMeterColor(const wchar_t* key, COLORREF fallback, int version, const wchar_t* filePath) {
    wchar_t text[32] = {};
    GetPrivateProfileStringW(L"Overlay", key, L"", text, _countof(text), filePath);
    if (!text[0]) return fallback;
    if (_wcsicmp(text, L"auto") == 0) return METER_COLOR_AUTO;
    ULONGLONG value = 0;
    if (!ParseUnsigned64(text, &value) || value > 0x00FFFFFF) return fallback;
    if (version < 2 && value == RGB(255, 255, 255)) return METER_COLOR_AUTO;
    return static_cast<COLORREF>(value);
}

COLORREF ResolveMeterColor(COLORREF color, bool lightTaskbar) {
    if (color == METER_COLOR_AUTO) return lightTaskbar ? RGB(28, 28, 28) : RGB(255, 255, 255);
    return color & 0x00FFFFFF;
}

bool IsSystemThemeLight() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                        L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS &&
           value != 0;
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
    const bool hasOffset = num[0] != L'\0';
    int taskbarOffset = 0;
    if (ParseTaskbarMeterOffset(num, &taskbarOffset)) s->taskbarOffset = taskbarOffset;

    // Releases before 0.2.0 always wrote TaskbarOffset and never Anchor: their
    // offset only makes sense from the old fixed point, so keep that for them.
    GetPrivateProfileStringW(L"Overlay", L"Anchor", L"", num, _countof(num), filePath);
    if (!num[0]) {
        s->meterAnchor = hasOffset ? METER_ANCHOR_LEGACY : METER_ANCHOR_TRAY;
    } else if (_wcsicmp(num, L"legacy") == 0) {
        s->meterAnchor = METER_ANCHOR_LEGACY;
    } else if (_wcsicmp(num, L"apps") == 0) {
        s->meterAnchor = METER_ANCHOR_APPS;
    } else if (_wcsicmp(num, L"left") == 0) {
        s->meterAnchor = METER_ANCHOR_LEFT;
    } else {
        s->meterAnchor = METER_ANCHOR_TRAY;
    }
    s->embedInTaskbar = (GetPrivateProfileIntW(L"Overlay", L"Embed", 0, filePath) != 0) ? 1 : 0;

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

    GetPrivateProfileStringW(L"Overlay", L"SpeedUnits", L"bytes", num, _countof(num), filePath);
    s->speedBits = _wcsicmp(num, L"bits") == 0 ? 1 : 0;

    const int version = static_cast<int>(GetPrivateProfileIntW(L"General", L"SettingsVersion", 1, filePath));
    s->down = LoadMeterColor(L"DownloadColor", s->down, version, filePath);
    s->up = LoadMeterColor(L"UploadColor", s->up, version, filePath);

    GetPrivateProfileStringW(L"Totals", L"Downloaded", L"", num, _countof(num), filePath);
    ParseUnsigned64(num, &s->lifetimeDownloaded);
    GetPrivateProfileStringW(L"Totals", L"Uploaded", L"", num, _countof(num), filePath);
    ParseUnsigned64(num, &s->lifetimeUploaded);
    GetPrivateProfileStringW(L"Totals", L"Since", L"", num, _countof(num), filePath);
    if (ParseIsoDate(num, nullptr)) {
        wcscpy_s(s->lifetimeSince, _countof(s->lifetimeSince), num);
    }

    // Absent in files written before 0.2.0: those users get Automatic.
    GetPrivateProfileStringW(L"Network", L"Adapter", L"auto", num, _countof(num), filePath);
    ULONGLONG adapterLuid = 0;
    if (_wcsicmp(num, L"auto") != 0 && ParseHex64(num, &adapterLuid) && adapterLuid != 0) {
        s->adapterAuto = 0;
        s->adapterLuid = adapterLuid;
        wchar_t encodedAlias[_countof(s->adapterAlias) * 4 + 2] = {};
        wchar_t alias[_countof(s->adapterAlias)] = {};
        GetPrivateProfileStringW(L"Network", L"AdapterName", L"", encodedAlias, _countof(encodedAlias), filePath);
        if (encodedAlias[0] && DecodePrefix(encodedAlias, alias, _countof(alias))) {
            wcscpy_s(s->adapterAlias, _countof(s->adapterAlias), alias);
        }
    }
}

// Renders the whole settings file in memory. Written as UTF-16LE with a BOM so
// GetPrivateProfileStringW reads it back as Unicode; that API also still reads
// the ANSI files produced by earlier versions, so upgrades keep working.
static std::wstring BuildSettingsIni(const AppSettings* s) {
    wchar_t num[32] = {};
    std::wstring out;

    out += L"[General]\r\n";
    swprintf_s(num, L"%d", SETTINGS_VERSION);
    out += L"SettingsVersion=";
    out += num;
    out += L"\r\nShowTrayIcon=";
    out += s->showTrayIcon ? L"1" : L"0";
    out += L"\r\n\r\n[Overlay]\r\n";

    out += L"FontFamily=";
    out += s->fontFamily;
    out += L"\r\nDownloadPrefix=";
    out += EncodePrefix(s->downPrefix);
    out += L"\r\nUploadPrefix=";
    out += EncodePrefix(s->upPrefix);

    swprintf_s(num, L"%.1f", s->fontSize);
    out += L"\r\nFontSize=";
    out += num;
    swprintf_s(num, L"%d", s->fontStyle);
    out += L"\r\nFontStyle=";
    out += num;
    out += L"\r\nShowWidget=";
    out += s->showWidget ? L"1" : L"0";
    swprintf_s(num, L"%d", ClampTaskbarMeterOffset(s->taskbarOffset));
    out += L"\r\nTaskbarOffset=";
    out += num;
    static const wchar_t* const anchors[] = { L"legacy", L"tray", L"apps", L"left" };
    out += L"\r\nAnchor=";
    out += anchors[(s->meterAnchor >= METER_ANCHOR_LEGACY && s->meterAnchor <= METER_ANCHOR_LEFT)
                       ? s->meterAnchor : METER_ANCHOR_TRAY];
    out += L"\r\nEmbed=";
    out += s->embedInTaskbar ? L"1" : L"0";

    const wchar_t* minimumUnit = L"Auto";
    if (s->minimumSpeedUnit == MinimumSpeedUnit::Kilobytes) minimumUnit = L"KB/s";
    else if (s->minimumSpeedUnit == MinimumSpeedUnit::Megabytes) minimumUnit = L"MB/s";
    else if (s->minimumSpeedUnit == MinimumSpeedUnit::Gigabytes) minimumUnit = L"GB/s";
    out += L"\r\nMinimumSpeedUnit=";
    out += minimumUnit;
    swprintf_s(num, L"%d", ClampSpeedDecimalPlaces(s->decimalPlaces));
    out += L"\r\nDecimalPlaces=";
    out += num;
    out += L"\r\nSpeedUnits=";
    out += s->speedBits ? L"bits" : L"bytes";

    for (const auto& color : { std::make_pair(L"DownloadColor", s->down), std::make_pair(L"UploadColor", s->up) }) {
        out += L"\r\n";
        out += color.first;
        if (color.second == METER_COLOR_AUTO) {
            out += L"=auto";
        } else {
            swprintf_s(num, L"=%lu", static_cast<DWORD>(color.second & 0x00FFFFFF));
            out += num;
        }
    }

    out += L"\r\n\r\n[Network]\r\nAdapter=";
    if (s->adapterAuto || s->adapterLuid == 0) {
        out += L"auto";
    } else {
        swprintf_s(num, L"%016llX", static_cast<unsigned long long>(s->adapterLuid));
        out += num;
        out += L"\r\nAdapterName=";
        out += EncodePrefix(s->adapterAlias);
    }

    out += L"\r\n\r\n[Totals]\r\n";
    swprintf_s(num, L"%llu", static_cast<unsigned long long>(s->lifetimeDownloaded));
    out += L"Downloaded=";
    out += num;
    swprintf_s(num, L"%llu", static_cast<unsigned long long>(s->lifetimeUploaded));
    out += L"\r\nUploaded=";
    out += num;

    wchar_t since[11] = {};
    if (ParseIsoDate(s->lifetimeSince, nullptr)) {
        wcscpy_s(since, s->lifetimeSince);
    } else {
        SetToday(since, _countof(since));
    }
    out += L"\r\nSince=";
    out += since;
    out += L"\r\n";
    return out;
}

#ifndef FILE_RENAME_FLAG_REPLACE_IF_EXISTS
#define FILE_RENAME_FLAG_REPLACE_IF_EXISTS 0x00000001
#endif
#ifndef FILE_RENAME_FLAG_POSIX_SEMANTICS
#define FILE_RENAME_FLAG_POSIX_SEMANTICS 0x00000002
#endif

static bool WriteSettingsText(HANDLE file, const std::wstring& text) {
    const wchar_t bom = 0xFEFF;
    DWORD written = 0;
    if (!WriteFile(file, &bom, sizeof(bom), &written, nullptr) || written != sizeof(bom)) return false;
    if (!text.empty()) {
        const DWORD bytes = static_cast<DWORD>(text.size() * sizeof(wchar_t));
        if (!WriteFile(file, text.data(), bytes, &written, nullptr) || written != bytes) return false;
    }
    return FlushFileBuffers(file) != FALSE;
}

// Swaps source over target. POSIX semantics (Windows 10 1709+) let the swap
// succeed while another process holds the target open with delete sharing, as
// virus scanners and indexers do; plain MoveFileExW fails in that case.
static bool ReplaceByRename(const std::wstring& source, const std::wstring& target) {
    HANDLE file = CreateFileW(source.c_str(), DELETE | SYNCHRONIZE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        const size_t bytes = sizeof(FILE_RENAME_INFO) + target.size() * sizeof(wchar_t);
        std::vector<ULONGLONG> buffer((bytes + sizeof(ULONGLONG) - 1) / sizeof(ULONGLONG), 0);
        auto* info = reinterpret_cast<FILE_RENAME_INFO*>(buffer.data());
        info->Flags = FILE_RENAME_FLAG_REPLACE_IF_EXISTS | FILE_RENAME_FLAG_POSIX_SEMANTICS;
        info->RootDirectory = nullptr;
        info->FileNameLength = static_cast<DWORD>(target.size() * sizeof(wchar_t));
        memcpy(info->FileName, target.c_str(), info->FileNameLength);
        BOOL renamed = SetFileInformationByHandle(file, FileRenameInfoEx, info, static_cast<DWORD>(bytes));
        CloseHandle(file);
        if (renamed) return true;
    }
    return MoveFileExW(source.c_str(), target.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

// Writes to a sibling temp file, flushes it, then swaps it into place, so an
// interrupted save can never leave a half-written settings file behind. If a
// holder refuses delete sharing, falls back to rewriting the file in place,
// which is what releases before 0.1.6 always did.
static bool WriteFileAtomic(const std::wstring& path, const std::wstring& text) {
    const std::wstring tempPath = path + L".tmp";
    HANDLE file = CreateFileW(tempPath.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    bool written = WriteSettingsText(file, text);
    CloseHandle(file);

    bool replaced = false;
    for (int attempt = 0; written && !replaced && attempt < 4; ++attempt) {
        if (attempt > 0) Sleep(15 * attempt);
        replaced = ReplaceByRename(tempPath, path);
        if (!replaced) {
            DWORD error = GetLastError();
            if (error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION &&
                error != ERROR_LOCK_VIOLATION) {
                break;
            }
        }
    }
    DeleteFileW(tempPath.c_str());
    if (replaced || !written) return replaced;

    HANDLE target = CreateFileW(path.c_str(), GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (target == INVALID_HANDLE_VALUE) return false;
    bool rewritten = WriteSettingsText(target, text);
    CloseHandle(target);
    return rewritten;
}

bool SaveSettingsCustom(const AppSettings* s, const wchar_t* filePath) {
    if (!s || !filePath) return false;

    // The rename API needs a fully qualified target, and relative names would
    // otherwise resolve differently for the reader (see GetDefaultSettingsPath).
    wchar_t full[MAX_PATH * 4] = {};
    DWORD length = GetFullPathNameW(filePath, _countof(full), full, nullptr);
    std::wstring path = (length > 0 && length < _countof(full)) ? full : filePath;
    size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        std::wstring directory = path.substr(0, slash);
        if (!directory.empty() && !CreateDirectoryW(directory.c_str(), nullptr) &&
            GetLastError() != ERROR_ALREADY_EXISTS) {
            return false;
        }
    }

    return WriteFileAtomic(path, BuildSettingsIni(s));
}

void LoadSettings(AppSettings* s) {
    std::wstring path = GetDefaultSettingsPath();
    LoadSettingsCustom(s, path.c_str());
    s->startWithWindows = IsStartWithWindowsEnabled() ? 1 : 0;
}

bool SaveSettings(const AppSettings* s) {
    std::wstring path = GetDefaultSettingsPath();
    return SaveSettingsCustom(s, path.c_str());
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
    if (RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, g_runValueName.c_str(), RRF_RT_REG_SZ,
                     nullptr, nullptr, &bytes) != ERROR_SUCCESS || bytes == 0) {
        return false;
    }

    std::wstring actual(bytes / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, g_runValueName.c_str(), RRF_RT_REG_SZ,
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
            : RegSetValueExW(key, g_runValueName.c_str(), 0, REG_SZ,
                             reinterpret_cast<const BYTE*>(command.c_str()),
                             static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        status = RegDeleteValueW(key, g_runValueName.c_str());
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

bool FormatLifetimeSinceDate(const wchar_t* isoDate, wchar_t* out, size_t maxLen, const wchar_t* locale) {
    SYSTEMTIME date = {};
    if (!out || maxLen == 0 || maxLen > INT_MAX || !ParseIsoDate(isoDate, &date)) return false;
    return GetDateFormatEx(locale ? locale : LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &date, nullptr,
                           out, static_cast<int>(maxLen), nullptr) > 0;
}
