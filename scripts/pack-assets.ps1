# Usage: scripts\pack-assets.ps1 -Example <Example> -Output <out.dpak> [-Configuration Release]
#
# Packs examples\<Example>\assets into <out.dpak>, encrypted with examples\<Example>\dpak.key,
# building DingoPack (with MSBuild, from the generated solution) first if it isn't built. The game
# mounts <executable name>.dpak from its own directory, so put the package beside a copy of the
# executable, never beside the one in build\bin: there it would shadow the loose files you edit.
param(
	[Parameter(Mandatory = $true)][string]$Example,
	[Parameter(Mandatory = $true)][string]$Output,
	[string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root "examples\$Example"
$key = Join-Path $source "dpak.key"
$tool = Join-Path $root "build\bin\$Configuration-windows-x86_64\DingoPack\DingoPack.exe"

if (-not (Test-Path (Join-Path $source "assets") -PathType Container)) { throw "pack-assets: no $source\assets" }
if (-not (Test-Path $key -PathType Leaf)) { throw "pack-assets: no $key (DingoPack genkey makes one)" }

if (-not (Test-Path $tool -PathType Leaf)) {
	$project = Join-Path $root "tools\DingoPack\DingoPack.vcxproj"
	if (-not (Test-Path $project)) { throw "pack-assets: generate the solution first (premake5 vs2026)" }
	msbuild $project /m /p:Configuration=$Configuration /p:Platform=x64
	if ($LASTEXITCODE -ne 0) { throw "pack-assets: building DingoPack failed" }
}

$outputDirectory = Split-Path -Parent $Output
if ($outputDirectory) { New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null }

& $tool pack (Join-Path $source "assets") $Output --key $key
if ($LASTEXITCODE -ne 0) { throw "pack-assets: DingoPack pack failed" }
& $tool verify $Output --key $key | Out-Null
if ($LASTEXITCODE -ne 0) { throw "pack-assets: DingoPack verify failed" }
