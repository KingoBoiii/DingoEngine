# powershell -ExecutionPolicy Bypass -File examples/Marionette/scripts/generate_audio.ps1

$ErrorActionPreference = "Stop"
$rate = 22050
$outDir = Join-Path $PSScriptRoot "..\assets\audio"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$TwoPi = [math]::PI * 2.0
$rng = New-Object System.Random 2310

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

function Add-NoiseBurst {
    param([float[]]$Dst, [double]$Start, [double]$Seconds, [double]$Amp, [double]$Decay, [double]$LowPass = 1.0, [double]$HighPass = 0.0, [bool]$Wrap = $false)
    $count = [int]($Seconds * $rate)
    $burst = New-Noise $count
    if ($LowPass -lt 1.0) { Invoke-LowPass $burst $LowPass }
    if ($HighPass -gt 0.0) { Invoke-HighPass $burst $HighPass }
    for ($i = 0; $i -lt $count; $i++) { $burst[$i] = [float]($burst[$i] * [math]::Exp(-$Decay * $i / [double]$rate)) }
    Add-Into $Dst $burst $Start $Amp $Wrap
}

function Add-Whoosh {
    param([float[]]$Dst, [double]$Start, [double]$Seconds, [double]$Amp, [double]$PeakAt, [double]$CoefLow, [double]$CoefHigh)
    $count = [int]($Seconds * $rate)
    $noise = New-Noise $count
    $y = 0.0
    for ($i = 0; $i -lt $count; $i++) {
        $t = $i / [double]$count
        if ($t -lt $PeakAt) { $shape = $t / $PeakAt } else { $shape = 1.0 - ($t - $PeakAt) / (1.0 - $PeakAt) }
        $coef = $CoefLow + ($CoefHigh - $CoefLow) * $shape
        $y += $coef * ($noise[$i] - $y)
        $noise[$i] = [float]($y * $shape * $shape)
    }
    Add-Into $Dst $noise $Start $Amp
}

function Add-Chirp {
    param([float[]]$Dst, [double]$FreqFrom, [double]$FreqTo, [double]$Start, [double]$Seconds, [double]$Amp, [double]$Decay)
    $offset = [int]($Start * $rate)
    $count = [int]($Seconds * $rate)
    $phase = 0.0
    for ($i = 0; $i -lt $count; $i++) {
        $j = $offset + $i
        if ($j -ge $Dst.Length) { break }
        $t = $i / [double]$rate
        $freq = $FreqFrom + ($FreqTo - $FreqFrom) * ($i / [double]$count)
        $phase += $TwoPi * $freq / $rate
        $Dst[$j] += [float]([math]::Sin($phase) * [math]::Exp(-$Decay * $t) * $Amp)
    }
}

Write-Host "Generating Marionette audio into $outDir"

$step = New-Buffer 0.16
Add-Tone $step 92.0 0.0 0.16 0.9 26.0 0.002
Add-NoiseBurst $step 0.0 0.12 0.6 38.0 0.3
Add-NoiseBurst $step 0.012 0.05 0.25 70.0 1.0 0.35
foreach ($f in @(2210.0, 3050.0)) { Add-Tone $step $f 0.004 0.1 0.07 55.0 0.001 }
Set-Peak $step 0.8
Set-Edges $step
Write-Wav "footstep.wav" $step

$swing = New-Buffer 0.3
Add-Whoosh $swing 0.0 0.3 1.0 0.45 0.04 0.5
Add-Chirp $swing 380.0 150.0 0.02 0.22 0.12 9.0
Invoke-HighPass $swing 0.03
Set-Peak $swing 0.7
Set-Edges $swing 0.004 0.02
Write-Wav "swing.wav" $swing

$hit = New-Buffer 0.26
Add-Chirp $hit 120.0 52.0 0.0 0.2 1.0 14.0
Add-Tone $hit 190.0 0.0 0.1 0.45 40.0 0.001
Add-NoiseBurst $hit 0.0 0.05 0.9 70.0 0.55
Add-NoiseBurst $hit 0.0 0.025 0.6 120.0 1.0 0.3
Set-Peak $hit 0.9
Set-Edges $hit 0.001 0.02
Write-Wav "hit.wav" $hit

$block = New-Buffer 0.42
foreach ($p in @(@(520.0, 0.5, 11.0), @(1340.0, 0.4, 13.0), @(2210.0, 0.28, 16.0), @(3170.0, 0.18, 19.0))) {
    Add-Tone $block $p[0] 0.0 0.4 $p[1] $p[2] 0.001
}
Add-NoiseBurst $block 0.0 0.03 0.8 90.0 1.0 0.25
Add-Tone $block 110.0 0.0 0.08 0.35 35.0 0.001
Set-Peak $block 0.8
Set-Edges $block 0.001 0.03
Write-Wav "block.wav" $block

$parry = New-Buffer 0.6
foreach ($p in @(@(1240.0, 0.45, 6.0), @(1860.0, 0.35, 7.5), @(2480.0, 0.3, 9.0), @(3720.0, 0.2, 12.0), @(4960.0, 0.12, 15.0))) {
    Add-Tone $parry $p[0] 0.0 0.55 $p[1] $p[2] 0.001
}
Add-Chirp $parry 700.0 2600.0 0.0 0.07 0.45 20.0
Add-NoiseBurst $parry 0.0 0.02 0.7 140.0 1.0 0.4
Set-Peak $parry 0.8
Set-Edges $parry 0.001 0.04
Write-Wav "parry.wav" $parry

$dodge = New-Buffer 0.24
Add-Whoosh $dodge 0.0 0.24 1.0 0.35 0.03 0.18
Add-Chirp $dodge 210.0 90.0 0.0 0.2 0.18 11.0
Invoke-HighPass $dodge 0.03
Set-Peak $dodge 0.5
Set-Edges $dodge 0.004 0.025
Write-Wav "dodge.wav" $dodge

# K.O.: a gong struck over a boom that falls away, with a rumble under it.
$ko = New-Buffer 1.3
foreach ($p in @(@(210.0, 0.45, 4.0), @(530.0, 0.32, 5.5), @(1010.0, 0.24, 7.0), @(1790.0, 0.14, 9.0))) {
    Add-Tone $ko $p[0] 0.0 1.2 $p[1] $p[2] 0.002
}
Add-Chirp $ko 150.0 42.0 0.0 0.6 1.0 4.5
Add-NoiseBurst $ko 0.0 0.06 0.9 60.0 0.5
Add-NoiseBurst $ko 0.0 0.7 0.4 3.5 0.03
Set-Peak $ko 0.9
Set-Edges $ko 0.002 0.08
Write-Wav "ko.wav" $ko

# Win: a rising arpeggio over a held major chord.
$win = New-Buffer 1.7
foreach ($n in @(@(392.0, 0.0), @(493.88, 0.16), @(587.33, 0.32), @(783.99, 0.48))) {
    Add-Tone $win $n[0] $n[1] 1.1 0.4 3.0 0.004 @(0.35, 0.12)
}
foreach ($f in @(196.0, 246.94, 293.66)) { Add-Tone $win $f 0.48 1.15 0.18 1.6 0.08 }
Set-Peak $win 0.8
Set-Edges $win 0.002 0.12
Write-Wav "win.wav" $win

# Lose: a falling line in the minor over a low tone that stays after it.
$lose = New-Buffer 1.9
foreach ($n in @(@(440.0, 0.0), @(349.23, 0.24), @(293.66, 0.48), @(220.0, 0.72))) {
    Add-Tone $lose $n[0] $n[1] 1.0 0.4 3.2 0.006 @(0.3, 0.1)
}
Add-Tone $lose 110.0 0.72 1.1 0.3 2.0 0.05
Add-Tone $lose 130.81 0.72 1.1 0.15 2.0 0.05
Set-Peak $lose 0.8
Set-Edges $lose 0.002 0.15
Write-Wav "lose.wav" $lose

# Brazier crackle (loop, 1.8 s): a soft rumble under random pops, all wrapped around the loop point so it seams
# without a click.
$len = [int](1.8 * $rate)
$crackle = New-Buffer 1.8
$rumble = New-Noise $len
Invoke-CircularLowPass $rumble 0.05
Set-Peak $rumble 0.3
Add-Into $crackle $rumble
for ($p = 0; $p -lt 40; $p++) {
    $start = $rng.NextDouble() * 1.8
    $amp = 0.15 + 0.85 * $rng.NextDouble() * $rng.NextDouble()
    Add-NoiseBurst $crackle $start (0.012 + 0.02 * $rng.NextDouble()) $amp (220.0 + 420.0 * $rng.NextDouble()) 1.0 0.15 $true
}
for ($i = 0; $i -lt $len; $i++) {
    $swell = 1.0 - 0.3 * (0.5 + 0.5 * [math]::Sin($TwoPi * 3.0 * $i / [double]$len))
    $crackle[$i] = [float]($crackle[$i] * $swell)
}
Set-Peak $crackle 0.7
Write-Wav "brazier.wav" $crackle

Write-Host "Done."
