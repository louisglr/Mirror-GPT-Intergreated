#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

// A compensated, slightly asymmetric soft stage.  It evaluates the curve at
// 2x internally and uses a short reconstruction average before returning to
// the host rate.  This is intentionally subtle: it keeps low-drive vocal
// colour smoother than a raw sample-rate tanh without adding a DSP-wide
// latency mode.
class WarmSaturator
{
public:
    void prepare(double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        dcCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 18.0f / (float) sampleRate);
        reset();
    }

    void reset()
    {
        dcState = 0.0f;
        previousAc = 0.0f;
        previousShaped = 0.0f;
    }

    float process(float x, float amount)
    {
        if (!std::isfinite(x) || !std::isfinite(amount) || !std::isfinite(dcState))
        {
            reset();
            return 0.0f;
        }

        const float a = juce::jlimit(0.0f, 1.0f, amount);
        if (a < 1.0e-5f)
        {
            // Keep the internal state coherent when saturation is automated
            // from bypass to audible drive.
            dcState += dcCoeff * (x - dcState);
            previousAc = x - dcState;
            previousShaped = previousAc;
            return x;
        }

        dcState += dcCoeff * (x - dcState);
        const float ac = x - dcState;
        const float drive = 1.0f + 2.4f * a;
        const float bias = 0.070f * a;
        const auto shape = [drive, bias](float in)
        {
            return (std::tanh((in + bias) * drive) - std::tanh(bias))
                / juce::jmax(0.1f, std::tanh(drive));
        };

        // A midpoint and endpoint are the 2x samples.  The 1:2:1 average
        // removes much of the foldback from the nonlinear curve while keeping
        // the stage free of allocations and practical for every voice.
        const float midpointShaped = shape(0.5f * (previousAc + ac));
        const float endpointShaped = shape(ac);
        const float reconstructed = 0.25f * previousShaped
                                  + 0.50f * midpointShaped
                                  + 0.25f * endpointShaped;
        previousAc = ac;
        previousShaped = endpointShaped;
        const float compensated = reconstructed / (1.0f + 0.10f * a);
        const float output = x + (compensated - x) * a;
        if (!std::isfinite(output) || !std::isfinite(dcState)
            || !std::isfinite(previousAc) || !std::isfinite(previousShaped))
        {
            reset();
            return 0.0f;
        }
        return output;
    }

private:
    double sampleRate = 44100.0;
    float dcCoeff = 0.002f, dcState = 0.0f;
    float previousAc = 0.0f, previousShaped = 0.0f;
};
