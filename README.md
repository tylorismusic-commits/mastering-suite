# Mastering Suite

A fixed-order stereo mastering chain with calibrated metering, built on
`hearth-dsp` and the Comp Suite/Patina visual language. Ships as CLAP and LV2
with a self-contained loudness analyser.

![Mastering Suite panel](docs/mastering-suite.png)

## Chain

`Input → EQ → Bus Compressor → Patina Color → Density → Peak Limiter → Output`

- **EQ** — smooth low shelf, wide mid bell, high shelf and high-pass. Broad,
  non-resonant shapes intended for mastering moves, with the combined curve
  drawn over the spectrum.
- **Bus Compressor** — stereo-linked feed-forward bus compressor with ratio,
  knee, side-chain high-pass, parallel mix and optional auto gain.
- **Patina Color** — single-band extraction of Patina's colour engine: five
  models (Valve, Console, Bus, Tape, Transformer), drive, character, tone, mix,
  level and Off/Soft/Hard output clipping.
- **Density** — original harmonic-density stage built on the asymmetric ADAA
  shaper. Input, density and output, with no proprietary curve.
- **Peak Limiter** — input gain to drive into the ceiling, five-millisecond
  lookahead, true-peak detection and a selectable ceiling. Latency is 240
  samples at 48 kHz and stays constant when the module is bypassed.

## Typed values

Right-click any knob to open a value field. Type a value with its unit, such
as `-6 dB`, `120 Hz`, `50%`, or `Soft`; Enter commits and Escape cancels.

## Metering

- Input and output sample peak, per channel
- Output true peak (dBTP) from a 4x detector
- Integrated, short-term and momentary LUFS, plus LRA (EBU R128, vendored
  `libebur128` 1.2.6)
- Stereo correlation
- Compressor and limiter gain reduction
- 128-bin log FFT spectrum with the live EQ curve
- Meter reset button for the integrated/loudness history

The display switches between **Spectrum**, **Loudness** and **GR** with the
tabs inside the bezel. The header shows integrated LUFS at all times.

## Build

Dependencies: CMake 3.16+, a C/C++17 compiler, Cairo, FreeType, X11 and the
LV2 headers. `lilv` is optional and only needed for the LV2 host test.
`hearth-dsp` is a submodule; `libebur128` is vendored.

```sh
git submodule update --init --recursive
cmake -S . -B build -DCLAP_DIR="$HOME/.clap" -DLV2_DIR="$HOME/.lv2"
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --install build
```

The install step writes `MasteringSuite.clap` with its resources into
`~/.clap`, and `mastering-suite.lv2` into `~/.lv2`.

## Release bundle

```sh
cmake --build build --target dist
```

This creates `build/dist/MasteringSuite-0.4.0-linux-x86_64.tar.gz` with:

- `MasteringSuite.clap` and `MasteringSuite.resources/`
- `mastering-suite.lv2/` (plugin, UI, TTL, presets, font)
- `LICENSE`, `THIRD_PARTY_NOTICES.md`, `README.md`

`dist-clap` and `dist-lv2` produce format-specific tarballs.

## Tests

- `mastering_dsp_test` — EQ unity, compressor gain reduction, limiter ceiling
  and true-peak reduction, colour/density stability, EBU R128 calibration,
  LUFS reset, no fade-in on fresh load, spectrum population, latency.
- `mastering_clap_host_test` — bundle load, factory, parameter/state
  round-trip, host text, audio processing, X11 editor embedding, click and
  drag interaction. Runs under `xvfb-run` when available.
- `mastering_lv2_host` — lilv bundle discovery, port layout, range/default
  consistency, presets, audio processing, meters, spectrum, reset and
  latency.
- `mastering_lv2_ui_host` — direct UI load, embedding, preferred size,
  resize handshake, meter/spectrum port events, parameter writes and the
  meter-reset pulse.
- `mastering_ttl` — fails if the committed TTL files drift from the code.

## Notes

- Formats: CLAP and LV2/X11, built from one DSP and panel source.
- No Sonnox Inflator, FabFilter Pro-L 2 or Tokyo Dawn SlickEQ code, curves or
  product names are used. The colour and density stages are original shapes.
- See `THIRD_PARTY_NOTICES.md` for licences and attribution.

## Licence

GPL-3.0-or-later. See `LICENSE`.
