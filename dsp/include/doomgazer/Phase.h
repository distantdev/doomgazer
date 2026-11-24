#pragma once

#include "doomgazer/BiquadFilter.h"
#include "doomgazer/Coefficients.h"

namespace doomgazer {

// Cascaded allpass phaser. Mix 0 returns the sample unchanged.
class Phase
{
public:
    void reset() noexcept
    {
        for (int i = 0; i < 4; ++i)
        {
            apL[i].reset();
            apR[i].reset();
        }
        mixState = 0.0;
        fbL = 0.0;
        fbR = 0.0;
        phaseL = 0.0;
        phaseR = 0.5 * kPi;
        cachedSr = -1.0;
        cachedFcL = -1.0;
        cachedFcR = -1.0;
        rebuildCountdown = 0;
    }

    void process(double& left, double& right, const Parameters& params, const Coefficients& c) noexcept
    {
        const double target = clamp(params.phaseMix, 0.0, 1.0);
        if (target <= 1.0e-6 && mixState <= 1.0e-6)
            return;

        const double sampleRate = std::max(8000.0, c.sampleRate);
        const double follow = c.follow18;
        mixState += follow * (target - mixState);

        const double rate = clamp(params.phaseRate, 0.05, 4.0);
        const double depth = clamp(params.phaseDepth, 0.0, 1.0);
        const double feedback = clamp(params.phaseFeedback, 0.0, 0.85);

        const bool stereo = asOn(params.phaseStereo);
        phaseL += (2.0 * kPi * rate) / sampleRate;
        if (stereo)
            phaseR += (2.0 * kPi * rate) / sampleRate;
        else
            phaseR = phaseL;
        if (phaseL >= 2.0 * kPi)
            phaseL -= 2.0 * kPi;
        if (phaseR >= 2.0 * kPi)
            phaseR -= 2.0 * kPi;

        const double lfoL = 0.5 + 0.5 * std::sin(phaseL);
        const double lfoR = stereo ? (0.5 + 0.5 * std::sin(phaseR)) : lfoL;
        const double nyquist = sampleRate * 0.42;
        const double fcMin = 320.0;
        const double fcMax = std::min(3500.0, nyquist);
        const double centerL = fcMin + (fcMax - fcMin) * lfoL * depth;
        const double centerR = fcMin + (fcMax - fcMin) * lfoR * depth;

        if (std::abs(sampleRate - cachedSr) > 0.5
            || std::abs(centerL - cachedFcL) > 8.0
            || std::abs(centerR - cachedFcR) > 8.0
            || rebuildCountdown <= 0)
        {
            cachedSr = sampleRate;
            cachedFcL = centerL;
            cachedFcR = centerR;
            rebuildCountdown = 32;
            coeffL = makeAllpass(centerL, 0.62, sampleRate);
            coeffR = stereo ? makeAllpass(centerR, 0.62, sampleRate) : coeffL;
        }
        else
        {
            --rebuildCountdown;
        }

        const double fbClampL = clamp(fbL, -0.85, 0.85);
        const double fbClampR = clamp(fbR, -0.85, 0.85);
        double wetL = left + fbClampL * feedback;
        double wetR = right + fbClampR * feedback;
        for (int i = 0; i < 4; ++i)
        {
            wetL = apL[i].process(wetL, coeffL);
            wetR = apR[i].process(wetR, coeffR);
        }
        fbL = clamp(wetL, -0.85, 0.85);
        fbR = clamp(wetR, -0.85, 0.85);

        const double dry = 1.0 - mixState;
        if (!stereo)
        {
            const double wet = 0.5 * (wetL + wetR);
            const double out = left * dry + wet * mixState;
            duplicateMono(out, left, right);
            return;
        }
        left = left * dry + wetL * mixState;
        right = right * dry + wetR * mixState;
    }

private:
    static BiquadCoeffs makeAllpass(double fc, double q, double sampleRate) noexcept
    {
        const double nyquist = std::max(50.0, sampleRate * 0.45);
        const double freq = clamp(fc, 40.0, nyquist);
        const double w0 = 2.0 * kPi * freq / sampleRate;
        const double cosw = std::cos(w0);
        const double alpha = std::sin(w0) / (2.0 * clamp(q, 0.2, 18.0));
        const double a0Inv = 1.0 / (1.0 + alpha);
        BiquadCoeffs coeffs;
        coeffs.b0 = (1.0 - alpha) * a0Inv;
        coeffs.b1 = (-2.0 * cosw) * a0Inv;
        coeffs.b2 = 1.0;
        coeffs.a1 = coeffs.b1;
        coeffs.a2 = (1.0 - alpha) * a0Inv;
        return coeffs;
    }

    Biquad apL[4];
    Biquad apR[4];
    BiquadCoeffs coeffL;
    BiquadCoeffs coeffR;
    double mixState = 0.0;
    double fbL = 0.0;
    double fbR = 0.0;
    double phaseL = 0.0;
    double phaseR = 0.5 * kPi;
    double cachedSr = -1.0;
    double cachedFcL = -1.0;
    double cachedFcR = -1.0;
    int rebuildCountdown = 0;
};

} // namespace doomgazer
