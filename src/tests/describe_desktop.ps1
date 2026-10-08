# Prints the state of the desktop the behavioral checks run on (screen, taskbar,
# tray parts, foreground and topmost windows), so a failing check on a CI runner
# can be told apart from an unusual runner. Changes nothing.
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class DesktopProbe
{
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)]
    public struct APPBARDATA { public uint cbSize; public IntPtr hWnd; public uint uCallbackMessage; public uint uEdge; public RECT rc; public IntPtr lParam; }
    public delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
    [DllImport("shell32.dll")] public static extern UIntPtr SHAppBarMessage(uint message, ref APPBARDATA data);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr GetWindowLongPtrW(IntPtr hwnd, int index);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] public static extern IntPtr GetTopWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr hwnd, uint command);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr hwnd, StringBuilder name, int max);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowExW(IntPtr parent, IntPtr after, string cls, string title);

    public static string Describe(IntPtr hwnd)
    {
        if (hwnd == IntPtr.Zero) return "(none)";
        var cls = new StringBuilder(256); GetClassNameW(hwnd, cls, cls.Capacity);
        var title = new StringBuilder(256); GetWindowTextW(hwnd, title, title.Capacity);
        RECT r; GetWindowRect(hwnd, out r);
        uint pid; GetWindowThreadProcessId(hwnd, out pid);
        string process = "?";
        try { process = System.Diagnostics.Process.GetProcessById((int)pid).ProcessName; } catch { }
        long style = GetWindowLongPtrW(hwnd, -16).ToInt64(), ex = GetWindowLongPtrW(hwnd, -20).ToInt64();
        return string.Format("{0} '{1}' [{2},{3},{4},{5}] visible={6} style=0x{7:X} ex=0x{8:X} ({9})",
            cls, title, r.Left, r.Top, r.Right, r.Bottom, IsWindowVisible(hwnd), style, ex, process);
    }

    public static IntPtr Child(IntPtr parent, string path)
    {
        IntPtr window = parent;
        foreach (var cls in path.Split('/')) {
            if (window == IntPtr.Zero) return IntPtr.Zero;
            window = FindWindowExW(window, IntPtr.Zero, cls, null);
        }
        return window;
    }

    public static List<string> TopWindows(int count)
    {
        var result = new List<string>();
        for (IntPtr w = GetTopWindow(IntPtr.Zero); w != IntPtr.Zero && result.Count < count; w = GetWindow(w, 2)) {
            if (IsWindowVisible(w)) result.Add(Describe(w));
        }
        return result;
    }
}
'@
[void][DesktopProbe]::SetThreadDpiAwarenessContext([IntPtr](-4))

"OS {0}, {1} (process {2})" -f [Environment]::OSVersion.VersionString,
    [Runtime.InteropServices.RuntimeInformation]::OSArchitecture, [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture
"Screen {0}x{1}, {2} monitor(s)" -f [DesktopProbe]::GetSystemMetrics(0), [DesktopProbe]::GetSystemMetrics(1), [DesktopProbe]::GetSystemMetrics(80)
$data = New-Object DesktopProbe+APPBARDATA
$data.cbSize = [Runtime.InteropServices.Marshal]::SizeOf($data)
if ([DesktopProbe]::SHAppBarMessage(5, [ref]$data) -ne [UIntPtr]::Zero) {
    "Taskbar (ABM_GETTASKBARPOS) [{0},{1},{2},{3}] edge {4}" -f $data.rc.Left, $data.rc.Top, $data.rc.Right, $data.rc.Bottom, $data.uEdge
} else { 'Taskbar: ABM_GETTASKBARPOS failed' }
$state = New-Object DesktopProbe+APPBARDATA
$state.cbSize = $data.cbSize
"Taskbar state (ABM_GETSTATE): 0x{0:X} (1 = auto-hide, 2 = always on top)" -f [DesktopProbe]::SHAppBarMessage(4, [ref]$state).ToUInt64()
$taskbar = [DesktopProbe]::FindWindowExW([IntPtr]::Zero, [IntPtr]::Zero, 'Shell_TrayWnd', $null)
"Shell_TrayWnd: " + [DesktopProbe]::Describe($taskbar) + " dpi " + [DesktopProbe]::GetDpiForWindow($taskbar)
foreach ($part in 'TrayNotifyWnd', 'ReBarWindow32/MSTaskSwWClass', 'Windows.UI.Composition.DesktopWindowContentBridge') {
    "  ${part}: " + [DesktopProbe]::Describe([DesktopProbe]::Child($taskbar, $part))
}
"Foreground: " + [DesktopProbe]::Describe([DesktopProbe]::GetForegroundWindow())
'Visible top-level windows, top of the z-order first:'
[DesktopProbe]::TopWindows(12) | ForEach-Object { "  $_" }
