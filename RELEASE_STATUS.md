# MIRROR 1.6.0 development candidate — not cleared for sale

## Scope of this pass

- New vector JUCE skin and 840 × 640 two-page layout; no screenshot backgrounds.
- Persistent, automatable Harmony Mix and Output on both pages.
- Separate appended `harmonyMix` parameter (version hint 2), unity by default.
  Legacy `harmony` remains ignored so old automation cannot change old mixes.
- Old state migration restores Mix=100% and Pitch Engine=Original, including
  when an old state is loaded into an already-used instance.
- Optional Refined interpolation with a continuous Hermite/sinc transition.
  New instances default to Refined; legacy songs remain Original.
- Shared channel-aware MIDI ownership in Live/Aligned modes, channel-specific
  sustain and CC120/121/123 handling; retained overflow note ownership.
- Queue overload fails closed instead of losing a release and hanging a voice.
- Long SysEx discarded before constructing owning MIDI messages on audio thread.
- Added full-processor, state migration, MIDI, UI and screenshot regression tests.
- Tighter one-cent test and explicit interpolation-boundary regression.
- A manually gated signing/notarisation/installer script. It is not connected to
  automatic publication and has not been run with release credentials.

## Verification status

The source has been edited locally. The host does not currently have Xcode / a
working C++ compiler or CMake. A compiled JUCE build, CTest run and actual editor
snapshots remain REQUIRED; the added tests are not evidence of a passing run.
Local checks completed: packaging shell syntax; source-level preservation of
the original parameter IDs; and an independent numerical model of the resampler
boundary. For the model's bright synthetic stimulus, the maximum local boundary
difference fell from 0.123773367 to 0.000007253. This is not a compiled-plugin
measurement, listening result, CPU benchmark or general audio-quality score.
GitHub upload/build requires the owner's approval. No installed plugin was
replaced and the v1.5.0 reference source/artifact was left unchanged.

## Hard release gates

1. Build both architectures; CTest + AU validation + independent VST3 host scan.
2. Inspect real captured Main/MIDI/Harmony/Advanced pages, text entry, keyboard
   focus, tooltips and DAW display scaling. Test minimum supported macOS.
3. Level-matched vocal listening on at least three singers, including the two
   production songs. Verify Original playback against v1.5; document differences
   from corrected multichannel/overload MIDI handling rather than promising
   bit-identical behavior for every MIDI edge case.
4. 30–60 minute real-time low-buffer tests and offline bounce; measure callback
   peaks, not only average CPU. Confirm no crackle builds over time.
5. Developer ID Application + Installer identities and an Apple notarisation
   profile. Sign, notarise, staple and validate the exact tested build. Test
   install, DAW rescan, removal and rollback on a clean Mac.
6. Confirm an appropriate JUCE licence, third-party notices and branding rights.
   Owner-approved EULA, privacy, support contact, system requirements and refund
   terms. No artist endorsements or guaranteed imitation claims.
7. Decide checkout/download delivery and whether activation is needed. No DRM,
   subscription, telemetry, account signup or payment system was added implicitly.

## Deliberately deferred, not silently "fixed"

- Full formant-preserving resynthesis: the existing Formant control remains a
  four-band tonal shaper. A replacement needs its own vocal A/B qualification.
- Changing MIDI glide/fade behavior: retain current sound until audio regression
  references exist; the existing 26 ms glide can still be audible on wide jumps.
- Auto harmony composition, new preset library and additional effects.
- Windows/AAX support. Current distribution target remains macOS AU + VST3.

## Owner decisions needed

- Approve an isolated GitHub test branch/build (not a public release).
- Confirm availability of Apple Developer credentials and JUCE licensing.
- Provide a support address / seller identity, and choose the sales-delivery
  route and licence policy before legal copy and an installer can be finalized.

Sources checked 2026-09-24:
- Apple Developer ID: https://developer.apple.com/developer-id/
- Apple certificates: https://developer.apple.com/help/account/certificates/create-developer-id-certificates
- JUCE 9 licence: https://juce.com/legal/juce-9-licence/

These are release-engineering gates, not a legal opinion or certification.
