# Handoff

Living status document. Update it at every milestone (see `CLAUDE.md`).

## Status

- Released: **v0.1.6** (tag `v0.1.6`). `main` is at the 0.1.6 code plus CI changes.
- In progress: the **0.2.0** work below, driven by the second audit (2026-10-08).
- Last completed milestone: **M12** (shell fullscreen signal); **M13** (release) in progress.

## 0.2.0 milestone plan

Order is chosen so that test isolation comes first (the suite is run constantly next to a
live deployed instance), low-risk fixes come before the meter-thread refactor, and the
refactor lands with no behavior change before features are built on it.

- [x] **M0** `CLAUDE.md` + this file.
- [x] **M1** Test isolation and tooling: `--settings <path>` and a separate Run-key value name
      in `--integration-test` mode; harness uses a temp settings file; `run_tests.bat` uses
      `.\unit_tests.exe`; CI actions `checkout`/`upload-artifact` v4 -> v7.
- [x] **M2** Settings save hardening: POSIX-semantics rename, retry, in-place fallback; fix the
      stacking save-failure dialog.
- [x] **M3** Application manifest (Common Controls v6, PerMonitorV2, supportedOS, asInvoker);
      drop the runtime DPI call; `Metadata` check asserts the manifest.
- [x] **M4** Adapter selection: "Automatic" mode that follows the default route (new default),
      remembered manual choice, re-evaluated while running.
- [x] **M5** Formatting: bits-per-second option; locale-aware "since" date.
- [x] **M6** Theme-aware default meter colours, with a settings-version migration.
- [x] **M7** Tray icon rendered at the shell's icon size for the taskbar DPI, with alpha.
- [x] **M8** Meter thread: move everything taskbar-related (meter windows, rendering, WinEvent
      hooks) to a dedicated thread fed by state snapshots. No behavior change.
- [x] **M9** Placement anchors (next to tray / after apps / left edge / legacy) with offsets
      relative to the anchor; legacy files keep their exact old position.
- [x] **M10** Embedded mode (opt-in): the meter is a layered child window of the taskbar, so it
      stays visible while Start is open. Automatic fallback to the overlay.
- [x] **M11** Meters on secondary-monitor taskbars (opt-in).
- [x] **M12** Shell fullscreen signal (`ABN_FULLSCREENAPP`) as an extra trigger, if it proves
      reliable for an appbar that reserves no space.
- [ ] **M13** README, version 0.2.0, push, CI, tag, release, deploy to the dev machine.

## Verified findings that drive this work

From the 2026-10-08 audit. "Confirmed" = reproduced on the dev machine.

1. **Adapter can get stuck (confirmed mechanism).** `PopulateAdapters(true)` picks the first
   adapter whose OperStatus is Up, in `GetIfTable2` order; the choice is not persisted; and the
   timer only re-scans when `NetSampler::Sample()` fails, which it never does for an adapter
   that exists. On the dev machine, three zero-traffic virtual adapters
   (`Local Area Connection* 6/7/8`, type 6, Up) sort ahead of Wi-Fi, so a Wi-Fi-only session
   would meter one of them at 0 B/s forever. With nothing Up at launch, index 0
   (`Ethernet (Kernel Debugger)`) is picked.
2. **No application manifest (confirmed).** 0 `RT_MANIFEST` resources; the process binds
   comctl32 5.82 (legacy controls). Adding a standard manifest in a scratch build loaded only
   comctl32 6.0 and passed 15/15 integration checks. `SetProcessDpiAwarenessContext` is a
   static import, so the exe cannot start on Windows 10 before 1703.
3. **Tray icon fixed at 16 px (confirmed).** At 288 DPI (300%) `SM_CXSMICON` is 48.
4. **0.1.6 atomic save regression (confirmed, rare).** `MoveFileExW(REPLACE_EXISTING)` fails
   with error 5 whenever another handle is open on `settings.ini`, even with full sharing.
   The old in-place write succeeded with READ|WRITE|DELETE and READ|WRITE holders.
   0 failures in 600 saves under Defender real-time scanning alone. A POSIX-semantics rename
   (`SetFileInformationByHandle(FileRenameInfoEx, REPLACE_IF_EXISTS | POSIX_SEMANTICS)`)
   succeeds with a READ|WRITE|DELETE holder; a READ|WRITE holder needs an in-place fallback.
5. **Stacking save-failure dialog (traced).** `ReportSettingsSaveFailure` clears
   `g_settingsSaveFailed` only after `MessageBoxW` returns; double-clicking the meter during
   the modal loop opens a second box.
6. **Offset anchored to a hardcoded point (measured).** `overlay.h` places the meter at
   `taskbar.right - 350 logical px - width + offset`. `TaskbarOffset=-796` means "left edge"
   only on a 3840 px / 300% display; on 1920 px / 100% it lands at x=642.
7. **Integration tests are not isolated (traced).** They write the real settings file; the
   `Preferences` check deletes and later restores the real Run-key value.
8. Smaller: `run_tests.bat` runs `unit_tests.exe` by bare name; CI actions pinned at v4 (v7 is
   current, v4 runs on the deprecated Node 20 runtime); the "since" date is hardcoded
   DD/MM/YYYY; README describes the CI and the known limitation inaccurately.

Rejected during verification (do not re-investigate): `1024.00 KB/s` rounding is asserted as
intended by the unit tests; `IsStartWithWindowsEnabled` sizing does not overflow (canary test);
the WinEvent location-hook redraw cost is about 1.6% of one core while dragging.

## Platform facts (experiments on Windows 11 build 26300)

Probes are solid-colour windows over an empty part of the taskbar; visibility was read from
DWM-composed screen pixels.

- Every normal window, including `Shell_TrayWnd` and the meter, is in z-order **band 1**.
  When Start opens, the shell moves `Shell_TrayWnd` to **band 6** (the band of the Start
  `Windows.UI.Core.CoreWindow`, hosted by `SearchHost.exe` on this build). Nothing in band 1
  can be drawn over band 6, so a `WS_EX_TOPMOST` overlay is covered while Start is open, and
  re-raising it then has no effect.
- When Start closes, the taskbar returns to band 1 **above** the overlay. A re-raise issued
  ~50 ms after closing is lost (the taskbar is still in band 6); one issued after it returns
  to band 1 works. The shipped 0.1.6 app recovered ~300 ms after Start closed and was not
  occluded when the taskbar took focus.
- Measured in M10 with the real Start menu (`StartMenu` check, magenta meter, pixels counted
  on screen): overlay **0 of N** pixels visible while Start is open, embedded **N of N**.
  After Start closes, the overlay is back 200-950 ms after Start loses the foreground when
  only the 1 s refresh re-raises it, and within ~150 ms with the 100 ms re-raise burst.
- WinNetMeter does not hide itself while Start is open: Start's window (720,108)-(3120,2016)
  does not cover the 3840x2160 monitor, so the fullscreen heuristic stays false.
- **Embedding:** a plain child window of `Shell_TrayWnd` is invisible even with Start closed
  (the XAML taskbar content composites over child windows; the probe was the topmost sibling).
  A plain child of `Windows.UI.Composition.DesktopWindowContentBridge` is also invisible.
  A **`WS_EX_LAYERED` child of `Shell_TrayWnd` is visible** with Start closed, open, 50/300/
  1000 ms after closing, and with the taskbar focused. Layered child windows require a
  manifest that declares Windows 8+.
- Cross-process parent/child windows attach the two threads' input queues: if the thread that
  owns the child blocks, taskbar input can stall. Only the meter thread may own such windows,
  and it must never block (no disk I/O, no modal UI, no `SendMessage` to other processes it
  can avoid).
- Taskbar children readable on this build: `TrayNotifyWnd` (tray area), `ReBarWindow32` ->
  `MSTaskSwWClass` (task-button area), `Windows.UI.Composition.DesktopWindowContentBridge`
  (full-width XAML host). The Start button and Widgets have no HWND.
- UIAccess would also place a window above Start, but needs a signed exe installed under
  Program Files. Not pursued (incompatible with portable, unsigned distribution).

## Dev machine notes

- Windows 11 build 26300, one 3840x2160 monitor at 300% scaling, dark taskbar, centered icons.
  On 2026-10-09 the display was 1280x720 at 100% instead (changed outside this work); all
  checks pass in both configurations, so do not hardcode either.
- A deployed WinNetMeter runs from a user folder with autostart (Run key). Its settings use
  `TaskbarOffset=-796` (meter at the far left), tray icon off, `MinimumSpeedUnit=KB/s`.
  New releases must keep that placement after an upgrade.
- `NoDefaultCurrentDirectoryInExePath=1` is set (see `CLAUDE.md`).

## Milestone log

### M0: documentation (2026-10-08)
Added `CLAUDE.md` (repo guidance, including the rule to update this file per milestone) and
this handoff. No code changes. Baseline before starting: build clean, all unit tests pass,
15/15 integration checks pass locally.

### M1: test isolation and tooling (2026-10-08)
- `--integration-test` now also redirects the settings file (`--settings <path>`, else
  `%TEMP%\WinNetMeter-integration-test\settings.ini`) and uses the Run-key value name
  `WinNetMeter.IntegrationTest` (`ApplyCommandLine` in `main.cpp`; `SetSettingsPathOverride` /
  `SetStartupValueName` in `settings.cpp`). `--settings` is ignored outside test mode.
- The harness creates a per-run temp directory (`WinNetMeter-it-<guid>`), passes it to every
  launch, and deletes it in `finally`. `Preferences` uses the test value name and asserts the
  real `WinNetMeter` value was not written. The old backup/restore of the real file and real
  Run value is gone.
- The settings-path fallback (only when `SHGetFolderPathW` fails) is now absolute, next to the
  exe; the old bare `settings.ini` was read from `%WINDIR%` but written to the working dir.
- `run_tests.bat` calls `.\unit_tests.exe`. CI and release use `actions/checkout@v7` and
  `actions/upload-artifact@v7` (not yet exercised on CI; first push will tell).
- New unit test `TestSettingsPathOverride`.
- Verified: build clean; unit tests pass via `.\run_tests.bat`; 15/15 integration checks;
  the real settings file's user keys and the real Run value were unchanged across the run
  while the deployed instance kept running; no temp directories or test Run values left.
- Gotcha: GNU `sed` treats `\u` in a replacement as "uppercase next char"; it turned
  `.\unit_tests.exe` into `.Nit_tests.exe`. Use the editor for edits containing backslashes.

### M2: settings save hardening (2026-10-08)
- `WriteFileAtomic` (`settings.cpp`) now swaps the temp file in with
  `SetFileInformationByHandle(FileRenameInfoEx, REPLACE_IF_EXISTS | POSIX_SEMANTICS)`, falling
  back to `MoveFileExW`; retries up to 4 times (15/30/45 ms) on access-denied or sharing
  errors; then rewrites the file in place (`TRUNCATE_EXISTING`, full sharing), which is what
  pre-0.1.6 releases did. Worst-case extra delay on a genuine failure: ~90 ms.
- `SaveSettingsCustom` normalizes the path with `GetFullPathNameW` (the rename API needs a
  fully qualified target).
- `ReportSettingsSaveFailure` (`main.cpp`) clears the flag before showing the box and refuses
  to re-enter, so the modal loop cannot stack dialogs; a new failure is reported once, later.
- Tests written first and seen failing on the old code: unit `TestSaveWithOpenHandles`
  (holder with read|write|delete sharing -> saved atomically; read|write -> saved in place;
  read only -> fails, file unchanged) and integration check `SaveFailureDialog` (read-only
  test settings file, two open requests -> exactly one dialog; the old code showed 2). The
  stacking-dialog bug is therefore reproduced, not just traced.
- CI now reads the behavioral check list from the harness `ValidateSet` (minus the three PE
  checks run by the build job), so new checks run in CI automatically. Fails if fewer than 12
  are parsed.
- `.gitignore`: `src/tests/*.ini`, `*.ini.tmp`, `test_override/` (left only by aborted tests).
- Verified: build clean; 40 unit tests; 16/16 integration checks; isolation guard clean.

### M3: application manifest (2026-10-08)
- `src/app.manifest`, embedded by `app.rc` as resource 1 of type 24: Common Controls 6,
  `asInvoker` (`uiAccess=false`), supportedOS Windows 10 (also what makes layered child windows
  legal for M10), `PerMonitorV2` with a `true/pm` fallback. No `assemblyIdentity`, so there is
  no version string to keep in sync with `version.h`.
- Removed the runtime `SetProcessDpiAwarenessContext` call; the manifest sets awareness at
  process creation. `InitCommonControls()` (a no-op under v6) became
  `InitCommonControlsEx(ICC_STANDARD_CLASSES | ICC_UPDOWN_CLASS)`.
- Minimum OS is now **Windows 10 1607**: the only DPI-era import left is `GetDpiForWindow`.
  Mention this in the README (M13); re-check imports if later milestones add APIs.
- Harness: `Metadata` asserts the manifest content (read via `LoadLibraryEx` +
  `FindResource`); `WindowStyles` asserts the process loads comctl32 6.x and not 5.82; `Dpi`
  asserts the overlay window is PerMonitorV2. Negative control: the manifest assertion
  reports "absent" on a pre-manifest build.
- Verified: build clean; 40 unit tests; 16/16 integration checks; isolation guard clean.

### M4: automatic adapter selection (2026-10-08)
- New in `network.cpp`: `GetDefaultRouteLuid` (`GetBestInterfaceEx` to 1.1.1.1, then
  2606:4700:4700::1111; addresses are written byte-wise so there is no ws2_32 import) and the
  pure `ChooseAdapter`. Mobile broadband types 243/244 are now listed.
- Settings: `[Network] Adapter=auto | <16 hex LUID>`, `AdapterName=<hex-encoded alias>`
  (`AppSettings::adapterAuto/adapterLuid/adapterAlias`). Missing or invalid -> Automatic, so
  every 0.1.x user starts in Automatic (on the dev machine that resolves to Ethernet, the
  adapter 0.1.6 happened to pick).
- `main.cpp`: combo item 0 is "Automatic (<default-route alias>)", adapters follow (the combo
  maps item i+1 to `g_comboLuids[i]`). Automatic re-resolves every tick and switches with
  `Rebind` (session totals kept); a user pick switches with `Reset` and is saved at once.
  Manual mode recovers a vanished adapter by LUID, then unique name, every 3 s, and never
  substitutes another adapter. The combo is no longer rebuilt every 3 s while disconnected.
  `ApplySettings` now preserves the adapter fields (it previously would have reverted a combo
  change made while the window was open, the same way it already protected the totals).
- Tests: `TestChooseAdapter` (includes the dev-machine adapter order that exposed the bug),
  `TestAdapterSettings` (round trip, legacy file, garbage values), `TestDefaultRouteLookup`
  (live); `TestAdapterRebindAndNoFallback` now calls `ChooseAdapter` instead of re-implementing
  it. Integration `AdapterSelection`: Automatic selected and labelled with the default-route
  adapter (compared against `GetBestInterface` + .NET; exercised here: "Ethernet"), manual
  choice saved and restored across a restart, switching back saves `auto`.
- Verified: build clean; 44 unit tests; 17/17 integration checks; isolation guard clean.

### M5: bits per second and localized date (2026-10-08)
- `FormatSpeed`/`FormatCompact` take `bool bits = false`. Bits use decimal multiples
  (`bps/Kbps/Mbps/Gbps`, 1 Mbps = 10^6 bit/s); the minimum-unit floor maps to Kbps/Mbps/Gbps.
  Compact bits are `800b`, `1K`, `100M`; the multiply saturates instead of wrapping. The
  byte path is unchanged (its tests are untouched).
- Setting `[Overlay] SpeedUnits=bytes|bits` (`AppSettings::speedBits`; missing/invalid ->
  bytes). UI: "Show speed in" combo (IDs 2029/2030); the minimum-unit combo relabels its
  choices in the chosen units (`RelabelMinimumUnitChoices`); stored values are unchanged.
- `FormatLifetimeSinceDate` uses `GetDateFormatEx(DATE_SHORTDATE)` with an optional locale
  (unit tests pin `en-GB`/`en-US`/`de-DE`); buffers widened for long formats.
- Settings panel laid out in its final shape now (window height 490 -> 590 logical). Free
  slots: y=322 for the M9 position row, y=427 right for M11, y=457 for M10.
- Tests: `TestBitRateFormatting`, `TestSpeedUnitsSetting`, `TestLocalizedSinceDate`;
  integration `SpeedUnits`; `CustomizationTotals` computes its expected date with the same
  API (it hardcoded DD/MM, which would fail on en-US runners).
- Verified: build clean; 48 unit tests; 18/18 integration checks; isolation guard clean;
  screenshot of the new window checked.

### M6: theme-aware colours (2026-10-08)
- `METER_COLOR_AUTO` (0xFF000000, never produced by ChooseColor) is the new default for both
  colours. `ResolveMeterColor` maps it to white on a dark taskbar and RGB(28,28,28) on a
  light one (`IsSystemThemeLight` reads `HKCU\...\Themes\Personalize\SystemUsesLightTheme`).
  `g_taskbarLight` is read at startup and on `WM_SETTINGCHANGE("ImmersiveColorSet")`.
- Files now carry `[General] SettingsVersion=2`. Migration: a file without it (0.1.x) that
  has white (16777215, the old default) gets Automatic; any other colour is kept. The dev
  machine's deployed file has white for both, so it migrates to Automatic and still renders
  white on its dark taskbar. Colours are saved as `auto` or decimal; values above 0xFFFFFF or
  non-numeric fall back to Automatic.
- UI: an "Automatic" checkbox beside each colour button (IDs 2031 up, 2032 down). Picking a
  colour unchecks it; unchecking pins the colour currently on screen. `PickColor` now reports
  Cancel (it used to re-apply and save the unchanged colour) and opens on the resolved colour.
- Contract change, deliberate: "Reset meter" restores Automatic colours, so
  `CustomizationTotals` now expects `auto` instead of 16777215.
- Tests: `TestFreshColorDefaults` (now Automatic, still white on dark), `TestMeterColorResolution`,
  `TestMeterColorSettings` (migration, explicit, garbage, round trip); integration
  `ThemeColors` (legacy migration, pin on screen colour computed from the same registry value,
  back to auto).
- Verified: build clean; 50 unit tests; 19/19 integration checks; isolation guard clean.

### M7: DPI-sized tray icon (2026-10-08)
- New `src/render.{h,cpp}` (added to `build.bat` and `run_tests.bat`, so icon rendering is
  unit-testable): `GetTrayIconSizeForDpi` (`SM_CXSMICON` for a DPI) and `CreateMeterIcon`
  (two lines, transparent background, largest bold Segoe UI that fits, clipped below a third
  of the height). `main.cpp` sizes it for the taskbar window's DPI.
- Measured, not assumed: `CreateIconIndirect` icons take **straight** alpha (50% white over
  black came out 128; a premultiplied pixel came out 64). Hence `ApplyIconAlpha` in
  `overlay.h`, separate from the premultiplied `ApplyOverlayAlpha` used for layered windows.
- The old icon used `DEFAULT_QUALITY` text on an opaque dark square, which renders ClearType
  colour fringes; scaled 3x at 300% it was unreadable. A side-by-side render confirmed the
  new 48 px icon is crisp.
- Tests: `TestIconAlpha`, `TestMeterIcon` (sizes 16..64, 32-bit, transparent corners, drawn
  text, and no GDI growth over 150 icons). First run of that test failed: the first icon
  allocates 5 GDI objects for process-wide GDI/font caches. A probe showed the count flat
  across 1000 further icons (no leak), so the test now warms up first. Unit test stdout is
  unbuffered so diagnostics print before an `assert` aborts.
- Verified: build clean; 52 unit tests; 19/19 integration checks; isolation guard clean.

### M8: meter thread (2026-10-08)
- New `src/meter.{h,cpp}` (in `build.bat`; not in the unit-test build). A dedicated thread
  owns a message-only command window (`WinNetMeterMeterHost`), the `WinNetMeterOverlay`
  window, both WinEvent hooks and the overlay font. The UI thread calls `UpdateMeter()`
  (`main.cpp`), which posts a heap `MeterState` snapshot; the meter thread takes ownership,
  coalesces queued snapshots, and renders. It never sends to the UI thread: a double-click
  posts `WM_OPEN_FROM_METER`. `StartMeterHost` runs in `WM_CREATE`; `StopMeterHost` in
  `WM_DESTROY` (waits up to 3 s; the meter thread never waits on the UI thread, so no
  deadlock). Overlay logic was ported unchanged (fullscreen heuristic, auto-hide, hooks).
- Renders are throttled on the leading edge to at most one per 33 ms; `RenderOverlay` no
  longer uses `GetDC(NULL)` (memory DC + null destination DC for `UpdateLayeredWindow`) and
  only passes `SWP_SHOWWINDOW` when the window is hidden.
- Test contract changes (deliberate): `WindowStyles`/`Preferences` wait for the window to
  appear after a meter double-click (`Wait-WindowVisible`), since it is now posted.
  `ResourceLeak` was redesigned, see below.
- **GDI investigation (keep this):** with the meter thread, a 200-tick burst left +1/+2 GDI
  objects. Per-type counts (read from the GDI shared handle table, `GdiCounts` in the
  harness) showed REGIONs, later also a DC. Facts: the 0.1.x/M7 single-threaded build stays
  flat under the same burst; with the tray icon off the M8 build stays flat; at the real
  1 Hz cadence it stays flat for 40 s; 5000 burst ticks plateau at +1; counts sometimes fall
  back by themselves. Conclusion: window-manager caches (regions, cached DCs) that grow when
  the UI thread's tray update overlaps the meter thread's layered-window update; bounded,
  not a leak. The throttle cut the frequency sharply but cannot remove it.
  `ResourceLeak` therefore runs a 200-tick warm-up, then 600 measured ticks, and asserts
  USER objects exactly equal and GDI growth <= 3 (catches leaks of 1 per 150 ticks or
  faster). Negative control: the same build with `DestroyIcon` removed from the tray update
  fails with USER 264 -> 866.
- Gotcha: **PowerShell passes `$null` to a .NET `string` parameter as `""`.**
  `FindWindowW('Class', $null)` therefore searches for an *empty title* and silently misses
  titled windows; two early probes sent all their ticks to a null window. Use
  `[NullString]::Value`.
- Verified: build clean; 52 unit tests; 19/19 integration checks; 8/8 `ResourceLeak` runs;
  5/5 each for ForegroundZOrder, Fullscreen, WindowStyles, Preferences, ExplorerRecovery.

### M9: placement anchors (2026-10-08)
- `overlay.h`: `MeterAnchor` {Legacy, BesideTray, AfterApps, LeftEdge}, `TaskbarLayout`
  (taskbar, edge, tray, apps), `CalculateAnchoredMeterRect` (pure; 4 logical px gap from the
  anchor part; offset moves along the taskbar; clamped inside the padding; vertical taskbars
  run top to bottom). `CalculateMeterBox` is shared with the legacy function, which Legacy
  still uses unchanged. A missing or off-taskbar part falls back (tray -> far end, apps ->
  left edge).
- `meter.cpp` reads the parts each render: `TrayNotifyWnd` and `ReBarWindow32` ->
  `MSTaskSwWClass` under `Shell_TrayWnd` (visible ones only).
- Settings: `[Overlay] Anchor=tray|apps|left|legacy` (`AppSettings::meterAnchor`). A file
  with `TaskbarOffset` but no `Anchor` (every 0.1.x file) loads as Legacy, so the dev
  machine's `-796` keeps the meter at the far left after the upgrade. Otherwise the default
  is "next to the tray". Unknown values -> tray.
- UI: "Meter position" combo (IDs 2033/2034; order tray, apps, left, classic). Changing it
  resets the offset to 0 (offsets are anchor-relative); "Reset meter" restores tray/0.
- Tests: `TestMeterAnchors` (horizontal, fallbacks, 300%, vertical), `TestAnchorSettings`
  (round trip, legacy detection, defaults); integration `Anchors` (exact pixel placement
  against the live `TrayNotifyWnd` / `MSTaskSwWClass` rects, offset reset, persistence);
  `CustomizationTotals` asserts its 0.1.x-style file stays Classic and reset restores tray.
- Gotcha: PowerShell unwraps `Nullable<T>` returned from .NET, so use `$rect.Left`, not
  `$rect.Value.Left`.
- Verified: build clean; 57 unit tests; 20/20 integration checks (Anchors 3/3 extra runs).

### M10: embedded mode (2026-10-09)
- `meter.cpp`: with `MeterState::embedded`, the meter is a `WS_EX_LAYERED | WS_EX_NOPARENTNOTIFY`
  `WS_CHILD` window of class `WinNetMeterEmbedded` under `Shell_TrayWnd`, drawn with the same
  `PaintMeter` as the overlay (shared now) at parent-client coordinates (`MapWindowPoints`
  with a rect, so a mirrored RTL taskbar maps correctly; not testable here) and kept
  `HWND_TOP` among the taskbar's children on every render. Only one of overlay/child exists
  at a time. Layout uses the taskbar's DPI and its window rect, so an auto-hide taskbar
  carries the meter. No fullscreen or auto-hide tracking is needed: the child hides with the
  taskbar. If the child cannot be created (no taskbar, or creation fails), the overlay is used.
  A taskbar that is recreated (Explorer restart) is detected by `IsWindow`/parent checks on the
  next render (at most 1 s). **Not exercised with a real Explorer restart** (too disruptive on
  the dev machine); `TaskbarCreated` itself is tested.
- The window procedure handles `WM_SETCURSOR`, `WM_MOUSEWHEEL/HWHEEL`, `WM_CONTEXTMENU` and
  `WM_APPCOMMAND` itself, because `DefWindowProc` forwards those from a child to its parent with
  `SendMessage`, i.e. a cross-process wait on Explorer. `WM_DPICHANGED_AFTERPARENT` refreshes.
- Overlay mode improvement found while testing Start: after a foreground change the overlay is
  re-raised immediately and then every 100 ms for 800 ms (`RAISE_TIMER`), because the taskbar
  drops back from band 6 after the foreground has moved and lands above the overlay.
- Setting `[Overlay] Embed=0|1` (`AppSettings::embedInTaskbar`, default 0, any non-zero = on).
  UI: checkbox 2035 "Embed in taskbar (stays visible over Start)" at y=457, applied with
  **Apply** like the other check boxes beside it. "Reset meter" does not change it (it resets
  looks and position, not the mode).
- Opt-in, not default, because it places a window inside Explorer's taskbar: untested
  third-party taskbar mods and future shell changes could break it, while the overlay is the
  long-proven path.
- Tests: unit `TestEmbedSetting`. Integration `Embedded` (child of the taskbar, `WS_CHILD` +
  layered, no overlay, taskbar DPI, size, exact tray-relative placement, double-click opens
  the window, `TaskbarCreated` does not duplicate it, reset keeps the mode, Apply switches to
  the overlay and back and persists). `StartMenu` opens the real Start menu with the Win key
  (Escape only while Start has the foreground) and counts magenta meter pixels on screen:
  overlay covered while open (informational), overlay back within 450 ms of Start closing in
  3 of 3 timed closes, embedded visible while open. Prints `START_MENU_SKIPPED` (and a GitHub
  warning) if Start cannot be opened. `ResourceLeak` now measures both modes.
- Negative control for the re-raise burst: without it, `StartMenu` failed 4 of 4 runs (overlay
  back after up to 953 ms). With it, 8 of 9 runs passed (back 11-123 ms after Start lost the
  foreground); the failure was the first close after a long idle (cold Start menu), so the
  check now opens and closes Start once, untimed, before the timed closes. With that warm-up:
  6 of 6 passed (5-125 ms).
- Harness gotchas: `ABM_GETTASKBARPOS` does not fill `APPBARDATA.hWnd` (use
  `FindWindow("Shell_TrayWnd")`); under StrictMode, `$x = if (...) { @() }` is `$null`, so wrap
  such assignments in `@(...)`.
- Verified: build clean; 58 unit tests; 22/22 integration checks; `StartMenu` 5/5 extra runs;
  `ResourceLeak` flat in both modes (GDI 21->21; USER 68->68 overlay, 67->67 embedded);
  isolation guard clean.

### M11: meters on all taskbars (2026-10-09)
- `meter.cpp` now keeps one `MeterSlot` per taskbar: the primary (`Shell_TrayWnd`, located
  through the appbar API as before) and, when `MeterState::allTaskbars` is set, every visible
  top-level `Shell_SecondaryTrayWnd`. Each slot has its own overlay or embedded child (same
  mode for all, same per-slot fallback to an overlay). Slots are reconciled on every sync
  (at least once a second): taskbars that appear get a meter, ones that disappear (monitor
  unplugged) lose theirs. Hooks and the fullscreen heuristic treat any slot window as ours.
- Secondary taskbars have no appbar API: their edge comes from the window and monitor rects
  (`TaskbarEdgeOnMonitor`), auto-hide from how much of the window is on its monitor
  (`IsTaskbarMostlyOnMonitor`), fullscreen per monitor as for the primary.
- Anchors on a secondary taskbar: Windows 11 has no window for its clock, so "next to the
  tray" reserves the primary's notification-area length (in logical px) at the far end
  (`MirrorTrayArea`), which keeps the meter off the secondary clock. Windows 10 class names
  `ClockButton` and `WorkerW` -> `MSTaskListWClass` are used when present; they come from
  public taskbar tools and are **not verified here**. "After the app buttons" falls back to the
  left edge where there is no task list window.
- Font cache: one font per DPI (bounded), so monitors at different DPIs do not recreate it on
  every render.
- Setting `[Overlay] AllTaskbars=0|1` (default 0). UI: checkbox 2036 "Show on all taskbars" at
  y=427 right, applied with Apply.
- **Not tested on real multiple monitors** (the dev machine and CI runners have one). The
  `AllTaskbars` check creates a stand-in `Shell_SecondaryTrayWnd` (`FakeTaskbar` in the
  harness, its own thread and message loop) along the top of the primary monitor and asserts:
  a second meter appears on it at the mirrored position, double-click works, the option off/on
  removes/restores it, embedded mode puts one child in each taskbar, and removing the stand-in
  removes its meter while the primary one stays. Negative control: without the mirroring the
  check fails ("right edge is 1276, expected 1018").
- Tests: unit `TestAllTaskbarsSetting`, `TestSecondaryTaskbarGeometry`; integration
  `AllTaskbars`.
- Gotcha: the Bash tool turns a double backslash into a single one, even inside a quoted
  heredoc, so Python patches containing C++ string escapes (`L"\r\n..."`) silently fail to
  match. Use the Edit tool for those.
- Verified: build clean; 60 unit tests; 23/23 integration checks; `AllTaskbars` 4/4 runs;
  settings window screenshot checked (new rows fit); isolation guard clean.

### M12: shell fullscreen signal (2026-10-09)
- Probe (`abprobe.cpp`, scratch): an appbar registered with `ABM_NEW` only (no `ABM_SETPOS`),
  as a hidden popup and as a message-only window, logged notifications while a window went
  through fullscreen states. Both variants identical: `ABN_FULLSCREENAPP` OPEN/CLOSE within
  ~20 ms of borderless-monitor-sized + foreground, back to windowed, minimized, restored and
  destroyed; nothing for a normal maximized window; work area unchanged; the taskbar's
  `WS_EX_TOPMOST` bit tracked the same state. Key finding: when a **smaller window takes the
  foreground** while the fullscreen window stays, the shell sends nothing and keeps its
  taskbar behind the fullscreen app, but the old heuristic (foreground window only) showed
  the overlay over the fullscreen content.
- Adopted (`meter.cpp`): the host window (message-only) registers as such an appbar,
  re-registers when `Shell_TrayWnd` changes (Explorer restart) and unregisters on shutdown.
  OPEN records the foreground app window and its monitor; CLOSE clears it. The overlay is now
  hidden if the foreground window is fullscreen (unchanged heuristic) **or** the recorded app
  is still visible, not minimized and on that monitor, unless the shell's own taskbar or
  desktop is in front. Anything inconsistent drops the record, so a missed CLOSE cannot hide
  the meter for long. Embedded meters are unaffected (they hide with their taskbar).
- Limits: one record at a time (two fullscreen apps on two monitors fall back to the heuristic
  after the first CLOSE); whether the shell sends this for fullscreen apps on secondary
  monitors to an appbar with no position is unknown (untestable here). Test instances are
  killed without `ABM_REMOVE`; Explorer drops appbars whose window is gone, and an empty
  registration reserves nothing.
- Test: `Fullscreen` gained step 6b (smaller topmost window takes the foreground over the
  fullscreen probe: the overlay must stay hidden), asserted when the taskbar has dropped
  `WS_EX_TOPMOST`, i.e. when the shell agrees it is fullscreen. Negative control without the
  shell state: fails 2/2 ("Overlay came back over a fullscreen app...").
- Verified: build clean; 60 unit tests; 23/23 integration checks; `Fullscreen` 4/4 runs;
  isolation guard clean.

### M13: README, 0.2.0, release, deploy (2026-10-09)
- README rewritten for 0.2.0: features, settings (which check boxes need Apply), the known
  limitation with the measured numbers and the embedded-mode trade-offs, multi-monitor
  caveat, minimum Windows 10 1607, and what CI and the release workflow actually run (the
  release job repeats unit and PE checks only; behavioral checks run in CI on `main`).
- `version.h` -> 0.2.0. Full local verification of that build: 60 unit tests, 23/23
  integration checks, isolation guard clean.
- Upgrade rehearsal on a copy of the dev machine's live 0.1.x settings file
  (`--integration-test --settings <copy>`, scratch `m13/rehearse.ps1`): the 0.2.0 meter sat at
  exactly the running 0.1.6 meter's rectangle ([2,676,134,716] on the 1280x720 display);
  Classic position, Automatic colours, Automatic (Ethernet) adapter, bytes, KB/s floor,
  1 decimal, tray off, totals preserved (471.28 GB / 144.88 GB since 31/08/2026); the saved copy
  gained `SettingsVersion=2`, `Anchor=legacy`, `Embed=0`, `AllTaskbars=0`, `SpeedUnits=bytes`,
  colours `auto`, `[Network] Adapter=auto`; the live file was untouched.
