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

GitHub Actions run 37 (2026-09-29) completed successfully on the release
candidate: universal arm64/x86_64 AU, VST3 and Standalone Release build; DSP and
full-processor regression suites; ad-hoc bundle-signature verification; AU
validation; packaged AU/VST3 artifact and SHA-256 manifest. The tested
macOS ZIP is available as workflow artifact 11017008790 (30-day retention at
the time of this run). The host used for this review still has no active Xcode
toolchain, so local build/install was not available.

The automated UI functional tests passed. The hosted off-screen screenshot
capture step is best-effort because this runner previously exited without
diagnostics; visual screenshots were not included in the uploaded QA artifact,
so manually inspect the editor in a DAW. An independent third-party VST3 host
scan, manual Logic installation, audio listening against the previous build,
and long-duration / low-buffer stress testing remain outstanding. The test
artifact is not signed with a Developer ID, notarised, or suitable as a
commercial installer.

No installed plugin was replaced and the v1.5.0 source/reference artifact was
left unchanged.
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

- Test-branch upload/build has been authorised and completed. Do not merge or publish a public release without separate explicit approval.
- Confirm availability of Apple Developer credentials and JUCE licensing.
- Provide a support address / seller identity, and choose the sales-delivery
  route and licence policy before legal copy and an installer can be finalized.

Sources checked 2026-09-24:
- Apple Developer ID: https://developer.apple.com/developer-id/
- Apple certificates: https://developer.apple.com/help/account/certificates/create-developer-id-certificates
- JUCE 9 licence: https://juce.com/legal/juce-9-licence/

These are release-engineering gates, not a legal opinion or certification.
