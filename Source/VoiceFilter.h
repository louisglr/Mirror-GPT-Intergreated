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
        // Keep the dynamics and coefficient interpolation expressed in time,
        // not samples.  A 96 kHz session must not sound twice as twitchy as a
        // 48 kHz session.
        filterSmoothingCoeff = timeCoefficientMs(9.0f);
        deEssAttackCoeff = timeCoefficientMs(2.5f);
        deEssReleaseCoeff = timeCoefficientMs(120.0f);
        broadbandAttackCoeff = timeCoefficientMs(5.0f);
        broadbandReleaseCoeff = timeCoefficientMs(185.0f);
        gainAttackCoeff = timeCoefficientMs(1.8f);
        gainReleaseCoeff = timeCoefficientMs(150.0f);
        highPass.setSmoothingCoefficient(filterSmoothingCoeff);
        lowPass.setSmoothingCoefficient(filterSmoothingCoeff);
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
        if (!std::isfinite(x))
        {
            reset();
            return 0.0f;
        }

        const float shaped = lowPass.process(highPass.process(x));

        deEssLow += deEssCoeff * (shaped - deEssLow);
        const float sibilance = shaped - deEssLow;
        const float highMagnitude = std::abs(sibilance);
        const float broadMagnitude = std::abs(shaped);

        deEssEnvelope += (highMagnitude > deEssEnvelope ? deEssAttackCoeff : deEssReleaseCoeff)
                       * (highMagnitude - deEssEnvelope);
        broadbandEnvelope += (broadMagnitude > broadbandEnvelope ? broadbandAttackCoeff : broadbandReleaseCoeff)
                           * (broadMagnitude - broadbandEnvelope);

        const float relativeHigh = deEssEnvelope / (broadbandEnvelope + 1.0e-4f);
        const float over = broadbandEnvelope > 0.001f
            ? juce::jlimit(0.0f, 1.0f, (relativeHigh - 0.18f) / 0.52f)
            : 0.0f;
        const float targetGain = 1.0f - deEssStrength * over;
        deEssGain += (targetGain - deEssGain)
                   * (targetGain < deEssGain ? gainAttackCoeff : gainReleaseCoeff);

        const float output = deEssLow + sibilance * deEssGain;
        if (!std::isfinite(output) || !std::isfinite(deEssLow)
            || !std::isfinite(deEssEnvelope) || !std::isfinite(broadbandEnvelope)
            || !std::isfinite(deEssGain))
        {
            reset();
            return 0.0f;
        }
        return output;
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

        void setSmoothingCoefficient(float next)
        {
            smoothingCoefficient = juce::jlimit(0.0f, 1.0f, next);
        }

        void setTarget(const Coefficients& c, bool immediate)
        {
            target = c;
            if (immediate) current = target;
        }

        float process(float x)
        {
            current.b0 += smoothingCoefficient * (target.b0 - current.b0);
            current.b1 += smoothingCoefficient * (target.b1 - current.b1);
            current.b2 += smoothingCoefficient * (target.b2 - current.b2);
            current.a1 += smoothingCoefficient * (target.a1 - current.a1);
            current.a2 += smoothingCoefficient * (target.a2 - current.a2);

            const float y = current.b0 * x + z1;
            z1 = current.b1 * x - current.a1 * y + z2;
            z2 = current.b2 * x - current.a2 * y;
            return y;
        }

    private:
        Coefficients current, target;
        float z1 = 0.0f, z2 = 0.0f;
        float smoothingCoefficient = 0.0025f;
    };

    void setCutoffs(float highPassHz, float lowPassHz, bool immediate)
    {
        const float sr = (float) sampleRate;
        const float hp = juce::jlimit(35.0f, sr * 0.18f, highPassHz);
        const float lp = juce::jlimit(hp * 1.8f, sr * 0.43f, lowPassHz);

        // Tone values are block-rate. Avoid recalculating trigonometric
        // biquad coefficients when the effective cutoffs have not moved by
        // an audible amount; the filter state still runs sample-by-sample.
        if (!immediate && hasCutoffTargets
            && std::abs(hp - lastHighPassHz) < 0.25f
            && std::abs(lp - lastLowPassHz) < 2.0f)
            return;

        highPass.setTarget(makeHighPass(hp), immediate);
        lowPass.setTarget(makeLowPass(lp), immediate);
        deEssStrength = juce::jlimit(0.16f, 0.50f, juce::jmap(lp, 6000.0f, 15000.0f, 0.48f, 0.18f));
        lastHighPassHz = hp;
        lastLowPassHz = lp;
        hasCutoffTargets = true;
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

    float timeCoefficientMs(float milliseconds) const
    {
        const float samples = juce::jmax(1.0f, (float) sampleRate * milliseconds * 0.001f);
        return 1.0f - std::exp(-1.0f / samples);
    }

    double sampleRate = 44100.0;
    SmoothBiquad highPass, lowPass;
    float deEssCoeff = 0.4f, deEssStrength = 0.25f, deEssGain = 1.0f;
    float filterSmoothingCoeff = 0.0025f;
    float deEssAttackCoeff = 0.16f, deEssReleaseCoeff = 0.0025f;
    float broadbandAttackCoeff = 0.08f, broadbandReleaseCoeff = 0.0015f;
    float gainAttackCoeff = 0.10f, gainReleaseCoeff = 0.0018f;
    float deEssLow = 0.0f, deEssEnvelope = 0.0f, broadbandEnvelope = 0.0f;
    float lastHighPassHz = -1.0f, lastLowPassHz = -1.0f;
    bool hasCutoffTargets = false;
};
