#include "doomgazer/Processor.h"

#include "doomgazer/Saturation.h"

namespace doomgazer {

Processor::Processor()
{
    prepare(48000.0, 512);
}

void Processor::prepare(double sampleRate, int maxBlockSize)
{
    (void) maxBlockSize;
    const double sr = std::max(8000.0, sampleRate);
    limits = makeBufferLimits(sr);
    setSmootherTimes();
    glide.prepare(limits);
    chorus.prepare(limits);
    delay.prepare(limits);
    reverb.prepare(limits);
    updateCoefficients();
    reverb.applyLengths(coeffs);
    snapSmoothers();
}

void Processor::reset()
{
    inputHp.reset();
    subGate.reset();
    leveler.reset();
    sag.reset();
    glide.reset();
    subOctave.reset();
    harmonic.reset();
    drone.reset();
    delay.reset();
    reverb.reset();
    shroud.reset();
    doublerShroud.reset();
    tremolo.reset();
    limiter.reset();
    pitch.reset();
    grit.reset();
    pedal.reset();
    swell.reset();
    howl.reset();
    phase.reset();
    chorus.reset();
    meter.reset();
    lastEngine = -1;
    fadeState = 0;
    fadeGain = 1.0;
    muteGain = 1.0;
    updateCoefficients();
    reverb.applyLengths(coeffs);
    snapSmoothers();
}

void Processor::setSmootherTimes() noexcept
{
    const double coeff = OnePoleChase::coeffFor(0.008, limits.sampleRate);
    cleanGainChase.setCoeff(coeff);
    cleanMakeupChase.setCoeff(coeff);
    cleanTrimChase.setCoeff(coeff);
    bassGainChase.setCoeff(coeff);
    bassTrimChase.setCoeff(coeff);
    stageGainChase.setCoeff(coeff);
    fuzzTrimChase.setCoeff(coeff);
    masterGainChase.setCoeff(coeff);
    inputTrimChase.setCoeff(coeff);
    delayMixChase.setCoeff(coeff);
    reverbMixChase.setCoeff(coeff);
    tremoloMixChase.setCoeff(coeff);
    glideMixChase.setCoeff(coeff);
    sustainChase.setCoeff(coeff);
    shroudWetChase.setCoeff(coeff);
    doublerWetChase.setCoeff(coeff);
    twinGainChase.setCoeff(coeff);
}

void Processor::publishRenderedCoeffs() noexcept
{
    rendered = coeffs;
    rendered.cleanGain = cleanGainChase.value;
    rendered.cleanMakeup = cleanMakeupChase.value;
    rendered.cleanTrim = cleanTrimChase.value;
    rendered.bassGain = bassGainChase.value;
    rendered.bassTrim = bassTrimChase.value;
    rendered.stageGain = stageGainChase.value;
    rendered.fuzzTrim = fuzzTrimChase.value;
    rendered.masterGain = masterGainChase.value;
    rendered.inputTrim = inputTrimChase.value;
    rendered.twinGain = twinGainChase.value;
}

void Processor::snapSmoothers() noexcept
{
    cleanGainChase.snap(coeffs.cleanGain);
    cleanMakeupChase.snap(coeffs.cleanMakeup);
    cleanTrimChase.snap(coeffs.cleanTrim);
    bassGainChase.snap(coeffs.bassGain);
    bassTrimChase.snap(coeffs.bassTrim);
    stageGainChase.snap(coeffs.stageGain);
    fuzzTrimChase.snap(coeffs.fuzzTrim);
    masterGainChase.snap(coeffs.masterGain);
    inputTrimChase.snap(coeffs.inputTrim);

    delayModeHeld = std::max(0, asInt(params.delayMode));
    reverbModeHeld = std::max(0, asInt(params.reverbMode));
    glideModeHeld = std::max(0, asInt(params.glideMode));
    delayMixChase.snap(delayModeHeld > 0 ? params.delayMix : 0.0);
    reverbMixChase.snap(reverbModeHeld > 0 ? params.reverbMix : 0.0);
    tremoloMixChase.snap(asOn(params.tremoloOn) ? params.tremoloMix : 0.0);
    glideMixChase.snap(glideModeHeld > 0 ? params.glideMix : 0.0);
    sustainChase.snap(asOn(params.sustainOn) ? 1.0 : 0.0);
    shroudWetChase.snap(asOn(params.shroudOn) ? 1.0 : 0.0);
    doublerWetChase.snap(asOn(params.doublerOn) ? 1.0 : 0.0);
    twinGainChase.snap(coeffs.twinGain);
    shroudWet = shroudWetChase.value;
    doublerWet = doublerWetChase.value;
    doublerSpreadState = paMono ? 0.0 : clamp(params.doublerSpread, 0.0, 1.0);
    doublerLevelState = clamp(params.doublerLevel, 0.0, 1.0);
    publishRenderedCoeffs();
}

void Processor::chaseGains() noexcept
{
    rendered.cleanGain = cleanGainChase.next(coeffs.cleanGain);
    rendered.cleanMakeup = cleanMakeupChase.next(coeffs.cleanMakeup);
    rendered.cleanTrim = cleanTrimChase.next(coeffs.cleanTrim);
    rendered.bassGain = bassGainChase.next(coeffs.bassGain);
    rendered.bassTrim = bassTrimChase.next(coeffs.bassTrim);
    rendered.stageGain = stageGainChase.next(coeffs.stageGain);
    rendered.fuzzTrim = fuzzTrimChase.next(coeffs.fuzzTrim);
    rendered.masterGain = masterGainChase.next(coeffs.masterGain);
    rendered.inputTrim = inputTrimChase.next(coeffs.inputTrim);
    rendered.twinGain = twinGainChase.next(coeffs.twinGain);
}

void Processor::beginBlockParameters() noexcept
{
    blockPrePeak = 0.0;
    blockPostPeak = 0.0;
    heard = params;
    if (asOn(heard.sync))
    {
        heard.delayMs = snapDelayMs(heard.delayMs, hostBpm, limits.atmos - 4, limits.sampleRate);
        heard.tremoloHz = snapTremoloHz(heard.tremoloHz, hostBpm);
    }
    resolveStereoPolicy(heard, paMono);
}

void Processor::updateHeldParameters() noexcept
{
    const double delayMixTarget = params.delayMix;
    const double reverbMixTarget = params.reverbMix;

    const bool delayOn = asInt(params.delayMode) > 0;
    if (delayOn)
        delayModeHeld = asInt(params.delayMode);
    heard.delayMix = delayMixChase.next(delayOn ? delayMixTarget : 0.0);
    const bool delayKeep = delayOn || heard.delayMix > 0.001;
    heard.delayMode = delayKeep ? static_cast<double>(std::max(delayModeHeld, 1)) : 0.0;

    const bool reverbOn = asInt(params.reverbMode) > 0;
    if (reverbOn)
        reverbModeHeld = asInt(params.reverbMode);
    heard.reverbMix = reverbMixChase.next(reverbOn ? reverbMixTarget : 0.0);
    const bool reverbKeep = reverbOn || heard.reverbMix > 0.001;
    heard.reverbMode = reverbKeep ? static_cast<double>(std::max(reverbModeHeld, 1)) : 0.0;

    const bool tremOn = asOn(params.tremoloOn);
    heard.tremoloMix = tremoloMixChase.next(tremOn ? params.tremoloMix : 0.0);
    heard.tremoloOn = (tremOn || heard.tremoloMix > 0.001) ? 1.0 : 0.0;

    const bool glideOn = asInt(params.glideMode) > 0;
    if (glideOn)
        glideModeHeld = asInt(params.glideMode);
    heard.glideMix = glideMixChase.next(glideOn ? params.glideMix : 0.0);
    const bool glideKeep = glideOn || heard.glideMix > 0.001;
    heard.glideMode = (glideKeep && glideModeHeld > 0) ? static_cast<double>(glideModeHeld) : 0.0;

    heard.sustainOn = sustainChase.next(asOn(params.sustainOn) ? 1.0 : 0.0);
    shroudWet = shroudWetChase.next(asOn(params.shroudOn) ? 1.0 : 0.0);
    doublerWet = doublerWetChase.next(asOn(params.doublerOn) ? 1.0 : 0.0);
}

void Processor::setPaMonoPolicy(bool mono, bool invertR) noexcept
{
    paMono = mono;
    paInvertR = invertR;
}

void Processor::updateCoefficients()
{
    params.clampIntoRange();
    Parameters shaped = params;
    if (asOn(shaped.sync))
    {
        shaped.delayMs = snapDelayMs(shaped.delayMs, hostBpm, limits.atmos - 4, limits.sampleRate);
        shaped.tremoloHz = snapTremoloHz(shaped.tremoloHz, hostBpm);
    }
    shaped.clampIntoRange();
    coeffs.update(shaped, limits);
    fillContour(doublerCoeffs, 28.0, shaped.doublerBodyHz, shaped.doublerThumpDb, shaped.doublerMidDb,
                shaped.doublerPresenceHz, 2.5, shaped.doublerHighCutHz, limits.sampleRate);
    reverb.applyLengths(coeffs);
    publishRenderedCoeffs();
}

void Processor::setParameters(const Parameters& next)
{
    params = next;
    updateCoefficients();
}

void Processor::setHostTempo(double bpm) noexcept
{
    if (!std::isfinite(bpm) || bpm < 20.0)
        bpm = 0.0;
    if (bpm == hostBpm)
        return;
    hostBpm = bpm;
    updateCoefficients();
}

void Processor::requestPreset(const Parameters& next) noexcept
{
    while (presetWriteBusy.test_and_set(std::memory_order_acquire))
    {
    }

    const int current = publishedPreset.load(std::memory_order_relaxed);
    const int slot = current == 0 ? 1 : 0;
    presetMailboxes[static_cast<std::size_t>(slot)] = next;
    publishedPreset.store(slot, std::memory_order_release);
    presetWriteBusy.clear(std::memory_order_release);
}

bool Processor::consumePresetApplied() noexcept
{
    return presetApplied.exchange(false, std::memory_order_acq_rel);
}

void Processor::installStagedPreset() noexcept
{
    params = staged;
    updateCoefficients();
    inputHp.reset();
    subGate.reset();
    leveler.reset();
    sag.reset();
    glide.reset();
    subOctave.reset();
    harmonic.reset();
    drone.reset();
    delay.reset();
    reverb.reset();
    shroud.reset();
    doublerShroud.reset();
    tremolo.reset();
    limiter.reset();
    grit.reset();
    pedal.reset();
    swell.reset();
    howl.reset();
    phase.reset();
    chorus.reset();
    lastEngine = asOn(params.engineMode) ? 1 : 0;
    snapSmoothers();
    stagedValid = false;
    presetApplied.store(true, std::memory_order_release);
}

void Processor::pollTuner() noexcept
{
    if (pitch.envelope() > 0.003)
        pitch.analyse();
    else
        pitch.noteSilence();
}

int Processor::scopeWriteIndex() const noexcept
{
    return meter.outputWriteIndex();
}

int Processor::inputScopeWriteIndex() const noexcept
{
    return meter.inputWriteIndex();
}

void Processor::runAtmosphere(double& left, double& right, const Parameters& settings,
                              bool preShroud) noexcept
{
    // DelayEngine clears its rings when it is off, so it is still called at
    // its routing point; that keeps a bypassed echo from replaying old audio.
    const bool delayHere = (asInt(settings.delayRouting) == 0) == preShroud;
    const bool reverbHere = asInt(settings.reverbMode) > 0
        && (asInt(settings.reverbRouting) == 0) == preShroud;
    const bool delayFirst = !asOn(settings.fxOrder);

    if (delayFirst)
    {
        if (delayHere)
            delay.process(left, right, settings, rendered);
        if (reverbHere)
            reverb.process(left, right, settings, rendered);
    }
    else
    {
        if (reverbHere)
            reverb.process(left, right, settings, rendered);
        if (delayHere)
            delay.process(left, right, settings, rendered);
    }
}

void Processor::processSample(double& left, double& right) noexcept
{
    if (!std::isfinite(left))
        left = 0.0;
    if (!std::isfinite(right))
        right = 0.0;

    int published = publishedPreset.load(std::memory_order_acquire);
    if (published >= 0)
    {
        staged = presetMailboxes[static_cast<std::size_t>(published)];
        stagedValid = true;
        if (fadeState != 1)
            fadeState = 1;
        publishedPreset.compare_exchange_strong(published, -1, std::memory_order_acq_rel);
    }

    updateHeldParameters();
    chaseGains();

    const int engine = asOn(params.engineMode) ? 1 : 0;
    if (engine != lastEngine)
    {
        harmonic.resetSwitch();
        drone.resetSwitch();
        lastEngine = engine;
    }

    blockPrePeak = std::max(blockPrePeak, std::max(std::abs(left), std::abs(right)));
    left *= rendered.inputTrim;
    right *= rendered.inputTrim;
    blockPostPeak = std::max(blockPostPeak, std::max(std::abs(left), std::abs(right)));
    const double inL = left;
    const double inR = right;
    // Dry input, before the cores and before the output mute.
    const double dryMono = 0.5 * (inL + inR);
    pitch.feed(dryMono, rendered);
    meter.pushInput(static_cast<float>(dryMono));

    const Stereo highpassed = inputHp.process(inL, inR, rendered.inHpA);
    if (asOn(heard.inputGate))
        subGate.process(highpassed.l, highpassed.r, rendered.subGateAtt, rendered.subGateRel);
    else
    {
        subGate.env = 1.0 + rendered.gateOpenPole * (subGate.env - 1.0);
    }
    Stereo leveled = leveler.process(highpassed.l, highpassed.r, heard, rendered);
    swell.process(leveled.l, leveled.r, heard, rendered);
    const double sagGain = sag.process(leveled.l, leveled.r, heard, rendered);

    double mono = 0.5 * (leveled.l + leveled.r);
    const GlideFrame washed = glide.process(leveled.l, leveled.r, heard, rendered);
    if (asInt(heard.glideRoute) == 0 && asInt(heard.glideMode) > 0 && heard.glideMix > 0.001)
        mono = mono * (1.0 - heard.glideMix * 0.50) + washed.mono * (heard.glideMix * 1.15);

    const double subGen = subOctave.process(mono, heard, rendered);

    double ampL = 0.0;
    double ampR = 0.0;
    if (engine == 0)
    {
        const double core = harmonic.process(mono, sagGain, subGate.env, subGen, heard, rendered);
        ampL = core;
        ampR = core;
    }
    else
    {
        const Stereo wall = drone.process(mono, sagGain, subGate.env, subGen, heard, rendered);
        // The Twin core has its own output trim; the harmonic core's LEVEL
        // (coreLevelDb) is baked into its coefficients instead.
        ampL = wall.l * rendered.twinGain;
        ampR = wall.r * rendered.twinGain;
    }

    if (asInt(heard.glideRoute) == 1 && asInt(heard.glideMode) > 0 && heard.glideMix > 0.001)
    {
        ampL = ampL * (1.0 - heard.glideMix * 0.45) + washed.l * (heard.glideMix * 0.90);
        ampR = ampR * (1.0 - heard.glideMix * 0.45) + washed.r * (heard.glideMix * 0.90);
    }

    grit.process(ampL, ampR, heard, rendered);
    pedal.process(ampL, ampR, heard, rendered);
    howl.process(ampL, ampR, heard, rendered);
    phase.process(ampL, ampR, heard, rendered);
    chorus.process(ampL, ampR, heard, rendered);

    double fxL = ampL;
    double fxR = ampR;
    runAtmosphere(fxL, fxR, heard, true);
    const double preL = fxL;
    const double preR = fxR;
    const Stereo enclosed = shroud.process(preL, preR, heard, rendered, shroudWet);
    const double origL = enclosed.l;
    const double origR = enclosed.r;
    fxL = origL;
    fxR = origR;
    if (doublerWet > 0.0008)
    {
        // Spread 0 keeps both centred. Higher spread pans the original left and this contour right.
        Parameters voiceParams = heard;
        voiceParams.shroudStereo = 0.0;
        const Stereo doubled = doublerShroud.process(preL, preR, voiceParams, doublerCoeffs, 1.0);
        double voice = doubled.l;
        if (asOn(heard.polarityInvert))
            voice = -voice;
        const double spreadTarget = paMono ? 0.0 : clamp(heard.doublerSpread, 0.0, 1.0);
        const double levelTarget = clamp(heard.doublerLevel, 0.0, 1.0);
        doublerSpreadState += rendered.follow18 * (spreadTarget - doublerSpreadState);
        doublerLevelState += rendered.follow18 * (levelTarget - doublerLevelState);
        const double s = doublerSpreadState;
        const double keep = 1.0 - s;
        const double wet = doublerWet;
        const double level = doublerLevelState * wet;
        fxL = origL + voice * level * keep;
        fxR = origR * (1.0 - wet * s) + voice * level;
    }
    tremolo.process(fxL, fxR, dryMono, heard, rendered);
    runAtmosphere(fxL, fxR, heard, false);

    double outL = saturate(fxL * rendered.masterGain * 0.65) * 0.95;
    double outR = saturate(fxR * rendered.masterGain * 0.65) * 0.95;
    if (asOn(params.limiterOn))
        limiter.process(outL, outR, rendered);

    if (fadeState == 1)
    {
        fadeGain -= coeffs.fadeStep;
        if (fadeGain <= 0.0)
        {
            fadeGain = 0.0;
            if (stagedValid)
                installStagedPreset();
            fadeState = 2;
        }
    }
    else if (fadeState == 2)
    {
        fadeGain += coeffs.fadeStep;
        if (fadeGain >= 1.0)
        {
            fadeGain = 1.0;
            fadeState = 0;
        }
    }

    if (asOn(params.tunerMute))
        muteGain = std::max(0.0, muteGain - coeffs.muteStep);
    else
        muteGain = std::min(1.0, muteGain + coeffs.muteStep);

    const double outputGain = fadeGain * muteGain;
    left = outL * outputGain;
    right = outR * outputGain;
    if (paMono)
    {
        const double folded = monoSum(left, right, paInvertR);
        duplicateMono(folded, left, right);
    }
    meter.pushOutput(static_cast<float>(0.5 * (left + right)));
}

void Processor::process(double* left, double* right, int numSamples, BlockMetrics* metrics)
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    shroud.heal();
    doublerShroud.heal();
    subGate.heal();
    leveler.heal();
    sag.heal();
    beginBlockParameters();
    for (int i = 0; i < numSamples; ++i)
    {
        double sampleL = left[i];
        double sampleR = right[i];
        processSample(sampleL, sampleR);
        if (!std::isfinite(sampleL) || !std::isfinite(sampleR))
        {
            if (metrics != nullptr)
                metrics->accumulate(sampleL, sampleR);
            sampleL = 0.0;
            sampleR = 0.0;
        }
        else if (metrics != nullptr)
        {
            metrics->accumulate(sampleL, sampleR);
        }
        left[i] = sampleL;
        right[i] = sampleR;
    }
    meter.publishInputLevel(static_cast<float>(blockPrePeak), static_cast<float>(blockPostPeak));
}

void Processor::process(float* left, float* right, int numSamples, BlockMetrics* metrics)
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    const bool aliased = left == right;
    shroud.heal();
    doublerShroud.heal();
    subGate.heal();
    leveler.heal();
    sag.heal();
    beginBlockParameters();
    for (int i = 0; i < numSamples; ++i)
    {
        double sampleL = static_cast<double>(left[i]);
        double sampleR = static_cast<double>(right[i]);
        processSample(sampleL, sampleR);
        if (!std::isfinite(sampleL) || !std::isfinite(sampleR))
        {
            if (metrics != nullptr)
                metrics->accumulate(sampleL, sampleR);
            sampleL = 0.0;
            sampleR = 0.0;
        }
        else if (metrics != nullptr)
        {
            metrics->accumulate(sampleL, sampleR);
        }
        if (aliased)
            left[i] = static_cast<float>(0.5 * (sampleL + sampleR));
        else
        {
            left[i] = static_cast<float>(sampleL);
            right[i] = static_cast<float>(sampleR);
        }
    }
    meter.publishInputLevel(static_cast<float>(blockPrePeak), static_cast<float>(blockPostPeak));
}

} // namespace doomgazer
