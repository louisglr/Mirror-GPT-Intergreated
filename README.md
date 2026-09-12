# MIЯЯOЯ — v1.5.0 Release Candidate

MIЯЯOЯ is a four-voice vocal harmonizer for macOS. It combines scale-aware
Manual harmonies with playable MIDI harmony, per-voice tone shaping and a
compact two-page workflow.

This branch is a release candidate for listening and compatibility testing. It
is not the final signed commercial installer.

## What changed in v1.5

### Sound and long-session stability

- The pitch-synchronous engine now keeps a continuous sample phase when the
  detected vocal period changes. New grains follow the strongest active grain
  and reject ambiguous pitch marks, reducing renewed combing and scratchy
  transitions on real vocals.
- A one-cent Fine Tune value now reaches the pitch engine. Exact unison remains
  a transparent, fixed-latency path without the former ±3.5-cent dead zone.
- The band-limited reader interpolates both fractional phase and anti-alias
  cutoff, removing small timbre steps during glides and vibrato.
- Saturation now uses first-order antiderivative anti-aliasing (ADAA),
  zero-centred asymmetry and DC removal for smoother colour without adding a
  hidden latency stage.
- Transport restarts, seeks and loop jumps clear old pitch, grain, delay and
  filter history before rendering the new position.
- Dry Pitch preserves the input stereo side around the shifted centre instead
  of collapsing every non-zero setting to mono.

### CPU and response

- YIN pitch analysis is divided into deterministic, bounded slices. The old
  low-note analysis could create a roughly 177,000-operation burst every
  10 ms; the new path performs at most 640 small analysis units per input
  sample and allocates nothing in the callback.
- The locked 100% Tracking mapping was reversed in the old implementation. It
  now selects the intended fast 7 ms correction response.
- Manual voice transitions use a responsive 38 ms target instead of the old
  155 ms path. MIDI keeps a faster 26 ms target with click-free retarget fades.

### MIDI and workflow

- Open and Wide now reshape the notes of a complete four-note chord; they no
  longer affect only duplicated notes.
- Chord reassignment fades the affected voice out, changes its target at zero,
  then fades it back in. Unchanged chord tones stay stable.
- **MIDI Timing** offers two explicit behaviours:
  - **Live**: lowest playing latency; MIDI is applied at the incoming sample.
  - **Aligned**: chord changes are delayed by MIЯЯOЯ's reported granular
    latency so they address the matching recorded vocal syllable.
- The disabled-voice Solo edge case no longer silences every audible voice.
- Presets use original product names—**Glass Bloom**, **Fractured Light** and
  **Night Choir**—and include their Advanced voice settings. Key and Scale are
  never changed by a preset.
- The preset field honestly shows **SELECT / CUSTOM** after manual edits or a
  restored session. MIDI controls and Output Gain are exposed in the UI;
  Tracking, Transition and Harmony are visibly locked to the DSP's 100% path.

## Build locally

Requirements: macOS, Xcode or Xcode Command Line Tools, CMake 3.22+, and an
internet connection on the first configure so CMake can fetch JUCE 9.0.1.

```sh
xcode-select --install
cmake -S . -B build -G Xcode
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

AU, VST3 and Standalone artefacts are placed under
`build/Mirror_artefacts/Release/`.

## Install a local test build in Logic Pro

```sh
ditto "build/Mirror_artefacts/Release/AU/MIRROR.component" \
  "$HOME/Library/Audio/Plug-Ins/Components/MIRROR.component"
codesign --force --deep --sign - \
  "$HOME/Library/Audio/Plug-Ins/Components/MIRROR.component"
killall AudioComponentRegistrar
```

Restart Logic after installing.

## Before calling it a product release

Run [TESTING.md](TESTING.md) and complete [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md).
A commercial macOS download still needs Developer ID signing, notarisation,
an installer/uninstaller, licence and privacy/support material. The GitHub
test artefact is ad-hoc signed for evaluation; Gatekeeper should not be treated
as complete until the final Apple credentials are configured.

## Quality scope

MIЯЯOЯ uses original time-domain, phase-locked pitch-synchronous DSP. Its goal
is a cohesive, expressive processed-vocal sound; it does not copy a proprietary
processor, endorse an artist, or guarantee an exact recording's sound on every
source. Moderate intervals and a clean, well-recorded lead remain the most
natural starting point for any real-time harmonizer.
