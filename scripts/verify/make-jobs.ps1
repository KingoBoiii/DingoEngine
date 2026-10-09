<#
.SYNOPSIS
  Writes capture.ps1 job lists for the v1.0 plan's sign-off runs.

.DESCRIPTION
  -Set regression: the plan's section 4 scene set (the 0 px gate), one frame each. Test-app
  captures are named t-*, crop them to the 3D viewport (300,24,1005,872) when diffing, since the
  side panels show live timings; m-* (Marionette) and c-* (Candlewick) compare whole.
  -Set perf: the timing runs, each waiting for its [PERF]/[Perf] line: the Throughput Test's modes
  (static, repeat, mixed and shadow at 10000, skinned at 64 and 128), Candlewick's four rooms and
  Marionette's first bout.
  -Set checks: every test case's start-up checks, for the validation-layer runs.

  -Bin is a folder holding Dingo-TestFramework\, Marionette\ and Candlewick\ as the build writes
  them (build\bin\<Config>-windows-x86_64, or a copy). -Backend vulkan|dx11|dx12. -Suffix is appended
  to every job name. -Env adds environment variables to every job (e.g. @{ VK_LOADER_LAYERS_DISABLE = "*validation*" }).

.EXAMPLE
  powershell -File scripts\verify\make-jobs.ps1 -Set regression -Bin build\baseline-v0.9.0\bin\Release -Backend dx12 -Out build\baseline-v0.9.0\jobs\regression-dx12.json
#>
[CmdletBinding()]
param(
    [ValidateSet("regression", "perf", "checks")]
    [string]$Set = "regression",
    [string]$Bin = "build\bin\Release-windows-x86_64",
    [ValidateSet("vulkan", "dx11", "dx12")]
    [string]$Backend = "vulkan",
    [string]$Suffix = "",
    [hashtable]$Env = @{},
    [Parameter(Mandatory = $true)]
    [string]$Out
)

$ErrorActionPreference = "Stop"
$testApp = Join-Path $Bin "Dingo-TestFramework\Dingo-TestFramework.exe"
$marionette = Join-Path $Bin "Marionette\Marionette.exe"
$candlewick = Join-Path $Bin "Candlewick\Candlewick.exe"
$graphics = if ($Backend -eq "vulkan") { "" } else { "--graphics=$Backend " }

$jobs = New-Object System.Collections.Generic.List[object]
function Add-Job([string]$name, [string]$exe, [string]$cwd, [string]$arguments, [hashtable]$extra = @{}) {
    $job = [ordered]@{ name = "$name$Suffix"; exe = $exe; cwd = $cwd; args = ($graphics + $arguments).Trim() }
    foreach ($key in $extra.Keys) { $job[$key] = $extra[$key] }
    if ($Env.Count) { $job.env = $Env }
    $jobs.Add($job)
}

switch ($Set) {
    "regression" {
        $test = @(
            @("t-light-default", "--test=light --lighting=default"),
            @("t-light-lights", "--test=light --lighting=lights"),
            @("t-light-overbudget", "--test=light --lighting=overbudget"),
            @("t-light-materials", "--test=light --lighting=materials"),
            @("t-light-entities", "--test=light --entities"),
            @("t-light-entities-overbudget", "--test=light --entities --lighting=overbudget"),
            @("t-batch", "--test=batch"),
            @("t-mesh-box", "--test=mesh --mesh=box --mesh-angle=30"),
            @("t-mesh-sphere", "--test=mesh --mesh=sphere --mesh-angle=30"),
            @("t-model", "--test=model --model-angle=30"),
            @("t-shadow", "--test=shadow"),
            @("t-post", "--test=post"),
            @("t-throughput-mixed", "--test=throughput --throughput=mixed --frame=300"),
            @("t-throughput-shadow", "--test=throughput --throughput=shadow --frame=300"))
        foreach ($t in $test) { Add-Job $t[0] $testApp "test" $t[1] @{ wait = 8 } }
        Add-Job "t-anim-clip" $testApp "test" "--test=anim --anim=clip --anim-time=0.5" @{ wait = 20 }

        Add-Job "m-freeze" $marionette "examples\Marionette" "--freeze" @{ wait = 10 }
        Add-Job "m-freeze-lineup" $marionette "examples\Marionette" "--freeze --lineup" @{ wait = 10 }
        foreach ($room in 1..4) {
            Add-Job "c-room$room" $candlewick "examples\Candlewick" "--room=$room --freeze" @{ wait = 10 }
            Add-Job "c-room$room-overview" $candlewick "examples\Candlewick" "--room=$room --freeze --overview" @{ wait = 10 }
        }
    }
    "perf" {
        $until = "\[PERF\]"
        foreach ($mode in "static", "repeat", "mixed", "shadow") {
            Add-Job "p-throughput-$mode" $testApp "test" "--test=throughput --throughput=$mode --perf --no-vsync" @{ until = $until; shot = $false; timeout = 900 }
        }
        foreach ($count in 64, 128) {
            Add-Job "p-throughput-skinned$count" $testApp "test" "--test=throughput --throughput=skinned --count=$count --perf --no-vsync" @{ until = $until; shot = $false; timeout = 900 }
        }
        foreach ($room in 1..4) {
            Add-Job "p-candlewick-room$room" $candlewick "examples\Candlewick" "--room=$room --perf --vsync=off" @{ until = "\[Perf\] GPU"; shot = $false; timeout = 900 }
        }
        Add-Job "p-marionette" $marionette "examples\Marionette" "--bout=1 --autoplay --perf --vsync=off" @{ until = "\[Perf\] frame"; shot = $false; timeout = 900 }
    }
    "checks" {
        $source = Get-Content -Raw (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) "test\src\TestLayer.cpp")
        foreach ($match in [regex]::Matches($source, '(?m)^\s*m_Tests\.push_back\(\{ "([^"]+)"')) {
            $name = $match.Groups[1].Value
            Add-Job ("v-" + ($name -replace '[^A-Za-z0-9]', '_')) $testApp "test" "--test=`"$name`"" @{ wait = 20; shot = $false }
        }
        Add-Job "v-marionette-check" $marionette "examples\Marionette" "--check" @{ until = "AI checks:"; shot = $false; timeout = 300 }
    }
}

$dir = Split-Path -Parent $Out
if ($dir) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
ConvertTo-Json -InputObject $jobs.ToArray() -Depth 4 | Set-Content -Encoding utf8 $Out
Write-Host "$($jobs.Count) job(s) -> $Out"
