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

Write-Host "Generating Marionette audio into $outDir"

$step = New-Buffer 0.16
Add-Tone $step 92.0 0.0 0.16 0.9 26.0 0.002
Add-NoiseBurst $step 0.0 0.12 0.6 38.0 0.3
Add-NoiseBurst $step 0.012 0.05 0.25 70.0 1.0 0.35
foreach ($f in @(2210.0, 3050.0)) { Add-Tone $step $f 0.004 0.1 0.07 55.0 0.001 }
Set-Peak $step 0.8
Set-Edges $step
Write-Wav "footstep.wav" $step

Write-Host "Done."
