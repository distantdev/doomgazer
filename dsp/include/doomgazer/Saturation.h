#pragma once

#include "doomgazer/MathConstants.h"

namespace doomgazer {

// Monotonic asymptotic saturator. Hard-clamps past the wavefold region.
inline double saturate(double x) noexcept
{
    if (x > 4.5)
        return 1.0;
    if (x < -4.5)
        return -1.0;
    const double x2 = x * x;
    return x * (27.0 + x2) / (27.0 + 9.0 * x2);
}

// Symmetric diode slam. It flattens sooner than saturate and stays finite.
inline double hardDiode(double x) noexcept
{
    const double limited = clamp(x, -2.4, 2.4);
    const double y = limited / (1.0 + std::abs(limited) * 1.15);
    return y * 1.55;
}

// Asymmetric clipper. Positive peaks fold sooner than negative ones.
inline double asymClip(double x, double asym) noexcept
{
    const double posGain = 1.0 + asym * 0.55;
    const double negGain = 1.0 - asym * 0.35;
    if (x >= 0.0)
        return saturate(x * posGain) / posGain;
    return saturate(x * negGain) / negGain;
}

} // namespace doomgazer
