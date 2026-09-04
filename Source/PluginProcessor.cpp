#include "PluginProcessor.h"
#include "PluginEditor.h"

MirrorAudioProcessor::MirrorAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    cacheParameterPointers();
}

MirrorAudioProcessor::~MirrorAudioProcessor() {}

void MirrorAudioProcessor::cacheParameterPointers()
{
    const auto get = [this](const juce::String& id)
    {
        auto* value = apvts.getRawParameterValue(id);
        jassert(value != nullptr);
        return value;
    };

    parameterValues.rootNote = get("rootNote");
    parameterValues.scaleType = get("scaleType");
    parameterValues.vocalRange = get("vocalRange");
    parameterValues.harmonyStyle = get("harmonyStyle");
    parameterValues.tracking = get("tracking");
    parameterValues.glide = get("glide");
    parameterValues.freeze = get("freeze");
    parameterValues.mode = get("mode");
    parameterValues.midiVelocity = get("midiVelocity");
    parameterValues.midiVoicing = get("midiVoicing");
    parameterValues.midiInversion = get("midiInversion");
    parameterValues.dry = get("dry");
    parameterValues.dryPan = get("dryPan");
    parameterValues.dryFormant = get("dryFormant");
    parameterValues.dryPitch = get("dryPitch");
    parameterValues.dryWidth = get("dryWidth");
    parameterValues.humanize = get("humanize");
    parameterValues.character = get("character");
    parameterValues.spread = get("spread");
    parameterValues.ambience = get("ambience");
    parameterValues.harmony = get("harmony");
    parameterValues.globalSaturation = get("globalSaturation");
    parameterValues.outputGain = get("outputGain");

    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        const juce::String index(i + 1);
        parameterValues.voiceEnable[(size_t) i] = get("voiceEnable" + index);
        parameterValues.voiceSolo[(size_t) i] = get("voiceSolo" + index);
        parameterValues.voiceInterval[(size_t) i] = get("voiceInterval" + index);
        parameterValues.voiceLevel[(size_t) i] = get("voiceLevel" + index);
        parameterValues.voicePan[(size_t) i] = get("voicePan" + index);
        parameterValues.voiceFormant[(size_t) i] = get("voiceFormant" + index);
        parameterValues.voiceFineTune[(size_t) i] = get("voiceFineTune" + index);
        parameterValues.voiceTone[(size_t) i] = get("voiceTone" + index);
        parameterValues.voiceSaturation[(size_t) i] = get("voiceSaturation" + index);
        parameterValues.voiceMicroDelay[(size_t) i] = get("voiceMicroDelay" + index);
        parameterValues.voiceVibrato[(size_t) i] = get("voiceVibrato" + index);
        parameterValues.voiceVibratoRate[(size_t) i] = get("voiceVibratoRate" + index);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MirrorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    using Range = juce::NormalisableRange<float>;

    auto add01 = [&](const char* id, const char* name, float def)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ id, 1 }, name, Range(0.0f, 1.0f, 0.001f), def));
    };
    auto addBipolar = [&](const char* id, const char* name, float def)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ id, 1 }, name, Range(-1.0f, 1.0f, 0.001f), def));
    };

    // --- INPUT ---
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "rootNote", 1 }, "Key",
        juce::StringArray{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "scaleType", 1 }, "Scale",
        juce::StringArray{ "Chromatic", "Major", "Minor" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "vocalRange", 1 }, "Vocal Range",
        juce::StringArray{ "Auto", "Bass", "Baritone", "Tenor", "Alto", "Soprano" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "harmonyStyle", 1 }, "Harmony Style",
        juce::StringArray{ "Tight", "Natural", "Wide", "Choir" }, 1));

    // These three core performance stages are intentionally full-scale in MIRROR.
    // Per-voice level and the dry control still determine the musical balance.
    add01("tracking", "Tracking", 1.0f);
    add01("glide", "Glide", 1.0f);
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "freeze", 1 }, "Freeze", false));

    // --- MODE (kun Manual/MIDI) ---
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "mode", 1 }, "Mode",
        juce::StringArray{ "Manual", "MIDI" }, 0));
    add01("midiVelocity", "MIDI Velocity", 0.0f);
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "midiVoicing", 1 }, "MIDI Voicing",
        juce::StringArray{ "Close", "Open", "Wide" }, 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "midiInversion", 1 }, "MIDI Inversion",
        juce::StringArray{ "Auto", "Root", "1st", "2nd", "3rd" }, 0));

    // --- DRY VOICE ---
    add01("dry", "Dry Level", 0.7f);
    addBipolar("dryPan", "Dry Pan", 0.0f);
    addBipolar("dryFormant", "Dry Formant", 0.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "dryPitch", 1 }, "Dry Pitch", Range(-12.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("st")));
    add01("dryWidth", "Dry Width", 0.5f);

    // --- HARMONY (4 stemmer) ---
    const int defaultIntervalIdx[kNumHarmonyVoices] = { 3, 7, 13, 14 };
    const float defaultPans[kNumHarmonyVoices] = { -0.25f, 0.3f, -0.45f, 0.5f };
    const bool defaultEnabled[kNumHarmonyVoices] = { true, true, false, false };
    const float defaultLevels[kNumHarmonyVoices] = { 0.75f, 0.65f, 0.55f, 0.45f };
    const float defaultTone[kNumHarmonyVoices] = { 0.05f, 0.12f, 0.18f, 0.25f };
    const float defaultSaturation[kNumHarmonyVoices] = { 0.05f, 0.07f, 0.08f, 0.1f };
    // Keep the ensemble effect available, but do not make it part of the
    // default timing cost.  Users who want a pronounced "separate singers"
    // effect can still raise these controls.
    const float defaultMicroDelayMs[kNumHarmonyVoices] = { 0.0f, 3.0f, 6.0f, 9.0f };

    auto intervalNames = getIntervalNames();

    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        juce::String idx(i + 1);

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ "voiceEnable" + idx, 1 }, "Voice " + idx + " Enable", defaultEnabled[i]));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ "voiceSolo" + idx, 1 }, "Voice " + idx + " Solo", false));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{ "voiceInterval" + idx, 1 }, "Voice " + idx + " Interval",
            intervalNames, defaultIntervalIdx[i]));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ "voiceLevel" + idx, 1 }, "Voice " + idx + " Level",
            Range(0.0f, 1.0f, 0.001f), defaultLevels[i]));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ "voicePan" + idx, 1 }, "Voice " + idx + " Pan",
            Range(-1.0f, 1.0f, 0.001f), defaultPans[i]));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ "voiceFormant" + idx, 1 }, "Voice " + idx + " Formant",
            Range(-1.0f, 1.0f, 0.001f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ "voiceFineTune" + idx, 1 }, "Voice " + idx + " Fine Tune",
            Range(-50.0f, 50.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("cents")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ "voiceTone" + idx, 1 }, "Voice " + idx + " Tone",
            Range(0.0f, 1.0f, 0.001f), defaultTone[i]));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ "voiceSaturation" + idx, 1 }, "Voice " + idx + " Saturation",
            Range(0.0f, 1.0f, 0.001f), defaultSaturation[i]));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ "voiceMicroDelay" + idx, 1 }, "Voice " + idx + " Micro Delay",
            Range(0.0f, 45.0f, 0.1f), defaultMicroDelayMs[i],
            juce::AudioParameterFloatAttributes().withLabel("ms")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ "voiceVibrato" + idx, 1 }, "Voice " + idx + " Vibrato",
            Range(0.0f, 1.0f, 0.001f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ "voiceVibratoRate" + idx, 1 }, "Voice " + idx + " Vibrato Rate",
            Range(0.0f, 1.0f, 0.001f), 0.45f));
    }

    // --- CHARACTER / HUMANIZE ---
    add01("humanize", "Humanize", 0.3f);
    add01("character", "Character", 0.0f);
    add01("spread", "Spread", 0.5f);

    // --- AMBIENCE ---
    add01("ambience", "Ambience", 0.2f);

    // --- MIX ---
    add01("harmony", "Harmony", 1.0f);
    add01("globalSaturation", "Global Saturation", 0.0f);
    // Dedicated final trim. It is neutral at 0 dB and runs after colour,
    // before the safety limiter, so it cannot change the proven default tone.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "outputGain", 1 }, "Output Gain",
        Range(-18.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    return { params.begin(), params.end() };
}

void MirrorAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;

    voiceBuffer.prepare(sampleRate, 1.0f);
    pitchDetector.prepare(sampleRate);
    pitchCorrector.prepare(sampleRate);

    dryVoice.prepare(sampleRate);
    reportedLatencySamples = dryVoice.getLatencySamples();
    dryAlignmentL.prepare(reportedLatencySamples + 8);
    dryAlignmentR.prepare(reportedLatencySamples + 8);
    dryAlignmentL.setDelaySamples(reportedLatencySamples);
    dryAlignmentR.setDelaySamples(reportedLatencySamples);
    setLatencySamples(reportedLatencySamples);

    dryFormantProcL.prepare(sampleRate);
    dryFormantProcR.prepare(sampleRate);

    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        harmonyVoices[(size_t) i].prepare(sampleRate);
        harmonyFormant[(size_t) i].prepare(sampleRate);
        harmonyFilters[(size_t) i].prepare(sampleRate);
        harmonySaturators[(size_t) i].prepare(sampleRate);
        harmonyHumanize[(size_t) i].prepare(sampleRate, 1000 + i * 137);
        harmonyMicroDelay[(size_t) i].prepare(sampleRate);
        voiceRatioSmoothed[(size_t) i] = 1.0f;
        voiceVibratoPhase[(size_t) i] = (float) i * 0.91f;
        voiceLastMidi[(size_t) i] = 69.0f + (float) i * 3.0f;
    }

    harmonyGlueL.prepare(sampleRate);
    harmonyGlueR.prepare(sampleRate);
    globalSaturatorL.prepare(sampleRate);
    globalSaturatorR.prepare(sampleRate);
    outputLimiter.prepare(sampleRate);

    ambienceApL1.prepare((int) (sampleRate * 0.009), 0.55f);
    ambienceApL2.prepare((int) (sampleRate * 0.015), 0.45f);
    ambienceApR1.prepare((int) (sampleRate * 0.011), 0.55f);
    ambienceApR2.prepare((int) (sampleRate * 0.017), 0.45f);

    numHeldNotes = 0;
    physicalKeys.fill(false);
    sustainPedalDown = false;
    midiAssignedNotes.fill(69);
    midiAssignedVelocities.fill(0.0f);
    midiAssignedFrequencies.fill(440.0f);
    midiVoiceHasTargets.fill(false);
    midiAssignmentsDirty = true;
    midiVoiceParticipates.fill(false);
    lastMidiVoicing = lastMidiInversion = -1;
    lastHandledPitchRevision = pitchDetector.getRevision();
    harmonyVoicing = 0.0f;
    inputEnvelope = 0.0f;
    voicingAttackCoeff = 1.0f - std::exp(-1.0f / (float) (sampleRate * 0.012));
    voicingReleaseCoeff = 1.0f - std::exp(-1.0f / (float) (sampleRate * 0.065));
    voicingUnvoicedReleaseCoeff = 1.0f - std::exp(-1.0f / (float) (sampleRate * 0.022));
    // Express the lead envelope in time rather than samples.  The previous
    // fixed coefficients made the gate noticeably more aggressive at 96 kHz
    // than at 44.1/48 kHz.
    inputEnvelopeAttackCoeff = 1.0f - std::exp(-1.0f / (float) (sampleRate * 0.0003));
    inputEnvelopeReleaseCoeff = 1.0f - std::exp(-1.0f / (float) (sampleRate * 0.015));
    // Fast enough for a played MIDI chord, slow enough to remove hard grain
    // cuts on note-off and when a voice is disabled.
    voiceGateAttackCoeff = 1.0f - std::exp(-1.0f / (float) (sampleRate * 0.015));
    voiceGateReleaseCoeff = 1.0f - std::exp(-1.0f / (float) (sampleRate * 0.030));
    voiceRenderGains.fill(0.0f);

    dryLevelSmoothed.reset(sampleRate, 0.03);
    harmonyLevelSmoothed.reset(sampleRate, 0.03);
    dryWidthSmoothed.reset(sampleRate, 0.05);
    outputGainSmoothed.reset(sampleRate, 0.04);
    dryPitchBlendSmoothed.reset(sampleRate, 0.025);
    dryFormantSmoothed.reset(sampleRate, 0.025);
    ambienceSmoothed.reset(sampleRate, 0.050);
    globalSaturationSmoothed.reset(sampleRate, 0.025);
    for (auto& s : voiceLevelSmoothed) s.reset(sampleRate, 0.03);
    for (auto& s : voicePanSmoothed) s.reset(sampleRate, 0.05);
    for (auto& s : voiceSaturationSmoothed) s.reset(sampleRate, 0.025);
    for (auto& s : voiceMicroDelaySmoothed) s.reset(sampleRate, 0.025);
    for (auto& s : voiceFormantSmoothed)
    {
        s.reset(sampleRate, 0.006);
        s.setCurrentAndTargetValue(0.0f);
    }

    dryLevelSmoothed.setCurrentAndTargetValue(parameterValues.dry->load(std::memory_order_relaxed));
    // Harmony/Mix is a locked unity stage in the current product direction;
    // keep its smoother initialised to the same value used in processBlock so
    // an old saved lower mix cannot create a startup level jump.
    harmonyLevelSmoothed.setCurrentAndTargetValue(1.0f);
    dryWidthSmoothed.setCurrentAndTargetValue(parameterValues.dryWidth->load(std::memory_order_relaxed));
    outputGainSmoothed.setCurrentAndTargetValue(parameterValues.outputGain->load(std::memory_order_relaxed));
    dryPitchBlendSmoothed.setCurrentAndTargetValue(
        std::abs(parameterValues.dryPitch->load(std::memory_order_relaxed)) >= 0.001f ? 1.0f : 0.0f);
    dryFormantSmoothed.setCurrentAndTargetValue(parameterValues.dryFormant->load(std::memory_order_relaxed));
    ambienceSmoothed.setCurrentAndTargetValue(parameterValues.ambience->load(std::memory_order_relaxed));
    globalSaturationSmoothed.setCurrentAndTargetValue(parameterValues.globalSaturation->load(std::memory_order_relaxed));
    const float initialCharacter = parameterValues.character->load(std::memory_order_relaxed);
    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        const float initialSaturation = juce::jlimit(0.0f, 1.0f,
            parameterValues.voiceSaturation[(size_t) i]->load(std::memory_order_relaxed)
                + initialCharacter * 0.12f);
        voiceSaturationSmoothed[(size_t) i].setCurrentAndTargetValue(initialSaturation);
        voiceMicroDelaySmoothed[(size_t) i].setCurrentAndTargetValue(
            parameterValues.voiceMicroDelay[(size_t) i]->load(std::memory_order_relaxed));
    }
}

void MirrorAudioProcessor::releaseResources() {}

bool MirrorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    if (input != juce::AudioChannelSet::mono() && input != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MirrorAudioProcessor::removeHeldNote(int note)
{
    for (int i = 0; i < numHeldNotes; ++i)
    {
        if (heldNotes[(size_t) i] == note)
        {
            for (int j = i; j < numHeldNotes - 1; ++j)
            {
                heldNotes[(size_t) j] = heldNotes[(size_t) (j + 1)];
                heldNoteVelocities[(size_t) j] = heldNoteVelocities[(size_t) (j + 1)];
            }
            --numHeldNotes;
            return;
        }
    }
}

void MirrorAudioProcessor::rebuildMidiAssignments(
    int midiVoicing, int midiInversion,
    const std::array<bool, kNumHarmonyVoices>& participatingVoices)
{
    midiAssignmentsDirty = false;
    lastMidiVoicing = midiVoicing;
    lastMidiInversion = midiInversion;

    // Muted voices must not reserve a pitch in the chord.  Keeping this list
    // compact is the key difference from the old four-voice-only allocator:
    // with two audible voices and a C/E MIDI chord, the result is C + E, not
    // C + C while E is silently assigned to a disabled slot.
    std::array<int, kNumHarmonyVoices> activeVoices {};
    int activeCount = 0;
    for (int v = 0; v < kNumHarmonyVoices; ++v)
        if (participatingVoices[(size_t) v])
            activeVoices[(size_t) activeCount++] = v;

    if (numHeldNotes == 0 || activeCount == 0)
        return;

    const int maxAutoInversion = juce::jmax(0, numHeldNotes - 1);
    const int maxExplicitInversion = juce::jmin(3, maxAutoInversion);
    const int firstInversion = midiInversion == 0
        ? 0
        : juce::jlimit(0, maxExplicitInversion, midiInversion - 1);
    const int lastInversion = midiInversion == 0 ? maxAutoInversion : firstInversion;

    std::array<int, kNumHarmonyVoices> bestPaletteIndices {};
    std::array<int, kMaxHeldNotes> bestPaletteNotes {};
    std::array<float, kMaxHeldNotes> bestPaletteVelocities {};
    float bestCost = std::numeric_limits<float>::max();
    bool foundAssignment = false;

    // Auto genuinely evaluates every available inversion and chooses the one
    // with the smallest total movement.  Root/1st/2nd/3rd evaluate only the
    // requested inversion, so those menu choices are deterministic.
    for (int inversion = firstInversion; inversion <= lastInversion; ++inversion)
    {
        std::array<int, kMaxHeldNotes> chordNotes {};
        std::array<float, kMaxHeldNotes> chordVelocities {};
        for (int j = 0; j < numHeldNotes; ++j)
        {
            chordNotes[(size_t) j] = heldNotes[(size_t) j];
            chordVelocities[(size_t) j] = heldNoteVelocities[(size_t) j];
        }

        for (int r = 0; r < inversion; ++r)
        {
            const int movedNote = chordNotes[0] + 12;
            const float movedVelocity = chordVelocities[0];
            for (int j = 0; j < numHeldNotes - 1; ++j)
            {
                chordNotes[(size_t) j] = chordNotes[(size_t) (j + 1)];
                chordVelocities[(size_t) j] = chordVelocities[(size_t) (j + 1)];
            }
            chordNotes[(size_t) (numHeldNotes - 1)] = movedNote;
            chordVelocities[(size_t) (numHeldNotes - 1)] = movedVelocity;
        }

        // Keep played notes in their original register whenever there are
        // enough of them. Voicing only spreads intentional *duplicates* used
        // when more MIRROR voices are active than MIDI notes are held.
        const int paletteCount = juce::jlimit(activeCount, kMaxHeldNotes,
                                               juce::jmax(numHeldNotes, activeCount));
        std::array<int, kMaxHeldNotes> paletteNotes {};
        std::array<float, kMaxHeldNotes> paletteVelocities {};
        for (int p = 0; p < paletteCount; ++p)
        {
            if (p < numHeldNotes)
            {
                paletteNotes[(size_t) p] = chordNotes[(size_t) p];
                paletteVelocities[(size_t) p] = chordVelocities[(size_t) p];
                continue;
            }

            const int repeated = p - numHeldNotes;
            const int chordIndex = repeated % numHeldNotes;
            int octave = 1 + repeated / numHeldNotes;
            if (midiVoicing == 1) // Open
                octave += chordIndex == numHeldNotes - 1 ? 1 : 0;
            else if (midiVoicing == 2) // Wide
                octave += 1 + (chordIndex > 0 ? 1 : 0);

            paletteNotes[(size_t) p] = chordNotes[(size_t) chordIndex] + 12 * octave;
            paletteVelocities[(size_t) p] = chordVelocities[(size_t) chordIndex];
        }

        std::array<int, kNumHarmonyVoices> chosen {};
        std::array<bool, kMaxHeldNotes> used {};

        const auto search = [&](auto&& self, int depth, float accumulatedCost) -> void
        {
            if (depth == activeCount)
            {
                // Break exact Auto ties toward root position without making
                // "Auto" secretly behave as Root for non-tied chords.
                const float totalCost = accumulatedCost
                    + (midiInversion == 0 ? 0.025f * (float) inversion : 0.0f);
                if (totalCost < bestCost)
                {
                    bestCost = totalCost;
                    foundAssignment = true;
                    bestPaletteIndices = chosen;
                    bestPaletteNotes = paletteNotes;
                    bestPaletteVelocities = paletteVelocities;
                }
                return;
            }

            const int voice = activeVoices[(size_t) depth];
            for (int p = 0; p < paletteCount; ++p)
            {
                if (used[(size_t) p])
                    continue;

                const int note = paletteNotes[(size_t) p];
                const float movementCost = midiVoiceHasTargets[(size_t) voice]
                    ? std::abs((float) note - voiceLastMidi[(size_t) voice])
                    // On a first chord, retain the MIDI player's vertical
                    // order. Auto then breaks the deliberate tie toward Root.
                    : std::abs((float) note - (float) paletteNotes[(size_t) depth]);
                float cost = accumulatedCost + movementCost;

                if (depth > 0)
                {
                    const int previousVoice = activeVoices[(size_t) (depth - 1)];
                    const int previousNote = paletteNotes[(size_t) chosen[(size_t) (depth - 1)]];
                    // Preserve the existing vertical order when the two
                    // voices were already ordered. Crossing remains possible
                    // when it prevents a much larger musical leap.
                    const bool previouslyAscending = !midiVoiceHasTargets[(size_t) previousVoice]
                        || !midiVoiceHasTargets[(size_t) voice]
                        || voiceLastMidi[(size_t) previousVoice]
                            <= voiceLastMidi[(size_t) voice] + 0.25f;
                    if (previouslyAscending && note < previousNote)
                        cost += 7.0f;
                    else if (!previouslyAscending && note > previousNote)
                        cost += 7.0f;
                }

                if (cost >= bestCost)
                    continue;

                used[(size_t) p] = true;
                chosen[(size_t) depth] = p;
                self(self, depth + 1, cost);
                used[(size_t) p] = false;
            }
        };

        search(search, 0, 0.0f);

        if (midiInversion != 0 && foundAssignment)
            break;
    }

    if (!foundAssignment)
        return;

    for (int d = 0; d < activeCount; ++d)
    {
        const int voice = activeVoices[(size_t) d];
        const int paletteIndex = bestPaletteIndices[(size_t) d];
        midiAssignedNotes[(size_t) voice] = bestPaletteNotes[(size_t) paletteIndex];
        midiAssignedVelocities[(size_t) voice] = bestPaletteVelocities[(size_t) paletteIndex];
        midiAssignedFrequencies[(size_t) voice] = PitchCorrector::midiToFreq(midiAssignedNotes[(size_t) voice]);
        voiceLastMidi[(size_t) voice] = (float) midiAssignedNotes[(size_t) voice];
        midiVoiceHasTargets[(size_t) voice] = true;
    }
}

void MirrorAudioProcessor::handleMidiMessage(const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        // Only chord-bearing messages invalidate the allocator.  Clock,
        // aftertouch, pitch bend and unrelated controllers must never force
        // a voice-leading rebuild on the audio thread.
        midiAssignmentsDirty = true;
        const int note = juce::jlimit(0, 127, m.getNoteNumber());
        physicalKeys[(size_t) note] = true;

        for (int i = 0; i < numHeldNotes; ++i)
        {
            if (heldNotes[(size_t) i] == note)
            {
                heldNoteVelocities[(size_t) i] = m.getFloatVelocity();
                return;
            }
        }

        if (numHeldNotes >= kMaxHeldNotes)
            return;

        int insertAt = numHeldNotes;
        while (insertAt > 0 && heldNotes[(size_t) (insertAt - 1)] > note)
        {
            heldNotes[(size_t) insertAt] = heldNotes[(size_t) (insertAt - 1)];
            heldNoteVelocities[(size_t) insertAt] = heldNoteVelocities[(size_t) (insertAt - 1)];
            --insertAt;
        }
        heldNotes[(size_t) insertAt] = note;
        heldNoteVelocities[(size_t) insertAt] = m.getFloatVelocity();
        ++numHeldNotes;
        return;
    }

    if (m.isNoteOff())
    {
        midiAssignmentsDirty = true;
        const int note = juce::jlimit(0, 127, m.getNoteNumber());
        physicalKeys[(size_t) note] = false;
        if (!sustainPedalDown)
            removeHeldNote(note);
        return;
    }

    if (m.isController() && m.getControllerNumber() == 64)
    {
        midiAssignmentsDirty = true;
        const bool nextSustainState = m.getControllerValue() >= 64;
        if (sustainPedalDown && !nextSustainState)
        {
            // Releasing the pedal lets go only of keys that are physically up.
            for (int i = numHeldNotes - 1; i >= 0; --i)
                if (!physicalKeys[(size_t) heldNotes[(size_t) i]])
                    removeHeldNote(heldNotes[(size_t) i]);
        }
        sustainPedalDown = nextSustainState;
        return;
    }

    if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        midiAssignmentsDirty = true;
        numHeldNotes = 0;
        physicalKeys.fill(false);
        sustainPedalDown = false;
    }
}

void MirrorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int inputChannels = getTotalNumInputChannels();
    if (inputChannels < 1 || buffer.getNumChannels() < 2)
        return;

    const int rootNote = (int) parameterValues.rootNote->load(std::memory_order_relaxed);
    const int scaleType = (int) parameterValues.scaleType->load(std::memory_order_relaxed);
    const int vocalRange = (int) parameterValues.vocalRange->load(std::memory_order_relaxed);
    const int harmonyStyle = (int) parameterValues.harmonyStyle->load(std::memory_order_relaxed);
    pitchDetector.setVocalRange(vocalRange);
    // These three macro stages are deliberately locked at unity.  Louis asked
    // for MIX, Tracking and Transition to remain 100% at all times; keeping
    // that policy in the DSP prevents an old preset or automation lane from
    // quietly degrading the tight, high-quality path.  The legacy parameters
    // remain registered for session compatibility, but do not alter audio.
    constexpr float tracking = 1.0f;
    constexpr float glide = 1.0f;
    const bool freeze = parameterValues.freeze->load(std::memory_order_relaxed) > 0.5f;
    const int mode = (int) parameterValues.mode->load(std::memory_order_relaxed);
    const float midiVelocitySensitivity = parameterValues.midiVelocity->load(std::memory_order_relaxed);
    const int midiVoicing = (int) parameterValues.midiVoicing->load(std::memory_order_relaxed);
    const int midiInversion = (int) parameterValues.midiInversion->load(std::memory_order_relaxed);

    float humanizeAmt = parameterValues.humanize->load(std::memory_order_relaxed);
    const float character = parameterValues.character->load(std::memory_order_relaxed);
    float spread = parameterValues.spread->load(std::memory_order_relaxed) * 2.0f;
    float ambienceAmt = parameterValues.ambience->load(std::memory_order_relaxed);
    const float globalSaturation = parameterValues.globalSaturation->load(std::memory_order_relaxed);

    // One musical macro controls the amount of separation while retaining
    // direct access to individual voices in Advanced.
    static constexpr float styleHumanize[] = { 0.35f, 1.0f, 1.15f, 0.85f };
    static constexpr float styleSpread[] = { 0.55f, 1.0f, 1.25f, 1.45f };
    static constexpr float styleAmbience[] = { 0.35f, 1.0f, 1.15f, 1.55f };
    const int styleIndex = juce::jlimit(0, 3, harmonyStyle);
    // Keep voice-local drift musical without allowing four independent
    // walkers to pull the stack apart.
    humanizeAmt = juce::jlimit(0.0f, 1.0f, humanizeAmt * styleHumanize[styleIndex] * 0.70f);
    spread *= styleSpread[styleIndex];
    ambienceAmt = juce::jlimit(0.0f, 1.0f, ambienceAmt * styleAmbience[styleIndex]);

    const float dryPanP = parameterValues.dryPan->load(std::memory_order_relaxed);
    const float dryFormantP = parameterValues.dryFormant->load(std::memory_order_relaxed);
    const float dryPitchSemis = parameterValues.dryPitch->load(std::memory_order_relaxed);
    const float dryPitchRatio = std::exp2(dryPitchSemis / 12.0f);

    dryLevelSmoothed.setTargetValue(parameterValues.dry->load(std::memory_order_relaxed));
    harmonyLevelSmoothed.setTargetValue(1.0f);
    dryWidthSmoothed.setTargetValue(parameterValues.dryWidth->load(std::memory_order_relaxed));
    outputGainSmoothed.setTargetValue(parameterValues.outputGain->load(std::memory_order_relaxed));
    dryPitchBlendSmoothed.setTargetValue(std::abs(dryPitchSemis) >= 0.001f ? 1.0f : 0.0f);
    dryFormantSmoothed.setTargetValue(dryFormantP);
    ambienceSmoothed.setTargetValue(ambienceAmt);
    globalSaturationSmoothed.setTargetValue(globalSaturation);

    struct VoiceParams
    {
        bool enable, solo; int intervalIdx; float formant, fineTune, tone, saturation, microDelayMs, vibrato, vibratoRate;
    };
    std::array<VoiceParams, kNumHarmonyVoices> vp;
    bool anySolo = false;
    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        const size_t voice = (size_t) i;
        vp[voice].enable = parameterValues.voiceEnable[voice]->load(std::memory_order_relaxed) > 0.5f;
        vp[voice].solo = parameterValues.voiceSolo[voice]->load(std::memory_order_relaxed) > 0.5f;
        vp[voice].intervalIdx = (int) parameterValues.voiceInterval[voice]->load(std::memory_order_relaxed);
        vp[voice].formant = parameterValues.voiceFormant[voice]->load(std::memory_order_relaxed)
                                  + character * 0.08f * ((i % 2 == 0) ? 1.0f : -1.0f);
        vp[voice].fineTune = parameterValues.voiceFineTune[voice]->load(std::memory_order_relaxed);
        vp[voice].tone = juce::jlimit(0.0f, 1.0f,
                                       parameterValues.voiceTone[voice]->load(std::memory_order_relaxed)
                                           + character * 0.1f);
        vp[voice].saturation = juce::jlimit(0.0f, 1.0f,
                                             parameterValues.voiceSaturation[voice]->load(std::memory_order_relaxed)
                                                 + character * 0.12f);
        vp[voice].microDelayMs = parameterValues.voiceMicroDelay[voice]->load(std::memory_order_relaxed);
        vp[voice].vibrato = parameterValues.voiceVibrato[voice]->load(std::memory_order_relaxed);
        vp[voice].vibratoRate = parameterValues.voiceVibratoRate[voice]->load(std::memory_order_relaxed);

        voiceLevelSmoothed[voice].setTargetValue(parameterValues.voiceLevel[voice]->load(std::memory_order_relaxed));
        voiceSaturationSmoothed[voice].setTargetValue(vp[voice].saturation);
        voiceMicroDelaySmoothed[voice].setTargetValue(vp[voice].microDelayMs);
        float panSpreadAmt = juce::jlimit(0.0f, 2.0f, spread + character * 0.1f);
        voicePanSmoothed[voice].setTargetValue(
            parameterValues.voicePan[voice]->load(std::memory_order_relaxed) * panSpreadAmt);
        if (vp[voice].solo) anySolo = true;
    }

    std::array<float, kNumHarmonyVoices> fineTuneRatios {};
    std::array<float, kNumHarmonyVoices> chromaticRatios {};
    std::array<float, kNumHarmonyVoices> manualScaleFrequencies {};
    std::array<bool, kNumHarmonyVoices> participatingMidiVoices {};
    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        const bool participates = vp[(size_t) i].enable && (!anySolo || vp[(size_t) i].solo);
        participatingMidiVoices[(size_t) i] = participates;
        if (midiVoiceParticipates[(size_t) i] != participates)
        {
            midiVoiceParticipates[(size_t) i] = participates;
            midiAssignmentsDirty = true;
        }
    }

    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        const auto& interval = kMusicalIntervals[(size_t) vp[(size_t) i].intervalIdx];
        fineTuneRatios[(size_t) i] = std::exp2(vp[(size_t) i].fineTune / 1200.0f);
        chromaticRatios[(size_t) i] = fineTuneRatios[(size_t) i]
            * std::exp2((float) interval.semitones / 12.0f);
    }

    int manualScaleBaseMidi = std::numeric_limits<int>::min();
    auto refreshManualScaleFrequencies = [&]
    {
        manualScaleBaseMidi = lastStableBaseMidi;
        for (int i = 0; i < kNumHarmonyVoices; ++i)
        {
            const auto& interval = kMusicalIntervals[(size_t) vp[(size_t) i].intervalIdx];
            const int targetMidi = PitchCorrector::shiftByScaleSteps(
                manualScaleBaseMidi, rootNote, scaleType, interval.scaleSteps);
            manualScaleFrequencies[(size_t) i] = PitchCorrector::midiToFreq(targetMidi);
        }
    };
    if (mode == 0 && scaleType != PitchCorrector::Chromatic)
        refreshManualScaleFrequencies();

    // Voice colour follows the register the voice is actually occupying, not
    // merely its UI slot.  That keeps an octave-down voice from losing its
    // fundamental through an overly high high-pass, while high MIDI notes get
    // a little more filtering and de-essing before they turn brittle.
    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        const size_t voice = (size_t) i;
        const auto& interval = kMusicalIntervals[(size_t) vp[voice].intervalIdx];
        int targetMidi = lastStableBaseMidi + interval.semitones;
        if (scaleType != PitchCorrector::Chromatic && mode == 0)
            targetMidi = PitchCorrector::shiftByScaleSteps(lastStableBaseMidi, rootNote,
                                                            scaleType, interval.scaleSteps);
        else if (mode == 1 && midiVoiceHasTargets[voice])
            targetMidi = midiAssignedNotes[voice];

        const float lowRegister = juce::jlimit(0.0f, 1.0f,
            (60.0f - (float) targetMidi) / 24.0f);
        const float highRegister = juce::jlimit(0.0f, 1.0f,
            ((float) targetMidi - 70.0f) / 26.0f);
        static constexpr float roleHighPassOffset[kNumHarmonyVoices] = { 0.0f, 10.0f, -18.0f, 22.0f };
        static constexpr float roleLowPassOffset[kNumHarmonyVoices] = { 1800.0f, -800.0f, -1700.0f, -1300.0f };
        const float baseHighPassHz = 92.0f - 44.0f * lowRegister
                                   + 72.0f * highRegister + roleHighPassOffset[i];
        const float baseLowPassHz = 12500.0f + 1800.0f * lowRegister
                                  - 3000.0f * highRegister + roleLowPassOffset[i];
        const float highPassHz = juce::jmax(38.0f,
            baseHighPassHz * (1.0f + vp[voice].tone * 0.35f));
        const float lowPassHz = baseLowPassHz
            * juce::jmap(vp[voice].tone, 0.0f, 1.0f, 1.0f, 0.64f);
        harmonyFilters[voice].setCutoffs(highPassHz, lowPassHz);
    }

    if (mode == 1 && (midiAssignmentsDirty || midiVoicing != lastMidiVoicing
                      || midiInversion != lastMidiInversion))
        rebuildMidiAssignments(midiVoicing, midiInversion, participatingMidiVoices);

    // MIDI chords need a shorter internal retarget than a continuously
    // corrected manual vocal.  The 26 ms response is quick enough to feel
    // played, while the 15/30 ms voice gate removes clicky re-triggers.
    const float glideTimeMs = mode == 1
        ? 26.0f
        : juce::jmap(juce::jlimit(0.0f, 1.0f, glide), 8.0f, 155.0f);
    const float glideCoeff = 1.0f - std::exp(-1.0f / (float) (currentSampleRate * glideTimeMs * 0.001));
    const float dryPanPos = (juce::jlimit(-1.0f, 1.0f, dryPanP) * 0.5f + 0.5f)
                          * juce::MathConstants<float>::halfPi;
    const float dryPanGainL = std::cos(dryPanPos) * 1.4142f;
    const float dryPanGainR = std::sin(dryPanPos) * 1.4142f;

    auto* channelL = buffer.getWritePointer(0);
    auto* channelR = buffer.getWritePointer(1);

    // Processing messages at their actual sample position keeps MIDI-triggered
    // chord changes tight even at large DAW buffer sizes.
    auto midiEvent = midiMessages.begin();
    const auto midiEnd = midiMessages.end();

    // UI telemetry is accumulated for the full block and published once.
    std::array<float, kNumHarmonyVoices> visualVoicePeaks {};

    for (int n = 0; n < numSamples; ++n)
    {
        while (midiEvent != midiEnd && (*midiEvent).samplePosition <= n)
        {
            handleMidiMessage((*midiEvent).getMessage());
            ++midiEvent;
        }
        if (mode == 1 && midiAssignmentsDirty)
            rebuildMidiAssignments(midiVoicing, midiInversion, participatingMidiVoices);

        // Sanitize before *any* recursive state sees the host input.  The
        // VoiceBuffer/PitchDetector already guard themselves, but leaving a
        // NaN here would otherwise poison inputEnvelope and harmonyVoicing
        // indefinitely after one malformed host sample.
        float dryL = channelL[n];
        if (!std::isfinite(dryL))
            dryL = 0.0f;
        float dryR = inputChannels > 1 ? channelR[n] : dryL;
        if (!std::isfinite(dryR))
            dryR = 0.0f;
        float monoIn = 0.5f * (dryL + dryR);
        if (!std::isfinite(monoIn))
            monoIn = 0.0f;

        voiceBuffer.write(monoIn);
        pitchDetector.pushSample(monoIn);

        if (!std::isfinite(inputEnvelope) || inputEnvelope < 0.0f)
            inputEnvelope = 0.0f;
        const float inputMagnitude = std::abs(monoIn);
        const float inputEnvelopeCoeff = inputMagnitude > inputEnvelope
            ? inputEnvelopeAttackCoeff : inputEnvelopeReleaseCoeff;
        inputEnvelope += (inputMagnitude - inputEnvelope) * inputEnvelopeCoeff;
        if (!std::isfinite(inputEnvelope) || inputEnvelope < 0.0f)
            inputEnvelope = 0.0f;

        float detectedFreq = pitchDetector.getFrequency();
        float confidence = pitchDetector.getConfidence();
        if (!std::isfinite(detectedFreq) || detectedFreq < 0.0f)
            detectedFreq = 0.0f;
        if (!std::isfinite(confidence))
            confidence = 0.0f;
        confidence = juce::jlimit(0.0f, 1.0f, confidence);

        float leadRatio;
        if (freeze)
        {
            leadRatio = frozenLeadRatio;
        }
        else
        {
            leadRatio = pitchCorrector.process(detectedFreq, confidence, rootNote, scaleType, tracking);
            frozenLeadRatio = leadRatio;
        }
        if (!std::isfinite(leadRatio))
            leadRatio = frozenLeadRatio = 1.0f;

        // Keep both dry paths alive at all times.  Previously, setting Dry
        // Pitch away from zero froze the alignment delays; returning to zero
        // then replayed old audio for one latency period.  A short crossfade
        // also prevents a click or abrupt stereo collapse while automating.
        const float alignedL = dryAlignmentL.process(dryL);
        const float alignedR = dryAlignmentR.process(dryR);
        const float shifted = dryVoice.process(voiceBuffer, dryPitchRatio, detectedFreq);
        const float dryPitchBlend = dryPitchBlendSmoothed.getNextValue();
        float dryProcL = alignedL + (shifted - alignedL) * dryPitchBlend;
        float dryProcR = alignedR + (shifted - alignedR) * dryPitchBlend;
        // A pitch shift moves formants with the source.  Counteracting part
        // of that movement makes upward voices less chipmunk-like and
        // downward voices less muffled; the user control remains additive.
        const float dryFormantAmount = juce::jlimit(-1.0f, 1.0f,
            dryFormantSmoothed.getNextValue() - 0.45f * dryPitchSemis / 12.0f);
        // Keep the all-pass state warm even at a neutral amount.  Skipping it
        // at exactly zero leaves stale state behind and makes a later formant
        // automation move less predictable.
        dryProcL = dryFormantProcL.process(dryProcL, dryFormantAmount);
        dryProcR = dryFormantProcR.process(dryProcR, dryFormantAmount);

        // A continuous pitch-and-level confidence gate avoids both the
        // metallic breath artefacts of a fully wet granular voice and the
        // obvious on/off edge of a binary voicing threshold.
        const float pitchGate = juce::jlimit(0.0f, 1.0f, (confidence - 0.25f) / 0.33f);
        const float smoothPitchGate = pitchGate * pitchGate * (3.0f - 2.0f * pitchGate);
        const float levelGate = juce::jlimit(0.0f, 1.0f, (inputEnvelope - 0.0008f) / 0.0072f);
        const float voicingTarget = smoothPitchGate * levelGate;
        // Release generated audio more quickly on an energetic unvoiced
        // consonant, but retain the longer release for vowels and breaths.
        // This keeps diction on the dry vocal instead of smearing an
        // artificial pitched tail across S/T/K sounds.
        const bool energeticUnvoiced = confidence < 0.18f && inputEnvelope > 0.012f;
        const float voicingCoeff = voicingTarget > harmonyVoicing
            ? voicingAttackCoeff
            : (energeticUnvoiced ? voicingUnvoicedReleaseCoeff : voicingReleaseCoeff);
        if (!std::isfinite(harmonyVoicing))
            harmonyVoicing = 0.0f;
        harmonyVoicing += (voicingTarget - harmonyVoicing) * voicingCoeff;
        if (!std::isfinite(harmonyVoicing))
            harmonyVoicing = 0.0f;
        harmonyVoicing = juce::jlimit(0.0f, 1.0f, harmonyVoicing);

        float dryWidthNow = dryWidthSmoothed.getNextValue() * 2.0f;
        float mid = 0.5f * (dryProcL + dryProcR);
        float side = 0.5f * (dryProcL - dryProcR) * dryWidthNow;
        float widenedL = mid + side;
        float widenedR = mid - side;
        float dryOutL = widenedL * dryPanGainL;
        float dryOutR = widenedR * dryPanGainR;

        float harmonySumL = 0.0f, harmonySumR = 0.0f;

        const auto pitchRevision = pitchDetector.getRevision();
        if (pitchRevision != lastHandledPitchRevision)
        {
            lastHandledPitchRevision = pitchRevision;
            if (detectedFreq > 20.0f)
            {
                // Use PitchCorrector's hysteretic decision, rather than a
                // second raw quantisation.  Lead and manual harmonies now
                // move together at a scale boundary.
                const int stableBaseMidi = pitchCorrector.getLastQuantisedMidi();
                if (confidence > 0.45f
                    && stableBaseMidi != std::numeric_limits<int>::min())
                    lastStableBaseMidi = stableBaseMidi;
            }
        }
        if (mode == 0 && scaleType != PitchCorrector::Chromatic
            && manualScaleBaseMidi != lastStableBaseMidi)
            refreshManualScaleFrequencies();

        // Normalise from the actual audible voice gains rather than simply
        // counting enabled slots.  This avoids an unnecessary level drop for
        // a quiet texture voice, while still protecting against correlated
        // vocal material summing aggressively in a dense stack.
        float stackGainSum = 0.0f;
        float stackGainSquares = 0.0f;
        float stackVoiceWeight = 0.0f;

        for (int i = 0; i < kNumHarmonyVoices; ++i)
        {
            const size_t voice = (size_t) i;
            const bool controlsActive = vp[voice].enable && (!anySolo || vp[voice].solo);
            const bool requestedActive = controlsActive && (mode != 1 || numHeldNotes > 0);

            float gate = voiceRenderGains[voice];
            if (!std::isfinite(gate))
                gate = 0.0f;
            const float gateTarget = requestedActive ? 1.0f : 0.0f;
            const float gateCoeff = gateTarget > gate ? voiceGateAttackCoeff : voiceGateReleaseCoeff;
            gate += (gateTarget - gate) * gateCoeff;
            gate = std::isfinite(gate) ? juce::jlimit(0.0f, 1.0f, gate) : 0.0f;
            voiceRenderGains[voice] = gate;

            // Keep granular read heads and short internal delay lines moving
            // even while a voice is inaudible.  The phase-locked reader has
            // no allocation/reset in the callback; clocking it here prevents
            // a re-enabled voice from reading a stale circular-buffer region.
            // The processing path remains fully allocation-free.
            const bool renderVoice = requestedActive || gate > 1.0e-4f;
            if (!renderVoice)
            {
                voiceRatioSmoothed[voice] += (1.0f - voiceRatioSmoothed[voice]) * glideCoeff;
                if (!std::isfinite(voiceRatioSmoothed[voice]))
                    voiceRatioSmoothed[voice] = 1.0f;
                (void) harmonyVoices[voice].process(voiceBuffer, voiceRatioSmoothed[voice], detectedFreq);
                (void) harmonyFormant[voice].process(0.0f, voiceFormantSmoothed[voice].getNextValue());
                (void) harmonySaturators[voice].process(0.0f, voiceSaturationSmoothed[voice].getNextValue());
                (void) harmonyFilters[voice].process(0.0f);
                (void) harmonyMicroDelay[voice].process(0.0f, voiceMicroDelaySmoothed[voice].getNextValue());
                (void) voiceLevelSmoothed[voice].getNextValue();
                (void) voicePanSmoothed[voice].getNextValue();
                continue;
            }

            auto hz = harmonyHumanize[voice].tick(humanizeAmt);
            // ±15 cents of drift and ±16 cents of vibrato are accurate enough
            // with this linear conversion, avoiding four exp/pow calls per
            // sample while retaining inaudible error at these tiny depths.
            constexpr float centsToRatio = 0.0005777895f;
            const float driftRatio = juce::jmax(0.5f, 1.0f + hz.pitchCents * centsToRatio);

            float targetRatio = 1.0f;
            if (mode == 0)
            {
                if (scaleType == PitchCorrector::Chromatic)
                    targetRatio = leadRatio * chromaticRatios[voice] * driftRatio;
                else if (detectedFreq > 20.0f)
                    targetRatio = manualScaleFrequencies[voice] * fineTuneRatios[voice]
                                * driftRatio / detectedFreq;
            }
            else if (detectedFreq > 20.0f && midiAssignedFrequencies[voice] > 20.0f)
            {
                // A releasing MIDI voice deliberately keeps its last target
                // until the gate has faded, rather than snapping to unison at
                // note-off.  This removes the old hard, artificial cutoff.
                targetRatio = midiAssignedFrequencies[voice] * fineTuneRatios[voice]
                            * driftRatio / detectedFreq;
            }

            // Keep the control state, formant compensation and granular
            // reader within the same musical range.  The phase-locked reader
            // has an internal safety clamp, but clamping before smoothing
            // prevents an unreachable MIDI target taking a long time to
            // recover.
            targetRatio = juce::jlimit(0.25f, 4.0f, targetRatio);

            voiceRatioSmoothed[voice] += (targetRatio - voiceRatioSmoothed[voice]) * glideCoeff;
            if (!std::isfinite(voiceRatioSmoothed[voice]))
                voiceRatioSmoothed[voice] = 1.0f;

            // Vibrato is a continuous sine modulation, deliberately separate
            // from Humanize's random drift to avoid zipper/saw artefacts.
            float readRatio = juce::jlimit(0.25f, 4.0f, voiceRatioSmoothed[voice]);
            if (vp[voice].vibrato > 1.0e-4f)
            {
                const float vibratoRateHz = juce::jmap(vp[voice].vibratoRate, 3.0f, 7.2f)
                    + (float) i * 0.12f;
                voiceVibratoPhase[voice] += juce::MathConstants<float>::twoPi
                    * vibratoRateHz / (float) currentSampleRate;
                if (voiceVibratoPhase[voice] >= juce::MathConstants<float>::twoPi)
                    voiceVibratoPhase[voice] -= juce::MathConstants<float>::twoPi;
                const float vibratoCents = std::sin(voiceVibratoPhase[voice])
                    * vp[voice].vibrato * 16.0f;
                readRatio *= juce::jmax(0.5f, 1.0f + vibratoCents * centsToRatio);
            }

            // Update the formant target at 1/8th sample rate. That is close
            // enough to follow a glide, avoids a per-sample logarithm, and
            // keeps the spectral compensation perceptually continuous.
            if ((n & 7) == 0)
            {
                const float shiftSemitones = 12.0f * std::log2(
                    juce::jmax(0.01f, readRatio));
                const float targetFormant = juce::jlimit(-1.0f, 1.0f,
                    vp[voice].formant - 0.45f * shiftSemitones / 12.0f);
                voiceFormantSmoothed[voice].setTargetValue(targetFormant);
            }

            float raw = harmonyVoices[voice].process(voiceBuffer, readRatio, detectedFreq);
            raw = harmonyFormant[voice].process(raw, voiceFormantSmoothed[voice].getNextValue());

            // Colour before the final tone/de-ess stage.  This keeps the
            // harmonic warmth while the voice EQ removes brittle by-products.
            raw = harmonySaturators[voice].process(raw, voiceSaturationSmoothed[voice].getNextValue());
            raw = harmonyFilters[voice].process(raw);

            raw *= (1.0f + hz.ampMod);
            if (mode == 1)
            {
                const float velocityGain = 0.35f + 0.65f * midiAssignedVelocities[voice];
                raw *= 1.0f + midiVelocitySensitivity * (velocityGain - 1.0f);
            }

            // Modulating a short fractional delay is effectively a second
            // pitch shifter.  It was the remaining source of the sharp,
            // saw-like artefact when Humanize was raised.  Keep Micro Delay
            // stable; Humanize still supplies gentle pitch and level drift.
            raw = harmonyMicroDelay[voice].process(raw, voiceMicroDelaySmoothed[voice].getNextValue());
            raw *= harmonyVoicing * gate;

            const float level = voiceLevelSmoothed[voice].getNextValue();
            const float effectiveGain = juce::jmax(0.0f, level * gate);
            stackGainSum += effectiveGain;
            stackGainSquares += effectiveGain * effectiveGain;
            stackVoiceWeight += gate;
            visualVoicePeaks[voice] = juce::jmax(visualVoicePeaks[voice], std::abs(raw * level));
            const float pan = voicePanSmoothed[voice].getNextValue();
            const float panPos = (juce::jlimit(-1.0f, 1.0f, pan) * 0.5f + 0.5f)
                * juce::MathConstants<float>::halfPi;

            harmonySumL += raw * level * std::cos(panPos);
            harmonySumR += raw * level * std::sin(panPos);
        }

        // Group the voices before ambience: a correlation-aware equal-power
        // normaliser retains a single voice unchanged but gently controls a
        // stack whose related vocal waveforms would otherwise add closer to
        // their peak sum than their RMS sum.
        constexpr float expectedVoiceCorrelation = 0.38f;
        const float correlatedGainSquared = stackGainSquares
            + expectedVoiceCorrelation * juce::jmax(0.0f, stackGainSum * stackGainSum - stackGainSquares);
        const float stackNormaliser = 1.0f / std::sqrt(juce::jmax(1.0f, correlatedGainSquared));
        harmonySumL *= stackNormaliser;
        harmonySumR *= stackNormaliser;

        const float stackMid = 0.5f * (harmonySumL + harmonySumR);
        const float stackSide = 0.5f * (harmonySumL - harmonySumR);
        const float multiVoiceAmount = juce::jlimit(0.0f, 1.0f, (stackVoiceWeight - 1.0f) * 0.5f);
        const float sideRetain = 1.0f - 0.18f * multiVoiceAmount;
        harmonySumL = stackMid + stackSide * sideRetain;
        harmonySumR = stackMid - stackSide * sideRetain;

        const float glueAmount = 0.025f + 0.0175f * juce::jmax(0.0f, stackVoiceWeight - 1.0f);
        harmonySumL = harmonyGlueL.process(harmonySumL, glueAmount);
        harmonySumR = harmonyGlueR.process(harmonySumR, glueAmount);

        const float ambienceNow = ambienceSmoothed.getNextValue();
        // Always clock ambience.  Previously switching Ambience from zero
        // could reveal stale delay content because the allpass state had been
        // frozen. The mix itself can still be exactly dry at zero.
        const float sharedStack = 0.5f * (harmonySumL + harmonySumR);
        const float diffL = ambienceApL2.process(ambienceApL1.process(sharedStack));
        const float diffR = ambienceApR2.process(ambienceApR1.process(sharedStack));
        const float ambienceMix = juce::jlimit(0.0f, 0.5f, ambienceNow * 0.5f);
        harmonySumL = harmonySumL * (1.0f - ambienceMix) + diffL * ambienceMix;
        harmonySumR = harmonySumR * (1.0f - ambienceMix) + diffR * ambienceMix;

        float harmonyLevelNow = harmonyLevelSmoothed.getNextValue();
        float wetL = harmonySumL * harmonyLevelNow;
        float wetR = harmonySumR * harmonyLevelNow;

        float dryNow = dryLevelSmoothed.getNextValue();
        float outL = dryOutL * dryNow + wetL;
        float outR = dryOutR * dryNow + wetR;

        const float globalSaturationNow = globalSaturationSmoothed.getNextValue();
        outL = globalSaturatorL.process(outL, globalSaturationNow);
        outR = globalSaturatorR.process(outR, globalSaturationNow);
        const float outputGain = juce::Decibels::decibelsToGain(outputGainSmoothed.getNextValue());
        outL *= outputGain;
        outR *= outputGain;
        outputLimiter.process(outL, outR);
        // A single invalid sample can otherwise persist in recursive filters
        // and present itself as crackle after a long session.
        if (!std::isfinite(outL)) outL = 0.0f;
        if (!std::isfinite(outR)) outR = 0.0f;
        // The stereo-linked limiter above already enforces the final ceiling.
        // Avoiding a second per-channel soft-clip keeps dense harmony peaks
        // wide and clean rather than adding a different distortion to L/R.
        channelL[n] = outL;
        channelR[n] = outR;
    }

    for (int i = 0; i < kNumHarmonyVoices; ++i)
        currentVoiceVisualLevels[(size_t) i].store(juce::jlimit(0.0f, 1.0f, visualVoicePeaks[(size_t) i] * 2.4f));

    const float meterFrequency = pitchDetector.getFrequency();
    const float meterConfidence = pitchDetector.getConfidence();
    currentDetectedFreq.store(std::isfinite(meterFrequency) ? juce::jmax(0.0f, meterFrequency) : 0.0f);
    currentConfidence.store(std::isfinite(meterConfidence)
        ? juce::jlimit(0.0f, 1.0f, meterConfidence) : 0.0f);
    currentHeldNoteCount.store(numHeldNotes);
}

juce::AudioProcessorEditor* MirrorAudioProcessor::createEditor()
{
    return new MirrorAudioProcessorEditor(*this);
}

void MirrorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MirrorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MirrorAudioProcessor();
}
