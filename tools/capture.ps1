# Capture real console screenshots of the assignment programs.
# Launches a console window, runs a WSL command inside it, then grabs the
# window's own bitmap with PrintWindow so overlapping windows cannot bleed in.
param(
    [Parameter(Mandatory = $true)][string]$Command,
    [Parameter(Mandatory = $true)][string]$OutFile,
    [string]$WorkDir = "",
    [Parameter(Mandatory = $true)][string]$Repo,
    [int]$Width = 140,
    [int]$Height = 42,
    [int]$WaitSeconds = 7
)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int n);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);
}
"@ -ErrorAction SilentlyContinue

$repo = $Repo
$target = if ($WorkDir) { "$repo/$WorkDir" } else { $repo }

# Write the bash command to a script file rather than nesting it inside
# `bash -lc "..."`; embedded quotes would otherwise break the wrapper.
$tmp = Join-Path $env:TEMP ("cap_" + [Guid]::NewGuid().ToString("N").Substring(0, 8) + ".sh")
$body = "cd $target`n$Command`n"
[IO.File]::WriteAllText($tmp, $body.Replace("`r`n", "`n"), (New-Object Text.UTF8Encoding($false)))
$wslScript = "/mnt/" + $tmp.Substring(0, 1).ToLower() + $tmp.Substring(2).Replace("\", "/")

$title = "MSCS632-CAPTURE-" + [Guid]::NewGuid().ToString("N").Substring(0, 8)
$ps = @"
`$Host.UI.RawUI.WindowTitle = '$title'
`$Host.UI.RawUI.BufferSize = New-Object Management.Automation.Host.Size($Width, 3000)
`$Host.UI.RawUI.WindowSize  = New-Object Management.Automation.Host.Size($Width, $Height)
Clear-Host
wsl.exe -d Ubuntu -e bash '$wslScript'
"@

$enc = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($ps))
$p = Start-Process conhost.exe -ArgumentList 'powershell.exe', '-NoLogo', '-NoExit', '-EncodedCommand', $enc -PassThru
Start-Sleep -Seconds $WaitSeconds

# The console window belongs to the hosted powershell child, not to conhost.
$h = [IntPtr]::Zero
$owner = $null
for ($i = 0; $i -lt 25; $i++) {
    $owner = Get-Process | Where-Object { $_.MainWindowTitle -eq $title } | Select-Object -First 1
    if ($owner -and $owner.MainWindowHandle -ne [IntPtr]::Zero) { $h = $owner.MainWindowHandle; break }
    Start-Sleep -Milliseconds 400
}
if ($h -eq [IntPtr]::Zero) {
    $p | Stop-Process -Force -ErrorAction SilentlyContinue
    throw "no window handle for $OutFile"
}

[Win]::ShowWindow($h, 9) | Out-Null      # SW_RESTORE
[Win]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 1200

$r = New-Object Win+RECT
[Win]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.Right - $r.Left
$hh = $r.Bottom - $r.Top
if ($w -le 0 -or $hh -le 0) {
    $p | Stop-Process -Force -ErrorAction SilentlyContinue
    throw "bad rect for $OutFile"
}

$bmp = New-Object System.Drawing.Bitmap($w, $hh)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
# flag 2 = PW_RENDERFULLCONTENT, required for console windows
$ok = [Win]::PrintWindow($h, $hdc, 2)
$g.ReleaseHdc($hdc)
if (-not $ok) { $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size) }

$dir = Split-Path -Parent $OutFile
if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
$bmp.Save($OutFile, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()

$p | Stop-Process -Force -ErrorAction SilentlyContinue
if ($owner) { $owner | Stop-Process -Force -ErrorAction SilentlyContinue }
Remove-Item $tmp -ErrorAction SilentlyContinue

Write-Output ("saved {0}  ({1}x{2})  printwindow={3}" -f $OutFile, $w, $hh, $ok)
