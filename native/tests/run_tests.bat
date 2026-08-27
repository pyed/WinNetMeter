@echo off
setlocal

if "%~1"=="--no-build" (
    if exist unit_tests.exe (
        unit_tests.exe
        exit /b %ERRORLEVEL%
    )
)

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

cl /nologo /std:c++20 /W4 /WX /permissive- /EHsc /MT /utf-8 /DUNICODE /D_UNICODE unit_tests.cpp ..\network.cpp ..\settings.cpp /link iphlpapi.lib shell32.lib user32.lib gdi32.lib /out:unit_tests.exe || exit /b 1
unit_tests.exe || exit /b 1
