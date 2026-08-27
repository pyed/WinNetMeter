# NetworkMonitorLite — Native Rewrite Spec

Persistent source of truth. Re-read before major edits. Update only when an agreed requirement deliberately changes.

## Goal
Replace the C#/.NET WinForms app with a **single small x64 native Windows executable**
(`NetworkMonitorLite.exe`) that preserves behavior, minus .NET architecture.
Original sources stay in `networkMonitorLite/` until the rewrite covers them, then are removed.

## Non-negotiables
- **Language/toolchain:** C++20, MSVC, x64, `/W4 /permissive-` (add `/WX` when clean),
  Release uses **static CRT `/MT`** → no VC runtime DLLs next to exe.
- **Dependencies:** none beyond Windows system DLLs (kernel32, user32, gdi32, shell32, iphlpapi, etc.).
  No .NET, Qt, wxWidgets, WinUI, WebView, Boost, vcpkg, NuGet, third-party JSON, CUDA. No new DLLs bundled.
- **Architecture:** tiny. ~5 small files (`main.cpp`, `network.cpp/h`, `settings.cpp/h`).
  No frameworks, DI, service locators, factories, event systems, speculative extensibility.
- **UI:** raw Win32 — `CreateWindowExW`, `WM_PAINT`, GDI, `SetWindowPos`, native menus,
  `Shell_NotifyIconW`. Unicode APIs everywhere. No UI framework. Crisp, flicker-free (double buffer if needed).
- **Network stats:** IP Helper API only (`GetIfTable2` / `MIB_IF_ROW2`, 64-bit `InOctets`/`OutOctets`).
  No packets, no sockets, no traffic inspection, no HTTP. Throughput = delta bytes / actual elapsed time
  from a monotonic clock (`QueryPerformanceCounter`/`steady_clock`) — never assume 1000 ms ticks.
  Handle counter reset/wrap, adapter disappear/reappear, index changes, VPN/VPN-ish virtual IFs, sleep/resume.
  Never crash on adapter changes.
- **Adapter identity:** stable Windows identity (interface index/LUID), not display name alone.
- **DPI:** Per-Monitor DPI Awareness V2 (`SetProcessDpiAwarenessContext`), handle `WM_DPICHANGED`
  (rescale + reposition), multi-monitor, fonts, taskbar position. No 96-DPI assumptions.
- **Taskbar/window positioning:** preserve original behavior; honor taskbar edge (not always bottom),
  work area, DPI, multiple monitors, taskbar resize (`WM_SETTINGCHANGE`/`TaskbarCreated`).
- **Tray:** `Shell_NotifyIconW` — tooltip, context menu, show/hide, Exit;
  re-register on `TaskbarCreated` (Explorer restart) without relaunch; full cleanup on exit.
- **Shutdown:** tray→Exit removes tray icon, kills timers, destroys windows, frees all GDI/menu/icon
  resources, exits message loop cleanly, leaves no background processes.
  Window close button keeps original behavior unless improving it is clearly right.
- **Settings:** INI under `%APPDATA%\NetworkMonitorLite\` (Get/WritePrivateProfileStringW).
  Preserve the settings users actually need. No JSON dependency, no registry, no auto-start,
  no machine-wide changes. Handle missing/malformed settings gracefully.
- **Security/privacy:** zero telemetry, analytics, crash reporting, auto-update, downloads,
  shell/PowerShell/CMD execution, hooks, packet capture, credential access, admin rights,
  services, scheduled tasks, startup persistence. Only unsolicited outbound network = none.
  User-clicked GitHub link via `ShellExecuteW` is allowed if the original has it.
- **Resources/artwork:** reuse existing icon via native `.rc`. No asset-management code.
- **Performance:** tiny. One counter read per update interval, no background threads, no waste.
- **Build:** minimal CMake (or simpler if it's genuinely simpler), x64, C++20, MSVC, MSVC security
  features on. Do not disable security checks to silence warnings.

## Preserve (behavioral spec from the C# app)
To be filled in during the study phase: window look/size, speed display, update interval,
settings items, positioning logic, tray behavior, close behavior, links.

## Improve (authorized)
Better adapter identity, accurate timing, real DPI support, tray restore after Explorer restart,
safe resource lifetimes, cleaner settings, reliable shutdown, cleaner positioning.
Do **not** invent unrelated features.

## Testing bar
Compiling is not enough. Build repeatedly, run it, exercise: startup, throughput (idle/download/
upload), disconnect/reconnect/switch, tray menu, hide/show, exit, settings persistence,
missing/malformed INI, DPI scaling, Explorer restart, relaunch cycles, shutdown cleanliness.
Document what remains manual-only.
