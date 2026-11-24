#pragma once

#include "doomgazer/Coefficients.h"
#include "doomgazer/FirstOrderFilters.h"

namespace doomgazer {

struct InputHighpass
{
    OnePoleHp left;
    OnePoleHp right;

    void reset() noexcept
    {
        left.reset();
        right.reset();
    }

    Stereo process(double inL, double inR, double coeff) noexcept
    {
        return { left.process(inL, coeff), right.process(inR, coeff) };
    }
};

struct SubGate
{
    double env = 0.0;

    void reset() noexcept { env = 0.0; }

    void heal() noexcept
    {
        if (!std::isfinite(env))
            env = 0.0;
    }

    void process(double inL, double inR, double attack, double release) noexcept
    {
        const double peak = std::max(std::abs(inL), std::abs(inR));
        const double target = peak > 0.004 ? 1.0 : 0.0;
        const double coeff = target > env ? attack : release;
        env = target + coeff * (env - target);
    }
};

struct OpticalLeveler
{
    double fastEnv = 0.0;
    double slowEnv = 0.0;
    double gateMix = 0.0;

    void reset() noexcept
    {
        fastEnv = 0.0;
        slowEnv = 0.0;
        gateMix = 0.0;
    }

    void heal() noexcept
    {
        if (!std::isfinite(fastEnv))
            fastEnv = 0.0;
        if (!std::isfinite(slowEnv))
            slowEnv = 0.0;
        if (!std::isfinite(gateMix))
            gateMix = 0.0;
    }

    Stereo process(double inL, double inR, const Parameters& params, const Coefficients& c) noexcept
    {
        const double rect = std::max(std::abs(inL), std::abs(inR));
        fastEnv = (rect > fastEnv)
            ? (c.compAtt * fastEnv + (1.0 - c.compAtt) * rect)
            : (c.compRelFast * fastEnv + (1.0 - c.compRelFast) * rect);
        slowEnv = (fastEnv > slowEnv)
            ? (0.94 * slowEnv + 0.06 * fastEnv)
            : (c.compRelSlow * slowEnv + (1.0 - c.compRelSlow) * fastEnv);

        const double det = 0.55 * fastEnv + 0.45 * slowEnv + kDenorm;
        const double open = clamp(params.sustainOn, 0.0, 1.0);
        if (open <= 0.0008)
            return { inL, inR };

        const double gainReduction = 1.0 / (1.0 + params.sustain * 5.2 * det);
        // The gate eases makeup off under about -74 dB. With the gate off,
        // makeup stays up so a decaying note is not chopped.
        const double follow = c.follow18;
        const double gateTarget = asOn(params.inputGate) ? 1.0 : 0.0;
        gateMix += follow * (gateTarget - gateMix);
        const double quiet = clamp((det - 0.0002) / 0.0018, 0.0, 1.0);
        const double lift = 1.0 + gateMix * (quiet - 1.0);
        const double makeup = 1.0 + params.sustain * 3.8 * lift;
        const double gain = gainReduction * makeup;
        return {
            inL * (1.0 - open) + inL * gain * open,
            inR * (1.0 - open) + inR * gain * open,
        };
    }
};

struct SupplySag
{
    double env = 0.0;

    void reset() noexcept { env = 0.0; }

    void heal() noexcept
    {
        if (!std::isfinite(env))
            env = 0.0;
    }

    double process(double inL, double inR, const Parameters& params, const Coefficients& c) noexcept
    {
        const double peak = std::max(std::abs(inL), std::abs(inR));
        env = (peak > env)
            ? (c.sagAtt * env + (1.0 - c.sagAtt) * peak)
            : (c.sagRel * env + (1.0 - c.sagRel) * peak);
        env += kDenorm;
        return 1.0 / (1.0 + params.sag * 1.35 * env);
    }
};

struct BrickwallLimiter
{
    double gain = 1.0;

    void reset() noexcept { gain = 1.0; }

    void process(double& left, double& right, const Coefficients& c) noexcept
    {
        // -3 dBFS ceiling. Gain eases down over a few milliseconds, then releases.
        // The clamp catches the tip that gets through the attack.
        constexpr double ceiling = 0.7079457843831198; // -3 dBFS
        const double peak = std::max(std::abs(left), std::abs(right));
        const double target = peak > ceiling ? (ceiling / peak) : 1.0;
        const double coeff = target < gain ? c.limAtt : c.limRel;
        gain = target + coeff * (gain - target);
        left *= gain;
        right *= gain;
        if (left > ceiling)
            left = ceiling;
        else if (left < -ceiling)
            left = -ceiling;
        if (right > ceiling)
            right = ceiling;
        else if (right < -ceiling)
            right = -ceiling;
    }
};

} // namespace doomgazer
