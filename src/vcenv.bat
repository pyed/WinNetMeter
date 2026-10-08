@echo off
rem Sets up MSVC in the caller's environment for vcvarsall arguments %1:
rem x64 or arm64 (native), x64_arm64 or arm64_x64 (cross). Does nothing when a
rem developer prompt for the same target is already active. No setlocal: the
rem point is to change the caller's environment.

set "WNM_VCARGS=%~1"
set "WNM_TARGET=%WNM_VCARGS:*_=%"
if /i "%VSCMD_ARG_TGT_ARCH%"=="%WNM_TARGET%" (
    where cl >nul 2>&1 && exit /b 0
)

set "WNM_COMPONENT=Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
if /i "%WNM_TARGET%"=="arm64" set "WNM_COMPONENT=Microsoft.VisualStudio.Component.VC.Tools.ARM64"

set "WNM_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%WNM_VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%WNM_VSWHERE%" -latest -products * -requires %WNM_COMPONENT% -property installationPath 2^>nul`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvarsall.bat" (
            call "%%i\VC\Auxiliary\Build\vcvarsall.bat" %WNM_VCARGS% || exit /b 1
            goto check
        )
    )
)

for %%p in (
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Community"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
) do (
    if exist "%%~p\VC\Auxiliary\Build\vcvarsall.bat" (
        call "%%~p\VC\Auxiliary\Build\vcvarsall.bat" %WNM_VCARGS% || exit /b 1
        goto check
    )
)

:check
rem vcvarsall sets up a target even when its tools are missing; ask the compiler.
where cl >nul 2>&1 || (echo ERROR: MSVC compiler for %WNM_TARGET% not found & exit /b 1)
cl 2>&1 | findstr /i /c:" for %WNM_TARGET%" >nul || (
    echo ERROR: the MSVC tools for %WNM_TARGET% are not installed
    exit /b 1
)
exit /b 0
