param(
    [Parameter(Mandatory)]
    [ValidateSet('SingleInstance', 'DuplicateUi', 'WindowStyles', 'Position', 'Dpi',
                 'ForegroundZOrder', 'Fullscreen', 'ExplorerRecovery', 'Metadata', 'StaticRuntime', 'Imports',
                 'ResourceLeak', 'FormattingDisplay', 'Preferences', 'CustomizationTotals')]
    [string]$Check
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not ('WinNetMeterNative' -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class WinNetMeterNative
{
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [StructLayout(LayoutKind.Sequential)]
    public struct APPBARDATA
    {
        public uint cbSize;
        public IntPtr hWnd;
        public uint uCallbackMessage;
        public uint uEdge;
        public RECT rc;
        public IntPtr lParam;
    }

    public sealed class WindowInfo
    {
        public IntPtr Handle;
        public string ClassName = "";
        public bool Visible;
        public ulong Style;
        public ulong ExStyle;
        public IntPtr Owner;
        public RECT Rect;
    }

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassNameW(IntPtr hwnd, StringBuilder className, int maxCount);
    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")]
    private static extern IntPtr GetWindowLongPtrW(IntPtr hwnd, int index);
    [DllImport("user32.dll", EntryPoint = "GetClassLongPtrW")]
    public static extern IntPtr GetClassLongPtrW(IntPtr hwnd, int index);
    [DllImport("user32.dll")]
    public static extern IntPtr GetSysColorBrush(int index);
    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")]
    private static extern IntPtr GetWindow(IntPtr hwnd, uint command);
    [DllImport("user32.dll")]
    private static extern IntPtr GetTopWindow(IntPtr hwnd);
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr CreateWindowExW(uint exStyle, string className, string windowName,
        uint style, int x, int y, int width, int height, IntPtr parent, IntPtr menu,
        IntPtr instance, IntPtr parameter);
    [DllImport("user32.dll")]
    public static extern bool DestroyWindow(IntPtr hwnd);
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hwnd, int command);
    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr hwnd, IntPtr insertAfter, int x, int y,
        int width, int height, uint flags);
    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")]
    public static extern void NotifyWinEvent(uint eventId, IntPtr hwnd, int objectId, int childId);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern uint RegisterWindowMessageW(string name);
    [DllImport("user32.dll")]
    public static extern bool PostMessageW(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern IntPtr SendMessageW(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowTextW(IntPtr hwnd, StringBuilder text, int maxCount);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")]
    public static extern IntPtr SendMessageTextW(IntPtr hwnd, uint message, IntPtr wParam, StringBuilder text);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")]
    public static extern IntPtr SendMessageStringW(IntPtr hwnd, uint message, IntPtr wParam, string text);
    [DllImport("user32.dll")]
    public static extern uint GetDpiForWindow(IntPtr hwnd);
    [DllImport("user32.dll")]
    private static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")]
    public static extern uint GetGuiResources(IntPtr process, uint flags);
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    public static extern uint GetPrivateProfileIntW(string section, string key, int fallback, string path);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    public static extern uint GetPrivateProfileStringW(string section, string key, string fallback,
        StringBuilder value, uint size, string path);
    [DllImport("user32.dll", EntryPoint = "SetWindowLongPtrW")]
    public static extern IntPtr SetWindowLongPtrW(IntPtr hwnd, int index, IntPtr newLong);
    [DllImport("user32.dll")]
    public static extern IntPtr MonitorFromWindow(IntPtr hwnd, uint flags);
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
    public struct MONITORINFO
    {
        public uint cbSize;
        public RECT rcMonitor;
        public RECT rcWork;
        public uint dwFlags;
    }
    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern bool GetMonitorInfo(IntPtr hMonitor, ref MONITORINFO lpmi);
    [DllImport("shell32.dll")]
    private static extern UIntPtr SHAppBarMessage(uint message, ref APPBARDATA data);

    public static RECT GetMonitorRect(IntPtr hwnd)
    {
        IntPtr hMon = MonitorFromWindow(hwnd, 1);
        MONITORINFO mi = new MONITORINFO();
        mi.cbSize = (uint)Marshal.SizeOf(typeof(MONITORINFO));
        GetMonitorInfo(hMon, ref mi);
        return mi.rcMonitor;
    }

    public static WindowInfo[] GetWindows(uint wantedProcessId)
    {
        var windows = new List<WindowInfo>();
        EnumWindows((hwnd, unused) => {
            uint processId;
            GetWindowThreadProcessId(hwnd, out processId);
            if (processId != wantedProcessId) return true;
            var name = new StringBuilder(256);
            GetClassNameW(hwnd, name, name.Capacity);
            RECT rect;
            GetWindowRect(hwnd, out rect);
            windows.Add(new WindowInfo {
                Handle = hwnd,
                ClassName = name.ToString(),
                Visible = IsWindowVisible(hwnd),
                Style = unchecked((ulong)GetWindowLongPtrW(hwnd, -16).ToInt64()),
                ExStyle = unchecked((ulong)GetWindowLongPtrW(hwnd, -20).ToInt64()),
                Owner = GetWindow(hwnd, 4),
                Rect = rect
            });
            return true;
        }, IntPtr.Zero);
        return windows.ToArray();
    }

    public static APPBARDATA GetTaskbar()
    {
        var data = new APPBARDATA();
        data.cbSize = (uint)Marshal.SizeOf(typeof(APPBARDATA));
        if (SHAppBarMessage(5, ref data) == UIntPtr.Zero)
            throw new InvalidOperationException("ABM_GETTASKBARPOS failed");
        return data;
    }

    public static void EnablePerMonitorDpi()
    {
        SetThreadDpiAwarenessContext(new IntPtr(-4));
    }

    public static bool IsAbove(IntPtr first, IntPtr second)
    {
        for (var window = GetTopWindow(IntPtr.Zero); window != IntPtr.Zero;
             window = GetWindow(window, 2))
        {
            if (window == first) return true;
            if (window == second) return false;
        }
        return false;
    }
}
'@
}

[WinNetMeterNative]::EnablePerMonitorDpi()

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$exePath = (Resolve-Path (Join-Path $repoRoot 'src\out\WinNetMeter.exe')).Path
$settingsPath = Join-Path $env:APPDATA 'WinNetMeter\settings.ini'
$mainClass = 'WinNetMeterMainTest'
$versionHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\version.h')
if ($versionHeader -notmatch '(?m)^#define WINNETMETER_VERSION_STRING "([^"]+)"\r?$') { throw 'Version header is malformed' }
$sourceVersion = $Matches[1]

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Get-RunningAppProcesses {
    @(Get-Process -Name WinNetMeter -ErrorAction SilentlyContinue | Where-Object {
        try { $_.Path -eq $exePath } catch { $false }
    })
}

function Wait-AppWindows([System.Diagnostics.Process]$Process) {
    for ($attempt = 0; $attempt -lt 50; ++$attempt) {
        if ($Process.HasExited) { throw "WinNetMeter exited during startup with code $($Process.ExitCode)" }
        $windows = @([WinNetMeterNative]::GetWindows([uint32]$Process.Id))
        $hasMain = @($windows | Where-Object ClassName -eq $mainClass).Count -eq 1
        $hasOverlay = @($windows | Where-Object ClassName -eq 'WinNetMeterOverlay').Count -eq 1
        if ($hasMain -and $hasOverlay) {
            return $windows
        }
        Start-Sleep -Milliseconds 100
    }
    throw 'Timed out waiting for WinNetMeter host and overlay windows'
}

function Start-TestApp([string]$SettingsContent = "[Overlay]`r`nShowWidget=1`r`n") {
    Assert-True (@(Get-RunningAppProcesses).Count -eq 0) 'A WinNetMeter process from this build is already running'
    $settingsExisted = Test-Path -LiteralPath $settingsPath
    $settingsBytes = if ($settingsExisted) { [IO.File]::ReadAllBytes($settingsPath) } else { $null }
    $settingsDirectory = Split-Path -Parent $settingsPath
    $directoryExisted = Test-Path -LiteralPath $settingsDirectory
    [IO.Directory]::CreateDirectory($settingsDirectory) | Out-Null
    [IO.File]::WriteAllText($settingsPath, $SettingsContent)

    $process = $null
    try {
        $process = Start-Process -FilePath $exePath -ArgumentList '--integration-test' -PassThru
        $windows = Wait-AppWindows $process
        return [pscustomobject]@{
            Process = $process
            Windows = $windows
            SettingsExisted = $settingsExisted
            SettingsBytes = $settingsBytes
            SettingsDirectoryExisted = $directoryExisted
        }
    } catch {
        if ($null -ne $process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            Wait-Process -Id $process.Id -Timeout 5 -ErrorAction SilentlyContinue
        }
        if ($settingsExisted) { [IO.File]::WriteAllBytes($settingsPath, $settingsBytes) }
        elseif (Test-Path -LiteralPath $settingsPath) { Remove-Item -LiteralPath $settingsPath -Force }
        throw
    }
}

function Stop-TestApp($Session) {
    if (-not $Session.Process.HasExited) {
        Stop-Process -Id $Session.Process.Id -Force -ErrorAction SilentlyContinue
        Wait-Process -Id $Session.Process.Id -Timeout 5 -ErrorAction SilentlyContinue
    }
    if ($Session.SettingsExisted) {
        [IO.File]::WriteAllBytes($settingsPath, $Session.SettingsBytes)
    } elseif (Test-Path -LiteralPath $settingsPath) {
        Remove-Item -LiteralPath $settingsPath -Force
    }
    $settingsDirectory = Split-Path -Parent $settingsPath
    if (-not $Session.SettingsDirectoryExisted -and (Test-Path -LiteralPath $settingsDirectory) -and
        @(Get-ChildItem -LiteralPath $settingsDirectory -Force).Count -eq 0) {
        Remove-Item -LiteralPath $settingsDirectory -Force
    }
}

function Get-AppWindow($Session, [string]$ClassName) {
    $windows = @([WinNetMeterNative]::GetWindows([uint32]$Session.Process.Id))
    $match = @($windows | Where-Object ClassName -eq $ClassName)
    Assert-True ($match.Count -eq 1) "Expected one $ClassName window, found $($match.Count)"
    $match[0]
}

function Get-Dumpbin {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    Assert-True (Test-Path -LiteralPath $vswhere) 'vswhere.exe was not found'
    $install = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    Assert-True ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($install)) 'MSVC installation was not found'
    $toolsRoot = Join-Path $install 'VC\Tools\MSVC'
    $toolset = Get-ChildItem -LiteralPath $toolsRoot -Directory |
        Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
    $dumpbin = Join-Path $toolset.FullName 'bin\Hostx64\x64\dumpbin.exe'
    Assert-True (Test-Path -LiteralPath $dumpbin) 'dumpbin.exe was not found'
    $dumpbin
}

function Get-Imports {
    $dumpbin = Get-Dumpbin
    $output = & $dumpbin /dependents $exePath 2>&1
    Assert-True ($LASTEXITCODE -eq 0) 'dumpbin /dependents failed'
    @($output | ForEach-Object {
        if ($_ -match '^\s+([A-Za-z0-9_.-]+\.dll)\s*$') { $Matches[1].ToUpperInvariant() }
    } | Sort-Object -Unique)
}

if ($Check -eq 'Metadata') {
    $version = (Get-Item -LiteralPath $exePath).VersionInfo
    Assert-True ($sourceVersion -match '^\d+\.\d+\.\d+$') 'Source version is not semantic x.y.z'
    Assert-True ($version.ProductName -eq 'WinNetMeter') 'ProductName mismatch'
    Assert-True ($version.FileDescription -eq 'WinNetMeter') 'FileDescription mismatch'
    Assert-True ($version.InternalName -eq 'WinNetMeter') 'InternalName mismatch'
    Assert-True ($version.OriginalFilename -eq 'WinNetMeter.exe') 'OriginalFilename mismatch'
    Assert-True ($version.FileVersion -eq $sourceVersion) 'FileVersion mismatch'
    Assert-True ($version.ProductVersion -eq $sourceVersion) 'ProductVersion mismatch'
    'VERSION_METADATA_OK'
    exit 0
}

if ($Check -eq 'StaticRuntime') {
    $imports = Get-Imports
    $forbiddenPattern = '^(VCRUNTIME|MSVCP|UCRTBASE|MSCOREE)'
    $forbiddenControl = @('VCRUNTIME_TEST.DLL' | Where-Object { $_ -match $forbiddenPattern }).Count -eq 1
    Assert-True $forbiddenControl 'Static-runtime negative control did not detect a forbidden import'
    $forbidden = @($imports | Where-Object { $_ -match $forbiddenPattern })
    Assert-True ($forbidden.Count -eq 0) "Dynamic VC/CLR dependency found: $($forbidden -join ', ')"
    $dumpbin = Get-Dumpbin
    $headers = (& $dumpbin /headers $exePath 2>&1) -join "`n"
    Assert-True ($LASTEXITCODE -eq 0) 'dumpbin /headers failed'
    $emptyClrPattern = '(?im)^\s*0 \[\s*0\] RVA \[size\] of COM descriptor directory\s*$'
    $clrControl = "0 [       0] RVA [size] of COM descriptor directory" -match $emptyClrPattern
    Assert-True $clrControl 'CLR-header negative control is invalid'
    Assert-True ($headers -match $emptyClrPattern) 'PE contains a CLR COM descriptor'
    'STATIC_RUNTIME_OK'
    exit 0
}

if ($Check -eq 'Imports') {
    $allowlist = @('ADVAPI32.DLL', 'COMCTL32.DLL', 'COMDLG32.DLL', 'DWMAPI.DLL', 'GDI32.DLL', 'IPHLPAPI.DLL',
                   'KERNEL32.DLL', 'SHELL32.DLL', 'USER32.DLL')
    $importControl = @('UNEXPECTED_TEST.DLL' | Where-Object { $_ -notin $allowlist }).Count -eq 1
    Assert-True $importControl 'Import allowlist negative control did not detect an unexpected DLL'
    $unexpected = @(Get-Imports | Where-Object { $_ -notin $allowlist })
    Assert-True ($unexpected.Count -eq 0) "Unexpected imports: $($unexpected -join ', ')"
    'IMPORT_ALLOWLIST_OK'
    exit 0
}

$testSettings = if ($Check -eq 'FormattingDisplay') {
    "[Overlay]`r`nShowWidget=1`r`nMinimumSpeedUnit=MB/s`r`nDecimalPlaces=1`r`nDownloadColor=1971210`r`nUploadColor=6592200`r`n"
} elseif ($Check -eq 'CustomizationTotals') {
    "[Overlay]`r`nShowWidget=1`r`nDownloadPrefix=x0044003A`r`nUploadPrefix=x0055003A`r`nDownloadColor=1971210`r`nUploadColor=6592200`r`nFontFamily=Arial`r`nFontSize=11.0`r`nFontStyle=0`r`nTaskbarOffset=37`r`nMinimumSpeedUnit=GB/s`r`nDecimalPlaces=0`r`n[Totals]`r`nDownloaded=8388608`r`nUploaded=3145728`r`nSince=2024-02-29`r`n"
} else {
    "[Overlay]`r`nShowWidget=1`r`n"
}
$startupKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$startupValueExisted = $false
$startupValue = $null
if ($Check -eq 'Preferences') {
    try {
        $startupValue = Get-ItemPropertyValue -LiteralPath $startupKey -Name WinNetMeter -ErrorAction Stop
        $startupValueExisted = $true
    } catch [System.Management.Automation.ItemNotFoundException] {
    } catch [System.Management.Automation.PSArgumentException] {
    }
    Remove-ItemProperty -LiteralPath $startupKey -Name WinNetMeter -ErrorAction SilentlyContinue
}

$session = $null
try {
    $session = Start-TestApp $testSettings
    switch ($Check) {
        'SingleInstance' {
            $duplicates = @(1..8 | ForEach-Object { Start-Process -FilePath $exePath -ArgumentList '--integration-test' -PassThru })
            foreach ($duplicate in $duplicates) {
                Assert-True ($duplicate.WaitForExit(3000)) 'A duplicate instance did not exit within three seconds'
                Assert-True ($duplicate.ExitCode -eq 0) "A duplicate instance exited with code $($duplicate.ExitCode)"
            }
            Assert-True (@(Get-RunningAppProcesses).Count -eq 1) 'Duplicate launch left more than one live process'
            'SINGLE_INSTANCE_OK'
        }
        'DuplicateUi' {
            $before = @([WinNetMeterNative]::GetWindows([uint32]$session.Process.Id))
            $second = Start-Process -FilePath $exePath -ArgumentList '--integration-test' -PassThru
            Assert-True ($second.WaitForExit(3000)) 'Second instance did not exit within three seconds'
            $after = @([WinNetMeterNative]::GetWindows([uint32]$session.Process.Id))
            Assert-True ($second.ExitCode -eq 0) 'Duplicate launch failed instead of handing off'
            Assert-True ($before.Count -eq $after.Count) 'Duplicate launch changed the top-level window count'
            Assert-True (@($after | Where-Object ClassName -eq $mainClass).Count -eq 1) 'Duplicate main host detected'
            Assert-True (@($after | Where-Object ClassName -eq 'WinNetMeterOverlay').Count -eq 1) 'Duplicate overlay detected'
            'DUPLICATE_UI_GUARD_OK'
        }
        'WindowStyles' {
            $main = Get-AppWindow $session $mainClass
            $overlay = Get-AppWindow $session 'WinNetMeterOverlay'
            $toolWindow = [uint64]0x80
            $appWindow = [uint64]0x40000
            $layered = [uint64]0x80000
            $noActivate = [uint64]0x08000000
            $popup = [uint64]2147483648
            Assert-True (-not $main.Visible) 'Main host is visible in resting mode'
            Assert-True (($main.ExStyle -band $toolWindow) -eq 0) 'Main window still uses tool-window chrome'
            Assert-True $overlay.Visible 'Overlay is not visible'
            Assert-True (($overlay.Style -band $popup) -ne 0) 'Overlay is not WS_POPUP'
            Assert-True (($overlay.ExStyle -band ($toolWindow -bor $layered -bor $noActivate)) -eq
                         ($toolWindow -bor $layered -bor $noActivate)) 'Overlay extended styles are incomplete'
            Assert-True (($overlay.ExStyle -band $appWindow) -eq 0) 'Overlay forces an application taskbar button'
            Assert-True ($overlay.Owner -eq [IntPtr]::Zero) 'Overlay is unexpectedly owned or parented'
            $altTabCandidates = @([WinNetMeterNative]::GetWindows([uint32]$session.Process.Id) | Where-Object {
                $_.Visible -and $_.Owner -eq [IntPtr]::Zero -and ($_.ExStyle -band $toolWindow) -eq 0
            })
            Assert-True ($altTabCandidates.Count -eq 0) 'A resting top-level window remains eligible for Alt+Tab'
            [void][WinNetMeterNative]::SendMessageW($overlay.Handle, 0x0203, [IntPtr]::Zero, [IntPtr]::Zero)
            $shownMain = Get-AppWindow $session $mainClass
            Assert-True $shownMain.Visible 'Taskbar-meter activation did not show the unified window'
            Assert-True (($shownMain.ExStyle -band $toolWindow) -eq 0) 'Unified window is not a regular application window'
            [void][WinNetMeterNative]::SendMessageW($main.Handle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
            $hiddenMain = Get-AppWindow $session $mainClass
            Assert-True (-not $hiddenMain.Visible) 'Closing the status window did not return to resting tray mode'
            'RESTING_WINDOW_STYLES_OK'
        }
        'Position' {
            $overlay = Get-AppWindow $session 'WinNetMeterOverlay'
            $taskbar = [WinNetMeterNative]::GetTaskbar().rc
            $contained = $overlay.Rect.Left -ge $taskbar.Left -and $overlay.Rect.Top -ge $taskbar.Top -and
                         $overlay.Rect.Right -le $taskbar.Right -and $overlay.Rect.Bottom -le $taskbar.Bottom
            Assert-True $contained ("Overlay [{0},{1},{2},{3}] is not contained by taskbar [{4},{5},{6},{7}]" -f
                $overlay.Rect.Left, $overlay.Rect.Top, $overlay.Rect.Right, $overlay.Rect.Bottom,
                $taskbar.Left, $taskbar.Top, $taskbar.Right, $taskbar.Bottom)
            'TASKBAR_POSITION_OK'
        }
        'ForegroundZOrder' {
            $overlay = Get-AppWindow $session 'WinNetMeterOverlay'
            $probe = [WinNetMeterNative]::CreateWindowExW(
                0, 'STATIC', 'WinNetMeter foreground probe', 0x10CF0000,
                20, 20, 400, 200, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-True ($probe -ne [IntPtr]::Zero) 'Failed to create foreground probe window'
            try {
                [void][WinNetMeterNative]::ShowWindow($probe, 5)
                Start-Sleep -Milliseconds 250
                $foreground = [WinNetMeterNative]::GetForegroundWindow()

                for ($transition = 0; $transition -lt 4; ++$transition) {
                    Assert-True ([WinNetMeterNative]::SetWindowPos(
                        $probe, [IntPtr](-2), 20, 20, 400, 200, 0x0010)) 'Failed to demote foreground probe'
                    $shown = [WinNetMeterNative]::SetWindowPos(
                        $probe, [IntPtr](-1), 20, 20, 400, 200, 0x0010)
                    Assert-True $shown 'Failed to show topmost foreground probe'
                    Assert-True ([WinNetMeterNative]::IsAbove($probe, $overlay.Handle)) 'Probe did not move above overlay'
                    [WinNetMeterNative]::NotifyWinEvent(3, $probe, 0, 0)

                    $restored = $false
                    for ($attempt = 0; $attempt -lt 20; ++$attempt) {
                        if ([WinNetMeterNative]::IsAbove($overlay.Handle, $probe)) {
                            $restored = $true
                            break
                        }
                        Start-Sleep -Milliseconds 10
                    }
                    Assert-True $restored 'Foreground event did not promptly restore overlay z-order'
                    Assert-True ([WinNetMeterNative]::GetForegroundWindow() -eq $foreground) 'Overlay z-order repair stole foreground focus'
                }

                $after = @([WinNetMeterNative]::GetWindows([uint32]$session.Process.Id))
                $afterOverlay = @($after | Where-Object ClassName -eq 'WinNetMeterOverlay')
                Assert-True ($afterOverlay.Count -eq 1) 'Foreground repair duplicated the overlay'
                Assert-True $afterOverlay[0].Visible 'Foreground repair hid the overlay'
                Assert-True (($afterOverlay[0].ExStyle -band [uint64]0x8) -ne 0) 'Overlay lost WS_EX_TOPMOST'
                Assert-True (($afterOverlay[0].ExStyle -band [uint64]0x08000000) -ne 0) 'Overlay lost WS_EX_NOACTIVATE'
                'FOREGROUND_Z_ORDER_OK'
            } finally {
                [void][WinNetMeterNative]::DestroyWindow($probe)
            }
        }
        'Fullscreen' {
            $overlay = Get-AppWindow $session 'WinNetMeterOverlay'
            Assert-True $overlay.Visible 'Overlay is not initially visible'

            # 1. Create a normal top-level probe window
            # WS_OVERLAPPEDWINDOW | WS_VISIBLE = 0x10CF0000
            $probe = [WinNetMeterNative]::CreateWindowExW(
                0, 'STATIC', 'WinNetMeter fullscreen probe', 0x10CF0000,
                50, 50, 600, 400, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-True ($probe -ne [IntPtr]::Zero) 'Failed to create fullscreen probe window'
            try {
                # 2. Make probe foreground
                [void][WinNetMeterNative]::ShowWindow($probe, 5)
                [void][WinNetMeterNative]::SetForegroundWindow($probe)
                [WinNetMeterNative]::NotifyWinEvent(3, $probe, 0, 0)
                Start-Sleep -Milliseconds 200

                # 3. Normal windowed probe -> overlay MUST remain visible
                $overlayWnd = Get-AppWindow $session 'WinNetMeterOverlay'
                Assert-True $overlayWnd.Visible 'Overlay became hidden for normal windowed probe'

                # 4. Maximize probe normally (SW_MAXIMIZE = 3)
                [void][WinNetMeterNative]::ShowWindow($probe, 3)
                [WinNetMeterNative]::NotifyWinEvent(0x800B, $probe, 0, 0)
                Start-Sleep -Milliseconds 200
                $overlayWnd = Get-AppWindow $session 'WinNetMeterOverlay'
                Assert-True $overlayWnd.Visible 'Overlay became hidden for normally maximized probe'

                # 5. Make SAME HWND fullscreen (borderless + monitor sized)
                $mon = [WinNetMeterNative]::GetMonitorRect($probe)
                $width = $mon.Right - $mon.Left
                $height = $mon.Bottom - $mon.Top
                # Style = WS_POPUP | WS_VISIBLE = 0x90000000
                [void][WinNetMeterNative]::SetWindowLongPtrW($probe, -16, [IntPtr]0x90000000)
                [void][WinNetMeterNative]::SetWindowPos($probe, [IntPtr](-1), $mon.Left, $mon.Top, $width, $height, 0x0040)
                [WinNetMeterNative]::NotifyWinEvent(0x800B, $probe, 0, 0)

                # 6. Verify overlay hides promptly
                $hidden = $false
                for ($attempt = 0; $attempt -lt 30; ++$attempt) {
                    $overlayWnd = Get-AppWindow $session 'WinNetMeterOverlay'
                    if (-not $overlayWnd.Visible) {
                        $hidden = $true
                        break
                    }
                    Start-Sleep -Milliseconds 50
                }
                Assert-True $hidden 'Overlay did not hide for fullscreen window'

                # 7. Model the Win+D transient: same foreground/fullscreen HWND, now minimized.
                # WS_POPUP | WS_VISIBLE | WS_MINIMIZE = 0xB0000000
                [void][WinNetMeterNative]::SetWindowLongPtrW($probe, -16, [IntPtr]0xB0000000)
                [WinNetMeterNative]::NotifyWinEvent(0x800B, $probe, 0, 0)
                Start-Sleep -Milliseconds 200
                $overlayWnd = Get-AppWindow $session 'WinNetMeterOverlay'
                Assert-True $overlayWnd.Visible 'Overlay stayed hidden for minimized fullscreen foreground'

                # Restore the minimized window as Windows does after Win+D, then return it to fullscreen.
                [void][WinNetMeterNative]::SetWindowLongPtrW($probe, -16, [IntPtr]0x90000000)
                [void][WinNetMeterNative]::ShowWindow($probe, 9)
                [void][WinNetMeterNative]::SetWindowPos($probe, [IntPtr](-1), $mon.Left, $mon.Top, $width, $height, 0x0040)
                [WinNetMeterNative]::NotifyWinEvent(0x800B, $probe, 0, 0)
                $hidden = $false
                for ($attempt = 0; $attempt -lt 30; ++$attempt) {
                    $overlayWnd = Get-AppWindow $session 'WinNetMeterOverlay'
                    if (-not $overlayWnd.Visible) {
                        $hidden = $true
                        break
                    }
                    Start-Sleep -Milliseconds 50
                }
                Assert-True $hidden 'Overlay did not hide after restoring the fullscreen window'

                # 8. Restore SAME HWND to normal windowed bounds
                [void][WinNetMeterNative]::SetWindowLongPtrW($probe, -16, [IntPtr]0x10CF0000)
                [void][WinNetMeterNative]::SetWindowPos($probe, [IntPtr](-2), 50, 50, 600, 400, 0x0040)
                [WinNetMeterNative]::NotifyWinEvent(0x800B, $probe, 0, 0)

                # 9. Verify overlay restores promptly
                $restored = $false
                for ($attempt = 0; $attempt -lt 30; ++$attempt) {
                    $overlayWnd = Get-AppWindow $session 'WinNetMeterOverlay'
                    if ($overlayWnd.Visible) {
                        $restored = $true
                        break
                    }
                    Start-Sleep -Milliseconds 50
                }
                Assert-True $restored 'Overlay did not restore after exiting fullscreen'

                # 10. Verify focus was not stolen by WinNetMeter
                $fg = [WinNetMeterNative]::GetForegroundWindow()
                Assert-True ($fg -eq $probe) 'WinNetMeter stole foreground focus'

                # 11. Verify exactly one overlay exists
                $windows = @([WinNetMeterNative]::GetWindows([uint32]$session.Process.Id))
                $overlays = @($windows | Where-Object ClassName -eq 'WinNetMeterOverlay')
                Assert-True ($overlays.Count -eq 1) 'Duplicate overlay detected'

                'FULLSCREEN_VISIBILITY_OK'
            } finally {
                [void][WinNetMeterNative]::DestroyWindow($probe)
            }
        }
        'Dpi' {
            $overlay = Get-AppWindow $session 'WinNetMeterOverlay'
            $taskbar = [WinNetMeterNative]::GetTaskbar().rc
            $dpi = [WinNetMeterNative]::GetDpiForWindow($overlay.Handle)
            Assert-True ($dpi -ge 96 -and $dpi -le 768) "Invalid live overlay DPI: $dpi"
            $scale = { param([int]$value) [int][Math]::Floor(($value * $dpi + 48) / 96) }
            $padding = [Math]::Max(1, (& $scale 2))
            $expectedWidth = [Math]::Max(1, [Math]::Min((& $scale 132),
                                                       $taskbar.Right - $taskbar.Left - 2 * $padding))
            $expectedHeight = [Math]::Max(1, [Math]::Min((& $scale 40),
                                                        $taskbar.Bottom - $taskbar.Top - 2 * $padding))
            Assert-True (($overlay.Rect.Right - $overlay.Rect.Left) -eq $expectedWidth) 'Live overlay width is not DPI-scaled'
            Assert-True (($overlay.Rect.Bottom - $overlay.Rect.Top) -eq $expectedHeight) 'Live overlay height is not DPI-scaled'
            'DPI_BEHAVIOR_OK'
        }
        'ExplorerRecovery' {
            $main = Get-AppWindow $session $mainClass
            $message = [WinNetMeterNative]::RegisterWindowMessageW('TaskbarCreated')
            Assert-True ($message -ne 0) 'TaskbarCreated registration failed'
            1..3 | ForEach-Object {
                $posted = [WinNetMeterNative]::PostMessageW($main.Handle, $message, [IntPtr]::Zero, [IntPtr]::Zero)
                Assert-True $posted 'Failed to post TaskbarCreated'
            }
            Start-Sleep -Milliseconds 500
            $windows = @([WinNetMeterNative]::GetWindows([uint32]$session.Process.Id))
            Assert-True (@($windows | Where-Object ClassName -eq $mainClass).Count -eq 1) 'Host duplicated after TaskbarCreated'
            Assert-True (@($windows | Where-Object ClassName -eq 'WinNetMeterOverlay').Count -eq 1) 'Overlay duplicated after TaskbarCreated'
            Assert-True (@($windows | Where-Object { $_.ClassName -eq 'WinNetMeterOverlay' -and $_.Visible }).Count -eq 1) 'Overlay did not recover visibly'
            'EXPLORER_RECOVERY_OK'
        }
        'ResourceLeak' {
            $main = Get-AppWindow $session $mainClass
            1..10 | ForEach-Object { [void][WinNetMeterNative]::SendMessageW($main.Handle, 0x0113, [IntPtr]1, [IntPtr]::Zero) }
            $session.Process.Refresh()
            $gdiBefore = [WinNetMeterNative]::GetGuiResources($session.Process.Handle, 0)
            $userBefore = [WinNetMeterNative]::GetGuiResources($session.Process.Handle, 1)
            1..200 | ForEach-Object { [void][WinNetMeterNative]::SendMessageW($main.Handle, 0x0113, [IntPtr]1, [IntPtr]::Zero) }
            $session.Process.Refresh()
            $gdiAfter = [WinNetMeterNative]::GetGuiResources($session.Process.Handle, 0)
            $userAfter = [WinNetMeterNative]::GetGuiResources($session.Process.Handle, 1)
            Assert-True ($gdiAfter -eq $gdiBefore) "GDI objects changed: $gdiBefore -> $gdiAfter"
            Assert-True ($userAfter -eq $userBefore) "User objects changed: $userBefore -> $userAfter"
            "RESOURCE_COUNTS GDI=$gdiBefore->$gdiAfter USER=$userBefore->$userAfter"
            'RESOURCE_LIFETIME_OK'
        }
        'FormattingDisplay' {
            $main = Get-AppWindow $session $mainClass
            $down = [WinNetMeterNative]::GetDlgItem($main.Handle, 102)
            $up = [WinNetMeterNative]::GetDlgItem($main.Handle, 103)
            Assert-True ($down -ne [IntPtr]::Zero -and $up -ne [IntPtr]::Zero) 'Speed value controls were not found'

            foreach ($control in @($down, $up)) {
                $text = New-Object Text.StringBuilder 64
                [void][WinNetMeterNative]::GetWindowTextW($control, $text, $text.Capacity)
                Assert-True ($text.ToString() -match '^\d+\.\d (MB|GB)/s$') "Unexpected configured speed text: $text"
            }

            'FORMATTING_DISPLAY_OK'
        }
        'CustomizationTotals' {
            $main = Get-AppWindow $session $mainClass
            $overlay = Get-AppWindow $session 'WinNetMeterOverlay'
            $iniValue = New-Object Text.StringBuilder 256
            [void][WinNetMeterNative]::GetPrivateProfileStringW('Overlay', 'DownloadPrefix', 'missing', $iniValue, $iniValue.Capacity, $settingsPath)
            Assert-True ($iniValue.ToString() -eq 'x0044003A') "Test prefix input was not readable: '$iniValue'"
            [void][WinNetMeterNative]::SendMessageW($overlay.Handle, 0x0203, [IntPtr]::Zero, [IntPtr]::Zero)

            $downPrefix = [WinNetMeterNative]::GetDlgItem($main.Handle, 2026)
            $upPrefix = [WinNetMeterNative]::GetDlgItem($main.Handle, 2028)
            $offsetEdit = [WinNetMeterNative]::GetDlgItem($main.Handle, 2013)
            Assert-True ($downPrefix -ne [IntPtr]::Zero -and $upPrefix -ne [IntPtr]::Zero -and
                         $offsetEdit -ne [IntPtr]::Zero) 'Live meter controls were not found'
            Assert-True ([WinNetMeterNative]::GetDlgItem($main.Handle, 2007) -eq [IntPtr]::Zero) 'Obsolete preview control still exists'

            $upPrefixRect = New-Object WinNetMeterNative+RECT
            $downPrefixRect = New-Object WinNetMeterNative+RECT
            $upColorRect = New-Object WinNetMeterNative+RECT
            $downColorRect = New-Object WinNetMeterNative+RECT
            [void][WinNetMeterNative]::GetWindowRect([WinNetMeterNative]::GetDlgItem($main.Handle, 2027), [ref]$upPrefixRect)
            [void][WinNetMeterNative]::GetWindowRect([WinNetMeterNative]::GetDlgItem($main.Handle, 2025), [ref]$downPrefixRect)
            [void][WinNetMeterNative]::GetWindowRect([WinNetMeterNative]::GetDlgItem($main.Handle, 2010), [ref]$upColorRect)
            [void][WinNetMeterNative]::GetWindowRect([WinNetMeterNative]::GetDlgItem($main.Handle, 2009), [ref]$downColorRect)
            Assert-True ($upPrefixRect.Top -lt $downPrefixRect.Top -and $upColorRect.Top -lt $downColorRect.Top) 'Settings are not ordered upload before download'

            $text = New-Object Text.StringBuilder 64
            [void][WinNetMeterNative]::SendMessageTextW($downPrefix, 0x000D, [IntPtr]$text.Capacity, $text)
            Assert-True ($text.ToString() -eq 'D:') "Download prefix did not load: '$text'"
            [void]$text.Clear()
            [void][WinNetMeterNative]::SendMessageTextW($upPrefix, 0x000D, [IntPtr]$text.Capacity, $text)
            Assert-True ($text.ToString() -eq 'U:') "Upload prefix did not load: '$text'"

            foreach ($item in @(@(109, 'Total data since 29/02/2024'), @(111, '8.00 MB'), @(113, '3.00 MB'))) {
                [void]$text.Clear()
                [void][WinNetMeterNative]::GetWindowTextW(
                    [WinNetMeterNative]::GetDlgItem($main.Handle, $item[0]), $text, $text.Capacity)
                Assert-True ($text.ToString() -eq $item[1]) "Unexpected total display for $($item[0]): $text"
            }

            $overlayBefore = New-Object WinNetMeterNative+RECT
            [void][WinNetMeterNative]::GetWindowRect($overlay.Handle, [ref]$overlayBefore)
            Assert-True ([WinNetMeterNative]::SendMessageStringW($downPrefix, 0x000C, [IntPtr]::Zero, 'DL:') -ne [IntPtr]::Zero) 'Could not edit download prefix'
            Assert-True ([WinNetMeterNative]::SendMessageStringW($upPrefix, 0x000C, [IntPtr]::Zero, 'UL:') -ne [IntPtr]::Zero) 'Could not edit upload prefix'
            [void]$iniValue.Clear()
            [void][WinNetMeterNative]::GetPrivateProfileStringW('Overlay', 'DownloadPrefix', '', $iniValue, $iniValue.Capacity, $settingsPath)
            Assert-True ($iniValue.ToString() -eq 'x0044004C003A') 'Download prefix did not update live'
            [void]$iniValue.Clear()
            [void][WinNetMeterNative]::GetPrivateProfileStringW('Overlay', 'UploadPrefix', 'missing', $iniValue, $iniValue.Capacity, $settingsPath)
            Assert-True ($iniValue.ToString() -eq 'x0055004C003A') 'Upload prefix did not update live'
            Assert-True ([WinNetMeterNative]::SendMessageStringW($offsetEdit, 0x000C, [IntPtr]::Zero, '80') -ne [IntPtr]::Zero) 'Could not edit meter offset'
            Assert-True ([WinNetMeterNative]::GetPrivateProfileIntW('Overlay', 'TaskbarOffset', -1, $settingsPath) -eq 80) 'Taskbar offset did not save live'
            $overlayAfter = New-Object WinNetMeterNative+RECT
            [void][WinNetMeterNative]::GetWindowRect($overlay.Handle, [ref]$overlayAfter)
            Assert-True ($overlayBefore.Left -ne $overlayAfter.Left -or $overlayBefore.Top -ne $overlayAfter.Top) 'Taskbar meter did not move live'

            [void][WinNetMeterNative]::SendMessageW($main.Handle, 0x0111, [IntPtr]2016, [IntPtr]::Zero)
            Assert-True ([WinNetMeterNative]::GetPrivateProfileIntW('Overlay', 'TaskbarOffset', -1, $settingsPath) -eq 0) 'Meter reset did not reset offset'
            Assert-True ([WinNetMeterNative]::GetPrivateProfileIntW('Overlay', 'DownloadColor', 0, $settingsPath) -eq 16777215) 'Meter reset did not reset download color'
            Assert-True ([WinNetMeterNative]::GetPrivateProfileIntW('Overlay', 'UploadColor', 0, $settingsPath) -eq 16777215) 'Meter reset did not reset upload color'
            Assert-True ([WinNetMeterNative]::GetPrivateProfileIntW('Overlay', 'FontStyle', -1, $settingsPath) -eq 1) 'Meter reset did not reset font style'
            Assert-True ([WinNetMeterNative]::GetPrivateProfileIntW('Overlay', 'DecimalPlaces', -1, $settingsPath) -eq 2) 'Meter reset did not reset decimal places'
            foreach ($item in @(@('DownloadPrefix', 'x2193'), @('UploadPrefix', 'x2191'), @('FontFamily', 'Segoe UI'), @('FontSize', '8.0'), @('MinimumSpeedUnit', 'Auto'))) {
                [void]$iniValue.Clear()
                [void][WinNetMeterNative]::GetPrivateProfileStringW('Overlay', $item[0], 'missing', $iniValue, $iniValue.Capacity, $settingsPath)
                Assert-True ($iniValue.ToString() -eq $item[1]) "Meter reset did not reset $($item[0]): $iniValue"
            }
            Assert-True ([WinNetMeterNative]::GetPrivateProfileIntW('Totals', 'Downloaded', 0, $settingsPath) -eq 8388608) 'Meter reset changed persistent totals'

            Assert-True ([WinNetMeterNative]::PostMessageW($main.Handle, 0x0111, [IntPtr]114, [IntPtr]::Zero)) 'Could not request total reset'
            $dialog = $null
            for ($attempt = 0; $attempt -lt 50 -and $null -eq $dialog; ++$attempt) {
                Start-Sleep -Milliseconds 100
                $dialog = @([WinNetMeterNative]::GetWindows([uint32]$session.Process.Id) |
                    Where-Object { $_.ClassName -eq '#32770' -and $_.Owner -eq $main.Handle }) | Select-Object -First 1
            }
            Assert-True ($null -ne $dialog) 'Reset confirmation did not appear'
            [void][WinNetMeterNative]::SendMessageW($dialog.Handle, 0x0111, [IntPtr]6, [IntPtr]::Zero)

            Start-Sleep -Milliseconds 200
            Assert-True ([WinNetMeterNative]::GetPrivateProfileIntW('Totals', 'Downloaded', 1, $settingsPath) -eq 0) 'Downloaded total was not reset'
            Assert-True ([WinNetMeterNative]::GetPrivateProfileIntW('Totals', 'Uploaded', 1, $settingsPath) -eq 0) 'Uploaded total was not reset'
            [void]$iniValue.Clear()
            [void][WinNetMeterNative]::GetPrivateProfileStringW('Totals', 'Since', '', $iniValue, $iniValue.Capacity, $settingsPath)
            Assert-True ($iniValue.ToString() -eq (Get-Date -Format 'yyyy-MM-dd')) 'Reset did not stamp today'
            'CUSTOMIZATION_TOTALS_OK'
        }
        'Preferences' {
            $main = Get-AppWindow $session $mainClass
            $overlay = Get-AppWindow $session 'WinNetMeterOverlay'
            Assert-True (-not $main.Visible) 'Unified window should start hidden'
            Assert-True ([WinNetMeterNative]::GetClassLongPtrW($main.Handle, -10) -eq
                         [WinNetMeterNative]::GetSysColorBrush(5)) 'Main window does not use COLOR_WINDOW'
            $settingsWindows = @([WinNetMeterNative]::GetWindows([uint32]$session.Process.Id) |
                Where-Object ClassName -eq 'WinNetMeterSettings')
            Assert-True ($settingsWindows.Count -eq 0) 'Legacy settings window still exists'

            foreach ($id in @(101, 102, 103, 2005, 2021, 2022, 2023, 2024)) {
                Assert-True ([WinNetMeterNative]::GetDlgItem($main.Handle, $id) -ne [IntPtr]::Zero) "Missing unified control $id"
            }

            [void][WinNetMeterNative]::SendMessageW($overlay.Handle, 0x0203, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-True (Get-AppWindow $session $mainClass).Visible 'Meter double-click did not open the unified window'

            $about = New-Object Text.StringBuilder 128
            [void][WinNetMeterNative]::GetWindowTextW(
                [WinNetMeterNative]::GetDlgItem($main.Handle, 106), $about, $about.Capacity)
            Assert-True ($about.ToString() -match ('WinNetMeter v' + [regex]::Escape($sourceVersion))) 'Visible version does not match version.h'
            Assert-True ($about.ToString() -match 'github\.com/pyed/WinNetMeter') 'Repository link is missing'

            $widget = [WinNetMeterNative]::GetDlgItem($main.Handle, 2021)
            $tray = [WinNetMeterNative]::GetDlgItem($main.Handle, 2022)
            $startup = [WinNetMeterNative]::GetDlgItem($main.Handle, 2023)
            [void][WinNetMeterNative]::SendMessageW($widget, 0x00F1, [IntPtr]1, [IntPtr]::Zero)
            [void][WinNetMeterNative]::SendMessageW($tray, 0x00F1, [IntPtr]::Zero, [IntPtr]::Zero)
            [void][WinNetMeterNative]::SendMessageW($startup, 0x00F1, [IntPtr]1, [IntPtr]::Zero)
            [void][WinNetMeterNative]::SendMessageW($main.Handle, 0x0111, [IntPtr]2005, [IntPtr]::Zero)

            Assert-True ([WinNetMeterNative]::GetPrivateProfileIntW('General', 'ShowTrayIcon', 1, $settingsPath) -eq 0) 'Tray preference was not saved'
            $registered = Get-ItemPropertyValue -LiteralPath $startupKey -Name WinNetMeter -ErrorAction Stop
            Assert-True ($registered -eq ('"' + $exePath + '"')) 'Startup command does not quote the current executable'

            [void][WinNetMeterNative]::SendMessageW($main.Handle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-True (-not (Get-AppWindow $session $mainClass).Visible) 'Closing did not hide the unified window'
            [void][WinNetMeterNative]::SendMessageW($overlay.Handle, 0x0203, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-True (Get-AppWindow $session $mainClass).Visible 'Meter could not reopen the app after hiding the tray icon'

            [void][WinNetMeterNative]::SendMessageW($startup, 0x00F1, [IntPtr]::Zero, [IntPtr]::Zero)
            [void][WinNetMeterNative]::SendMessageW($main.Handle, 0x0111, [IntPtr]2005, [IntPtr]::Zero)
            $startupProperties = Get-ItemProperty -LiteralPath $startupKey
            Assert-True ($null -eq $startupProperties.PSObject.Properties['WinNetMeter']) 'Startup entry was not removed'
            'PREFERENCES_INTEGRATION_OK'
        }
    }
} finally {
    if ($null -ne $session) { Stop-TestApp $session }
    if ($Check -eq 'Preferences') {
        if ($startupValueExisted) {
            New-Item -Path $startupKey -Force | Out-Null
            Set-ItemProperty -LiteralPath $startupKey -Name WinNetMeter -Value $startupValue
        } else {
            Remove-ItemProperty -LiteralPath $startupKey -Name WinNetMeter -ErrorAction SilentlyContinue
        }
    }
}
