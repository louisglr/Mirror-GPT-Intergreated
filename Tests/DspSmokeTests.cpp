#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "PitchDetector.h"
#include "PhaseLockedPsolaGrainVoice.h"
#include "WarmSaturator.h"

namespace
{
int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

double estimateFrequency(const std::vector<float>& signal, double sampleRate,
                         int firstSample)
{
    std::vector<double> risingCrossings;
    risingCrossings.reserve(signal.size() / 80);
    for (int i = juce::jmax(1, firstSample); i < (int) signal.size(); ++i)
    {
        const float before = signal[(size_t) (i - 1)];
        const float after = signal[(size_t) i];
        if (before <= 0.0f && after > 0.0f && after > before)
        {
            const double fraction = (double) -before / (double) (after - before);
            risingCrossings.push_back((double) (i - 1) + fraction);
        }
    }

    if (risingCrossings.size() < 8)
        return 0.0;
    return ((double) risingCrossings.size() - 1.0) * sampleRate
        / (risingCrossings.back() - risingCrossings.front());
}

double renderPitchShift(double sampleRate, float sourceHz, float ratio)
{
    VoiceBuffer buffer;
    PhaseLockedPsolaGrainVoice voice;
    buffer.prepare(sampleRate, 1.0f);
    voice.prepare(sampleRate);

    const int sampleCount = (int) std::lround(sampleRate * 1.5);
    std::vector<float> output((size_t) sampleCount, 0.0f);
    for (int i = 0; i < sampleCount; ++i)
    {
        const float input = 0.25f * std::sin(juce::MathConstants<float>::twoPi
            * sourceHz * (float) i / (float) sampleRate);
        buffer.write(input);
        output[(size_t) i] = voice.process(buffer, ratio, sourceHz);
        expect(std::isfinite(output[(size_t) i]), "pitch shifter emitted a non-finite sample");
    }

    return estimateFrequency(output, sampleRate, (int) std::lround(sampleRate * 0.55));
}

void testPitchDetector(double sampleRate)
{
    PitchDetector detector;
    detector.prepare(sampleRate);
    detector.setVocalRange(0);

    constexpr float sourceHz = 200.0f;
    const int sampleCount = (int) std::lround(sampleRate * 0.85);
    for (int i = 0; i < sampleCount; ++i)
    {
        float input = 0.35f * std::sin(juce::MathConstants<float>::twoPi
            * sourceHz * (float) i / (float) sampleRate);
        if (i == sampleCount / 3)
            input = std::numeric_limits<float>::quiet_NaN();
        detector.pushSample(input);
    }

    const float detected = detector.getFrequency();
    expect(std::isfinite(detected), "pitch detector frequency is non-finite");
    expect(std::abs(detected - sourceHz) < 2.0f,
           "pitch detector measured " + std::to_string(detected)
               + " Hz instead of 200 Hz at "
               + std::to_string((int) sampleRate) + " Hz");
    expect(detector.getConfidence() > 0.75f,
           "pitch detector confidence is unexpectedly low at "
               + std::to_string((int) sampleRate) + " Hz");

    detector.reset();
    expect(detector.getFrequency() == 0.0f && detector.getConfidence() == 0.0f,
           "pitch detector reset retained stale analysis");
}

void testPitchShifter()
{
    constexpr double sampleRate = 48000.0;
    const double unison = renderPitchShift(sampleRate, 200.0f, 1.0f);
    const double fine = renderPitchShift(sampleRate, 200.0f,
        std::exp2(1.0f / 1200.0f));
    const double fifth = renderPitchShift(sampleRate, 200.0f, 1.5f);

    expect(std::abs(unison - 200.0) < 0.8, "transparent unison is off pitch");
    expect(std::abs(fine - 200.1156) < 0.8, "one-cent fine tune did not enter pitch path");
    expect(std::abs(fifth - 300.0) < 2.0, "fractional 1.5x shift did not hold 300 Hz");
}

void testSaturatorAndReset()
{
    WarmSaturator saturator;
    saturator.prepare(96000.0);
    float largest = 0.0f;
    for (int i = 0; i < 96000 * 3; ++i)
    {
        const float drive = (float) (i % 96000) / 95999.0f;
        const float input = 1.8f * std::sin(juce::MathConstants<float>::twoPi
            * 997.0f * (float) i / 96000.0f);
        const float output = saturator.process(input, drive);
        expect(std::isfinite(output), "ADAA saturator emitted a non-finite sample");
        largest = juce::jmax(largest, std::abs(output));
    }
    expect(largest < 4.0f, "ADAA saturator produced an unsafe level");
    expect(saturator.process(std::numeric_limits<float>::quiet_NaN(), 1.0f) == 0.0f,
           "ADAA saturator did not contain an invalid sample");
    expect(std::isfinite(saturator.process(0.1f, 0.5f)),
           "ADAA saturator failed to recover after reset");

    VoiceBuffer buffer;
    buffer.prepare(48000.0, 0.1f);
    for (int i = 0; i < 512; ++i)
        buffer.write(1.0f);
    buffer.reset();
    expect(buffer.getWriteHead() == 0, "voice history reset retained its write head");
    expect(std::abs(buffer.readInterpolated(-1.0)) < 1.0e-8f,
           "voice history reset retained stale audio");
}
}

int main()
{
    testPitchDetector(44100.0);
    testPitchDetector(48000.0);
    testPitchDetector(96000.0);
    testPitchShifter();
    testSaturatorAndReset();

    if (failures != 0)
    {
        std::cerr << failures << " MIRROR DSP smoke test(s) failed.\n";
        return 1;
    }

    std::cout << "MIRROR DSP smoke tests passed.\n";
    return 0;
}
