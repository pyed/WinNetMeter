@echo off
setlocal
rem Builds and runs the native unit tests for this machine's architecture.
rem   run_tests.bat --no-build    runs an existing unit_tests.exe

if "%~1"=="--no-build" (
    if exist "%~dp0unit_tests.exe" (
        "%~dp0unit_tests.exe"
        exit /b %ERRORLEVEL%
    )
)

set "TARGET=x64"
if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "TARGET=arm64"
call "%~dp0..\vcenv.bat" %TARGET% || exit /b 1

cd /d "%~dp0"
cl /nologo /std:c++20 /W4 /WX /permissive- /EHsc /MT /utf-8 /DUNICODE /D_UNICODE unit_tests.cpp ..\network.cpp ..\settings.cpp ..\render.cpp /link advapi32.lib iphlpapi.lib shell32.lib user32.lib gdi32.lib /out:unit_tests.exe || exit /b 1
.\unit_tests.exe || exit /b 1
