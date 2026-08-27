@echo off
setlocal
rem Build: NetworkMonitorLite.exe (static CRT, Release)

where cl >nul 2>&1
if %ERRORLEVEL% equ 0 goto compile

if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" (
            call "%%i\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
            goto compile
        )
    )
)

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
    goto compile
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
    goto compile
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
    goto compile
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
    goto compile
)

:compile
where cl >nul 2>&1 || (echo ERROR: MSVC x64 compiler not found & exit /b 1)

if not exist out mkdir out
rc /nologo /fo out\app.res app.rc || exit /b 1
cl /nologo /std:c++20 /W4 /WX /permissive- /EHsc /MT /O2 /utf-8 /DUNICODE /D_UNICODE /DNDEBUG /Fo"out\\" /Fe"out\NetworkMonitorLite.exe" main.cpp network.cpp settings.cpp out\app.res /link iphlpapi.lib gdi32.lib user32.lib shell32.lib comctl32.lib comdlg32.lib /SUBSYSTEM:WINDOWS
exit /b %ERRORLEVEL%
