#pragma once

#include <cstdint>
#include <vector>
#include <cmath>
#include <juce_core/juce_core.h>

// YIN analysis runs close to 24 kHz, independent of the host sample rate.
// A two-pole anti-alias filter before decimation and a fixed 10 ms analysis
// cadence keep tracking consistent at 44.1, 48, 88.2 and 96 kHz.
class PitchDetector
{
public:
    void prepare(double inputSampleRate)
    {
        decimationFactor = juce::jmax(1, (int) std::ceil(inputSampleRate / 24000.0));
        analysisSampleRate = inputSampleRate / (double) decimationFactor;
        windowSize = 1024;
        maxLag = windowSize / 2;
        hopSize = juce::jmax(1, (int) std::round(analysisSampleRate * 0.010));

        history.assign((size_t) windowSize, 0.0f);
        temp.assign((size_t) windowSize, 0.0f);
        diffFn.assign((size_t) maxLag, 0.0f);
        cmnd.assign((size_t) maxLag, 1.0f);

        const float antiAliasHz = juce::jmin(9000.0f, (float) analysisSampleRate * 0.38f);
        antiAliasCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi
                                         * antiAliasHz / (float) inputSampleRate);

        writePos = hopCounter = decimationPhase = 0;
        decimationSum = dcState = antiAlias1 = antiAlias2 = 0.0f;
        lastFrequency = lastConfidence = 0.0f;
        estimateRevision = 0;
    }

    void pushSample(float x)
    {
        dcState += 0.0015f * (x - dcState);
        const float dcFree = x - dcState;

        antiAlias1 += antiAliasCoeff * (dcFree - antiAlias1);
        antiAlias2 += antiAliasCoeff * (antiAlias1 - antiAlias2);
        decimationSum += antiAlias2;

        if (++decimationPhase < decimationFactor)
            return;

        const float analysisSample = decimationSum / (float) decimationFactor;
        decimationSum = 0.0f;
        decimationPhase = 0;

        history[(size_t) writePos] = analysisSample;
        writePos = (writePos + 1) % windowSize;

        if (++hopCounter >= hopSize)
        {
            hopCounter = 0;
            detect();
            ++estimateRevision;
        }
    }

    float getFrequency() const { return lastFrequency; }
    float getConfidence() const { return lastConfidence; }
    std::uint32_t getRevision() const { return estimateRevision; }

    void setVocalRange(int range)
    {
        static constexpr float minHz[] = { 55.0f, 55.0f, 75.0f, 95.0f, 145.0f, 210.0f };
        static constexpr float maxHz[] = { 1100.0f, 250.0f, 350.0f, 550.0f, 750.0f, 1100.0f };
        const int index = juce::jlimit(0, 5, range);
        expectedMinHz = minHz[index];
        expectedMaxHz = maxHz[index];
    }

private:
    void detect()
    {
        float energy = 0.0f;
        for (int i = 0; i < windowSize; ++i)
        {
            const float sample = history[(size_t) ((writePos + i) % windowSize)];
            temp[(size_t) i] = sample;
            energy += sample * sample;
        }

        if (energy / (float) windowSize < 1.5e-7f)
        {
            lastFrequency = 0.0f;
            lastConfidence = 0.0f;
            return;
        }

        const int usable = windowSize - maxLag;
        for (int tau = 1; tau < maxLag; ++tau)
        {
            float sum = 0.0f;
            for (int j = 0; j < usable; ++j)
            {
                const float d = temp[(size_t) j] - temp[(size_t) (j + tau)];
                sum += d * d;
            }
            diffFn[(size_t) tau] = sum;
        }

        diffFn[0] = 1.0f;
        float runningSum = 0.0f;
        cmnd[0] = 1.0f;
        for (int tau = 1; tau < maxLag; ++tau)
        {
            runningSum += diffFn[(size_t) tau];
            cmnd[(size_t) tau] = runningSum > 0.0f
                ? diffFn[(size_t) tau] * (float) tau / runningSum
                : 1.0f;
        }

        constexpr float threshold = 0.15f;
        int tauEstimate = -1;
        for (int tau = 2; tau < maxLag - 1; ++tau)
        {
            if (cmnd[(size_t) tau] < threshold
                && cmnd[(size_t) tau] < cmnd[(size_t) (tau + 1)]
                && cmnd[(size_t) tau] <= cmnd[(size_t) (tau - 1)])
            {
                tauEstimate = tau;
                break;
            }
        }

        if (tauEstimate < 0)
        {
            lastConfidence = 0.0f;
            return;
        }

        float betterTau = (float) tauEstimate;
        const float s0 = cmnd[(size_t) (tauEstimate - 1)];
        const float s1 = cmnd[(size_t) tauEstimate];
        const float s2 = cmnd[(size_t) (tauEstimate + 1)];
        const float denominator = 2.0f * s1 - s2 - s0;
        if (std::abs(denominator) > 1.0e-9f)
            betterTau += 0.5f * (s0 - s2) / denominator;

        if (betterTau <= 1.0f)
        {
            lastConfidence = 0.0f;
            return;
        }

        float candidate = (float) analysisSampleRate / betterTau;
        const float confidence = juce::jlimit(0.0f, 1.0f, 1.0f - cmnd[(size_t) tauEstimate]);

        if (candidate < expectedMinHz || candidate > expectedMaxHz)
        {
            lastFrequency = 0.0f;
            lastConfidence = 0.0f;
            return;
        }

        // Correct only near-exact octave errors. Larger melodic moves remain
        // available, while the most common vocal tracker failure is suppressed.
        if (lastFrequency > 0.0f && lastConfidence > 0.50f)
        {
            const float ratio = candidate / lastFrequency;
            if (ratio > 1.84f && ratio < 2.16f && candidate * 0.5f >= expectedMinHz)
                candidate *= 0.5f;
            else if (ratio > 0.463f && ratio < 0.543f && candidate * 2.0f <= expectedMaxHz)
                candidate *= 2.0f;
        }

        const float response = confidence > 0.75f ? 0.42f : 0.25f;
        lastFrequency = lastFrequency > 0.0f
            ? lastFrequency + (candidate - lastFrequency) * response
            : candidate;
        lastConfidence = confidence;
    }

    std::vector<float> history, temp, diffFn, cmnd;
    int windowSize = 1024, maxLag = 512, hopSize = 240;
    int writePos = 0, hopCounter = 0;
    int decimationPhase = 0, decimationFactor = 2;
    float decimationSum = 0.0f, dcState = 0.0f;
    float antiAlias1 = 0.0f, antiAlias2 = 0.0f, antiAliasCoeff = 0.3f;
    double analysisSampleRate = 22050.0;

    float lastFrequency = 0.0f;
    float lastConfidence = 0.0f;
    float expectedMinHz = 55.0f, expectedMaxHz = 1100.0f;
    std::uint32_t estimateRevision = 0;
};
