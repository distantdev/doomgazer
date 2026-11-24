#pragma once

#include "doomgazer/Coefficients.h"
#include "doomgazer/OctaveDivider.h"
#include "doomgazer/Saturation.h"

#include <cmath>

namespace doomgazer {

class DroneEngine
{
public:
    void reset() noexcept
    {
        xLp1.reset();
        xLp2.reset();
        bassLp.reset();
        stg1Lp.reset();
        stg2Lp.reset();
        tightHp.reset();
        coupleHp.reset();
        toneLp.reset();
        toneHp.reset();
        subOctLp.reset();
        subDivider.reset();
        melodyEnv = 0.0;
    }

    void resetSwitch() noexcept
    {
        xLp1.reset();
        xLp2.reset();
        bassLp.reset();
        subOctLp.reset();
        subDivider.reset();
    }

    Stereo process(double mono, double sagGain, double subGate, double subGen,
                   const Parameters& params, const Coefficients& c) noexcept
    {
        const double low1 = xLp1.process(mono, c.xoverA0, c.xoverB1);
        const double low2 = xLp2.process(low1, c.xoverA0, c.xoverB1);
        const double subBand = low2;
        const double highBand = mono - low2;

        double carve = 1.0;
        if (asOn(params.melodyCarve))
        {
            const double absHigh = std::abs(highBand);
            melodyEnv = (absHigh > melodyEnv)
                ? (0.92 * melodyEnv + 0.08 * absHigh)
                : (0.999 * melodyEnv + 0.001 * absHigh);
            carve = 1.0 - std::min(0.70, melodyEnv * 2.5 * params.carveDepth);
        }

        const double bassDriven = subBand * c.bassGain * 0.75 * sagGain;
        const double bass1 = asymClip(bassDriven, 0.22);
        const double bassFiltered = bassLp.process(bass1, c.is1A0, c.is1B1);
        const double bass2 = asymClip(bassFiltered * 1.60, 0.18);

        const double follow = c.follow18;
        subAmount += follow * (params.subOctave - subAmount);
        double oct = subDivider.process(bass2, c.sampleRate, asInt(params.lowSubShift));
        if (params.subDrive > 0.001)
        {
            const double driven = oct * (1.0 + params.subDrive * 2.8);
            oct = oct * (1.0 - params.subDrive * 0.30)
                + asymClip(driven, 0.25 * params.subDrive) * (params.subDrive * 0.70);
        }

        const double fuzzIn = highBand + subBand * 0.18;
        const double tightenedHp = tightHp.process(fuzzIn, c.tightHpA);
        const double tightMix = fuzzIn * (1.0 - params.inputTight * 0.82)
                              + tightenedHp * (params.inputTight * 0.82);

        const double stage1 = asymClip(tightMix * c.stageGain * sagGain, 0.35);
        const double stage1Lp = stg1Lp.process(stage1, c.is1A0, c.is1B1);
        const double stage2 = asymClip(stage1Lp * c.stageGain * 1.32, 0.42);
        const double stage2Lp = stg2Lp.process(stage2, c.is2A0, c.is2B1);
        const double coupled = coupleHp.process(stage2Lp, c.coupHpA);
        const double stage3 = saturate(coupled * c.stageGain * 1.55 * sagGain);
        const double tiltLp = toneLp.process(stage3, c.toneLpA0, c.toneLpB1);
        const double tiltHp = toneHp.process(stage3, c.toneHpA);
        const double toned = tiltLp * (1.0 - params.spectralTilt * 0.70)
                           + tiltHp * (0.25 + params.spectralTilt * 0.85);
        const double fuzzOut = saturate(toned * 1.55) * 0.85 * c.fuzzTrim;

        const double blend = clamp(params.spectrumBlend, 0.0, 1.0);
        const double bassScale = 0.82 * (1.0 - blend * 0.28);
        const double fuzzScale = 1.45 * (0.62 + blend * 0.38);
        const double octVoice = oct * (subAmount * 1.15) * subGate * c.bassTrim * bassScale;
        const double bassDry = (bass2 * 1.15 * carve * c.bassTrim + subGen) * bassScale;
        const double solo = clamp(params.subSolo, 0.0, 1.0);
        const double bassPart = mixSubSolo(bassDry, octVoice, solo) * c.droneMakeup;
        const double fuzzPart = fuzzOut * fuzzScale * (1.0 - solo) * c.droneMakeup;
        const double sum = bassPart + fuzzPart;
        return { sum, sum };
    }

private:
    OnePoleLp xLp1;
    OnePoleLp xLp2;
    OnePoleLp bassLp;
    OnePoleLp stg1Lp;
    OnePoleLp stg2Lp;
    OnePoleLp subOctLp;
    OctaveDivider subDivider;
    OnePoleHp tightHp;
    OnePoleHp coupleHp;
    OnePoleLp toneLp;
    OnePoleHp toneHp;
    double melodyEnv = 0.0;
    double subAmount = 0.0;
    double subDc = 0.0;
};

} // namespace doomgazer
