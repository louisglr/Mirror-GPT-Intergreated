# MIRROR · Quick start

## Start here

Use a clean, predominantly monophonic vocal. Set **Mode → Manual**, choose your
song's **Key / Scale**, and open **Harmony** to choose intervals. Start with two
voices, then add the other two if the arrangement needs them.

**Harmony Mix** controls the complete harmony bus, including its ambience:
0% = no generated harmonies; 100% = the existing full harmony balance.
It does not turn down the lead. This is an additive harmony-level control,
not a crossfade: for harmonies-only output, set **Lead Level** to Off.
The percentage is linear gain, so 50% is approximately -6 dB on the harmony bus.
Per-voice Level remains independent. Output trims the complete final signal.
The final safety limiter and global Glue still operate on the combined output.

## MIDI

Select **MIDI**, then route MIDI from your DAW to the plugin. The exact routing
depends on the host; in Logic, use the MIDI-controlled Audio Unit effect routing
and provide the vocal as its audio input/side chain as appropriate to your setup.
Do not expect a normal audio insert to receive keyboard notes automatically.
Manual interval menus are disabled in MIDI mode because played notes determine
the pitches. Key / Scale remain saved but do not constrain the MIDI chord.

**Live** applies incoming note events immediately; it does not remove the pitch
engine's audio latency. **Aligned** delays note events by the reported processing
latency to follow a recorded vocal. Change routing/timing/mode between phrases,
then re-play the chord. Up to eight unique notes form the palette for four voices.
When more than eight notes are held, the eight lowest form the active palette.
Note ownership and sustain are independent on all 16 MIDI channels. Repeated
Note On on the same channel/note retriggers; it is not an overlapping-note counter.

## Shape the sound

- **Lead:** level, pan, pitch, width and gentle formant-like tone shaping.
- **Ensemble:** Humanize, Character, Spread and the overall harmony style.
- **Space & Colour:** short diffusion (Ambience), plus global saturation (Glue).
- **Harmony → Advanced:** per-voice Fine, Tone, Sat, Delay, Vibrato and Rate.
- **Original / Refined:** Original preserves v1.5's interpolation behavior.
  Refined smooths the interpolation-method transition near small upward shifts.
  It is not a new formant-preserving resynthesis engine.

Presets preserve Key, Scale, Mode, master Harmony Mix and Output. They do change
voice levels, Lead Level and texture settings. Old sessions load Original and
Harmony Mix 100% automatically. Tracking / Transition stay internally at their
existing fixed settings; nonfunctional knobs are no longer displayed.

Double-click controls to reset. Click values to type. Percentages are displayed
as whole numbers; dB, fine pitch and small delays retain one useful decimal.
Pan / Pitch / Formant / Fine have centred neutral positions; amounts start at
the left. Tab and arrow keys support keyboard adjustment. Hover for help.

## Installation and removal

Evaluation builds are not commercial installers. Do not replace a production
installation while a song is open. Keep a backup of the current plugin and
render important vocal stems before evaluating a different version.

The planned signed installer uses `/Library/Audio/Plug-Ins/Components/MIRROR.component`
and `/Library/Audio/Plug-Ins/VST3/MIRROR.vst3`. Check for older per-user copies in
`~/Library/Audio/Plug-Ins/` before installing: two copies can confuse host scanning.
Do not remove another plugin's files. To uninstall, quit the DAW and move only
the MIRROR bundles you installed to Trash, then restart/rescan the host.

This candidate has not yet been approved for sale. See RELEASE_STATUS.md.
