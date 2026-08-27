#include "settings.h"
#include <shlobj.h>
#include <stdio.h>
#include <cwchar>

static std::wstring SettingsPath() {
    wchar_t appdata[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata);
    return std::wstring(appdata) + L"\\NetworkMonitorLite\\settings.ini";
}

void LoadSettings(AppSettings* s) {
    *s = AppSettings(); // defaults; missing file / keys keep defaults
    const wchar_t* f = SettingsPath().c_str();
    GetPrivateProfileStringW(L"Overlay", L"FontFamily", L"Segoe UI", s->fontFamily, 64, f);
    wchar_t num[32] = {};
    GetPrivateProfileStringW(L"Overlay", L"FontSize", L"8", num, 32, f);
    s->fontSize = wcstod(num, nullptr);
    s->fontStyle = GetPrivateProfileIntW(L"Overlay", L"FontStyle", 1, f) ? 1 : 0;
    s->bg = static_cast<COLORREF>(GetPrivateProfileIntW(L"Overlay", L"Background", static_cast<DWORD>(s->bg), f));
    s->down = static_cast<COLORREF>(GetPrivateProfileIntW(L"Overlay", L"DownloadColor", static_cast<DWORD>(s->down), f));
    s->up = static_cast<COLORREF>(GetPrivateProfileIntW(L"Overlay", L"UploadColor", static_cast<DWORD>(s->up), f));
}

void SaveSettings(const AppSettings* s) {
    std::wstring path = SettingsPath();
    size_t slash = path.find_last_of(L'\\');
    if (slash != std::wstring::npos)
        CreateDirectoryW(path.substr(0, slash).c_str(), nullptr);
    const wchar_t* f = path.c_str();
    wchar_t num[32] = {};
    WritePrivateProfileStringW(L"Overlay", L"FontFamily", s->fontFamily, f);
    swprintf_s(num, L"%g", s->fontSize);
    WritePrivateProfileStringW(L"Overlay", L"FontSize", num, f);
    WritePrivateProfileStringW(L"Overlay", L"FontStyle", s->fontStyle ? L"1" : L"0", f);
    swprintf_s(num, L"%lu", static_cast<DWORD>(s->bg));
    WritePrivateProfileStringW(L"Overlay", L"Background", num, f);
    swprintf_s(num, L"%lu", static_cast<DWORD>(s->down));
    WritePrivateProfileStringW(L"Overlay", L"DownloadColor", num, f);
    swprintf_s(num, L"%lu", static_cast<DWORD>(s->up));
    WritePrivateProfileStringW(L"Overlay", L"UploadColor", num, f);
}
