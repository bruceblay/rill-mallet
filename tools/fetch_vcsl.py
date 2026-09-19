#!/usr/bin/env python3
# Copyright (c) 2026 Bruce Blay
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fetch real kalimba recordings from the Versilian Community Sample Library.

VCSL is CC0 (https://github.com/sgossner/VCSL), so its samples can be embedded
in a GPL-3.0 project without conditions. Credit is recorded in docs/SOURCES.md
as a courtesy, not as an obligation.

The ten zones below are the recorded pitches, chosen to sit roughly four
semitones apart across the score's range, so no note is resampled more than
about two and a half semitones from something real. Requires ffmpeg for the
conversion to mono 16-bit at 32 kHz.
"""
import argparse
import struct
import subprocess
import urllib.parse
import urllib.request
import wave
from pathlib import Path

RATE = 32000
BASE = 'https://raw.githubusercontent.com/sgossner/VCSL/master/Idiophones/Plucked Idiophones/Kalimba, Tanzania/'

# midi root -> file recorded at that pitch
ZONES = {
    55: 'MBira3_pluck_Main_G3_k6_50_100_rr2.wav',
    58: 'MBira3_pluck_Main_A#3_k18_50_100_rr2.wav',
    62: 'MBira3_pluck_Main_D4_k19_50_100_rr2.wav',
    65: 'MBira3_pluck_Main_F4_k21_50_100_rr2.wav',
    67: 'MBira3_pluck_Main_G4_k22_50_100_rr2.wav',
    71: 'MBira3_pluck_Main_B4_k23_50_100_rr2.wav',
    73: 'MBira3_pluck_Main_C#5_k24_50_100_rr2.wav',
    76: 'MBira3_pluck_Main_E5_k25_50_100_rr2.wav',
    80: 'MBira3_pluck_Main_G#5_k26_50_100_rr2.wav',
    85: 'MBira3_pluck_Main_C#6_k27_50_100_rr2.wav',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('outdir', type=Path)
    parser.add_argument('--seconds', type=float, default=1.9,
                        help='clip length; the score closes every note well before this')
    args = parser.parse_args()
    raw = args.outdir / 'raw'
    raw.mkdir(parents=True, exist_ok=True)

    clips = {}
    for root, name in sorted(ZONES.items()):
        source = raw / name
        if not source.exists():
            url = BASE.replace(' ', '%20').replace(',', '%2C') + urllib.parse.quote(name)
            print(f'fetching {name}')
            with urllib.request.urlopen(url) as response, open(source, 'wb') as out:
                out.write(response.read())
        scratch = args.outdir / f'.{root}.wav'
        subprocess.run([
            'ffmpeg', '-loglevel', 'error', '-y', '-i', str(source),
            '-ac', '1', '-ar', str(RATE), '-sample_fmt', 's16', str(scratch)], check=True)
        with wave.open(str(scratch)) as w:
            frames = w.getnframes()
            data = list(struct.unpack(f'<{frames}h', w.readframes(frames)))
        scratch.unlink()
        # Every one of these clips opens with about a tenth of a second of
        # room tone. Left in, it would delay every note by that much and the
        # rhythm would be gone, so the lead is cut back to just before the
        # pluck rather than to the pluck itself.
        peak = max(abs(v) for v in data) or 1
        onset = next((i for i, v in enumerate(data) if abs(v) > peak * 0.02), 0)
        start = max(0, onset - int(0.002 * RATE))
        data = data[start:start + int(args.seconds * RATE)]
        clips[root] = data

    # One gain across the set. Normalizing each zone to its own peak would
    # flatten the instrument's own register balance, which is part of what
    # makes it sound like one instrument.
    loudest = max(max(abs(v) for v in d) for d in clips.values())
    gain = 0.92 * 32767 / loudest
    fade = int(0.04 * RATE)
    for root, data in sorted(clips.items()):
        for i in range(min(fade, len(data))):
            data[len(data) - 1 - i] = int(data[len(data) - 1 - i] * i / fade)
        target = args.outdir / f'kalimba_{root}.wav'
        with wave.open(str(target), 'wb') as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(b''.join(
                struct.pack('<h', max(-32768, min(32767, int(v * gain)))) for v in data))
        print(f'{target.name}: {len(data) / RATE:.2f} s, {len(data) * 2 / 1024:.0f} KB')


if __name__ == '__main__':
    main()
