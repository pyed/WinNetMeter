# CI only: the Windows on Arm runner image starts with a fullscreen "Microsoft
# account" setup prompt in the foreground (WWAHost in Explorer's Shell_OOBEProxy).
# A fullscreen foreground window hides the overlay meter by design, so the
# behavioral checks need it gone. Refuses to run outside GitHub Actions: it
# closes windows that do not belong to WinNetMeter.
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
}
'@
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
