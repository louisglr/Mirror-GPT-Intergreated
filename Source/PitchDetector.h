#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>
#include <cmath>
#include <juce_core/juce_core.h>

// Real-time YIN tracker. Analysis is decimated to a vocal-only bandwidth and
// uses a bounded history: it is more stable at low male registers while doing
// substantially less work than a full-band, full-window scan on the audio
// thread.
class PitchDetector
{
public:
    void prepare(double inputSampleRate)
    {
        inputRate = juce::jmax(1.0, inputSampleRate);
        // Most DAWs run at 44.1/48/88.2/96 kHz. Around 16 kHz preserves all
        // F0 information needed here, while a 24 kHz-or-lower host is already
        // cheap enough to analyse at its native rate.
        decimationFactor = inputRate < 30000.0 ? 1
            : juce::jmax(1, (int) std::ceil(inputRate / kTargetAnalysisRate));
        analysisSampleRate = inputRate / (double) decimationFactor;

        // 896 samples are roughly 56 ms at 16 kHz: over three periods at
        // 55 Hz, but far cheaper than the previous 1024-sample scan.
        windowSize = 896;
        maxLag = windowSize / 2;
        hopSize = juce::jmax(1, (int) std::lround(analysisSampleRate * 0.010));

        history.assign((size_t) windowSize, 0.0f);
        temp.assign((size_t) windowSize, 0.0f);
        diffFn.assign((size_t) maxLag + 1, 0.0f);
        cmnd.assign((size_t) maxLag + 1, 1.0f);

        // Four inexpensive one-pole stages plus the decimation average give
        // the detector meaningful anti-alias rejection without allocating or
        // using an FFT in processBlock.
        const float antiAliasHz = juce::jmin(6000.0f,
            (float) analysisSampleRate * 0.30f);
        antiAliasCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi
                                         * antiAliasHz / (float) inputRate);
        reset();
    }

    void reset()
    {
        std::fill(history.begin(), history.end(), 0.0f);
        std::fill(temp.begin(), temp.end(), 0.0f);
        std::fill(diffFn.begin(), diffFn.end(), 0.0f);
        std::fill(cmnd.begin(), cmnd.end(), 1.0f);

        writePos = 0;
        hopCounter = 0;
        validHistorySamples = 0;
        decimationPhase = 0;
        decimationSum = 0.0f;
        dcState = 0.0f;
        antiAlias1 = antiAlias2 = antiAlias3 = antiAlias4 = 0.0f;
        lastFrequency = 0.0f;
        lastConfidence = 0.0f;
        pendingOctaveFrequency = 0.0f;
        pendingOctaveFrames = 0;
        unvoicedFrames = 0;
        estimateRevision = 0;
    }

    void pushSample(float x)
    {
        // A bad host/input sample must not poison a recursive state and leave
        // the tracker unstable for the rest of a session.
        if (!std::isfinite(x))
            x = 0.0f;

        dcState += 0.0015f * (x - dcState);
        const float dcFree = x - dcState;

        antiAlias1 += antiAliasCoeff * (dcFree - antiAlias1);
        antiAlias2 += antiAliasCoeff * (antiAlias1 - antiAlias2);
        antiAlias3 += antiAliasCoeff * (antiAlias2 - antiAlias3);
        antiAlias4 += antiAliasCoeff * (antiAlias3 - antiAlias4);
        decimationSum += antiAlias4;

        if (++decimationPhase < decimationFactor)
            return;

        const float analysisSample = decimationSum / (float) decimationFactor;
        decimationSum = 0.0f;
        decimationPhase = 0;

        history[(size_t) writePos] = std::isfinite(analysisSample) ? analysisSample : 0.0f;
        writePos = (writePos + 1) % windowSize;
        validHistorySamples = juce::jmin(windowSize, validHistorySamples + 1);

        if (++hopCounter >= hopSize)
        {
            hopCounter = 0;
            if (validHistorySamples >= windowSize)
                detect();
            else
                markUnvoiced();
            ++estimateRevision;
        }
    }

    float getFrequency() const { return lastFrequency; }
    float getConfidence() const { return lastConfidence; }
    std::uint32_t getRevision() const { return estimateRevision; }
    bool isPrimed() const { return validHistorySamples >= windowSize; }

    void setVocalRange(int range)
    {
        static constexpr float minHz[] = { 55.0f, 55.0f, 75.0f, 95.0f, 145.0f, 210.0f };
        static constexpr float maxHz[] = { 1100.0f, 250.0f, 350.0f, 550.0f, 750.0f, 1100.0f };
        const int index = juce::jlimit(0, 5, range);
        if (index != vocalRangeIndex)
        {
            pendingOctaveFrames = 0;
            pendingOctaveFrequency = 0.0f;
            vocalRangeIndex = index;
        }
        expectedMinHz = minHz[index];
        expectedMaxHz = maxHz[index];
    }

private:
    static constexpr double kTargetAnalysisRate = 16000.0;

    void markUnvoiced()
    {
        lastConfidence = 0.0f;
        ++unvoicedFrames;
        if (unvoicedFrames >= 3)
        {
            lastFrequency = 0.0f;
            pendingOctaveFrequency = 0.0f;
            pendingOctaveFrames = 0;
        }
    }

    bool isNearPendingOctave(float candidate) const
    {
        if (pendingOctaveFrequency <= 0.0f || !std::isfinite(pendingOctaveFrequency))
            return false;
        const float ratio = candidate / pendingOctaveFrequency;
        return ratio > 0.92f && ratio < 1.08f;
    }

    float suppressTransientOctaveJump(float candidate, float confidence)
    {
        const bool hasReliableHistory = lastFrequency > 0.0f
            && lastConfidence > 0.50f && unvoicedFrames == 0;
        if (!hasReliableHistory || confidence < 0.58f)
        {
            pendingOctaveFrames = 0;
            pendingOctaveFrequency = 0.0f;
            return candidate;
        }

        const float ratio = candidate / lastFrequency;
        const bool octaveUp = ratio > 1.78f && ratio < 2.25f;
        const bool octaveDown = ratio > 0.445f && ratio < 0.562f;
        if (!octaveUp && !octaveDown)
        {
            pendingOctaveFrames = 0;
            pendingOctaveFrequency = 0.0f;
            return candidate;
        }

        if (isNearPendingOctave(candidate))
            ++pendingOctaveFrames;
        else
        {
            pendingOctaveFrequency = candidate;
            pendingOctaveFrames = 1;
        }

        // A real octave leap is allowed after 30 ms of consistent, confident
        // evidence. Isolated harmonic errors instead stay on the established
        // fundamental and cannot make a harmony suddenly double in pitch.
        if (pendingOctaveFrames >= 3)
        {
            pendingOctaveFrames = 0;
            pendingOctaveFrequency = 0.0f;
            return candidate;
        }

        return octaveUp ? candidate * 0.5f : candidate * 2.0f;
    }

    void detect()
    {
        float energy = 0.0f;
        for (int i = 0; i < windowSize; ++i)
        {
            const float sample = history[(size_t) ((writePos + i) % windowSize)];
            temp[(size_t) i] = std::isfinite(sample) ? sample : 0.0f;
            energy += temp[(size_t) i] * temp[(size_t) i];
        }

        if (!std::isfinite(energy) || energy / (float) windowSize < 1.5e-7f)
        {
            markUnvoiced();
            return;
        }

        // Keep a common analysis region for all lags. This is both cheaper
        // than a full-length comparison per tau and avoids a short-lag bias.
        const int minTau = juce::jlimit(2, maxLag - 3,
            (int) std::floor((float) analysisSampleRate / expectedMaxHz) - 1);
        const int maxTau = juce::jlimit(minTau + 2, maxLag - 1,
            (int) std::ceil((float) analysisSampleRate / expectedMinHz) + 2);
        const int usable = windowSize - maxTau;
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            float sum = 0.0f;
            for (int j = 0; j < usable; ++j)
            {
                const float difference = temp[(size_t) j] - temp[(size_t) (j + tau)];
                sum += difference * difference;
            }
            diffFn[(size_t) tau] = sum;
        }

        diffFn[0] = 1.0f;
        cmnd[0] = 1.0f;
        float runningSum = 0.0f;
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            runningSum += diffFn[(size_t) tau];
            cmnd[(size_t) tau] = runningSum > 0.0f
                ? diffFn[(size_t) tau] * (float) tau / runningSum
                : 1.0f;
        }

        constexpr float threshold = 0.15f;
        int tauEstimate = -1;
        for (int tau = minTau; tau < maxTau; ++tau)
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
            markUnvoiced();
            return;
        }

        float betterTau = (float) tauEstimate;
        const float s0 = cmnd[(size_t) (tauEstimate - 1)];
        const float s1 = cmnd[(size_t) tauEstimate];
        const float s2 = cmnd[(size_t) (tauEstimate + 1)];
        const float denominator = 2.0f * s1 - s2 - s0;
        if (std::abs(denominator) > 1.0e-9f)
            betterTau += 0.5f * (s0 - s2) / denominator;

        const float confidence = juce::jlimit(0.0f, 1.0f, 1.0f - s1);
        float candidate = betterTau > 1.0f
            ? (float) analysisSampleRate / betterTau : 0.0f;
        if (!std::isfinite(candidate) || candidate < expectedMinHz || candidate > expectedMaxHz)
        {
            markUnvoiced();
            return;
        }

        candidate = suppressTransientOctaveJump(candidate, confidence);
        candidate = juce::jlimit(expectedMinHz, expectedMaxHz, candidate);
        const float response = confidence > 0.78f ? 0.40f : 0.22f;
        lastFrequency = lastFrequency > 0.0f
            ? lastFrequency + (candidate - lastFrequency) * response
            : candidate;
        lastFrequency = std::isfinite(lastFrequency) ? lastFrequency : 0.0f;
        lastConfidence = confidence;
        unvoicedFrames = 0;
    }

    std::vector<float> history, temp, diffFn, cmnd;
    int windowSize = 896, maxLag = 448, hopSize = 160;
    int writePos = 0, hopCounter = 0, validHistorySamples = 0;
    int decimationPhase = 0, decimationFactor = 1;
    int vocalRangeIndex = 0;
    float decimationSum = 0.0f, dcState = 0.0f;
    float antiAlias1 = 0.0f, antiAlias2 = 0.0f, antiAlias3 = 0.0f, antiAlias4 = 0.0f;
    float antiAliasCoeff = 0.3f;
    double inputRate = 44100.0, analysisSampleRate = 16000.0;

    float lastFrequency = 0.0f;
    float lastConfidence = 0.0f;
    float expectedMinHz = 55.0f, expectedMaxHz = 1100.0f;
    float pendingOctaveFrequency = 0.0f;
    int pendingOctaveFrames = 0;
    int unvoicedFrames = 0;
    std::uint32_t estimateRevision = 0;
};
