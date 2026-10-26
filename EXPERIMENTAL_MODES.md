# Instruments / Auto: development and listening notes

## Source: Vocal or Instruments

Choose Source independently of Manual, MIDI or Auto. Use a single isolated
melodic source for Instruments: saxophone, trumpet, flute, trombone or similar.
The supported tracking range is approximately 55–2000 Hz. This is a monophonic
pitch engine; a stereo orchestral mix or played piano chord is not a supported
input for accurate tracking.

Instruments retains more high-frequency content by reducing vocal de-essing
and using register-aware, gentler tone cutoffs. It reduces automatic vocal
formant compensation, follows quiet playing with a lower-level gate, and keeps
more source vibrato/bends around a stable note anchor. Humanize drift is gentler.
Your explicit per-voice Tone, Formant, Fine, Vibrato, Delay and Saturation controls
remain available. No extra compressor, automatic gain rider or look-ahead was
added. The advertised processing latency remains fixed; pitch acquisition and
Auto's note confirmation still take time.

## Harmony: Auto

Set Key and Major / Minor first. Auto considers the seven diatonic triads that
can contain a settled melody note, then scores the available voice placements
for register, spacing, chord continuity and movement from the preceding chord.
It avoids exact collisions with the lead and assigns notes in low-to-high
order across participating voice slots. Each target stays within one octave
of the settled melody. Disabled voices do not consume chord tones.

- Close: compact harmony around the lead.
- Open: more space above and below the melody.
- Air: more octave separation.
- Calm: a new note must settle for about 65 ms before revoicing.
- Flow: about 30 ms.
- Responsive: about 12 ms.

Those are confirmation times after the pitch tracker reports a changed note,
not end-to-end latency figures. The first reliable note enters immediately
without this additional dwell. Low-confidence evidence cannot change a chord;
after roughly 450 ms without reliable pitch, the phrase memory clears. Freeze
holds the chosen harmony. The existing voicing/level gates still control the
audio release, so silence does not produce a held synthetic tone.

Chromatic has no major/minor context, so it uses parallel fifths/fourths and
octaves around the source rather than assuming a key. Auto does not hear your
backing track and cannot know which of several valid chords your song uses.
Use MIDI when the harmony must follow an exact progression.

## UI and compatibility

Source and the general Harmony Mix stay visible on both pages. Auto's Voicing
and Movement controls replace MIDI-specific controls on Main. Harmony shows
the actual selected note for each Auto/MIDI voice; Manual retains its interval
menu. The two values of the legacy `mode` parameter remain unchanged; new
version-hint-3 parameters hold Source and Auto settings. Loading an older state
into a used instance restores Vocal, disables Auto and restores its default
Voicing/Movement rather than inheriting a previous experimental state.

The previous CI screenshot failure was a wrong executable filename, not an
established JUCE rendering limitation. The workflow now checks the actual
`MirrorIntegrationTests` executable and requires six nonempty PNGs.

## Verification and listening

New automated coverage: all 12 roots, major/minor, all three Auto voicings,
all 15 nonempty voice masks, three registers, passing-tone suppression, silence
re-entry, state migration, high-register harmonic-rich tracking, Auto/Instruments
audio at 44.1/48/96 kHz, and functional UI wiring. Passing tests are necessary
but do not establish professional audio quality on recordings.

Compare at matched loudness on an expressive saxophone, a breathy flute and a
brass recording with sharp attacks. Check long sustained notes, vibrato, quiet
entries and octave jumps. Compare Vocal and Instruments with identical voice
settings. In Auto, play slow scale motion, passing notes and large melody
leaps; listen for unwanted chord changes and audible pitch glides. Confirm
MIDI still wins over Auto when the legacy Mode parameter is automated to MIDI.
