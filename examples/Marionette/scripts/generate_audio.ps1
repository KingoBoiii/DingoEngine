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
    param([float[]]$Dst, [float[]]$Src, [double]$Start = 0.0, [double]$Gain = 1.0)
    $offset = [int]($Start * $rate)
    for ($i = 0; $i -lt $Src.Length; $i++) {
        $j = $offset + $i
        if ($j -ge $Dst.Length) { break }
        $Dst[$j] += [float]($Src[$i] * $Gain)
    }
}

function Add-Tone {
    param([float[]]$Dst, [double]$Freq, [double]$Start, [double]$Seconds, [double]$Amp, [double]$Decay, [double]$Attack = 0.003)
    $offset = [int]($Start * $rate)
    $count = [int]($Seconds * $rate)
    for ($i = 0; $i -lt $count; $i++) {
        $j = $offset + $i
        if ($j -ge $Dst.Length) { break }
        $t = $i / [double]$rate
        $env = [math]::Exp(-$Decay * $t)
        if ($t -lt $Attack) { $env *= $t / $Attack }
        $Dst[$j] += [float]([math]::Sin($TwoPi * $Freq * $t) * $env * $Amp)
    }
}

function Add-NoiseBurst {
    param([float[]]$Dst, [double]$Start, [double]$Seconds, [double]$Amp, [double]$Decay, [double]$LowPass = 1.0, [double]$HighPass = 0.0)
    $count = [int]($Seconds * $rate)
    $burst = New-Noise $count
    if ($LowPass -lt 1.0) { Invoke-LowPass $burst $LowPass }
    if ($HighPass -gt 0.0) { Invoke-HighPass $burst $HighPass }
    for ($i = 0; $i -lt $count; $i++) { $burst[$i] = [float]($burst[$i] * [math]::Exp(-$Decay * $i / [double]$rate)) }
    Add-Into $Dst $burst $Start $Amp
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

Write-Host "Done."
