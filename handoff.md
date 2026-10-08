# Handoff

Living status document. Update it at every milestone (see `CLAUDE.md`).

## Status

- Released: **v0.1.6** (tag `v0.1.6`). `main` is at the 0.1.6 code plus CI changes.
- In progress: the **0.2.0** work below, driven by the second audit (2026-10-08).
- Last completed milestone: **M0** (this document and `CLAUDE.md`).

## 0.2.0 milestone plan

Order is chosen so that test isolation comes first (the suite is run constantly next to a
live deployed instance), low-risk fixes come before the meter-thread refactor, and the
refactor lands with no behavior change before features are built on it.

- [x] **M0** `CLAUDE.md` + this file.
- [ ] **M1** Test isolation and tooling: `--settings <path>` and a separate Run-key value name
      in `--integration-test` mode; harness uses a temp settings file; `run_tests.bat` uses
      `.\unit_tests.exe`; CI actions `checkout`/`upload-artifact` v4 -> v7.
- [ ] **M2** Settings save hardening: POSIX-semantics rename, retry, in-place fallback; fix the
      stacking save-failure dialog.
- [ ] **M3** Application manifest (Common Controls v6, PerMonitorV2, supportedOS, asInvoker);
      drop the runtime DPI call; `Metadata` check asserts the manifest.
- [ ] **M4** Adapter selection: "Automatic" mode that follows the default route (new default),
      remembered manual choice, re-evaluated while running.
- [ ] **M5** Formatting: bits-per-second option; locale-aware "since" date.
- [ ] **M6** Theme-aware default meter colours, with a settings-version migration.
- [ ] **M7** Tray icon rendered at the shell's icon size for the taskbar DPI, with alpha.
- [ ] **M8** Meter thread: move everything taskbar-related (meter windows, rendering, WinEvent
      hooks) to a dedicated thread fed by state snapshots. No behavior change.
- [ ] **M9** Placement anchors (next to tray / after apps / left edge / legacy) with offsets
      relative to the anchor; legacy files keep their exact old position.
- [ ] **M10** Embedded mode (opt-in): the meter is a layered child window of the taskbar, so it
      stays visible while Start is open. Automatic fallback to the overlay.
- [ ] **M11** Meters on secondary-monitor taskbars (opt-in).
- [ ] **M12** Shell fullscreen signal (`ABN_FULLSCREENAPP`) as an extra trigger, if it proves
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
- A deployed WinNetMeter runs from a user folder with autostart (Run key). Its settings use
  `TaskbarOffset=-796` (meter at the far left), tray icon off, `MinimumSpeedUnit=KB/s`.
  New releases must keep that placement after an upgrade.
- `NoDefaultCurrentDirectoryInExePath=1` is set (see `CLAUDE.md`).

## Milestone log

### M0: documentation (2026-10-08)
Added `CLAUDE.md` (repo guidance, including the rule to update this file per milestone) and
this handoff. No code changes. Baseline before starting: build clean, all unit tests pass,
15/15 integration checks pass locally.
