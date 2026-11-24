#pragma once

#include "doomgazer/BiquadFilter.h"
#include "doomgazer/Coefficients.h"
#include "doomgazer/Saturation.h"

namespace doomgazer {

// Fixed-partial resonator. Amount 0 returns before any processing.
class Howl
{
public:
    void reset() noexcept
    {
        bandL.reset();
        bandR.reset();
        recircL = 0.0;
        recircR = 0.0;
        bodyEnv = 0.0;
        wetGain = 0.0;
        amountState = 0.0;
        smoothedHz = 220.0;
        coeffHz = -1.0;
        coeffSr = -1.0;
        bodySr = -1.0;
    }

    void process(double& left, double& right, const Parameters& params, const Coefficients& c) noexcept
    {
        const double amount = clamp(params.howlAmount, 0.0, 1.0);
        if (amount <= 1.0e-6 && amountState <= 1.0e-5
            && std::abs(recircL) < 1.0e-5 && std::abs(recircR) < 1.0e-5)
        {
            if (wetGain != 0.0 || bodyEnv != 0.0)
            {
                wetGain = 0.0;
                bodyEnv = 0.0;
                recircL = 0.0;
                recircR = 0.0;
                bandL.reset();
                bandR.reset();
            }
            return;
        }

        const double sampleRate = std::max(8000.0, c.sampleRate);
        const double follow = c.follow18;
        amountState += follow * (amount - amountState);
        const int pitch = std::max(0, std::min(3, asInt(params.howlPitch)));
        static constexpr double kPartials[] = { 220.0, 330.0, 440.0, 550.0 };
        const double targetHz = kPartials[pitch];
        smoothedHz += follow * (targetHz - smoothedHz);
        if (coeffHz < 0.0 || std::abs(smoothedHz - coeffHz) > 0.5 || std::abs(sampleRate - coeffSr) > 0.5)
        {
            coeffHz = smoothedHz;
            coeffSr = sampleRate;
            bandCoeffs = makeBandpass(smoothedHz, 8.0, sampleRate);
        }

        const double mono = 0.5 * (left + right);
        if (sampleRate != bodySr)
        {
            bodySr = sampleRate;
            bodyFollowCoeff = 1.0 - std::exp(-1.0 / (0.045 * sampleRate));
        }
        bodyEnv += bodyFollowCoeff * (std::abs(mono) - bodyEnv);

        // Sense lowers the note level that opens the resonance. The dry guitar stays.
        const double sense = clamp(params.howlSense, 0.0, 1.0);
        const double thresh = 0.040 - sense * 0.030;
        const double openTarget = clamp(bodyEnv / std::max(thresh, 0.008), 0.0, 1.0);
        wetGain += follow * (openTarget - wetGain);

        // Distort the feed so a clean note still has the selected partial to ring.
        const double excited = asymClip(mono * (2.4 + 1.2 * sense), 0.45);
        const double regen = amountState * (0.55 + 0.38 * sense) * (0.35 + 0.65 * wetGain);
        const double driveL = excited + clamp(recircL, -0.9, 0.9) * regen;
        const double driveR = excited + clamp(recircR, -0.9, 0.9) * regen;
        double wetL = bandL.process(driveL, bandCoeffs);
        double wetR = bandR.process(driveR, bandCoeffs);
        recircL = wetL;
        recircR = wetR;

        const double howlGain = 2.20 * amountState;
        double addL = wetL * howlGain;
        double addR = wetR * howlGain;
        if (!asOn(params.howlStereo))
        {
            const double add = 0.5 * (addL + addR);
            addL = add;
            addR = add;
        }
        left += addL;
        right += addR;
    }

private:
    Biquad bandL;
    Biquad bandR;
    BiquadCoeffs bandCoeffs;
    double recircL = 0.0;
    double recircR = 0.0;
    double bodyEnv = 0.0;
    double wetGain = 0.0;
    double amountState = 0.0;
    double bodyFollowCoeff = 0.0;
    double bodySr = -1.0;
    double smoothedHz = 220.0;
    double coeffHz = -1.0;
    double coeffSr = -1.0;
};

} // namespace doomgazer
