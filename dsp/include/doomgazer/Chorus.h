#pragma once

#include "doomgazer/Coefficients.h"
#include "doomgazer/DelayLine.h"
#include "doomgazer/FirstOrderFilters.h"

namespace doomgazer {

// Modulated short delay chorus. Mix 0 returns the sample unchanged.
class Chorus
{
public:
    void prepare(const BufferLimits& limits) noexcept
    {
        const int cap = std::max(128, static_cast<int>(std::ceil(0.040 * limits.sampleRate)) + 8);
        bufL.assign(cap);
        bufR.assign(cap);
        const double pole = rcLpPole(1400.0, limits.sampleRate);
        baritoneA0 = 1.0 - pole;
        baritoneB1 = pole;
        sampleRate = limits.sampleRate;
    }

    void reset() noexcept
    {
        bufL.clear();
        bufR.clear();
        baritoneL.reset();
        baritoneR.reset();
        mixState = 0.0;
        phase = 0.0;
    }

    void process(double& left, double& right, const Parameters& params, const Coefficients& c) noexcept
    {
        const double target = clamp(params.chorusMix, 0.0, 1.0);
        if (target <= 1.0e-6 && mixState <= 1.0e-6)
            return;

        const double sr = std::max(8000.0, c.sampleRate);
        const double follow = c.follow18;
        mixState += follow * (target - mixState);

        const double rate = clamp(params.chorusRate, 0.08, 1.2);
        phase += (2.0 * kPi * rate) / sr;
        if (phase >= 2.0 * kPi)
            phase -= 2.0 * kPi;
        const double lfo = 0.5 + 0.5 * std::sin(phase);
        const double delayMs = 12.0 + 16.0 * lfo;
        const double delaySamps = delayMs * 0.001 * sr;
        const double detunedSamps = delaySamps / kDetuneRatio;

        bufL.writeCursor(left);
        bufR.writeCursor(right);
        double wetL = bufL.readInterp(delaySamps) + bufL.readInterp(detunedSamps) * 0.18;
        double wetR = bufR.readInterp(delaySamps) + bufR.readInterp(detunedSamps) * 0.18;
        bufL.advance();
        bufR.advance();

        if (asInt(params.chorusBaritone) > 0)
        {
            wetL = baritoneL.process(wetL, baritoneA0, baritoneB1);
            wetR = baritoneR.process(wetR, baritoneA0, baritoneB1);
        }

        const double dry = 1.0 - mixState;
        if (!asOn(params.chorusStereo))
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
    static constexpr double kDetuneRatio = 1.00462468913496; // 2^(8/1200)

    RingBuffer bufL;
    RingBuffer bufR;
    OnePoleLp baritoneL;
    OnePoleLp baritoneR;
    double baritoneA0 = 0.5;
    double baritoneB1 = 0.5;
    double sampleRate = 48000.0;
    double mixState = 0.0;
    double phase = 0.0;
};

} // namespace doomgazer
