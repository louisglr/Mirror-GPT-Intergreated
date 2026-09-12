#pragma once

#include <cmath>
#include <vector>
#include <juce_core/juce_core.h>
#include "VoiceBuffer.h"

// A pitch-mark-aligned, overlap-add reader with a deliberately fixed output
// latency. A granular shifter must look farther back for an upward shift than
// for a downward shift; without that extra, fixed safety delay, its effective
// latency moves with the interval and dry/harmony alignment becomes unstable.
class GrainVoice
{
public:
    void prepare(double sampleRateIn)
    {
        sampleRate = juce::jmax(1.0, sampleRateIn);
        grainSize = juce::jmax(128, (int) std::lround(sampleRate * 0.020));
        maxGrainSize = grainSize;

        hannTable.resize((size_t) maxGrainSize + 1);
        for (int i = 0; i <= maxGrainSize; ++i)
        {
            const float phase = (float) i / (float) maxGrainSize;
            hannTable[(size_t) i] = 0.5f * (1.0f
                - std::cos(juce::MathConstants<float>::twoPi * phase));
        }

        // At the end of a maximum-ratio grain the reader can advance 1.5
        // grain lengths farther than the input write head. Hold every voice
        // back by that worst case plus interpolation headroom. This is the
        // ratio-independent value the processor reports to the host.
        const double maximumAdvance = 0.5 * (double) (grainSize - 1)
            * (double) (kMaximumPitchRatio - 1.0f);
        fixedLatencySamples = (int) std::ceil(maximumAdvance)
            + kInterpolationSafetySamples;
        activationStep = 1.0f / (float) juce::jmax(16,
            (int) std::lround(sampleRate * 0.0075));

        reset();
    }

    // The latency stays fixed for every pitch ratio accepted by process().
    // The processor may therefore align dry audio once and report correct PDC.
    int getLatencySamples() const { return fixedLatencySamples; }

    // Safe to call on the audio thread. The next process() will initialise
    // all grain readers at the current VoiceBuffer write position.
    void reset()
    {
        for (int i = 0; i < kNumGrains; ++i)
        {
            age[i] = grainSize > 0 ? (grainSize * i) / kNumGrains : 0;
            pos[i] = 0.0;
            readIncrement[i] = 1.0f;
        }
        initialised = false;
        hasLastWriteHead = false;
        activationGain = 0.0f;
    }

    // Re-prime explicitly after a bypass/enable transition when the caller
    // knows one occurred. process() also detects skipped samples itself, so
    // a forgotten caller cannot revive old circular-buffer material.
    void prime(const VoiceBuffer& vb, float pitchRatio, float sourceFrequency)
    {
        const float safeRatio = sanitiseRatio(pitchRatio);
        const int pitchMarkRadius = getPitchMarkRadius(sourceFrequency);
        const long long writeHead = vb.getWriteHead();

        for (int i = 0; i < kNumGrains; ++i)
        {
            age[i] = (grainSize * i) / kNumGrains;
            readIncrement[i] = safeRatio;
            const double desiredPosition = positionForAge(writeHead, age[i], safeRatio);
            pos[i] = shouldSnapToPitchMark(safeRatio)
                ? alignToPreviousPitchMark(vb, desiredPosition, pitchMarkRadius)
                : desiredPosition;
        }

        initialised = true;
        hasLastWriteHead = true;
        lastWriteHead = writeHead;
        activationGain = 0.0f;
    }

    float process(const VoiceBuffer& vb, float pitchRatio, float sourceFrequency)
    {
        const float safeRatio = sanitiseRatio(pitchRatio);
        const long long writeHead = vb.getWriteHead();

        bool positionsAreValid = initialised;
        for (const auto p : pos)
            positionsAreValid = positionsAreValid && std::isfinite(p);

        // GrainVoice is intentionally skipped when a harmony is inactive.
        // Reusing its old absolute positions after that gap reads stale data
        // from the circular buffer, so any discontinuity is a re-prime.
        const bool sequentialInput = hasLastWriteHead && writeHead == lastWriteHead + 1;
        const bool needsPrime = !positionsAreValid || !sequentialInput;
        if (needsPrime)
            prime(vb, safeRatio, sourceFrequency);

        const int pitchMarkRadius = getPitchMarkRadius(sourceFrequency);
        float sum = 0.0f;
        float windowSum = 0.0f;
        for (int i = 0; i < kNumGrains; ++i)
        {
            if (age[i] == 0 && !needsPrime)
                startGrain(i, vb, writeHead, safeRatio, pitchMarkRadius);

            const float sample = vb.readBandLimited(pos[i], readIncrement[i]);
            pos[i] += (double) readIncrement[i];

            const float envelope = window(age[i]);
            sum += sample * envelope;
            windowSum += envelope;

            if (++age[i] >= grainSize)
                age[i] = 0;
        }

        lastWriteHead = writeHead;
        hasLastWriteHead = true;

        float output = sum / juce::jmax(0.18f, windowSum);
        if (!std::isfinite(output))
        {
            reset();
            return 0.0f;
        }

        // A short, deterministic fade prevents a reset or voice enable from
        // creating a discontinuity at the main harmony summing bus.
        output *= activationGain;
        activationGain = juce::jmin(1.0f, activationGain + activationStep);
        return output;
    }

private:
    static constexpr int kNumGrains = 6;
    static constexpr float kMinimumPitchRatio = 0.25f;
    static constexpr float kMaximumPitchRatio = 4.0f;
    static constexpr int kInterpolationSafetySamples = 12;

    float sanitiseRatio(float pitchRatio) const
    {
        if (!std::isfinite(pitchRatio))
            return 1.0f;
        return juce::jlimit(kMinimumPitchRatio, kMaximumPitchRatio, pitchRatio);
    }

    int getPitchMarkRadius(float sourceFrequency) const
    {
        const float frequency = (std::isfinite(sourceFrequency) && sourceFrequency > 35.0f)
            ? sourceFrequency : 180.0f;
        return juce::jlimit(4, juce::jmax(4, grainSize / 4),
            (int) std::lround((float) sampleRate / frequency * 0.35f));
    }

    double positionForAge(long long writeHead, int grainAge, float pitchRatio) const
    {
        // At a grain's Hann-window centre, this evaluates to writeHead minus
        // fixedLatencySamples regardless of pitchRatio. Start/end positions
        // remain safely in the past even at the 4:1 maximum read increment.
        const double centreOffset = 0.5 * (double) grainSize - (double) grainAge;
        // VoiceBuffer increments its write head immediately after writing the
        // current source sample.  The extra one sample here makes a 1:1 grain
        // read the exact same sample as SampleAlignmentDelay(L), rather than
        // being one sample early relative to the dry path.
        return (double) writeHead - (double) fixedLatencySamples - 1.0
            - centreOffset * ((double) pitchRatio - 1.0);
    }

    bool shouldSnapToPitchMark(float pitchRatio) const
    {
        // A unison reader is a fixed delay, not a pitch shifter. Snapping its
        // six grains to separate zero crossings turns that transparent path
        // into a faint chorus and makes dry alignment needlessly ambiguous.
        return std::abs(pitchRatio - 1.0f) > 0.01f;
    }

    double alignToPreviousPitchMark(const VoiceBuffer& vb, double desiredPosition,
                                    int pitchMarkRadius) const
    {
        return vb.findNearestRisingZeroCrossingAtOrBefore(desiredPosition, pitchMarkRadius);
    }

    void startGrain(int grain, const VoiceBuffer& vb, long long writeHead,
                    float pitchRatio, int pitchMarkRadius)
    {
        readIncrement[grain] = pitchRatio;
        const double desiredPosition = positionForAge(writeHead, 0, pitchRatio);
        pos[grain] = shouldSnapToPitchMark(pitchRatio)
            ? alignToPreviousPitchMark(vb, desiredPosition, pitchMarkRadius)
            : desiredPosition;
    }

    float window(int grainAge) const
    {
        const float position = (float) grainAge / (float) grainSize * (float) maxGrainSize;
        const int i0 = juce::jlimit(0, maxGrainSize - 1, (int) position);
        const float fraction = position - (float) i0;
        return hannTable[(size_t) i0] + fraction
            * (hannTable[(size_t) (i0 + 1)] - hannTable[(size_t) i0]);
    }

    double sampleRate = 44100.0;
    int grainSize = 882;
    int maxGrainSize = 882;
    int fixedLatencySamples = 1335;
    int age[kNumGrains] {};
    double pos[kNumGrains] {};
    float readIncrement[kNumGrains] {};
    std::vector<float> hannTable;
    bool initialised = false;
    bool hasLastWriteHead = false;
    long long lastWriteHead = 0;
    float activationGain = 0.0f;
    float activationStep = 0.003f;
};
