#pragma once

#include <array>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PitchDetector.h"
#include "PitchCorrector.h"
#include "PhaseLockedPsolaGrainVoice.h"
#include "FormantTilt.h"
#include "VoiceFilter.h"
#include "WarmSaturator.h"
#include "HumanizeWalker.h"
#include "MusicalIntervals.h"

static constexpr int kNumHarmonyVoices = 4;
static constexpr int kMaxHeldNotes = 8;

// Lille fast-størrelse delay-linje pr. stemme til MICRO DELAY - giver hver
// stemme sin egen, faste, lette timing-forskydning (naturlig ensemble-
// fornemmelse, ikke en tydelig ekko-effekt).
class MicroDelayLine
{
public:
    void prepare(double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        int size = (int) (sampleRate * 0.06) + 8;
        buf.assign((size_t) size, 0.0f);
        writePos = 0;
    }

    float process(float x, float delayMs)
    {
        if (!std::isfinite(x))
            x = 0.0f;

        int size = (int) buf.size();
        buf[(size_t) writePos] = x;

        float delaySamples = delayMs * 0.001f * (float) sampleRate;
        float readPos = (float) writePos - delaySamples;
        while (readPos < 0.0f) readPos += (float) size;

        int i0 = (int) readPos;
        float frac = readPos - (float) i0;
        int idx0 = i0 % size;
        int idx1 = (idx0 + 1) % size;
        const float sample0 = buf[(size_t) idx0];
        const float sample1 = buf[(size_t) idx1];
        float out = sample0 + frac * (sample1 - sample0);
        if (!std::isfinite(out))
            out = 0.0f;

        writePos = (writePos + 1) % size;
        return out;
    }

private:
    std::vector<float> buf;
    int writePos = 0;
    double sampleRate = 44100.0;
};

// Simpel allpass-diffuser til AMBIENCE - udvisker transienter subtilt uden
// at tilføje en tydelig, hørbar reverb-hale.
class SimpleAllpass
{
public:
    void prepare(int delaySamples, float coeffIn)
    {
        buf.assign((size_t) juce::jmax(1, delaySamples), 0.0f);
        pos = 0;
        coeff = coeffIn;
    }

    float process(float in)
    {
        if (!std::isfinite(in))
            in = 0.0f;

        float bufOut = buf[(size_t) pos];
        if (!std::isfinite(bufOut))
            bufOut = 0.0f;
        float out = -in * coeff + bufOut;
        const float next = in + bufOut * coeff;
        buf[(size_t) pos] = std::isfinite(next) ? next : 0.0f;
        pos = (pos + 1) % (int) buf.size();
        return std::isfinite(out) ? out : 0.0f;
    }

private:
    std::vector<float> buf;
    int pos = 0;
    float coeff = 0.5f;
};


// Fixed integer delay for dry-path alignment.  It never resizes or allocates
// while processing, so enabling the high-quality harmony engine remains
// real-time safe.
class SampleAlignmentDelay
{
public:
    void prepare(int maximumDelaySamples)
    {
        buffer.assign((size_t) juce::jmax(2, maximumDelaySamples + 2), 0.0f);
        writePosition = 0;
        delaySamples = 0;
    }

    void setDelaySamples(int samples)
    {
        delaySamples = juce::jlimit(0, (int) buffer.size() - 1, samples);
    }

    float process(float input)
    {
        if (!std::isfinite(input))
            input = 0.0f;
        buffer[(size_t) writePosition] = input;
        int readPosition = writePosition - delaySamples;
        if (readPosition < 0)
            readPosition += (int) buffer.size();
        const float output = buffer[(size_t) readPosition];
        writePosition = (writePosition + 1) % (int) buffer.size();
        return std::isfinite(output) ? output : 0.0f;
    }

private:
    std::vector<float> buffer;
    int writePosition = 0;
    int delaySamples = 0;
};


// Stereo-linked safety stage. It preserves the stereo image when a dense
// stack peaks, with immediate protection and a musical recovery.
class StereoSafetyLimiter
{
public:
    void prepare(double sampleRate)
    {
        gain = 1.0f;
        releaseCoeff = 1.0f - std::exp(-1.0f / (float) (sampleRate * 0.085));
    }

    void process(float& left, float& right)
    {
        if (!std::isfinite(left) || !std::isfinite(right) || !std::isfinite(gain))
        {
            left = right = 0.0f;
            gain = 1.0f;
            return;
        }

        const float peak = juce::jmax(std::abs(left), std::abs(right));
        const float target = peak > 0.985f ? 0.985f / peak : 1.0f;
        if (target < gain)
            gain = target;
        else
            gain += (target - gain) * releaseCoeff;
        left *= gain;
        right *= gain;
    }

private:
    float gain = 1.0f, releaseCoeff = 0.002f;
};

class MirrorAudioProcessor : public juce::AudioProcessor
{
public:
    using juce::AudioProcessor::processBlock;

    MirrorAudioProcessor();
    ~MirrorAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MIRROR"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // The longest internal tail is the two-stage ambience diffuser plus the
    // short, click-free voice release.  Keep a little conservative headroom
    // so an offline render cannot trim the final diffuser repeats.
    double getTailLengthSeconds() const override { return 0.40; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> currentDetectedFreq { 0.0f };
    std::atomic<float> currentConfidence { 0.0f };
    std::atomic<int> currentHeldNoteCount { 0 };
    // UI-only meters: updated once per audio block, read by the editor timer.
    std::array<std::atomic<float>, kNumHarmonyVoices> currentVoiceVisualLevels {};

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void cacheParameterPointers();

    // APVTS lookup is convenient but should not happen from the real-time
    // callback. Pointers are stable for the lifetime of the processor.
    struct ParameterPointers
    {
        std::atomic<float>* rootNote = nullptr;
        std::atomic<float>* scaleType = nullptr;
        std::atomic<float>* vocalRange = nullptr;
        std::atomic<float>* harmonyStyle = nullptr;
        std::atomic<float>* tracking = nullptr;
        std::atomic<float>* glide = nullptr;
        std::atomic<float>* freeze = nullptr;
        std::atomic<float>* mode = nullptr;
        std::atomic<float>* midiVelocity = nullptr;
        std::atomic<float>* midiVoicing = nullptr;
        std::atomic<float>* midiInversion = nullptr;
        std::atomic<float>* dry = nullptr;
        std::atomic<float>* dryPan = nullptr;
        std::atomic<float>* dryFormant = nullptr;
        std::atomic<float>* dryPitch = nullptr;
        std::atomic<float>* dryWidth = nullptr;
        std::atomic<float>* humanize = nullptr;
        std::atomic<float>* character = nullptr;
        std::atomic<float>* spread = nullptr;
        std::atomic<float>* ambience = nullptr;
        std::atomic<float>* harmony = nullptr;
        std::atomic<float>* globalSaturation = nullptr;
        std::atomic<float>* outputGain = nullptr;

        std::array<std::atomic<float>*, kNumHarmonyVoices> voiceEnable {};
        std::array<std::atomic<float>*, kNumHarmonyVoices> voiceSolo {};
        std::array<std::atomic<float>*, kNumHarmonyVoices> voiceInterval {};
        std::array<std::atomic<float>*, kNumHarmonyVoices> voiceLevel {};
        std::array<std::atomic<float>*, kNumHarmonyVoices> voicePan {};
        std::array<std::atomic<float>*, kNumHarmonyVoices> voiceFormant {};
        std::array<std::atomic<float>*, kNumHarmonyVoices> voiceFineTune {};
        std::array<std::atomic<float>*, kNumHarmonyVoices> voiceTone {};
        std::array<std::atomic<float>*, kNumHarmonyVoices> voiceSaturation {};
        std::array<std::atomic<float>*, kNumHarmonyVoices> voiceMicroDelay {};
        std::array<std::atomic<float>*, kNumHarmonyVoices> voiceVibrato {};
        std::array<std::atomic<float>*, kNumHarmonyVoices> voiceVibratoRate {};
    };
    ParameterPointers parameterValues;

    VoiceBuffer voiceBuffer;
    PitchDetector pitchDetector;
    PitchCorrector pitchCorrector;

    PhaseLockedPsolaGrainVoice dryVoice;
    SampleAlignmentDelay dryAlignmentL, dryAlignmentR;
    // Formant filtering needs independent state per channel; sharing one
    // filter state made an active dry formant control collapse stereo to mono.
    FormantTilt dryFormantProcL, dryFormantProcR;

    std::array<PhaseLockedPsolaGrainVoice, kNumHarmonyVoices> harmonyVoices;
    std::array<FormantTilt, kNumHarmonyVoices> harmonyFormant;
    std::array<VoiceFilter, kNumHarmonyVoices> harmonyFilters;
    std::array<WarmSaturator, kNumHarmonyVoices> harmonySaturators;
    // A low-drive shared bus for cohesion before dry/wet summing.
    WarmSaturator harmonyGlueL, harmonyGlueR;
    WarmSaturator globalSaturatorL, globalSaturatorR;
    StereoSafetyLimiter outputLimiter;
    std::array<HumanizeWalker, kNumHarmonyVoices> harmonyHumanize;
    std::array<MicroDelayLine, kNumHarmonyVoices> harmonyMicroDelay;
    std::array<float, kNumHarmonyVoices> voiceRatioSmoothed;
    std::array<float, kNumHarmonyVoices> voiceVibratoPhase;
    std::array<float, kNumHarmonyVoices> voiceLastMidi;
    int lastStableBaseMidi = 69;
    std::uint32_t lastHandledPitchRevision = 0;

    SimpleAllpass ambienceApL1, ambienceApL2, ambienceApR1, ambienceApR2;

    std::array<int, kMaxHeldNotes> heldNotes {};
    std::array<float, kMaxHeldNotes> heldNoteVelocities {};
    std::array<bool, 128> physicalKeys {};
    int numHeldNotes = 0;
    bool sustainPedalDown = false;
    std::array<int, kNumHarmonyVoices> midiAssignedNotes {};
    std::array<float, kNumHarmonyVoices> midiAssignedVelocities {};
    std::array<float, kNumHarmonyVoices> midiAssignedFrequencies {};
    // A voice without a previous MIDI target should begin from the chord as
    // played, rather than being pulled toward the arbitrary initial register
    // used to seed the voice-leading memory.
    std::array<bool, kNumHarmonyVoices> midiVoiceHasTargets {};
    // MIDI target assignment deliberately follows only voices that are
    // currently audible (enabled, or soloed when any solo is active).  This
    // avoids a muted voice silently consuming a chord tone.
    std::array<bool, kNumHarmonyVoices> midiVoiceParticipates {};
    bool midiAssignmentsDirty = true;
    int lastMidiVoicing = -1, lastMidiInversion = -1;
    void removeHeldNote(int note);
    void rebuildMidiAssignments(int midiVoicing, int midiInversion,
                                const std::array<bool, kNumHarmonyVoices>& participatingVoices);
    void handleMidiMessage(const juce::MidiMessage& m);

    float frozenLeadRatio = 1.0f;
    // Adaptive, confidence-weighted voicing fades generated material in and
    // out instead of hard-switching around breaths and consonants.
    float harmonyVoicing = 0.0f;
    float inputEnvelope = 0.0f;
    float voicingAttackCoeff = 0.01f, voicingReleaseCoeff = 0.001f;
    float voicingUnvoicedReleaseCoeff = 0.004f;
    // A separate per-voice gate gives MIDI note-offs and Enable/Solo changes
    // a musical release instead of abruptly cutting a granular reader.
    std::array<float, kNumHarmonyVoices> voiceRenderGains {};
    float voiceGateAttackCoeff = 0.01f, voiceGateReleaseCoeff = 0.001f;
    int reportedLatencySamples = 0;

    juce::SmoothedValue<float> dryLevelSmoothed, harmonyLevelSmoothed, dryWidthSmoothed, outputGainSmoothed;
    // Both dry paths always run; this blend crossfades between the stereo
    // aligned lead and the granular pitch-shifted lead without replaying a
    // frozen delay buffer when Dry Pitch crosses zero.
    juce::SmoothedValue<float> dryPitchBlendSmoothed;
    juce::SmoothedValue<float> dryFormantSmoothed, ambienceSmoothed, globalSaturationSmoothed;
    std::array<juce::SmoothedValue<float>, kNumHarmonyVoices> voiceLevelSmoothed, voicePanSmoothed, voiceSaturationSmoothed, voiceMicroDelaySmoothed;
    std::array<juce::SmoothedValue<float>, kNumHarmonyVoices> voiceFormantSmoothed;

    double currentSampleRate = 44100.0;
    float inputEnvelopeAttackCoeff = 0.08f;
    float inputEnvelopeReleaseCoeff = 0.0015f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MirrorAudioProcessor)
};
