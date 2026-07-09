# TuneTackle

One-knob vocal mixing plugin: pitch tuning, cleanup, EQ, compression, air and
space in a single chain, balanced by the **Polish** control.

## Formats

- **VST3** — works in Ableton Live, FL Studio, Cubase, Reaper, Studio One, etc.
- **AU** — works in Logic Pro and GarageBand (passes `auval`).
- **Standalone** — run it directly and sing into any microphone.

Built plugins are installed automatically after every build:

- `~/Library/Audio/Plug-Ins/VST3/TuneTackle.vst3`
- `~/Library/Audio/Plug-Ins/Components/TuneTackle.component`
- Standalone app: `build/TuneTackle_artefacts/Release/Standalone/TuneTackle.app`

## Building

Requires CMake 3.22+ and the Xcode command line tools. JUCE is vendored in
`JUCE/`.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

## Signal chain

`Input → Clean (rumble filter + noise gate) → Tune (autocorrelation pitch
detection + dual-tap shifter) → EQ (mud cut + presence) → Compress →
Air (high shelf) → Space (reverb) → Output`

**Polish** scales the effective amount of every module at once (100% of the
knob values at Polish = 72%). **Before/After** bypasses the whole chain for
instant comparison. The waveform bars, level readout, and key detection stay
live in both modes.

## Presets

Radio Ready, Natural, Pop Sheen, Trap Hard Tune, R&B Smooth, Rock Grit,
Podcast Voice, Intimate Booth.

## Notes

- In the standalone app, audio input starts muted to avoid feedback — click
  **Settings…** in the banner (or Options → Audio/MIDI Settings) to enable
  your microphone and pick an output device.
- The key readout listens to the incoming vocal and estimates the musical key
  once it has heard enough pitched material; the BPM readout shows the host
  tempo when running inside a DAW.
