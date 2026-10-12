# Usage: scripts/ci/windows-smoke-test.ps1 -Config <Debug|Release> [-Graphics dx11,dx12] [-Seconds 30] [-Tournament]
#
# The Windows twin of linux-smoke-test.sh, for runners without a GPU: the unit tests, then every
# Dingo-TestFramework case on each -Graphics API (on WARP: D3D11 takes it as the default adapter and
# D3D12 falls back to it; Vulkan has no driver there), then the test app's deliberate crashes
# (--crash=), which must leave their report and a minidump in the log folder, and with -Tournament
# Marionette's tournament. Apps that don't exit by themselves are closed after -Seconds through
# WM_CLOSE, so teardown runs too. A run fails on an unexpected exit code, a hang, a [FAIL]
# self-check or an spdlog LOG ERROR. WARP's timings mean nothing, so nothing here times anything.
# Logs go to build/smoke/<config>-windows/.
param(
	[Parameter(Mandatory = $true)] [string] $Config,
	[string[]] $Graphics = @('dx11', 'dx12'),
	[int] $Seconds = 30,
	[switch] $Tournament
)

$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$Bin = Join-Path $Root "build/bin/$Config-windows-x86_64"
$Logs = Join-Path $Root "build/smoke/$Config-windows"
New-Item -ItemType Directory -Force -Path $Logs | Out-Null
$script:Failures = 0

function Format-Argument([string] $Argument)
{
	if ($Argument -match '\s') { return '"' + $Argument + '"' }
	return $Argument
}

function Invoke-Run
{
	param(
		[string] $Name,
		[string] $Cwd,
		[string] $Exe,
		[string[]] $Arguments = @(),
		[int] $RunSeconds = $Seconds,
		[int] $ExpectExit = 0,
		[string] $ExpectLog = '',
		[string] $ExpectLogFile = ''
	)

	$log = Join-Path $Logs "$Name.log"
	$errors = Join-Path $Logs "$Name.stderr.log"
	$argumentLine = ($Arguments | ForEach-Object { Format-Argument $_ }) -join ' '
	$startArguments = @{
		FilePath = $Exe
		WorkingDirectory = $Cwd
		RedirectStandardOutput = $log
		RedirectStandardError = $errors
		PassThru = $true
		NoNewWindow = $true
	}
	if ($argumentLine) { $startArguments.ArgumentList = $argumentLine }
	$process = Start-Process @startArguments
	# Without reading the handle once, ExitCode stays empty after the process ends.
	$null = $process.Handle

	$problems = @()
	if (-not $process.WaitForExit($RunSeconds * 1000))
	{
		$null = $process.CloseMainWindow()
		if (-not $process.WaitForExit(60000))
		{
			$process.Kill()
			$process.WaitForExit()
			$problems += 'hung'
		}
	}
	$process.WaitForExit()
	$status = [int] $process.ExitCode

	$text = ''
	foreach ($file in @($log, $errors))
	{
		if (Test-Path $file) { $text += [System.IO.File]::ReadAllText($file) }
	}

	if ($status -ne $ExpectExit) { $problems += ('exit=0x{0:X8}' -f $status) }
	if ($ExpectLog)
	{
		$expectText = $text
		if ($ExpectLogFile)
		{
			$expectText = if (Test-Path $ExpectLogFile) { [System.IO.File]::ReadAllText($ExpectLogFile) } else { '' }
		}
		if ($expectText -notmatch $ExpectLog) { $problems += "missing:'$ExpectLog'" }
	}
	$failed = ([regex]::Matches($text, '\[FAIL\]')).Count
	if ($failed -gt 0) { $problems += "failed-checks=$failed" }
	if ($text -match 'LOG ERROR') { $problems += 'log-error' }

	$passed = ([regex]::Matches($text, '\[PASS\]')).Count
	if ($problems.Count -gt 0)
	{
		$script:Failures++
		Write-Output "::error title=Smoke test $Name ($Config)::$($problems -join ' ')"
		Write-Output "FAIL  $Name ($passed passed): $($problems -join ' ')"
		$text -split "`n" | Where-Object { $_ -match '\[FAIL\]|LOG ERROR|Assertion failed|crashed' } | Select-Object -First 20 | ForEach-Object { Write-Output $_ }
	}
	else
	{
		Write-Output "ok    $Name ($passed passed)"
	}
}

$unitLog = Join-Path $Logs 'unit-tests.log'
& (Join-Path $Bin 'Dingo-UnitTests/Dingo-UnitTests.exe') *> $unitLog
if ($LASTEXITCODE -eq 0)
{
	Write-Output "ok    unit-tests"
}
else
{
	$script:Failures++
	Write-Output "::error title=Unit tests ($Config)::see $unitLog"
	Write-Output "FAIL  unit-tests"
	Get-Content $unitLog | Where-Object { $_ -match 'ERROR|Status' } | Select-Object -First 20 | ForEach-Object { Write-Output $_ }
}

$testApp = Join-Path $Bin 'Dingo-TestFramework/Dingo-TestFramework.exe'
$testDir = Join-Path $Root 'test'
$tests = Select-String -Path (Join-Path $testDir 'src/TestLayer.cpp') -Pattern '^\s*m_Tests\.push_back\(\{ "([^"]+)"' |
	ForEach-Object { $_.Matches[0].Groups[1].Value }

foreach ($api in $Graphics)
{
	foreach ($test in $tests)
	{
		Invoke-Run -Name ("$api-test-" + ($test -replace '[^A-Za-z0-9]', '_')) -Cwd $testDir -Exe $testApp -Arguments @("--test=$test", "--graphics=$api")
	}
}

if ($Tournament)
{
	Invoke-Run -Name 'marionette-tournament' -Cwd (Join-Path $Root 'examples/Marionette') -Exe (Join-Path $Bin 'Marionette/Marionette.exe') `
		-Arguments @('--tournament=10', '--seed=1', '--player-tier=3', "--graphics=$($Graphics[0])") -RunSeconds 1800 -ExpectLog '\[PASS\] tournament'
}

# The crash reports land in the log file, beside a minidump.
# Hex literals with the top bit set are negative Int32s in PowerShell, as Windows exit codes are.
function Invoke-Crash([string] $Kind, [int] $ExpectExit, [string] $ExpectLog)
{
	$dir = Join-Path $Logs "crash-$Kind"
	if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
	Invoke-Run -Name "crash-$Kind" -Cwd $testDir -Exe $testApp -Arguments @('--test=Text', "--crash=$Kind", "--log-dir=$dir", "--graphics=$($Graphics[0])") `
		-ExpectExit $ExpectExit -ExpectLog $ExpectLog -ExpectLogFile (Join-Path $dir 'Dingo.log')
	if ($ExpectLog -match 'Minidump' -and -not (Get-ChildItem -Path $dir -Filter 'Dingo-*.dmp' -ErrorAction SilentlyContinue))
	{
		$script:Failures++
		Write-Output "::error title=Smoke test crash-$Kind ($Config)::no minidump in $dir"
		Write-Output "FAIL  crash-${Kind}: no minidump"
	}
}

Invoke-Crash 'fatal' 1 "Fatal error: The test app's --crash=fatal"
Invoke-Crash 'segv' 0xC0000005 'crashed: access violation[\s\S]*Minidump: '
Invoke-Crash 'thread' 0xC0000005 'crashed: access violation[\s\S]*Minidump: '
Invoke-Crash 'abort' 3 'crashed: abort\(\)[\s\S]*Minidump: '
Invoke-Crash 'terminate' 3 'std::terminate called on an uncaught exception[\s\S]*Minidump: '
Invoke-Crash 'verify' 0x80000003 'crashed: breakpoint[\s\S]*Minidump: '

Write-Output "Unit tests, $($tests.Count) test cases on $($Graphics -join ', '), the crash reports$(if ($Tournament) { ' and the tournament' }): $($script:Failures) failed"
exit [int] ($script:Failures -gt 0)
