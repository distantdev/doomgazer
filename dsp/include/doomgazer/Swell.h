#pragma once

#include "doomgazer/Coefficients.h"

namespace doomgazer {

// Input volume swell on both channels. Amount 0 is gain 1.
class Swell
{
public:
    void reset() noexcept
    {
        gainState = 1.0;
        ramp = 1.0;
        peakEnv = 0.0;
        bodyEnv = 0.0;
        cachedSr = -1.0;
    }

    void process(double& left, double& right, const Parameters& params, const Coefficients& c) noexcept
    {
        const double amount = clamp(params.swellAmount, 0.0, 1.0);
        if (amount <= 1.0e-6 && gainState >= 0.999)
            return;

        const double sampleRate = std::max(8000.0, c.sampleRate);
        if (amount > 1.0e-6)
        {
            const double level = std::max(std::abs(left), std::abs(right));
            if (sampleRate != cachedSr)
            {
                cachedSr = sampleRate;
                peakFollow = 1.0 - std::exp(-1.0 / (0.004 * sampleRate));
                bodyAttack = 1.0 - std::exp(-1.0 / (0.008 * sampleRate));
                bodyRelease = 1.0 - std::exp(-1.0 / (0.200 * sampleRate));
            }
            const double bodyFollow = level > bodyEnv ? bodyAttack : bodyRelease;
            peakEnv += peakFollow * (level - peakEnv);
            bodyEnv += bodyFollow * (level - bodyEnv);

            if (peakEnv > bodyEnv + 0.0015 && peakEnv > 0.012)
                ramp = 0.0;

            const double attackMs = clamp(params.swellAttack, 20.0, 1200.0);
            const double attackSamps = std::max(1.0, attackMs * 0.001 * sampleRate);
            ramp = std::min(1.0, ramp + 1.0 / attackSamps);
        }

        const double targetGain = (1.0 - amount) + amount * ramp;
        gainState += c.follow12 * (targetGain - gainState);

        left *= gainState;
        right *= gainState;
    }

private:
    double gainState = 1.0;
    double ramp = 1.0;
    double peakEnv = 0.0;
    double bodyEnv = 0.0;
    double cachedSr = -1.0;
    double peakFollow = 0.0;
    double bodyAttack = 0.0;
    double bodyRelease = 0.0;
};

} // namespace doomgazer
