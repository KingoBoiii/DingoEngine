# Generates EchoVault's placeholder audio as small mono 16-bit PCM WAVs.
# No audio files ship in the repo, so this synthesizes them programmatically.
# Loops (orb chime, platform hum, ambient pad) use integer cycle counts and end
# at a zero crossing so they seam click-free. Run from anywhere:
#   powershell -File examples/EchoVault/scripts/generate_audio.ps1

$ErrorActionPreference = "Stop"
$rate = 22050
$outDir = Join-Path $PSScriptRoot "..\assets\audio"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

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
    $bw.Write([int]16)          # PCM chunk size
    $bw.Write([int16]1)         # PCM
    $bw.Write([int16]1)         # mono
    $bw.Write([int]$rate)
    $bw.Write([int]($rate * 2)) # byte rate
    $bw.Write([int16]2)         # block align
    $bw.Write([int16]16)        # bits
    $bw.Write([System.Text.Encoding]::ASCII.GetBytes("data"))
    $bw.Write([int]$dataLen)
    $bw.Write($bytes)
    $bw.Close(); $fs.Close()
    Write-Host ("  {0}  ({1} samples, {2:N2}s)" -f $Name, $count, ($count / [double]$rate))
}

$TwoPi = [math]::PI * 2.0

# Exact integer number of cycles across the buffer => seamless loop.
function New-Loop {
    param([double]$Freq, [double]$Seconds, [double]$Amp = 0.4, [double[]]$Harmonics = @(1.0))
    $approx = [int][math]::Round($Freq * $Seconds)
    if ($approx -lt 1) { $approx = 1 }
    $len = [int][math]::Round($approx / $Freq * $rate)
    $buf = New-Object float[] $len
    for ($i = 0; $i -lt $len; $i++) {
        $t = $i / [double]$rate
        $v = 0.0
        for ($h = 0; $h -lt $Harmonics.Length; $h++) {
            $mult = $h + 1
            $v += $Harmonics[$h] * [math]::Sin($TwoPi * $Freq * $mult * $t)
        }
        $buf[$i] = [float]($v * $Amp)
    }
    return $buf
}

# One-shot decaying tone (exp decay to ~0 at the tail => click-free end).
function New-Ping {
    param([double]$Freq, [double]$Seconds, [double]$Amp = 0.5, [double]$Decay = 8.0)
    $len = [int]($Seconds * $rate)
    $buf = New-Object float[] $len
    for ($i = 0; $i -lt $len; $i++) {
        $t = $i / [double]$rate
        $env = [math]::Exp(-$Decay * $t)
        $buf[$i] = [float]([math]::Sin($TwoPi * $Freq * $t) * $env * $Amp)
    }
    return $buf
}

Write-Host "Generating EchoVault audio into $outDir"

# Orb chime loop: bright 880Hz + soft octave, ~1.5s seamless loop.
Write-Wav "orb_chime.wav" (New-Loop -Freq 880.0 -Seconds 1.5 -Amp 0.30 -Harmonics @(1.0, 0.35, 0.15))

# Platform hum loop: low 110Hz drone with a fifth, ~1.0s seamless loop.
Write-Wav "platform_hum.wav" (New-Loop -Freq 110.0 -Seconds 1.0 -Amp 0.28 -Harmonics @(1.0, 0.4, 0.2))

# Ambient pad loop: soft 55Hz + 82.5Hz-ish body via harmonics, ~2.0s loop.
Write-Wav "ambient_pad.wav" (New-Loop -Freq 55.0 -Seconds 2.0 -Amp 0.20 -Harmonics @(1.0, 0.5, 0.25, 0.12))

# Footstep: short filtered noise burst.
$fsLen = [int](0.10 * $rate)
$foot = New-Object float[] $fsLen
$rng = New-Object System.Random 1234
$prev = 0.0
for ($i = 0; $i -lt $fsLen; $i++) {
    $t = $i / [double]$rate
    $env = [math]::Exp(-30.0 * $t)
    $n = ($rng.NextDouble() * 2.0 - 1.0)
    $prev = $prev * 0.6 + $n * 0.4   # cheap low-pass => a duller thud
    $foot[$i] = [float]($prev * $env * 0.5)
}
Write-Wav "footstep.wav" $foot

# Pickup arpeggio: three rising decaying tones concatenated.
$p1 = New-Ping -Freq 660.0 -Seconds 0.09 -Amp 0.45 -Decay 12.0
$p2 = New-Ping -Freq 880.0 -Seconds 0.09 -Amp 0.45 -Decay 12.0
$p3 = New-Ping -Freq 1320.0 -Seconds 0.22 -Amp 0.50 -Decay 9.0
$pickup = New-Object float[] ($p1.Length + $p2.Length + $p3.Length)
[Array]::Copy($p1, 0, $pickup, 0, $p1.Length)
[Array]::Copy($p2, 0, $pickup, $p1.Length, $p2.Length)
[Array]::Copy($p3, 0, $pickup, $p1.Length + $p2.Length, $p3.Length)
Write-Wav "pickup.wav" $pickup

# Jump: quick upward chirp.
$jLen = [int](0.18 * $rate)
$jump = New-Object float[] $jLen
for ($i = 0; $i -lt $jLen; $i++) {
    $t = $i / [double]$rate
    $freq = 300.0 + 500.0 * ($t / 0.18)
    $env = [math]::Exp(-9.0 * $t)
    $jump[$i] = [float]([math]::Sin($TwoPi * $freq * $t) * $env * 0.4)
}
Write-Wav "jump.wav" $jump

# Detection sting: harsh descending two-tone (sentry spotted you).
$dLen = [int](0.45 * $rate)
$det = New-Object float[] $dLen
for ($i = 0; $i -lt $dLen; $i++) {
    $t = $i / [double]$rate
    $freq = 520.0 - 220.0 * ($t / 0.45)
    $env = [math]::Exp(-4.0 * $t)
    $v = [math]::Sin($TwoPi * $freq * $t) + 0.5 * [math]::Sin($TwoPi * $freq * 1.5 * $t)
    $det[$i] = [float]($v * $env * 0.35)
}
Write-Wav "detection.wav" $det

# Win fanfare: rising major triad, non-looping.
$w1 = New-Ping -Freq 523.25 -Seconds 0.16 -Amp 0.40 -Decay 6.0  # C5
$w2 = New-Ping -Freq 659.25 -Seconds 0.16 -Amp 0.40 -Decay 6.0  # E5
$w3 = New-Ping -Freq 783.99 -Seconds 0.16 -Amp 0.40 -Decay 6.0  # G5
$w4 = New-Ping -Freq 1046.50 -Seconds 0.45 -Amp 0.45 -Decay 4.0 # C6
$win = New-Object float[] ($w1.Length + $w2.Length + $w3.Length + $w4.Length)
$off = 0
foreach ($seg in @($w1, $w2, $w3, $w4)) { [Array]::Copy($seg, 0, $win, $off, $seg.Length); $off += $seg.Length }
Write-Wav "win.wav" $win

Write-Host "Done."
