#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

// A sample-rate-aware four-band spectral-envelope shaper. It deliberately
// stays gentler than an LPC formant resynthesiser, but gives upward and
// downward shifts more believable vocal weight than a single shelf tilt.
class FormantTilt
{
public:
    void prepare(double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        lowCoeff = coefficientForHz(520.0f);
        midCoeff = coefficientForHz(1850.0f);
        presenceCoeff = coefficientForHz(5200.0f);
        reset();
    }

    void reset()
    {
        lowState = midState = presenceState = 0.0f;
    }

    float process(float x, float amount)
    {
        if (!std::isfinite(x) || !std::isfinite(amount)
            || !std::isfinite(lowState) || !std::isfinite(midState) || !std::isfinite(presenceState))
        {
            reset();
            return 0.0f;
        }

        lowState += lowCoeff * (x - lowState);
        midState += midCoeff * (x - midState);
        presenceState += presenceCoeff * (x - presenceState);

        const float low = lowState;
        const float lowMid = midState - lowState;
        const float presence = presenceState - midState;
        const float air = x - presenceState;
        const float a = juce::jlimit(-1.0f, 1.0f, amount);
        const float magnitude = std::abs(a);

        // Positive values brighten/de-body; negative values restore weight
        // after an upward pitch shift. A small compensation prevents the
        // envelope move itself from reading as a level jump.
        const float shaped = low * (1.0f - 0.38f * a)
                           + lowMid * (1.0f - 0.13f * a)
                           + presence * (1.0f + 0.12f * a)
                           + air * (1.0f + 0.46f * a);
        const float output = shaped * (1.0f - 0.055f * magnitude);
        if (!std::isfinite(output))
        {
            reset();
            return 0.0f;
        }
        return output;
    }

private:
    float coefficientForHz(float hz) const
    {
        return 1.0f - std::exp(-juce::MathConstants<float>::twoPi * hz / (float) sampleRate);
    }

    double sampleRate = 44100.0;
    float lowCoeff = 0.08f, midCoeff = 0.32f, presenceCoeff = 0.52f;
    float lowState = 0.0f, midState = 0.0f, presenceState = 0.0f;
};
