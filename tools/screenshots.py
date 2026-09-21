#!/usr/bin/env python3
"""Capture actual renderer frames and assemble the README lead image (C++17 required)."""
import struct
import subprocess
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/images'
# Character, seed, elapsed display frames. All captures use real engine strikes.
SHOTS = {'split': (0, 99, 210), 'spark': (1, 99, 210), 'ring': (2, 99, 210),
         'trace': (3, 17, 210), 'fold': (4, 17, 210), 'bloom': (5, 99, 210),
         'snap': (6, 17, 210)}

def png(path, width, height, pixels):
    def chunk(kind, data):
        return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data))
    scanlines = b''.join(b'\0' + pixels[y*width*3:(y+1)*width*3] for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!2I5B', width, height, 8, 2, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(scanlines, 9)) + chunk(b'IEND', b''))

with tempfile.TemporaryDirectory() as directory:
    temp = Path(directory)
    binary = temp / 'preview'
    subprocess.run(['c++', '-std=c++17', '-O2', str(ROOT / 'tools/visual_preview.cpp'), '-o', str(binary)], check=True)
    captures = {}
    for name, (family, seed, frames) in SHOTS.items():
        ppm = temp / f'{name}.ppm'
        subprocess.run([str(binary), str(ppm), str(family), str(frames), str(seed)], check=True)
        captures[name] = ppm.read_bytes().split(b'\n', 3)[3]
        assert len(captures[name]) == 240 * 135 * 3
        png(OUT / f'{name}.png', 240, 135, captures[name])
    # 2× native pixels, with a small consistent gutter. Never interpolate artwork.
    width, height, gap = 1008, 588, 16
    pixels = bytearray(bytes([237, 236, 229]) * width * height)
    for index, name in enumerate(['ring', 'bloom', 'split', 'fold']):
        x0, y0 = gap + (index % 2) * (480 + gap), gap + (index // 2) * (270 + gap)
        data = captures[name]
        for y in range(270):
            for x in range(480):
                source = ((y // 2) * 240 + x // 2) * 3
                target = ((y0 + y) * width + x0 + x) * 3
                pixels[target:target+3] = data[source:source+3]
    png(OUT / 'characters.png', width, height, pixels)
