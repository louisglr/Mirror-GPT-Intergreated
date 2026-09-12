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
// Grain renewal searches for a prior, low-pass-marked rising crossing only once
// per hop. It keeps the desired fractional phase *after* that crossing instead
// of snapping the entire read pointer to it. A safety ceiling guarantees a
// grain cannot read unwritten input before it expires. If a changing ratio
// makes an older grain unsafe, it is moved backwards by whole estimated
// periods; this retains phase for periodic material and is allocation-free.
class PhaseLockedPsolaGrainVoice
{
public:
    void prepare(double sampleRateIn, float renewalStaggerFraction = 0.0f)
    {
        prepared = false;
        sampleRate = std::isfinite(sampleRateIn) && sampleRateIn >= 1000.0
            ? sampleRateIn : 44100.0;

        // Four 75%-overlapped Hann frames are COLA after normalisation.  A
        // 12 ms frame is short enough for vocal timing and still contains
        // several periods for normal singing registers.
        const int requestedSize = juce::jmax(kNumGrains * kMinimumHopSamples,
            (int) std::lround(sampleRate * kGrainSeconds));
        grainHop = juce::jmax(kMinimumHopSamples, requestedSize / kNumGrains);
        grainSize = grainHop * kNumGrains;
        const float safeStagger = std::isfinite(renewalStaggerFraction)
            ? juce::jlimit(0.0f, 0.999f, renewalStaggerFraction) : 0.0f;
        renewalStaggerSamples = juce::jlimit(0, grainHop - 1,
            (int) std::lround((float) grainHop * safeStagger));

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
        unityBlendCoefficient = 1.0f - std::exp(-1.0f / (float) (sampleRate
            * kUnisonBlendSeconds));
        reset();
        prepared = true;
    }

    // The nominal source delay is independent of pitch ratio.  Pitch-mark
    // projection can select an additional older period, which is intentional
    // PSOLA period reuse rather than a changing processor delay.
    int getLatencySamples() const { return fixedLatencySamples; }

    void reset()
    {
        for (int i = 0; i < kNumGrains; ++i)
        {
            age[i] = initialAgeForGrain(i);
            pos[i] = 0.0;
        }

        initialised = false;
        hasLastWriteHead = false;
        unityFastPathActive = false;
        estimatedPeriodSamples = (float) sampleRate / kFallbackPitchHz;
        synthesisPhaseSamples = 0.0;
        activationGain = 0.0f;
        unityBlend = 1.0f;
    }

    // Explicit priming is useful after a bypass or transport jump.  It is
    // also invoked automatically when process() detects non-sequential input.
    void prime(const VoiceBuffer& vb, float pitchRatio, float sourceFrequency)
    {
        if (!prepared)
            return;

        const float safeRatio = sanitiseRatio(pitchRatio);
        const float period = updateEstimatedPeriod(sourceFrequency, true);
        const long long writeHead = vb.getWriteHead();

        // Keep synthesis phase in source *samples*, not normalised cycles.
        // If the detector revises T, a sample-space phase still agrees with
        // every active reader; only its modulo operation uses the new period.
        synthesisPhaseSamples = 0.0;
        for (int i = 0; i < kNumGrains; ++i)
        {
            age[i] = initialAgeForGrain(i);
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
        unityFastPathActive = false;
        lastWriteHead = writeHead;
        activationGain = 0.0f;
    }

    float process(const VoiceBuffer& vb, float pitchRatio, float sourceFrequency)
    {
        if (!prepared)
            return 0.0f;

        const float safeRatio = sanitiseRatio(pitchRatio);
        const long long writeHead = vb.getWriteHead();
        const float direct = vb.readInterpolated((double) writeHead
            - (double) fixedLatencySamples - 1.0);

        // Once the transparent path has reached an exact coefficient of one,
        // an exact-unity voice needs no grain reads at all. Do not enter this
        // path during the crossfade. Leaving it explicitly primes a fresh,
        // phase-consistent grain set so skipped grain state can never reappear.
        const bool exactUnity = safeRatio == 1.0f;
        if (exactUnity && unityBlend >= 1.0f)
        {
            unityBlend = 1.0f;
            unityFastPathActive = true;
            lastWriteHead = writeHead;
            hasLastWriteHead = true;
            return std::isfinite(direct) ? direct : 0.0f;
        }

        float period = updateEstimatedPeriod(sourceFrequency, false);

        bool positionsAreValid = initialised && std::isfinite(synthesisPhaseSamples);
        for (const auto p : pos)
            positionsAreValid = positionsAreValid && std::isfinite(p);

        const bool sequentialInput = hasLastWriteHead && writeHead == lastWriteHead + 1;
        const bool needsPrime = unityFastPathActive || !positionsAreValid || !sequentialInput;
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

        // This is deliberately an unwrapped sample position. The previous
        // normalised-cycle accumulator multiplied a new period by an old
        // normalised phase whenever the detector updated T, causing a renewed
        // grain to start a few source samples away from its active neighbours.
        // A double keeps sub-sample precision for far longer than a session.
        synthesisPhaseSamples += (double) safeRatio;

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

        // Exact unity is a transparent fixed-delay reader. The old ±0.002
        // ratio tolerance silently swallowed roughly ±3.5 cents of requested
        // pitch movement. A sub-0.04-cent smooth bridge now removes clicks at
        // the exact crossing without becoming a musical dead zone.
        const float unisonDistance = std::abs(safeRatio - 1.0f);
        const float granularTarget = smoothstep(0.0f, kUnisonBridgeRatio, unisonDistance);
        const float unityTarget = 1.0f - granularTarget;
        unityBlend += (unityTarget - unityBlend) * unityBlendCoefficient;
        if (unityTarget >= 1.0f && unityBlend > 0.99999f)
            unityBlend = 1.0f;
        else if (unityTarget <= 0.0f && unityBlend < 0.00001f)
            unityBlend = 0.0f;
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
    static constexpr float kMaximumMarkerPhaseResidual = 0.18f;
    static constexpr float kUnisonBridgeRatio = 2.0e-5f;
    static constexpr float kUnisonBlendSeconds = 0.0025f;

    static float smoothstep(float edge0, float edge1, float x)
    {
        const float range = juce::jmax(1.0e-9f, edge1 - edge0);
        const float t = juce::jlimit(0.0f, 1.0f, (x - edge0) / range);
        return t * t * (3.0f - 2.0f * t);
    }

    int initialAgeForGrain(int grain) const
    {
        return (grainHop * grain + renewalStaggerSamples) % grainSize;
    }

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

    static double positiveModulo(double value, float period)
    {
        const double safePeriod = juce::jmax(1.0f, period);
        if (!std::isfinite(value))
            return 0.0;

        double phase = std::fmod(value, safePeriod);
        if (phase < 0.0)
            phase += safePeriod;
        return phase;
    }

    double findPriorRisingMarker(const VoiceBuffer& vb, double atOrBefore,
                                  float period) const
    {
        const int desiredRadius = (int) std::ceil((double) period * 1.25) + 4;
        const int radius = juce::jlimit(16, maxPitchMarkSearch, desiredRadius);
        return vb.findRisingPitchMarkAtOrBefore(atOrBefore, radius);
    }

    static bool markerIsValid(const VoiceBuffer& vb, double marker)
    {
        if (!std::isfinite(marker))
            return false;
        // VoiceBuffer returns its requested position when there was no
        // crossing in the bounded search. Validate the low-pass pitch mark so
        // silence and unvoiced material use the deterministic fallback.
        return vb.isValidRisingPitchMark(marker);
    }

    void rebaseIfUnsafe(int grain, long long writeHead, float ratio, float period)
    {
        const double ceiling = safeReadCeiling(writeHead, age[grain], ratio);
        if (pos[grain] > ceiling)
            pos[grain] = projectAtOrBefore(pos[grain], ceiling, period);
    }

    double phaseReferenceFor(int exceptGrain, double fallback) const
    {
        float strongestWindow = -1.0f;
        double reference = fallback;
        for (int i = 0; i < kNumGrains; ++i)
            if (i != exceptGrain && std::isfinite(pos[i]))
            {
                // A reader at the middle of its Hann window is the most
                // reliable phase leader; a nearly expired/reborn reader has
                // little contribution and should not choose a new grain's
                // anchor.
                const float weight = window(age[i]);
                if (weight > strongestWindow)
                {
                    strongestWindow = weight;
                    reference = pos[i];
                }
            }
        return reference;
    }

    void startGrain(int grain, const VoiceBuffer& vb, long long writeHead,
                    float ratio, float period)
    {
        const double ceiling = grainCeiling(writeHead, 0, ratio);
        const double reference = phaseReferenceFor(grain, ceiling);
        const double phaseContinuousPosition = projectAtOrBefore(reference, ceiling, period);
        const double phaseOffset = positiveModulo(synthesisPhaseSamples, period);
        const double marker = findPriorRisingMarker(vb, ceiling - phaseOffset, period);

        if (markerIsValid(vb, marker))
        {
            // Keep phaseOffset after the mark. Snapping the whole pointer to
            // marker is the fractional-ratio failure mode this class avoids.
            const double markerPosition = marker + phaseOffset;
            const double phaseResidual = std::abs(std::remainder(
                markerPosition - phaseContinuousPosition, (double) juce::jmax(1.0f, period)));

            // The low-pass marker may still be ambiguous on a breathy or
            // multi-formant syllable. Reject a marker that would move the new
            // grain more than 18% of a cycle away from the active phase leader;
            // the projected reference is then click-free and deterministic.
            pos[grain] = phaseResidual <= (double) period * kMaximumMarkerPhaseResidual
                ? markerPosition : phaseContinuousPosition;
        }
        else
        {
            pos[grain] = phaseContinuousPosition;
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
    // Different processor voices use different sub-hop offsets. Their bounded
    // pitch-mark searches therefore no longer land on the same audio sample,
    // avoiding periodic CPU spikes without altering any voice's COLA spacing.
    int renewalStaggerSamples = 0;
    int age[kNumGrains] {};
    double pos[kNumGrains] {};
    std::vector<float> hannTable;
    float estimatedPeriodSamples = 245.0f;
    double synthesisPhaseSamples = 0.0;
    bool initialised = false;
    bool hasLastWriteHead = false;
    bool unityFastPathActive = false;
    long long lastWriteHead = 0;
    float activationGain = 0.0f;
    float activationStep = 0.003f;
    float unityBlend = 1.0f;
    float unityBlendCoefficient = 0.004f;
    bool prepared = false;
};
