#!/usr/bin/env python3
# Copyright (c) 2026 Bruce Blay
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fetch tuned percussion from the Versilian Community Sample Library.

VCSL is CC0 (https://github.com/sgossner/VCSL), so its recordings can be
embedded in a GPL-3.0 project without conditions. Credit is in
docs/SOURCES.md as a courtesy, not as an obligation.

For each instrument this picks zones from the pitches actually recorded,
spread across the instrument's own range, so no note is resampled far from
something real. Clip length is per instrument: a xylophone bar is finished in
under a second and a vibraphone is not, and storing silence is storing flash.

Requires ffmpeg.
"""
import argparse
import json
import re
import struct
import subprocess
import urllib.parse
import urllib.request
import wave
from pathlib import Path

RATE = 32000
TREE = 'https://api.github.com/repos/sgossner/VCSL/git/trees/master?recursive=1'
RAW = 'https://raw.githubusercontent.com/sgossner/VCSL/master/'
NOTE = re.compile(r'_([A-G]#?)(-?\d)[_.]')
STEPS = {'C': 0, 'C#': 1, 'D': 2, 'D#': 3, 'E': 4, 'F': 5, 'F#': 6,
         'G': 7, 'G#': 8, 'A': 9, 'A#': 10, 'B': 11}

# Directory, the velocity or round-robin layer to prefer, the register this
# instrument plays in, how many zones to take inside it, and how long to keep
# of each clip.
#
# The register matters as much as the recording. Taken across a whole bank the
# zones end up a fifth apart and every note is resampled a long way from
# something real; confined to the part of the instrument worth playing, they
# land within a few semitones. It also puts each instrument where it belongs:
# the glockenspiel sings above the staff, the marimba sits under it.
INSTRUMENTS = {
    'kalimba': ('Idiophones/Plucked Idiophones/Kalimba, Tanzania', 'rr2', (55, 86), 8, 1.7),
    'marimba': ('Idiophones/Struck Idiophones/Marimba', 'med', (48, 84), 8, 1.3),
    'xylophone': ('Idiophones/Struck Idiophones/Xylophone/Soft Mallets', 'pp', (65, 96), 7, 0.7),
    'balafon': ('Idiophones/Struck Idiophones/Balafon/Traditional Mallet', 'vl2', (49, 79), 7, 1.0),
    'glockenspiel': ('Idiophones/Struck Idiophones/Glockenspiel', 'loud', (72, 96), 6, 1.5),
    'vibraphone': ('Idiophones/Struck Idiophones/Vibraphone/Soft Mallets', 'v1', (53, 84), 7, 2.0),
    'piano': ('Chordophones/Zithers/Grand Piano, Kawai/Sustains', 'v2', (48, 84), 8, 1.8),
}


def midi_of(name):
    found = NOTE.search(name)
    if not found:
        return None
    return 12 * (int(found.group(2)) + 1) + STEPS[found.group(1)]


def pick_zones(paths, layer, register, count):
    """One file per recorded pitch, then `count` of them spread evenly."""
    by_pitch = {}
    for path in sorted(paths):
        name = path.rsplit('/', 1)[1]
        midi = midi_of(name)
        if midi is None or not register[0] <= midi <= register[1]:
            continue
        # Prefer the requested layer; fall back to whatever exists so a bank
        # that names its layers differently still yields an instrument.
        score = (0 if layer in name else 1, name)
        if midi not in by_pitch or score < by_pitch[midi][0]:
            by_pitch[midi] = (score, path)
    if layer and any(layer in p for _, p in by_pitch.values()):
        by_pitch = {m: v for m, v in by_pitch.items() if layer in v[1]}
    pitches = sorted(by_pitch)
    if len(pitches) <= count:
        return [(m, by_pitch[m][1]) for m in pitches]
    step = (len(pitches) - 1) / (count - 1)
    chosen = sorted({pitches[round(i * step)] for i in range(count)})
    return [(m, by_pitch[m][1]) for m in chosen]


def convert(source, seconds):
    scratch = source.with_suffix('.mono.wav')
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', str(source),
                    '-ac', '1', '-ar', str(RATE), '-sample_fmt', 's16', str(scratch)], check=True)
    with wave.open(str(scratch)) as w:
        frames = w.getnframes()
        data = list(struct.unpack(f'<{frames}h', w.readframes(frames)))
    scratch.unlink()
    # These clips open with room tone before the strike. Left in, it would
    # delay every note by that much and the rhythm would go with it.
    peak = max(abs(v) for v in data) or 1
    onset = next((i for i, v in enumerate(data) if abs(v) > peak * 0.02), 0)
    start = max(0, onset - int(0.002 * RATE))
    return data[start:start + int(seconds * RATE)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('outdir', type=Path)
    parser.add_argument('--only', action='append', help='fetch just this instrument')
    args = parser.parse_args()
    raw = args.outdir / 'raw'
    raw.mkdir(parents=True, exist_ok=True)

    cache = raw / 'tree.json'
    if not cache.exists():
        with urllib.request.urlopen(TREE) as response:
            cache.write_bytes(response.read())
    tree = json.loads(cache.read_text())
    wavs = [e['path'] for e in tree['tree'] if e['path'].lower().endswith('.wav')]

    for instrument, (directory, layer, register, count, seconds) in sorted(INSTRUMENTS.items()):
        if args.only and instrument not in args.only:
            continue
        paths = [p for p in wavs if p.startswith(directory + '/')]
        if not paths:
            raise SystemExit(f'{instrument}: nothing under {directory}')
        zones = pick_zones(paths, layer, register, count)
        clips = {}
        for midi, path in zones:
            source = raw / path.rsplit('/', 1)[1]
            if not source.exists():
                url = RAW + urllib.parse.quote(path)
                print(f'fetching {source.name}')
                with urllib.request.urlopen(url) as response:
                    source.write_bytes(response.read())
            clips[midi] = convert(source, seconds)
        # One gain per instrument. Normalizing each zone to its own peak would
        # flatten the register balance that makes a bank sound like one thing.
        loudest = max(max(abs(v) for v in d) for d in clips.values())
        gain = 0.92 * 32767 / loudest
        fade = int(0.03 * RATE)
        total = 0
        for midi, data in sorted(clips.items()):
            for i in range(min(fade, len(data))):
                data[len(data) - 1 - i] = int(data[len(data) - 1 - i] * i / fade)
            target = args.outdir / f'{instrument}_{midi}.wav'
            with wave.open(str(target), 'wb') as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(RATE)
                w.writeframes(b''.join(
                    struct.pack('<h', max(-32768, min(32767, int(v * gain)))) for v in data))
            total += len(data) * 2
        span = f'{min(clips)}-{max(clips)}'
        print(f'{instrument}: {len(clips)} zones, MIDI {span}, {total / 1024:.0f} KB')


if __name__ == '__main__':
    main()
