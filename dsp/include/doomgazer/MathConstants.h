#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace doomgazer {

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kDenorm = 1.0e-24;
inline constexpr int kParameterCount = 100;

inline double monoSum(double l, double r, bool invertR) noexcept
{
    return 0.5 * (l + (invertR ? -r : r));
}

inline void duplicateMono(double m, double& l, double& r) noexcept
{
    l = r = m;
}

inline double clamp(double value, double lo, double hi) noexcept
{
    return std::max(lo, std::min(hi, value));
}

// dry * (1 - solo) + sub. At solo 0 the current dry-plus-sub mix is unchanged.
inline double mixSubSolo(double dry, double sub, double solo) noexcept
{
    const double t = clamp(solo, 0.0, 1.0);
    return dry * (1.0 - t) + sub;
}

inline double dbToGain(double db) noexcept
{
    return std::pow(10.0, db / 20.0);
}

inline double gainToDb(double gain) noexcept
{
    return 20.0 * std::log10(std::max(gain, 1.0e-12));
}

inline int asInt(double value) noexcept
{
    return static_cast<int>(std::floor(value + 0.5));
}

inline bool asOn(double value) noexcept
{
    return value > 0.5;
}

inline bool isBad(double value) noexcept
{
    return !std::isfinite(value);
}

struct Stereo
{
    double l = 0.0;
    double r = 0.0;
};

struct BufferLimits
{
    int atmos = 8192;
    int c1 = 64;
    int c2 = 64;
    int c3 = 64;
    int c4 = 64;
    int ap1 = 32;
    int ap2 = 32;
    int glide = 4096;
    int shimGrainL = 1024;
    int shimLenL = 4096;
    int shimGrainR = 1024;
    int shimLenR = 4096;
    double sampleRate = 48000.0;
};

inline BufferLimits makeBufferLimits(double sampleRate) noexcept
{
    BufferLimits limits;
    limits.sampleRate = sampleRate;
    limits.atmos = std::max(8192, static_cast<int>(std::ceil(2.6 * sampleRate)) + 64);
    limits.c1 = static_cast<int>(std::ceil(0.120 * sampleRate)) + 8;
    limits.c2 = static_cast<int>(std::ceil(0.145 * sampleRate)) + 8;
    limits.c3 = static_cast<int>(std::ceil(0.175 * sampleRate)) + 8;
    limits.c4 = static_cast<int>(std::ceil(0.210 * sampleRate)) + 8;
    limits.ap1 = static_cast<int>(std::ceil(0.025 * sampleRate)) + 8;
    limits.ap2 = static_cast<int>(std::ceil(0.045 * sampleRate)) + 8;
    limits.glide = std::max(4096, static_cast<int>(std::ceil(0.420 * sampleRate)) + 64);
    limits.shimGrainL = std::max(1024, static_cast<int>(std::floor(0.085 * sampleRate)));
    limits.shimLenL = limits.shimGrainL * 4;
    limits.shimGrainR = std::max(1024, static_cast<int>(std::floor(0.095 * sampleRate)));
    limits.shimLenR = limits.shimGrainR * 4;
    return limits;
}

} // namespace doomgazer
