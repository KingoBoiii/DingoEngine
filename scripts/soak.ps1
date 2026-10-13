<#
.SYNOPSIS
  The v1.0 soak: Marionette's tournament and Candlewick side by side for -Minutes, sampling each
  process's private bytes every minute.

.DESCRIPTION
  A game fails when its private bytes at the end are more than -MaxGrowth percent above its sample
  at the -SettleMinutes mark, when it exits with a non-zero code, or when its output has a
  LOG ERROR or a [FAIL]. Marionette's --tournament is capped at 1000 bouts, which can end before
  -Minutes; the tournament is then started again on the next seeds, and each process is judged on
  its own (a process shorter than twice -SettleMinutes is judged on its exit and log only).
  Candlewick has no autoplay, so it idles in -Room with its wardens, flames and lights running.
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
    [double]$MaxGrowth = 5.0,
    [int]$Room = 1,
    [string]$Out = "build\soak"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$Bin = if ([System.IO.Path]::IsPathRooted($Bin)) { $Bin } else { Join-Path $root $Bin }
$Out = if ([System.IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $root $Out }
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$graphicsArg = if ($Graphics -eq "vulkan") { @() } else { @("--graphics=$Graphics") }
$tournamentBouts = 1000

$games = @(
    [pscustomobject]@{ Name = "marionette"; Exe = Join-Path $Bin "Marionette\Marionette.exe"; Cwd = Join-Path $root "examples\Marionette"; Run = 0; Seed = 1 },
    [pscustomobject]@{ Name = "candlewick"; Exe = Join-Path $Bin "Candlewick\Candlewick.exe"; Cwd = Join-Path $root "examples\Candlewick"; Run = 0; Seed = 0 }
)

function Get-Arguments($game) {
    if ($game.Name -eq "marionette") {
        return @("--tournament=$tournamentBouts", "--seed=$($game.Seed)", "--player-tier=3") + $graphicsArg
    }
    return @("--room=$Room") + $graphicsArg
}

function Start-Game($game) {
    $game.Run++
    $tag = "$($game.Name)-$($game.Run)"
    $arguments = Get-Arguments $game
    $process = Start-Process -FilePath $game.Exe -WorkingDirectory $game.Cwd -ArgumentList ($arguments -join " ") `
        -RedirectStandardOutput (Join-Path $Out "$tag.log") -RedirectStandardError (Join-Path $Out "$tag.stderr.log") -PassThru
    # Without reading the handle once, ExitCode stays empty after the process ends.
    $null = $process.Handle
    $game | Add-Member -Force NoteProperty Process $process
    $game | Add-Member -Force NoteProperty Tag $tag
    $game | Add-Member -Force NoteProperty Started (Get-Date)
    $game | Add-Member -Force NoteProperty Samples (New-Object System.Collections.Generic.List[double])
    Write-Host "$tag started: $($arguments -join ' ')"
}

$results = New-Object System.Collections.Generic.List[object]
function Finish-Game($game) {
    $process = $game.Process
    if (-not $process.HasExited) {
        $null = $process.CloseMainWindow()
        if (-not $process.WaitForExit(60000)) { $process.Kill(); $process.WaitForExit() }
    }
    $process.WaitForExit()
    $problems = @()
    if ($process.ExitCode -ne 0) { $problems += "exit $($process.ExitCode)" }
    $text = Get-Content -Raw -ErrorAction SilentlyContinue (Join-Path $Out "$($game.Tag).log")
    if ($text -match "LOG ERROR") { $problems += "log-error" }
    if ($text -match "\[FAIL\]") { $problems += "fail" }

    $samples = $game.Samples
    $settled = $null; $last = $null; $growth = $null
    if ($samples.Count -gt 2 * $SettleMinutes) {
        $settled = $samples[$SettleMinutes]
        $last = $samples[$samples.Count - 1]
        $growth = 100.0 * ($last - $settled) / $settled
        if ($growth -gt $MaxGrowth) { $problems += ("growth {0:N1}%" -f $growth) }
    }
    $results.Add([pscustomobject]@{
        Run = $game.Tag; Minutes = $samples.Count - 1
        StartMB = $(if ($samples.Count) { [math]::Round($samples[0], 1) } else { $null })
        SettledMB = $(if ($settled) { [math]::Round($settled, 1) } else { $null })
        EndMB = $(if ($last) { [math]::Round($last, 1) } else { $null })
        GrowthPct = $(if ($null -ne $growth) { [math]::Round($growth, 2) } else { $null })
        Result = $(if ($problems.Count) { $problems -join ", " } else { "ok" })
    })
}

function Add-Sample($game) {
    $game.Process.Refresh()
    if (-not $game.Process.HasExited) {
        $mb = $game.Process.PrivateMemorySize64 / 1MB
        $game.Samples.Add($mb)
        Add-Content -Path (Join-Path $Out "$($game.Name)-memory.csv") -Value ("{0},{1},{2:N1}" -f $game.Tag, ((Get-Date) - $game.Started).TotalMinutes.ToString("N1"), $mb)
    }
}

foreach ($game in $games) { Start-Game $game }
$deadline = (Get-Date).AddMinutes($Minutes)
$nextSample = Get-Date
while ((Get-Date) -lt $deadline) {
    foreach ($game in $games) {
        if ($game.Process.HasExited) {
            Finish-Game $game
            if ($game.Name -eq "marionette") { $game.Seed += $tournamentBouts }
            Start-Game $game
        }
    }
    if ((Get-Date) -ge $nextSample) {
        foreach ($game in $games) { Add-Sample $game }
        $nextSample = $nextSample.AddMinutes(1)
    }
    Start-Sleep -Seconds 5
}
foreach ($game in $games) {
    Add-Sample $game
    Finish-Game $game
}

$results | Format-Table -AutoSize | Out-String | Tee-Object -FilePath (Join-Path $Out "summary.txt") | Write-Host
$failed = @($results | Where-Object { $_.Result -ne "ok" }).Count
Write-Host "Soak ($Graphics, $Minutes min): $failed of $($results.Count) run(s) failed. Output: $Out"
exit [int]($failed -gt 0)
