#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

// A compensated, slightly asymmetric soft stage.  It is deliberately subtle:
// the surrounding voice EQ/de-esser then shapes the added harmonics rather
// than letting a raw digital clipper make the stack brittle.
class WarmSaturator
{
public:
    void prepare(double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        dcCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 18.0f / (float) sampleRate);
        reset();
    }

    void reset() { dcState = 0.0f; }

    float process(float x, float amount)
    {
        const float a = juce::jlimit(0.0f, 1.0f, amount);
        if (a < 1.0e-5f)
            return x;

        dcState += dcCoeff * (x - dcState);
        const float ac = x - dcState;
        const float drive = 1.0f + 2.4f * a;
        const float bias = 0.070f * a;
        const float shaped = (std::tanh((ac + bias) * drive) - std::tanh(bias))
                           / juce::jmax(0.1f, std::tanh(drive));
        const float compensated = shaped / (1.0f + 0.12f * a);
        return x + (compensated - x) * a;
    }

private:
    double sampleRate = 44100.0;
    float dcCoeff = 0.002f, dcState = 0.0f;
};
