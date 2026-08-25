#pragma once

#include <cmath>
#include <vector>
#include <juce_core/juce_core.h>
#include "VoiceBuffer.h"

// Pitch-mark-aligned granular pitch shifter.  Twenty milliseconds is long
// enough for lower vocal fundamentals and stable overlap, but short enough
// for a compact studio latency once the dry path is aligned and reported.
class GrainVoice
{
public:
    void prepare(double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        grainSize = juce::jmax(96, (int) (sampleRate * 0.020));
        maxGrainSize = grainSize;
        hannTable.resize((size_t) maxGrainSize + 1);
        for (int i = 0; i <= maxGrainSize; ++i)
        {
            const float phase = (float) i / (float) maxGrainSize;
            hannTable[(size_t) i] = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * phase));
        }

        for (int i = 0; i < kNumGrains; ++i)
            age[i] = (grainSize * i) / kNumGrains;

        for (auto& p : pos) p = 0.0;
        initialised = false;
    }

    int getLatencySamples() const
    {
        // Four Hann windows are evenly distributed across one grain.  Their
        // perceptual centre is at half a grain, which is also the dry delay.
        return grainSize / 2;
    }

    float process(const VoiceBuffer& vb, float pitchRatio, float sourceFrequency)
    {
        if (!std::isfinite(pitchRatio))
            pitchRatio = 1.0f;
        pitchRatio = juce::jlimit(0.25f, 4.0f, pitchRatio);

        const float safeFrequency = (std::isfinite(sourceFrequency) && sourceFrequency > 35.0f)
            ? sourceFrequency : 180.0f;
        const int pitchMarkRadius = juce::jlimit(8, grainSize / 2,
            (int) std::round((float) sampleRate / safeFrequency * 0.55f));

        if (!initialised)
        {
            for (int i = 0; i < kNumGrains; ++i)
            {
                const double candidate = (double) vb.getWriteHead()
                    - (double) (grainSize - age[i]) * (double) pitchRatio - 3.0;
                pos[i] = vb.findNearestRisingZeroCrossing(candidate, pitchMarkRadius);
            }
            initialised = true;
        }

        float sum = 0.0f;
        for (int i = 0; i < kNumGrains; ++i)
        {
            if (age[i] == 0)
            {
                const double lookback = (double) grainSize * (double) pitchRatio + 3.0;
                const double candidate = (double) vb.getWriteHead() - lookback;
                pos[i] = vb.findNearestRisingZeroCrossing(candidate, pitchMarkRadius);
            }

            const float sample = vb.readInterpolated(pos[i]);
            pos[i] += pitchRatio;
            sum += sample * window(age[i]);
            age[i] = (age[i] + 1) % grainSize;
        }

        return sum * 0.5f;
    }

private:
    float window(int grainAge) const
    {
        const float position = (float) grainAge / (float) grainSize * (float) maxGrainSize;
        const int i0 = juce::jlimit(0, maxGrainSize - 1, (int) position);
        const float fraction = position - (float) i0;
        return hannTable[(size_t) i0] + fraction
            * (hannTable[(size_t) (i0 + 1)] - hannTable[(size_t) i0]);
    }

    static constexpr int kNumGrains = 4;
    double sampleRate = 44100.0;
    int grainSize = 882;
    int maxGrainSize = 882;
    int age[kNumGrains] = { 0, 0, 0, 0 };
    double pos[kNumGrains] = { 0.0, 0.0, 0.0, 0.0 };
    std::vector<float> hannTable;
    bool initialised = false;
};
