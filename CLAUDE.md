# WinNetMeter: notes for Claude

Native C++20 Win32 network meter for Windows 10/11 x64. One statically linked exe, no
installer, no runtime framework, no third-party code.

**Start every session by reading `handoff.md`.** It holds the current state, the milestone
plan, open work, and findings that were verified by experiment.

## Keep handoff.md current

Update `handoff.md` at every milestone, in the same commit as the milestone's code:
what changed, what was verified (exact commands and results), decisions and the reasons for
them, anything left half-done, and what comes next. A fresh session must be able to continue
from `handoff.md` alone, without this conversation.

## Layout

| Path | Contents |
|---|---|
| `src/main.cpp` | Win32 UI (status + settings window), tray icon, adapter choice, lifecycle |
| `src/meter.{h,cpp}` | The taskbar meter on its own thread: overlay or embedded window, rendering, hooks |
| `src/render.{h,cpp}` | Tray icon rendering (unit-testable) |
| `src/network.{h,cpp}` | Interface enumeration (IP Helper), `NetSampler`, speed/byte formatting |
| `src/settings.{h,cpp}` | `AppSettings`, INI persistence (atomic write), Run-key startup registration |
| `src/overlay.h` | Header-only geometry (anchors) and alpha helpers, unit-testable |
| `src/app.rc`, `src/version.h` | Resources, manifest, version (single source of truth) |
| `src/tests/unit_tests.cpp` | Native unit tests: plain `assert`, one `PASS:` line per test |
| `src/tests/windows_integration_tests.ps1` | Behavioral checks against the built exe |
| `src/build.bat`, `src/vcenv.bat` | Build for x64 or arm64; `vcenv.bat` sets up MSVC for a target (shared with the tests) |
| `.github/workflows/` | `ci.yml` (build, unit tests, all integration checks; x64 and ARM64), `release.yml` (tag -> release) |

## Build and test

Run from any shell; the scripts locate Visual Studio through vswhere:

```
cd src && .\build.bat                          # -> src\out\WinNetMeter.exe, this machine's architecture
cd src && .\build.bat arm64                    # cross-compile (needs the MSVC ARM64 build tools)
cd src\tests && .\run_tests.bat                # builds and runs unit_tests.exe natively
pwsh src\tests\windows_integration_tests.ps1 -Check <Name> [-Arch x64|arm64]
```

- Always call scripts and exes with an explicit `.\`. The dev machine sets
  `NoDefaultCurrentDirectoryInExePath=1`, so bare names fail with "is not recognized".
- `'vswhere.exe' is not recognized` during a build comes from Microsoft's `vcvarsall.bat`
  under that setting. It is harmless.
- Builds use `/W4 /WX`: every warning is an error. Local builds use VS 2022 Build Tools (x64
  only: no ARM64 tools on the dev machine); CI's `windows-latest` uses VS 2026 and
  `windows-11-arm` builds ARM64 natively. Code must compile cleanly on all of them.
- `-Arch` only matters to the PE checks (they assert the machine type); behavioral checks run
  the build for the machine they are on.
- Batch files are checked out with CRLF (`.gitattributes`): cmd.exe can miss labels in
  LF-only batch files.
- The list of integration check names is the `ValidateSet` at the top of
  `windows_integration_tests.ps1`; CI runs every one of them on both architectures.

## Integration tests: rules

- They launch the exe with `--integration-test`, which uses its own window class, mutex,
  settings file and Run-key value name. They must never touch the real
  `%APPDATA%\WinNetMeter\settings.ini` or the real `HKCU\...\Run\WinNetMeter` value.
- A deployed copy of WinNetMeter is usually running on the dev machine with autostart.
  Never kill it, never edit its settings, never use its window class in tests.
- Some checks drive the real desktop (foreground changes, fullscreen probes, opening Start)
  and briefly take focus. That is expected.

## Conventions

- Match the existing style: 4-space indent, `static` free functions, `g_` globals, comments
  that explain *why*, not what.
- No new DLL imports without updating the `Imports` allowlist on purpose; static CRT (`/MT`).
- Settings must stay backward compatible: never rename or repurpose an INI key, give every new
  key a default, and keep loading files written by every earlier release (UTF-16 since 0.1.6,
  ANSI before). Unit tests cover legacy files; extend them when the format changes.
- Every behavior change gets a unit test (pure logic) and/or an integration check (Win32
  behavior). Do not weaken an existing assertion to make a change pass; if a contract changes
  on purpose, say so in the commit message and in `handoff.md`.
- Anything touching the taskbar runs on the meter thread, never on the UI thread (see
  `handoff.md`, "Platform facts").

## Release

1. Bump both macros in `src/version.h` (`WINNETMETER_VERSION` and `WINNETMETER_VERSION_STRING`).
2. Push `main` and wait for CI to pass.
3. `git tag -a vX.Y.Z` and push the tag. `release.yml` builds, tests and publishes the zip,
   the exe and `.sha256` files. It fails if the tag does not match `version.h`.
