#pragma once

#include <algorithm>
#include <vector>
#include <cmath>
#include <juce_core/juce_core.h>

// Shared circular mono buffer for the lead and all harmony generators.
class VoiceBuffer
{
public:
    void prepare(double sampleRate, float seconds = 1.0f)
    {
        size = juce::jmax(1024, (int) (sampleRate * (double) seconds));
        buf.assign((size_t) size, 0.0f);
        buildResampleKernels();
        reset();
    }

    // Transport jumps and a host-side reset must never leave old circular
    // history available to a freshly-primed grain reader. This intentionally
    // reuses the existing allocation: call it on the host reset/transport path
    // before rendering resumes; it does not change capacity or the kernel table.
    void reset()
    {
        std::fill(buf.begin(), buf.end(), 0.0f);
        writeHead = 0;
    }

    void write(float x)
    {
        // Never let one invalid host sample contaminate every later grain.
        buf[(size_t) wrapIndex(writeHead)] = std::isfinite(x) ? x : 0.0f;
        ++writeHead;
    }

    float readInterpolated(double absPos) const
    {
        if (!std::isfinite(absPos) || size <= 0)
            return 0.0f;

        const long long i0 = (long long) std::floor(absPos);
        const float frac = (float) (absPos - (double) i0);

        // Four-point Hermite interpolation is cleaner than linear
        // interpolation when a voice is read at a different speed.
        const float y0 = buf[(size_t) wrapIndex(i0 - 1)];
        const float y1 = buf[(size_t) wrapIndex(i0)];
        const float y2 = buf[(size_t) wrapIndex(i0 + 1)];
        const float y3 = buf[(size_t) wrapIndex(i0 + 2)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        const float output = ((c3 * frac + c2) * frac + c1) * frac + y1;
        return std::isfinite(output) ? output : 0.0f;
    }

    // A windowed-sinc reader is used for substantial upward shifts.  Hermite
    // interpolation is excellent for small ratios, but cannot remove source
    // frequencies that would fold back above Nyquist when a voice reads more
    // quickly than real time.  The kernels are made in prepare(), so this is
    // allocation-free and deterministic on the audio thread.
    float readBandLimited(double absPos, float sourceIncrement) const
    {
        if (!std::isfinite(absPos) || !std::isfinite(sourceIncrement) || size <= 0)
            return 0.0f;

        if (sourceIncrement <= 1.075f || resampleKernels.empty())
            return readInterpolated(absPos);

        const float cutoff = juce::jlimit(kMinimumResampleCutoff, 1.0f,
            1.0f / juce::jmax(1.0f, sourceIncrement));
        const float bandPosition = (cutoff - kMinimumResampleCutoff)
            / (1.0f - kMinimumResampleCutoff) * (float) (kResampleCutoffBands - 1);
        const float clampedBandPosition = juce::jlimit(0.0f,
            (float) (kResampleCutoffBands - 1), bandPosition);
        const int band0 = juce::jlimit(0, kResampleCutoffBands - 1,
            (int) std::floor(clampedBandPosition));
        const int band1 = juce::jmin(kResampleCutoffBands - 1, band0 + 1);
        const float bandFraction = clampedBandPosition - (float) band0;

        const long long i0 = (long long) std::floor(absPos);
        const float fraction = juce::jlimit(0.0f, 1.0f,
            (float) (absPos - (double) i0));
        const float phasePosition = fraction * (float) (kResamplePhases - 1);
        const int phase0 = juce::jlimit(0, kResamplePhases - 1,
            (int) std::floor(phasePosition));
        const int phase1 = juce::jmin(kResamplePhases - 1, phase0 + 1);
        const float phaseFraction = phasePosition - (float) phase0;

        // Do not quantise cutoff or fractional phase while a glide/vibrato is
        // moving.  Every table kernel has unity DC gain, so bilinear blending
        // retains that gain while removing the old small band/phase steps.
        const size_t offset00 = kernelOffsetFor(band0, phase0);
        const size_t offset01 = kernelOffsetFor(band0, phase1);
        const size_t offset10 = kernelOffsetFor(band1, phase0);
        const size_t offset11 = kernelOffsetFor(band1, phase1);
        float output = 0.0f;
        for (int tap = 0; tap < kResampleTaps; ++tap)
        {
            const int sourceOffset = tap - (kResampleTaps / 2 - 1);
            const size_t kernelTap = (size_t) tap;
            const float phase0Coefficient = resampleKernels[offset00 + kernelTap]
                + phaseFraction * (resampleKernels[offset01 + kernelTap]
                    - resampleKernels[offset00 + kernelTap]);
            const float phase1Coefficient = resampleKernels[offset10 + kernelTap]
                + phaseFraction * (resampleKernels[offset11 + kernelTap]
                    - resampleKernels[offset10 + kernelTap]);
            const float coefficient = phase0Coefficient + bandFraction
                * (phase1Coefficient - phase0Coefficient);
            output += buf[(size_t) wrapIndex(i0 + (long long) sourceOffset)] * coefficient;
        }
        return std::isfinite(output) ? output : 0.0f;
    }

    // Pitch marks use a tiny five-point binomial low-pass before finding the
    // crossing.  It makes the marker follow the vocal fundamental rather than
    // an arbitrary high-frequency/formant zero crossing, without adding state,
    // latency or an allocation on the audio thread.
    double findRisingPitchMarkAtOrBefore(double absPos, int searchRadius) const
    {
        if (!std::isfinite(absPos) || size <= 0)
            return (double) writeHead;

        const long long centre = (long long) std::floor(absPos);
        const long long start = centre - juce::jmax(1, searchRadius);
        double bestPosition = absPos;
        double bestDistance = (double) searchRadius + 1.0;
        // Carry the five-sample binomial window while scanning. This keeps the
        // robust marker to one new circular-buffer read per candidate instead
        // of re-reading the same five samples at every position.
        float x0 = sourceSample(start - 3);
        float x1 = sourceSample(start - 2);
        float x2 = sourceSample(start - 1);
        float x3 = sourceSample(start);
        float x4 = sourceSample(start + 1);
        float previous = binomialPitchMark(x0, x1, x2, x3, x4);

        for (long long i = start; i <= centre; ++i)
        {
            const float next = sourceSample(i + 2);
            const float current = binomialPitchMark(x1, x2, x3, x4, next);
            const float slope = current - previous;
            if (std::isfinite(previous) && std::isfinite(current)
                && previous <= 0.0f && current > 0.0f && slope > 1.0e-9f)
            {
                const double crossing = (double) i - (double) previous / (double) slope;
                const double distance = absPos - crossing;
                if (distance >= 0.0 && distance < bestDistance)
                {
                    bestDistance = distance;
                    bestPosition = crossing;
                }
            }
            previous = current;
            x1 = x2;
            x2 = x3;
            x3 = x4;
            x4 = next;
        }
        return bestPosition;
    }

    bool isValidRisingPitchMark(double marker) const
    {
        if (!std::isfinite(marker) || size <= 0)
            return false;

        const long long upperIndex = (long long) std::ceil(marker);
        const float before = pitchMarkSample(upperIndex - 1);
        const float after = pitchMarkSample(upperIndex);
        return std::isfinite(before) && std::isfinite(after)
            && before <= 0.0f && after > 0.0f && after - before > 1.0e-9f;
    }

    // Find a nearby rising zero crossing to use as a lightweight pitch mark.
    // The search is only performed when a grain is renewed, never per sample.
    double findNearestRisingZeroCrossing(double absPos, int searchRadius) const
    {
        if (!std::isfinite(absPos) || size <= 0)
            return (double) writeHead;

        const long long centre = (long long) std::llround(absPos);
        const long long start = centre - juce::jmax(1, searchRadius);
        const long long end = centre + juce::jmax(1, searchRadius);
        double bestPosition = absPos;
        double bestDistance = (double) searchRadius + 1.0;

        for (long long i = start; i <= end; ++i)
        {
            const float previous = buf[(size_t) wrapIndex(i - 1)];
            const float current = buf[(size_t) wrapIndex(i)];
            if (std::isfinite(previous) && std::isfinite(current)
                && previous <= 0.0f && current > 0.0f)
            {
                const double crossing = (double) i - (double) previous / (double) (current - previous);
                const double distance = std::abs(crossing - absPos);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestPosition = crossing;
                }
            }
        }
        return bestPosition;
    }

    // When a grain is started, a future crossing is unsafe: a fast upward
    // read can otherwise reach unwritten samples before its window ends.  The
    // previous crossing gives the same phase-reference benefit without ever
    // compromising the fixed-latency safety margin.
    double findNearestRisingZeroCrossingAtOrBefore(double absPos, int searchRadius) const
    {
        if (!std::isfinite(absPos) || size <= 0)
            return (double) writeHead;

        const long long centre = (long long) std::floor(absPos);
        const long long start = centre - juce::jmax(1, searchRadius);
        double bestPosition = absPos;
        double bestDistance = (double) searchRadius + 1.0;

        for (long long i = start; i <= centre; ++i)
        {
            const float previous = buf[(size_t) wrapIndex(i - 1)];
            const float current = buf[(size_t) wrapIndex(i)];
            if (std::isfinite(previous) && std::isfinite(current)
                && previous <= 0.0f && current > 0.0f)
            {
                const double crossing = (double) i - (double) previous / (double) (current - previous);
                if (crossing <= absPos)
                {
                    const double distance = absPos - crossing;
                    if (distance < bestDistance)
                    {
                        bestDistance = distance;
                        bestPosition = crossing;
                    }
                }
            }
        }
        return bestPosition;
    }

    long long getWriteHead() const { return writeHead; }
    int getSize() const { return size; }

private:
    static constexpr int kResampleTaps = 12;
    static constexpr int kResamplePhases = 128;
    static constexpr int kResampleCutoffBands = 24;
    static constexpr float kMinimumResampleCutoff = 0.25f;

    static float sinc(float x)
    {
        const float piX = juce::MathConstants<float>::pi * x;
        return std::abs(piX) < 1.0e-5f ? 1.0f : std::sin(piX) / piX;
    }

    size_t kernelOffsetFor(int band, int phase) const
    {
        return ((size_t) band * (size_t) kResamplePhases + (size_t) phase)
            * (size_t) kResampleTaps;
    }

    float pitchMarkSample(long long absoluteIndex) const
    {
        // [1 4 6 4 1] / 16: enough high-frequency rejection to make a robust
        // marker, but its phase is consistent for all searches and it leaves
        // every supported vocal fundamental well inside the passband.
        return binomialPitchMark(sourceSample(absoluteIndex - 2),
                                 sourceSample(absoluteIndex - 1),
                                 sourceSample(absoluteIndex),
                                 sourceSample(absoluteIndex + 1),
                                 sourceSample(absoluteIndex + 2));
    }

    static float binomialPitchMark(float x0, float x1, float x2, float x3, float x4)
    {
        const float output = (x0 + 4.0f * x1 + 6.0f * x2 + 4.0f * x3 + x4) * 0.0625f;
        return std::isfinite(output) ? output : 0.0f;
    }

    float sourceSample(long long absoluteIndex) const
    {
        const float sample = buf[(size_t) wrapIndex(absoluteIndex)];
        return std::isfinite(sample) ? sample : 0.0f;
    }

    void buildResampleKernels()
    {
        resampleKernels.assign((size_t) kResampleCutoffBands
            * (size_t) kResamplePhases * (size_t) kResampleTaps, 0.0f);

        for (int band = 0; band < kResampleCutoffBands; ++band)
        {
            const float cutoff = kMinimumResampleCutoff
                + (1.0f - kMinimumResampleCutoff) * (float) band
                    / (float) (kResampleCutoffBands - 1);
            for (int phase = 0; phase < kResamplePhases; ++phase)
            {
                const float fractional = (float) phase / (float) (kResamplePhases - 1);
                const size_t offset = ((size_t) band * (size_t) kResamplePhases
                    + (size_t) phase) * (size_t) kResampleTaps;
                float normaliser = 0.0f;

                for (int tap = 0; tap < kResampleTaps; ++tap)
                {
                    const int sampleOffset = tap - (kResampleTaps / 2 - 1);
                    const float x = (float) sampleOffset - fractional;
                    const float windowPhase = (float) tap / (float) (kResampleTaps - 1);
                    const float blackman = 0.42f
                        - 0.5f * std::cos(juce::MathConstants<float>::twoPi * windowPhase)
                        + 0.08f * std::cos(2.0f * juce::MathConstants<float>::twoPi * windowPhase);
                    const float coefficient = cutoff * sinc(cutoff * x) * blackman;
                    resampleKernels[offset + (size_t) tap] = coefficient;
                    normaliser += coefficient;
                }

                if (std::abs(normaliser) > 1.0e-8f)
                    for (int tap = 0; tap < kResampleTaps; ++tap)
                        resampleKernels[offset + (size_t) tap] /= normaliser;
                else
                    resampleKernels[offset + (size_t) (kResampleTaps / 2 - 1)] = 1.0f;
            }
        }
    }

    int wrapIndex(long long i) const
    {
        const long long m = i % (long long) size;
        return (int) (m < 0 ? m + size : m);
    }

    std::vector<float> buf;
    std::vector<float> resampleKernels;
    long long writeHead = 0;
    int size = 0;
};
