#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

// Glat, langsomt varierende (filtreret random-walk) modulation - IKKE hvid
// støj. Hver harmonistemme har sin egen instans, seedet forskelligt, så de
// vandrer lidt forskelligt fra hinanden (giver "separate musikere"-følelsen
// i stedet for "én stemme kopieret tre gange").
class HumanizeWalker
{
public:
    void prepare(double sampleRateIn, int seed)
    {
        sampleRate = sampleRateIn;
        random = juce::Random(seed);
        pitchVal = 0.0f; pitchTarget = 0.0f; pitchCounter = 0;
        ampVal = 0.0f; ampTarget = 0.0f; ampCounter = 0;
        lastAmount = -1.0f;
        smoothing = 0.0f;
    }

    void reset()
    {
        pitchVal = pitchTarget = 0.0f;
        ampVal = ampTarget = 0.0f;
        pitchCounter = ampCounter = 0;
    }

    struct Values { float pitchCents, ampMod; };

    Values tick(float amount)
    {
        const float boundedAmount = std::isfinite(amount)
            ? juce::jlimit(0.0f, 1.0f, amount) : 0.0f;
        if (std::abs(boundedAmount - lastAmount) > 1.0e-6f)
        {
            const float responseMs = juce::jmap(boundedAmount, 0.0f, 1.0f, 500.0f, 120.0f);
            smoothing = 1.0f - std::exp(-1.0f / (float) (sampleRate * responseMs * 0.001));
            lastAmount = boundedAmount;
        }

        Values v;
        v.pitchCents = step(pitchVal, pitchTarget, pitchCounter, boundedAmount, 15.0f) * boundedAmount;
        v.ampMod = step(ampVal, ampTarget, ampCounter, boundedAmount, 0.15f) * boundedAmount;
        return v;
    }

private:
    float step(float& val, float& target, int& counter, float /*amount*/, float range)
    {
        if (--counter <= 0)
        {
            target = (random.nextFloat() * 2.0f - 1.0f) * range;
            float ms = juce::jmap(random.nextFloat(), 300.0f, 900.0f);
            counter = juce::jmax(1, (int) (ms * 0.001 * sampleRate));
        }
        val += (target - val) * smoothing;
        if (!std::isfinite(val) || !std::isfinite(target))
        {
            val = 0.0f;
            target = 0.0f;
        }
        return val;
    }

    juce::Random random;
    double sampleRate = 44100.0;
    float pitchVal, pitchTarget; int pitchCounter;
    float ampVal, ampTarget; int ampCounter;
    float lastAmount = -1.0f;
    float smoothing = 0.0f;
};
