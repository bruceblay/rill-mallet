#!/usr/bin/env python3
# Copyright (c) 2026 Bruce Blay
# SPDX-License-Identifier: GPL-3.0-or-later
"""Render a kalimba multisample set to mono 16-bit WAV at mallet::rate.

Clips run to 1.9 seconds. A real tine rings longer, but the score's own
envelope closes every melody note between a fifth of a second and one and a
third, and its one long support note is quiet by then, so storing more would
be storing something no note ever reaches.

These are synthesized, not recorded: a kalimba tine is a bar clamped at one
end, whose partials sit near 1 : 6.27 : 17.6 : 34.4 rather than in a harmonic
series, and each partial is given its own decay. Rendering them offline buys
what the firmware cannot afford per sample -- many partials, long tails, a
plucked transient and a body resonance -- which is the whole point of moving
this instrument to samples.

They are placeholders for real recordings. Anything recorded that replaces
them has to carry a license compatible with this project's GPL-3.0; note the
source in docs/SOURCES.md before embedding it.
"""
import argparse
import math
import random
import struct
import wave
from pathlib import Path

RATE = 32000

# Roots every four semitones from G3 to F6, which covers the score's melody
# range (MIDI 60-91) and its support range (55-72) with no voice ever bent
# more than two semitones from a root.
ROOTS = [55, 59, 63, 67, 71, 75, 79, 83, 87, 91]

# Ideal clamped-bar partials, flattened slightly toward the harmonic series
# the way a real tine's are by its mounting.
PARTIALS = [
    # ratio, level, decay seconds at middle C, spread
    (1.000, 1.00, 1.55),
    (6.180, 0.22, 0.62),
    (17.30, 0.07, 0.24),
    (33.80, 0.02, 0.12),
]


def render_note(midi, seconds, rng):
    hz = 440.0 * 2 ** ((midi - 69) / 12.0)
    frames = int(seconds * RATE)
    out = [0.0] * frames
    # Higher tines are shorter and stiffer, so they ring for less time.
    scale = (261.6 / hz) ** 0.55
    for ratio, level, decay in PARTIALS:
        freq = hz * ratio * (1.0 + rng.uniform(-0.0015, 0.0015))
        if freq > RATE * 0.45:
            continue
        life = decay * scale * rng.uniform(0.92, 1.08)
        omega = 2 * math.pi * freq / RATE
        damp = math.exp(-1.0 / (RATE * life))
        amp = level * rng.uniform(0.9, 1.1)
        phase = rng.uniform(0, 2 * math.pi)
        for i in range(frames):
            out[i] += amp * math.sin(omega * i + phase)
            amp *= damp
            if amp < 1e-5:
                break
    # The thumbnail release: a short bright click, filtered so it reads as a
    # nail leaving the tine rather than as a pop.
    click = 0.0
    lp = 0.0
    for i in range(min(frames, int(0.03 * RATE))):
        noise = rng.uniform(-1, 1) * math.exp(-i / (0.0022 * RATE))
        lp += (noise - lp) * 0.45
        click = lp * 0.5
        out[i] += click
    # Body: a resonant cavity an octave or so below the tine, barely audible
    # on its own but what keeps the note from sounding like a test tone.
    body_hz = max(90.0, hz * 0.5)
    omega = 2 * math.pi * body_hz / RATE
    amp = 0.16
    damp = math.exp(-1.0 / (RATE * 0.22))
    for i in range(frames):
        out[i] += amp * math.sin(omega * i)
        amp *= damp
        if amp < 1e-5:
            break
    # A couple of milliseconds of attack ramp, so the start is a strike and
    # not a click of the buffer opening.
    ramp = int(0.0015 * RATE)
    for i in range(min(ramp, frames)):
        out[i] *= i / ramp
    # Trim the tail once it is inaudible, then fade the last few milliseconds
    # so the truncation cannot click. The test is a block maximum, not a
    # single sample: a decaying sine crosses zero constantly, so comparing
    # one sample at a time finds a loud-looking value in every tail and trims
    # nothing at all.
    peak = max(abs(v) for v in out) or 1.0
    floor = peak * 0.014
    block = int(0.01 * RATE)
    end = len(out)
    while end > block and max(abs(v) for v in out[end - block:end]) < floor:
        end -= block
    out = out[:max(end, int(0.05 * RATE))]
    fade = min(len(out), int(0.008 * RATE))
    for i in range(fade):
        out[len(out) - fade + i] *= 1.0 - i / fade
    return out, peak


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('outdir', type=Path)
    parser.add_argument('--seconds', type=float, default=1.9)
    args = parser.parse_args()
    args.outdir.mkdir(parents=True, exist_ok=True)
    rendered = []
    for root in ROOTS:
        rng = random.Random(root * 7919)
        samples, _ = render_note(root, args.seconds, rng)
        rendered.append((root, samples))
    # One gain for the whole set, so the zones keep their natural balance
    # instead of each being normalized to its own peak.
    loudest = max(max(abs(v) for v in s) for _, s in rendered)
    gain = 0.89 / loudest
    for root, samples in rendered:
        path = args.outdir / f'kalimba_{root:02d}.wav'
        with wave.open(str(path), 'wb') as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(b''.join(
                struct.pack('<h', max(-32768, min(32767, int(v * gain * 32767)))) for v in samples))
        print(f'{path.name}: {len(samples)/RATE:.2f} s, {len(samples)*2/1024:.0f} KB')


if __name__ == '__main__':
    main()
