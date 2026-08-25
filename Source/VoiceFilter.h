#pragma once

#include <array>
#include <cmath>
#include <juce_core/juce_core.h>

// Two smooth 12 dB/octave filters plus a level-relative de-esser.  Coefficients
// are interpolated safely, so Tone automation remains click-free.
class VoiceFilter
{
public:
    void prepare(double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        deEssCoeff = coefficientForHz(5400.0f);
        reset();
        setCutoffs(100.0f, 12000.0f, true);
    }

    void reset()
    {
        highPass.reset();
        lowPass.reset();
        deEssLow = deEssEnvelope = broadbandEnvelope = 0.0f;
        deEssGain = 1.0f;
    }

    void setCutoffs(float highPassHz, float lowPassHz)
    {
        setCutoffs(highPassHz, lowPassHz, false);
    }

    float process(float x)
    {
        const float shaped = lowPass.process(highPass.process(x));

        deEssLow += deEssCoeff * (shaped - deEssLow);
        const float sibilance = shaped - deEssLow;
        const float highMagnitude = std::abs(sibilance);
        const float broadMagnitude = std::abs(shaped);

        deEssEnvelope += (highMagnitude > deEssEnvelope ? 0.16f : 0.0025f)
                       * (highMagnitude - deEssEnvelope);
        broadbandEnvelope += (broadMagnitude > broadbandEnvelope ? 0.08f : 0.0015f)
                           * (broadMagnitude - broadbandEnvelope);

        const float relativeHigh = deEssEnvelope / (broadbandEnvelope + 1.0e-4f);
        const float over = broadbandEnvelope > 0.001f
            ? juce::jlimit(0.0f, 1.0f, (relativeHigh - 0.18f) / 0.52f)
            : 0.0f;
        const float targetGain = 1.0f - deEssStrength * over;
        deEssGain += (targetGain - deEssGain) * (targetGain < deEssGain ? 0.10f : 0.0018f);

        return deEssLow + sibilance * deEssGain;
    }

private:
    struct Coefficients
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    };

    class SmoothBiquad
    {
    public:
        void reset() { z1 = z2 = 0.0f; }

        void setTarget(const Coefficients& c, bool immediate)
        {
            target = c;
            if (immediate) current = target;
        }

        float process(float x)
        {
            current.b0 += 0.0025f * (target.b0 - current.b0);
            current.b1 += 0.0025f * (target.b1 - current.b1);
            current.b2 += 0.0025f * (target.b2 - current.b2);
            current.a1 += 0.0025f * (target.a1 - current.a1);
            current.a2 += 0.0025f * (target.a2 - current.a2);

            const float y = current.b0 * x + z1;
            z1 = current.b1 * x - current.a1 * y + z2;
            z2 = current.b2 * x - current.a2 * y;
            return y;
        }

    private:
        Coefficients current, target;
        float z1 = 0.0f, z2 = 0.0f;
    };

    void setCutoffs(float highPassHz, float lowPassHz, bool immediate)
    {
        const float sr = (float) sampleRate;
        const float hp = juce::jlimit(35.0f, sr * 0.18f, highPassHz);
        const float lp = juce::jlimit(hp * 1.8f, sr * 0.43f, lowPassHz);
        highPass.setTarget(makeHighPass(hp), immediate);
        lowPass.setTarget(makeLowPass(lp), immediate);
        deEssStrength = juce::jlimit(0.16f, 0.50f, juce::jmap(lp, 6000.0f, 15000.0f, 0.48f, 0.18f));
    }

    Coefficients makeLowPass(float hz) const { return makeFilter(hz, false); }
    Coefficients makeHighPass(float hz) const { return makeFilter(hz, true); }

    Coefficients makeFilter(float hz, bool highPassFilter) const
    {
        const float omega = juce::MathConstants<float>::twoPi * hz / (float) sampleRate;
        const float cosine = std::cos(omega);
        const float alpha = std::sin(omega) * 0.70710678f;
        const float a0 = 1.0f + alpha;
        Coefficients c;

        if (highPassFilter)
        {
            c.b0 = (1.0f + cosine) * 0.5f / a0;
            c.b1 = -(1.0f + cosine) / a0;
            c.b2 = c.b0;
        }
        else
        {
            c.b0 = (1.0f - cosine) * 0.5f / a0;
            c.b1 = (1.0f - cosine) / a0;
            c.b2 = c.b0;
        }

        c.a1 = -2.0f * cosine / a0;
        c.a2 = (1.0f - alpha) / a0;
        return c;
    }

    float coefficientForHz(float hz) const
    {
        return 1.0f - std::exp(-juce::MathConstants<float>::twoPi * hz / (float) sampleRate);
    }

    double sampleRate = 44100.0;
    SmoothBiquad highPass, lowPass;
    float deEssCoeff = 0.4f, deEssStrength = 0.25f, deEssGain = 1.0f;
    float deEssLow = 0.0f, deEssEnvelope = 0.0f, broadbandEnvelope = 0.0f;
};
