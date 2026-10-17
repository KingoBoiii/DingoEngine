<#
  Runs a PR's GPU checks on every backend, on private desktops, and prints one report.
    -Tag       run name, e.g. pr240; output goes to build\gpu-test\<Tag>\
    -Tests     ';'-separated test-app argument sets, each with an optional @waitSec (default 15):
               "--test=hierarchy@30;--test=light --fog"
    -Examples  ';'-separated "Name|args|waitSec|action": "Marionette|--check|75;Candlewick||30|keys:77"
    -Apis      "vulkan,dx11,dx12" (default)
  Each backend runs in parallel; every --test=anim job runs afterwards, one at a time, because
  all of them share %TEMP%\DingoAnimationTest.
#>
param(
    [Parameter(Mandatory)] [string]$Tag,
    [string[]]$Tests = @(),
    [string[]]$Examples = @(),
    [string[]]$Apis = @("vulkan", "dx11", "dx12"),
    [string]$Config = "Debug",
    [string]$Worktree,
    [int]$FirstWait = 30,
    [switch]$ReportOnly
)
$ErrorActionPreference = "Stop"
if ($Tag -notmatch '^[A-Za-z0-9_-]+$') { throw "-Tag may only hold letters, digits, '-' and '_'" }
$Tests = @($Tests -split ';' | ForEach-Object { $_.Trim() } | Where-Object { $_ })
$Examples = @($Examples -split ';' | ForEach-Object { $_.Trim() } | Where-Object { $_ })
$Apis = @($Apis -split '[,;]' | ForEach-Object { $_.Trim().ToLower() } | Where-Object { $_ })
foreach ($a in $Apis) { if ($a -notin "vulkan", "dx11", "dx12") { throw "unknown API '$a': use vulkan, dx11 or dx12" } }
if (-not $Worktree) { $Worktree = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path }
$runner = Join-Path $PSScriptRoot "run-jobs.ps1"
$root = Join-Path $Worktree "build\gpu-test\$Tag"
$out = Join-Path $root "out"
$bin = Join-Path $Worktree "build\bin\$Config-windows-x86_64"

$knownErrors = [ordered]@{
    "SetParent: parenting '.*' to '.*' would make a cycle" = "expected: Hierarchy Test's deliberate bad SetParent"
    "SetParent: '.*' can't take a parent from another scene" = "expected: Hierarchy Test's deliberate bad SetParent"
    "Model::LoadFromFile failed for .*DingoAnimationTest.*Fox\.gltf" = "expected: Animation Test's broken-file reload"
    "Failed to load image: .*textures.does_not_exist\.png" = "expected: Asset Manager Test's missing-file load"
    "AssetManager: failed to load Texture2D '.*textures.does_not_exist\.png" = "expected: Asset Manager Test's missing-file load"
    "AssetManager: cannot import 'data/unknown\.xyz'" = "expected: Asset Manager Test's unknown-extension import"
    "Model::LoadFromFile failed for .*Duck\.gltf': ClipsOnly, but the file has no bones" = "expected: Asset Manager Test's ClipsOnly on a static model"
    "AudioEngine::CreateBus: a bus needs a name" = "expected: Audio Bus Test's unnamed bus"
    "AudioEngine::CreateBus: the parent of bus 'Bus Check Child' is destroyed" = "expected: Audio Bus Test's destroyed parent"
    "AudioEngine::CreateStream: invalid stream \(" = "expected: Audio Bus Test's invalid stream formats"
    "AssetManager: can't open package '.*pack-test.*\.dpak'" = "expected: Pack Test's wrong-key, missing and damaged packages"
    "Package 'damaged\.dpak': 'textures/container\.jpg' is damaged \(CRC mismatch\)" = "expected: Pack Test's damaged entry"
}
$knownD3D12 = [ordered]@{
    "Renderer3D_ShadowAtlas \(Depth\)" = "known: duplicate shadow-atlas barrier, on master since v0.9"
}

function Get-RunDir([string]$api, [string]$project, [string]$source) {
    $d = Join-Path $root "$api\$project"
    New-Item -ItemType Directory -Force $d | Out-Null
    if (-not (Test-Path "$d\assets")) { New-Item -ItemType Junction -Path "$d\assets" -Target "$source\assets" | Out-Null }
    if ((Test-Path "$source\imgui.ini") -and -not (Test-Path "$d\imgui.ini")) { Copy-Item "$source\imgui.ini" $d }
    $d
}

function Write-Jobs([string]$path, [string[]]$lines) {
    [IO.File]::WriteAllLines($path, [string[]]$lines)
}

function Start-Runner([string]$jobsFile, [string]$key) {
    Start-Process powershell -WindowStyle Hidden -PassThru -ArgumentList "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$runner`"", "-Jobs", "`"$jobsFile`"", "-OutDir", "`"$out\$key`"", "-Desk", "DingoGpu$Tag$key" `
        -RedirectStandardOutput (Join-Path $root "runner-$key.txt") -RedirectStandardError (Join-Path $root "runner-$key.err")
}

if (-not $ReportOnly) {
    if (-not ($Tests.Count + $Examples.Count)) { throw "nothing to run: pass -Tests and/or -Examples" }
    $testExe = Join-Path $bin "Dingo-TestFramework\Dingo-TestFramework.exe"
    if ($Tests.Count -and -not (Test-Path $testExe)) { throw "missing $($testExe): build Dingo-TestFramework ($Config) first" }
    $testNames = [regex]::Matches((Get-Content (Join-Path $Worktree "test\src\TestLayer.cpp") -Raw), 'm_Tests\.push_back\(\{ "([^"]+)"') | ForEach-Object { $_.Groups[1].Value.ToLower() }
    foreach ($t in $Tests) {
        if ($t -match '--test=("([^"]+)"|(\S+))') {
            $needle = ($Matches[2] + $Matches[3]).ToLower() -replace '@\d+$', ''
            if (-not ($testNames | Where-Object { $_.Contains($needle) })) { throw "--test=$needle matches no test in TestLayer.cpp; the app would silently start the first test" }
        }
    }
    foreach ($e in $Examples) {
        $n = ($e -split '\|')[0]
        if (-not (Test-Path (Join-Path $bin "$n\$n.exe"))) { throw "missing $n.exe: build examples\$n ($Config) first" }
    }
    if (Test-Path $out) { Remove-Item -Recurse -Force $out }
    Get-ChildItem $root -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "jobs-*.txt" -or $_.Name -like "runner-*" } | Remove-Item -Force
    New-Item -ItemType Directory -Force $out | Out-Null

    $anim = @()
    foreach ($api in $Apis) {
        $g = if ($api -eq "vulkan") { "" } else { "--graphics=$api" }
        $db = if ($api -eq "dx12") { "1" } else { "0" }
        $jobs = @(); $names = @{}
        foreach ($t in $Tests) {
            $spec, $wait = $t -split '@', 2
            if (-not $wait) { $wait = 15 }
            $args1 = ("$spec $g").Trim()
            $name = "$api-" + (($spec -replace '--test=', '' -replace '[^A-Za-z0-9]+', '-').Trim('-'))
            while ($names.ContainsKey($name)) { $name += "x" }; $names[$name] = 1
            $line = @($name, $testExe, (Get-RunDir $api "test" (Join-Path $Worktree "test")), $args1, $wait, "-", "full", $db, "-")
            if ($spec -match '--test=anim') { $anim += ($line -join " | ") } else { $jobs += , $line }
        }
        foreach ($e in $Examples) {
            $n, $eargs, $wait, $action = $e -split '\|'
            if (-not $wait) { $wait = 30 }
            if (-not $action) { $action = "-" }
            $crop = if ($action -eq "min") { "-" } else { "full" }
            $args1 = ("$eargs $g").Trim(); if (-not $args1) { $args1 = "-" }
            $name = "$api-" + $n.ToLower()
            if ($eargs) { $name += "-" + (($eargs -replace '[^A-Za-z0-9]+', '-').Trim('-')) }
            if ($action -ne "-") { $name += "-" + ($action -replace '[^A-Za-z0-9]+', '-').Trim('-') }
            while ($names.ContainsKey($name)) { $name += "x" }; $names[$name] = 1
            $dir = Get-RunDir $api $n.ToLower() (Join-Path $Worktree "examples\$n")
            $jobs += , @($name, (Join-Path $bin "$n\$n.exe"), $dir, $args1, $wait, "-", $crop, $db, $action)
        }
        if ($jobs.Count) {
            if ([int]$jobs[0][4] -lt $FirstWait) { $jobs[0][4] = $FirstWait }
            Write-Jobs (Join-Path $root "jobs-$api.txt") ($jobs | ForEach-Object { $_ -join " | " })
        }
    }

    $t0 = Get-Date
    $procs = foreach ($api in $Apis) {
        $jf = Join-Path $root "jobs-$api.txt"
        if (Test-Path $jf) { Start-Runner $jf $api }
    }
    $procs | ForEach-Object { $_.WaitForExit() }
    if ($anim.Count) {
        Write-Jobs (Join-Path $root "jobs-anim.txt") $anim
        (Start-Runner (Join-Path $root "jobs-anim.txt") "anim").WaitForExit()
    }
    "Ran in {0:N0} s. Output: $out" -f ((Get-Date) - $t0).TotalSeconds
}
if (-not (Test-Path $out)) { throw "no output at $($out): run without -ReportOnly first" }

"`n== Jobs"
$bad = 0
foreach ($jf in Get-ChildItem $root -Filter "jobs-*.txt") {
    $key = $jf.BaseName.Substring(5)
    $sumFile = Join-Path $out "$key\summary.txt"
    $rows = if (Test-Path $sumFile) { @(Get-Content $sumFile) } else { @() }
    foreach ($l in $rows) {
        $ok = $l -match 'exit=0 ' -and $l -match 'fail=0 ' -and $l -match 'sync=0 ' -and $l -match 'vuid=0 ' -and $l -match 'close->exit='
        if (-not $ok) { $bad++ }
        ($(if ($ok) { "  " } else { "!!" }) + " " + $l.TrimEnd())
    }
    $missing = @(Get-Content $jf.FullName | ForEach-Object { ($_ -split '\|')[0].Trim() } | Where-Object { $_ -and -not ($rows -match "^$([regex]::Escape($_)) ") })
    if ($missing.Count) {
        $bad++
        "!! $key runner stopped before: $($missing -join ', ')"
        $rl = Join-Path $out "$key\runner.log"
        if (Test-Path $rl) { Get-Content $rl -Tail 8 | ForEach-Object { "     $_" } }
    }
    $err = Join-Path $root "runner-$key.err"
    if ((Test-Path $err) -and (Get-Content $err -Raw)) { $bad++; "!! runner-$key.err: " + (Get-Content $err -Raw).Trim() }
}

"`n== [error]/[critical] log lines"
$errs = Get-ChildItem $out -Recurse -Filter "*.log" | Select-String -Pattern '\[(error|critical)\]'
$errs | ForEach-Object { ($_.Line -replace '^\[[0-9:]+\] ', '') } | Group-Object | ForEach-Object {
    $note = "UNEXPECTED"
    foreach ($k in $knownErrors.Keys) { if ($_.Name -match $k) { $note = $knownErrors[$k] } }
    if ($note -eq "UNEXPECTED") { $bad++ }
    "  x{0,-4} [{1}] {2}" -f $_.Count, $note, $_.Name.Substring(0, [Math]::Min(180, $_.Name.Length))
}

"`n== D3D12 debug layer"
$dbwin = Get-ChildItem $out -Recurse -Filter "*.dbwin.txt"
foreach ($f in $dbwin | Select-String -Pattern 'dbwin setup failed') { $bad++; "!! $($f.Filename): DBWIN listener failed, so D3D12 messages were not captured" }
$dx = $dbwin | Select-String -Pattern '(D3D12|DXGI) (ERROR|CORRUPTION|WARNING)'
$dx | ForEach-Object { ($_.Line -replace '0x[0-9A-Fa-f]+', '#') } | Group-Object | ForEach-Object {
    $note = "UNEXPECTED"
    foreach ($k in $knownD3D12.Keys) { if ($_.Name -match $k) { $note = $knownD3D12[$k] } }
    if ($_.Name -match '(ERROR|CORRUPTION):') { $bad++; if ($note -ne "UNEXPECTED") { $note = "KNOWN-ERROR, " + $note } }
    elseif ($note -eq "UNEXPECTED") { $bad++ }
    "  x{0,-6} [{1}] {2}" -f $_.Count, $note, $_.Name.Substring(0, [Math]::Min(180, $_.Name.Length))
}

"`nRESULT: " + $(if ($bad) { "$bad item(s) need a look (marked !!, UNEXPECTED or KNOWN-ERROR)" } else { "all clean" })
