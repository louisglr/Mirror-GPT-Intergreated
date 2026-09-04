# MIRROR — Quality Update 14

MIRROR is a four-voice vocal harmonizer for macOS. It has Manual and MIDI
harmonies, per-voice colour controls, and a deliberately compact, musical
workflow.

## What is improved in this version

- MIDI allocation only treats audible voices as candidates, so disabled voices
  no longer steal chord notes from the active stack.
- MIDI note changes use a short release fade and stable voice-leading instead
  of hard cuts between grain states.
- The harmony engine now uses a phase-locked, pitch-synchronous overlap-add
  reader. Its active grains share the same source phase, so fractional shifts
  such as +84 cents and 1.5x no longer collapse back toward the input pitch.
- The engine reports a conservative, fixed nominal group latency (about 36 ms
  at 48 kHz) and aligns the unison/dry path to the same delayed source sample,
  rather than using the old half-grain estimate.
- Voice activation, ambience bypass and invalid-input recovery are protected
  against stale circular-buffer data and long-session crackle.
- Harmony tone is register-aware: low voices retain body, while high/airy
  voices receive smoother filtering and de-essing.
- Saturation uses a gentler sub-sample reconstruction curve and one
  stereo-linked output safety stage to keep dense stacks wide and controlled.
- Filter and de-essing time constants remain consistent at 44.1, 48 and
  96 kHz.

## Using MIRROR

1. Choose **Manual** to generate scale-aware intervals, or **MIDI** to let
   incoming MIDI notes select the harmony pitches.
2. In MIDI mode, hold a chord. Active voices receive the musical chord tones;
   turn on more voices when you want an expanded stack.
3. Open **Harmony → Advanced** for Fine Tune, Tone, Saturation, Micro Delay
   and Vibrato per voice.
4. Keep Humanize modest for the smoothest stack. It now affects slow pitch and
   amplitude drift only; it does not modulate the short delay line.

## Build locally

Requirements: macOS, Xcode or Xcode Command Line Tools, CMake 3.22+, and an
internet connection for JUCE on the first configure.

```sh
xcode-select --install
cmake -S . -B build -G Xcode
cmake --build build --config Release
```

The AU and VST3 are placed under `build/Mirror_artefacts/Release/`.

## Install in Logic Pro

```sh
ditto "build/Mirror_artefacts/Release/AU/MIRROR.component" "$HOME/Library/Audio/Plug-Ins/Components/MIRROR.component"
codesign --force --deep --sign - "$HOME/Library/Audio/Plug-Ins/Components/MIRROR.component"
killall AudioComponentRegistrar
```

Restart Logic after installing.

## Test the release candidate

Use the focused checklist in [TESTING.md](TESTING.md) before treating this as
a final release. In particular, test a C+E MIDI dyad with only Voice 1–2 on,
Dry Pitch automation through zero, long playback, and a 64-sample buffer.

## Quality note

MIRROR uses original time-domain, phase-locked pitch-synchronous DSP. The
focus is smooth vocal stacking rather than replicating a specific artist,
production or proprietary processor. Moderate intervals will always be the
most natural setting; extreme shifts require more audible processing in any
real-time engine.
