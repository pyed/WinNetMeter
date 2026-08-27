#include "settings.h"
#include <shlobj.h>
#include <stdio.h>
#include <cwchar>
#include <string>

static std::wstring GetDefaultSettingsPath() {
    wchar_t appdata[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata) == S_OK) {
        return std::wstring(appdata) + L"\\WinNetMeter\\settings.ini";
    }
    return L"settings.ini";
}

void LoadSettingsCustom(AppSettings* s, const wchar_t* filePath) {
    *s = AppSettings(); // start with defaults
    GetPrivateProfileStringW(L"Overlay", L"FontFamily", L"Segoe UI", s->fontFamily, _countof(s->fontFamily), filePath);
    
    wchar_t num[32] = {};
    GetPrivateProfileStringW(L"Overlay", L"FontSize", L"8", num, _countof(num), filePath);
    double sz = wcstod(num, nullptr);
    if (sz >= 4.0 && sz <= 72.0) {
        s->fontSize = sz;
    }

    s->fontStyle = GetPrivateProfileIntW(L"Overlay", L"FontStyle", 1, filePath);
    s->showWidget = (GetPrivateProfileIntW(L"Overlay", L"ShowWidget", 1, filePath) != 0) ? 1 : 0;
    
    s->bg = static_cast<COLORREF>(GetPrivateProfileIntW(L"Overlay", L"Background", static_cast<DWORD>(s->bg), filePath));
    s->down = static_cast<COLORREF>(GetPrivateProfileIntW(L"Overlay", L"DownloadColor", static_cast<DWORD>(s->down), filePath));
    s->up = static_cast<COLORREF>(GetPrivateProfileIntW(L"Overlay", L"UploadColor", static_cast<DWORD>(s->up), filePath));
}

void SaveSettingsCustom(const AppSettings* s, const wchar_t* filePath) {
    std::wstring path(filePath);
    size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        CreateDirectoryW(path.substr(0, slash).c_str(), nullptr);
    }

    wchar_t num[32] = {};
    WritePrivateProfileStringW(L"Overlay", L"FontFamily", s->fontFamily, filePath);
    swprintf_s(num, L"%.1f", s->fontSize);
    WritePrivateProfileStringW(L"Overlay", L"FontSize", num, filePath);
    swprintf_s(num, L"%d", s->fontStyle);
    WritePrivateProfileStringW(L"Overlay", L"FontStyle", num, filePath);
    WritePrivateProfileStringW(L"Overlay", L"ShowWidget", s->showWidget ? L"1" : L"0", filePath);

    swprintf_s(num, L"%lu", static_cast<DWORD>(s->bg));
    WritePrivateProfileStringW(L"Overlay", L"Background", num, filePath);
    swprintf_s(num, L"%lu", static_cast<DWORD>(s->down));
    WritePrivateProfileStringW(L"Overlay", L"DownloadColor", num, filePath);
    swprintf_s(num, L"%lu", static_cast<DWORD>(s->up));
    WritePrivateProfileStringW(L"Overlay", L"UploadColor", num, filePath);
}

void LoadSettings(AppSettings* s) {
    std::wstring path = GetDefaultSettingsPath();
    LoadSettingsCustom(s, path.c_str());
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
