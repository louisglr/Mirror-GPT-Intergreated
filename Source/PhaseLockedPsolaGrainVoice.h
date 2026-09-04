#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include <juce_core/juce_core.h>
#include "VoiceBuffer.h"

// A phase-locked, pitch-mark-assisted overlap-add shifter.
//
// Its important invariant is stronger than "start every grain near a zero
// crossing": for a stationary source with period T, every active grain obeys
//
//     readPosition[i] = pitchMark[i] + phase(n) + k[i] * T,
//     phase(n + 1) = phase(n) + pitchRatio   (mod T).
//
// Consequently all grains read the *same source phase* at a given output
// sample.  For x[n] = sin(2*pi*n/T), the normalised OLA sum is therefore
// sin(2*pi*phase(n)/T), i.e. exactly pitchRatio times the source frequency
// (apart from the interpolation filter), including non-integer ratios such as
// 1.05 and 1.5.  A conventional fixed-hop granular reader does not satisfy
// this invariant: its grain starts advance by the output hop rather than the
// requested synthesis phase, leaving a (1 - ratio) * hop phase error between
// overlapping grains.
//
// Grain renewal searches for a prior rising crossing only once per hop.  It
// keeps the desired fractional phase *after* that crossing instead of snapping
// the entire read pointer to it.  A safety ceiling guarantees a grain cannot
// read unwritten input before it expires.  If a changing ratio makes an older
// grain unsafe, it is moved backwards by whole estimated periods; this retains
// phase for periodic material and is allocation-free.
class PhaseLockedPsolaGrainVoice
{
public:
    void prepare(double sampleRateIn)
    {
        sampleRate = juce::jmax(1.0, sampleRateIn);

        // Four 75%-overlapped Hann frames are COLA after normalisation.  A
        // 12 ms frame is short enough for vocal timing and still contains
        // several periods for normal singing registers.
        const int requestedSize = juce::jmax(kNumGrains * kMinimumHopSamples,
            (int) std::lround(sampleRate * kGrainSeconds));
        grainHop = juce::jmax(kMinimumHopSamples, requestedSize / kNumGrains);
        grainSize = grainHop * kNumGrains;

        // This is the source look-back needed by a new grain at the largest
        // supported ratio.  At 48 kHz it is 1,737 samples (36.2 ms).  It does
        // not grow with the requested interval, so host PDC remains stable.
        fixedLatencySamples = kInterpolationSafetySamples
            + (int) std::ceil((double) (grainSize - 1)
                * (double) (kMaximumPitchRatio - 1.0f));

        maxPitchMarkSearch = juce::jmax(32,
            (int) std::lround(sampleRate * kMaximumPitchMarkSearchSeconds));
        estimatedPeriodSamples = (float) sampleRate / kFallbackPitchHz;

        hannTable.resize((size_t) grainSize + 1);
        for (int i = 0; i <= grainSize; ++i)
        {
            const float phase = (float) i / (float) grainSize;
            hannTable[(size_t) i] = 0.5f * (1.0f
                - std::cos(juce::MathConstants<float>::twoPi * phase));
        }

        activationStep = 1.0f / (float) juce::jmax(16,
            (int) std::lround(sampleRate * kActivationSeconds));
        reset();
    }

    // The nominal source delay is independent of pitch ratio.  Pitch-mark
    // projection can select an additional older period, which is intentional
    // PSOLA period reuse rather than a changing processor delay.
    int getLatencySamples() const { return fixedLatencySamples; }

    void reset()
    {
        for (int i = 0; i < kNumGrains; ++i)
        {
            age[i] = grainHop * i;
            pos[i] = 0.0;
        }

        initialised = false;
        hasLastWriteHead = false;
        synthesisPhaseCycles = 0.0f;
        activationGain = 0.0f;
        unityBlend = 1.0f;
    }

    // Explicit priming is useful after a bypass or transport jump.  It is
    // also invoked automatically when process() detects non-sequential input.
    void prime(const VoiceBuffer& vb, float pitchRatio, float sourceFrequency)
    {
        const float safeRatio = sanitiseRatio(pitchRatio);
        const float period = updateEstimatedPeriod(sourceFrequency, true);
        const long long writeHead = vb.getWriteHead();

        synthesisPhaseCycles = 0.0f;
        for (int i = 0; i < kNumGrains; ++i)
        {
            age[i] = grainHop * i;
            const double ceiling = grainCeiling(writeHead, age[i], safeRatio);
            const double marker = findPriorRisingMarker(vb, ceiling, period);
            // All primed grains begin at phase zero (a rising crossing).  The
            // projection keeps each one in its own safe remaining-read range.
            pos[i] = markerIsValid(vb, marker)
                ? marker
                : projectAtOrBefore(ceiling, ceiling, period);
        }

        initialised = true;
        hasLastWriteHead = true;
        lastWriteHead = writeHead;
        activationGain = 0.0f;
    }

    float process(const VoiceBuffer& vb, float pitchRatio, float sourceFrequency)
    {
        const float safeRatio = sanitiseRatio(pitchRatio);
        const long long writeHead = vb.getWriteHead();
        float period = updateEstimatedPeriod(sourceFrequency, false);

        bool positionsAreValid = initialised && std::isfinite(synthesisPhaseCycles);
        for (const auto p : pos)
            positionsAreValid = positionsAreValid && std::isfinite(p);

        const bool sequentialInput = hasLastWriteHead && writeHead == lastWriteHead + 1;
        const bool needsPrime = !positionsAreValid || !sequentialInput;
        if (needsPrime)
        {
            prime(vb, safeRatio, sourceFrequency);
            period = estimatedPeriodSamples;
        }

        // A pitch-ratio change is applied to every active reader, rather than
        // being frozen into a grain.  Thus all active grains retain the same
        // synthesis phase under a glide or vibrato.  Rebase only when needed
        // to preserve the no-future-read invariant.
        if (!needsPrime)
            for (int i = 0; i < kNumGrains; ++i)
                if (age[i] != 0)
                    rebaseIfUnsafe(i, writeHead, safeRatio, period);

        float sum = 0.0f;
        float windowSum = 0.0f;
        for (int i = 0; i < kNumGrains; ++i)
        {
            if (age[i] == 0 && !needsPrime)
                startGrain(i, vb, writeHead, safeRatio, period);

            const float sample = vb.readBandLimited(pos[i], safeRatio);
            const float envelope = window(age[i]);
            sum += sample * envelope;
            windowSum += envelope;

            pos[i] += (double) safeRatio;
            if (++age[i] >= grainSize)
                age[i] = 0;
        }

        // phaseCycles is a normalised source-period phase.  Keeping it in
        // cycles avoids a discontinuity when the detector updates T.
        synthesisPhaseCycles += safeRatio / period;
        synthesisPhaseCycles -= std::floor(synthesisPhaseCycles);

        lastWriteHead = writeHead;
        hasLastWriteHead = true;

        float granular = sum / juce::jmax(0.18f, windowSum);
        if (!std::isfinite(granular))
        {
            reset();
            return 0.0f;
        }

        granular *= activationGain;
        activationGain = juce::jmin(1.0f, activationGain + activationStep);

        // Preserve an exact, fixed-delay unison path.  The phase-locked OLA
        // engine still runs behind it, so leaving unison does not revive stale
        // circular-buffer data.  The short blend makes a slow glide through
        // unity click-free.
        const float unityTarget = std::abs(safeRatio - 1.0f) < kUnityTolerance
            ? 1.0f : 0.0f;
        unityBlend += (unityTarget - unityBlend) * unityBlendStep;
        const float direct = vb.readInterpolated((double) writeHead
            - (double) fixedLatencySamples - 1.0);
        const float output = granular + (direct - granular) * unityBlend;
        return std::isfinite(output) ? output : 0.0f;
    }

private:
    static constexpr int kNumGrains = 4;
    static constexpr int kMinimumHopSamples = 24;
    static constexpr float kGrainSeconds = 0.012f;
    static constexpr float kMinimumPitchRatio = 0.25f;
    static constexpr float kMaximumPitchRatio = 4.0f;
    static constexpr float kMinimumTrackedPitchHz = 55.0f;
    static constexpr float kMaximumTrackedPitchHz = 1500.0f;
    static constexpr float kFallbackPitchHz = 180.0f;
    static constexpr float kMaximumPitchMarkSearchSeconds = 0.030f;
    static constexpr int kInterpolationSafetySamples = 12;
    static constexpr float kActivationSeconds = 0.0075f;
    static constexpr float kUnityTolerance = 0.002f;
    static constexpr float unityBlendStep = 0.004f;

    float sanitiseRatio(float ratio) const
    {
        if (!std::isfinite(ratio))
            return 1.0f;
        return juce::jlimit(kMinimumPitchRatio, kMaximumPitchRatio, ratio);
    }

    bool hasUsablePitch(float sourceFrequency) const
    {
        return std::isfinite(sourceFrequency)
            && sourceFrequency >= kMinimumTrackedPitchHz
            && sourceFrequency <= kMaximumTrackedPitchHz;
    }

    float updateEstimatedPeriod(float sourceFrequency, bool force)
    {
        if (!hasUsablePitch(sourceFrequency))
            return estimatedPeriodSamples;

        const float target = juce::jlimit((float) sampleRate / kMaximumTrackedPitchHz,
            (float) sampleRate / kMinimumTrackedPitchHz,
            (float) sampleRate / sourceFrequency);
        if (force || !std::isfinite(estimatedPeriodSamples))
        {
            estimatedPeriodSamples = target;
        }
        else
        {
            // Limit only detector jumps, not a stationary test tone.  A
            // constant f0 therefore retains an exact T for phase locking.
            const float maximumStep = juce::jmax(0.25f, estimatedPeriodSamples * 0.02f);
            estimatedPeriodSamples += juce::jlimit(-maximumStep, maximumStep,
                target - estimatedPeriodSamples);
        }
        return estimatedPeriodSamples;
    }

    double safeReadCeiling(long long writeHead, int grainAge, float ratio) const
    {
        const int futureSamples = juce::jmax(0, grainSize - 1 - grainAge);
        return (double) writeHead - 1.0 - (double) kInterpolationSafetySamples
            - ((double) ratio - 1.0) * (double) futureSamples;
    }

    double nominalCeiling(long long writeHead, int grainAge, float ratio) const
    {
        return (double) writeHead - 1.0 - (double) fixedLatencySamples
            + ((double) ratio - 1.0) * (double) grainAge;
    }

    double grainCeiling(long long writeHead, int grainAge, float ratio) const
    {
        return std::min(safeReadCeiling(writeHead, grainAge, ratio),
            nominalCeiling(writeHead, grainAge, ratio));
    }

    // Return the latest member of { phaseReference + k * period } that is no
    // later than ceiling.  It is the central operation that preserves source
    // phase while keeping the reader inside the finite input history.
    static double projectAtOrBefore(double phaseReference, double ceiling, float period)
    {
        const double safePeriod = juce::jmax(1.0f, period);
        const double periodsToSubtract = std::ceil((phaseReference - ceiling) / safePeriod);
        return phaseReference - periodsToSubtract * safePeriod;
    }

    double findPriorRisingMarker(const VoiceBuffer& vb, double atOrBefore,
                                  float period) const
    {
        const int desiredRadius = (int) std::ceil((double) period * 1.25) + 4;
        const int radius = juce::jlimit(16, maxPitchMarkSearch, desiredRadius);
        return vb.findNearestRisingZeroCrossingAtOrBefore(atOrBefore, radius);
    }

    static bool markerIsValid(const VoiceBuffer& vb, double marker)
    {
        if (!std::isfinite(marker))
            return false;
        // VoiceBuffer returns its requested position when there was no
        // crossing in the bounded search.  Validate the result so silence and
        // unvoiced material use the deterministic phase-projection fallback.
        const float before = vb.readInterpolated(marker - 0.25);
        const float after = vb.readInterpolated(marker + 0.25);
        return std::isfinite(before) && std::isfinite(after)
            && before <= 0.0f && after > 0.0f;
    }

    void rebaseIfUnsafe(int grain, long long writeHead, float ratio, float period)
    {
        const double ceiling = safeReadCeiling(writeHead, age[grain], ratio);
        if (pos[grain] > ceiling)
            pos[grain] = projectAtOrBefore(pos[grain], ceiling, period);
    }

    double phaseReferenceFor(int exceptGrain, double fallback) const
    {
        for (int i = 0; i < kNumGrains; ++i)
            if (i != exceptGrain && std::isfinite(pos[i]))
                return pos[i];
        return fallback;
    }

    void startGrain(int grain, const VoiceBuffer& vb, long long writeHead,
                    float ratio, float period)
    {
        const double ceiling = grainCeiling(writeHead, 0, ratio);
        const float phaseOffset = synthesisPhaseCycles * period;
        const double marker = findPriorRisingMarker(vb, ceiling - phaseOffset, period);

        if (markerIsValid(vb, marker))
        {
            // Keep phaseOffset after the mark.  Snapping the whole pointer to
            // marker is the fractional-ratio failure mode this class avoids.
            pos[grain] = marker + (double) phaseOffset;
        }
        else
        {
            const double reference = phaseReferenceFor(grain, ceiling);
            pos[grain] = projectAtOrBefore(reference, ceiling, period);
        }
    }

    float window(int grainAge) const
    {
        const int i = juce::jlimit(0, grainSize, grainAge);
        return hannTable[(size_t) i];
    }

    double sampleRate = 44100.0;
    int grainSize = 528;
    int grainHop = 132;
    int fixedLatencySamples = 1593;
    int maxPitchMarkSearch = 1323;
    int age[kNumGrains] {};
    double pos[kNumGrains] {};
    std::vector<float> hannTable;
    float estimatedPeriodSamples = 245.0f;
    float synthesisPhaseCycles = 0.0f;
    bool initialised = false;
    bool hasLastWriteHead = false;
    long long lastWriteHead = 0;
    float activationGain = 0.0f;
    float activationStep = 0.003f;
    float unityBlend = 1.0f;
};
