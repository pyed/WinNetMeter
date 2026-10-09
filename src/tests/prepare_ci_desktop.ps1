# CI only: the Windows on Arm runner image starts with a fullscreen "Microsoft
# account" setup prompt in the foreground (WWAHost in Explorer's Shell_OOBEProxy),
# and once that is gone, with Search open. A fullscreen foreground window hides
# the overlay meter by design, and while Start or Search is open the taskbar sits
# in a higher z-order band that covers overlays and refuses new child windows,
# so the behavioral checks need both closed. Refuses to run outside GitHub
# Actions: it closes windows that do not belong to WinNetMeter.
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'prepare_ci_desktop.ps1 only runs on CI runners' }
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class CiDesktop
{
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindowExW(IntPtr parent, IntPtr after, string cls, string title);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr hwnd, int command);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
}
'@
# Start or Search: their window is in the foreground, owned by one of these.
function Test-ShellFlyoutOpen {
    $ownerId = [uint32]0
    [void][CiDesktop]::GetWindowThreadProcessId([CiDesktop]::GetForegroundWindow(), [ref]$ownerId)
    $owner = Get-Process -Id $ownerId -ErrorAction SilentlyContinue
    $null -ne $owner -and $owner.ProcessName -in @('StartMenuExperienceHost', 'SearchHost', 'SearchApp')
}
$prompts = @(Get-Process -Name WWAHost -ErrorAction SilentlyContinue)
$prompts | Stop-Process -Force
"Stopped $($prompts.Count) WWAHost process(es)"
for ($proxy = [CiDesktop]::FindWindowExW([IntPtr]::Zero, [IntPtr]::Zero, 'Shell_OOBEProxy', $null); $proxy -ne [IntPtr]::Zero;
     $proxy = [CiDesktop]::FindWindowExW([IntPtr]::Zero, $proxy, 'Shell_OOBEProxy', $null)) {
    if ([CiDesktop]::IsWindowVisible($proxy)) {
        [void][CiDesktop]::ShowWindowAsync($proxy, 0)   # SW_HIDE
        'Hid a Shell_OOBEProxy window'
    }
}
Start-Sleep -Seconds 2
for ($attempt = 0; $attempt -lt 3 -and (Test-ShellFlyoutOpen); ++$attempt) {
    [CiDesktop]::keybd_event(0x1B, 0, 0, [UIntPtr]::Zero)   # Escape, down and up
    [CiDesktop]::keybd_event(0x1B, 0, 2, [UIntPtr]::Zero)
    'Pressed Escape to close Start or Search'
    Start-Sleep -Seconds 1
}
