# WinNetMeter

<a href="https://github.com/pyed/WinNetMeter" title="Go to GitHub repo"><img src="https://img.shields.io/static/v1?label=pyed&message=WinNetMeter&color=blue&logo=github" alt="pyed - WinNetMeter"></a>
<a href="https://github.com/pyed/WinNetMeter/releases/"><img src="https://img.shields.io/github/v/release/pyed/WinNetMeter?include_prereleases=&sort=semver&color=blue" alt="GitHub release"></a>
<a href="#license"><img src="https://img.shields.io/badge/License-Apache_v2-red" alt="License"></a>
<a href="https://github.com/pyed/WinNetMeter/issues"><img src="https://img.shields.io/github/issues/pyed/WinNetMeter" alt="issues - WinNetMeter"></a>

**WinNetMeter** is a lightweight, zero-dependency native Windows utility that displays real-time download and upload throughput through a dynamic system tray icon, a transparent taskbar meter, and an optional status window.

Built in modern C++20 for 64-bit Windows 10 and 11, it operates entirely via native Win32 and IP Helper APIs with zero runtime dependencies.

---

## Key Highlights

- **Zero Dependencies**: Pure native C++20 Win32 application compiled with static C Runtime (`/MT`). No .NET runtime, VC++ redistributables, or third-party DLLs required.
- **Tiny & Fast**: Single standalone executable (~400 KB) with minimal CPU and memory footprints.
- **Precise Throughput Measurement**: Uses Windows IP Helper APIs (`GetIfTable2` / `GetIfEntry2`) and a high-resolution monotonic clock (`QueryPerformanceCounter`) to sample 64-bit network octet counters.
- **Stable Adapter Identity**: Tracks network interfaces by stable 64-bit `NET_LUID`, surviving adapter re-indexing, VPN reconnects, and disconnect/reconnect cycles without data spikes.
- **Per-Monitor V2 DPI**: Crisp rendering and dynamic rescaling across multiple monitors with varying DPI scaling factors.
- **Privacy & Security**: Zero telemetry, zero analytics, zero background network connections, and no administrative elevation required.
- **Portable**: Fully self-contained; settings persist in `%APPDATA%\WinNetMeter\settings.ini`.
- **Single Instance**: Repeated launches hand off to the existing per-session process without duplicating the tray icon, sampler, or meter.

---

## Features

- **Live Speed Monitoring**: Real-time upload and download speeds and session totals for physical (Ethernet, Wi-Fi) and VPN/virtual network adapters.
- **Taskbar-Attached Meter**: Compact, transparent, non-activating overlay that follows the documented system-taskbar rectangle and DPI, with a signed taskbar-relative position offset in Settings.
- **Dynamic System Tray Icon**: Generates real-time speed icons in the notification area with leak-free GDI handle lifecycle management.
- **Customizable Appearance**: Settings dialog to choose a minimum throughput unit, 0/1/2 decimal places, download/upload text colors, and overlay font styling. Fresh configurations use white speed text by default.

---

## Screenshots

### Main Window & System Tray
![Main Window](img/ui.png)

### Taskbar Widget Placement
![How to use the taskbar widget](img/how_to_display.gif)

### Settings Dialog
![Settings Dialog](img/settings.png)

> *Note*: Existing screenshot assets depict the original UI layout and may show the earlier opaque, draggable widget.

---

## System Requirements

- **Operating System**: Windows 10 or Windows 11 (x64)
- **Privileges**: Standard user (no administrator privileges needed)

---

## Downloads & Releases

- **Pre-built Releases**: Download official packaged archives from [GitHub Releases](https://github.com/pyed/WinNetMeter/releases) (starting with initial release **v0.1.0**).
- **Self-Contained**: The release package (`WinNetMeter-v<version>-windows-x64.zip`) is fully standalone for x64 Windows with no .NET runtime or Visual C++ Redistributable requirements.
- **CI Artifacts**: Automated continuous integration builds are produced by GitHub Actions on every commit to `main`.
- **Code Signing**: Binaries are currently unsigned; standard Windows SmartScreen prompts may appear on first run.

---

## Building from Source

### Prerequisites
- **Visual Studio 2022** or **Build Tools for Visual Studio 2022** (with *Desktop development with C++* workload)
- **Windows 10/11 SDK**

### Build Instructions
Open a **Developer Command Prompt for VS 2022** (or `x64 Native Tools Command Prompt`) and run:

```cmd
cd native
build.bat
```

The build compiles with strict flags (`/W4 /WX /permissive- /std:c++20 /O2 /MT`) and outputs the standalone executable to:

```text
native\out\WinNetMeter.exe
```

> **Note**: Statically linking the CRT via `/MT` embeds the necessary C/C++ runtime routines directly into the binary, allowing `WinNetMeter.exe` to run on any clean Windows installation without installing Visual C++ Redistributable packages.

---

## Running the Test Suite

The repository includes a comprehensive, dependency-free native regression and unit test harness:

```cmd
cd native\tests
run_tests.bat
```

### Verified Test Cases
- **Speed & Byte Formatting**: Exact representation and rounding for adaptive or minimum-unit B/s, KB/s, MB/s, and GB/s displays, plus independent byte totals and compact tray strings.
- **Monotonic Timing**: Monotonic throughput calculation using actual high-resolution clock deltas.
- **64-bit Counter Accumulation & Wrap**: Correct delta calculations with 64-bit counters exceeding 4 GB.
- **Adapter Rebind & Fallback**: Proof that interface reconnection with a new LUID establishes a zero-speed baseline without spikes, preserves totals, and never falls back to an unrelated adapter.
- **Settings INI Roundtrip**: Reading, parsing, and persisting colors, fonts, styles, and taskbar position offset to INI at `%APPDATA%\WinNetMeter\settings.ini`.
- **Zero GDI Resource Leaks**: Stress-tested across 200 icon and font lifecycles verified via `GetGuiResources` (`GR_GDIOBJECTS` and `GR_USEROBJECTS`).
- **Taskbar Geometry & Alpha**: Deterministic coverage for taskbar edges, signed offset/clamping, negative monitor coordinates, DPI scaling, and premultiplied per-pixel alpha.

Built-EXE integration checks are also available from PowerShell through `native\tests\windows_integration_tests.ps1` for instance, HWND, foreground z-order, metadata, import, and live resource-lifetime verification.

---

## Configuration & Settings

Settings are stored in standard INI format at:

```text
%APPDATA%\WinNetMeter\settings.ini
```

Example configuration:
```ini
[Overlay]
FontFamily=Segoe UI
FontSize=8.0
FontStyle=1
ShowWidget=1
TaskbarOffset=0
MinimumSpeedUnit=Auto
DecimalPlaces=2
DownloadColor=16777215
UploadColor=16777215
```

---

## Attribution & Origins

WinNetMeter is an independent native C++20 rewrite inspired by [NetworkMonitorLite](https://github.com/mcagriaksoy/NetworkMonitorLite) by mcagriaksoy.

WinNetMeter is independently maintained and is not an official release by the original author.

---

## License

Licensed under the [Apache License, Version 2.0](LICENSE).
