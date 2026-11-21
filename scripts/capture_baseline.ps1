#Requires -Version 5.1
<#
.SYNOPSIS
  Task 0.5 baseline capture helper (scripts/capture_baseline.ps1).

.DESCRIPTION
  Locates a top-level window by title and captures it to PNG using
  PrintWindow (PW_RENDERFULLCONTENT) with a CopyFromScreen foreground
  fallback. The Mini badge (lwf-mini, transparent framebuffer) is captured
  with the CopyFromScreen path preferred. Resizing is done through
  MoveWindow with an outer-size compensation computed from GetClientRect so
  the requested client size is what ends up on screen.

  This script is a helper library; it does not launch the app, write
  configs, or own the capture matrix. Dot-source it to use the functions,
  or call it directly with -Title/-OutputPath for a single capture.

.EXAMPLE
  . .\scripts\capture_baseline.ps1
  $w = Wait-ForTopWindow -Title "Local Wisper Flow" -TimeoutSeconds 60
  Set-WindowClientSize -Hwnd $w.Hwnd -ClientWidth 960 -ClientHeight 640
  Capture-WindowToFile -Hwnd $w.Hwnd -Path "shot.png" -Method Auto

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\capture_baseline.ps1 -Title "lwf-mini" -OutputPath out\mini.png -PreferScreen
#>
[CmdletBinding()]
param(
    [string]$Title = "Local Wisper Flow",
    [string]$OutputPath = "",
    [int]$ClientWidth = 0,
    [int]$ClientHeight = 0,
    [switch]$PreferScreen,
    [int]$WaitSeconds = 60,
    [switch]$OpenSettingsBeforeWait,
    [switch]$ListWindows
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# DPI awareness (future 150%/200% runs). At 100% this is a no-op. Failures are
# expected when the host process is already DPI-aware, so they are ignored.
# ---------------------------------------------------------------------------
try {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class LwfDpi {
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool SetProcessDpiAwarenessContext(IntPtr dpiContext);
    [DllImport("shcore.dll", SetLastError=true)]
    public static extern int SetProcessDpiAwareness(int value);
    public static void TrySetAwareness() {
        IntPtr perMonitorV2 = new IntPtr(-4);
        try { if (SetProcessDpiAwarenessContext(perMonitorV2)) return; } catch { }
        try { SetProcessDpiAwareness(2); } catch { }
    }
    [DllImport("user32.dll", SetLastError=true)]
    public static extern uint GetDpiForSystem();
}
"@
} catch { }
try { [LwfDpi]::TrySetAwareness() } catch { }

# LwfWin32Capture is compiled once per PowerShell session; a second
# dot-source in the same host would otherwise throw "type already defined".
if (-not ([System.Management.Automation.PSTypeName]'LwfWin32Capture').Type) {
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

public static class LwfWin32Capture {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X; public int Y; }

    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int cx, int cy, bool bRepaint);
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, int nFlags);
    [DllImport("user32.dll")]
    public static extern IntPtr GetDesktopWindow();
    [DllImport("user32.dll")]
    public static extern bool BringWindowToTop(IntPtr hWnd);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    public static string WindowTitle(IntPtr hWnd) {
        StringBuilder sb = new StringBuilder(512);
        GetWindowText(hWnd, sb, sb.Capacity);
        return sb.ToString();
    }
    public static bool TryGetOuterRect(IntPtr hWnd, out RECT rect) {
        return GetWindowRect(hWnd, out rect);
    }
    public static void SavePrintWindow(IntPtr hWnd, string path) {
        RECT r;
        if (!GetWindowRect(hWnd, out r)) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowRect failed.");
        int w = r.Right - r.Left;
        int h = r.Bottom - r.Top;
        if (w <= 0 || h <= 0) throw new InvalidOperationException("Window rect is empty (w=" + w + " h=" + h + "). Is the window visible?");
        string dir = Path.GetDirectoryName(Path.GetFullPath(path));
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        using (Bitmap bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb))
        using (Graphics g = Graphics.FromImage(bmp)) {
            IntPtr hdc = g.GetHdc();
            try {
                const int PW_RENDERFULLCONTENT = 2;
                bool ok = PrintWindow(hWnd, hdc, PW_RENDERFULLCONTENT);
                if (!ok) throw new Win32Exception(Marshal.GetLastWin32Error(), "PrintWindow returned false.");
            } finally { g.ReleaseHdc(hdc); }
            bmp.Save(path, ImageFormat.Png);
        }
    }
    public static void SaveScreenCrop(IntPtr hWnd, string path) {
        RECT r;
        if (!GetWindowRect(hWnd, out r)) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowRect failed.");
        int w = r.Right - r.Left;
        int h = r.Bottom - r.Top;
        if (w <= 0 || h <= 0) throw new InvalidOperationException("Window rect is empty (w=" + w + " h=" + h + "). Is the window visible?");
        string dir = Path.GetDirectoryName(Path.GetFullPath(path));
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        try { ShowWindow(hWnd, 9); SetForegroundWindow(hWnd); BringWindowToTop(hWnd); } catch { }
        System.Threading.Thread.Sleep(400);
        // Re-read after foregrounding: position does not change, but size is
        // re-validated so a minimized/zero-size window errors clearly.
        if (!GetWindowRect(hWnd, out r)) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetWindowRect (recheck) failed.");
        w = r.Right - r.Left; h = r.Bottom - r.Top;
        if (w <= 0 || h <= 0) throw new InvalidOperationException("Window rect empty after foregrounding.");
        using (Bitmap bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb))
        using (Graphics g = Graphics.FromImage(bmp)) {
            g.CopyFromScreen(new Point(r.Left, r.Top), new Point(0, 0), new Size(w, h));
            bmp.Save(path, ImageFormat.Png);
        }
    }
}
"@
}

function Get-TopWindowByTitle {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$Title)
    $found = New-Object System.Collections.Generic.List[object]
    $cb = {
        param([IntPtr]$hWnd, [IntPtr]$lParam)
        try {
            $t = [LwfWin32Capture]::WindowTitle($hWnd)
            if ($t -eq $Title) {
                $rect = New-Object LwfWin32Capture+RECT
                $ok = [LwfWin32Capture]::GetWindowRect($hWnd, [ref]$rect)
                $visible = [LwfWin32Capture]::IsWindowVisible($hWnd)
                $w = if ($ok) { $rect.Right - $rect.Left } else { 0 }
                $h = if ($ok) { $rect.Bottom - $rect.Top } else { 0 }
                $found.Add([pscustomobject]@{
                    Hwnd    = $hWnd
                    Title   = $t
                    Visible = [bool]$visible
                    Left    = if ($ok) { $rect.Left } else { 0 }
                    Top     = if ($ok) { $rect.Top } else { 0 }
                    Width   = $w
                    Height  = $h
                })
            }
        } catch { }
        return $true
    }
    $delegate = [LwfWin32Capture+EnumWindowsProc]$cb
    [LwfWin32Capture]::EnumWindows($delegate, [IntPtr]::Zero) | Out-Null
    # Prefer visible windows, largest area first.
    return @($found | Sort-Object -Property @{Expression = { !$_.Visible }}, @{Expression = { -$_.Width * $_.Height }})
}

function Wait-ForTopWindow {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$Title,
        [int]$TimeoutSeconds = 60,
        [int]$PollMs = 500,
        [switch]$RequireVisible
    )
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ($true) {
        $list = Get-TopWindowByTitle -Title $Title
        foreach ($w in $list) {
            if ($RequireVisible -and -not $w.Visible) { continue }
            if (-not $RequireVisible) { return $w }
            if ($w.Visible -and $w.Width -gt 0 -and $w.Height -gt 0) { return $w }
        }
        if ((Get-Date) -ge $deadline) { return $null }
        Start-Sleep -Milliseconds $PollMs
    }
}

function Set-WindowClientSize {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][IntPtr]$Hwnd,
        [Parameter(Mandatory = $true)][int]$ClientWidth,
        [Parameter(Mandatory = $true)][int]$ClientHeight
    )
    if ($ClientWidth -le 0 -or $ClientHeight -le 0) { throw "Client size must be positive (got ${ClientWidth}x${ClientHeight})." }
    $outer = New-Object LwfWin32Capture+RECT
    $client = New-Object LwfWin32Capture+RECT
    if (-not [LwfWin32Capture]::GetWindowRect($Hwnd, [ref]$outer)) {
        throw "GetWindowRect failed for HWND $Hwnd (win32 err $([System.Runtime.InteropServices.Marshal]::GetLastWin32Error()))."
    }
    if (-not [LwfWin32Capture]::GetClientRect($Hwnd, [ref]$client)) {
        throw "GetClientRect failed for HWND $Hwnd (win32 err $([System.Runtime.InteropServices.Marshal]::GetLastWin32Error()))."
    }
    $outerW = $outer.Right - $outer.Left
    $outerH = $outer.Bottom - $outer.Top
    $clientW = $client.Right - $client.Left
    $clientH = $client.Bottom - $client.Top
    $deltaW = $outerW - $clientW
    $deltaH = $outerH - $clientH
    $newOuterW = $ClientWidth + $deltaW
    $newOuterH = $ClientHeight + $deltaH
    $ok = [LwfWin32Capture]::MoveWindow($Hwnd, $outer.Left, $outer.Top, $newOuterW, $newOuterH, $true)
    if (-not $ok) {
        throw "MoveWindow failed for HWND $Hwnd (win32 err $([System.Runtime.InteropServices.Marshal]::GetLastWin32Error()))."
    }
    # GLFW enforces size limits (760x520 min); the request may be clipped.
    # Re-read so callers can log the actual size.
    $after = New-Object LwfWin32Capture+RECT
    [LwfWin32Capture]::GetWindowRect($Hwnd, [ref]$after) | Out-Null
    $afterClient = New-Object LwfWin32Capture+RECT
    [LwfWin32Capture]::GetClientRect($Hwnd, [ref]$afterClient) | Out-Null
    Start-Sleep -Milliseconds 300
    return [pscustomobject]@{
        Hwnd       = $Hwnd
        RequestedClientWidth  = $ClientWidth
        RequestedClientHeight = $ClientHeight
        ActualClientWidth     = $afterClient.Right - $afterClient.Left
        ActualClientHeight    = $afterClient.Bottom - $afterClient.Top
        OuterLeft  = $after.Left
        OuterTop   = $after.Top
        OuterWidth = $after.Right - $after.Left
        OuterHeight = $after.Bottom - $after.Top
    }
}

function Capture-WindowToFile {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][IntPtr]$Hwnd,
        [Parameter(Mandatory = $true)][string]$Path,
        [ValidateSet("Auto", "PrintWindow", "Screen")][string]$Method = "Auto"
    )
    $order = @()
    if ($Method -eq "PrintWindow") { $order = @("PrintWindow") }
    elseif ($Method -eq "Screen") { $order = @("Screen") }
    else { $order = @("PrintWindow", "Screen") }
    $lastErr = $null
    foreach ($m in $order) {
        try {
            if ($m -eq "PrintWindow") { [LwfWin32Capture]::SavePrintWindow($Hwnd, $Path) }
            else { [LwfWin32Capture]::SaveScreenCrop($Hwnd, $Path) }
            if (-not (Test-Path -LiteralPath $Path)) { throw "Capture file was not created via $m." }
            $len = (Get-Item -LiteralPath $Path).Length
            if ($len -eq 0) { throw "Capture file is empty via $m." }
            $w = Get-TopWindowByTitle -Title ([LwfWin32Capture]::WindowTitle($Hwnd)) | Select-Object -First 1
            return [pscustomobject]@{
                Path   = (Resolve-Path -LiteralPath $Path).Path
                Method = $m
                Bytes  = $len
                Width  = if ($null -ne $w) { $w.Width } else { 0 }
                Height = if ($null -ne $w) { $w.Height } else { 0 }
            }
        } catch { $lastErr = $_ }
    }
    throw "Capture failed for HWND $Hwnd; last error: $($lastErr.Exception.Message)"
}

function Capture-WindowAuto {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][IntPtr]$Hwnd,
        [Parameter(Mandatory = $true)][string]$Path,
        [switch]$PreferScreen
    )
    $method = if ($PreferScreen) { "Screen" } else { "Auto" }
    if ($method -eq "Screen") {
        # Try Screen first, fall back to PrintWindow.
        try { return Capture-WindowToFile -Hwnd $Hwnd -Path $Path -Method Screen }
        catch { return Capture-WindowToFile -Hwnd $Hwnd -Path $Path -Method PrintWindow }
    }
    return Capture-WindowToFile -Hwnd $Hwnd -Path $Path -Method Auto
}

function Request-OpenSettings {
    <#
    .SYNOPSIS
      Shows the Full window the way the tray does (WM_APP+8 to LwfTrayWindow).
    .DESCRIPTION
      The second-instance named event LwfOpenSettingsEvent / SingleInstance::SignalOpenSettings
      has no receiver on this HEAD, so launching the exe again does not show the window. The
      tray consumer (WindowsTray WindowProc, WM_APP+8 -> UiBus OpenSettings -> MainWindow
      SetVisible(true)) is the working pipe. Posting that message to the LwfTrayWindow
      message-only window is scriptable and drives the app's own show path.
    #>
    [CmdletBinding()]
    param(
        [string]$TrayClass = "LwfTrayWindow",
        [uint32]$Message = 0x8008
    )
    $hwnd = [LwfWin32Capture]::FindWindow($TrayClass, $null)
    if ($hwnd -eq [IntPtr]::Zero -or $hwnd -eq $null) {
        throw "Tray window class '$TrayClass' not found. Is the app running?"
    }
    $ok = [LwfWin32Capture]::PostMessage($hwnd, $Message, [IntPtr]::Zero, [IntPtr]::Zero)
    return [pscustomobject]@{ Hwnd = $hwnd; Posted = [bool]$ok }
}

function Get-SystemDpi {
    [CmdletBinding()]
    param()
    try {
        $dpi = [LwfDpi]::GetDpiForSystem()
        if ($dpi -gt 0) { return [int]$dpi }
    } catch { }
    try {
        $g = [System.Drawing.Graphics]::FromHwnd([IntPtr]::Zero)
        try { return [int]$g.DpiX } finally { $g.Dispose() }
    } catch { return 96 }
}

# ---------------------------------------------------------------------------
# Direct-invocation entry point (dot-sourcing skips this block).
# ---------------------------------------------------------------------------
if ($MyInvocation.InvocationName -ne '.') {
    if ($ListWindows) {
        foreach ($t in @("Local Wisper Flow", "lwf-mini")) {
            Get-TopWindowByTitle -Title $t | Format-Table -AutoSize | Out-String | Write-Output
        }
        exit 0
    }
    if ([string]::IsNullOrWhiteSpace($OutputPath)) {
        Get-Help $MyInvocation.MyCommand.Path | Out-String | Write-Output
        exit 0
    }
    $win = Wait-ForTopWindow -Title $Title -TimeoutSeconds $WaitSeconds -RequireVisible
    if ($null -eq $win -and $OpenSettingsBeforeWait) {
        Request-OpenSettings | Out-Null
        $win = Wait-ForTopWindow -Title $Title -TimeoutSeconds $WaitSeconds -RequireVisible
    }
    if ($null -eq $win) {
        Write-Error "Window '$Title' did not become visible within ${WaitSeconds}s."
        exit 1
    }
    if ($ClientWidth -gt 0 -and $ClientHeight -gt 0) {
        $info = Set-WindowClientSize -Hwnd $win.Hwnd -ClientWidth $ClientWidth -ClientHeight $ClientHeight
        Write-Output ("RESIZE hwnd={0} requested={1}x{2} actualClient={3}x{4} outer={5}+{6},{7}x{8}" -f `
            $info.Hwnd.ToInt64(), $info.RequestedClientWidth, $info.RequestedClientHeight, `
            $info.ActualClientWidth, $info.ActualClientHeight, `
            $info.OuterLeft, $info.OuterTop, $info.OuterWidth, $info.OuterHeight)
    }
    $result = Capture-WindowAuto -Hwnd $win.Hwnd -Path $OutputPath -PreferScreen:$PreferScreen
    Write-Output ("CAPTURE_OK path={0} method={1} bytes={2} rect={3}x{4}" -f $result.Path, $result.Method, $result.Bytes, $result.Width, $result.Height)
    exit 0
}
