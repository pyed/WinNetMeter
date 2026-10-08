param(
    [Parameter(Mandatory)]
    [ValidateSet('SingleInstance', 'DuplicateUi', 'WindowStyles', 'Position', 'Dpi',
                 'ForegroundZOrder', 'Fullscreen', 'ExplorerRecovery', 'Metadata', 'StaticRuntime', 'Imports',
                 'ResourceLeak', 'FormattingDisplay', 'Preferences', 'CustomizationTotals', 'SaveFailureDialog',
                 'AdapterSelection', 'SpeedUnits', 'ThemeColors', 'Anchors', 'Embedded', 'StartMenu')]
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
    [DllImport("iphlpapi.dll")]
    public static extern uint GetBestInterface(uint destination, out uint index);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr FindWindowExW(IntPtr parent, IntPtr after, string className, string title);

    // Screen rect of a taskbar part, found by class under Shell_TrayWnd (a path
    // like "ReBarWindow32/MSTaskSwWClass"); null when it does not exist.
    public static RECT? TaskbarPart(string path)
    {
        IntPtr window = FindWindowExW(IntPtr.Zero, IntPtr.Zero, "Shell_TrayWnd", null);
        foreach (var cls in path.Split('/')) {
            if (window == IntPtr.Zero) return null;
            window = FindWindowExW(window, IntPtr.Zero, cls, null);
        }
        RECT rect;
        if (window == IntPtr.Zero || !IsWindowVisible(window) || !GetWindowRect(window, out rect)) return null;
        return rect;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct PROCESS_BASIC_INFORMATION
    {
        public IntPtr ExitStatus, PebBaseAddress, AffinityMask, BasePriority, UniqueProcessId, InheritedFrom;
    }
    [DllImport("ntdll.dll")]
    private static extern int NtQueryInformationProcess(IntPtr process, int infoClass,
        ref PROCESS_BASIC_INFORMATION info, int size, out int returned);
    [DllImport("kernel32.dll")]
    private static extern IntPtr GetCurrentProcess();
    [DllImport("gdi32.dll")]
    private static extern IntPtr CreateCompatibleDC(IntPtr dc);
    [DllImport("gdi32.dll")]
    private static extern bool DeleteDC(IntPtr dc);

    // GDI handles of a process by object type (4 = region, 5 = bitmap, 10 = font, ...),
    // read from the session-wide GDI handle table mapped into every GUI process
    // (PEB->GdiSharedHandleTable at 0xF8 on x64; 24-byte cells: owner pid at 8,
    // type at 14). Returns null if the table is unavailable.
    public static Dictionary<int, int> GdiCounts(uint pid)
    {
        IntPtr dc = CreateCompatibleDC(IntPtr.Zero);   // ensures the table is mapped here
        try {
            var info = new PROCESS_BASIC_INFORMATION();
            int returned;
            if (NtQueryInformationProcess(GetCurrentProcess(), 0, ref info, Marshal.SizeOf(info), out returned) != 0) return null;
            IntPtr table = Marshal.ReadIntPtr(info.PebBaseAddress, 0xF8);
            if (table == IntPtr.Zero) return null;
            var counts = new Dictionary<int, int>();
            for (int i = 0; i < 65536; ++i) {
                IntPtr cell = IntPtr.Add(table, i * 24);
                if ((ushort)Marshal.ReadInt16(cell, 8) != (ushort)pid) continue;
                int type = Marshal.ReadInt16(cell, 14) & 0x7F;
                if (type == 0) continue;
                int current;
                counts.TryGetValue(type, out current);
                counts[type] = current + 1;
            }
            return counts;
        } finally {
            if (dc != IntPtr.Zero) DeleteDC(dc);
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SYSTEMTIME
    {
        public ushort Year, Month, DayOfWeek, Day, Hour, Minute, Second, Milliseconds;
    }
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetDateFormatEx(string locale, uint flags, ref SYSTEMTIME date, string format,
        StringBuilder buffer, int size, string calendar);

    // The user's short date, exactly as the app formats it (DATE_SHORTDATE).
    public static string ShortDate(int year, int month, int day)
    {
        var date = new SYSTEMTIME { Year = (ushort)year, Month = (ushort)month, Day = (ushort)day };
        var buffer = new StringBuilder(80);
        return GetDateFormatEx(null, 1, ref date, null, buffer, buffer.Capacity, null) > 0 ? buffer.ToString() : null;
    }
    [DllImport("user32.dll")]
    public static extern IntPtr GetWindowDpiAwarenessContext(IntPtr hwnd);
    [DllImport("user32.dll")]
    public static extern bool AreDpiAwarenessContextsEqual(IntPtr first, IntPtr second);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
    [DllImport("kernel32.dll")]
    private static extern bool FreeLibrary(IntPtr module);
    [DllImport("kernel32.dll")]
    private static extern IntPtr FindResourceW(IntPtr module, IntPtr name, IntPtr type);
    [DllImport("kernel32.dll")]
    private static extern IntPtr LoadResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll")]
    private static extern uint SizeofResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll")]
    private static extern IntPtr LockResource(IntPtr data);

    // Returns the embedded application manifest (RT_MANIFEST #1), or null.
    public static string ReadManifest(string path)
    {
        IntPtr module = LoadLibraryExW(path, IntPtr.Zero, 0x22);  // AS_DATAFILE | AS_IMAGE_RESOURCE
        if (module == IntPtr.Zero) return null;
        try {
            IntPtr resource = FindResourceW(module, new IntPtr(1), new IntPtr(24));
            if (resource == IntPtr.Zero) return null;
            uint size = SizeofResource(module, resource);
            IntPtr data = LockResource(LoadResource(module, resource));
            var bytes = new byte[size];
            Marshal.Copy(data, bytes, 0, (int)size);
            return Encoding.UTF8.GetString(bytes);
        } finally {
            FreeLibrary(module);
        }
    }

    public static RECT GetMonitorRect(IntPtr hwnd)
    {
        IntPtr hMon = MonitorFromWindow(hwnd, 1);
        MONITORINFO mi = new MONITORINFO();
        mi.cbSize = (uint)Marshal.SizeOf(typeof(MONITORINFO));
        GetMonitorInfo(hMon, ref mi);
        return mi.rcMonitor;
    }

    private static WindowInfo Describe(IntPtr hwnd)
    {
        RECT rect;
        GetWindowRect(hwnd, out rect);
        return new WindowInfo {
            Handle = hwnd,
            ClassName = ClassOf(hwnd),
            Visible = IsWindowVisible(hwnd),
            Style = unchecked((ulong)GetWindowLongPtrW(hwnd, -16).ToInt64()),
            ExStyle = unchecked((ulong)GetWindowLongPtrW(hwnd, -20).ToInt64()),
            Owner = GetWindow(hwnd, 4),
            Rect = rect
        };
    }

    public static string ClassOf(IntPtr hwnd)
    {
        var name = new StringBuilder(256);
        GetClassNameW(hwnd, name, name.Capacity);
        return name.ToString();
    }

    public static uint ProcessOf(IntPtr hwnd)
    {
        uint processId;
        GetWindowThreadProcessId(hwnd, out processId);
        return processId;
    }

    public static WindowInfo[] GetWindows(uint wantedProcessId)
    {
        var windows = new List<WindowInfo>();
        EnumWindows((hwnd, unused) => {
            if (ProcessOf(hwnd) == wantedProcessId) windows.Add(Describe(hwnd));
            return true;
        }, IntPtr.Zero);
        return windows.ToArray();
    }

    public static IntPtr TaskbarWindow()
    {
        return FindWindowExW(IntPtr.Zero, IntPtr.Zero, "Shell_TrayWnd", null);
    }

    // A process's windows parented directly to the taskbar (the embedded meter).
    public static WindowInfo[] GetTaskbarChildren(uint wantedProcessId)
    {
        var windows = new List<WindowInfo>();
        IntPtr taskbar = TaskbarWindow();
        if (taskbar == IntPtr.Zero) return windows.ToArray();
        for (IntPtr child = FindWindowExW(taskbar, IntPtr.Zero, null, null); child != IntPtr.Zero;
             child = FindWindowExW(taskbar, child, null, null)) {
            if (ProcessOf(child) == wantedProcessId) windows.Add(Describe(child));
        }
        return windows.ToArray();
    }

    [DllImport("user32.dll")]
    private static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);

    public static void PressKey(byte key, bool extended)
    {
        uint flags = extended ? 1u : 0u;                    // KEYEVENTF_EXTENDEDKEY
        keybd_event(key, 0, flags, UIntPtr.Zero);
        keybd_event(key, 0, flags | 2u, UIntPtr.Zero);      // KEYEVENTF_KEYUP
    }

    [DllImport("user32.dll")]
    private static extern IntPtr GetDC(IntPtr hwnd);
    [DllImport("user32.dll")]
    private static extern int ReleaseDC(IntPtr hwnd, IntPtr dc);
    [DllImport("gdi32.dll")]
    private static extern IntPtr CreateCompatibleBitmap(IntPtr dc, int width, int height);
    [DllImport("gdi32.dll")]
    private static extern IntPtr SelectObject(IntPtr dc, IntPtr gdiObject);
    [DllImport("gdi32.dll")]
    private static extern bool DeleteObject(IntPtr gdiObject);
    [DllImport("gdi32.dll")]
    private static extern bool BitBlt(IntPtr destination, int x, int y, int width, int height,
        IntPtr source, int sourceX, int sourceY, uint operation);
    [StructLayout(LayoutKind.Sequential)]
    private struct BITMAPINFO
    {
        public uint biSize;
        public int biWidth, biHeight;
        public ushort biPlanes, biBitCount;
        public uint biCompression, biSizeImage;
        public int biXPelsPerMeter, biYPelsPerMeter;
        public uint biClrUsed, biClrImportant;
        public uint mask0, mask1, mask2;   // room for colour masks
    }
    [DllImport("gdi32.dll")]
    private static extern int GetDIBits(IntPtr dc, IntPtr bitmap, uint start, uint lines,
        [Out] int[] bits, ref BITMAPINFO info, uint usage);

    // Pixels of a screen rectangle, as composed on screen (layered windows
    // included), that are within tolerance of a colour in every channel.
    public static int CountScreenPixels(RECT rect, int red, int green, int blue, int tolerance)
    {
        int width = rect.Right - rect.Left, height = rect.Bottom - rect.Top;
        if (width <= 0 || height <= 0) return 0;
        IntPtr screen = GetDC(IntPtr.Zero);
        IntPtr memory = CreateCompatibleDC(screen);
        IntPtr bitmap = CreateCompatibleBitmap(screen, width, height);
        try {
            IntPtr previous = SelectObject(memory, bitmap);
            BitBlt(memory, 0, 0, width, height, screen, rect.Left, rect.Top, 0x00CC0020 | 0x40000000);  // SRCCOPY | CAPTUREBLT
            SelectObject(memory, previous);
            var info = new BITMAPINFO { biSize = 40, biWidth = width, biHeight = -height, biPlanes = 1, biBitCount = 32 };
            var bits = new int[width * height];
            if (GetDIBits(memory, bitmap, 0, (uint)height, bits, ref info, 0) == 0) return -1;
            int count = 0;
            foreach (int pixel in bits) {
                if (Math.Abs(((pixel >> 16) & 0xFF) - red) <= tolerance &&
                    Math.Abs(((pixel >> 8) & 0xFF) - green) <= tolerance &&
                    Math.Abs((pixel & 0xFF) - blue) <= tolerance) {
                    ++count;
                }
            }
            return count;
        } finally {
            DeleteObject(bitmap);
            DeleteDC(memory);
            ReleaseDC(IntPtr.Zero, screen);
        }
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
# Every run gets its own settings file and the app uses its own Run-key value
# name in --integration-test mode, so a real installation is never touched.
$settingsDirectory = Join-Path ([IO.Path]::GetTempPath()) ('WinNetMeter-it-' + [guid]::NewGuid().ToString('N'))
$settingsPath = Join-Path $settingsDirectory 'settings.ini'
$appArguments = '--integration-test --settings "{0}"' -f $settingsPath
$startupValueName = 'WinNetMeter.IntegrationTest'
$mainClass = 'WinNetMeterMainTest'
$versionHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\version.h')
if ($versionHeader -notmatch '(?m)^#define WINNETMETER_VERSION_STRING "([^"]+)"\r?$') { throw 'Version header is malformed' }
$sourceVersion = $Matches[1]

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

# Live meter edits update the UI immediately but their settings write is
# debounced, so persistence assertions poll instead of reading once.
function Wait-IniString([string]$Section, [string]$Key, [string]$Expected, [int]$TimeoutMs = 6000) {
    $buffer = New-Object Text.StringBuilder 256
    $deadline = [Environment]::TickCount + $TimeoutMs
    do {
        [void]$buffer.Clear()
        [void][WinNetMeterNative]::GetPrivateProfileStringW($Section, $Key, '', $buffer, $buffer.Capacity, $settingsPath)
        if ($buffer.ToString() -eq $Expected) { return $true }
        Start-Sleep -Milliseconds 100
    } while ([Environment]::TickCount -lt $deadline)
    Write-Host "  Wait-IniString($Section/$Key) timed out; last value '$buffer', expected '$Expected'"
    return $false
}

function Wait-IniInt([string]$Section, [string]$Key, [int]$Expected, [int]$TimeoutMs = 6000) {
    $deadline = [Environment]::TickCount + $TimeoutMs
    do {
        $actual = [WinNetMeterNative]::GetPrivateProfileIntW($Section, $Key, [int]::MinValue, $settingsPath)
        if ($actual -eq $Expected) { return $true }
        Start-Sleep -Milliseconds 100
    } while ([Environment]::TickCount -lt $deadline)
    Write-Host "  Wait-IniInt($Section/$Key) timed out; last value '$actual', expected '$Expected'"
    return $false
}

function Get-RunningAppProcesses {
    @(Get-Process -Name WinNetMeter -ErrorAction SilentlyContinue | Where-Object {
        try { $_.Path -eq $exePath } catch { $false }
    })
}

function Get-EmbeddedMeters([System.Diagnostics.Process]$Process) {
    @([WinNetMeterNative]::GetTaskbarChildren([uint32]$Process.Id) | Where-Object ClassName -eq 'WinNetMeterEmbedded')
}

function Wait-AppWindows([System.Diagnostics.Process]$Process) {
    for ($attempt = 0; $attempt -lt 50; ++$attempt) {
        if ($Process.HasExited) { throw "WinNetMeter exited during startup with code $($Process.ExitCode)" }
        $windows = @([WinNetMeterNative]::GetWindows([uint32]$Process.Id))
        $hasMain = @($windows | Where-Object ClassName -eq $mainClass).Count -eq 1
        # The meter is a top-level overlay, or a child of the taskbar in embedded mode.
        $hasMeter = @($windows | Where-Object ClassName -eq 'WinNetMeterOverlay').Count -eq 1 -or
                    @(Get-EmbeddedMeters $Process).Count -eq 1
        if ($hasMain -and $hasMeter) {
            return $windows
        }
        Start-Sleep -Milliseconds 100
    }
    throw 'Timed out waiting for WinNetMeter host and meter windows'
}

function Start-TestApp([string]$SettingsContent = "[Overlay]`r`nShowWidget=1`r`n", [switch]$KeepSettings) {
    Assert-True (@(Get-RunningAppProcesses).Count -eq 0) 'A WinNetMeter process from this build is already running'
    if (-not $KeepSettings) {
        [IO.Directory]::CreateDirectory($settingsDirectory) | Out-Null
        [IO.File]::WriteAllText($settingsPath, $SettingsContent)
    }

    $process = $null
    try {
        $process = Start-Process -FilePath $exePath -ArgumentList $appArguments -PassThru
        $windows = Wait-AppWindows $process
        return [pscustomobject]@{
            Process = $process
            Windows = $windows
        }
    } catch {
        if ($null -ne $process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            Wait-Process -Id $process.Id -Timeout 5 -ErrorAction SilentlyContinue
        }
        throw
    }
}

function Stop-TestApp($Session) {
    if (-not $Session.Process.HasExited) {
        Stop-Process -Id $Session.Process.Id -Force -ErrorAction SilentlyContinue
        Wait-Process -Id $Session.Process.Id -Timeout 5 -ErrorAction SilentlyContinue
    }
}

# The meter runs on its own thread and posts a double-click to the UI thread,
# so the window opens asynchronously; wait for it instead of reading once.
function Wait-WindowVisible($Session, [string]$ClassName, [bool]$Visible = $true, [int]$TimeoutMs = 3000) {
    $deadline = [Environment]::TickCount + $TimeoutMs
    do {
        if ((Get-AppWindow $Session $ClassName).Visible -eq $Visible) { return $true }
        Start-Sleep -Milliseconds 25
    } while ([Environment]::TickCount -lt $deadline)
    return $false
}

function Get-IniString([string]$Section, [string]$Key) {
    $buffer = New-Object Text.StringBuilder 1024
    [void][WinNetMeterNative]::GetPrivateProfileStringW($Section, $Key, '', $buffer, $buffer.Capacity, $settingsPath)
    $buffer.ToString()
}

# Reverses the app's text encoding for INI values: 'x' + four hex digits per UTF-16 unit.
function ConvertFrom-HexText([string]$Encoded) {
    if (-not $Encoded.StartsWith('x') -or (($Encoded.Length - 1) % 4) -ne 0) { return $null }
    -join @(for ($i = 1; $i -lt $Encoded.Length; $i += 4) { [char][Convert]::ToInt32($Encoded.Substring($i, 4), 16) })
}

function Get-ComboText([IntPtr]$Combo, [int]$Index) {
    $length = [int][WinNetMeterNative]::SendMessageW($Combo, 0x0149, [IntPtr]$Index, [IntPtr]::Zero)
    $text = New-Object Text.StringBuilder ([Math]::Max($length + 1, 16))
    [void][WinNetMeterNative]::SendMessageTextW($Combo, 0x0148, [IntPtr]$Index, $text)
    $text.ToString()
}

function Select-ComboItem([IntPtr]$Main, [IntPtr]$Combo, [int]$Id, [int]$Index) {
    [void][WinNetMeterNative]::SendMessageW($Combo, 0x014E, [IntPtr]$Index, [IntPtr]::Zero)
    [void][WinNetMeterNative]::SendMessageW($Main, 0x0111, [IntPtr]($Id -bor (1 -shl 16)), $Combo)
}

# The adapter Windows routes 1.1.1.1 through, by friendly name (what the app shows).
function Get-DefaultRouteAlias {
    $index = [uint32]0
    if ([WinNetMeterNative]::GetBestInterface(0x01010101, [ref]$index) -ne 0) { return $null }
    foreach ($nic in [System.Net.NetworkInformation.NetworkInterface]::GetAllNetworkInterfaces()) {
        try { if ($nic.GetIPProperties().GetIPv4Properties().Index -eq $index) { return $nic.Name } } catch { }
    }
    $null
}

function Remove-TestSettings {
    if (Test-Path -LiteralPath $settingsDirectory) {
        Remove-Item -LiteralPath $settingsDirectory -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Get-AppWindow($Session, [string]$ClassName) {
    $windows = @([WinNetMeterNative]::GetWindows([uint32]$Session.Process.Id))
    $match = @($windows | Where-Object ClassName -eq $ClassName)
    Assert-True ($match.Count -eq 1) "Expected one $ClassName window, found $($match.Count)"
    $match[0]
}

function Get-EmbeddedMeter($Session) {
    $match = @(Get-EmbeddedMeters $Session.Process)
    Assert-True ($match.Count -eq 1) "Expected one WinNetMeterEmbedded window, found $($match.Count)"
    $match[0]
}

# Waits until the meter is in one mode only: a visible child of the taskbar and
# no overlay, or a visible overlay and no child.
function Wait-MeterMode($Session, [bool]$Embedded, [int]$TimeoutMs = 4000) {
    $deadline = [Environment]::TickCount + $TimeoutMs
    do {
        $children = @(Get-EmbeddedMeters $Session.Process)
        $overlays = @([WinNetMeterNative]::GetWindows([uint32]$Session.Process.Id) | Where-Object ClassName -eq 'WinNetMeterOverlay')
        $meters = @(if ($Embedded) { $children } else { $overlays })
        $others = @(if ($Embedded) { $overlays } else { $children })
        if ($meters.Count -eq 1 -and $meters[0].Visible -and $others.Count -eq 0) { return $true }
        Start-Sleep -Milliseconds 50
    } while ([Environment]::TickCount -lt $deadline)
    Write-Host "  Wait-MeterMode(embedded=$Embedded) timed out: $($children.Count) embedded, $($overlays.Count) overlay"
    return $false
}

# The embed option takes effect on Apply, like the other check boxes beside it.
function Set-EmbedMode($Session, [IntPtr]$Main, [bool]$On) {
    $box = [WinNetMeterNative]::GetDlgItem($Main, 2035)
    Assert-True ($box -ne [IntPtr]::Zero) 'Embed control was not found'
    [void][WinNetMeterNative]::SendMessageW($box, 0x00F1, [IntPtr][int]$On, [IntPtr]::Zero)
    [void][WinNetMeterNative]::SendMessageW($Main, 0x0111, [IntPtr]2005, [IntPtr]::Zero)
    Assert-True (Wait-IniString 'Overlay' 'Embed' ([string][int]$On)) "Embed=$([int]$On) was not saved"
    Assert-True (Wait-MeterMode $Session $On) "The meter did not switch to $(if ($On) { 'embedded' } else { 'overlay' }) mode"
}

# Magenta pixels on screen where the meter is (the StartMenu check draws it magenta).
function Measure-MeterPixels($Session, [bool]$Embedded) {
    $meter = if ($Embedded) { Get-EmbeddedMeter $Session } else { Get-AppWindow $Session 'WinNetMeterOverlay' }
    [WinNetMeterNative]::CountScreenPixels($meter.Rect, 255, 0, 255, 40)
}

function Wait-MeterPixels($Session, [bool]$Embedded, [int]$Minimum, [int]$TimeoutMs = 3000) {
    $best = 0
    $deadline = [Environment]::TickCount + $TimeoutMs
    do {
        $best = [Math]::Max($best, (Measure-MeterPixels $Session $Embedded))
        if ($best -ge $Minimum) { break }
        Start-Sleep -Milliseconds 100
    } while ([Environment]::TickCount -lt $deadline)
    $best
}

function Test-StartOpen {
    $foreground = [WinNetMeterNative]::GetForegroundWindow()
    if ($foreground -eq [IntPtr]::Zero -or [WinNetMeterNative]::ClassOf($foreground) -ne 'Windows.UI.Core.CoreWindow') {
        return $false
    }
    $owner = Get-Process -Id ([WinNetMeterNative]::ProcessOf($foreground)) -ErrorAction SilentlyContinue
    $null -ne $owner -and $owner.ProcessName -in @('StartMenuExperienceHost', 'SearchHost', 'SearchApp')
}

function Open-StartMenu {
    [WinNetMeterNative]::PressKey(0x5B, $true)   # VK_LWIN
    $deadline = [Environment]::TickCount + 3000
    do {
        Start-Sleep -Milliseconds 100
        if (Test-StartOpen) {
            Start-Sleep -Milliseconds 800   # let the opening animation finish
            return $true
        }
    } while ([Environment]::TickCount -lt $deadline)
    $false
}

# Escape is only sent while Start itself has the keyboard, never to whatever
# else is in the foreground.
function Close-StartMenu {
    for ($attempt = 0; $attempt -lt 3 -and (Test-StartOpen); ++$attempt) {
        [WinNetMeterNative]::PressKey(0x1B, $false)   # VK_ESCAPE
        Start-Sleep -Milliseconds 500
    }
}

# Closes Start and returns how many ms after Start lost the foreground the
# overlay was fully back on screen; -1 if it did not come back, -2 if Start
# was not open, -3 if Start did not close.
function Close-StartMenuTimed($Session, [int]$Minimum, [int]$TimeoutMs = 3000) {
    $overlay = Get-AppWindow $Session 'WinNetMeterOverlay'
    if (-not (Test-StartOpen)) {
        $foreground = [WinNetMeterNative]::GetForegroundWindow()
        Write-Host "  Start was not in the foreground; '$([WinNetMeterNative]::ClassOf($foreground))' was"
        return -2
    }
    [WinNetMeterNative]::PressKey(0x1B, $false)   # VK_ESCAPE
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $closed = $null
    $pixels = 0
    while ($clock.ElapsedMilliseconds -lt $TimeoutMs) {
        if ($null -eq $closed -and -not (Test-StartOpen)) { $closed = $clock.ElapsedMilliseconds }
        if ($null -ne $closed) {
            $pixels = [WinNetMeterNative]::CountScreenPixels($overlay.Rect, 255, 0, 255, 40)
            if ($pixels -ge $Minimum) { return [int]($clock.ElapsedMilliseconds - $closed) }
        }
        Start-Sleep -Milliseconds 10
    }
    if ($null -eq $closed) {
        Write-Host '  Start did not close after Escape'
        return -3
    }
    $now = Get-AppWindow $Session 'WinNetMeterOverlay'
    $foreground = [WinNetMeterNative]::GetForegroundWindow()
    Write-Host ("  Overlay not back: Start closed at {0} ms, {1} of {2} pixels; overlay visible={3} at [{4},{5},{6},{7}], measured at [{8},{9},{10},{11}]; foreground '{12}'" -f
        $closed, $pixels, $Minimum, $now.Visible, $now.Rect.Left, $now.Rect.Top, $now.Rect.Right, $now.Rect.Bottom,
        $overlay.Rect.Left, $overlay.Rect.Top, $overlay.Rect.Right, $overlay.Rect.Bottom, [WinNetMeterNative]::ClassOf($foreground))
    -1
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
    $manifest = [WinNetMeterNative]::ReadManifest($exePath)
    Assert-True ($null -ne $manifest) 'No embedded application manifest'
    Assert-True ($manifest -match 'name="Microsoft\.Windows\.Common-Controls"\s+version="6\.0\.0\.0"') 'Manifest lacks the Common Controls 6 dependency'
    Assert-True ($manifest -match '>PerMonitorV2<') 'Manifest does not declare PerMonitorV2'
    Assert-True ($manifest -match '\{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a\}') 'Manifest does not declare Windows 10 support'
    Assert-True ($manifest -match 'level="asInvoker"') 'Manifest does not request asInvoker'
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
} elseif ($Check -eq 'ThemeColors') {
    # As written by 0.1.x: no SettingsVersion, white download (the old default), red upload.
    "[Overlay]`r`nShowWidget=1`r`nDownloadColor=16777215`r`nUploadColor=255`r`n"
} elseif ($Check -eq 'SpeedUnits') {
    "[Overlay]`r`nShowWidget=1`r`nSpeedUnits=bits`r`nMinimumSpeedUnit=MB/s`r`nDecimalPlaces=1`r`n"
} elseif ($Check -eq 'Embedded') {
    "[Overlay]`r`nShowWidget=1`r`nEmbed=1`r`n"
} elseif ($Check -eq 'StartMenu') {
    # Magenta "WWWW 0 GB/s" on both lines: a constant patch of unmistakable pixels.
    "[Overlay]`r`nShowWidget=1`r`nEmbed=0`r`nDownloadColor=16711935`r`nUploadColor=16711935`r`nDownloadPrefix=x0057005700570057`r`nUploadPrefix=x0057005700570057`r`nMinimumSpeedUnit=GB/s`r`nDecimalPlaces=0`r`n"
} elseif ($Check -eq 'CustomizationTotals') {
    "[Overlay]`r`nShowWidget=1`r`nDownloadPrefix=x0044003A`r`nUploadPrefix=x0055003A`r`nDownloadColor=1971210`r`nUploadColor=6592200`r`nFontFamily=Arial`r`nFontSize=11.0`r`nFontStyle=0`r`nTaskbarOffset=37`r`nMinimumSpeedUnit=GB/s`r`nDecimalPlaces=0`r`n[Totals]`r`nDownloaded=8388608`r`nUploaded=3145728`r`nSince=2024-02-29`r`n"
} else {
    "[Overlay]`r`nShowWidget=1`r`n"
}
$startupKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
if ($Check -eq 'Preferences') {
    Remove-ItemProperty -LiteralPath $startupKey -Name $startupValueName -ErrorAction SilentlyContinue
}

$session = $null
try {
    $session = Start-TestApp $testSettings
    switch ($Check) {
        'SingleInstance' {
            $duplicates = @(1..8 | ForEach-Object { Start-Process -FilePath $exePath -ArgumentList $appArguments -PassThru })
            foreach ($duplicate in $duplicates) {
                Assert-True ($duplicate.WaitForExit(3000)) 'A duplicate instance did not exit within three seconds'
                Assert-True ($duplicate.ExitCode -eq 0) "A duplicate instance exited with code $($duplicate.ExitCode)"
            }
            Assert-True (@(Get-RunningAppProcesses).Count -eq 1) 'Duplicate launch left more than one live process'
            'SINGLE_INSTANCE_OK'
        }
        'DuplicateUi' {
            $before = @([WinNetMeterNative]::GetWindows([uint32]$session.Process.Id))
            $second = Start-Process -FilePath $exePath -ArgumentList $appArguments -PassThru
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
            # Without the manifest's Common Controls 6 dependency the process binds the
            # legacy 5.82 assembly and the settings window gets unthemed controls.
            $session.Process.Refresh()
            $comctl = @($session.Process.Modules | Where-Object ModuleName -ieq 'comctl32.dll' | ForEach-Object FileName)
            Assert-True (@($comctl | Where-Object { $_ -match 'common-controls_6595b64144ccf1df_6\.' }).Count -ge 1) "Common Controls 6 not loaded: $($comctl -join '; ')"
            Assert-True (@($comctl | Where-Object { $_ -match 'common-controls_6595b64144ccf1df_5\.' }).Count -eq 0) "Legacy Common Controls 5.82 loaded: $($comctl -join '; ')"
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
            Assert-True (Wait-WindowVisible $session $mainClass) 'Taskbar-meter activation did not show the unified window'
            $shownMain = Get-AppWindow $session $mainClass
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
            $context = [WinNetMeterNative]::GetWindowDpiAwarenessContext($overlay.Handle)
            Assert-True ([WinNetMeterNative]::AreDpiAwarenessContextsEqual($context, [IntPtr](-4))) 'Overlay is not per-monitor v2 DPI aware'
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
            # A leak grows with the amount of work; a cache plateaus. Measured in 0.2.0
            # (M8): when the UI thread's tray update overlaps the meter thread's
            # layered-window update, the window manager caches an extra GDI region or
            # DC for the process (5000 ticks plateau at +1; never with the tray icon
            # off). So after a warm-up round, 600 more ticks must leave USER objects
            # (windows, icons, menus) exactly unchanged and GDI objects within 3,
            # which still catches any leak of one object per 150 ticks or faster.
            $process = $session.Process
            $round = {
                1..200 | ForEach-Object { [void][WinNetMeterNative]::SendMessageW($main.Handle, 0x0113, [IntPtr]1, [IntPtr]::Zero) }
                Start-Sleep -Milliseconds 400   # the meter thread renders asynchronously
            }
            $describe = { param($types) if ($null -eq $types) { 'n/a' } else { ($types.Keys | Sort-Object | ForEach-Object { '0x{0:X2}={1}' -f $_, $types[$_] }) -join ' ' } }
            # Both meter modes render differently (top-level vs. child of the taskbar).
            foreach ($mode in @('overlay', 'embedded')) {
                if ($mode -eq 'embedded') { Set-EmbedMode $session $main.Handle $true }
                & $round
                $gdiBefore = [WinNetMeterNative]::GetGuiResources($process.Handle, 0)
                $userBefore = [WinNetMeterNative]::GetGuiResources($process.Handle, 1)
                $typesBefore = [WinNetMeterNative]::GdiCounts([uint32]$process.Id)
                1..3 | ForEach-Object { & $round }
                $gdiAfter = [WinNetMeterNative]::GetGuiResources($process.Handle, 0)
                $userAfter = [WinNetMeterNative]::GetGuiResources($process.Handle, 1)
                $typesAfter = [WinNetMeterNative]::GdiCounts([uint32]$process.Id)
                Assert-True ($userAfter -eq $userBefore) "User objects changed ($mode): $userBefore -> $userAfter"
                Assert-True (($gdiAfter - $gdiBefore) -le 3) ("GDI objects grew ({0}) {1} -> {2} over 600 ticks; by type before [{3}] after [{4}]" -f
                    $mode, $gdiBefore, $gdiAfter, (& $describe $typesBefore), (& $describe $typesAfter))
                "RESOURCE_COUNTS ($mode) GDI=$gdiBefore->$gdiAfter USER=$userBefore->$userAfter over 600 ticks"
            }
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
            # A 0.1.x-style file (TaskbarOffset, no Anchor) keeps the classic fixed point.
            $anchorCombo = [WinNetMeterNative]::GetDlgItem($main.Handle, 2034)
            Assert-True ([int][WinNetMeterNative]::SendMessageW($anchorCombo, 0x0147, [IntPtr]::Zero, [IntPtr]::Zero) -eq 3) 'Legacy file did not keep the classic position'

            $text = New-Object Text.StringBuilder 64
            [void][WinNetMeterNative]::SendMessageTextW($downPrefix, 0x000D, [IntPtr]$text.Capacity, $text)
            Assert-True ($text.ToString() -eq 'D:') "Download prefix did not load: '$text'"
            [void]$text.Clear()
            [void][WinNetMeterNative]::SendMessageTextW($upPrefix, 0x000D, [IntPtr]$text.Capacity, $text)
            Assert-True ($text.ToString() -eq 'U:') "Upload prefix did not load: '$text'"

            $since = 'Total data since ' + [WinNetMeterNative]::ShortDate(2024, 2, 29)
            foreach ($item in @(@(109, $since), @(111, '8.00 MB'), @(113, '3.00 MB'))) {
                [void]$text.Clear()
                [void][WinNetMeterNative]::GetWindowTextW(
                    [WinNetMeterNative]::GetDlgItem($main.Handle, $item[0]), $text, $text.Capacity)
                Assert-True ($text.ToString() -eq $item[1]) "Unexpected total display for $($item[0]): $text"
            }

            $overlayBefore = New-Object WinNetMeterNative+RECT
            [void][WinNetMeterNative]::GetWindowRect($overlay.Handle, [ref]$overlayBefore)
            Assert-True ([WinNetMeterNative]::SendMessageStringW($downPrefix, 0x000C, [IntPtr]::Zero, 'DL:') -ne [IntPtr]::Zero) 'Could not edit download prefix'
            Assert-True ([WinNetMeterNative]::SendMessageStringW($upPrefix, 0x000C, [IntPtr]::Zero, 'UL:') -ne [IntPtr]::Zero) 'Could not edit upload prefix'
            Assert-True (Wait-IniString 'Overlay' 'DownloadPrefix' 'x0044004C003A') 'Download prefix was not persisted'
            Assert-True (Wait-IniString 'Overlay' 'UploadPrefix' 'x0055004C003A') 'Upload prefix was not persisted'
            Assert-True ([WinNetMeterNative]::SendMessageStringW($offsetEdit, 0x000C, [IntPtr]::Zero, '80') -ne [IntPtr]::Zero) 'Could not edit meter offset'
            Assert-True (Wait-IniInt 'Overlay' 'TaskbarOffset' 80) 'Taskbar offset was not persisted'
            $overlayAfter = New-Object WinNetMeterNative+RECT
            [void][WinNetMeterNative]::GetWindowRect($overlay.Handle, [ref]$overlayAfter)
            Assert-True ($overlayBefore.Left -ne $overlayAfter.Left -or $overlayBefore.Top -ne $overlayAfter.Top) 'Taskbar meter did not move live'

            [void][WinNetMeterNative]::SendMessageW($main.Handle, 0x0111, [IntPtr]2016, [IntPtr]::Zero)
            Assert-True (Wait-IniInt 'Overlay' 'TaskbarOffset' 0) 'Meter reset did not reset offset'
            Assert-True (Wait-IniString 'Overlay' 'Anchor' 'tray') 'Meter reset did not restore the default position'
            Assert-True (Wait-IniString 'Overlay' 'DownloadColor' 'auto') 'Meter reset did not restore the automatic download color'
            Assert-True (Wait-IniString 'Overlay' 'UploadColor' 'auto') 'Meter reset did not restore the automatic upload color'
            Assert-True (Wait-IniInt 'Overlay' 'FontStyle' 1) 'Meter reset did not reset font style'
            Assert-True (Wait-IniInt 'Overlay' 'DecimalPlaces' 2) 'Meter reset did not reset decimal places'
            foreach ($item in @(@('DownloadPrefix', 'x2193'), @('UploadPrefix', 'x2191'), @('FontFamily', 'Segoe UI'), @('FontSize', '8.0'), @('MinimumSpeedUnit', 'Auto'))) {
                Assert-True (Wait-IniString 'Overlay' $item[0] $item[1]) "Meter reset did not reset $($item[0])"
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
            Assert-True (Wait-WindowVisible $session $mainClass) 'Meter double-click did not open the unified window'

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
            $registered = Get-ItemPropertyValue -LiteralPath $startupKey -Name $startupValueName -ErrorAction Stop
            Assert-True ($registered -eq ('"' + $exePath + '"')) 'Startup command does not quote the current executable'
            $realStartup = (Get-ItemProperty -LiteralPath $startupKey).PSObject.Properties['WinNetMeter']
            Assert-True ($null -eq $realStartup -or $realStartup.Value -ne ('"' + $exePath + '"')) 'Test instance wrote the real startup entry'

            [void][WinNetMeterNative]::SendMessageW($main.Handle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-True (-not (Get-AppWindow $session $mainClass).Visible) 'Closing did not hide the unified window'
            [void][WinNetMeterNative]::SendMessageW($overlay.Handle, 0x0203, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-True (Wait-WindowVisible $session $mainClass) 'Meter could not reopen the app after hiding the tray icon'

            [void][WinNetMeterNative]::SendMessageW($startup, 0x00F1, [IntPtr]::Zero, [IntPtr]::Zero)
            [void][WinNetMeterNative]::SendMessageW($main.Handle, 0x0111, [IntPtr]2005, [IntPtr]::Zero)
            $startupProperties = Get-ItemProperty -LiteralPath $startupKey
            Assert-True ($null -eq $startupProperties.PSObject.Properties[$startupValueName]) 'Startup entry was not removed'
            'PREFERENCES_INTEGRATION_OK'
        }
        'SaveFailureDialog' {
            $main = Get-AppWindow $session $mainClass
            $overlay = Get-AppWindow $session 'WinNetMeterOverlay'
            $unitCombo = [WinNetMeterNative]::GetDlgItem($main.Handle, 2018)
            Assert-True ($unitCombo -ne [IntPtr]::Zero) 'Minimum speed unit control was not found'
            Set-ItemProperty -LiteralPath $settingsPath -Name IsReadOnly -Value $true
            try {
                # A live meter change persists at once; the read-only file makes that save fail.
                [void][WinNetMeterNative]::SendMessageW($main.Handle, 0x0111, [IntPtr](2018 -bor (1 -shl 16)), $unitCombo)
                # Opening the window reports the failure in a modal box. The second request is
                # dispatched inside that box's modal loop and must not stack another dialog.
                Assert-True ([WinNetMeterNative]::PostMessageW($overlay.Handle, 0x0203, [IntPtr]::Zero, [IntPtr]::Zero)) 'First open request failed'
                Start-Sleep -Milliseconds 700
                Assert-True ([WinNetMeterNative]::PostMessageW($overlay.Handle, 0x0203, [IntPtr]::Zero, [IntPtr]::Zero)) 'Second open request failed'
                Start-Sleep -Milliseconds 900
                $dialogs = @([WinNetMeterNative]::GetWindows([uint32]$session.Process.Id) |
                    Where-Object { $_.ClassName -eq '#32770' -and $_.Visible })
                foreach ($dialog in $dialogs) {
                    [void][WinNetMeterNative]::PostMessageW($dialog.Handle, 0x0111, [IntPtr]1, [IntPtr]::Zero)
                }
                Assert-True ($dialogs.Count -eq 1) "Expected one save-failure dialog, found $($dialogs.Count)"
                'SAVE_FAILURE_DIALOG_OK'
            } finally {
                Set-ItemProperty -LiteralPath $settingsPath -Name IsReadOnly -Value $false
            }
        }
        'AdapterSelection' {
            $main = Get-AppWindow $session $mainClass
            $combo = [WinNetMeterNative]::GetDlgItem($main.Handle, 101)
            Assert-True ($combo -ne [IntPtr]::Zero) 'Adapter list was not found'
            $count = [int][WinNetMeterNative]::SendMessageW($combo, 0x0146, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-True ($count -ge 2) "Expected Automatic plus at least one adapter, found $count entries"

            # Fresh settings: Automatic, labelled with the adapter that carries the default route.
            Assert-True ([int][WinNetMeterNative]::SendMessageW($combo, 0x0147, [IntPtr]::Zero, [IntPtr]::Zero) -eq 0) 'Automatic is not selected by default'
            $automatic = Get-ComboText $combo 0
            $expected = Get-DefaultRouteAlias
            if ($expected) {
                Assert-True ($automatic -eq "Automatic ($expected)") "Automatic shows '$automatic'; the default route uses '$expected'"
            } else {
                Assert-True ($automatic -like 'Automatic*') "First entry is not Automatic: '$automatic'"
            }

            # A manual choice is saved at once (LUID + name) and survives a restart.
            $choice = $count - 1
            $choiceName = Get-ComboText $combo $choice
            Select-ComboItem $main.Handle $combo 101 $choice
            Assert-True ((Get-IniString 'Network' 'Adapter') -match '^[0-9A-F]{16}$') "Manual adapter LUID not saved: '$(Get-IniString 'Network' 'Adapter')'"
            Assert-True ((ConvertFrom-HexText (Get-IniString 'Network' 'AdapterName')) -eq $choiceName) 'Manual adapter name not saved'

            Stop-TestApp $session
            $session = Start-TestApp -KeepSettings
            $main = Get-AppWindow $session $mainClass
            $combo = [WinNetMeterNative]::GetDlgItem($main.Handle, 101)
            $selected = [int][WinNetMeterNative]::SendMessageW($combo, 0x0147, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-True ($selected -ge 1 -and (Get-ComboText $combo $selected) -eq $choiceName) "Restart did not restore '$choiceName' (selected index $selected)"

            Select-ComboItem $main.Handle $combo 101 0
            Assert-True ((Get-IniString 'Network' 'Adapter') -eq 'auto') 'Switching back to Automatic was not saved'
            'ADAPTER_SELECTION_OK'
        }
        'SpeedUnits' {
            $main = Get-AppWindow $session $mainClass
            $units = [WinNetMeterNative]::GetDlgItem($main.Handle, 2030)
            $minimum = [WinNetMeterNative]::GetDlgItem($main.Handle, 2018)
            Assert-True ($units -ne [IntPtr]::Zero -and $minimum -ne [IntPtr]::Zero) 'Unit controls were not found'
            $speedTexts = {
                foreach ($id in @(102, 103)) {
                    $text = New-Object Text.StringBuilder 64
                    [void][WinNetMeterNative]::GetWindowTextW([WinNetMeterNative]::GetDlgItem($main.Handle, $id), $text, $text.Capacity)
                    $text.ToString()
                }
            }

            # Loaded as bits with a megabit floor and one decimal.
            Assert-True ([int][WinNetMeterNative]::SendMessageW($units, 0x0147, [IntPtr]::Zero, [IntPtr]::Zero) -eq 1) 'Bits were not selected from the settings file'
            Assert-True ((Get-ComboText $minimum 2) -eq 'Mbps') "Minimum-unit choices are not in bits: '$(Get-ComboText $minimum 2)'"
            foreach ($text in (& $speedTexts)) { Assert-True ($text -match '^\d+\.\d (Mbps|Gbps)$') "Unexpected bit-rate text: '$text'" }

            # Switching to bytes applies live, relabels the choices and is saved at once.
            Select-ComboItem $main.Handle $units 2030 0
            Assert-True ((Get-IniString 'Overlay' 'SpeedUnits') -eq 'bytes') 'Byte units were not saved'
            Assert-True ((Get-ComboText $minimum 2) -eq 'MB/s') "Minimum-unit choices are not in bytes: '$(Get-ComboText $minimum 2)'"
            foreach ($text in (& $speedTexts)) { Assert-True ($text -match '^\d+\.\d (MB|GB)/s$') "Unexpected byte-rate text: '$text'" }
            'SPEED_UNITS_OK'
        }
        'ThemeColors' {
            $main = Get-AppWindow $session $mainClass
            $upAuto = [WinNetMeterNative]::GetDlgItem($main.Handle, 2031)
            $downAuto = [WinNetMeterNative]::GetDlgItem($main.Handle, 2032)
            Assert-True ($upAuto -ne [IntPtr]::Zero -and $downAuto -ne [IntPtr]::Zero) 'Automatic color controls were not found'
            $checked = { param([IntPtr]$box) [int][WinNetMeterNative]::SendMessageW($box, 0x00F0, [IntPtr]::Zero, [IntPtr]::Zero) -eq 1 }
            $click = {
                param([IntPtr]$box, [int]$id, [bool]$on)
                [void][WinNetMeterNative]::SendMessageW($box, 0x00F1, [IntPtr]([int]$on), [IntPtr]::Zero)
                [void][WinNetMeterNative]::SendMessageW($main.Handle, 0x0111, [IntPtr]$id, $box)
            }

            # Migration: the old white default became Automatic; the chosen red stayed.
            Assert-True (& $checked $downAuto) 'Legacy white download color did not migrate to Automatic'
            Assert-True (-not (& $checked $upAuto)) 'Legacy custom upload color was turned into Automatic'

            # Turning Automatic off pins the color currently on screen for this theme.
            $light = (Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' -ErrorAction SilentlyContinue).PSObject.Properties['SystemUsesLightTheme']
            $expected = if ($light -and $light.Value -ne 0) { 28 + 28 * 256 + 28 * 65536 } else { 16777215 }
            & $click $downAuto 2032 $false
            Assert-True ((Get-IniString 'Overlay' 'DownloadColor') -eq "$expected") "Pinned color is '$(Get-IniString 'Overlay' 'DownloadColor')', expected $expected"
            Assert-True ((Get-IniString 'Overlay' 'UploadColor') -eq '255') 'Upload color changed unexpectedly'
            Assert-True ((Get-IniString 'General' 'SettingsVersion') -eq '2') 'Saved file lacks SettingsVersion=2'

            & $click $downAuto 2032 $true
            Assert-True ((Get-IniString 'Overlay' 'DownloadColor') -eq 'auto') 'Automatic download color was not saved'
            'THEME_COLORS_OK'
        }
        'Anchors' {
            $main = Get-AppWindow $session $mainClass
            $overlay = Get-AppWindow $session 'WinNetMeterOverlay'
            $anchor = [WinNetMeterNative]::GetDlgItem($main.Handle, 2034)
            $offsetEdit = [WinNetMeterNative]::GetDlgItem($main.Handle, 2013)
            Assert-True ($anchor -ne [IntPtr]::Zero) 'Meter position control was not found'
            $taskbar = [WinNetMeterNative]::GetTaskbar().rc
            $dpi = [WinNetMeterNative]::GetDpiForWindow($overlay.Handle)
            $gap = [int][Math]::Floor((4 * $dpi + 48) / 96)
            # The meter renders on its own thread; wait for it to satisfy a placement.
            $waitPlaced = {
                param([scriptblock]$Placed)
                $deadline = [Environment]::TickCount + 3000
                do {
                    $rect = New-Object WinNetMeterNative+RECT
                    [void][WinNetMeterNative]::GetWindowRect($overlay.Handle, [ref]$rect)
                    if (& $Placed $rect) { return $rect }
                    Start-Sleep -Milliseconds 50
                } while ([Environment]::TickCount -lt $deadline)
                return $null
            }

            # Fresh settings default to "next to the tray".
            Assert-True ([int][WinNetMeterNative]::SendMessageW($anchor, 0x0147, [IntPtr]::Zero, [IntPtr]::Zero) -eq 0) 'Default position is not next to the tray'
            $tray = [WinNetMeterNative]::TaskbarPart('TrayNotifyWnd')
            if ($tray) {
                $placed = & $waitPlaced { param($r) $r.Right -eq ($tray.Left - $gap) }
                Assert-True ($null -ne $placed) "Meter is not $gap px left of the tray at x=$($tray.Left)"
            }

            # Changing the anchor resets the anchor-relative offset and is saved at once.
            [void][WinNetMeterNative]::SendMessageStringW($offsetEdit, 0x000C, [IntPtr]::Zero, '80')
            Assert-True (Wait-IniInt 'Overlay' 'TaskbarOffset' 80) 'Offset edit was not saved'
            Select-ComboItem $main.Handle $anchor 2034 1
            Assert-True ((Get-IniString 'Overlay' 'Anchor') -eq 'apps') 'After-apps position was not saved'
            Assert-True ((Get-IniString 'Overlay' 'TaskbarOffset') -eq '0') 'Changing the position did not reset the offset'
            $apps = [WinNetMeterNative]::TaskbarPart('ReBarWindow32/MSTaskSwWClass')
            if ($apps) {
                $placed = & $waitPlaced { param($r) $r.Left -eq ($apps.Right + $gap) }
                Assert-True ($null -ne $placed) "Meter is not $gap px right of the app buttons at x=$($apps.Right)"
            }

            Select-ComboItem $main.Handle $anchor 2034 2
            Assert-True ((Get-IniString 'Overlay' 'Anchor') -eq 'left') 'Left-edge position was not saved'
            $placed = & $waitPlaced { param($r) $r.Left -eq ($taskbar.Left + $gap) }
            Assert-True ($null -ne $placed) 'Meter is not at the left edge of the taskbar'
            'ANCHORS_OK'
        }
        'Embedded' {
            $main = Get-AppWindow $session $mainClass
            Assert-True (Wait-MeterMode $session $true) 'Embed=1 did not start as a child of the taskbar'
            $meter = Get-EmbeddedMeter $session
            $child = [uint64]0x40000000
            $layered = [uint64]0x80000
            Assert-True (($meter.Style -band $child) -ne 0) 'Embedded meter is not WS_CHILD'
            Assert-True (($meter.ExStyle -band $layered) -ne 0) 'Embedded meter is not layered'
            $box = [WinNetMeterNative]::GetDlgItem($main.Handle, 2035)
            Assert-True ([int][WinNetMeterNative]::SendMessageW($box, 0x00F0, [IntPtr]::Zero, [IntPtr]::Zero) -eq 1) 'Embed option is not checked'

            # Same size and anchor rules as the overlay, at the taskbar's DPI.
            $taskbar = [WinNetMeterNative]::GetTaskbar().rc
            $dpi = [WinNetMeterNative]::GetDpiForWindow([WinNetMeterNative]::TaskbarWindow())
            Assert-True ($dpi -ge 96) "Invalid taskbar DPI: $dpi"
            Assert-True ([WinNetMeterNative]::GetDpiForWindow($meter.Handle) -eq $dpi) 'Embedded meter DPI differs from the taskbar'
            $scale = { param([int]$value) [int][Math]::Floor(($value * $dpi + 48) / 96) }
            $padding = [Math]::Max(1, (& $scale 2))
            $expectedWidth = [Math]::Max(1, [Math]::Min((& $scale 132), $taskbar.Right - $taskbar.Left - 2 * $padding))
            $expectedHeight = [Math]::Max(1, [Math]::Min((& $scale 40), $taskbar.Bottom - $taskbar.Top - 2 * $padding))
            Assert-True (($meter.Rect.Right - $meter.Rect.Left) -eq $expectedWidth) 'Embedded meter width is not DPI-scaled'
            Assert-True (($meter.Rect.Bottom - $meter.Rect.Top) -eq $expectedHeight) 'Embedded meter height is not DPI-scaled'
            $contained = $meter.Rect.Left -ge $taskbar.Left -and $meter.Rect.Top -ge $taskbar.Top -and
                         $meter.Rect.Right -le $taskbar.Right -and $meter.Rect.Bottom -le $taskbar.Bottom
            Assert-True $contained 'Embedded meter is not on the taskbar'
            # The tray can still be resizing (this instance's own icon), so poll.
            if ([WinNetMeterNative]::TaskbarPart('TrayNotifyWnd')) {
                $gap = & $scale 4
                $deadline = [Environment]::TickCount + 3000
                do {
                    $tray = [WinNetMeterNative]::TaskbarPart('TrayNotifyWnd')
                    $placed = $tray -and (Get-EmbeddedMeter $session).Rect.Right -eq ($tray.Left - $gap)
                    if (-not $placed) { Start-Sleep -Milliseconds 50 }
                } while (-not $placed -and [Environment]::TickCount -lt $deadline)
                Assert-True $placed "Embedded meter is not $gap px left of the tray"
            }

            # Double-click still opens the app; Explorer's TaskbarCreated does not duplicate it.
            [void][WinNetMeterNative]::SendMessageW($meter.Handle, 0x0203, [IntPtr]::Zero, [IntPtr]::Zero)
            Assert-True (Wait-WindowVisible $session $mainClass) 'Embedded meter double-click did not open the window'
            $message = [WinNetMeterNative]::RegisterWindowMessageW('TaskbarCreated')
            1..3 | ForEach-Object { [void][WinNetMeterNative]::PostMessageW($main.Handle, $message, [IntPtr]::Zero, [IntPtr]::Zero) }
            Start-Sleep -Milliseconds 500
            Assert-True (Wait-MeterMode $session $true) 'TaskbarCreated duplicated or lost the embedded meter'

            # "Reset meter" restores looks and position, not the mode.
            [void][WinNetMeterNative]::SendMessageW($main.Handle, 0x0111, [IntPtr]2016, [IntPtr]::Zero)
            Assert-True (Wait-IniString 'Overlay' 'Anchor' 'tray') 'Meter reset was not saved'
            Assert-True ((Get-IniString 'Overlay' 'Embed') -eq '1') 'Meter reset turned embedding off'

            # Switching modes swaps the windows; each switch is saved.
            Set-EmbedMode $session $main.Handle $false
            Set-EmbedMode $session $main.Handle $true
            'EMBEDDED_OK'
        }
        'StartMenu' {
            # The known limitation and its fix, on the real shell: Start lifts the
            # taskbar into a z-order band above every application window, covering
            # the topmost overlay; the embedded meter is part of the taskbar.
            $main = Get-AppWindow $session $mainClass
            $minimumPixels = 20
            try {
                Assert-True (Wait-MeterMode $session $false) 'The overlay is not up'
                $overlayBaseline = Wait-MeterPixels $session $false 200
                Assert-True ($overlayBaseline -ge $minimumPixels) "Only $overlayBaseline meter pixels on screen before opening Start"
                if (-not (Open-StartMenu)) {
                    $reason = 'Start did not open (no interactive shell?)'
                    if ($env:GITHUB_ACTIONS -eq 'true') { Write-Host "::warning::StartMenu check skipped: $reason" }
                    "START_MENU_SKIPPED: $reason"
                } else {
                    $overlayOpen = Measure-MeterPixels $session $false
                    # When Start closes, the taskbar returns to its band a little after
                    # the foreground moves and lands on top of the overlay. The overlay's
                    # quick re-raises bring it back well before the next once-a-second
                    # refresh would (up to 1000 ms; ~150 ms with them, measured). Three
                    # closes at random refresh phases make a lucky pass unlikely. The
                    # first open above also warms Start up: a cold one can be slow.
                    Close-StartMenu
                    $recoveries = @()
                    for ($trial = 1; $trial -le 3; ++$trial) {
                        Start-Sleep -Milliseconds (Get-Random -Minimum 100 -Maximum 900)
                        Assert-True (Open-StartMenu) 'Start did not open again'
                        $recoveries += Close-StartMenuTimed $session ([int]($overlayBaseline / 2))
                    }
                    Assert-True (-not (Test-StartOpen)) 'Start did not close'
                    $slow = @($recoveries | Where-Object { $_ -lt 0 -or $_ -gt 450 })
                    Assert-True ($slow.Count -eq 0) "Overlay was slow to come back after Start closed: $($recoveries -join ', ') ms"

                    Set-EmbedMode $session $main.Handle $true
                    $embeddedBaseline = Wait-MeterPixels $session $true 200
                    Assert-True ($embeddedBaseline -ge $minimumPixels) "Only $embeddedBaseline embedded meter pixels on screen"
                    Assert-True (Open-StartMenu) 'Start did not open a second time'
                    $embeddedOpen = Wait-MeterPixels $session $true ([int]($embeddedBaseline / 2)) 1500
                    Close-StartMenu
                    Assert-True ($embeddedOpen -ge $embeddedBaseline / 2) "Embedded meter was covered while Start was open: $embeddedOpen of $embeddedBaseline pixels"
                    "START_MENU overlay: $overlayOpen of $overlayBaseline pixels visible with Start open, back $($recoveries -join '/') ms after it closed; embedded: $embeddedOpen of $embeddedBaseline visible with Start open"
                    'START_MENU_OK'
                }
            } finally {
                Close-StartMenu
            }
        }
    }
} finally {
    if ($null -ne $session) { Stop-TestApp $session }
    Remove-TestSettings
    if ($Check -eq 'Preferences') {
        Remove-ItemProperty -LiteralPath $startupKey -Name $startupValueName -ErrorAction SilentlyContinue
    }
}
