#pragma once

#include "doomgazer/BiquadFilter.h"
#include "doomgazer/Coefficients.h"

namespace doomgazer {

// Expression pedal. Toe never adds gain.
// Volume: heel is silence, toe is the level already leaving the amp.
// Wah: heel is a dark vowel, toe is an open one, at a constant peak.
class Pedal
{
public:
    void reset() noexcept
    {
        bandL.reset();
        bandR.reset();
        gain = 1.0;
        smoothedHz = -1.0;
        cachedToe = -1.0;
        cachedWahToe = -1.0;
        volumeTarget = 1.0;
        wahHz = 300.0;
    }

    void process(double& left, double& right, const Parameters& params, const Coefficients& c) noexcept
    {
        if (!asOn(params.pedalOn))
        {
            if (gain >= 0.999)
                return;
            gain += c.follow12 * (1.0 - gain);
            if (gain < 0.999)
            {
                left *= gain;
                right *= gain;
            }
            return;
        }

        const double sampleRate = std::max(8000.0, c.sampleRate);
        const double follow = c.follow12;

        const double toe = clamp(params.pedalToe, 0.0, 1.0);
        if (asInt(params.pedalMode) <= 0)
        {
            if (toe != cachedToe)
            {
                cachedToe = toe;
                volumeTarget = std::pow(toe, 1.6);
            }
            gain += follow * (volumeTarget - gain);
            left *= gain;
            right *= gain;
            return;
        }

        if (toe != cachedWahToe)
        {
            cachedWahToe = toe;
            wahHz = 300.0 * std::pow(5.0, toe);
        }
        const double hz = wahHz;
        if (smoothedHz < 0.0)
            smoothedHz = hz;
        smoothedHz += follow * (hz - smoothedHz);
        if (std::abs(smoothedHz - coeffHz) > 1.0 || std::abs(sampleRate - coeffSr) > 0.5)
        {
            coeffHz = smoothedHz;
            coeffSr = sampleRate;
            bandCoeffs = makeBandpass(smoothedHz, 3.4, sampleRate);
        }
        const double wet = 0.78;
        const double dry = 1.0 - wet;
        left = left * dry + bandL.process(left, bandCoeffs) * wet;
        right = right * dry + bandR.process(right, bandCoeffs) * wet;
    }

private:
    Biquad bandL;
    Biquad bandR;
    BiquadCoeffs bandCoeffs;
    double gain = 1.0;
    double smoothedHz = -1.0;
    double cachedToe = -1.0;
    double cachedWahToe = -1.0;
    double volumeTarget = 1.0;
    double wahHz = 300.0;
    double coeffHz = -1.0;
    double coeffSr = 0.0;
};

} // namespace doomgazer
