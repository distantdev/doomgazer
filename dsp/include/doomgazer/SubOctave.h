#pragma once

#include "doomgazer/BiquadFilter.h"
#include "doomgazer/Coefficients.h"
#include "doomgazer/MathConstants.h"
#include "doomgazer/OctaveDivider.h"

namespace doomgazer {

class SubOctave
{
public:
    void reset() noexcept
    {
        pre.reset();
        tone.reset();
        divider.reset();
    }

    double process(double mono, const Parameters& params, const Coefficients& c) noexcept
    {
        if (params.subOctLevel <= 1.0e-6 && level <= 1.0e-6)
            return 0.0;

        const double follow = c.follow18;
        level += follow * (params.subOctLevel - level);

        const double filtered = pre.process(mono, c.subPre);
        const double raw = divider.process(filtered, c.sampleRate);
        const double toned = tone.process(raw, c.subTone);
        const double voiced = subAtPitch(filtered, toned, params.subPitch, dc, c.sampleRate);
        return voiced * level * 1.6;
    }

private:
    Biquad pre;
    Biquad tone;
    OctaveDivider divider;
    double level = 0.0;
    double dc = 0.0;
};

} // namespace doomgazer
