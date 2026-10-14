<#
.SYNOPSIS
  The v1.0 soak: Marionette's tournament and Candlewick side by side for -Minutes, sampling each
  process's private bytes every -SampleSeconds.

.DESCRIPTION
  Samples start once a process is a minute old, past start-up. A run fails when it exits with a
  non-zero code, when its output has a LOG ERROR or a [FAIL], or when its private bytes grow more
  than -MaxGrowth percent: from its sample at -SettleMinutes to its last, or, for a run shorter
  than twice -SettleMinutes, from its first sample to its last.
  Marionette's --tournament is capped at 1000 bouts, about 3 minutes in Release, so it is started
  again on the next seeds until -Minutes are up. Its runs are also judged against each other: the
  last run's final sample against the second run's. The first run is left out: it settled about
  30 MB above the rest in the first soak. A game that exits within a minute three times in a row isn't
  started again.
  Candlewick has no autoplay, so it idles in -Room with its wardens, flames and lights running.
  --perf keeps it updating while unfocused; it times the first 600 frames once and goes on.
  Both are closed through WM_CLOSE at the end, so teardown runs too. Output goes to -Out.

.EXAMPLE
  powershell -File scripts\soak.ps1 -Bin build\bin\Release-windows-x86_64 -Graphics vulkan -Minutes 60
#>
[CmdletBinding()]
param(
    [string]$Bin = "build\bin\Release-windows-x86_64",
    [ValidateSet("vulkan", "dx11", "dx12")]
    [string]$Graphics = "vulkan",
    [int]$Minutes = 60,
    [int]$SettleMinutes = 5,
    [int]$SampleSeconds = 30,
    [double]$MaxGrowth = 5.0,
    [int]$Room = 1,
    [string]$Out = "build\soak"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$Bin = if ([System.IO.Path]::IsPathRooted($Bin)) { $Bin } else { Join-Path $root $Bin }
$Out = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $root $Out }
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$graphicsArg = @(if ($Graphics -ne "vulkan") { "--graphics=$Graphics" })
$tournamentBouts = 1000
$maxShortRuns = 3
$warmupMinutes = 1.0

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
    $game | Add-Member -Force NoteProperty RunEnds (New-Object System.Collections.Generic.List[object])
}

function Get-Arguments($game) {
    if ($game.Name -eq "marionette") {
        return @("--tournament=$tournamentBouts", "--seed=$($game.Seed)", "--player-tier=3") + $graphicsArg
    }
    return @("--room=$Room", "--perf") + $graphicsArg
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
    $first = $null; $settled = $null; $last = $null; $growth = $null
    if ($samples.Count) {
        $first = $samples[0].MB
        $last = $samples[$samples.Count - 1].MB
        $game.RunEnds.Add([pscustomobject]@{ Run = $game.Run; MB = $last })
    }
    if ($samples.Count -ge 3) {
        $span = $samples[$samples.Count - 1].Minutes
        $settledSample = $samples[0]
        if ($span -ge 2 * $SettleMinutes) {
            $settledSample = @($samples | Where-Object { $_.Minutes -ge $SettleMinutes })[0]
        }
        $settled = $settledSample.MB
        $growth = 100.0 * ($last - $settled) / $settled
        if ($growth -gt $MaxGrowth) { $problems += ("growth {0:N1}%" -f $growth) }
    }
    $results.Add([pscustomobject]@{
        Run = $game.Tag; Minutes = [math]::Round(($process.ExitTime - $game.Started).TotalMinutes, 1)
        FirstMB = $(if ($null -ne $first) { [math]::Round($first, 1) } else { $null })
        SettledMB = $(if ($null -ne $settled) { [math]::Round($settled, 1) } else { $null })
        EndMB = $(if ($null -ne $last) { [math]::Round($last, 1) } else { $null })
        GrowthPct = $(if ($null -ne $growth) { [math]::Round($growth, 2) } else { $null })
        Result = $(if ($problems.Count) { $problems -join ", " } else { "ok" })
    })
}

function Add-Sample($game) {
    $game.Process.Refresh()
    if ($game.Process.HasExited) { return }
    $age = ((Get-Date) - $game.Started).TotalMinutes
    if ($age -lt $warmupMinutes) { return }
    $mb = $game.Process.PrivateMemorySize64 / 1MB
    $game.Samples.Add([pscustomobject]@{ Minutes = $age; MB = $mb })
    $line = [string]::Format([cultureinfo]::InvariantCulture, "{0},{1:F2},{2:F1}", $game.Tag, $age, $mb)
    Add-Content -Path (Join-Path $Out "$($game.Name)-memory.csv") -Value $line
}

foreach ($game in $games) { Start-Game $game }
$deadline = (Get-Date).AddMinutes($Minutes)
$nextSample = Get-Date
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
    Start-Sleep -Seconds 5
}
foreach ($game in $games) {
    if (-not $game.Process) { continue }
    Add-Sample $game
    Finish-Game $game
}

foreach ($game in $games) {
    $ends = @($game.RunEnds | Where-Object { $_.Run -ge 2 })
    if ($ends.Count -lt 2) { continue }
    $from = $ends[0]; $to = $ends[$ends.Count - 1]
    $growth = 100.0 * ($to.MB - $from.MB) / $from.MB
    $results.Add([pscustomobject]@{
        Run = "$($game.Name) runs $($from.Run)-$($to.Run)"; Minutes = $null; FirstMB = $null
        SettledMB = [math]::Round($from.MB, 1); EndMB = [math]::Round($to.MB, 1); GrowthPct = [math]::Round($growth, 2)
        Result = $(if ($growth -gt $MaxGrowth) { "growth {0:N1}% across runs" -f $growth } else { "ok" })
    })
}

$results | Format-Table -AutoSize | Out-String | Tee-Object -FilePath (Join-Path $Out "summary.txt") | Write-Host
$failed = @($results | Where-Object { $_.Result -ne "ok" }).Count
Write-Host "Soak ($Graphics, $Minutes min): $failed of $($results.Count) run(s) failed. Output: $Out"
exit [int]($failed -gt 0)
