# MIRROR

MIRROR er en vokal-harmonizer til macOS med fire individuelle harmony voices, Manual- og MIDI-mode, pitch-mark-aligned granular processing, Advanced voice-kontroller og MIDI chord voicing.

## Funktioner

- Fire harmony voices med interval, level, pan, formant, fine tune, tone/EQ, saturation, micro delay og vibrato.
- `ADVANCED`-panel pr. voice.
- Vocal Range, Transition, Harmony Style og global Glue-saturation.
- MIDI Velocity, Close/Open/Wide voicing samt Root/1st/2nd/3rd inversion.
- Tre sound-design presets: `Vocoder Glass`, `Fractured Stack` og `Stadium Choir`.

## Krav

- macOS
- Xcode eller Xcode Command Line Tools
- CMake 3.22 eller nyere
- Internetforbindelse ved første konfigurering, da CMake henter JUCE 7.0.12

Installer Command Line Tools, hvis de mangler:

```sh
xcode-select --install
```

## Byg lokalt

Kør kommandoerne fra repositoryets rodmappe:

```sh
cmake -S . -B build -G Xcode
cmake --build build --config Release
```

Den færdige Audio Unit ligger normalt her:

```
build/Mirror_artefacts/Release/AU/MIRROR.component
```

VST3-versionen ligger normalt her:

```
build/Mirror_artefacts/Release/VST3/MIRROR.vst3
```

## Installer i Logic Pro

Kopiér Audio Unit-filen til din bruger-plugins-mappe:

```sh
ditto "build/Mirror_artefacts/Release/AU/MIRROR.component" "$HOME/Library/Audio/Plug-Ins/Components/MIRROR.component"
codesign --force --deep --sign - "$HOME/Library/Audio/Plug-Ins/Components/MIRROR.component"
```

Luk og genåbn derefter Logic Pro. Hvis Logic stadig ikke viser pluginnet, kan Audio Unit-registreringen genstartes:

```sh
killall AudioComponentRegistrar
```

## Brug

1. Vælg **Manual** for skala-baserede harmonier, eller **MIDI** for harmonier styret af indkommende MIDI-noter.
2. Sæt **Vocal Range** til den mest passende stemmetype for mere stabil pitch-tracking.
3. Brug **Transition** til at styre, hvor hurtigt stemmerne følger nye toner.
4. Åbn **ADVANCED** på Harmony-siden for at forme hver voice individuelt.
5. I MIDI-mode kan **MIDI Voicing** og **Inversion** bruges til at skabe tætte eller åbne akkorder med bedre voice-leading.

## Kendte begrænsninger

MIRROR bruger original, tidsdomæne pitch-mark-aligned granular DSP. Det er ikke en klon af en specifik kommerciel processor eller indspilning. Ekstreme pitch-shifts vil altid kunne give mere hørbar processing end moderate intervaller.

