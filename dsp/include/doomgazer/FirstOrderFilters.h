#pragma once

#include "doomgazer/MathConstants.h"

namespace doomgazer {

inline double rcLpPole(double fc, double sampleRate) noexcept
{
    return std::exp(-2.0 * kPi * fc / sampleRate);
}

inline double rcHpCoeff(double fc, double sampleRate) noexcept
{
    return 1.0 / (1.0 + 2.0 * kPi * fc / sampleRate);
}

// y = (1 - pole) * x + pole * y + denorm
struct OnePoleLp
{
    double z = 0.0;

    void reset() noexcept { z = 0.0; }

    void setState(double value) noexcept { z = value; }

    double process(double x, double a0, double b1) noexcept
    {
        z = a0 * x + b1 * z + kDenorm;
        return z;
    }
};

// y = a * (y + x - x_prev) + denorm
struct OnePoleHp
{
    double y = 0.0;
    double prev = 0.0;

    void reset() noexcept
    {
        y = 0.0;
        prev = 0.0;
    }

    double process(double x, double a) noexcept
    {
        y = a * (y + x - prev) + kDenorm;
        prev = x;
        return y;
    }
};

// y += coeff * (x - y) + denorm, where coeff = 1 - exp(-2 pi fc / sr)
struct OnePoleSmooth
{
    double z = 0.0;

    void reset() noexcept { z = 0.0; }

    double process(double x, double coeff) noexcept
    {
        z += coeff * (x - z) + kDenorm;
        return z;
    }
};

} // namespace doomgazer
