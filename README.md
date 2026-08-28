# WinNetMeter

[![CI](https://github.com/pyed/WinNetMeter/actions/workflows/ci.yml/badge.svg)](https://github.com/pyed/WinNetMeter/actions/workflows/ci.yml)
[![Latest release](https://img.shields.io/github/v/release/pyed/WinNetMeter)](https://github.com/pyed/WinNetMeter/releases)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

WinNetMeter is a lightweight Windows 10 and 11 x64 utility that shows real-time upload and download speed in the notification area and in a transparent meter positioned on the taskbar. It is a native C++20 Win32 application with no installer, runtime framework, telemetry, updater, cloud service, or administrator/service requirement.

## Features

- Live upload/download speed and session totals for a selected Ethernet, Wi-Fi, VPN, or virtual interface.
- Compact dynamic tray icon, optional status window, and non-activating per-pixel-alpha taskbar meter.
- Configurable upload/download colors and overlay font.
- Auto, KB/s, MB/s, or GB/s minimum display unit, with 0, 1, or 2 decimal places.
- Signed taskbar offset for adjusting meter placement without dragging the overlay.
- Taskbar auto-hide integration and automatic suppression for genuine fullscreen applications.
- Stable adapter identity using 64-bit interface LUIDs, with safe rebind behavior after reconnects.
- Per-monitor V2 DPI awareness and taskbar-monitor positioning.
- Single-instance operation and automatic tray/meter recovery after Explorer restarts.
- One statically linked native executable; no .NET, Electron, Qt, WinUI, WebView, or third-party runtime framework.

## Installation

1. Download `WinNetMeter-v<version>-windows-x64.zip` or the standalone `WinNetMeter.exe` from [GitHub Releases](https://github.com/pyed/WinNetMeter/releases).
2. Extract the ZIP if needed, then run `WinNetMeter.exe`.
3. Keep the executable anywhere you prefer; WinNetMeter does not require installation or administrator rights.

Release binaries are currently unsigned, so Windows SmartScreen may show a warning the first time an unfamiliar build is run.

## Usage and settings

WinNetMeter starts in the notification area. Double-click the tray icon or taskbar meter to open the status window, where you can choose the network interface and view current speed and session totals. Right-click the tray icon to open Settings, show or hide the taskbar meter, show the status window, or exit.

Settings provide:

- Upload and download colors.
- Overlay font and style.
- Minimum speed unit: Auto, KB/s, MB/s, or GB/s.
- Decimal precision: 0, 1, or 2 places.
- A taskbar offset from `-4096` to `4096` logical pixels, clamped to the visible taskbar.

Settings are stored in:

```text
%APPDATA%\WinNetMeter\settings.ini
```

## Known limitations

On Windows 11, Start and some taskbar shell surfaces can temporarily draw over WinNetMeter's independent taskbar overlay even though Windows still reports the overlay as visible and topmost. WinNetMeter deliberately uses public, non-invasive Win32 APIs; it does not inject into Explorer or rely on undocumented shell APIs, UIAccess, or similar workarounds.

The meter may therefore be briefly visually occluded, particularly after clicking Start with the mouse, and returns when normal application focus resumes. This is separate from fullscreen handling: genuine fullscreen applications intentionally hide the meter until fullscreen exits.

## Building from source

Requirements:

- Windows x64.
- Visual Studio 2022 or Build Tools for Visual Studio 2022 with the MSVC x64 C++ tools.
- A Windows SDK containing the resource compiler and Win32 headers/libraries.

From the repository root:

```cmd
cd native
build.bat
```

The build script locates an installed MSVC toolchain when needed and compiles with C++20, `/W4`, `/WX`, `/permissive-`, `/MT`, and `/O2`. The output is:

```text
native\out\WinNetMeter.exe
```

Run the native unit suite with:

```cmd
cd native\tests
run_tests.bat
```

Built-executable integration checks are available through `native\tests\windows_integration_tests.ps1`. CI builds the x64 executable, runs the native suite, and verifies PE metadata, static-runtime linkage, and imports.

## Technical notes and privacy

WinNetMeter uses raw Win32/GDI, a layered window for the transparent meter, Windows IP Helper APIs for 64-bit interface counters, and `QueryPerformanceCounter` for monotonic sampling. Measurements stay local: WinNetMeter does not capture packets, transmit traffic data, or use telemetry or a cloud service.

The executable links the C/C++ runtime statically with `/MT`; only standard Windows system DLLs are required.

## Credits

WinNetMeter is an independent native C++20 rewrite inspired by [NetworkMonitorLite](https://github.com/mcagriaksoy/NetworkMonitorLite) by mcagriaksoy. It is not affiliated with or endorsed by the original project or author.

## License

Licensed under the [Apache License, Version 2.0](LICENSE).
