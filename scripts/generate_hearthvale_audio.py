#!/usr/bin/env python3
"""Synthesizes Hearthvale's placeholder audio as small mono 16-bit PCM WAVs at 22050 Hz.

Nothing is downloaded and no binary source is needed: every file is rebuilt from this script,
seeded, so a rerun writes identical bytes. Loops (music, ambience, pond) wrap in a circle, so
they seam without a click. Run from anywhere:
    python3 scripts/generate_hearthvale_audio.py
"""
import math
import os
import random
import struct
import wave

RATE = 22050
TWO_PI = math.pi * 2.0
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "examples", "Hearthvale", "assets", "audio")


def buf(seconds):
    return [0.0] * int(round(seconds * RATE))


def midi(note):
    return 440.0 * 2.0 ** ((note - 69) / 12.0)


def noise(rng, count):
    return [rng.uniform(-1.0, 1.0) for _ in range(count)]


def lowpass(x, cutoff, circular=False):
    a = 1.0 - math.exp(-TWO_PI * cutoff / RATE)
    n = len(x)
    src = x * 3 if circular else x
    y = [0.0] * len(src)
    s = 0.0
    for i, v in enumerate(src):
        s += a * (v - s)
        y[i] = s
    return y[n:2 * n] if circular else y


def highpass(x, cutoff, circular=False):
    low = lowpass(x, cutoff, circular)
    return [v - l for v, l in zip(x, low)]


def bandpass(x, low_cut, high_cut, circular=False):
    return lowpass(highpass(x, low_cut, circular), high_cut, circular)


def normalise(x, peak):
    top = max(abs(v) for v in x) or 1.0
    return [v * peak / top for v in x]


def env_ad(n, attack, decay_rate):
    out = [0.0] * n
    a = max(1, int(attack * RATE))
    for i in range(n):
        t = i / RATE
        up = min(1.0, i / a)
        out[i] = up * math.exp(-t * decay_rate)
    return out


def tone(freq, seconds, decay, attack=0.004, harmonics=((1.0, 1.0),), phase=0.0):
    n = int(seconds * RATE)
    e = env_ad(n, attack, decay)
    out = [0.0] * n
    for i in range(n):
        t = i / RATE
        s = 0.0
        for mult, amp in harmonics:
            s += amp * math.sin(TWO_PI * freq * mult * t + phase)
        out[i] = s * e[i]
    return out


def add(dst, src, offset=0, gain=1.0, wrap=False):
    n = len(dst)
    for i, v in enumerate(src):
        j = offset + i
        if wrap:
            j %= n
        elif j >= n:
            break
        dst[j] += v * gain


def fade_edges(x, ms=4):
    f = max(1, int(RATE * ms / 1000.0))
    for i in range(min(f, len(x))):
        g = i / f
        x[i] *= g
        x[len(x) - 1 - i] *= g
    return x


def write_wav(name, samples):
    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, name)
    data = b"".join(struct.pack("<h", int(round(max(-1.0, min(1.0, v)) * 32767.0))) for v in samples)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data)
    print("  %-24s %5.2fs %8d bytes" % (name, len(samples) / RATE, 44 + len(data)))


def thump(freq, seconds, decay, rng, grit=0.0):
    n = int(seconds * RATE)
    out = [0.0] * n
    phase = 0.0
    for i in range(n):
        t = i / RATE
        f = freq * (1.0 + 1.5 * math.exp(-t * 40.0))
        phase += TWO_PI * f / RATE
        out[i] = math.sin(phase) * math.exp(-t * decay)
    if grit > 0.0:
        g = lowpass(noise(rng, n), 2500.0)
        for i in range(n):
            out[i] += g[i] * grit * math.exp(-i / RATE * decay * 1.6)
    return out


def sfx_ui(rng):
    write_wav("ui_click.wav", fade_edges(normalise(tone(1250.0, 0.07, 55.0, harmonics=((1, 1), (2, 0.3))), 0.5)))
    write_wav("ui_focus.wav", fade_edges(normalise(tone(900.0, 0.035, 90.0), 0.3)))
    up = buf(0.18)
    add(up, tone(600.0, 0.1, 20.0), 0)
    add(up, tone(900.0, 0.1, 20.0), int(0.06 * RATE))
    write_wav("ui_open.wav", fade_edges(normalise(up, 0.5)))
    down = buf(0.18)
    add(down, tone(900.0, 0.1, 20.0), 0)
    add(down, tone(600.0, 0.1, 20.0), int(0.06 * RATE))
    write_wav("ui_close.wav", fade_edges(normalise(down, 0.5)))
    coin = buf(0.5)
    add(coin, tone(1568.0, 0.4, 9.0, harmonics=((1, 1), (3, 0.2))), 0)
    add(coin, tone(2093.0, 0.4, 7.0, harmonics=((1, 1), (3, 0.15))), int(0.07 * RATE))
    write_wav("ui_coin.wav", fade_edges(normalise(coin, 0.55)))
    n = int(0.22 * RATE)
    err = [0.0] * n
    for i in range(n):
        t = i / RATE
        sq = 1.0 if math.sin(TWO_PI * 170.0 * t) >= 0.0 else -1.0
        err[i] = sq * math.exp(-t * 9.0) * 0.35
    write_wav("ui_error.wav", fade_edges(normalise(lowpass(err, 900.0), 0.5)))


def sfx_tools(rng):
    hoe = thump(110.0, 0.2, 22.0, rng, 0.9)
    write_wav("tool_hoe.wav", fade_edges(normalise(hoe, 0.7)))

    n = int(0.55 * RATE)
    water = lowpass(highpass(noise(rng, n), 1200.0), 5000.0)
    water = [v * math.sin(math.pi * i / n) ** 0.7 for i, v in enumerate(water)]
    write_wav("tool_water.wav", fade_edges(normalise(water, 0.45)))

    axe = thump(150.0, 0.26, 16.0, rng, 0.7)
    add(axe, tone(520.0, 0.1, 40.0), 0, 0.3)
    write_wav("tool_axe.wav", fade_edges(normalise(axe, 0.7)))

    pick = buf(0.3)
    add(pick, tone(2400.0, 0.3, 22.0, harmonics=((1, 1), (1.51, 0.6), (2.3, 0.3))), 0)
    add(pick, tone(3100.0, 0.2, 35.0), 0, 0.6)
    add(pick, thump(180.0, 0.1, 40.0, rng, 0.4), 0, 0.7)
    write_wav("tool_pick.wav", fade_edges(normalise(pick, 0.6)))

    n = int(0.3 * RATE)
    sc = bandpass(noise(rng, n), 800.0, 4500.0)
    sc = [v * math.sin(math.pi * i / n) ** 2 for i, v in enumerate(sc)]
    write_wav("tool_scythe.wav", fade_edges(normalise(sc, 0.5)))


def sfx_world(rng):
    place = thump(95.0, 0.26, 18.0, rng, 0.5)
    add(place, tone(700.0, 0.06, 60.0), int(0.02 * RATE), 0.25)
    write_wav("place.wav", fade_edges(normalise(place, 0.7)))

    n = int(0.22 * RATE)
    rem = [0.0] * n
    phase = 0.0
    for i in range(n):
        t = i / RATE
        phase += TWO_PI * (520.0 - 900.0 * t) / RATE
        rem[i] = math.sin(phase) * math.exp(-t * 14.0)
    write_wav("remove.wav", fade_edges(normalise(rem, 0.5)))

    plant = tone(420.0, 0.16, 28.0, harmonics=((1, 1), (2, 0.4)))
    add(plant, lowpass(noise(rng, len(plant)), 1500.0), 0, 0.12)
    write_wav("plant.wav", fade_edges(normalise(plant, 0.45)))

    harvest = buf(0.45)
    for k, note in enumerate((72, 76, 79)):
        add(harvest, tone(midi(note), 0.3, 9.0, harmonics=((1, 1), (2, 0.25))), int(k * 0.07 * RATE))
    write_wav("harvest.wav", fade_edges(normalise(harvest, 0.5)))

    bell = tone(880.0, 1.0, 4.5, harmonics=((1, 1), (2.4, 0.5), (3.9, 0.3), (5.6, 0.15)))
    write_wav("shop_bell.wav", fade_edges(normalise(bell, 0.5)))

    thunk = thump(70.0, 0.34, 12.0, rng, 0.6)
    add(thunk, tone(300.0, 0.08, 50.0), int(0.12 * RATE), 0.3)
    write_wav("bin_thunk.wav", fade_edges(normalise(thunk, 0.7)))

    def step(cut_low, cut_high, length, decay):
        n = int(length * RATE)
        s = bandpass(noise(rng, n), cut_low, cut_high)
        return fade_edges(normalise([v * math.exp(-i / RATE * decay) for i, v in enumerate(s)], 0.5), 2)

    write_wav("footstep_grass.wav", step(300.0, 2200.0, 0.14, 28.0))
    write_wav("footstep_dirt.wav", step(120.0, 1100.0, 0.12, 34.0))
    write_wav("footstep_path.wav", step(600.0, 4200.0, 0.09, 50.0))

    n = int(0.3 * RATE)
    cluck = [0.0] * n
    phase = 0.0
    for i in range(n):
        t = i / RATE
        f = 650.0 + 120.0 * math.sin(TWO_PI * 22.0 * t) + 180.0 * math.exp(-t * 18.0)
        phase += TWO_PI * f / RATE
        gate = 1.0 if (t % 0.1) < 0.06 else 0.2
        cluck[i] = math.sin(phase) * gate * math.exp(-t * 6.0)
    write_wav("chicken.wav", fade_edges(normalise(lowpass(cluck, 3000.0), 0.45)))

    seconds = 4.0
    n = int(seconds * RATE)
    pond = lowpass(noise(rng, n), 700.0, circular=True)
    pond = [v * (0.65 + 0.35 * math.sin(TWO_PI * 0.5 * i / RATE)) for i, v in enumerate(pond)]
    write_wav("pond_loop.wav", normalise(pond, 0.4))


def ambience(rng):
    seconds = 4.0
    n = int(seconds * RATE)

    birds = buf(seconds)
    for start, base, count in ((0.3, 3200.0, 3), (1.4, 2700.0, 4), (2.6, 3600.0, 2), (3.1, 2900.0, 3)):
        for k in range(count):
            chirp = buf(0.09)
            phase = 0.0
            for i in range(len(chirp)):
                t = i / RATE
                phase += TWO_PI * (base + 900.0 * t / 0.09 * (1 if k % 2 == 0 else -1)) / RATE
                chirp[i] = math.sin(phase) * math.sin(math.pi * t / 0.09) ** 2
            add(birds, chirp, int((start + k * 0.13) * RATE), 0.7)
    add(birds, lowpass(noise(rng, n), 600.0, circular=True), 0, 0.02)
    write_wav("amb_birds.wav", normalise(birds, 0.4))

    crickets = buf(seconds)
    for g in range(8):
        for p in range(4):
            start = g * 0.5 + p * 0.075
            pulse = tone(4300.0, 0.05, 40.0, attack=0.005)
            add(crickets, pulse, int(start * RATE), 0.6)
    write_wav("amb_crickets.wav", normalise(crickets, 0.3))

    wind = lowpass(noise(rng, n), 450.0, circular=True)
    wind = [v * (0.6 + 0.4 * math.sin(TWO_PI * 0.25 * i / RATE + 0.7)) for i, v in enumerate(wind)]
    write_wav("amb_wind.wav", normalise(wind, 0.45))

    rain = bandpass(noise(rng, n), 900.0, 7000.0, circular=True)
    write_wav("amb_rain.wav", normalise(rain, 0.35))


def pluck(freq, seconds, decay, bright):
    return tone(freq, seconds, decay, attack=0.006, harmonics=((1, 1.0), (2, 0.35 * bright), (3, 0.12 * bright), (4, 0.05 * bright)))


def music(name, bpm, root, scale_chords, pattern, bright, pad, decay, seconds=8.0, bell=False):
    """Four chords of `scale_chords`, one bar each; `pattern` lists chord-tone indexes per eighth note (None = rest)."""
    out = buf(seconds)
    bar = seconds / 4.0
    eighth = bar / len(pattern)
    for b, chord in enumerate(scale_chords):
        tones = [root + c for c in chord]
        for step, idx in enumerate(pattern):
            if idx is None:
                continue
            note = tones[idx % len(tones)] + 12 * (idx // len(tones))
            start = b * bar + step * eighth
            if bell:
                voice = tone(midi(note + 12), 1.6, decay, attack=0.004, harmonics=((1, 1.0), (2.76, 0.4), (5.4, 0.15)))
            else:
                voice = pluck(midi(note), 1.2, decay, bright)
            add(out, voice, int(start * RATE), 0.5, wrap=True)
        padgain = pad
        for t in tones[:3]:
            span = int((bar + 0.4) * RATE)
            drone = [0.0] * span
            f = midi(t - 12)
            for i in range(span):
                x = i / span
                drone[i] = math.sin(TWO_PI * f * i / RATE) * math.sin(math.pi * x) ** 1.5
            add(out, drone, int(b * bar * RATE - 0.2 * RATE), padgain, wrap=True)
    write_wav(name, normalise(out, 0.5))


def make_music():
    music("music_spring.wav", 120, 60, [(0, 4, 7, 12), (-3, 4, 9, 12), (-5, 2, 7, 11), (0, 4, 7, 12)],
          [0, 1, 2, 3, 2, 1, 2, 1], bright=1.0, pad=0.12, decay=4.0)
    music("music_summer.wav", 120, 67, [(0, 4, 7, 12), (5, 9, 12, 16), (2, 5, 9, 14), (0, 4, 7, 12)],
          [0, 2, 1, 3, 2, 3, 1, 2], bright=1.3, pad=0.1, decay=5.0)
    music("music_fall.wav", 120, 57, [(0, 3, 7, 12), (-4, 0, 3, 7), (-2, 2, 5, 9), (-7, -4, 0, 3)],
          [0, None, 1, None, 2, None, 1, None], bright=0.6, pad=0.18, decay=2.6)
    music("music_winter.wav", 120, 65, [(0, 4, 7, 11), (-5, 0, 4, 7), (-3, 4, 7, 9), (-5, 0, 4, 7)],
          [0, None, None, 2, None, None, 1, None], bright=0.5, pad=0.14, decay=1.6, bell=True)


def main():
    rng = random.Random(4711)
    print("Writing Hearthvale audio to", os.path.normpath(OUT_DIR))
    sfx_ui(rng)
    sfx_tools(rng)
    sfx_world(rng)
    ambience(rng)
    make_music()


if __name__ == "__main__":
    main()
