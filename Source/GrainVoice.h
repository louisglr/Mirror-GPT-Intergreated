#pragma once

#include <cmath>
#include <vector>
#include <juce_core/juce_core.h>
#include "VoiceBuffer.h"

// Pitch-mark-aligned granular pitch-shifter with four overlapping grains.
// Grains begin at nearby rising zero crossings, while their shared fixed
// length keeps the overlap pattern phase-stable over long sessions.
// Læser fra en delt VoiceBuffer, så flere GrainVoice-instanser (lead +
// harmonier) kan afspille samme kilde ved forskellige pitch-forhold
// samtidig, uden at duplikere lyddata.
class GrainVoice
{
public:
    void prepare(double sampleRate)
    {
        grainSize = juce::jmax(64, (int) (sampleRate * 0.014)); // fallback: 14 ms
        maxGrainSize = juce::jmax(grainSize, (int) (sampleRate * 0.018));
        hannTable.resize((size_t) maxGrainSize + 1);
        for (int i = 0; i <= maxGrainSize; ++i)
        {
            const float phase = (float) i / (float) maxGrainSize;
            hannTable[(size_t) i] = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * phase));
        }
        for (int i = 0; i < kNumGrains; ++i)
        {
            age[i] = (grainSize * i) / kNumGrains;
        }
        for (auto& p : pos) p = 0.0;
        initialised = false;
    }

    float process(const VoiceBuffer& vb, float pitchRatio, float /* sourceFrequency */)
    {
        // Corrupt or extremely implausible detector output must never send a
        // grain reader many buffer lengths away from the live signal.
        if (!std::isfinite(pitchRatio))
            pitchRatio = 1.0f;
        pitchRatio = juce::jlimit(0.25f, 4.0f, pitchRatio);
        // Start every overlapping grain at a valid, phase-correct read
        // position.  Previously only the grain at age zero was initialised;
        // the remaining grains briefly read buffer position zero after a
        // transport start or sample-rate change.
        if (!initialised)
        {
            for (int i = 0; i < kNumGrains; ++i)
            {
                const double candidate = (double) vb.getWriteHead()
                    - (double) (grainSize - age[i]) * (double) pitchRatio - 3.0;
                pos[i] = vb.findNearestRisingZeroCrossing(candidate, grainSize / 5);
            }
            initialised = true;
        }

        float sum = 0.0f;
        for (int i = 0; i < kNumGrains; ++i)
        {
            if (age[i] == 0)
            {
                // KRITISK: lookback skal skaleres med pitchRatio. Ved opadgående
                // pitch-shift (ratio > 1) læser kornet hurtigere fremad end
                // real-time, så uden denne skalering løber læsepositionen forbi
                // det faktisk skrevne data og begynder at læse "fremtid"/gammelt
                // ombrudt data - det giver præcis den krakelering/artefakt der
                // bliver værre jo højere (mere ekstremt) intervallet er.
                // Leave a few interpolation samples behind the write head as
                // well.  Cubic interpolation needs samples on both sides of
                // the read position; without this margin a grain may touch
                // unwritten data at its end.
                double lookback = (double) grainSize * (double) pitchRatio + 3.0;
                const double candidate = (double) vb.getWriteHead() - lookback;
                pos[i] = vb.findNearestRisingZeroCrossing(candidate, grainSize / 5);
            }

            float s = vb.readInterpolated(pos[i]);
            pos[i] += pitchRatio;

            float env = window(age[i]);
            sum += s * env;

            age[i] = (age[i] + 1) % grainSize;
        }
        // Four evenly spaced Hann windows have a stable gain sum of 2.
        return sum * 0.5f;
    }

private:
    float window(int grainAge) const
    {
        // Avoid several expensive cosine calls per audio sample.  The table
        // is prepared off the audio thread and linearly interpolated here.
        const float position = (float) grainAge / (float) grainSize * (float) maxGrainSize;
        const int i0 = juce::jlimit(0, maxGrainSize - 1, (int) position);
        const float fraction = position - (float) i0;
        return hannTable[(size_t) i0] + fraction
            * (hannTable[(size_t) (i0 + 1)] - hannTable[(size_t) i0]);
    }

    static constexpr int kNumGrains = 4;
    int grainSize = 2205;
    int maxGrainSize = 2205;
    int age[kNumGrains] = { 0, 0, 0, 0 };
    double pos[kNumGrains] = { 0.0, 0.0, 0.0, 0.0 };
    std::vector<float> hannTable;
    bool initialised = false;
};
