@echo off
rem Build: NetworkMonitorLite.exe (static CRT, Release)
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
if not exist out mkdir out
rc /nologo /fo out\app.res app.rc || exit /b 1
cl /nologo /std:c++20 /W4 /permissive- /EHsc /MT /O2 /utf-8 /DUNICODE /D_UNICODE /DNDEBUG /Fo"out\\" /Fe"out\NetworkMonitorLite.exe" main.cpp network.cpp settings.cpp out\app.res /link iphlpapi.lib gdi32.lib user32.lib shell32.lib comctl32.lib /SUBSYSTEM:WINDOWS
exit /b %ERRORLEVEL%
