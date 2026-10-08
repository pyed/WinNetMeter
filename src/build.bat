@echo off
setlocal
rem Build: out\WinNetMeter.exe (static CRT, Release)
rem   build.bat          for this machine's architecture
rem   build.bat arm64    for Windows on Arm (x64 or arm64; cross builds need
rem                      the MSVC tools for the target installed)

set "TARGET=%~1"
if not defined TARGET (
    set "TARGET=x64"
    if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "TARGET=arm64"
)
if /i not "%TARGET%"=="x64" if /i not "%TARGET%"=="arm64" (
    echo ERROR: unknown target "%TARGET%", use x64 or arm64
    exit /b 1
)
set "HOST=x64"
if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "HOST=arm64"
set "VCARGS=%HOST%_%TARGET%"
if /i "%HOST%"=="%TARGET%" set "VCARGS=%TARGET%"
call "%~dp0vcenv.bat" %VCARGS% || exit /b 1

cd /d "%~dp0"
if not exist out mkdir out
rc /nologo /fo out\app.res app.rc || exit /b 1
cl /nologo /std:c++20 /W4 /WX /permissive- /EHsc /MT /O2 /utf-8 /DUNICODE /D_UNICODE /DNDEBUG /Fo"out\\" /Fe"out\WinNetMeter.exe" main.cpp network.cpp settings.cpp render.cpp meter.cpp out\app.res /link advapi32.lib iphlpapi.lib gdi32.lib user32.lib shell32.lib comctl32.lib comdlg32.lib dwmapi.lib /SUBSYSTEM:WINDOWS
exit /b %ERRORLEVEL%
