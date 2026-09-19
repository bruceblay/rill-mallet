# Rill Chime

A sampled sibling to [Rill](../rill). Same composer, different voice: instead of
synthesizing each note, it plays multisampled tuned percussion — kalimba first,
with xylophone and wooden bars to follow.

## Why samples

Rill's seven timbres are built per sample on the device, which caps how complex
a single note can be. A recorded or offline-rendered note carries detail no
per-sample budget on this board can afford, and playing it back costs almost
nothing: a read pointer, a resampling ratio and linear interpolation.

The clips live in flash and are read from there a sample at a time. RAM is
unchanged from Rill.

## Multisampling

One zone every four semitones from G3 to G6 (MIDI 55 to 91), which covers the
score's melody range (60–91) and its support range (55–72). A note picks the
nearest zone and resamples from that zone's root, so nothing is ever bent more
than two semitones. Stretching one clip across the whole range is what makes a
sampled instrument sound like a cartoon at its edges.

## The samples are placeholders

`tools/render_kalimba.py` synthesizes the current set: a kalimba tine modelled
as a bar clamped at one end, with partials near 1 : 6.18 : 17.3 : 33.8, each
given its own decay, plus a filtered strike transient and a body resonance.
They are rendered, not recorded.

Anything recorded that replaces them must carry a license compatible with this
project's GPL-3.0. Record the source in `docs/SOURCES.md` before embedding it.

```sh
python tools/render_kalimba.py build/samples
python tools/embed_samples.py build/samples
```

`embed_samples.py` takes `<instrument>_<root midi>.wav`, mono 16-bit PCM at
32 kHz, and writes `src/Samples.h`. The root in the filename is the pitch the
clip was made at; get it wrong and the whole zone is detuned.

## Build and install

```sh
python -m pip install -r requirements-dev.txt
pio run
python tools/flash.py --port YOUR_DEVICE_PORT
```

Host audition, without hardware:

```sh
c++ -std=c++17 -O2 tools/render.cpp -o build/render
build/render build/chime.wav 60 42
```

Arguments are output path, seconds, seed, and optional first family (0–6).

## Layout

- `src/Chime.h` — Rill's score with a sampled voice layer
- `src/Samples.h` — generated; the multisample zones
- `src/Light.h` — visuals, currently Rill's, to be replaced
- `tools/render_kalimba.py` — renders the placeholder set
- `tools/embed_samples.py` — WAV to header

## License

GPL-3.0-or-later, following Rill. See [LICENSE](LICENSE).
