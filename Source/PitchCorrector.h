#pragma once

#include <cmath>
#include <limits>
#include <juce_core/juce_core.h>

// Scale-aware correction with time-based transitions.  Scale quantisation is
// recomputed only on a new pitch estimate or a musical parameter change; the
// audio thread then only performs the smooth interpolation.
class PitchCorrector
{
public:
    enum ScaleType { Chromatic = 0, Major = 1, Minor = 2 };

    static constexpr int majorSteps[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr int minorSteps[7] = { 0, 2, 3, 5, 7, 8, 10 };

    void prepare(double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        smoothedRatio = targetRatio = 1.0f;
        lastTracking = -1.0f;
        lastDetectedFreq = -1.0f;
        lastRootNote = -1;
        lastScaleType = -1;
        lastQuantisedMidi = std::numeric_limits<int>::min();
        smoothingCoeff = 0.01f;
        neutralReleaseCoeff = 1.0f - std::exp(-1.0f / (float) (sampleRate * 0.090));
    }

    float process(float detectedFreq, float confidence, int rootNote, int scaleType, float trackingSmooth01)
    {
        const float tracking = juce::jlimit(0.0f, 1.0f, trackingSmooth01);
        if (std::abs(tracking - lastTracking) > 1.0e-6f)
        {
            const float timeMs = juce::jmap(tracking, 7.0f, 125.0f);
            smoothingCoeff = 1.0f - std::exp(-1.0f / (float) (sampleRate * timeMs * 0.001));
            lastTracking = tracking;
        }

        if (!std::isfinite(detectedFreq) || !std::isfinite(confidence)
            || detectedFreq <= 0.0f || confidence < 0.35f)
        {
            smoothedRatio += (1.0f - smoothedRatio) * neutralReleaseCoeff;
            return smoothedRatio;
        }

        if (std::abs(detectedFreq - lastDetectedFreq) > 1.0e-5f
            || rootNote != lastRootNote || scaleType != lastScaleType)
        {
            // A small note-boundary hysteresis prevents an otherwise stable
            // vocal from toggling between adjacent scale tones around a
            // quantisation midpoint. Large melodic moves still retarget
            // immediately.
            targetRatio = nearestScaleFreqWithHysteresis(detectedFreq, rootNote, scaleType) / detectedFreq;
            lastDetectedFreq = detectedFreq;
            lastRootNote = rootNote;
            lastScaleType = scaleType;
        }

        smoothedRatio += (targetRatio - smoothedRatio) * smoothingCoeff;
        return smoothedRatio;
    }

    static int nearestScaleMidi(float freq, int rootNote, int scaleType)
    {
        const float midiFloat = 69.0f + 12.0f * std::log2(freq / 440.0f);
        return quantizeToScale(midiFloat, rootNote, scaleType);
    }

    static int shiftByScaleSteps(int midiNote, int rootNote, int scaleType, int steps)
    {
        if (scaleType == Chromatic)
            return midiNote + steps * 2;

        const int* stepsArr = (scaleType == Major) ? majorSteps : minorSteps;
        const int relative = midiNote - rootNote;
        const int octave = (int) std::floor((float) relative / 12.0f);
        const int withinOctave = relative - octave * 12;

        int nearestStepIndex = 0;
        int bestDist = 1000;
        for (int i = 0; i < 7; ++i)
        {
            const int dist = std::abs(stepsArr[i] - withinOctave);
            if (dist < bestDist) { bestDist = dist; nearestStepIndex = i; }
        }

        const int totalStepIndex = octave * 7 + nearestStepIndex + steps;
        const int newOctave = (int) std::floor((float) totalStepIndex / 7.0f);
        const int newStepIndex = totalStepIndex - newOctave * 7;
        return rootNote + newOctave * 12 + stepsArr[newStepIndex];
    }

    static float midiToFreq(int midiNote)
    {
        return 440.0f * std::exp2((float) (midiNote - 69) / 12.0f);
    }

    // Manual scale harmonies use this same hysteretic note state as the lead
    // correction path.  Exposing it avoids a lead that is stable on one scale
    // degree while its harmony stack flips to the adjacent degree.
    int getLastQuantisedMidi() const { return lastQuantisedMidi; }

private:
    float nearestScaleFreqWithHysteresis(float freq, int rootNote, int scaleType)
    {
        const float midiFloat = 69.0f + 12.0f * std::log2(freq / 440.0f);
        const int candidate = quantizeToScale(midiFloat, rootNote, scaleType);

        if (lastQuantisedMidi == std::numeric_limits<int>::min()
            || rootNote != lastRootNote || scaleType != lastScaleType)
        {
            lastQuantisedMidi = candidate;
            return midiToFreq(lastQuantisedMidi);
        }

        if (candidate != lastQuantisedMidi)
        {
            const int distance = std::abs(candidate - lastQuantisedMidi);
            if (distance > 3)
            {
                lastQuantisedMidi = candidate;
            }
            else
            {
                const float midpoint = 0.5f * (float) (candidate + lastQuantisedMidi);
                constexpr float hysteresisSemitones = 0.18f;
                const bool movedUp = candidate > lastQuantisedMidi;
                const bool crossedBoundary = movedUp
                    ? midiFloat >= midpoint + hysteresisSemitones
                    : midiFloat <= midpoint - hysteresisSemitones;
                if (crossedBoundary)
                    lastQuantisedMidi = candidate;
            }
        }

        return midiToFreq(lastQuantisedMidi);
    }

    static int quantizeToScale(float midiFloat, int root, int scaleType)
    {
        if (scaleType == Chromatic)
            return (int) std::round(midiFloat);

        const int* steps = (scaleType == Major) ? majorSteps : minorSteps;
        const int nearestChromatic = (int) std::round(midiFloat);
        int best = nearestChromatic;
        float bestDist = 1.0e9f;
        const int baseOctave = (int) std::floor((float) (nearestChromatic - root) / 12.0f);

        for (int octaveOffset = -1; octaveOffset <= 1; ++octaveOffset)
            for (int i = 0; i < 7; ++i)
            {
                const int candidate = root + steps[i] + 12 * (baseOctave + octaveOffset);
                const float dist = std::abs((float) candidate - midiFloat);
                if (dist < bestDist) { bestDist = dist; best = candidate; }
            }
        return best;
    }

    float smoothedRatio = 1.0f, targetRatio = 1.0f;
    double sampleRate = 44100.0;
    float lastTracking = -1.0f, lastDetectedFreq = -1.0f;
    int lastRootNote = -1, lastScaleType = -1;
    int lastQuantisedMidi = std::numeric_limits<int>::min();
    float smoothingCoeff = 0.01f, neutralReleaseCoeff = 0.001f;
};
