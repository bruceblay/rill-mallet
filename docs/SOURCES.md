# Sample sources

All recordings come from the [Versilian Community Sample
Library](https://github.com/sgossner/VCSL), which is CC0 and so carries no
conditions at all. The credit here is a courtesy, not an obligation.

| Instrument | VCSL bank | Layer | Zones | Register |
| --- | --- | --- | --- | --- |
| Balafon | Struck Idiophones / Balafon / Traditional Mallet | vl2 | 6 | MIDI 47–79 |
| Glockenspiel | Struck Idiophones / Glockenspiel | loud | 5 | MIDI 70–98 |
| Kalimba | Plucked Idiophones / Kalimba, Tanzania | rr2 | 8 | MIDI 53–87 |
| Marimba | Struck Idiophones / Marimba | med | 6 | MIDI 51–86 |
| Piano | Zithers / Grand Piano, Kawai / Sustains | v2 | 8 | MIDI 46–86 |
| Vibraphone | Struck Idiophones / Vibraphone / Soft Mallets | v1 | 7 | MIDI 53–78 |
| Xylophone | Struck Idiophones / Xylophone / Soft Mallets | pp | 6 | MIDI 65–98 |

`tools/fetch_vcsl.py` holds the selection: which bank, which velocity or
round-robin layer, the register to confine zones to, and how much of each clip
to keep. Re-running it rebuilds the set from scratch.

Anything else recorded that lands in `src/Samples.h` needs a license
compatible with GPL-3.0-or-later — CC0 and CC-BY work, anything
non-commercial does not. Record the origin, the license and the date here
before embedding it.

`tools/render_kalimba.py` remains as a synthesized fallback and as a way to
prototype an instrument no free recording exists for.
