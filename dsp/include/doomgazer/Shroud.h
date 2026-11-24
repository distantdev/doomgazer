#pragma once

#include "doomgazer/Coefficients.h"

namespace doomgazer {

class Shroud
{
public:
    void reset() noexcept
    {
        hpL.reset();
        hpR.reset();
        thumpL.reset();
        thumpR.reset();
        midL.reset();
        midR.reset();
        barkL.reset();
        barkR.reset();
        cab1L.reset();
        cab1R.reset();
        cab2L.reset();
        cab2R.reset();
        coneL.reset();
        coneR.reset();
    }

    void heal() noexcept
    {
        thumpL.heal();
        thumpR.heal();
        midL.heal();
        midR.heal();
        barkL.heal();
        barkR.heal();
        cab1L.heal();
        cab1R.heal();
        cab2L.heal();
        cab2R.heal();
    }

    // wet is 0..1. The cab keeps running while wet is open so a bypass can fade out.
    Stereo process(double inL, double inR, const Parameters& params, const Coefficients& c, double wet) noexcept
    {
        if (wet <= 0.0008)
            return { inL, inR };

        thumpL.setTarget(c.thump);
        thumpR.setTarget(c.thump);
        midL.setTarget(c.mid);
        midR.setTarget(c.mid);
        barkL.setTarget(c.bark);
        barkR.setTarget(c.bark);
        cab1L.setTarget(c.cab1);
        cab1R.setTarget(c.cab1);
        cab2L.setTarget(c.cab2);
        cab2R.setTarget(c.cab2);

        const bool stereo = asOn(params.shroudStereo);
        if (!stereo)
        {
            const double m = 0.5 * (inL + inR);
            const double hpOut = hpL.process(m, c.cabHpA);
            const double th = thumpL.process(hpOut);
            const double md = midL.process(th);
            const double bk = barkL.process(md);
            const double c1 = cab1L.process(bk);
            const double c2 = cab2L.process(c1);
            const double cab = coneL.process(c2, c.coneAbsA0, c.coneAbsB1);
            const double dry = 1.0 - wet;
            const double out = m * dry + cab * wet;
            return { out, out };
        }

        const double hpOutL = hpL.process(inL, c.cabHpA);
        const double hpOutR = hpR.process(inR, c.cabHpA);

        const double thL = thumpL.process(hpOutL);
        const double thR = thumpR.process(hpOutR);
        const double mdL = midL.process(thL);
        const double mdR = midR.process(thR);
        const double bkL = barkL.process(mdL);
        const double bkR = barkR.process(mdR);
        const double c1L = cab1L.process(bkL);
        const double c1R = cab1R.process(bkR);
        const double c2L = cab2L.process(c1L);
        const double c2R = cab2R.process(c1R);
        const double cabL = coneL.process(c2L, c.coneAbsA0, c.coneAbsB1);
        const double cabR = coneR.process(c2R, c.coneAbsA0, c.coneAbsB1);
        const double dry = 1.0 - wet;
        return { inL * dry + cabL * wet, inR * dry + cabR * wet };
    }

private:
    OnePoleHp hpL;
    OnePoleHp hpR;
    SmoothedBiquad thumpL;
    SmoothedBiquad thumpR;
    SmoothedBiquad midL;
    SmoothedBiquad midR;
    SmoothedBiquad barkL;
    SmoothedBiquad barkR;
    SmoothedBiquad cab1L;
    SmoothedBiquad cab1R;
    SmoothedBiquad cab2L;
    SmoothedBiquad cab2R;
    OnePoleLp coneL;
    OnePoleLp coneR;
};

} // namespace doomgazer
