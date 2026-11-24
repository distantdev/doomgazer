#pragma once

#include "doomgazer/Coefficients.h"
#include "doomgazer/OctaveDivider.h"
#include "doomgazer/Saturation.h"

namespace doomgazer {

class HarmonicCore
{
public:
    void reset() noexcept
    {
        toneHp1.reset();
        toneHp2.reset();
        postLp.reset();
        tiltLp.reset();
        subLp.reset();
        subOctLp.reset();
        subDivider.reset();
    }

    // Clears the divider and the filters the JSFX resets on an engine change.
    void resetSwitch() noexcept
    {
        subLp.reset();
        subOctLp.reset();
        subDivider.reset();
    }

    double process(double mono, double sagGain, double subGate, double subGen,
                   const Parameters& params, const Coefficients& c) noexcept
    {
        const double hp1 = toneHp1.process(mono, c.clToneHpA);
        const double hp2 = toneHp2.process(hp1, c.clToneHpA);

        const double driven = hp2 * (0.75 * c.cleanGain) * sagGain;
        const double saturated = asymClip(driven, 0.25);
        const double lowpassed = postLp.process(saturated, c.is1A0, c.is1B1);
        const double tilt = tiltLp.process(lowpassed, c.clTiltA0, c.clTiltB1);
        const double core = (tilt * c.clTiltLo + (lowpassed - tilt) * c.clTiltHi) * (c.cleanMakeup * c.cleanTrim);

        if (subAmount <= 0.0008 && params.subWeight <= 0.001)
            return mixSubSolo(core + subGen, 0.0, params.subSolo);

        const double follow = c.follow18;
        subAmount += follow * (params.subWeight - subAmount);

        const double tracked = subLp.process(mono, c.subOctLpA0, c.subOctLpB1);
        const double voiced = subDivider.process(tracked, c.sampleRate, asInt(params.coreSubShift)) * subGate;
        const double raw = voiced;

        double subSum = raw;
        if (params.subDrive > 0.001)
        {
            const double drive = raw * (1.0 + params.subDrive * 3.5);
            const double shaped = asymClip(drive, 0.30 * params.subDrive);
            subSum = raw * (1.0 - params.subDrive * 0.35) + shaped * (params.subDrive * 0.75);
        }

        return mixSubSolo(core + subGen, subSum * (subAmount * 1.6), params.subSolo);
    }

private:
    OnePoleHp toneHp1;
    OnePoleHp toneHp2;
    OnePoleLp postLp;
    OnePoleLp tiltLp;
    OnePoleLp subLp;
    OnePoleLp subOctLp;
    OctaveDivider subDivider;
    double subAmount = 0.0;
    double subDc = 0.0;
};

} // namespace doomgazer
