#pragma once

#include "doomgazer/Coefficients.h"
#include "doomgazer/DelayLine.h"

namespace doomgazer {

struct GlideFrame
{
    double l = 0.0;
    double r = 0.0;
    double mono = 0.0;
};

class GlideWash
{
public:
    void prepare(const BufferLimits& limits)
    {
        left.assign(limits.glide);
        right.assign(limits.glide);
    }

    void reset() noexcept
    {
        left.clear();
        right.clear();
    }

    GlideFrame process(double inL, double inR, const Parameters& params, const Coefficients& c) noexcept
    {
        GlideFrame frame;
        const int mode = asInt(params.glideMode);
        if (mode <= 0 || params.glideMix <= 0.001)
        {
            // Keep the ring current while bypassed so switching the wash back
            // on does not replay audio captured before it was switched off.
            left.writeCursor(0.0);
            right.writeCursor(0.0);
            left.advance();
            right.advance();
            return frame;
        }

        left.writeCursor(inL);
        right.writeCursor(inR);

        const int taps[8] = {
            c.glideTap[0], c.glideTap[1], c.glideTap[2], c.glideTap[3],
            c.glideTap[4], c.glideTap[5], c.glideTap[6], c.glideTap[7],
        };
        const double sL[8] = {
            left.readSamplesBack(taps[0]), left.readSamplesBack(taps[1]),
            left.readSamplesBack(taps[2]), left.readSamplesBack(taps[3]),
            left.readSamplesBack(taps[4]), left.readSamplesBack(taps[5]),
            left.readSamplesBack(taps[6]), left.readSamplesBack(taps[7]),
        };
        const double sR[8] = {
            right.readSamplesBack(taps[0]), right.readSamplesBack(taps[1]),
            right.readSamplesBack(taps[2]), right.readSamplesBack(taps[3]),
            right.readSamplesBack(taps[4]), right.readSamplesBack(taps[5]),
            right.readSamplesBack(taps[6]), right.readSamplesBack(taps[7]),
        };

        if (mode == 1)
        {
            frame.l = (sL[0] * 0.90 + sL[2] * 0.75 + sL[4] * 0.60 + sL[6] * 0.40) * 0.38
                    + (sR[1] * 0.25 + sR[5] * 0.15);
            frame.r = (sR[1] * 0.85 + sR[3] * 0.70 + sR[5] * 0.55 + sR[7] * 0.35) * 0.38
                    + (sL[0] * 0.25 + sL[4] * 0.15);
        }
        else
        {
            frame.l = (sL[0] * 0.18 + sL[2] * 0.36 + sL[4] * 0.64 + sL[6] * 0.95) * 0.36
                    + (sR[1] * 0.15 + sR[5] * 0.30);
            frame.r = (sR[1] * 0.25 + sR[3] * 0.48 + sR[5] * 0.78 + sR[7] * 1.00) * 0.36
                    + (sL[0] * 0.15 + sL[4] * 0.30);
        }

        left.advance();
        right.advance();
        frame.mono = (frame.l + frame.r) * 0.5;
        if (!asOn(params.glideStereo))
        {
            frame.l = frame.mono;
            frame.r = frame.mono;
        }
        return frame;
    }

private:
    RingBuffer left;
    RingBuffer right;
};

} // namespace doomgazer
