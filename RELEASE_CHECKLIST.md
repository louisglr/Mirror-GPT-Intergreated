# MIЯЯOЯ commercial release checklist

This separates a technically green release candidate from a product customers
can safely install, buy and receive support for.

## Engineering

- [ ] Universal arm64 + x86_64 AU and VST3 build is green.
- [ ] DSP smoke tests and `auval` pass from a clean CI runner.
- [ ] Manual matrix in `TESTING.md` passes on Apple Silicon and Intel.
- [ ] Logic Pro and one VST3 host pass scan, load, save/restore and automation.
- [ ] Host bypass preserves PDC alignment and returns click-free on transients.
- [ ] CPU usage is measured at 44.1/48/96 kHz and 32/64/128/256 samples.
- [ ] A 30–60 minute loop and offline bounce complete without crackle or drift.
- [ ] Parameter IDs and manufacturer/plugin codes are frozen before 1.0.
- [ ] Preset names, defaults and gain staging pass a final listening review.

## macOS distribution

- [ ] Apple Developer Program membership and Developer ID Application/
      Installer certificates are available in CI secrets.
- [ ] AU, VST3 and installer are signed with the product certificate—not ad-hoc.
- [ ] The installer is notarised, stapled and tested on a clean Mac.
- [ ] Install and uninstall paths are documented and do not overwrite unrelated
      files.
- [ ] Version number is visible consistently in bundle metadata, installer,
      release notes and download filename.
- [ ] SHA-256 checksums are published with the download.

## Product and support

- [ ] Original brand/preset names have trademark and endorsement review.
- [ ] EULA, privacy statement and third-party notices (including JUCE) ship.
- [ ] System requirements and supported DAWs/formats are explicit.
- [ ] Quick-start guide explains Manual/MIDI, Live/Aligned and reported latency.
- [ ] Support email, bug-report template and crash/log instructions exist.
- [ ] Licence/activation and refund policy have been tested end-to-end.
- [ ] Accessibility, text legibility and keyboard/automation workflows are checked.
- [ ] A rollback download of the previous stable build is retained.

## Launch gate

- [ ] A versioned, non-draft release is created from the exact tested commit.
- [ ] Download is tested through the real customer delivery path.
- [ ] Release notes distinguish fixes, known limitations and compatibility.
- [ ] No marketing copy promises an exact artist, recording or competitor sound.
