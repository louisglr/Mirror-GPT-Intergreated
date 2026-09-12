#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "PitchDetector.h"
#include "PhaseLockedPsolaGrainVoice.h"
#include "PitchCorrector.h"
#include "VoiceFilter.h"
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

double renderPitchShift(double sampleRate, float sourceHz, float ratio,
                        float renewalStagger = 0.0f)
{
    VoiceBuffer buffer;
    PhaseLockedPsolaGrainVoice voice;
    buffer.prepare(sampleRate, 1.0f);
    voice.prepare(sampleRate, renewalStagger);

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
    const double fifth = renderPitchShift(sampleRate, 200.0f, 1.5f, 0.4f);

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

    // The real-time path uses a precomputed Hermite antiderivative instead of
    // transcendental functions. Compare it with the exact ADAA equation so a
    // future CPU optimisation cannot silently change the saturation colour.
    saturator.prepare(48000.0);
    constexpr float amount = 0.73f;
    const float drive = 1.0f + 2.4f * amount;
    const float bias = 0.070f * amount;
    const float biasValue = std::tanh(drive * bias);
    const float normaliser = juce::jmax(0.1f, std::tanh(drive));
    const float compensation = 1.0f / (1.0f + 0.10f * amount);
    const float dcCoefficient = 1.0f - std::exp(
        -juce::MathConstants<float>::twoPi * 18.0f / 48000.0f);
    float dcState = 0.0f, shapedDcState = 0.0f, previousAc = 0.0f;
    float maximumCurveError = 0.0f;
    const auto exactLogCosh = [] (float x)
    {
        const float magnitude = std::abs(x);
        return magnitude + std::log1p(std::exp(-2.0f * magnitude))
            - 0.6931471805599453f;
    };
    const auto exactShape = [=] (float x)
    {
        return (std::tanh((x + bias) * drive) - biasValue) / normaliser;
    };
    const auto exactAntiDerivative = [=] (float x)
    {
        return (exactLogCosh((x + bias) * drive) / drive - x * biasValue)
            / normaliser;
    };
    for (int i = 0; i < 48000; ++i)
    {
        const float input = 0.72f * std::sin(juce::MathConstants<float>::twoPi
            * 997.0f * (float) i / 48000.0f);
        dcState += dcCoefficient * (input - dcState);
        const float ac = input - dcState;
        const float delta = ac - previousAc;
        const float shaped = std::abs(delta) < 1.0e-4f
            ? exactShape(0.5f * (previousAc + ac))
            : (exactAntiDerivative(ac) - exactAntiDerivative(previousAc)) / delta;
        previousAc = ac;
        shapedDcState += dcCoefficient * (shaped - shapedDcState);
        const float exactOutput = input
            + ((shaped - shapedDcState) * compensation - input) * amount;
        const float tableOutput = saturator.process(input, amount);
        maximumCurveError = juce::jmax(maximumCurveError,
            std::abs(tableOutput - exactOutput));
    }
    expect(maximumCurveError < 5.0e-4f,
           "fast ADAA curve drifted audibly from its exact reference");

    VoiceBuffer buffer;
    buffer.prepare(48000.0, 0.1f);
    for (int i = 0; i < 512; ++i)
        buffer.write(1.0f);
    buffer.reset();
    expect(buffer.getWriteHead() == 0, "voice history reset retained its write head");
    expect(std::abs(buffer.readInterpolated(-1.0)) < 1.0e-8f,
           "voice history reset retained stale audio");
    for (int i = 0; i < 32; ++i)
        buffer.write(0.25f);
    expect(std::abs(buffer.readInterpolated(15.0) - 0.25f) < 1.0e-5f,
           "voice history did not recover after its O(1) reset");
    expect(std::abs(buffer.readInterpolated(-1.0)) < 1.0e-8f,
           "voice history exposed pre-reset circular-buffer contents");
}

void testDefensiveRecovery()
{
    const float nan = std::numeric_limits<float>::quiet_NaN();

    VoiceFilter filter;
    filter.prepare(48000.0);
    filter.setCutoffs(nan, nan);
    for (int i = 0; i < 4096; ++i)
    {
        const float input = 0.3f * std::sin(juce::MathConstants<float>::twoPi
            * 440.0f * (float) i / 48000.0f);
        expect(std::isfinite(filter.process(input)),
               "voice filter was poisoned by an invalid cutoff");
    }
    filter.setCutoffs(120.0f, 9000.0f);
    expect(std::isfinite(filter.process(0.1f)),
           "voice filter did not accept a valid target after invalid automation");

    expect(PitchCorrector::nearestScaleMidi(nan, 0, PitchCorrector::Major) == 69,
           "scale quantiser did not contain invalid pitch input");
    expect(std::isfinite(PitchCorrector::midiToFreq(std::numeric_limits<int>::max())),
           "MIDI-to-frequency conversion did not clamp an invalid note range");

    // Lifecycle guards are intentionally cheap, but they prevent malformed
    // hosts or validation tools from indexing unprepared buffers.
    VoiceBuffer unpreparedBuffer;
    unpreparedBuffer.write(1.0f);
    expect(unpreparedBuffer.readInterpolated(0.0) == 0.0f,
           "unprepared voice history did not fail closed");
    PitchDetector unpreparedDetector;
    unpreparedDetector.pushSample(1.0f);
    expect(unpreparedDetector.getFrequency() == 0.0f,
           "unprepared pitch detector did not fail closed");
    PhaseLockedPsolaGrainVoice unpreparedVoice;
    expect(unpreparedVoice.process(unpreparedBuffer, 1.5f, 200.0f) == 0.0f,
           "unprepared PSOLA voice did not fail closed");
    WarmSaturator unpreparedSaturator;
    expect(unpreparedSaturator.process(0.1f, 0.5f) == 0.0f,
           "unprepared saturator did not fail closed");
}
}

int main()
{
    testPitchDetector(22050.0);
    testPitchDetector(24000.0);
    testPitchDetector(29400.0);
    testPitchDetector(44100.0);
    testPitchDetector(48000.0);
    testPitchDetector(96000.0);
    testPitchDetector(192000.0);
    testPitchShifter();
    testSaturatorAndReset();
    testDefensiveRecovery();

    if (failures != 0)
    {
        std::cerr << failures << " MIRROR DSP smoke test(s) failed.\n";
        return 1;
    }

    std::cout << "MIRROR DSP smoke tests passed.\n";
    return 0;
}
