# Generates Candlewick's audio as small mono 16-bit PCM WAVs at 22050 Hz, so no binary source is
# needed to rebuild them. Adapted from EchoVault's generator. The loops seam without a click: the
# drone uses whole cycle counts and the crackle wraps its events and smooths its hiss in a circle.
# Seeded, so a rerun writes identical files. Run from anywhere:
#   powershell -ExecutionPolicy Bypass -File examples/Candlewick/scripts/generate_audio.ps1

$ErrorActionPreference = "Stop"
$rate = 22050
$outDir = Join-Path $PSScriptRoot "..\assets\audio"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$TwoPi = [math]::PI * 2.0
$rng = New-Object System.Random 4711

function Write-Wav {
    param([string]$Name, [float[]]$Samples)
    $count = $Samples.Length
    $bytes = New-Object byte[] ($count * 2)
    for ($i = 0; $i -lt $count; $i++) {
        $v = $Samples[$i]
        if ($v -gt 1.0) { $v = 1.0 } elseif ($v -lt -1.0) { $v = -1.0 }
        $s = [int][math]::Round($v * 32767.0)
        $bytes[$i*2]   = [byte]($s -band 0xFF)
        $bytes[$i*2+1] = [byte](($s -shr 8) -band 0xFF)
    }
    $dataLen = $bytes.Length
    $path = Join-Path $outDir $Name
    $fs = [System.IO.File]::Create($path)
    $bw = New-Object System.IO.BinaryWriter($fs)
    $bw.Write([System.Text.Encoding]::ASCII.GetBytes("RIFF"))
    $bw.Write([int](36 + $dataLen))
    $bw.Write([System.Text.Encoding]::ASCII.GetBytes("WAVE"))
    $bw.Write([System.Text.Encoding]::ASCII.GetBytes("fmt "))
    $bw.Write([int]16)
    $bw.Write([int16]1)
    $bw.Write([int16]1)
    $bw.Write([int]$rate)
    $bw.Write([int]($rate * 2))
    $bw.Write([int16]2)
    $bw.Write([int16]16)
    $bw.Write([System.Text.Encoding]::ASCII.GetBytes("data"))
    $bw.Write([int]$dataLen)
    $bw.Write($bytes)
    $bw.Close(); $fs.Close()
    Write-Host ("  {0}  ({1} samples, {2:N2}s, {3} bytes)" -f $Name, $count, ($count / [double]$rate), (44 + $dataLen))
}

function New-Buffer {
    param([double]$Seconds)
    $buf = New-Object float[] ([int][math]::Round($Seconds * $rate))
    return ,$buf
}

function New-Noise {
    param([int]$Count)
    $buf = New-Object float[] $Count
    for ($i = 0; $i -lt $Count; $i++) { $buf[$i] = [float]($rng.NextDouble() * 2.0 - 1.0) }
    return ,$buf
}

# One-pole low-pass, in place. A small coefficient is dull, 1.0 passes everything.
function Invoke-LowPass {
    param([float[]]$Buf, [double]$Coef)
    $y = 0.0
    for ($i = 0; $i -lt $Buf.Length; $i++) { $y += $Coef * ($Buf[$i] - $y); $Buf[$i] = [float]$y }
}

# The same filter run twice around the buffer, so its end flows into its start.
function Invoke-CircularLowPass {
    param([float[]]$Buf, [double]$Coef)
    $y = 0.0
    for ($i = 0; $i -lt $Buf.Length; $i++) { $y += $Coef * ($Buf[$i] - $y) }
    for ($i = 0; $i -lt $Buf.Length; $i++) { $y += $Coef * ($Buf[$i] - $y); $Buf[$i] = [float]$y }
}

function Invoke-HighPass {
    param([float[]]$Buf, [double]$Coef)
    $y = 0.0
    for ($i = 0; $i -lt $Buf.Length; $i++) { $y += $Coef * ($Buf[$i] - $y); $Buf[$i] = [float]($Buf[$i] - $y) }
}

function Set-Peak {
    param([float[]]$Buf, [double]$Peak)
    $max = 0.0
    foreach ($v in $Buf) { $a = [math]::Abs($v); if ($a -gt $max) { $max = $a } }
    if ($max -le 0.0) { return }
    $scale = $Peak / $max
    for ($i = 0; $i -lt $Buf.Length; $i++) { $Buf[$i] = [float]($Buf[$i] * $scale) }
}

# Short ramps at both ends, so a one-shot never starts or stops on a step.
function Set-Edges {
    param([float[]]$Buf, [double]$InSeconds = 0.002, [double]$OutSeconds = 0.01)
    $inCount = [int]($InSeconds * $rate)
    $outCount = [int]($OutSeconds * $rate)
    for ($i = 0; $i -lt $inCount -and $i -lt $Buf.Length; $i++) { $Buf[$i] = [float]($Buf[$i] * ($i / [double]$inCount)) }
    for ($i = 0; $i -lt $outCount -and $i -lt $Buf.Length; $i++) { $Buf[$Buf.Length - 1 - $i] = [float]($Buf[$Buf.Length - 1 - $i] * ($i / [double]$outCount)) }
}

function Add-Into {
    param([float[]]$Dst, [float[]]$Src, [double]$Start = 0.0, [double]$Gain = 1.0, [bool]$Wrap = $false)
    $offset = [int]($Start * $rate)
    for ($i = 0; $i -lt $Src.Length; $i++) {
        $j = $offset + $i
        if ($j -ge $Dst.Length) { if ($Wrap) { $j = $j % $Dst.Length } else { break } }
        $Dst[$j] += [float]($Src[$i] * $Gain)
    }
}

# A decaying sine, optionally with a few harmonics (amplitudes for harmonics 2, 3, ...).
function Add-Tone {
    param([float[]]$Dst, [double]$Freq, [double]$Start, [double]$Seconds, [double]$Amp, [double]$Decay, [double]$Attack = 0.003, [double[]]$Harmonics = @())
    $offset = [int]($Start * $rate)
    $count = [int]($Seconds * $rate)
    for ($i = 0; $i -lt $count; $i++) {
        $j = $offset + $i
        if ($j -ge $Dst.Length) { break }
        $t = $i / [double]$rate
        $env = [math]::Exp(-$Decay * $t)
        if ($t -lt $Attack) { $env *= $t / $Attack }
        $v = [math]::Sin($TwoPi * $Freq * $t)
        for ($h = 0; $h -lt $Harmonics.Length; $h++) { $v += $Harmonics[$h] * [math]::Sin($TwoPi * $Freq * ($h + 2) * $t) }
        $Dst[$j] += [float]($v * $env * $Amp)
    }
}

# A burst of filtered noise under an exponential decay.
function Add-NoiseBurst {
    param([float[]]$Dst, [double]$Start, [double]$Seconds, [double]$Amp, [double]$Decay, [double]$LowPass = 1.0, [double]$HighPass = 0.0, [bool]$Wrap = $false)
    $count = [int]($Seconds * $rate)
    $burst = New-Noise $count
    if ($LowPass -lt 1.0) { Invoke-LowPass $burst $LowPass }
    if ($HighPass -gt 0.0) { Invoke-HighPass $burst $HighPass }
    for ($i = 0; $i -lt $count; $i++) { $burst[$i] = [float]($burst[$i] * [math]::Exp(-$Decay * $i / [double]$rate)) }
    Add-Into $Dst $burst $Start $Amp $Wrap
}

Write-Host "Generating Candlewick audio into $outDir"

# Brazier crackle (loop, 0.9 s): a soft rumble under random pops, all wrapped around the loop point.
$len = [int](0.9 * $rate)
$crackle = New-Buffer 0.9
$rumble = New-Noise $len
Invoke-CircularLowPass $rumble 0.05
Set-Peak $rumble 0.3
Add-Into $crackle $rumble
for ($p = 0; $p -lt 22; $p++) {
    $start = $rng.NextDouble() * 0.9
    $amp = 0.15 + 0.85 * $rng.NextDouble() * $rng.NextDouble()
    Add-NoiseBurst $crackle $start (0.012 + 0.02 * $rng.NextDouble()) $amp (220.0 + 420.0 * $rng.NextDouble()) 1.0 0.15 $true
}
for ($i = 0; $i -lt $len; $i++) {
    $swell = 1.0 - 0.3 * (0.5 + 0.5 * [math]::Sin($TwoPi * 2.0 * $i / [double]$len))
    $crackle[$i] = [float]($crackle[$i] * $swell)
}
Set-Peak $crackle 0.7
Write-Wav "crackle_loop.wav" $crackle

# Ambient drone (loop, 2 s): low partials with a slow beat; every frequency fits a whole number of cycles.
$len = 2 * $rate
$drone = New-Buffer 2.0
$partials = @( @(55.0, 0.5), @(55.5, 0.45), @(58.5, 0.12), @(82.5, 0.25), @(110.0, 0.15), @(165.0, 0.07) )
for ($i = 0; $i -lt $len; $i++) {
    $t = $i / [double]$rate
    $v = 0.0
    foreach ($p in $partials) { $v += $p[1] * [math]::Sin($TwoPi * $p[0] * $t) }
    $drone[$i] = [float]($v * (0.8 + 0.2 * [math]::Sin($TwoPi * 0.5 * $t)))
}
Set-Peak $drone 0.55
Write-Wav "ambient_drone.wav" $drone

# Footstep on stone: a dull thud.
$step = New-Buffer 0.13
Add-NoiseBurst $step 0.0 0.13 0.8 40.0 0.22
Add-Tone $step 80.0 0.0 0.13 0.9 28.0 0.002
Set-Peak $step 0.8
Set-Edges $step
Write-Wav "footstep.wav" $step

# Warden step: a heavier thud with an armour clank on top.
$clank = New-Buffer 0.22
Add-Tone $clank 62.0 0.0 0.22 0.7 24.0 0.002
Add-NoiseBurst $clank 0.0 0.1 0.5 60.0 0.5
foreach ($f in @(1230.0, 1870.0, 2570.0)) { Add-Tone $clank $f 0.0 0.2 0.18 38.0 0.001 }
Set-Peak $clank 0.7
Set-Edges $clank
Write-Wav "warden_step.wav" $clank

# Strike: three flint scrapes, a spark, then the wick catching.
$strike = New-Buffer 0.8
Add-NoiseBurst $strike 0.04 0.07 0.5 40.0 1.0 0.3
Add-NoiseBurst $strike 0.24 0.07 0.6 40.0 1.0 0.3
Add-NoiseBurst $strike 0.42 0.07 0.7 40.0 1.0 0.3
Add-Tone $strike 3100.0 0.43 0.05 0.12 70.0 0.001
Add-NoiseBurst $strike 0.58 0.22 0.9 9.0 0.12
Add-Tone $strike 140.0 0.58 0.22 0.5 7.0 0.04
Set-Peak $strike 0.8
Set-Edges $strike
Write-Wav "strike.wav" $strike

# Snuff: a breath of air over a falling tone.
$snuff = New-Buffer 0.4
Add-NoiseBurst $snuff 0.0 0.35 0.8 10.0 0.25
for ($i = 0; $i -lt $snuff.Length; $i++) {
    $t = $i / [double]$rate
    $freq = 260.0 - 140.0 * ($t / 0.4)
    $snuff[$i] += [float]([math]::Sin($TwoPi * $freq * $t) * [math]::Exp(-9.0 * $t) * 0.25)
}
for ($i = 0; $i -lt [int](0.02 * $rate); $i++) { $snuff[$i] = [float]($snuff[$i] * ($i / (0.02 * $rate))) }
Set-Peak $snuff 0.7
Set-Edges $snuff
Write-Wav "snuff.wav" $snuff

# Oil flask: two glass clinks and a short glug.
$flask = New-Buffer 0.5
foreach ($f in @(1760.0, 2637.0)) { Add-Tone $flask $f 0.0 0.2 0.4 16.0 0.001 }
foreach ($f in @(1976.0, 2960.0)) { Add-Tone $flask $f 0.11 0.2 0.35 16.0 0.001 }
$glugStart = [int](0.18 * $rate)
$glugLength = [int](0.25 * $rate)
$phase = 0.0
for ($i = 0; $i -lt $glugLength; $i++) {
    $t = $i / [double]$rate
    $freq = 260.0 + 180.0 * ($t / 0.25)
    $phase += $TwoPi * $freq / $rate
    $hann = [math]::Sin([math]::PI * $i / $glugLength)
    $flask[$glugStart + $i] += [float]([math]::Sin($phase) * (0.6 + 0.4 * [math]::Sin($TwoPi * 22.0 * $t)) * $hann * 0.4)
}
Set-Peak $flask 0.8
Set-Edges $flask
Write-Wav "flask.wav" $flask

# Alert sting: two buzzy pulses a tritone apart.
$alert = New-Buffer 0.5
Add-Tone $alert 622.0 0.0 0.16 0.6 9.0 0.004 @(0.0, 0.33, 0.0, 0.2)
Add-Tone $alert 880.0 0.16 0.34 0.7 6.5 0.004 @(0.0, 0.33, 0.0, 0.2)
Set-Peak $alert 0.8
Set-Edges $alert
Write-Wav "alert.wav" $alert

# Caught: a clang, then a boom falling away under a low rumble.
$caught = New-Buffer 1.0
$clangs = @( @(310.0, 0.5), @(770.0, 0.4), @(1130.0, 0.3), @(1830.0, 0.2) )
foreach ($c in $clangs) { Add-Tone $caught $c[0] 0.0 0.6 $c[1] 9.0 0.002 }
$phase = 0.0
for ($i = 0; $i -lt $caught.Length; $i++) {
    $t = $i / [double]$rate
    $freq = 40.0 + 160.0 * [math]::Exp(-2.2 * $t)
    $phase += $TwoPi * $freq / $rate
    $caught[$i] += [float]([math]::Sin($phase) * [math]::Exp(-3.2 * $t) * 0.9)
}
Add-NoiseBurst $caught 0.0 1.0 0.5 3.0 0.04
Set-Peak $caught 0.9
Set-Edges $caught
Write-Wav "caught.wav" $caught

# Ignite: a whoosh that brightens, a low woomph and the first crackles.
$ignite = New-Buffer 0.7
$whoosh = New-Noise $ignite.Length
$y = 0.0
for ($i = 0; $i -lt $whoosh.Length; $i++) {
    $t = $i / [double]$rate
    $coef = 0.03 + 0.52 * [math]::Min(1.0, $t / 0.4)
    $y += $coef * ($whoosh[$i] - $y)
    $env = if ($t -lt 0.18) { $t / 0.18 } else { [math]::Exp(-5.0 * ($t - 0.18)) }
    $ignite[$i] = [float]($y * $env * 1.6)
}
Add-Tone $ignite 95.0 0.1 0.5 0.6 7.0 0.05
for ($p = 0; $p -lt 5; $p++) { Add-NoiseBurst $ignite (0.25 + 0.4 * $rng.NextDouble()) 0.015 (0.2 + 0.4 * $rng.NextDouble()) 300.0 1.0 0.15 }
Set-Peak $ignite 0.8
Set-Edges $ignite
Write-Wav "ignite.wav" $ignite

# Win: a rising arpeggio over a held major chord.
$win = New-Buffer 1.5
$notes = @( @(523.25, 0.0), @(659.25, 0.14), @(783.99, 0.28), @(1046.5, 0.42) )
foreach ($n in $notes) { Add-Tone $win $n[0] $n[1] 1.0 0.4 3.0 0.004 @(0.35, 0.12) }
foreach ($f in @(261.63, 329.63, 392.0)) { Add-Tone $win $f 0.42 1.05 0.18 1.6 0.08 }
Set-Peak $win 0.8
Set-Edges $win 0.002 0.12
Write-Wav "win.wav" $win

Write-Host "Done."
