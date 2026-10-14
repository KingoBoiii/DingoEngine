<#
  Runs DingoEngine jobs on a private desktop (nothing reaches the user's screen or focus).
  Jobs file: one job per line, '|'-separated:
    name | exe | cwd | args | waitSec | env (K=V;K=V or -) | crop (x,y,w,h or full or -) | dbwin (1/0) | action
  action: - | min (minimize at 8 s, close while minimized) | keys:<vk hex,...> (posted 10 s after launch; non-extended keys only: arrows, Home, End, Insert and Delete arrive as numpad keys)
  Posted mouse messages never reach ImGui widgets: its GLFW backend reads the real cursor.
  Per job writes <OutDir>\<name>.log (Dingo.log copy), .stdout, .stderr, .png, .dbwin.txt, and a
  summary line to <OutDir>\summary.txt.
#>
param(
    [Parameter(Mandatory)] [string]$Jobs,
    [Parameter(Mandatory)] [string]$OutDir,
    [switch]$Inner,
    [string]$Desk = "DingoGpuTest"
)
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if (-not $Inner) {
    Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class JobDesk {
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
  public static int Run(string commandLine, string cwd, uint timeoutMs, string desk) {
    string name = desk;
    IntPtr hdesk = CreateDesktopW(name, IntPtr.Zero, IntPtr.Zero, 0, 0x10000000, IntPtr.Zero);
    if (hdesk == IntPtr.Zero) throw new Exception("CreateDesktop failed: " + Marshal.GetLastWin32Error());
    try {
      var si = new STARTUPINFO(); si.cb = Marshal.SizeOf(si); si.lpDesktop = "WinSta0\\" + name;
      PROCESS_INFORMATION pi;
      if (!CreateProcessW(null, new StringBuilder(commandLine), IntPtr.Zero, IntPtr.Zero, false, 0,
                          IntPtr.Zero, cwd, ref si, out pi))
        throw new Exception("CreateProcess failed: " + Marshal.GetLastWin32Error());
      uint code = 1;
      if (WaitForSingleObject(pi.hProcess, timeoutMs) != 0) TerminateProcess(pi.hProcess, 124);
      GetExitCodeProcess(pi.hProcess, out code);
      CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
      return (int)code;
    } finally { CloseDesktop(hdesk); }
  }
}
"@
    $runLog = Join-Path $OutDir "runner.log"
    $cmd = "cmd.exe /c powershell -NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Inner -Jobs `"$Jobs`" -OutDir `"$OutDir`" -Desk $Desk > `"$runLog`" 2>&1"
    $code = [JobDesk]::Run($cmd, $OutDir, [uint32](3600 * 1000), $Desk)
    if (Test-Path $runLog) { Get-Content $runLog | Select-Object -Last 5 }
    exit $code
}

Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Text;
using System.Threading;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class JobWin {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern int GetClassName(IntPtr h, StringBuilder s, int max);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [StructLayout(LayoutKind.Sequential)]
  struct BMIH { public int biSize, biWidth, biHeight; public short biPlanes, biBitCount;
                public int biCompression, biSizeImage, biX, biY, biClrUsed, biClrImportant; }
  [DllImport("gdi32.dll")] static extern IntPtr CreateCompatibleDC(IntPtr dc);
  [DllImport("gdi32.dll")] static extern bool DeleteDC(IntPtr dc);
  [DllImport("gdi32.dll")] static extern IntPtr CreateDIBSection(IntPtr dc, ref BMIH b, uint u, out IntPtr bits, IntPtr s, uint o);
  [DllImport("gdi32.dll")] static extern IntPtr SelectObject(IntPtr dc, IntPtr o);
  [DllImport("gdi32.dll")] static extern bool DeleteObject(IntPtr o);

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
  // Client-area capture into a GDI DIB (GDI+ HDCs key out RGB(13,11,12)).
  public static Bitmap Capture(IntPtr hwnd) {
    RECT r; GetClientRect(hwnd, out r);
    int w = r.R - r.L, h = r.B - r.T;
    var bmi = new BMIH { biSize = 40, biWidth = w, biHeight = -h, biPlanes = 1, biBitCount = 32 };
    IntPtr dc = CreateCompatibleDC(IntPtr.Zero), bits;
    IntPtr dib = CreateDIBSection(dc, ref bmi, 0, out bits, IntPtr.Zero, 0);
    IntPtr old = SelectObject(dc, dib);
    PrintWindow(hwnd, dc, 3);
    var bmp = new Bitmap(w, h, PixelFormat.Format32bppRgb);
    var data = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format32bppRgb);
    var row = new byte[w * 4];
    for (int y = 0; y < h; y++) { Marshal.Copy(bits + y * row.Length, row, 0, row.Length); Marshal.Copy(row, 0, data.Scan0 + y * data.Stride, row.Length); }
    bmp.UnlockBits(data);
    SelectObject(dc, old); DeleteObject(dib); DeleteDC(dc);
    return bmp;
  }
}

// OutputDebugString listener (the D3D12 debug layer writes there, not to Dingo.log).
public static class DbWin {
  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  static extern IntPtr CreateEventW(IntPtr sa, bool manual, bool initial, string name);
  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  static extern IntPtr CreateFileMappingW(IntPtr file, IntPtr sa, uint protect, uint hi, uint lo, string name);
  [DllImport("kernel32.dll")] static extern IntPtr MapViewOfFile(IntPtr map, uint access, uint hi, uint lo, UIntPtr size);
  [DllImport("kernel32.dll")] static extern bool SetEvent(IntPtr h);
  [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr h, uint ms);
  static Thread t; static volatile bool stop; static uint pid;
  static StringBuilder sb = new StringBuilder();
  public static string Error = "";
  public static void Start(uint targetPid) {
    pid = targetPid; stop = false; lock (sb) sb.Clear();
    IntPtr ready = CreateEventW(IntPtr.Zero, false, false, "DBWIN_BUFFER_READY");
    IntPtr dataReady = CreateEventW(IntPtr.Zero, false, false, "DBWIN_DATA_READY");
    IntPtr map = CreateFileMappingW(new IntPtr(-1), IntPtr.Zero, 4, 0, 4096, "DBWIN_BUFFER");
    if (ready == IntPtr.Zero || dataReady == IntPtr.Zero || map == IntPtr.Zero) { Error = "dbwin setup failed " + Marshal.GetLastWin32Error(); return; }
    IntPtr view = MapViewOfFile(map, 4, 0, 0, new UIntPtr(4096));
    hReady = ready; hData = dataReady; hMap = map; hView = view;
    t = new Thread(() => {
      SetEvent(ready);
      while (!stop) {
        if (WaitForSingleObject(dataReady, 100) == 0) {
          uint p = (uint)Marshal.ReadInt32(view);
          string s = Marshal.PtrToStringAnsi(view + 4);
          if (p == pid) lock (sb) sb.Append(s);
          SetEvent(ready);
        }
      }
    });
    t.IsBackground = true; t.Start();
  }
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
  [DllImport("kernel32.dll")] static extern bool UnmapViewOfFile(IntPtr v);
  static IntPtr hReady, hData, hMap, hView;
  // Leaving these open makes every later OutputDebugString in the session wait 10 s for a reader.
  public static string Stop() {
    stop = true; if (t != null) t.Join(); t = null;
    if (hView != IntPtr.Zero) UnmapViewOfFile(hView);
    foreach (var h in new[] { hMap, hData, hReady }) if (h != IntPtr.Zero) CloseHandle(h);
    hReady = hData = hMap = hView = IntPtr.Zero;
    lock (sb) return sb.ToString();
  }
}
"@

$WM_CLOSE = 0x0010
$summary = Join-Path $OutDir "summary.txt"
foreach ($line in Get-Content $Jobs) {
    if (-not $line.Trim() -or $line.TrimStart().StartsWith("#")) { continue }
    $f = $line -split '\|' | ForEach-Object { $_.Trim() }
    $name, $exe, $cwd, $jobArgs, $wait, $envs, $crop, $dbwin, $action = $f
    $dlog = Join-Path $cwd "Dingo.log"
    if (Test-Path $dlog) { Remove-Item $dlog -Force }
    $saved = @{}
    if ($envs -and $envs -ne "-") {
        foreach ($kv in $envs -split ';') { $k, $v = $kv -split '=', 2; $saved[$k] = [Environment]::GetEnvironmentVariable($k); [Environment]::SetEnvironmentVariable($k, $v) }
    }
    $start = @{ FilePath = $exe; WorkingDirectory = $cwd; PassThru = $true
                RedirectStandardOutput = (Join-Path $OutDir "$name.stdout"); RedirectStandardError = (Join-Path $OutDir "$name.stderr") }
    if ($jobArgs -and $jobArgs -ne "-") { $start.ArgumentList = $jobArgs }
    $t0 = Get-Date
    $p = Start-Process @start
    $null = $p.Handle
    foreach ($k in $saved.Keys) { [Environment]::SetEnvironmentVariable($k, $saved[$k]) }
    if ($dbwin -eq "1") { [DbWin]::Start([uint32]$p.Id) }
    $h = [IntPtr]::Zero
    $deadline = (Get-Date).AddSeconds(60)
    while ($h -eq [IntPtr]::Zero -and -not $p.HasExited -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250; $h = [JobWin]::FindGlfw([uint32]$p.Id) }
    $status = ""
    if ($h -ne [IntPtr]::Zero) {
        $end = $t0.AddSeconds([double]$wait)
        $iconic = ""
        if ($action -eq "min") { while ((Get-Date) -lt $t0.AddSeconds(8) -and -not $p.HasExited) { Start-Sleep -Milliseconds 200 }; [JobWin]::ShowWindow($h, 6) | Out-Null; Start-Sleep -Milliseconds 500; $iconic = "iconic=" + [JobWin]::IsIconic($h) }
        if ($action -like "keys:*") {
            while ((Get-Date) -lt $t0.AddSeconds(10) -and -not $p.HasExited) { Start-Sleep -Milliseconds 200 }
            foreach ($vk in ($action.Substring(5) -split ",")) {
                $code = [Convert]::ToInt32($vk, 16); $scan = [JobWin]::MapVirtualKey([uint32]$code, 0)
                [JobWin]::PostMessage($h, 0x0100, [IntPtr]$code, [IntPtr](1 -bor ($scan -shl 16))) | Out-Null; Start-Sleep -Milliseconds 120
                [JobWin]::PostMessage($h, 0x0101, [IntPtr]$code, [IntPtr]([int64]1 -bor ($scan -shl 16) -bor 0xC0000000L)) | Out-Null; Start-Sleep -Milliseconds 1500
            }
        }
        while ((Get-Date) -lt $end -and -not $p.HasExited) { Start-Sleep -Milliseconds 200 }
        if (-not $p.HasExited -and $crop -and $crop -ne "-") {
            $bmp = [JobWin]::Capture($h)
            if ($crop -ne "full") {
                $c = $crop -split ',' | ForEach-Object { [int]$_ }
                $sub = $bmp.Clone((New-Object System.Drawing.Rectangle $c[0], $c[1], $c[2], $c[3]), $bmp.PixelFormat)
                $bmp.Dispose(); $bmp = $sub
            }
            $bmp.Save((Join-Path $OutDir "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
        }
        if (-not $p.HasExited) {
            $tc = Get-Date
            if ($action -eq "min") { [JobWin]::PostMessage($h, [uint32]0x0112, [IntPtr]0xF060, [IntPtr]::Zero) | Out-Null } else { [JobWin]::PostMessage($h, [uint32]$WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null }
            if (-not $p.WaitForExit(15000)) { $p.Kill(); $status = "KILLED(no exit)" } else { $status = "close->exit=" + [int]((Get-Date) - $tc).TotalMilliseconds + "ms " + $iconic }
        } else { $status = "EXITED-EARLY" }
    } else {
        if (-not $p.HasExited) { $p.Kill(); $status = "NO-WINDOW(killed)" } else { $status = "EXITED-NO-WINDOW" }
    }
    $p.WaitForExit()
    if ($dbwin -eq "1") { Set-Content -Path (Join-Path $OutDir "$name.dbwin.txt") -Value ([DbWin]::Stop() + [DbWin]::Error) }
    Start-Sleep -Milliseconds 300
    if (Test-Path $dlog) { Copy-Item $dlog (Join-Path $OutDir "$name.log") -Force }
    $logText = if (Test-Path $dlog) { Get-Content $dlog -Raw } else { "" }
    $pass = ([regex]::Matches($logText, '\[PASS\]')).Count
    $fail = ([regex]::Matches($logText, '\[FAIL\]')).Count
    $sync = ([regex]::Matches($logText, 'hazard')).Count
    $vuid = ([regex]::Matches($logText, 'VUID-')).Count
    $errs = ([regex]::Matches($logText, '\[(error|critical)\]')).Count
    $line = "{0,-34} exit={1,-6} pass={2,-3} fail={3,-2} sync={4,-5} vuid={5,-4} errlines={6,-4} {7}" -f $name, $p.ExitCode, $pass, $fail, $sync, $vuid, $errs, $status
    for ($try = 0; $try -lt 20; $try++) {
        try { [System.IO.File]::AppendAllText($summary, $line + "`r`n"); break } catch { Start-Sleep -Milliseconds 250 }
    }
    Write-Host $line
}
