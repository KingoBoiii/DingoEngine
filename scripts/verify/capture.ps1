<#
.SYNOPSIS
  Runs DingoEngine programs and captures their frames and logs, for baselines, 0 px gates and timings.

.DESCRIPTION
  Each job launches a program from its working directory, waits, optionally captures the window's
  client area (PrintWindow, PW_CLIENTONLY | PW_RENDERFULLCONTENT, into a plain GDI DIB, because a
  GDI+ bitmap's GetHdc() keys RGB(13,11,12) out), closes it with WM_CLOSE so teardown runs and
  Dingo.log flushes, and keeps its stdout as <name>.log.

  By default every job runs on a private Win32 desktop: no window reaches the user's screen or takes
  focus. -Visible runs them on the current desktop instead; Vulkan's present timing differs between
  the two, so compare timings only within one.

  A job waits either a fixed time after its window appears (wait) or for a stdout line matching a
  regex (until, e.g. "\[PERF\]"), then captures (shot) and closes. A program that exits by itself
  ends its job. The working directory's imgui.ini is put back after every job.

  Jobs come from a JSON file (-Jobs), an array of objects with these fields:
      name     output basename (required)
      exe      the executable, relative to the repo root or absolute (required)
      cwd      working directory, relative to the repo root or absolute (default: the exe's folder)
      args     command-line arguments
      wait     seconds to wait after the window appears (default 8)
      until    a regex to wait for in stdout instead of a fixed wait
      shot     capture the client area (default true)
      timeout  seconds before the job gives up waiting (default 120)
      env      environment variables for the program, as an object of name: value
  or, for one job, from -Exe/-Cwd/-ProgramArgs/-Name/-Wait/-Until/-NoShot.

  Results go to -OutDir: <name>.png, <name>.log, <name>.stderr.log and results.csv (name, exit code,
  client size, seconds, whether until matched, error-line count).

.EXAMPLE
  powershell -File scripts\verify\capture.ps1 -Exe build\bin\Release-windows-x86_64\Dingo-TestFramework\Dingo-TestFramework.exe -Cwd test -ProgramArgs "--test=batch" -Name batch -OutDir build\baseline-v0.9.0\shots
.EXAMPLE
  powershell -File scripts\verify\capture.ps1 -Jobs build\baseline-v0.9.0\jobs.json -OutDir build\baseline-v0.9.0\vk
#>
[CmdletBinding()]
param(
    [string]$Jobs = "",
    [string]$OutDir = "",
    [string]$Exe = "",
    [string]$Cwd = "",
    [string]$ProgramArgs = "",
    [string]$Name = "capture",
    [double]$Wait = 8,
    [string]$Until = "",
    [switch]$NoShot,
    [int]$Timeout = 120,
    [switch]$Visible,
    [switch]$Inner
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Resolve-Root([string]$path) {
    if ([string]::IsNullOrEmpty($path)) { return $path }
    if ([System.IO.Path]::IsPathRooted($path)) { return [System.IO.Path]::GetFullPath($path) }
    return [System.IO.Path]::GetFullPath((Join-Path $Root $path))
}

if (-not $OutDir) { $OutDir = Join-Path $Root "build\capture" }
$OutDir = Resolve-Root $OutDir
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if (-not $Jobs) {
    if (-not $Exe) { throw "Pass -Jobs <file.json> or -Exe <program>" }
    $single = @([ordered]@{ name = $Name; exe = $Exe; cwd = $Cwd; args = $ProgramArgs; wait = $Wait; until = $Until; shot = (-not $NoShot); timeout = $Timeout })
    $Jobs = Join-Path $OutDir "$Name.job.json"
    ConvertTo-Json -InputObject $single -Depth 3 | Set-Content -Encoding utf8 $Jobs
}
$Jobs = Resolve-Root $Jobs

if (-not $Visible -and -not $Inner) {
    Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class DingoDesktop {
  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
  struct STARTUPINFO {
    public int cb; public string lpReserved; public string lpDesktop; public string lpTitle;
    public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    public short wShowWindow, cbReserved2; public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError;
  }
  [StructLayout(LayoutKind.Sequential)]
  struct PROCESS_INFORMATION { public IntPtr hProcess, hThread; public int dwProcessId, dwThreadId; }
  [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  static extern IntPtr CreateDesktopW(string name, IntPtr device, IntPtr devmode, uint flags, uint access, IntPtr sa);
  [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr desktop);
  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  static extern bool CreateProcessW(string app, StringBuilder cmd, IntPtr pa, IntPtr ta, bool inherit, uint flags,
                                    IntPtr env, string cwd, ref STARTUPINFO si, out PROCESS_INFORMATION pi);
  [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr h, uint ms);
  [DllImport("kernel32.dll")] static extern bool GetExitCodeProcess(IntPtr h, out uint code);
  [DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr h, uint code);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);

  public static int Run(string desktopName, string commandLine, string cwd, uint timeoutMs) {
    IntPtr desk = CreateDesktopW(desktopName, IntPtr.Zero, IntPtr.Zero, 0, 0x10000000 /*GENERIC_ALL*/, IntPtr.Zero);
    if (desk == IntPtr.Zero) throw new Exception("CreateDesktop failed: " + Marshal.GetLastWin32Error());
    try {
      var si = new STARTUPINFO(); si.cb = Marshal.SizeOf(si); si.lpDesktop = "WinSta0\\" + desktopName;
      PROCESS_INFORMATION pi;
      if (!CreateProcessW(null, new StringBuilder(commandLine), IntPtr.Zero, IntPtr.Zero, false, 0x08000000 /*CREATE_NO_WINDOW*/,
                          IntPtr.Zero, cwd, ref si, out pi))
        throw new Exception("CreateProcess failed: " + Marshal.GetLastWin32Error());
      uint code = 1;
      if (WaitForSingleObject(pi.hProcess, timeoutMs) != 0) TerminateProcess(pi.hProcess, 124);
      GetExitCodeProcess(pi.hProcess, out code);
      CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
      return (int)code;
    } finally { CloseDesktop(desk); }
  }
}
"@
    $jobList = @(Get-Content -Raw $Jobs | ConvertFrom-Json | ForEach-Object { $_ })
    $budget = 120
    foreach ($j in $jobList) { $budget += $(if ($j.timeout) { [int]$j.timeout } else { 120 }) + 30 }
    $runLog = Join-Path $OutDir "capture-run.log"
    $cmd = "cmd.exe /c powershell -NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Inner -Jobs `"$Jobs`" -OutDir `"$OutDir`" > `"$runLog`" 2>&1"
    Write-Host "[capture] $($jobList.Count) job(s) on a private desktop -> $OutDir"
    $code = [DingoDesktop]::Run("DingoCapture", $cmd, $Root, [uint32]($budget * 1000))
    if (Test-Path $runLog) { Get-Content $runLog }
    exit $code
}

Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Text;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public struct DingoRect { public int Left, Top, Right, Bottom; }
public static class DingoWin {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern int GetClassName(IntPtr h, StringBuilder s, int max);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out DingoRect r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);

  public static IntPtr FindGlfw(uint pid) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, l) => {
      uint p; GetWindowThreadProcessId(h, out p);
      if (p != pid || !IsWindowVisible(h)) return true;
      var sb = new StringBuilder(64); GetClassName(h, sb, 64);
      if (sb.ToString() == "GLFW30") { found = h; return false; }
      return true;
    }, IntPtr.Zero);
    return found;
  }
}
public sealed class DingoDib : IDisposable {
  [StructLayout(LayoutKind.Sequential)]
  struct BITMAPINFOHEADER {
    public int biSize, biWidth, biHeight; public short biPlanes, biBitCount;
    public int biCompression, biSizeImage, biXPelsPerMeter, biYPelsPerMeter, biClrUsed, biClrImportant;
  }
  [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("gdi32.dll")] static extern IntPtr CreateCompatibleDC(IntPtr dc);
  [DllImport("gdi32.dll")] static extern bool DeleteDC(IntPtr dc);
  [DllImport("gdi32.dll")] static extern IntPtr CreateDIBSection(IntPtr dc, ref BITMAPINFOHEADER bmi, uint usage, out IntPtr bits, IntPtr section, uint offset);
  [DllImport("gdi32.dll")] static extern IntPtr SelectObject(IntPtr dc, IntPtr obj);
  [DllImport("gdi32.dll")] static extern bool DeleteObject(IntPtr obj);

  public readonly int Width, Height;
  public readonly IntPtr Bits;
  readonly IntPtr dc, dib, old;

  public DingoDib(int w, int h) {
    Width = w; Height = h;
    var bmi = new BITMAPINFOHEADER { biSize = 40, biWidth = w, biHeight = -h, biPlanes = 1, biBitCount = 32 };
    dc = CreateCompatibleDC(IntPtr.Zero);
    dib = CreateDIBSection(dc, ref bmi, 0, out Bits, IntPtr.Zero, 0);
    old = SelectObject(dc, dib);
  }
  public bool Capture(IntPtr hwnd) { return PrintWindow(hwnd, dc, 3); }
  public bool IsBlank() {
    int first = Marshal.ReadInt32(Bits);
    for (int i = 1; i <= 40; i++) {
      int x = (int)((long)Width * i / 41), y = (int)((long)Height * (((i * 7) % 41) + 0.5) / 41);
      if ((Marshal.ReadInt32(Bits + (y * Width + x) * 4) & 0xFFFFFF) != (first & 0xFFFFFF)) return false;
    }
    return true;
  }
  public void Save(string path) {
    var bmp = new Bitmap(Width, Height, PixelFormat.Format24bppRgb);
    var data = bmp.LockBits(new Rectangle(0, 0, Width, Height), ImageLockMode.WriteOnly, PixelFormat.Format24bppRgb);
    var src = new byte[Width * 4];
    var dst = new byte[data.Stride];
    for (int y = 0; y < Height; y++) {
      Marshal.Copy(Bits + y * Width * 4, src, 0, src.Length);
      for (int x = 0; x < Width; x++) { dst[x * 3] = src[x * 4]; dst[x * 3 + 1] = src[x * 4 + 1]; dst[x * 3 + 2] = src[x * 4 + 2]; }
      Marshal.Copy(dst, 0, data.Scan0 + y * data.Stride, data.Stride);
    }
    bmp.UnlockBits(data);
    bmp.Save(path, ImageFormat.Png);
    bmp.Dispose();
  }
  public void Dispose() { SelectObject(dc, old); DeleteObject(dib); DeleteDC(dc); }
}
"@

$WM_CLOSE = 0x0010

function Read-Shared([string]$path) {
    if (-not (Test-Path $path)) { return "" }
    try {
        $fs = [System.IO.File]::Open($path, 'Open', 'Read', 'ReadWrite')
        try { return (New-Object System.IO.StreamReader($fs)).ReadToEnd() } finally { $fs.Close() }
    } catch [System.IO.IOException] { return "" }
}

function Run-Job($job) {
    $exe = Resolve-Root $job.exe
    if (-not (Test-Path $exe)) { throw "Missing $exe" }
    $cwd = if ($job.cwd) { Resolve-Root $job.cwd } else { Split-Path -Parent $exe }
    $wait = if ($null -ne $job.wait) { [double]$job.wait } else { 8.0 }
    $timeout = if ($job.timeout) { [double]$job.timeout } else { 120.0 }
    $shot = if ($null -ne $job.shot) { [bool]$job.shot } else { $true }
    $log = Join-Path $OutDir "$($job.name).log"
    $errLog = Join-Path $OutDir "$($job.name).stderr.log"
    $png = Join-Path $OutDir "$($job.name).png"
    Remove-Item -Force -ErrorAction SilentlyContinue $log, $errLog, $png

    $ini = Join-Path $cwd "imgui.ini"
    $iniBytes = if (Test-Path $ini) { [System.IO.File]::ReadAllBytes($ini) } else { $null }

    $savedEnv = @{}
    if ($job.env) {
        foreach ($variable in $job.env.PSObject.Properties) {
            $savedEnv[$variable.Name] = [Environment]::GetEnvironmentVariable($variable.Name)
            [Environment]::SetEnvironmentVariable($variable.Name, [string]$variable.Value)
        }
    }

    $start = Get-Date
    $startArgs = @{ FilePath = $exe; WorkingDirectory = $cwd; PassThru = $true; RedirectStandardOutput = $log; RedirectStandardError = $errLog }
    if ($job.args) { $startArgs.ArgumentList = $job.args }
    $p = Start-Process @startArgs
    $null = $p.Handle

    $result = [ordered]@{ name = $job.name; exit = ""; client = ""; seconds = 0; until = ""; errors = 0 }
    try {
        $h = [IntPtr]::Zero
        $deadline = $start.AddSeconds($timeout)
        while ($h -eq [IntPtr]::Zero -and -not $p.HasExited -and (Get-Date) -lt $deadline) {
            Start-Sleep -Milliseconds 200
            $h = [DingoWin]::FindGlfw([uint32]$p.Id)
        }

        if ($h -ne [IntPtr]::Zero) {
            if ($job.until) {
                $result.until = "no"
                while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
                    if ((Read-Shared $log) -match $job.until) { $result.until = "yes"; break }
                    Start-Sleep -Milliseconds 250
                }
            } else {
                $end = (Get-Date).AddSeconds($wait)
                while (-not $p.HasExited -and (Get-Date) -lt $end) { Start-Sleep -Milliseconds 100 }
            }

            if ($shot -and -not $p.HasExited) {
                $r = New-Object DingoRect
                [DingoWin]::GetClientRect($h, [ref]$r) | Out-Null
                $w = $r.Right - $r.Left; $hh = $r.Bottom - $r.Top
                $result.client = "${w}x${hh}"
                $dib = New-Object DingoDib $w, $hh
                for ($try = 0; $try -lt 6; $try++) {
                    if ($try -gt 0) { Start-Sleep -Milliseconds 150 }
                    $dib.Capture($h) | Out-Null
                    if (-not $dib.IsBlank()) { break }
                }
                $dib.Save($png)
                $dib.Dispose()
            }

            if (-not $p.HasExited) { [DingoWin]::PostMessage($h, [uint32]$WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null }
        }

        if (-not $p.WaitForExit(20000)) { $p.Kill(); $p.WaitForExit(5000) | Out-Null; $result.exit = "killed" }
        else { $result.exit = "$($p.ExitCode)" }
    }
    finally {
        if (-not $p.HasExited) { $p.Kill() }
        if ($null -ne $iniBytes) { [System.IO.File]::WriteAllBytes($ini, $iniBytes) }
        foreach ($variable in $savedEnv.Keys) { [Environment]::SetEnvironmentVariable($variable, $savedEnv[$variable]) }
    }

    $result.seconds = [math]::Round(((Get-Date) - $start).TotalSeconds, 1)
    $text = Read-Shared $log
    $result.errors = ([regex]::Matches($text, '\[error\]|\[critical\]|\[FAIL\]|VUID-|Validation Error')).Count
    Write-Host ("[job] {0}: exit {1}, client {2}, {3} s, until {4}, {5} error line(s)" -f $result.name, $result.exit, $result.client, $result.seconds, $result.until, $result.errors)
    return [pscustomobject]$result
}

$jobList = @(Get-Content -Raw $Jobs | ConvertFrom-Json | ForEach-Object { $_ })
$results = @()
foreach ($job in $jobList) {
    try { $results += Run-Job $job }
    catch { Write-Host "[job] $($job.name): $($_.Exception.Message)"; $results += [pscustomobject]@{ name = $job.name; exit = "error"; client = ""; seconds = 0; until = ""; errors = -1 } }
}
$csv = Join-Path $OutDir "results.csv"
$results | Export-Csv -NoTypeInformation -Append -Path $csv
