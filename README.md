<a href="https://github.com/mcagriaksoy/networkMonitorLite" title="Go to GitHub repo"><img src="https://img.shields.io/static/v1?label=mcagriaksoy&message=networkMonitorLite&color=blue&logo=github" alt="mcagriaksoy - networkMonitorLite"></a>
<a href="https://github.com/mcagriaksoy/networkMonitorLite/releases/"><img src="https://img.shields.io/github/tag/mcagriaksoy/networkMonitorLite?include_prereleases=&sort=semver&color=blue" alt="GitHub tag"></a>
<a href="#license"><img src="https://img.shields.io/badge/License-Apache_v2-red" alt="License"></a>
<a href="https://github.com/mcagriaksoy/networkMonitorLite/issues"><img src="https://img.shields.io/github/issues/mcagriaksoy/networkMonitorLite" alt="issues - networkMonitorLite"></a>


NetworkMonitorLite™ is a tiny, dependency-free native Windows application that shows live upload/download speeds in a clean interface, system tray icon, and draggable taskbar widget. Lightweight, portable, and fully compatible with Windows 10 and Windows 11.

- **Zero dependencies** – Native C++20 Win32 binary, statically linked CRT (`/MT`), no .NET or VC runtime DLL requirements
- **Tiny footprint** – Single ~400KB executable, ultra-fast and resource-friendly
- **Per-Monitor V2 DPI** – Crisp scaling on multi-monitor setups with different scaling factors
- **No malware, no bloat** – Zero telemetry, zero updaters, zero background socket connections
- **Instant visibility** – Real-time speeds using monotonic 64-bit IP Helper API counters
- **No installation required** – 100% portable

## Features
- Live download and upload speed monitoring per active network interface (Ethernet, Wi-Fi, VPNs)
- Minimal taskbar widget you can drag and place near the system tray (monitor-aware)
- Tray icon with dynamic live speeds (zero GDI leaks)
- Native Settings dialog to customize:
  - Widget background color
  - Download and upload text colors
  - Font family, size, and style
- Settings persist across sessions in `%APPDATA%\NetworkMonitorLite\settings.ini`

## Screenshots

Main UI:

![Main Window](img/ui.png)

How to Open the taskbar widget:

![how to use the taskbar widget?](img/how_to_display.gif)

Settings UI:

![Settings Dialog](img/settings.png)

## Requirements
- Windows 10 or Windows 11 (x64)

## Build and Run

### Native x64 Build (Recommended)
From Visual Studio Developer Command Prompt or x64 Native Tools:

```cmd
cd native
build.bat
```

The output executable is generated at `native\out\NetworkMonitorLite.exe`.

### Running Unit Tests
```cmd
cd native\tests
run_tests.bat
```

### C# / .NET Legacy Build
```powershell
cd networkMonitorLite
dotnet build
dotnet run
```

## Usage
1. Pick a network interface from the dropdown in the main window.
2. Watch live speeds (Download/Upload) and total transfer amounts.
3. Use the system tray icon menu:
   - **Show Window**: Bring main window to front
   - **Settings…**: Open native customization dialog
   - **Show Taskbar Widget**: Toggle the draggable overlay
   - **Exit**: Clean shutdown

### Settings
- Right-click the tray icon and choose "Settings…" to customize:
  - Taskbar widget background color
  - Download and upload text colors
  - Font family, size, and style
- Settings are saved to:
  - `%APPDATA%\NetworkMonitorLite\settings.ini`

## Support
- Author: mcagriaksoy — https://github.com/mcagriaksoy/NetworkMonitorLite
