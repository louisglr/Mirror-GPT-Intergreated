#pragma once

#include <array>
#include <cmath>
#include <juce_core/juce_core.h>

// A compensated, slightly asymmetric soft stage with first-order
// antiderivative anti-aliasing (ADAA).  Unlike a midpoint average, ADAA
// evaluates the time-integral of the actual nonlinear curve across each input
// segment, substantially reducing foldback without pretending to be a true
// oversampler or adding an unreported delay to one audio path.
class WarmSaturator
{
public:
    void prepare(double sampleRateIn)
    {
        prepared = false;
        sampleRate = std::isfinite(sampleRateIn) && sampleRateIn >= 1000.0
            ? sampleRateIn : 44100.0;
        buildCurveTables();
        const float nextDcCoeff = 1.0f - std::exp(
            -juce::MathConstants<float>::twoPi * 18.0f / (float) sampleRate);
        dcCoeff = std::isfinite(nextDcCoeff)
            ? juce::jlimit(0.0f, 1.0f, nextDcCoeff) : 0.002f;
        reset();
        prepared = true;
    }

    void reset()
    {
        dcState = 0.0f;
        shapedDcState = 0.0f;
        previousAc = 0.0f;
        hasCachedCurve = false;
    }

    float process(float x, float amount)
    {
        if (!prepared)
            return 0.0f;

        if (!std::isfinite(x) || !std::isfinite(amount) || !std::isfinite(dcCoeff)
            || !std::isfinite(dcState) || !std::isfinite(shapedDcState)
            || !std::isfinite(previousAc))
        {
            reset();
            return 0.0f;
        }

        const float a = juce::jlimit(0.0f, 1.0f, amount);
        if (a < 1.0e-5f)
        {
            // Keep the internal state coherent when saturation is automated
            // from bypass to audible drive.
            dcState += dcCoeff * (x - dcState);
            previousAc = x - dcState;
            shapedDcState += dcCoeff * (previousAc - shapedDcState);
            if (!std::isfinite(dcState) || !std::isfinite(previousAc)
                || !std::isfinite(shapedDcState))
            {
                reset();
                return 0.0f;
            }
            return x;
        }

        // The amount normally remains constant for thousands of samples once
        // an APVTS smoother has reached its target. Cache only *exactly* equal
        // values: automation still evaluates the original curve at every new
        // amount, so this removes steady-state tanh work without quantising or
        // otherwise changing the saturation sound.
        if (!hasCachedCurve || a < cachedAmount || a > cachedAmount)
            updateCurveCache(a);
        if (!hasCachedCurve)
        {
            reset();
            return 0.0f;
        }

        dcState += dcCoeff * (x - dcState);
        const float ac = x - dcState;
        const float delta = ac - previousAc;

        // ADAA is the finite difference of an antiderivative of the exact
        // tanh curve. For tiny deltas the difference quotient loses precision,
        // so evaluate the mathematically equivalent midpoint limit instead.
        const float shaped = std::abs(delta) < 1.0e-4f
            ? shapeAt(0.5f * (previousAc + ac), cachedDrive, cachedBias,
                      cachedBiasValue, cachedNormaliser)
            : (antiDerivative(ac, cachedDrive, cachedBias, cachedBiasValue,
                              cachedNormaliser)
                - antiDerivative(previousAc, cachedDrive, cachedBias,
                                 cachedBiasValue, cachedNormaliser)) / delta;
        previousAc = ac;
        // Asymmetry creates musically useful even harmonics, but it must not
        // leak a DC component into the following filters/delay lines.
        shapedDcState += dcCoeff * (shaped - shapedDcState);
        const float compensated = (shaped - shapedDcState) * cachedCompensation;
        const float output = x + (compensated - x) * a;
        if (!std::isfinite(output) || !std::isfinite(dcState)
            || !std::isfinite(shapedDcState) || !std::isfinite(previousAc))
        {
            reset();
            return 0.0f;
        }
        return output;
    }

private:
    void updateCurveCache(float amount)
    {
        cachedAmount = amount;
        cachedDrive = 1.0f + 2.4f * amount;
        cachedBias = 0.070f * amount;
        cachedNormaliser = juce::jmax(0.1f, fastTanh(cachedDrive));
        // Subtract the value at zero after the *driven* bias. This preserves
        // the intended asymmetric character while ensuring a zero input cannot
        // manufacture a static DC offset.
        cachedBiasValue = fastTanh(cachedDrive * cachedBias);
        cachedCompensation = 1.0f / (1.0f + 0.10f * amount);
        hasCachedCurve = std::isfinite(cachedAmount) && std::isfinite(cachedDrive)
            && std::isfinite(cachedBias) && std::isfinite(cachedNormaliser)
            && cachedNormaliser > 0.0f && std::isfinite(cachedBiasValue)
            && std::isfinite(cachedCompensation);
    }

    float shapeAt(float input, float drive, float bias, float biasValue,
                  float normaliser) const
    {
        return (fastTanh((input + bias) * drive) - biasValue) / normaliser;
    }

    static float exactLogCosh(float x)
    {
        // log(cosh(x)) written without an overflowing cosh() call.  The vocal
        // path can occasionally receive a hot pre-fader signal, and a robust
        // nonlinear stage must stay finite even then.
        const float magnitude = std::abs(x);
        return magnitude + std::log1p(std::exp(-2.0f * magnitude))
            - 0.6931471805599453f;
    }

    float antiDerivative(float input, float drive, float bias,
                         float biasValue, float normaliser) const
    {
        // d/dx [log(cosh(d*(x+b)))/d - x*tanh(d*b)] equals the
        // zero-centred asymmetric tanh in shapeAt().
        return (fastLogCosh((input + bias) * drive) / drive - input * biasValue)
            / normaliser;
    }

    void buildCurveTables()
    {
        for (int i = 0; i < kTableSize; ++i)
        {
            const float x = -kTableRange + (float) i * kTableStep;
            tanhTable[(size_t) i] = std::tanh(x);
            logCoshTable[(size_t) i] = exactLogCosh(x);
        }
    }

    float fastTanh(float x) const
    {
        if (!std::isfinite(x))
            return 0.0f;
        if (x <= -kTableRange)
            return -1.0f;
        if (x >= kTableRange)
            return 1.0f;

        const float tablePosition = (x + kTableRange) * kInverseTableStep;
        const int index = juce::jlimit(0, kTableSize - 2, (int) tablePosition);
        const float fraction = tablePosition - (float) index;
        const float a = tanhTable[(size_t) index];
        return a + (tanhTable[(size_t) (index + 1)] - a) * fraction;
    }

    float fastLogCosh(float x) const
    {
        if (!std::isfinite(x))
            return 0.0f;

        const float magnitude = std::abs(x);
        if (magnitude >= kTableRange)
            // The omitted log1p(exp(-2*|x|)) term is below 4e-11 at the
            // boundary. This is both cheaper and numerically continuous.
            return magnitude - kLogTwo;

        const float tablePosition = (x + kTableRange) * kInverseTableStep;
        const int index = juce::jlimit(0, kTableSize - 2, (int) tablePosition);
        const float t = tablePosition - (float) index;
        const float t2 = t * t;
        const float t3 = t2 * t;

        // Cubic Hermite interpolation uses tanh as the exact derivative of
        // log(cosh). Unlike linear interpolation, its slope is continuous at
        // table boundaries, which is important because ADAA differentiates
        // this antiderivative through a finite difference.
        const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
        const float h10 = t3 - 2.0f * t2 + t;
        const float h01 = -2.0f * t3 + 3.0f * t2;
        const float h11 = t3 - t2;
        return h00 * logCoshTable[(size_t) index]
            + h10 * kTableStep * tanhTable[(size_t) index]
            + h01 * logCoshTable[(size_t) (index + 1)]
            + h11 * kTableStep * tanhTable[(size_t) (index + 1)];
    }

    static constexpr int kTableSize = 2049;
    static constexpr float kTableRange = 12.0f;
    static constexpr float kTableStep = 2.0f * kTableRange / (float) (kTableSize - 1);
    static constexpr float kInverseTableStep = 1.0f / kTableStep;
    static constexpr float kLogTwo = 0.6931471805599453f;

    double sampleRate = 44100.0;
    float dcCoeff = 0.002f, dcState = 0.0f, shapedDcState = 0.0f;
    float previousAc = 0.0f;
    float cachedAmount = 0.0f, cachedDrive = 1.0f, cachedBias = 0.0f;
    float cachedNormaliser = 1.0f, cachedBiasValue = 0.0f;
    float cachedCompensation = 1.0f;
    std::array<float, kTableSize> tanhTable {};
    std::array<float, kTableSize> logCoshTable {};
    bool hasCachedCurve = false;
    bool prepared = false;
};
