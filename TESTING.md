# MIRROR v1.4.0-rc1 — listening and stability test

This is a release candidate for real musical testing, not a claim that every
vocal source or extreme interval is final. Use a clean Logic session at 48 kHz
before replacing a working production version.

## Five-minute functional check

1. Insert MIRROR on a recorded mono or stereo vocal. Confirm that the default
   Manual stack is delayed as one coherent sound, not dry plus late harmonies.
2. Switch to **MIDI**, turn on only Voice 1 and Voice 2, and hold C4 + E4.
   You should hear C and E—not C plus an octave-C with E assigned to an
   inaudible voice.
3. Change between two held MIDI chords. Listen for smooth 15–30 ms releases,
   rather than clicks or a jump to unison at note-off.
4. Automate **Dry Pitch** from 0 to +3 semitones and back to 0. It should
   crossfade smoothly; it must not replay old audio when it returns to zero.
5. Toggle a harmony voice off and on during playback. It should fade cleanly
   without a stale fragment from an earlier phrase.

## Sound check

- Compare one active voice, two voices and four voices. Lower voices should
  retain body; high voices should be smoother and less sibilant.
- With a sustained, clean vocal, compare Unison, +3, +5 and +12. Each harmony
  must clearly hold its intended note; it must not pull back toward the input
  note after a few seconds. This specifically checks the new phase-locked
  pitch-synchronous reader.
- Raise Humanize and Character gradually. There should be gentle movement,
  never a fast saw-wave/pulsing artifact.
- Test a bright vocal at +12 semitones and a low vocal at -12 semitones. Some
  processing character is normal at extremes; report any metallic aliasing or
  loss of pitch centre.
- Test Global Saturation at 0, 50 and 100 percent. It should add colour, not
  narrow the stereo image or make the output harsh.

## Optional pitch/latency lab check

- Feed a 200 Hz sine wave, set Dry to zero and use one active harmony voice.
  Check Unison, +1, +7 and +12 semitones with a tuner or spectrum analyser.
  The output should follow the selected interval continuously, including a
  slow Fine Tune sweep; it should not jump back to 200 Hz between grain hops.
- At 48 kHz the reported nominal group latency is about 36 ms. Record a short
  transient with Dry and one Unison harmony, then check that they remain time
  aligned. Logic should compensate a normal playback path automatically; live
  monitoring still has the plug-in's real-time group latency.

## Stability check

- Start at a 64-sample buffer; also test 128/256 if the host reports an
  overload. The current pitch detector still does its analysis in the audio
  thread, so low-buffer performance is an explicit test target.
- Leave the plug-in open for at least 30 minutes while looping a vocal and
  changing Voice enable, MIDI notes, Humanize and Character.
- Repeat the critical checks at 44.1, 48 and 96 kHz if available.

Please note the DAW, sample rate, buffer size, source type, exact controls and
the time into playback if anything crackles, shifts register or loses a note.
