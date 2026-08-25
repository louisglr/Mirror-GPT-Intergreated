#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

// A sample-rate-aware three-band spectral tilt.  It is intentionally gentle:
// upward shifts retain articulation without becoming brittle, while downward
// shifts keep body without a boxy low-mid buildup.
class FormantTilt
{
public:
    void prepare(double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        lowCoeff = coefficientForHz(650.0f);
        midCoeff = coefficientForHz(2800.0f);
        reset();
    }

    void reset()
    {
        lowState = 0.0f;
        midState = 0.0f;
    }

    float process(float x, float amount)
    {
        lowState += lowCoeff * (x - lowState);
        midState += midCoeff * (x - midState);

        const float low = lowState;
        const float mid = midState - lowState;
        const float high = x - midState;
        const float a = juce::jlimit(-1.0f, 1.0f, amount);

        return low * (1.0f - 0.34f * a)
             + mid * (1.0f - 0.06f * a)
             + high * (1.0f + 0.42f * a);
    }

private:
    float coefficientForHz(float hz) const
    {
        return 1.0f - std::exp(-juce::MathConstants<float>::twoPi * hz / (float) sampleRate);
    }

    double sampleRate = 44100.0;
    float lowCoeff = 0.08f, midCoeff = 0.32f;
    float lowState = 0.0f, midState = 0.0f;
};
