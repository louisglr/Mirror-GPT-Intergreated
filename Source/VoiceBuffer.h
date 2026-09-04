#pragma once

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
        const int band = juce::jlimit(0, kResampleCutoffBands - 1,
            (int) std::lround(bandPosition));

        const long long i0 = (long long) std::floor(absPos);
        const float fraction = juce::jlimit(0.0f, 1.0f,
            (float) (absPos - (double) i0));
        const int phase = juce::jlimit(0, kResamplePhases - 1,
            (int) std::lround(fraction * (float) (kResamplePhases - 1)));

        const size_t kernelOffset = ((size_t) band * (size_t) kResamplePhases
            + (size_t) phase) * (size_t) kResampleTaps;
        float output = 0.0f;
        for (int tap = 0; tap < kResampleTaps; ++tap)
        {
            const int sourceOffset = tap - (kResampleTaps / 2 - 1);
            output += buf[(size_t) wrapIndex(i0 + (long long) sourceOffset)]
                * resampleKernels[kernelOffset + (size_t) tap];
        }
        return std::isfinite(output) ? output : 0.0f;
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
