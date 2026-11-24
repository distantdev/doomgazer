#pragma once

#include "doomgazer/BiquadFilter.h"
#include "doomgazer/Coefficients.h"
#include "doomgazer/Saturation.h"

namespace doomgazer {

// Post-amp clip flavor. Amount 0 returns the sample unchanged.
class Grit
{
public:
    void reset() noexcept
    {
        notchL.reset();
        notchR.reset();
        woolLpL.reset();
        woolLpR.reset();
        barkL.reset();
        barkR.reset();
        hushL1.reset();
        hushR1.reset();
        hushL2.reset();
        hushR2.reset();
        amountState = 0.0;
        characterState = 0.0;
        levelState = 1.0;
        levelTarget = 1.0;
        cachedLevelDb = -1000.0;
        cachedHz = -1.0;
        cachedSr = -1.0;
    }

    void process(double& left, double& right, const Parameters& params, const Coefficients& c) noexcept
    {
        const double target = clamp(params.gritAmount, 0.0, 1.0);
        if (target <= 1.0e-6 && amountState <= 1.0e-6)
            return;

        const double sampleRate = std::max(8000.0, c.sampleRate);
        const double follow = c.follow18;
        amountState += follow * (target - amountState);

        characterState += follow * (clamp(params.gritCharacter, 0.0, 1.0) - characterState);
        if (params.gritLevelDb != cachedLevelDb)
        {
            cachedLevelDb = params.gritLevelDb;
            levelTarget = dbToGain(cachedLevelDb);
        }
        levelState += follow * (levelTarget - levelState);
        updateCoeffs(params.gritFreqHz, sampleRate);

        const double woolL = woolSample(left, notchL, woolLpL);
        const double woolR = woolSample(right, notchR, woolLpR);
        const double grindL = grindSample(left, barkL, hushL1, hushL2);
        const double grindR = grindSample(right, barkR, hushR1, hushR2);
        const double woolMix = 1.0 - characterState;
        const double wetL = (woolL * woolMix + grindL * characterState) * levelState;
        const double wetR = (woolR * woolMix + grindR * characterState) * levelState;
        const double dry = 1.0 - amountState;
        left = left * dry + wetL * amountState;
        right = right * dry + wetR * amountState;
    }

private:
    double woolSample(double x, Biquad& notch, Biquad& lowpass) noexcept
    {
        const double first = asymClip(x * 2.1, 0.12);
        const double scooped = notch.process(first, notchCoeffs);
        const double second = saturate(scooped * 2.4);
        return lowpass.process(second, woolLpCoeffs) * 0.85;
    }

    double grindSample(double x, Biquad& bark, Biquad& hushA, Biquad& hushB) noexcept
    {
        const double bumped = bark.process(x * 1.6, barkCoeffs);
        const double slammed = hardDiode(bumped * 1.8);
        const double hushed = hushA.process(slammed, hushCoeffs);
        return hushB.process(hushed, hushCoeffs) * 0.90;
    }

    void updateCoeffs(double freq, double sampleRate) noexcept
    {
        if (std::abs(freq - cachedHz) < 0.5 && std::abs(sampleRate - cachedSr) < 0.5)
            return;
        cachedHz = freq;
        cachedSr = sampleRate;
        const double nyquist = sampleRate * 0.42;
        const double center = clamp(freq, 500.0, std::min(2500.0, nyquist));
        notchCoeffs = makeNotch(center, 0.75, sampleRate);
        woolLpCoeffs = makeLowpass(std::min(1500.0, nyquist), 0.707, sampleRate);
        barkCoeffs = makePeaking(center, 1.4, 9.0, sampleRate);
        hushCoeffs = makeLowpass(std::min(2600.0, nyquist), 0.707, sampleRate);
    }

    Biquad notchL;
    Biquad notchR;
    Biquad woolLpL;
    Biquad woolLpR;
    Biquad barkL;
    Biquad barkR;
    Biquad hushL1;
    Biquad hushR1;
    Biquad hushL2;
    Biquad hushR2;
    BiquadCoeffs notchCoeffs;
    BiquadCoeffs woolLpCoeffs;
    BiquadCoeffs barkCoeffs;
    BiquadCoeffs hushCoeffs;
    double amountState = 0.0;
    double characterState = 0.0;
    double levelState = 1.0;
    double levelTarget = 1.0;
    double cachedLevelDb = -1000.0;
    double cachedHz = -1.0;
    double cachedSr = -1.0;
};

} // namespace doomgazer
