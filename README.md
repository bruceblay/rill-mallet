# Rill Chime

A sampled sibling to [Rill](../rill). Same composer, different voice: instead of
synthesizing each note, it plays multisampled tuned percussion. Seven
instruments, one per generation: balafon, glockenspiel, kalimba, marimba,
piano, vibraphone and xylophone.

## Why samples

Rill's seven timbres are built per sample on the device, which caps how complex
a single note can be. A recorded or offline-rendered note carries detail no
per-sample budget on this board can afford, and playing it back costs almost
nothing: a read pointer, a resampling ratio and linear interpolation.

The clips live in flash and are read from there a sample at a time. RAM is
unchanged from Rill.

## Multisampling

Five to eight zones per instrument, at pitches it was actually recorded at. A
note picks the nearest zone and resamples from that zone's root, so nothing is
bent more than a few semitones. Stretching one clip across a whole range is
what makes a sampled instrument sound like a cartoon at its edges.

Each instrument also brings its own register. Rill folds its melody into a
fixed MIDI 60–91 because its synthesis can play anything; a glockenspiel has
no low F and a marimba has no top C. The melody takes the top two thirds of
whatever the instrument's recorded range is and the support part the bottom
half, so the two never sit on top of each other.

## The samples

All seven come from the [Versilian Community Sample
Library](https://github.com/sgossner/VCSL), which is CC0 and so carries no
conditions. `tools/fetch_vcsl.py` picks the zones, downloads them, converts to
mono 16-bit at 32 kHz, cuts the room tone each clip opens with, and normalizes
each instrument with a single gain so it keeps its own register balance.

Clip length is per instrument. A xylophone bar is finished inside a second and
a vibraphone is not, and storing silence is storing flash.

```sh
python tools/fetch_vcsl.py build/samples
python tools/embed_samples.py build/samples
```

`tools/render_kalimba.py` synthesizes a set instead — a tine modelled as a bar
clamped at one end, partials near 1 : 6.18 : 17.3 : 33.8 with separate decays,
a filtered strike and a body resonance. It is kept for prototyping an
instrument no free recording exists for. Anything recorded that goes into
`src/Samples.h` needs a license compatible with GPL-3.0; see
[docs/SOURCES.md](docs/SOURCES.md).

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
- `src/Samples.h` — generated; roughly 4 MB of multisample zones
- `partitions_chime.csv` — one factory app partition instead of two OTA slots, for the samples
- `src/Light.h` — visuals, currently Rill's, to be replaced
- `tools/render_kalimba.py` — renders the placeholder set
- `tools/embed_samples.py` — WAV to header

## License

GPL-3.0-or-later, following Rill. See [LICENSE](LICENSE).
