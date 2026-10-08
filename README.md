# WinNetMeter

[![CI](https://github.com/pyed/WinNetMeter/actions/workflows/ci.yml/badge.svg)](https://github.com/pyed/WinNetMeter/actions/workflows/ci.yml)
[![Latest release](https://img.shields.io/github/v/release/pyed/WinNetMeter)](https://github.com/pyed/WinNetMeter/releases)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

WinNetMeter is a lightweight Windows 10 and 11 x64 utility that shows real-time upload and download speed in the notification area and in a transparent meter on the taskbar. It is a native C++20 Win32 application with no installer, runtime framework, telemetry, updater, cloud service, or administrator/service requirement.

## Features

- Live upload/download speed, session totals, and resettable persisted totals.
- **Automatic** adapter selection that follows whichever interface carries the default route (switching between Wi-Fi, Ethernet and VPN as you do), or a fixed adapter that is remembered across restarts and reconnects.
- Speeds in bytes (KB/s, MB/s, binary) or bits (Kbps, Mbps, decimal), with a minimum unit and 0 to 2 decimal places.
- A transparent, non-activating taskbar meter placed **next to the tray**, **after the app buttons** or at the **left edge**, plus an offset from there.
- Optional **embedded** meter that stays visible while Start or Search is open (see [Known limitations](#known-limitations)).
- Optional meters on **every taskbar** when the taskbar is shown on several monitors.
- Colors that follow the light or dark taskbar automatically, or fixed colors of your choice; configurable prefixes and font, all updated live.
- A tray icon drawn at the size Windows uses for the taskbar's DPI.
- Hides for genuine fullscreen applications, following the shell's own fullscreen state, and follows an auto-hiding taskbar.
- Per-monitor DPI awareness, single-instance operation, recovery after Explorer restarts, optional start with Windows for the current user.
- One statically linked native executable; no .NET, Electron, Qt, WinUI, WebView, or third-party runtime.

## Installation

1. Download `WinNetMeter-v<version>-windows-x64.zip` or the standalone `WinNetMeter.exe` from [GitHub Releases](https://github.com/pyed/WinNetMeter/releases). Each file has a `.sha256` checksum next to it.
2. Extract the ZIP if needed, then run `WinNetMeter.exe`.
3. Keep the executable anywhere you prefer; WinNetMeter does not require installation or administrator rights.

Requires Windows 10 version 1607 or later, x64. Release binaries are currently unsigned, so Windows SmartScreen may show a warning the first time an unfamiliar build is run.

## Usage and settings

WinNetMeter starts in the notification area. Double-click the tray icon or the taskbar meter to open its status and settings window. Right-click the tray icon for the window, taskbar-meter visibility, and Exit.

The status panel shows the speeds, totals for the current session, and saved totals accumulated since the displayed date. **Reset total** clears the saved counters and stamps them with today's date. The adapter list starts with **Automatic (adapter in use)**; choosing a specific adapter keeps WinNetMeter on it, and it is found again after the adapter is reconnected or renamed.

Settings provide:

- Upload and download prefixes (empty is allowed), colors with an **Automatic** option that follows the taskbar theme, and the meter font.
- **Show speed in** bytes or bits, the minimum speed unit, and decimal places.
- **Meter position**: next to the tray, after the app buttons, at the left edge, or *Classic* (the fixed point used before 0.2.0, kept for upgraded settings), and an offset from there in logical pixels (`-4096` to `4096`), clamped to the taskbar.
- **Reset meter** restores the default prefixes, colors, font, speed format and position. It does not clear totals or change the check boxes below.
- **Show taskbar meter** and **Show tray icon**. At least one stays enabled so the window remains reachable.
- **Start with Windows** for the current user, through the standard `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` entry.
- **Show on all taskbars**: a meter on each monitor's taskbar.
- **Embed in taskbar (stays visible over Start)**: see below.

**Show taskbar meter**, **Show tray icon**, **Start with Windows**, **Show on all taskbars** and **Embed in taskbar** take effect with **Apply**; the other settings apply as you change them. Settings are stored in:

```text
%APPDATA%\WinNetMeter\settings.ini
```

Settings files from earlier releases keep working: an upgraded meter keeps its exact position (shown as *Classic*), and the old default white colors become **Automatic**.

## Known limitations

**Start and Search cover the default meter.** When Start or Search opens, Windows moves the taskbar into a higher z-order band than any application window can use, including always-on-top ones. A meter drawn as its own window (the default) is therefore covered while Start or Search is open. Measured on Windows 11: none of the meter's pixels are visible while Start is open, and it is back within about 150 ms after Start closes.

**Embed in taskbar** avoids this. The meter becomes a child window of the taskbar, so it moves, hides and stays visible with it, including over Start. It is opt-in because the meter then lives inside Explorer's taskbar window: no code is injected into Explorer, but taskbar customization tools or a future Windows update could hide or misplace it. If the meter cannot be embedded, WinNetMeter falls back to the default overlay.

**Several monitors.** Meters on secondary taskbars are covered by automated tests that simulate a second taskbar, but have not been tested on real multi-monitor hardware. Windows 11 secondary taskbars expose no window for their clock, so "next to the tray" keeps the same distance from the end of the taskbar as on the main one.

Fullscreen applications hide the meter on purpose until fullscreen ends. WinNetMeter relies on public Win32 APIs and the taskbar's long-standing window class names (to find the taskbar, its tray and its app buttons); it does not use UIAccess, code injection or undocumented shell interfaces.

## Building from source

Requirements:

- Windows x64.
- Visual Studio 2022 or later (or its Build Tools) with the MSVC x64 C++ tools.
- A Windows SDK containing the resource compiler and Win32 headers/libraries.

From the repository root:

```cmd
cd src
.\build.bat
```

The build script locates an installed MSVC toolchain and compiles with C++20, `/W4`, `/WX`, `/permissive-`, `/MT`, and `/O2`. The output is:

```text
src\out\WinNetMeter.exe
```

Run the native unit suite with:

```cmd
cd src\tests
.\run_tests.bat
```

Behavioral checks run against the built executable with `pwsh src\tests\windows_integration_tests.ps1 -Check <Name>`; the check names are listed at the top of that script. They use their own settings file and startup entry, so they never touch a real installation, but they drive the real desktop: they take the foreground, create fullscreen windows and open Start.

CI builds the executable for every push to `main` and every pull request, runs the unit suite, verifies the PE metadata, manifest, static runtime and imports, and runs every behavioral check on a Windows runner. A release tag rebuilds the executable, repeats the unit and PE checks, refuses a tag that does not match `src/version.h`, and publishes the ZIP, the executable and their SHA-256 checksums.

## Technical notes and privacy

WinNetMeter uses raw Win32/GDI, layered windows for the transparent meter, Windows IP Helper APIs for 64-bit interface counters, and `QueryPerformanceCounter` for monotonic sampling. The taskbar meter runs on its own thread, so a busy Explorer cannot stall the settings window and the reverse. Automatic adapter selection asks the local routing table which interface would reach a public address; no packet is sent. Measurements stay local: WinNetMeter does not capture packets, transmit traffic data, or use telemetry or a cloud service.

The executable links the C/C++ runtime statically with `/MT`; only standard Windows system DLLs are required.

## Credits

WinNetMeter is an independent native C++20 rewrite inspired by [NetworkMonitorLite](https://github.com/mcagriaksoy/NetworkMonitorLite) by mcagriaksoy. It is not affiliated with or endorsed by the original project or author.

## License

Licensed under the [Apache License, Version 2.0](LICENSE).
