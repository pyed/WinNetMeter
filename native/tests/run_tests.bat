@echo off
if "%~1"=="--no-build" (
    if exist unit_tests.exe (
        unit_tests.exe
        exit /b %ERRORLEVEL%
    )
)
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
cl /nologo /std:c++20 /W4 /WX /permissive- /EHsc /MT /utf-8 /DUNICODE /D_UNICODE unit_tests.cpp ..\network.cpp ..\settings.cpp /link iphlpapi.lib shell32.lib user32.lib gdi32.lib /out:unit_tests.exe || exit /b 1
unit_tests.exe || exit /b 1
