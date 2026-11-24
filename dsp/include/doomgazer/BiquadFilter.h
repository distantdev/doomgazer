#pragma once

#include "doomgazer/MathConstants.h"

namespace doomgazer {

struct BiquadCoeffs
{
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
};

// Direct form I, matching the JSFX enclosure filters.
// Feedback coefficients are stored with their cookbook sign and subtracted.
struct Biquad
{
    double x1 = 0.0;
    double x2 = 0.0;
    double y1 = 0.0;
    double y2 = 0.0;

    void reset() noexcept
    {
        x1 = x2 = y1 = y2 = 0.0;
    }

    void heal() noexcept
    {
        if (!std::isfinite(y1) || std::abs(y1) > 20.0)
            reset();
    }

    double process(double x, const BiquadCoeffs& c) noexcept
    {
        const double y = c.b0 * x + c.b1 * x1 + c.b2 * x2 - c.a1 * y1 - c.a2 * y2 + kDenorm;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }
};

// A biquad whose coefficients ease toward the block target instead of
// stepping at the block boundary. Used where a knob is likely to be automated.
struct SmoothedBiquad
{
    void reset() noexcept
    {
        filter.reset();
        current = {};
        target = {};
        primed = false;
    }

    void heal() noexcept { filter.heal(); }

    void setTarget(const BiquadCoeffs& next) noexcept
    {
        if (!primed)
        {
            current = next;
            target = next;
            primed = true;
        }
        else
        {
            target = next;
        }
    }

    double process(double x) noexcept
    {
        current.b0 += coeff * (target.b0 - current.b0);
        current.b1 += coeff * (target.b1 - current.b1);
        current.b2 += coeff * (target.b2 - current.b2);
        current.a1 += coeff * (target.a1 - current.a1);
        current.a2 += coeff * (target.a2 - current.a2);
        return filter.process(x, current);
    }

    Biquad filter;
    BiquadCoeffs current;
    BiquadCoeffs target;
    double coeff = 0.002;
    bool primed = false;
};

inline BiquadCoeffs makePeaking(double fc, double q, double gainDb, double sampleRate) noexcept
{
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * fc / sampleRate;
    const double cosw = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0Inv = 1.0 / (1.0 + alpha / A);
    BiquadCoeffs c;
    c.b0 = (1.0 + alpha * A) * a0Inv;
    c.b1 = (-2.0 * cosw) * a0Inv;
    c.b2 = (1.0 - alpha * A) * a0Inv;
    c.a1 = c.b1;
    c.a2 = (1.0 - alpha / A) * a0Inv;
    return c;
}

inline BiquadCoeffs makeNotch(double fc, double q, double sampleRate) noexcept
{
    const double nyquist = std::max(50.0, sampleRate * 0.45);
    const double freq = clamp(fc, 40.0, nyquist);
    const double w0 = 2.0 * kPi * freq / sampleRate;
    const double cosw = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * clamp(q, 0.2, 18.0));
    const double a0Inv = 1.0 / (1.0 + alpha);
    BiquadCoeffs c;
    c.b0 = a0Inv;
    c.b1 = (-2.0 * cosw) * a0Inv;
    c.b2 = a0Inv;
    c.a1 = c.b1;
    c.a2 = (1.0 - alpha) * a0Inv;
    return c;
}

// Constant peak gain. Resonance narrows the peak without raising its level.
inline BiquadCoeffs makeBandpass(double fc, double q, double sampleRate) noexcept
{
    const double nyquist = std::max(50.0, sampleRate * 0.45);
    const double freq = clamp(fc, 40.0, nyquist);
    const double w0 = 2.0 * kPi * freq / sampleRate;
    const double cosw = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * clamp(q, 0.2, 18.0));
    const double a0Inv = 1.0 / (1.0 + alpha);
    BiquadCoeffs c;
    c.b0 = alpha * a0Inv;
    c.b1 = 0.0;
    c.b2 = -c.b0;
    c.a1 = (-2.0 * cosw) * a0Inv;
    c.a2 = (1.0 - alpha) * a0Inv;
    return c;
}

inline BiquadCoeffs makeLowpass(double fc, double q, double sampleRate) noexcept
{
    const double w0 = 2.0 * kPi * fc / sampleRate;
    const double cosw = std::cos(w0);
    const double sinw = std::sin(w0);
    const double alpha = sinw / (2.0 * q);
    const double a0Inv = 1.0 / (1.0 + alpha);
    BiquadCoeffs c;
    c.b1 = (1.0 - cosw) * a0Inv;
    c.b0 = c.b1 * 0.5;
    c.b2 = c.b0;
    c.a1 = (-2.0 * cosw) * a0Inv;
    c.a2 = (1.0 - alpha) * a0Inv;
    return c;
}

} // namespace doomgazer
