#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    float softSafetyLimit(float x)
    {
        const float sign = x < 0.0f ? -1.0f : 1.0f;
        const float magnitude = std::abs(x);
        if (magnitude <= 0.95f)
            return x;
        const float excess = magnitude - 0.95f;
        return sign * (0.95f + excess / (1.0f + 4.0f * excess));
    }
}

MirrorAudioProcessor::MirrorAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

MirrorAudioProcessor::~MirrorAudioProcessor() {}

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
    midiAssignmentsDirty = true;
    lastMidiVoicing = lastMidiInversion = -1;
    lastHandledPitchRevision = pitchDetector.getRevision();
    harmonyVoicing = 0.0f;
    inputEnvelope = 0.0f;
    voicingAttackCoeff = 1.0f - std::exp(-1.0f / (float) (sampleRate * 0.012));
    voicingReleaseCoeff = 1.0f - std::exp(-1.0f / (float) (sampleRate * 0.065));

    dryLevelSmoothed.reset(sampleRate, 0.03);
    harmonyLevelSmoothed.reset(sampleRate, 0.03);
    dryWidthSmoothed.reset(sampleRate, 0.05);
    outputGainSmoothed.reset(sampleRate, 0.04);
    for (auto& s : voiceLevelSmoothed) s.reset(sampleRate, 0.03);
    for (auto& s : voicePanSmoothed) s.reset(sampleRate, 0.05);

    dryLevelSmoothed.setCurrentAndTargetValue(*apvts.getRawParameterValue("dry"));
    harmonyLevelSmoothed.setCurrentAndTargetValue(*apvts.getRawParameterValue("harmony"));
    dryWidthSmoothed.setCurrentAndTargetValue(*apvts.getRawParameterValue("dryWidth"));
    outputGainSmoothed.setCurrentAndTargetValue(*apvts.getRawParameterValue("outputGain"));
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

void MirrorAudioProcessor::rebuildMidiAssignments(int midiVoicing, int midiInversion)
{
    midiAssignmentsDirty = false;
    lastMidiVoicing = midiVoicing;
    lastMidiInversion = midiInversion;

    if (numHeldNotes == 0)
        return;

    std::array<int, kMaxHeldNotes> chordNotes {};
    std::array<float, kMaxHeldNotes> chordVelocities {};
    for (int j = 0; j < numHeldNotes; ++j)
    {
        chordNotes[(size_t) j] = heldNotes[(size_t) j];
        chordVelocities[(size_t) j] = heldNoteVelocities[(size_t) j];
    }

    const int inversion = midiInversion == 0 ? 0 : midiInversion - 1;
    for (int r = 0; r < inversion && numHeldNotes > 1; ++r)
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

    std::array<int, kMaxHeldNotes> paletteNotes {};
    std::array<float, kMaxHeldNotes> paletteVelocities {};
    for (int p = 0; p < kMaxHeldNotes; ++p)
    {
        const int chordIndex = p % numHeldNotes;
        int octave = p / numHeldNotes;
        if (midiVoicing == 1 && chordIndex > 0)
            ++octave;
        else if (midiVoicing == 2)
            octave += p / 2;
        paletteNotes[(size_t) p] = chordNotes[(size_t) chordIndex] + 12 * octave;
        paletteVelocities[(size_t) p] = chordVelocities[(size_t) chordIndex];
    }

    std::array<bool, kMaxHeldNotes> claimed {};
    for (int v = 0; v < kNumHarmonyVoices; ++v)
    {
        int bestIndex = -1;
        float bestDistance = 1.0e9f;
        for (int p = 0; p < kMaxHeldNotes; ++p)
        {
            if (claimed[(size_t) p]) continue;
            const float distance = std::abs((float) paletteNotes[(size_t) p] - voiceLastMidi[(size_t) v]);
            if (distance < bestDistance) { bestDistance = distance; bestIndex = p; }
        }
        if (bestIndex < 0) bestIndex = 0;
        claimed[(size_t) bestIndex] = true;
        midiAssignedNotes[(size_t) v] = paletteNotes[(size_t) bestIndex];
        midiAssignedVelocities[(size_t) v] = paletteVelocities[(size_t) bestIndex];
        midiAssignedFrequencies[(size_t) v] = PitchCorrector::midiToFreq(midiAssignedNotes[(size_t) v]);
        voiceLastMidi[(size_t) v] = (float) midiAssignedNotes[(size_t) v];
    }
}

void MirrorAudioProcessor::handleMidiMessage(const juce::MidiMessage& m)
{
    midiAssignmentsDirty = true;
    if (m.isNoteOn())
    {
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
        const int note = juce::jlimit(0, 127, m.getNoteNumber());
        physicalKeys[(size_t) note] = false;
        if (!sustainPedalDown)
            removeHeldNote(note);
        return;
    }

    if (m.isController() && m.getControllerNumber() == 64)
    {
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

    int rootNote = (int) *apvts.getRawParameterValue("rootNote");
    int scaleType = (int) *apvts.getRawParameterValue("scaleType");
    int vocalRange = (int) *apvts.getRawParameterValue("vocalRange");
    int harmonyStyle = (int) *apvts.getRawParameterValue("harmonyStyle");
    pitchDetector.setVocalRange(vocalRange);
    // Locked performance defaults: the engine always tracks and transitions
    // at its highest-quality setting.  The remaining musical balance lives
    // in the dry and per-voice controls.
    constexpr float tracking = 1.0f;
    constexpr float glide = 1.0f;
    bool freeze = *apvts.getRawParameterValue("freeze") > 0.5f;
    int mode = (int) *apvts.getRawParameterValue("mode");
    float midiVelocitySensitivity = *apvts.getRawParameterValue("midiVelocity");
    int midiVoicing = (int) *apvts.getRawParameterValue("midiVoicing");
    int midiInversion = (int) *apvts.getRawParameterValue("midiInversion");

    float humanizeAmt = *apvts.getRawParameterValue("humanize");
    float character = *apvts.getRawParameterValue("character");
    float spread = *apvts.getRawParameterValue("spread") * 2.0f;
    float ambienceAmt = *apvts.getRawParameterValue("ambience");
    float globalSaturation = *apvts.getRawParameterValue("globalSaturation");

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

    float dryPanP = *apvts.getRawParameterValue("dryPan");
    float dryFormantP = *apvts.getRawParameterValue("dryFormant");
    float dryPitchSemis = *apvts.getRawParameterValue("dryPitch");
    const float dryPitchRatio = std::exp2(dryPitchSemis / 12.0f);

    dryLevelSmoothed.setTargetValue(*apvts.getRawParameterValue("dry"));
    harmonyLevelSmoothed.setTargetValue(1.0f);
    dryWidthSmoothed.setTargetValue(*apvts.getRawParameterValue("dryWidth"));
    outputGainSmoothed.setTargetValue(*apvts.getRawParameterValue("outputGain"));

    struct VoiceParams
    {
        bool enable, solo; int intervalIdx; float formant, fineTune, tone, saturation, microDelayMs, vibrato, vibratoRate;
    };
    std::array<VoiceParams, kNumHarmonyVoices> vp;
    bool anySolo = false;
    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        juce::String idx(i + 1);
        vp[(size_t) i].enable = *apvts.getRawParameterValue("voiceEnable" + idx) > 0.5f;
        vp[(size_t) i].solo = *apvts.getRawParameterValue("voiceSolo" + idx) > 0.5f;
        vp[(size_t) i].intervalIdx = (int) *apvts.getRawParameterValue("voiceInterval" + idx);
        vp[(size_t) i].formant = *apvts.getRawParameterValue("voiceFormant" + idx)
                                  + character * 0.08f * ((i % 2 == 0) ? 1.0f : -1.0f);
        vp[(size_t) i].fineTune = *apvts.getRawParameterValue("voiceFineTune" + idx);
        vp[(size_t) i].tone = juce::jlimit(0.0f, 1.0f, *apvts.getRawParameterValue("voiceTone" + idx) + character * 0.1f);
        vp[(size_t) i].saturation = juce::jlimit(0.0f, 1.0f, *apvts.getRawParameterValue("voiceSaturation" + idx) + character * 0.12f);
        vp[(size_t) i].microDelayMs = *apvts.getRawParameterValue("voiceMicroDelay" + idx);
        vp[(size_t) i].vibrato = *apvts.getRawParameterValue("voiceVibrato" + idx);
        vp[(size_t) i].vibratoRate = *apvts.getRawParameterValue("voiceVibratoRate" + idx);

        // The four voices occupy deliberately different spectral spaces:
        // clear lead support, darker width, low-mid body and airy texture.
        static constexpr float baseHighPassHz[kNumHarmonyVoices] = { 70.0f, 100.0f, 140.0f, 220.0f };
        static constexpr float baseLowPassHz[kNumHarmonyVoices] = { 15000.0f, 11800.0f, 9000.0f, 10500.0f };
        const float highPassHz = baseHighPassHz[i] * (1.0f + vp[(size_t) i].tone * 0.45f);
        const float lowPassHz = baseLowPassHz[i] * juce::jmap(vp[(size_t) i].tone, 0.0f, 1.0f, 1.0f, 0.58f);
        harmonyFilters[(size_t) i].setCutoffs(highPassHz, lowPassHz);

        voiceLevelSmoothed[(size_t) i].setTargetValue(*apvts.getRawParameterValue("voiceLevel" + idx));
        float panSpreadAmt = juce::jlimit(0.0f, 2.0f, spread + character * 0.1f);
        voicePanSmoothed[(size_t) i].setTargetValue(*apvts.getRawParameterValue("voicePan" + idx) * panSpreadAmt);
        if (vp[(size_t) i].solo) anySolo = true;
    }

    std::array<float, kNumHarmonyVoices> fineTuneRatios {};
    std::array<float, kNumHarmonyVoices> chromaticRatios {};
    std::array<float, kNumHarmonyVoices> manualScaleFrequencies {};
    int activeHarmonyVoiceCount = 0;
    for (int i = 0; i < kNumHarmonyVoices; ++i)
        if (vp[(size_t) i].enable && (!anySolo || vp[(size_t) i].solo))
            ++activeHarmonyVoiceCount;

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

    if (mode == 1 && (midiAssignmentsDirty || midiVoicing != lastMidiVoicing
                      || midiInversion != lastMidiInversion))
        rebuildMidiAssignments(midiVoicing, midiInversion);

    const float glideTimeMs = juce::jmap(juce::jlimit(0.0f, 1.0f, glide), 8.0f, 155.0f);
    const float glideCoeff = 1.0f - std::exp(-1.0f / (float) (currentSampleRate * glideTimeMs * 0.001));

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
            rebuildMidiAssignments(midiVoicing, midiInversion);

        float dryL = channelL[n];
        float dryR = inputChannels > 1 ? channelR[n] : dryL;
        float monoIn = 0.5f * (dryL + dryR);

        voiceBuffer.write(monoIn);
        pitchDetector.pushSample(monoIn);

        const float inputMagnitude = std::abs(monoIn);
        const float inputEnvelopeCoeff = inputMagnitude > inputEnvelope ? 0.08f : 0.0015f;
        inputEnvelope += (inputMagnitude - inputEnvelope) * inputEnvelopeCoeff;

        float detectedFreq = pitchDetector.getFrequency();
        float confidence = pitchDetector.getConfidence();

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

        float dryProcL, dryProcR;
        if (std::abs(dryPitchSemis) < 0.001f)
        {
            // The harmony grains are centred half a grain in the past.  Delay
            // the unshifted lead by that same amount and report it to the DAW:
            // dry and harmony lock together, while plug-in delay compensation
            // keeps the track aligned with the session.
            dryProcL = dryAlignmentL.process(dryL);
            dryProcR = dryAlignmentR.process(dryR);
        }
        else
        {
            float shifted = dryVoice.process(voiceBuffer, dryPitchRatio, detectedFreq);
            dryProcL = shifted;
            dryProcR = shifted;
        }
        // A pitch shift moves formants with the source.  Counteracting part
        // of that movement makes upward voices less chipmunk-like and
        // downward voices less muffled; the user control remains additive.
        const float dryFormantAmount = juce::jlimit(-1.0f, 1.0f,
            dryFormantP - 0.45f * dryPitchSemis / 12.0f);
        if (std::abs(dryFormantAmount) > 0.001f)
        {
            dryProcL = dryFormantProcL.process(dryProcL, dryFormantAmount);
            dryProcR = dryFormantProcR.process(dryProcR, dryFormantAmount);
        }

        // A continuous pitch-and-level confidence gate avoids both the
        // metallic breath artefacts of a fully wet granular voice and the
        // obvious on/off edge of a binary voicing threshold.
        const float pitchGate = juce::jlimit(0.0f, 1.0f, (confidence - 0.25f) / 0.33f);
        const float smoothPitchGate = pitchGate * pitchGate * (3.0f - 2.0f * pitchGate);
        const float levelGate = juce::jlimit(0.0f, 1.0f, (inputEnvelope - 0.0008f) / 0.0072f);
        const float voicingTarget = smoothPitchGate * levelGate;
        const float voicingCoeff = voicingTarget > harmonyVoicing
            ? voicingAttackCoeff : voicingReleaseCoeff;
        harmonyVoicing += (voicingTarget - harmonyVoicing) * voicingCoeff;

        float dryWidthNow = dryWidthSmoothed.getNextValue() * 2.0f;
        float mid = 0.5f * (dryProcL + dryProcR);
        float side = 0.5f * (dryProcL - dryProcR) * dryWidthNow;
        float widenedL = mid + side;
        float widenedR = mid - side;
        float dryPanPos = (juce::jlimit(-1.0f, 1.0f, dryPanP) * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
        float dryOutL = widenedL * std::cos(dryPanPos) * 1.4142f;
        float dryOutR = widenedR * std::sin(dryPanPos) * 1.4142f;

        float harmonySumL = 0.0f, harmonySumR = 0.0f;

        const auto pitchRevision = pitchDetector.getRevision();
        if (pitchRevision != lastHandledPitchRevision)
        {
            lastHandledPitchRevision = pitchRevision;
            if (detectedFreq > 20.0f)
            {
                const int rawBaseMidi = PitchCorrector::nearestScaleMidi(detectedFreq, rootNote, scaleType);
                if (confidence > 0.45f)
                    lastStableBaseMidi = rawBaseMidi;
            }
        }
        if (mode == 0 && scaleType != PitchCorrector::Chromatic
            && manualScaleBaseMidi != lastStableBaseMidi)
            refreshManualScaleFrequencies();

        for (int i = 0; i < kNumHarmonyVoices; ++i)
        {
            bool active = vp[(size_t) i].enable && (!anySolo || vp[(size_t) i].solo);

            if (mode == 1 && numHeldNotes == 0)
                active = false;

            if (!active)
            {
                voiceRatioSmoothed[(size_t) i] += (1.0f - voiceRatioSmoothed[(size_t) i]) * glideCoeff;
                continue;
            }

            auto hz = harmonyHumanize[(size_t) i].tick(humanizeAmt);
            // ±15 cents of drift and ±16 cents of vibrato are accurate enough
            // with this linear conversion, avoiding four exp/pow calls per
            // sample while retaining inaudible error at these tiny depths.
            constexpr float centsToRatio = 0.0005777895f;
            const float driftRatio = juce::jmax(0.5f, 1.0f + hz.pitchCents * centsToRatio);

            float targetRatio = 1.0f;
            if (mode == 0)
            {
                if (scaleType == PitchCorrector::Chromatic)
                    targetRatio = leadRatio * chromaticRatios[(size_t) i] * driftRatio;
                else if (detectedFreq > 20.0f)
                    targetRatio = manualScaleFrequencies[(size_t) i] * fineTuneRatios[(size_t) i]
                                * driftRatio / detectedFreq;
            }
            else if (detectedFreq > 20.0f)
            {
                targetRatio = midiAssignedFrequencies[(size_t) i] * fineTuneRatios[(size_t) i]
                            * driftRatio / detectedFreq;
            }

            voiceRatioSmoothed[(size_t) i] += (targetRatio - voiceRatioSmoothed[(size_t) i]) * glideCoeff;

            // Vibrato is a continuous sine modulation, deliberately separate
            // from Humanize's random drift to avoid zipper/saw artefacts.
            float readRatio = voiceRatioSmoothed[(size_t) i];
            if (vp[(size_t) i].vibrato > 1.0e-4f)
            {
                const float vibratoRateHz = juce::jmap(vp[(size_t) i].vibratoRate, 3.0f, 7.2f)
                    + (float) i * 0.12f;
                voiceVibratoPhase[(size_t) i] += juce::MathConstants<float>::twoPi
                    * vibratoRateHz / (float) currentSampleRate;
                if (voiceVibratoPhase[(size_t) i] >= juce::MathConstants<float>::twoPi)
                    voiceVibratoPhase[(size_t) i] -= juce::MathConstants<float>::twoPi;
                const float vibratoCents = std::sin(voiceVibratoPhase[(size_t) i])
                    * vp[(size_t) i].vibrato * 16.0f;
                readRatio *= juce::jmax(0.5f, 1.0f + vibratoCents * 0.0005777895f);
            }

            float raw = harmonyVoices[(size_t) i].process(voiceBuffer, readRatio, detectedFreq);
            const float shiftSemitones = 12.0f * std::log2(
                juce::jmax(0.01f, readRatio));
            const float formantAmount = juce::jlimit(-1.0f, 1.0f,
                vp[(size_t) i].formant - 0.45f * shiftSemitones / 12.0f);
            raw = harmonyFormant[(size_t) i].process(raw, formantAmount);

            // Colour before the final tone/de-ess stage.  This keeps the
            // harmonic warmth while the voice EQ removes brittle by-products.
            raw = harmonySaturators[(size_t) i].process(raw, vp[(size_t) i].saturation);
            raw = harmonyFilters[(size_t) i].process(raw);

            raw *= (1.0f + hz.ampMod);
            if (mode == 1)
            {
                const float velocityGain = 0.35f + 0.65f * midiAssignedVelocities[(size_t) i];
                raw *= 1.0f + midiVelocitySensitivity * (velocityGain - 1.0f);
            }

            // Modulating a short fractional delay is effectively a second
            // pitch shifter.  It was the remaining source of the sharp,
            // saw-like artefact when Humanize was raised.  Keep Micro Delay
            // stable; Humanize still supplies gentle pitch and level drift.
            raw = harmonyMicroDelay[(size_t) i].process(raw, vp[(size_t) i].microDelayMs);
            raw *= harmonyVoicing;

            float level = voiceLevelSmoothed[(size_t) i].getNextValue();
            visualVoicePeaks[(size_t) i] = juce::jmax(visualVoicePeaks[(size_t) i], std::abs(raw * level));
            float pan = voicePanSmoothed[(size_t) i].getNextValue();
            float panPos = (juce::jlimit(-1.0f, 1.0f, pan) * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;

            harmonySumL += raw * level * std::cos(panPos);
            harmonySumR += raw * level * std::sin(panPos);
        }

        // Group the voices before ambience: modest level normalisation, a
        // controlled mid/side fold, and a shared warm bus make four separate
        // generators feel like one coherent vocal layer rather than four
        // competing takes.
        const int extraVoices = juce::jmax(0, activeHarmonyVoiceCount - 1);
        const float stackNormaliser = 1.0f / (1.0f + 0.16f * (float) extraVoices);
        harmonySumL *= stackNormaliser;
        harmonySumR *= stackNormaliser;

        const float stackMid = 0.5f * (harmonySumL + harmonySumR);
        const float stackSide = 0.5f * (harmonySumL - harmonySumR);
        const float sideRetain = activeHarmonyVoiceCount > 1 ? 0.82f : 1.0f;
        harmonySumL = stackMid + stackSide * sideRetain;
        harmonySumR = stackMid - stackSide * sideRetain;

        const float glueAmount = 0.025f + 0.0175f * (float) extraVoices;
        harmonySumL = harmonyGlueL.process(harmonySumL, glueAmount);
        harmonySumR = harmonyGlueR.process(harmonySumR, glueAmount);

        if (ambienceAmt > 0.001f)
        {
            // The diffuser is fed a shared signal.  It gives the voices a
            // common room instead of adding a different haze to each side.
            const float sharedStack = 0.5f * (harmonySumL + harmonySumR);
            const float diffL = ambienceApL2.process(ambienceApL1.process(sharedStack));
            const float diffR = ambienceApR2.process(ambienceApR1.process(sharedStack));
            harmonySumL = harmonySumL * (1.0f - ambienceAmt * 0.5f) + diffL * ambienceAmt * 0.5f;
            harmonySumR = harmonySumR * (1.0f - ambienceAmt * 0.5f) + diffR * ambienceAmt * 0.5f;
        }

        float harmonyLevelNow = harmonyLevelSmoothed.getNextValue();
        float wetL = harmonySumL * harmonyLevelNow;
        float wetR = harmonySumR * harmonyLevelNow;

        float dryNow = dryLevelSmoothed.getNextValue();
        float outL = dryOutL * dryNow + wetL;
        float outR = dryOutR * dryNow + wetR;

        outL = globalSaturatorL.process(outL, globalSaturation);
        outR = globalSaturatorR.process(outR, globalSaturation);
        const float outputGain = juce::Decibels::decibelsToGain(outputGainSmoothed.getNextValue());
        outL *= outputGain;
        outR *= outputGain;
        outputLimiter.process(outL, outR);
        // A single invalid sample can otherwise persist in recursive filters
        // and present itself as crackle after a long session.
        if (!std::isfinite(outL)) outL = 0.0f;
        if (!std::isfinite(outR)) outR = 0.0f;
        channelL[n] = softSafetyLimit(outL);
        channelR[n] = softSafetyLimit(outR);
    }

    for (int i = 0; i < kNumHarmonyVoices; ++i)
        currentVoiceVisualLevels[(size_t) i].store(juce::jlimit(0.0f, 1.0f, visualVoicePeaks[(size_t) i] * 2.4f));

    currentDetectedFreq.store(pitchDetector.getFrequency());
    currentConfidence.store(pitchDetector.getConfidence());
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
