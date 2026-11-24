#pragma once

#include "doomgazer/Atmosphere.h"
#include "doomgazer/DroneEngine.h"
#include "doomgazer/Dynamics.h"
#include "doomgazer/GlideWash.h"
#include "doomgazer/Chorus.h"
#include "doomgazer/Grit.h"
#include "doomgazer/HarmonicCore.h"
#include "doomgazer/Howl.h"
#include "doomgazer/ScopeMeter.h"
#include "doomgazer/Pedal.h"
#include "doomgazer/Phase.h"
#include "doomgazer/Shroud.h"
#include "doomgazer/Swell.h"
#include "doomgazer/Smoother.h"
#include "doomgazer/SubOctave.h"
#include "doomgazer/Tremolo.h"
#include "doomgazer/Tuner.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace doomgazer {

struct BlockMetrics
{
    double peak = 0.0;
    double sum = 0.0;
    double sumSquares = 0.0;
    int samples = 0;
    int nonFinite = 0;

    void accumulate(double left, double right) noexcept
    {
        if (!std::isfinite(left) || !std::isfinite(right))
        {
            ++nonFinite;
            return;
        }
        peak = std::max(peak, std::max(std::abs(left), std::abs(right)));
        sum += 0.5 * (left + right);
        sumSquares += 0.5 * (left * left + right * right);
        ++samples;
    }

    double peakDbfs() const noexcept
    {
        return samples > 0 ? gainToDb(peak) : -120.0;
    }

    double rmsDbfs() const noexcept
    {
        if (samples <= 0)
            return -120.0;
        return gainToDb(std::sqrt(sumSquares / static_cast<double>(samples)));
    }

    double dcOffset() const noexcept
    {
        return samples > 0 ? sum / static_cast<double>(samples) : 0.0;
    }
};

class Processor
{
public:
    static constexpr int kScopeSize = ScopeMeter::kScopeRing;

    Processor();

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParameters(const Parameters& next);
    void setPaMonoPolicy(bool mono, bool invertR) noexcept;
    void setHostTempo(double bpm) noexcept;
    const Parameters& parameters() const noexcept { return params; }

    // Message or audio thread may publish a preset. The running block fades
    // through zero, then installs it and clears the signal path.
    void requestPreset(const Parameters& next) noexcept;
    bool consumePresetApplied() noexcept;

    void process(double* left, double* right, int numSamples, BlockMetrics* metrics = nullptr);
    void process(float* left, float* right, int numSamples, BlockMetrics* metrics = nullptr);

    void pollTuner() noexcept;
    TunerReadout tuner() const noexcept { return pitch.reading(); }
    uint32_t tunerSerial() const noexcept { return pitch.snapshotSerial(); }

    int scopeWriteIndex() const noexcept;
    const std::array<float, kScopeSize>& scopeBuffer() const noexcept { return meter.outputBuffer(); }

    // Hottest |sample| since the last take. Pre-trim is the signal as it arrived.
    // Post-trim is what the cores hear. The take clears the held peak.
    float takeInputPrePeak() noexcept { return meter.takePreTrimPeak(); }
    float takeInputPostPeak() noexcept { return meter.takePostTrimPeak(); }
    int inputScopeWriteIndex() const noexcept;
    const std::array<float, kScopeSize>& inputScopeBuffer() const noexcept { return meter.inputBuffer(); }
    const std::array<float, ScopeMeter::kSpectrumBands>& inputSpectrum() const noexcept { return meter.spectrum(); }

private:
    void updateCoefficients();
    void setSmootherTimes() noexcept;
    void publishRenderedCoeffs() noexcept;
    void snapSmoothers() noexcept;
    void chaseGains() noexcept;
    void beginBlockParameters() noexcept;
    void updateHeldParameters() noexcept;
    void processSample(double& left, double& right) noexcept;
    void runAtmosphere(double& left, double& right, const Parameters& settings, bool preShroud) noexcept;
    void installStagedPreset() noexcept;

    Parameters params;
    Parameters heard;
    Parameters staged;
    Coefficients coeffs;
    Coefficients rendered;
    Coefficients doublerCoeffs;
    BufferLimits limits;

    OnePoleChase cleanGainChase;
    OnePoleChase cleanMakeupChase;
    OnePoleChase cleanTrimChase;
    OnePoleChase bassGainChase;
    OnePoleChase bassTrimChase;
    OnePoleChase stageGainChase;
    OnePoleChase fuzzTrimChase;
    OnePoleChase masterGainChase;
    OnePoleChase inputTrimChase;
    OnePoleChase delayMixChase;
    OnePoleChase reverbMixChase;
    OnePoleChase tremoloMixChase;
    OnePoleChase glideMixChase;
    OnePoleChase sustainChase;
    OnePoleChase shroudWetChase;
    OnePoleChase doublerWetChase;
    OnePoleChase twinGainChase;
    int delayModeHeld = 0;
    int reverbModeHeld = 0;
    int glideModeHeld = 0;
    double shroudWet = 0.0;
    double doublerWet = 0.0;
    double doublerSpreadState = 0.0;
    double doublerLevelState = 0.50;

    InputHighpass inputHp;
    SubGate subGate;
    OpticalLeveler leveler;
    SupplySag sag;
    GlideWash glide;
    Grit grit;
    SubOctave subOctave;
    Pedal pedal;
    Swell swell;
    Howl howl;
    Phase phase;
    Chorus chorus;
    HarmonicCore harmonic;
    DroneEngine drone;
    DelayEngine delay;
    ReverbEngine reverb;
    Shroud shroud;
    Shroud doublerShroud;
    Tremolo tremolo;
    BrickwallLimiter limiter;
    Tuner pitch;

    int lastEngine = -1;
    int fadeState = 0;
    double blockPrePeak = 0.0;
    double blockPostPeak = 0.0;
    double fadeGain = 1.0;
    double muteGain = 1.0;
    bool stagedValid = false;
    double hostBpm = 0.0;
    bool paMono = false;
    bool paInvertR = false;

    std::array<Parameters, 2> presetMailboxes {};
    std::atomic<int> publishedPreset { -1 };
    std::atomic<bool> presetApplied { false };
    std::atomic_flag presetWriteBusy {};

    ScopeMeter meter;
};

} // namespace doomgazer
