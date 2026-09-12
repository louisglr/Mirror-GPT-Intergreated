#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>
#include <cmath>
#include <juce_core/juce_core.h>

// Real-time YIN tracker with bounded audio-thread work.
//
// A conventional YIN implementation performs the entire difference function
// in one hop. At a low vocal range that meant roughly 177,000 multiply/add
// operations in a *single* audio callback sample every 10 ms. The total
// analysis work is still necessary for the same detector quality, but it is
// now spread in deterministic slices over successive input samples. A frame
// snapshot is analysed in the background while the live history keeps filling;
// no allocation, resize, FFT, lock or unbounded loop happens in pushSample().
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
        frame.assign((size_t) windowSize, 0.0f);
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
        std::fill(frame.begin(), frame.end(), 0.0f);
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

        analysisState = AnalysisState::idle;
        analysisQueued = false;
        snapshotWritePos = 0;
        snapshotIndex = 0;
        frameEnergy = 0.0f;
        frameMinHz = expectedMinHz;
        frameMaxHz = expectedMaxHz;
        frameMinTau = 2;
        frameMaxTau = 4;
        frameUsable = 0;
        differenceTau = 1;
        differenceIndex = 0;
        differenceSum = 0.0f;
        cmndTau = 1;
        cmndRunningSum = 0.0f;
        scanTau = 2;
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

        if (++decimationPhase >= decimationFactor)
        {
            const float analysisSample = decimationSum / (float) decimationFactor;
            decimationSum = 0.0f;
            decimationPhase = 0;

            history[(size_t) writePos] = std::isfinite(analysisSample) ? analysisSample : 0.0f;
            writePos = (writePos + 1) % windowSize;
            validHistorySamples = juce::jmin(windowSize, validHistorySamples + 1);

            if (++hopCounter >= hopSize)
            {
                hopCounter = 0;
                requestAnalysis();
            }
        }

        // Do this on every input sample, not just on decimated samples. It
        // spreads one analysis hop across the whole host buffer and bounds the
        // worst single-sample workload even at 64-sample I/O settings.
        advanceAnalysis(kWorkUnitsPerInputSample);
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
    // One unit is one short scalar operation from Copy, Difference, CMND or
    // Scan. 640 units is over 275x smaller than the old worst YIN burst while
    // still completing a 48 kHz / 55 Hz frame before the next 10 ms hop.
    static constexpr int kWorkUnitsPerInputSample = 640;

    enum class AnalysisState
    {
        idle,
        snapshot,
        difference,
        cmnd,
        scan
    };

    void requestAnalysis()
    {
        if (validHistorySamples < windowSize)
        {
            markUnvoiced();
            ++estimateRevision;
            return;
        }

        if (analysisState == AnalysisState::idle)
            beginAnalysis();
        else
            // A boolean is intentional: a fresh snapshot after the current
            // frame is more useful than a growing queue of stale analyses.
            analysisQueued = true;
    }

    void beginAnalysis()
    {
        // writePos points to the oldest element once the ring is full. Freeze
        // it and the range for a self-consistent frame while new audio arrives.
        snapshotWritePos = writePos;
        snapshotIndex = 0;
        frameEnergy = 0.0f;
        frameMinHz = expectedMinHz;
        frameMaxHz = expectedMaxHz;
        analysisState = AnalysisState::snapshot;
    }

    void completeAnalysis()
    {
        analysisState = AnalysisState::idle;
        if (analysisQueued)
        {
            analysisQueued = false;
            beginAnalysis();
        }
    }

    void finishUnvoicedAnalysis()
    {
        markUnvoiced();
        ++estimateRevision;
        completeAnalysis();
    }

    void finishCandidateAnalysis(int tauEstimate)
    {
        float betterTau = (float) tauEstimate;
        const float s0 = cmnd[(size_t) (tauEstimate - 1)];
        const float s1 = cmnd[(size_t) tauEstimate];
        const float s2 = cmnd[(size_t) (tauEstimate + 1)];
        const float denominator = 2.0f * s1 - s2 - s0;
        if (std::abs(denominator) > 1.0e-9f)
            // Parabolic interpolation around the CMND minimum.  The
            // numerator sign matters: the opposite sign mirrors the vertex
            // around the integer bin and was about 2.75 Hz sharp for a
            // 200 Hz input at 44.1 kHz (73.5 analysis samples per period).
            betterTau += 0.5f * (s2 - s0) / denominator;

        const float confidence = juce::jlimit(0.0f, 1.0f, 1.0f - s1);
        float candidate = betterTau > 1.0f
            ? (float) analysisSampleRate / betterTau : 0.0f;
        if (!std::isfinite(candidate) || candidate < frameMinHz || candidate > frameMaxHz)
        {
            finishUnvoicedAnalysis();
            return;
        }

        candidate = suppressTransientOctaveJump(candidate, confidence);
        candidate = juce::jlimit(frameMinHz, frameMaxHz, candidate);
        const float response = confidence > 0.78f ? 0.40f : 0.22f;
        lastFrequency = lastFrequency > 0.0f
            ? lastFrequency + (candidate - lastFrequency) * response
            : candidate;
        lastFrequency = std::isfinite(lastFrequency) ? lastFrequency : 0.0f;
        lastConfidence = confidence;
        unvoicedFrames = 0;
        ++estimateRevision;
        completeAnalysis();
    }

    void advanceAnalysis(int budget)
    {
        while (budget > 0 && analysisState != AnalysisState::idle)
        {
            if (analysisState == AnalysisState::snapshot)
            {
                while (budget > 0 && snapshotIndex < windowSize)
                {
                    const float sample = history[(size_t) ((snapshotWritePos + snapshotIndex) % windowSize)];
                    const float safeSample = std::isfinite(sample) ? sample : 0.0f;
                    frame[(size_t) snapshotIndex] = safeSample;
                    frameEnergy += safeSample * safeSample;
                    ++snapshotIndex;
                    --budget;
                }

                if (snapshotIndex < windowSize)
                    break;

                if (!std::isfinite(frameEnergy)
                    || frameEnergy / (float) windowSize < 1.5e-7f)
                {
                    finishUnvoicedAnalysis();
                    continue;
                }

                // Keep a common analysis region for all lags. This matches
                // the prior detector's bias-resistant YIN calculation exactly.
                frameMinTau = juce::jlimit(2, maxLag - 3,
                    (int) std::floor((float) analysisSampleRate / frameMaxHz) - 1);
                frameMaxTau = juce::jlimit(frameMinTau + 2, maxLag - 1,
                    (int) std::ceil((float) analysisSampleRate / frameMinHz) + 2);
                frameUsable = windowSize - frameMaxTau;
                diffFn[0] = 1.0f;
                cmnd[0] = 1.0f;
                differenceTau = 1;
                differenceIndex = 0;
                differenceSum = 0.0f;
                analysisState = AnalysisState::difference;
                continue;
            }

            if (analysisState == AnalysisState::difference)
            {
                while (budget > 0 && differenceTau <= frameMaxTau)
                {
                    const float difference = frame[(size_t) differenceIndex]
                        - frame[(size_t) (differenceIndex + differenceTau)];
                    differenceSum += difference * difference;
                    ++differenceIndex;
                    --budget;

                    if (differenceIndex >= frameUsable)
                    {
                        diffFn[(size_t) differenceTau] = std::isfinite(differenceSum)
                            ? differenceSum : 0.0f;
                        ++differenceTau;
                        differenceIndex = 0;
                        differenceSum = 0.0f;
                    }
                }

                if (differenceTau <= frameMaxTau)
                    break;

                cmndTau = 1;
                cmndRunningSum = 0.0f;
                analysisState = AnalysisState::cmnd;
                continue;
            }

            if (analysisState == AnalysisState::cmnd)
            {
                while (budget > 0 && cmndTau <= frameMaxTau)
                {
                    cmndRunningSum += diffFn[(size_t) cmndTau];
                    cmnd[(size_t) cmndTau] = cmndRunningSum > 0.0f
                        ? diffFn[(size_t) cmndTau] * (float) cmndTau / cmndRunningSum
                        : 1.0f;
                    ++cmndTau;
                    --budget;
                }

                if (cmndTau <= frameMaxTau)
                    break;

                scanTau = frameMinTau;
                analysisState = AnalysisState::scan;
                continue;
            }

            // First local YIN minimum below the same threshold as the old
            // synchronous detector. This stage is also sliced so a failed
            // frame cannot create a full-lag callback spike.
            while (budget > 0 && scanTau < frameMaxTau)
            {
                constexpr float threshold = 0.15f;
                const int tau = scanTau++;
                --budget;
                if (cmnd[(size_t) tau] < threshold
                    && cmnd[(size_t) tau] < cmnd[(size_t) (tau + 1)]
                    && cmnd[(size_t) tau] <= cmnd[(size_t) (tau - 1)])
                {
                    finishCandidateAnalysis(tau);
                    break;
                }
            }

            if (analysisState == AnalysisState::scan && scanTau >= frameMaxTau)
                finishUnvoicedAnalysis();
        }
    }

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

    std::vector<float> history, frame, diffFn, cmnd;
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

    AnalysisState analysisState = AnalysisState::idle;
    bool analysisQueued = false;
    int snapshotWritePos = 0, snapshotIndex = 0;
    float frameEnergy = 0.0f, frameMinHz = 55.0f, frameMaxHz = 1100.0f;
    int frameMinTau = 2, frameMaxTau = 4, frameUsable = 0;
    int differenceTau = 1, differenceIndex = 0;
    float differenceSum = 0.0f;
    int cmndTau = 1;
    float cmndRunningSum = 0.0f;
    int scanTau = 2;
};
