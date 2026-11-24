#pragma once

#include "doomgazer/Coefficients.h"
#include "doomgazer/DelayLine.h"
#include "doomgazer/Saturation.h"

namespace doomgazer {

class DelayEngine
{
public:
    void prepare(const BufferLimits& limits)
    {
        left.assign(limits.atmos);
        right.assign(limits.atmos);
        maxLen = limits.atmos;
    }

    void reset() noexcept
    {
        left.clear();
        right.clear();
        inHpL.reset();
        inHpR.reset();
        dampL.reset();
        dampR.reset();
        revDampL.reset();
        revDampR.reset();
        revPlayhead = 0;
        lastMode = -1;
        smoothedSamps = -1.0;
        slewRate = 0.0;
    }

    // Move the read head toward the target. Small edits settle in about 30 ms.
    // A long jump is limited to one sample per sample so the buffer cannot click.
    double slewRead(double target, double sampleRate) noexcept
    {
        if (sampleRate != slewRate)
        {
            slewRate = sampleRate;
            const double samples = std::max(1.0, 0.030 * sampleRate);
            slewCoeff = 1.0 - std::exp(-1.0 / samples);
        }
        if (smoothedSamps < 0.0)
            smoothedSamps = target;
        const double step = slewCoeff * (target - smoothedSamps);
        const double limited = std::max(-1.0, std::min(1.0, step));
        smoothedSamps += limited;
        return smoothedSamps;
    }

    void process(double& sigL, double& sigR, const Parameters& params, const Coefficients& c) noexcept
    {
        const int mode = asInt(params.delayMode);
        if (mode != lastMode)
        {
            inHpL.reset();
            inHpR.reset();
            lastMode = mode;
            if (mode <= 0)
            {
                left.clear();
                right.clear();
            }
        }
        if (mode <= 0)
            return;

        const bool stereo = asOn(params.delayStereo);
        double inL = inHpL.process(sigL, c.delayHpA);
        double inR = inHpR.process(sigR, c.delayHpA);
        if (!stereo)
        {
            const double m = 0.5 * (inL + inR);
            inL = m;
            inR = m;
        }
        const double feedback = clamp(params.delayFeedback, 0.0, 1.25);
        double wetL = 0.0;
        double wetR = 0.0;

        if (mode == 1)
        {
            const double read = slewRead(static_cast<double>(std::max(1, c.delaySamps)), c.sampleRate);
            const double rawL = left.readInterp(read);
            const double readR = stereo
                ? std::min(static_cast<double>(maxLen - 4), read * 1.025)
                : read;
            const double rawR = right.readInterp(readR);
            const double dampedL = dampL.process(rawL, c.delayLpA0, c.delayLpB1);
            const double dampedR = dampR.process(rawR, c.delayLpA0, c.delayLpB1);
            const double fbL = clamp(asymClip(dampedL * feedback * 1.15, 0.20), -1.20, 1.20);
            const double fbR = clamp(asymClip(dampedR * feedback * 1.15, 0.20), -1.20, 1.20);
            left.writeCursor(inL + fbL + kDenorm);
            right.writeCursor(inR + fbR + kDenorm);
            left.advance();
            right.advance();
            wetL = rawL;
            wetR = rawR;
        }
        else if (mode == 2)
        {
            const int window = std::max(1, c.delayRevWin);
            if (++revPlayhead >= window)
                revPlayhead = 0;
            int second = revPlayhead + window / 2;
            if (second >= window)
                second -= window;

            double u = static_cast<double>(revPlayhead) / (static_cast<double>(window) * 0.5);
            if (u > 1.0)
                u = 2.0 - u;
            const double w1 = u * u * (3.0 - 2.0 * u);
            const double w2 = 1.0 - w1;
            const double d1 = 2.0 * static_cast<double>(revPlayhead);
            const double d2 = 2.0 * static_cast<double>(second);

            const double aL = left.readInterp(d1);
            const double bL = left.readInterp(d2);
            const double aR = right.readInterp(d1);
            const double bR = right.readInterp(d2);
            const double revL = aL * w1 + bL * w2;
            const double revR = aR * w1 + bR * w2;
            const double dampedL = revDampL.process(revL, c.delayLpA0, c.delayLpB1);
            const double dampedR = revDampR.process(revR, c.delayLpA0, c.delayLpB1);
            const double fb = feedback;
            const double fbL = clamp(asymClip(dampedL * fb * 1.10, 0.20), -1.20, 1.20);
            const double fbR = clamp(asymClip(dampedR * fb * 1.10, 0.20), -1.20, 1.20);
            left.writeCursor(inL + fbL + kDenorm);
            right.writeCursor(inR + fbR + kDenorm);
            left.advance();
            right.advance();
            wetL = revL;
            wetR = revR;
        }

        wetL = saturate(wetL * 0.95);
        wetR = saturate(wetR * 0.95);
        if (!stereo)
        {
            const double wet = 0.5 * (wetL + wetR);
            wetL = wet;
            wetR = wet;
        }
        const double mix = clamp(params.delayMix, 0.0, 1.0);
        sigL = sigL * (1.0 - mix * 0.40) + wetL * mix;
        sigR = sigR * (1.0 - mix * 0.40) + wetR * mix;
        if (!stereo)
        {
            const double out = 0.5 * (sigL + sigR);
            duplicateMono(out, sigL, sigR);
        }
    }

private:
    RingBuffer left;
    RingBuffer right;
    OnePoleHp inHpL;
    OnePoleHp inHpR;
    OnePoleLp dampL;
    OnePoleLp dampR;
    OnePoleLp revDampL;
    OnePoleLp revDampR;
    int revPlayhead = 0;
    int lastMode = -1;
    int maxLen = 8192;
    double smoothedSamps = -1.0;
    double slewRate = 0.0;
    double slewCoeff = 1.0;
};

class ReverbEngine
{
public:
    void prepare(const BufferLimits& limits)
    {
        for (int i = 0; i < 4; ++i)
        {
            combL[i].assign(limitsFor(limits, i));
            combR[i].assign(limitsFor(limits, i));
        }
        apL[0].assign(limits.ap1);
        apR[0].assign(limits.ap1);
        apL[1].assign(limits.ap2);
        apR[1].assign(limits.ap2);
        shimL.assign(limits.shimLenL + 8);
        shimR.assign(limits.shimLenR + 8);
        grainL = limits.shimGrainL;
        grainR = limits.shimGrainR;
        lenL = limits.shimLenL;
        lenR = limits.shimLenR;
        incL = 1.0 / static_cast<double>(grainL);
        incR = 1.0 / static_cast<double>(grainR);
        shimL.setLength(lenL);
        shimR.setLength(lenR);
        const int springCap = std::max(512, static_cast<int>(std::ceil(0.090 * limits.sampleRate)) + 8);
        springL.assign(springCap);
        springR.assign(springCap);
    }

    void reset() noexcept
    {
        for (int i = 0; i < 4; ++i)
        {
            combL[i].clear();
            combR[i].clear();
            combDampL[i].reset();
            combDampR[i].reset();
        }
        for (int i = 0; i < 2; ++i)
        {
            apL[i].clear();
            apR[i].clear();
        }
        inHpL.reset();
        inHpR.reset();
        shimL.clear();
        shimR.clear();
        shimLpL = 0.0;
        shimLpR = 0.0;
        shimAmount = 0.0;
        freezeAmount = 0.0;
        springL.clear();
        springR.clear();
        springPeakL.reset();
        springPeakR.reset();
        springBellL.reset();
        springBellR.reset();
        springSr = -1.0;
        followSr = -1.0;
        phaseL = 0.0;
        phaseR = 0.25;
        writeL = 0;
        writeR = 0;
    }

    void applyLengths(const Coefficients& c) noexcept
    {
        combL[0].setLength(c.c1Len);
        combR[0].setLength(c.c1Len);
        combL[1].setLength(c.c2Len);
        combR[1].setLength(c.c2Len);
        combL[2].setLength(c.c3Len);
        combR[2].setLength(c.c3Len);
        combL[3].setLength(c.c4Len);
        combR[3].setLength(c.c4Len);
        apL[0].setLength(c.ap1Len);
        apR[0].setLength(c.ap1Len);
        apL[1].setLength(c.ap2Len);
        apR[1].setLength(c.ap2Len);
    }

    void process(double& sigL, double& sigR, const Parameters& params, const Coefficients& c) noexcept
    {
        const int mode = asInt(params.reverbMode);
        if (c.sampleRate != followSr)
        {
            followSr = c.sampleRate;
            shimFollow = 1.0 - std::exp(-1.0 / (0.025 * c.sampleRate));
            freezeFollow = 1.0 - std::exp(-1.0 / (0.040 * c.sampleRate));
        }
        shimAmount += shimFollow * ((mode == 2 ? 1.0 : 0.0) - shimAmount);
        const double freezeTarget = (mode > 0 && asOn(params.freeze)) ? 1.0 : 0.0;
        freezeAmount += freezeFollow * (freezeTarget - freezeAmount);
        const double live = 1.0 - freezeAmount;

        const bool stereo = asOn(params.reverbStereo);
        double inL = inHpL.process(sigL, c.revHpA);
        double inR = inHpR.process(sigR, c.revHpA);
        if (!stereo)
        {
            const double m = 0.5 * (inL + inR);
            inL = m;
            inR = m;
        }
        double wetL = 0.0;
        double wetR = 0.0;

        if (mode == 3)
            springVoice(inL, inR, live, wetL, wetR, params, c, stereo);
        else if (mode > 0)
        {
            double combInL = inL * live;
            double combInR = inR * live;
            if ((mode == 2 || shimAmount > 0.001) && live > 0.001)
            {
                const double pitchedL = shimmerVoice(shimL, writeL, phaseL, grainL, lenL);
                const double pitchedR = shimmerVoice(shimR, writeR, phaseR, grainR, lenR);
                shimLpL = c.shimLpA0 * pitchedL + c.shimLpB1 * shimLpL + kDenorm;
                shimLpR = c.shimLpA0 * pitchedR + c.shimLpB1 * shimLpR + kDenorm;
                const double fb = std::min(0.55, clamp(params.reverbDecay, 0.0, 0.99) * c.shimFbScale) * shimAmount * live;
                combInL = inL * live + saturate(shimLpL * fb);
                combInR = inR * live + saturate(shimLpR * fb);
                phaseL += incL;
                if (phaseL >= 1.0)
                    phaseL -= 1.0;
                phaseR += incR;
                if (phaseR >= 1.0)
                    phaseR -= 1.0;
            }

            const double liveFb = clamp(params.reverbDecay, 0.0, 0.99) * 0.96;
            const double fbScale = liveFb + (0.997 - liveFb) * freezeAmount;
            const double c1L = combStep(combL[0], combDampL[0], combInL, fbScale, c);
            const double c1R = combStep(combR[0], combDampR[0], combInR, fbScale, c);
            const double c2L = combStep(combL[1], combDampL[1], combInL, fbScale, c);
            const double c2R = combStep(combR[1], combDampR[1], combInR, fbScale, c);
            const double c3L = combStep(combL[2], combDampL[2], combInL, fbScale, c);
            const double c3R = combStep(combR[2], combDampR[2], combInR, fbScale, c);
            const double c4L = combStep(combL[3], combDampL[3], combInL, fbScale, c);
            const double c4R = combStep(combR[3], combDampR[3], combInR, fbScale, c);

            const double sumL = (c1L - c2L + c3L - c4L) * 0.35;
            const double sumR = (c1R + c2R - c3R - c4R) * 0.35;
            const double ap1L = allpassStep(apL[0], sumL);
            const double ap1R = allpassStep(apR[0], sumR);
            const double ap2L = allpassStep(apL[1], ap1L);
            const double ap2R = allpassStep(apR[1], ap1R);

            if ((mode == 2 || shimAmount > 0.001) && live > 0.02)
            {
                shimL.writeWrapped(writeL, ap2L);
                shimR.writeWrapped(writeR, ap2R);
                if (++writeL >= lenL)
                    writeL = 0;
                if (++writeR >= lenR)
                    writeR = 0;
            }
            wetL = ap2L * (1.0 - 0.15 * shimAmount) + shimLpL * c.shimBlend * shimAmount;
            wetR = ap2R * (1.0 - 0.15 * shimAmount) + shimLpR * c.shimBlend * shimAmount;
        }

        if (!stereo && mode > 0)
        {
            const double wet = 0.5 * (wetL + wetR);
            wetL = wet;
            wetR = wet;
        }

        double mix = clamp(params.reverbMix, 0.0, 1.0);
        double dryKeep = 0.40;
        if (mode == 2)
            mix = mix * (0.35 + 0.65 * mix);
        else if (mode == 3)
        {
            mix = mix * 0.55;
            dryKeep = 0.18;
        }
        sigL = sigL * (1.0 - mix * dryKeep) + wetL * mix;
        sigR = sigR * (1.0 - mix * dryKeep) + wetR * mix;
        if (!stereo)
        {
            const double out = 0.5 * (sigL + sigR);
            duplicateMono(out, sigL, sigR);
        }
    }

    void springVoice(double inL, double inR, double live, double& wetL, double& wetR,
                     const Parameters& params, const Coefficients& c, bool stereo) noexcept
    {
        const double sampleRate = std::max(8000.0, c.sampleRate);
        if (std::abs(sampleRate - springSr) > 0.5)
        {
            springSr = sampleRate;
            const double nyquist = sampleRate * 0.42;
            const BiquadCoeffs peak = makePeaking(std::min(1750.0, nyquist), 5.0, 6.5, sampleRate);
            const BiquadCoeffs bell = makePeaking(std::min(3200.0, nyquist), 6.5, 5.5, sampleRate);
            springPeak = peak;
            springBell = bell;
        }

        const double scale = clamp(params.reverbSizeMs / 900.0, 0.55, 1.05);
        const double tapsMs[4] = { 29.0, 37.0, 47.0, 61.0 };
        const double weights[4] = { 0.55, 0.36, 0.28, 0.16 };
        double bodyL = 0.0;
        double bodyR = 0.0;
        for (int tap = 0; tap < 4; ++tap)
        {
            const double delay = tapsMs[tap] * 0.001 * sampleRate * scale;
            bodyL += springL.readInterp(delay) * weights[tap];
            const double delayR = stereo ? std::min(delay * 1.06, delay + 48.0) : delay;
            bodyR += springR.readInterp(delayR) * weights[tap];
        }
        const double metalL = springPeakL.process(bodyL, springPeak) * 0.55
                            + springBellL.process(bodyL, springBell) * 0.40;
        const double metalR = springPeakR.process(bodyR, springPeak) * 0.55
                            + springBellR.process(bodyR, springBell) * 0.40;
        wetL = bodyL * 0.70 + metalL;
        wetR = bodyR * 0.70 + metalR;

        const double fbLive = clamp(params.reverbDecay, 0.0, 0.99) * 0.30;
        const double fb = fbLive + (0.72 - fbLive) * (1.0 - live);
        const double fbL = clamp(saturate(wetL * fb * 1.35), -0.85, 0.85);
        const double fbR = clamp(saturate(wetR * fb * 1.35), -0.85, 0.85);
        springL.writeCursor(inL * live + fbL + kDenorm);
        springR.writeCursor(inR * live + fbR + kDenorm);
        springL.advance();
        springR.advance();
    }

private:
    static int limitsFor(const BufferLimits& limits, int index) noexcept
    {
        switch (index)
        {
            case 0: return limits.c1;
            case 1: return limits.c2;
            case 2: return limits.c3;
            case 3: return limits.c4;
            default: return limits.c1;
        }
    }

    static double combStep(RingBuffer& buffer, OnePoleLp& damp, double input, double fb, const Coefficients& c) noexcept
    {
        const double tapped = buffer.atCursor();
        const double damped = damp.process(tapped, c.revLpA0, c.revLpB1);
        buffer.writeCursor(input + saturate(damped * fb));
        buffer.advance();
        return tapped;
    }

    static double allpassStep(RingBuffer& buffer, double input) noexcept
    {
        const double buffered = buffer.atCursor();
        const double entered = input + buffered * 0.50;
        buffer.writeCursor(entered);
        buffer.advance();
        return buffered - entered * 0.50;
    }

    static double shimmerVoice(const RingBuffer& buffer, int write, double phase, int grain, int length) noexcept
    {
        const double first = shimmerTap(buffer, write, phase, grain, length);
        double phase2 = phase + 0.5;
        if (phase2 >= 1.0)
            phase2 -= 1.0;
        const double second = shimmerTap(buffer, write, phase2, grain, length);
        const double w1 = 0.5 - 0.5 * std::cos(2.0 * kPi * phase);
        return first * w1 + second * (1.0 - w1);
    }

    static double shimmerTap(const RingBuffer& buffer, int write, double phase, int grain, int length) noexcept
    {
        const double delay = static_cast<double>(grain) * 2.0 - phase * static_cast<double>(grain);
        double read = static_cast<double>(write) - delay;
        const double len = static_cast<double>(length);
        while (read < 0.0)
            read += len;
        while (read >= len)
            read -= len;
        return buffer.readWrapped(read);
    }

    RingBuffer combL[4];
    RingBuffer combR[4];
    OnePoleLp combDampL[4];
    OnePoleLp combDampR[4];
    RingBuffer apL[2];
    RingBuffer apR[2];
    OnePoleHp inHpL;
    OnePoleHp inHpR;
    RingBuffer shimL;
    RingBuffer shimR;
    double shimLpL = 0.0;
    double shimLpR = 0.0;
    double shimAmount = 0.0;
    double freezeAmount = 0.0;
    RingBuffer springL;
    RingBuffer springR;
    Biquad springPeakL;
    Biquad springPeakR;
    Biquad springBellL;
    Biquad springBellR;
    BiquadCoeffs springPeak;
    BiquadCoeffs springBell;
    double springSr = -1.0;
    double followSr = -1.0;
    double shimFollow = 0.0;
    double freezeFollow = 0.0;
    double phaseL = 0.0;
    double phaseR = 0.25;
    double incL = 0.001;
    double incR = 0.001;
    int writeL = 0;
    int writeR = 0;
    int grainL = 1024;
    int grainR = 1024;
    int lenL = 4096;
    int lenR = 4096;
};

} // namespace doomgazer
