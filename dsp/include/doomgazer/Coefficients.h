#pragma once

#include "doomgazer/BiquadFilter.h"
#include "doomgazer/FirstOrderFilters.h"
#include "doomgazer/Parameters.h"

namespace doomgazer {

struct Coefficients
{
    double sampleRate = 48000.0;

    double cleanGain = 1.0;
    double cleanMakeup = 1.0;
    double cleanTrim = 1.0;
    double clToneHpA = 0.5;
    double clTiltB1 = 0.5;
    double clTiltA0 = 0.5;
    double clTiltLo = 1.0;
    double clTiltHi = 1.0;

    double bassGain = 1.0;
    double bassTrim = 1.0;
    double stageGain = 1.0;
    double fuzzTrim = 1.0;
    double masterGain = 1.0;
    double inputTrim = 1.0;

    double inHpA = 0.5;
    double tightHpA = 0.5;
    double xoverB1 = 0.5;
    double xoverA0 = 0.5;
    double subOctLpB1 = 0.5;
    double subOctLpA0 = 0.5;
    double is1B1 = 0.5;
    double is1A0 = 0.5;
    double is2B1 = 0.5;
    double is2A0 = 0.5;
    double coupHpA = 0.5;
    double toneLpB1 = 0.5;
    double toneLpA0 = 0.5;
    double toneHpA = 0.5;

    double compAtt = 0.0;
    double compRelFast = 0.0;
    double compRelSlow = 0.0;
    double sagAtt = 0.0;
    double sagRel = 0.0;
    double subGateAtt = 0.0;
    double subGateRel = 0.0;
    double limAtt = 0.0;
    double limRel = 0.0;
    double fadeStep = 1.0 / 64.0;
    double muteStep = 1.0 / 240.0;
    // 48 kHz seeds. update() replaces them for the real sample rate.
    double follow18 = 0.0011567378697882;
    double follow12 = 0.0017346049419678;
    double gateOpenPole = 0.9982653950580322;

    double droneMakeup = 1.0;
    double twinGain = 1.0;

    double cabHpA = 0.5;
    BiquadCoeffs thump;
    BiquadCoeffs mid;
    BiquadCoeffs bark;
    BiquadCoeffs cab1;
    BiquadCoeffs cab2;
    double coneAbsB1 = 0.5;
    double coneAbsA0 = 0.5;

    double delayHpA = 0.5;
    double delayLpB1 = 0.5;
    double delayLpA0 = 0.5;
    int delaySamps = 1000;
    int delayRevWin = 512;

    double revHpA = 0.5;
    double revLpB1 = 0.5;
    double revLpA0 = 0.5;
    double shimLpB1 = 0.5;
    double shimLpA0 = 0.5;
    double shimBlend = 0.3;
    double shimFbScale = 0.32;
    int c1Len = 32;
    int c2Len = 32;
    int c3Len = 32;
    int c4Len = 32;
    int ap1Len = 16;
    int ap2Len = 16;

    double tremLfoStep = 0.0;
    double tremXoverA = 0.1;

    BiquadCoeffs subTone;
    BiquadCoeffs subPre;
    BiquadCoeffs tunerLp;
    double tunerHpA = 0.5;
    int tunerDecim = 4;
    double tunerSampleRate = 12000.0;

    int glideTap[8] = { 100, 200, 300, 400, 500, 600, 700, 800 };

    void update(const Parameters& input, const BufferLimits& limits) noexcept;
};

inline int scaledLength(int maxLen, double norm, double floorScale, int minLen) noexcept
{
    const int raw = static_cast<int>(std::floor(static_cast<double>(maxLen) * (floorScale + (1.0 - floorScale) * norm)));
    return std::max(minLen, std::min(maxLen - 4, raw));
}

// Nearest note that fits the delay buffer. No tempo leaves the knob's milliseconds.
inline double snapDelayMs(double delayMs, double bpm, int maxDelaySamps, double sampleRate) noexcept
{
    if (!(bpm >= 20.0) || sampleRate < 8000.0)
        return delayMs;
    const double quarterMs = 60000.0 / bpm;
    const double ratios[] = { 2.0, 1.5, 1.0, 0.75, 0.5, 0.25 };
    const double fitted = (static_cast<double>(std::max(1, maxDelaySamps)) / sampleRate) * 1000.0;
    const double maxMs = std::min(1200.0, fitted);
    double best = delayMs;
    double bestDist = 1.0e12;
    bool found = false;
    for (double ratio : ratios)
    {
        const double ms = quarterMs * ratio;
        if (ms < 60.0 || ms > maxMs)
            continue;
        const double dist = std::abs(ms - delayMs);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = ms;
            found = true;
        }
    }
    return found ? best : delayMs;
}

// Nearest of half, one, two, three, or four cycles per beat.
inline double snapTremoloHz(double tremHz, double bpm) noexcept
{
    if (!(bpm >= 20.0))
        return tremHz;
    const double beatHz = bpm / 60.0;
    const double cycles[] = { 0.5, 1.0, 2.0, 3.0, 4.0 };
    double best = tremHz;
    double bestDist = 1.0e12;
    for (double cycle : cycles)
    {
        const double hz = beatHz * cycle;
        if (hz < 0.5 || hz > 15.0)
            continue;
        const double dist = std::abs(hz - tremHz);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = hz;
        }
    }
    return best;
}

// Cabinet filters shared by the main contour and the doubler's second contour.
inline void fillContour(Coefficients& c, double hpHz, double bodyHz, double thumpDb,
                        double midDb, double presenceHz, double barkDb, double highCutHz,
                        double sampleRate) noexcept
{
    const double sr = std::max(8000.0, sampleRate);
    const double cabRes = clamp(bodyHz, 60.0, 300.0);
    const double coneHz = clamp(presenceHz, 500.0, 3000.0);
    const double cabLpf = clamp(highCutHz, 800.0, sr * 0.45);
    c.cabHpA = rcHpCoeff(clamp(hpHz, 20.0, 400.0), sr);
    c.thump = makePeaking(cabRes, 1.50, thumpDb, sr);
    c.mid = makePeaking(550.0, 1.1, midDb, sr);
    c.bark = makePeaking(coneHz, 1.3, barkDb, sr);

    const double effCab = std::min(sr * 0.42, cabLpf);
    const double w0 = 2.0 * kPi * effCab / sr;
    const double cosw = std::cos(w0);
    const double sinw = std::sin(w0);
    const double al1 = sinw / (2.0 * 0.54119610);
    const double a0Inv1 = 1.0 / (1.0 + al1);
    c.cab1.b1 = (1.0 - cosw) * a0Inv1;
    c.cab1.b0 = c.cab1.b1 * 0.5;
    c.cab1.b2 = c.cab1.b0;
    c.cab1.a1 = (-2.0 * cosw) * a0Inv1;
    c.cab1.a2 = (1.0 - al1) * a0Inv1;

    const double al2 = sinw / (2.0 * 1.30656296);
    const double a0Inv2 = 1.0 / (1.0 + al2);
    c.cab2.b1 = (1.0 - cosw) * a0Inv2;
    c.cab2.b0 = c.cab2.b1 * 0.5;
    c.cab2.b2 = c.cab2.b0;
    c.cab2.a1 = (-2.0 * cosw) * a0Inv2;
    c.cab2.a2 = (1.0 - al2) * a0Inv2;

    const double coneFc = std::min(sr * 0.44, effCab * 1.25);
    c.coneAbsB1 = rcLpPole(coneFc, sr);
    c.coneAbsA0 = 1.0 - c.coneAbsB1;
}

inline void Coefficients::update(const Parameters& input, const BufferLimits& limits) noexcept
{
    const double sr = std::max(8000.0, limits.sampleRate);
    sampleRate = sr;
    Parameters p = input;
    p.clampIntoRange();

    cleanGain = std::pow(10.0, (p.coreGainDb - 6.0) / 20.0);
    cleanMakeup = 1.30 / std::pow(cleanGain, 0.32);
    cleanTrim = dbToGain(p.coreLevelDb);
    const double tone = clamp(p.coreTone, 0.0, 1.0);
    clToneHpA = rcHpCoeff(50.0 * std::pow(6.4, tone), sr);
    clTiltB1 = rcLpPole(700.0, sr);
    clTiltA0 = 1.0 - clTiltB1;
    clTiltLo = 1.15 - tone * 0.65;
    clTiltHi = 0.80 + tone * 0.55;

    bassGain = dbToGain(p.lowGainDb);
    bassTrim = 1.0;
    stageGain = std::pow(10.0, (p.harmonicGainDb / 3.0) / 20.0);
    fuzzTrim = 1.0;
    // The wall was turned up against the bass stack. This trim puts a
    // midrange note at the dual defaults next to the harmonic core.
    droneMakeup = 0.60;
    twinGain = dbToGain(p.twinLevelDb);
    masterGain = dbToGain(p.masterDb);
    inputTrim = dbToGain(p.inputTrimDb);

    const double interHz = clamp(p.antiFizzHz, 800.0, sr * 0.45);
    const double fuzzHpf = clamp(p.preDriveHz, 30.0, 300.0);
    const double xoverHz = clamp(p.splitHz, 80.0, 400.0);
    const double cabRes = clamp(p.shroudHz, 60.0, 300.0);
    const double coneHz = clamp(p.presenceHz, 500.0, 3000.0);
    const double cabLpf = clamp(p.highCutHz, 800.0, sr * 0.45);

    inHpA = rcHpCoeff(30.0, sr);
    tightHpA = rcHpCoeff(fuzzHpf, sr);
    xoverB1 = rcLpPole(xoverHz, sr);
    xoverA0 = 1.0 - xoverB1;
    subOctLpB1 = rcLpPole(85.0, sr);
    subOctLpA0 = 1.0 - subOctLpB1;
    is1B1 = rcLpPole(interHz, sr);
    is1A0 = 1.0 - is1B1;
    is2B1 = rcLpPole(interHz * 0.85, sr);
    is2A0 = 1.0 - is2B1;
    coupHpA = rcHpCoeff(45.0, sr);

    const double tilt = clamp(p.spectralTilt, 0.0, 1.0);
    toneLpB1 = rcLpPole(420.0 + tilt * 400.0, sr);
    toneLpA0 = 1.0 - toneLpB1;
    toneHpA = rcHpCoeff(950.0 - tilt * 350.0, sr);

    compAtt = std::exp(-1.0 / (0.008 * sr));
    compRelFast = std::exp(-1.0 / (0.080 * sr));
    compRelSlow = std::exp(-1.0 / (1.500 * sr));
    sagAtt = std::exp(-1.0 / (0.004 * sr));
    sagRel = std::exp(-1.0 / (0.160 * sr));
    subGateAtt = std::exp(-1.0 / (0.020 * sr));
    subGateRel = std::exp(-1.0 / (0.400 * sr));
    limAtt = std::exp(-1.0 / (0.0025 * sr));
    limRel = std::exp(-1.0 / (0.050 * sr));
    fadeStep = 1.0 / std::max(64.0, std::floor(0.006 * sr));
    muteStep = 1.0 / std::max(1.0, std::floor(0.005 * sr));
    follow18 = 1.0 - std::exp(-1.0 / (0.018 * sr));
    gateOpenPole = std::exp(-1.0 / (0.012 * sr));
    follow12 = 1.0 - gateOpenPole;

    fillContour(*this, 28.0, cabRes, p.thumpDb, p.midDb, coneHz, 2.5, cabLpf, sr);

    const double delayMs = clamp(p.delayMs, 60.0, 1200.0);
    const double delayDamp = clamp(p.delayDampHz, 800.0, sr * 0.45);
    delayHpA = rcHpCoeff(140.0, sr);
    delayLpB1 = rcLpPole(delayDamp, sr);
    delayLpA0 = 1.0 - delayLpB1;
    delaySamps = std::max(1, std::min(limits.atmos - 4, static_cast<int>(std::floor(delayMs * 0.001 * sr))));
    const int halfWindow = static_cast<int>(std::floor((limits.atmos - 32) * 0.5));
    delayRevWin = std::max(512, std::min(halfWindow, delaySamps));

    const double revMs = clamp(p.reverbSizeMs, 80.0, 2500.0);
    const double revDamp = clamp(p.reverbDampHz, 800.0, sr * 0.45);
    revHpA = rcHpCoeff(120.0, sr);
    revLpB1 = rcLpPole(revDamp, sr);
    revLpA0 = 1.0 - revLpB1;
    const double shimNorm = clamp((revDamp - 800.0) / 7200.0, 0.0, 1.0);
    const double shimFc = clamp(1600.0 + shimNorm * 4800.0, 1200.0, sr * 0.45);
    shimLpB1 = rcLpPole(shimFc, sr);
    shimLpA0 = 1.0 - shimLpB1;
    shimBlend = 0.08 + 0.57 * std::pow(shimNorm, 0.90);
    shimFbScale = 0.22 + 0.30 * shimNorm;

    const double sizeNorm = (revMs - 80.0) / (2500.0 - 80.0);
    c1Len = scaledLength(limits.c1, sizeNorm, 0.25, 32);
    c2Len = scaledLength(limits.c2, sizeNorm, 0.25, 32);
    c3Len = scaledLength(limits.c3, sizeNorm, 0.25, 32);
    c4Len = scaledLength(limits.c4, sizeNorm, 0.25, 32);
    ap1Len = scaledLength(limits.ap1, sizeNorm, 0.30, 16);
    ap2Len = scaledLength(limits.ap2, sizeNorm, 0.30, 16);

    const double tremHz = clamp(p.tremoloHz, 0.5, 15.0);
    tremLfoStep = (2.0 * kPi * tremHz) / sr;
    tremXoverA = 1.0 - std::exp(-2.0 * kPi * 450.0 / sr);

    const double subHz = clamp(p.subOctToneHz, 50.0, 400.0);
    subTone = makeLowpass(subHz, 0.7071, sr);
    subPre = makeLowpass(180.0, 0.7071, sr);

    tunerDecim = sr >= 80000.0 ? 8 : 4;
    tunerSampleRate = sr / static_cast<double>(tunerDecim);
    tunerHpA = rcHpCoeff(20.0, sr);
    // Open high strings in A standard, C standard, and drop A# sit between
    // A3 and C4. A cutoff on that C removes the partials a plain string uses
    // to speak. 1 kHz keeps them, and the low path still settles fundamentals
    // under 120 Hz.
    tunerLp = makeLowpass(1000.0, 0.7071, sr);

    const double glideRatios[8] = { 0.023, 0.043, 0.071, 0.109, 0.157, 0.211, 0.277, 0.349 };
    for (int i = 0; i < 8; ++i)
    {
        const int tap = static_cast<int>(std::floor(glideRatios[i] * sr));
        glideTap[i] = std::max(1, std::min(limits.glide - 2, tap));
    }
}

} // namespace doomgazer
