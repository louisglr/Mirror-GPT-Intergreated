#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

// Per-voice two-pole tone shaping with a transparent automatic de-esser.
// Generated voices stay behind the lead vocal and high shifted sibilants are
// softened only when their high-band energy is excessive.
class VoiceFilter
{
public:
    void prepare(double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        reset();
        setCutoffs(100.0f, 12000.0f);
        hpCoeff = hpTarget;
        lpCoeff = lpTarget;
        deEssCoeff = coefficientForHz(5200.0f);
    }

    void reset()
    {
        hpInput1 = hpInput2 = 0.0f;
        hpOutput1 = hpOutput2 = 0.0f;
        lpState1 = lpState2 = 0.0f;
        deEssLow = deEssEnvelope = 0.0f;
        deEssGain = 1.0f;
    }

    void setCutoffs(float highPassHz, float lowPassHz)
    {
        const float sr = (float) sampleRate;
        const float hp = juce::jlimit(30.0f, sr * 0.20f, highPassHz);
        const float lp = juce::jlimit(hp * 1.7f, sr * 0.43f, lowPassHz);
        hpTarget = std::exp(-juce::MathConstants<float>::twoPi * hp / sr);
        lpTarget = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * lp / sr);
        deEssStrength = juce::jmap(lp, 6000.0f, 15000.0f, 0.48f, 0.18f);
    }

    float process(float x)
    {
        // Smooth coefficient changes so Tone automation cannot click.
        hpCoeff += 0.0025f * (hpTarget - hpCoeff);
        lpCoeff += 0.0025f * (lpTarget - lpCoeff);

        hpOutput1 = hpCoeff * (hpOutput1 + x - hpInput1);
        hpInput1 = x;
        hpOutput2 = hpCoeff * (hpOutput2 + hpOutput1 - hpInput2);
        hpInput2 = hpOutput1;

        lpState1 += lpCoeff * (hpOutput2 - lpState1);
        lpState2 += lpCoeff * (lpState1 - lpState2);
        const float shaped = lpState2;

        deEssLow += deEssCoeff * (shaped - deEssLow);
        const float sibilance = shaped - deEssLow;
        const float magnitude = std::abs(sibilance);
        const float envelopeCoeff = magnitude > deEssEnvelope ? 0.18f : 0.003f;
        deEssEnvelope += envelopeCoeff * (magnitude - deEssEnvelope);

        const float over = juce::jlimit(0.0f, 1.0f, (deEssEnvelope - 0.012f) / 0.070f);
        const float targetGain = 1.0f - deEssStrength * over;
        deEssGain += (targetGain - deEssGain) * (targetGain < deEssGain ? 0.12f : 0.0025f);

        return deEssLow + sibilance * deEssGain;
    }

private:
    float coefficientForHz(float hz) const
    {
        return 1.0f - std::exp(-juce::MathConstants<float>::twoPi * hz / (float) sampleRate);
    }

    double sampleRate = 44100.0;
    float hpTarget = 0.98f, lpTarget = 0.3f;
    float hpCoeff = 0.98f, lpCoeff = 0.3f;
    float deEssCoeff = 0.4f, deEssStrength = 0.25f, deEssGain = 1.0f;
    float hpInput1 = 0.0f, hpInput2 = 0.0f;
    float hpOutput1 = 0.0f, hpOutput2 = 0.0f;
    float lpState1 = 0.0f, lpState2 = 0.0f;
    float deEssLow = 0.0f, deEssEnvelope = 0.0f;
};
