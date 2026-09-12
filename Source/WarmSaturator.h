#pragma once

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
        sampleRate = sampleRateIn;
        dcCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 18.0f / (float) sampleRate);
        reset();
    }

    void reset()
    {
        dcState = 0.0f;
        shapedDcState = 0.0f;
        previousAc = 0.0f;
    }

    float process(float x, float amount)
    {
        if (!std::isfinite(x) || !std::isfinite(amount) || !std::isfinite(dcState))
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
            return x;
        }

        dcState += dcCoeff * (x - dcState);
        const float ac = x - dcState;
        const float drive = 1.0f + 2.4f * a;
        const float bias = 0.070f * a;
        const float normaliser = juce::jmax(0.1f, std::tanh(drive));
        // Subtract the value at zero after the *driven* bias. This preserves
        // the intended asymmetric character while ensuring a zero input cannot
        // manufacture a static DC offset.
        const float biasValue = std::tanh(drive * bias);
        const float delta = ac - previousAc;

        // ADAA is the finite difference of an antiderivative of the exact
        // tanh curve. For tiny deltas the difference quotient loses precision,
        // so evaluate the mathematically equivalent midpoint limit instead.
        const float shaped = std::abs(delta) < 1.0e-4f
            ? shapeAt(0.5f * (previousAc + ac), drive, bias, biasValue, normaliser)
            : (antiDerivative(ac, drive, bias, biasValue, normaliser)
                - antiDerivative(previousAc, drive, bias, biasValue, normaliser)) / delta;
        previousAc = ac;
        // Asymmetry creates musically useful even harmonics, but it must not
        // leak a DC component into the following filters/delay lines.
        shapedDcState += dcCoeff * (shaped - shapedDcState);
        const float compensated = (shaped - shapedDcState) / (1.0f + 0.10f * a);
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
    static float shapeAt(float input, float drive, float bias, float biasValue,
                         float normaliser)
    {
        return (std::tanh((input + bias) * drive) - biasValue) / normaliser;
    }

    static float logCosh(float x)
    {
        // log(cosh(x)) written without an overflowing cosh() call.  The vocal
        // path can occasionally receive a hot pre-fader signal, and a robust
        // nonlinear stage must stay finite even then.
        const float magnitude = std::abs(x);
        return magnitude + std::log1p(std::exp(-2.0f * magnitude))
            - 0.6931471805599453f;
    }

    static float antiDerivative(float input, float drive, float bias,
                                float biasValue, float normaliser)
    {
        // d/dx [log(cosh(d*(x+b)))/d - x*tanh(d*b)] equals the
        // zero-centred asymmetric tanh in shapeAt().
        return (logCosh((input + bias) * drive) / drive - input * biasValue)
            / normaliser;
    }

    double sampleRate = 44100.0;
    float dcCoeff = 0.002f, dcState = 0.0f, shapedDcState = 0.0f;
    float previousAc = 0.0f;
};
