<#
.SYNOPSIS
  The v1.0 soak: Marionette's tournament and Candlewick side by side for -Minutes, sampling each
  process's private bytes every -SampleSeconds.

.DESCRIPTION
  Each process is judged on its own samples from -SettleMinutes after it started: a Theil-Sen
  slope above -MaxSlope MB per minute fails it, and so does a process with fewer than -MinSamples
  settled samples, so a soak can't pass without measuring anything. A process also fails when it
  exits with a non-zero code or its output has a LOG ERROR or a [FAIL].
  Marionette runs one --tournament long enough to outlast -Minutes, so a leak can build up inside
  one process; if it exits early it is started again on the next seeds. Candlewick has no
  autoplay, so it idles in -Room with its wardens, flames and lights running, and --background
  keeps it updating while its window is unfocused. A game that exits within a minute three times
  in a row isn't started again. Both are closed through WM_CLOSE at the end, so teardown runs too.
  Output goes to -Out.

  A sign-off runs the 20-minute default; the run before a release tag uses -Minutes 60.

.EXAMPLE
  powershell -File scripts\soak.ps1 -Bin build\bin\Release-windows-x86_64 -Graphics vulkan -Minutes 20
#>
[CmdletBinding()]
param(
    [string]$Bin = "build\bin\Release-windows-x86_64",
    [ValidateSet("vulkan", "dx11", "dx12")]
    [string]$Graphics = "vulkan",
    [int]$Minutes = 20,
    [int]$SettleMinutes = 5,
    [int]$SampleSeconds = 15,
    [double]$MaxSlope = 0.1,
    [int]$MinSamples = 20,
    [int]$Room = 1,
    [string]$Out = "build\soak"
)

$ErrorActionPreference = "Stop"
if ($Minutes * 60 -lt $SettleMinutes * 60 + $MinSamples * $SampleSeconds) {
    [Console]::Error.WriteLine("-Minutes $Minutes is too short to judge: it needs -SettleMinutes plus -MinSamples samples ($SettleMinutes min + $MinSamples x $SampleSeconds s).")
    exit 2
}
$root = Split-Path -Parent $PSScriptRoot
$Bin = if ([System.IO.Path]::IsPathRooted($Bin)) { $Bin } else { Join-Path $root $Bin }
$Out = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $root $Out }
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$graphicsArg = @(if ($Graphics -ne "vulkan") { "--graphics=$Graphics" })
# Marionette's cap (TOURNAMENT_MAX): about 350 bouts a minute on the GPU machine, so hours of play.
$tournamentBouts = 100000
$maxShortRuns = 3

# WM_CLOSE to every visible top-level window of the process, as windows-smoke-test.ps1 does:
# CloseMainWindow posts to the one window .NET guesses is the main one, which needn't be the game's.
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class SoakWindows
{
	delegate bool EnumProc(IntPtr window, IntPtr parameter);
	[DllImport("user32.dll")] static extern bool EnumWindows(EnumProc callback, IntPtr parameter);
	[DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
	[DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr window);
	[DllImport("user32.dll")] static extern bool PostMessageW(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

	public static int CloseAll(uint processId)
	{
		int sent = 0;
		EnumWindows((window, parameter) =>
		{
			uint owner;
			GetWindowThreadProcessId(window, out owner);
			if (owner == processId && IsWindowVisible(window) && PostMessageW(window, 0x0010, IntPtr.Zero, IntPtr.Zero))
				++sent;
			return true;
		}, IntPtr.Zero);
		return sent;
	}
}
'@

$games = @(
    [pscustomobject]@{ Name = "marionette"; Exe = Join-Path $Bin "Marionette\Marionette.exe"; Cwd = Join-Path $root "examples\Marionette"; Run = 0; Seed = 1; ShortRuns = 0 },
    [pscustomobject]@{ Name = "candlewick"; Exe = Join-Path $Bin "Candlewick\Candlewick.exe"; Cwd = Join-Path $root "examples\Candlewick"; Run = 0; Seed = 0; ShortRuns = 0 }
)
foreach ($game in $games) {
    Set-Content -Path (Join-Path $Out "$($game.Name)-memory.csv") -Value "run,minutes,private_mb"
}

function Get-Arguments($game) {
    if ($game.Name -eq "marionette") {
        return @("--tournament=$tournamentBouts", "--seed=$($game.Seed)", "--player-tier=3") + $graphicsArg
    }
    return @("--room=$Room", "--background") + $graphicsArg
}

function Start-Game($game) {
    $game.Run++
    $tag = "$($game.Name)-$($game.Run)"
    $arguments = Get-Arguments $game
    $process = Start-Process -FilePath $game.Exe -WorkingDirectory $game.Cwd -ArgumentList ($arguments -join " ") `
        -RedirectStandardOutput (Join-Path $Out "$tag.log") -RedirectStandardError (Join-Path $Out "$tag.stderr.log") -PassThru -NoNewWindow
    # Without reading the handle once, ExitCode stays empty after the process ends.
    $null = $process.Handle
    $game | Add-Member -Force NoteProperty Process $process
    $game | Add-Member -Force NoteProperty Tag $tag
    $game | Add-Member -Force NoteProperty Started (Get-Date)
    $game | Add-Member -Force NoteProperty Samples (New-Object System.Collections.Generic.List[object])
    Write-Host "$tag started: $($arguments -join ' ')"
}

# The median of the pairwise slopes (Theil-Sen), in MB per minute: one transient 41 MB dip moved a
# least-squares fit of a Marionette run between 0.15 and 1.8 MB/min.
function Get-Slope($samples) {
    $slopes = New-Object System.Collections.Generic.List[double]
    for ($i = 0; $i -lt $samples.Count; $i++) {
        for ($j = $i + 1; $j -lt $samples.Count; $j++) {
            $dt = $samples[$j].Minutes - $samples[$i].Minutes
            if ($dt -gt 0) { $slopes.Add(($samples[$j].MB - $samples[$i].MB) / $dt) }
        }
    }
    if ($slopes.Count -eq 0) { return 0.0 }
    $sorted = $slopes.ToArray()
    [Array]::Sort($sorted)
    return $sorted[[int][math]::Floor($sorted.Length / 2)]
}

$results = New-Object System.Collections.Generic.List[object]
function Finish-Game($game) {
    $process = $game.Process
    $problems = @()
    if (-not $process.HasExited) {
        $null = [SoakWindows]::CloseAll([uint32]$process.Id)
        if (-not $process.WaitForExit(60000)) {
            try { $process.Kill() } catch {}
            $problems += "hung"
        }
    }
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { $problems += "exit $($process.ExitCode)" }
    # spdlog's own errors ("[*** LOG ERROR") go to stderr, the engine's log to stdout.
    $text = ""
    foreach ($file in "$($game.Tag).log", "$($game.Tag).stderr.log") {
        $path = Join-Path $Out $file
        if (Test-Path $path) { $text += [System.IO.File]::ReadAllText($path) }
    }
    if ($text -match "LOG ERROR") { $problems += "log-error" }
    if ($text -match "\[FAIL\]") { $problems += "fail" }

    $samples = $game.Samples
    $settled = @($samples | Where-Object { $_.Minutes -ge $SettleMinutes })
    $slope = $null
    if ($settled.Count -ge $MinSamples) {
        $slope = Get-Slope $settled
        if ($slope -gt $MaxSlope) { $problems += [string]::Format([cultureinfo]::InvariantCulture, "growth {0:F3} MB/min", $slope) }
    }
    else {
        $problems += "unjudged ($($settled.Count) settled samples)"
    }
    $results.Add([pscustomobject]@{
        Run = $game.Tag
        Minutes = $(if ($samples.Count) { [math]::Round($samples[$samples.Count - 1].Minutes, 1) } else { 0 })
        FirstMB = $(if ($samples.Count) { [math]::Round($samples[0].MB, 1) } else { $null })
        SettledMB = $(if ($settled.Count) { [math]::Round($settled[0].MB, 1) } else { $null })
        EndMB = $(if ($samples.Count) { [math]::Round($samples[$samples.Count - 1].MB, 1) } else { $null })
        SlopeMBPerMin = $(if ($null -ne $slope) { [math]::Round($slope, 4) } else { $null })
        Result = $(if ($problems.Count) { $problems -join ", " } else { "ok" })
    })
}

function Add-Sample($game) {
    $game.Process.Refresh()
    if (-not $game.Process.HasExited) {
        $minutes = ((Get-Date) - $game.Started).TotalMinutes
        $mb = $game.Process.PrivateMemorySize64 / 1MB
        $game.Samples.Add([pscustomobject]@{ Minutes = $minutes; MB = $mb })
        $line = [string]::Format([cultureinfo]::InvariantCulture, "{0},{1:F2},{2:F1}", $game.Tag, $minutes, $mb)
        Add-Content -Path (Join-Path $Out "$($game.Name)-memory.csv") -Value $line
    }
}

foreach ($game in $games) { Start-Game $game }
$deadline = (Get-Date).AddMinutes($Minutes)
# The first sample waits one interval, so it never reads a process that hasn't initialised yet.
$nextSample = (Get-Date).AddSeconds($SampleSeconds)
while ((Get-Date) -lt $deadline) {
    foreach ($game in $games) {
        if ($game.Process -and $game.Process.HasExited) {
            $short = ((Get-Date) - $game.Started).TotalMinutes -lt 1
            Finish-Game $game
            $game.ShortRuns = if ($short) { $game.ShortRuns + 1 } else { 0 }
            if ($game.ShortRuns -ge $maxShortRuns) {
                Write-Host "$($game.Name) exited within a minute $maxShortRuns times in a row; not restarting it."
                $game.Process = $null
                continue
            }
            if ($game.Name -eq "marionette") { $game.Seed += $tournamentBouts }
            Start-Game $game
        }
    }
    if ((Get-Date) -ge $nextSample) {
        foreach ($game in $games) { if ($game.Process) { Add-Sample $game } }
        $nextSample = $nextSample.AddSeconds($SampleSeconds)
    }
    Start-Sleep -Seconds 1
}
foreach ($game in $games) {
    if (-not $game.Process) { continue }
    Add-Sample $game
    Finish-Game $game
}

$results | Format-Table -AutoSize | Out-String -Width 200 | Tee-Object -FilePath (Join-Path $Out "summary.txt") | Write-Host
$failed = @($results | Where-Object { $_.Result -ne "ok" }).Count
Write-Host "Soak ($Graphics, $Minutes min, max $MaxSlope MB/min): $failed of $($results.Count) run(s) failed. Output: $Out"
exit [int]($failed -gt 0)
