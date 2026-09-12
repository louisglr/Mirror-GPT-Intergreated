# MIЯЯOЯ v1.5.0-rc1 — listening, MIDI and stability test

Do not replace a known-good production build until this checklist passes. Start
with a clean Logic project at 48 kHz / 64 samples, then repeat the marked tests
at 44.1 and 96 kHz.

## 1. Automated build gate

- Configure and build Release for arm64 and x86_64.
- Run `ctest --test-dir build -C Release --output-on-failure`.
- Confirm the `MirrorDspSmoke` test passes across 22.05, 24, 29.4, 44.1, 48,
  96 and 192 kHz, including the saturation reference and invalid-state tests.
- Validate the MIDI-controlled Audio Unit with `auval -v aumf MIRR LGRL`.
- Open AU and VST3 once and confirm the 700 × 520 editor renders.

## 2. Five-minute functional check

1. Insert MIЯЯOЯ on a mono recording and then a true stereo recording. The
   default Manual stack must arrive as one coherent sound.
2. Change Dry Pitch 0 → +1 → 0. The lead must remain stereo, return without an
   old fragment and produce no click at zero.
3. Toggle the host's plug-in Bypass repeatedly on a sharp transient. Timing
   must remain sample-aligned with the declared latency, with no old delay
   fragment or hard edge when the effect returns.
4. Enable only Voice 1 and 2. Solo a disabled Voice 3; Voice 1 and 2 must keep
   playing. Solo an enabled voice; only that voice should remain.
5. Open Harmony → Advanced. Fine, Tone, Sat, Delay, Vibrato and Vib Rate must
   appear for all four voices and remain editable.
6. Select a preset, then change an audible parameter. The preset field must
   read SELECT / CUSTOM. Re-select the preset and confirm its Advanced values
   restore. Key and Scale must not change.

## 3. Pitch and sound quality

- On a sustained clean vocal, compare Unison, +3, +5, +7 and +12. Each harmony
  must hold its intended centre without renewed buzz at grain boundaries.
- Sweep Fine Tune slowly through −10…+10 cents, explicitly checking ±1 cent.
  Movement must be continuous; no central dead band or stepped timbre.
- Move Humanize, Character, Formant, Tone, Saturation and Vibrato separately,
  then together. Listen for movement—not saw-wave pulsing, zipper noise or
  periodic crackle.
- Use a bright soprano at +12 and a low baritone at −12. Check sibilance,
  aliasing, body and pitch centre. Extreme shifts may have processing character,
  but must stay finite, controlled and musically usable.
- Compare one, two and four voices at matched loudness. Adding voices should
  form a single stack rather than four unrelated copies; the centre must remain
  stable and the output limiter must not narrow left/right independently.
- Test Global Saturation and per-voice Sat at 0, 25, 50 and 100%. No DC thump,
  level runaway or brittle high-frequency foldback is acceptable.

## 4. MIDI mode

- Hold C4–E4 with only Voice 1–2 enabled. The audible pitches must be C and E;
  disabled voices may not consume notes.
- Hold C4–E4–G4–B4 and compare Close, Open and Wide. All three must produce
  audibly different registers even with four notes and four voices.
- Compare Root, 1st, 2nd, 3rd and Auto inversions. Auto should minimise motion;
  fixed inversions should be deterministic.
- Change one note in a held four-note chord. Unchanged tones should remain
  stable while only reassigned voices fade/retarget. Repeat with sustain pedal.
- Send repeated note-on, note-off, CC64, All Notes Off and All Sound Off. There
  must be no stuck tone, hard click or allocation-like overload.
- Compare **MIDI Timing: Live** while playing to **Aligned** on a recorded vocal.
  Live should feel immediate. Aligned should place the chord change on the
  matching processed syllable after accounting for the reported latency.
- Automate the timing mode only while no notes are held; confirm no delayed
  event appears afterwards.

## 5. Transport and session state

- Loop across a phrase boundary for five minutes. No audio from the previous
  loop may leak into the new loop and no crackle may build over time.
- Seek forward/backward while playing, stop/start, and restart from bar 1.
  Pitch/grain/delay history must re-prime silently.
- Save/reopen a project in Manual and MIDI modes. All APVTS parameters must
  restore; the non-state-backed preset selector should honestly show Custom.
- Automate Enable, Solo, Dry Pitch, Formant, Tone, Sat, Delay and Output Gain.
  No stale audio, NaN propagation or abrupt uncontrolled edge is acceptable.
- Sweep Dry Pan, Dry Pitch, MIDI Velocity, Vibrato and Vib Rate while audio is
  running. Their smoothing must prevent block-edge clicks and zipper steps.

## 6. Long-session and performance matrix

Run at least 30 minutes per critical configuration:

| Sample rate | Buffer | Voices | Required exercise |
|---|---:|---:|---|
| 44.1 kHz | 64 | 4 | Manual, Humanize/Character movement |
| 48 kHz | 32 or 64 | 4 | MIDI chord changes, Live and Aligned |
| 48 kHz | 128 | 4 | Loop/seek/stop-start stability |
| 96 kHz | 64 | 4 | Bright vocal, +12, saturation sweep |
| 96 kHz | 256 | 4 | 30-minute render and null/finite check |

Record Logic's CPU meter and any overload timestamp. A repeatable overload at
32 samples is important data; a single unrelated system spike is not evidence
of a plugin leak. Report DAW version, Mac model/chip, sample rate, buffer,
source, mode, preset and exact control values with every failure.

## 7. Release acceptance

The candidate is accepted only when:

- automated DSP tests, universal build and AU validation are green;
- no repeatable crackle or stale-history event occurs in the long-session matrix;
- MIDI note ownership, sustain, panic, voicing and timing all pass;
- preset/UI behaviour matches the audio state;
- at least three different vocal recordings have passed blind level-matched
  comparisons against the previous known-good build.
