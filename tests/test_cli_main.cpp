#include "doomgazer/OctaveDivider.h"
#include "doomgazer/Processor.h"
#include "doomgazer/Saturation.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Options
{
    double sampleRate = 48000.0;
    int blockSize = 512;
    int blocks = 64;
    std::string signal = "impulse";
    int preset = -1;
    bool json = false;
    bool selfTest = false;
    bool audit = false;
};

void printUsage()
{
    std::cerr
        << "doomgazer_test_cli --signal impulse|sine|noise|zero [--blocks N] [--block-size N]\n"
        << "                    [--sample-rate Hz] [--preset N] [--json] [--self-test]\n"
        << "                    [--audit]\n";
}

bool parseArgs(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc)
            {
                std::cerr << "missing value for " << name << "\n";
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--json")
            options.json = true;
        else if (arg == "--self-test")
            options.selfTest = true;
        else if (arg == "--audit")
            options.audit = true;
        else if (arg == "--signal")
        {
            const char* value = need("--signal");
            if (value == nullptr)
                return false;
            options.signal = value;
        }
        else if (arg == "--sample-rate")
        {
            const char* value = need("--sample-rate");
            if (value == nullptr)
                return false;
            options.sampleRate = std::stod(value);
        }
        else if (arg == "--block-size")
        {
            const char* value = need("--block-size");
            if (value == nullptr)
                return false;
            options.blockSize = std::stoi(value);
        }
        else if (arg == "--blocks")
        {
            const char* value = need("--blocks");
            if (value == nullptr)
                return false;
            options.blocks = std::stoi(value);
        }
        else if (arg == "--preset")
        {
            const char* value = need("--preset");
            if (value == nullptr)
                return false;
            options.preset = std::stoi(value);
        }
        else if (arg == "--help" || arg == "-h")
        {
            printUsage();
            return false;
        }
        else
        {
            std::cerr << "unknown argument " << arg << "\n";
            printUsage();
            return false;
        }
    }
    return true;
}

void renderSignal(const std::string& kind, int index, double sampleRate, double& left, double& right)
{
    if (kind == "zero")
    {
        left = 0.0;
        right = 0.0;
    }
    else if (kind == "impulse")
    {
        left = index == 0 ? 1.0 : 0.0;
        right = left;
    }
    else if (kind == "sine")
    {
        const double phase = 2.0 * doomgazer::kPi * 1000.0 * static_cast<double>(index) / sampleRate;
        left = 0.25 * std::sin(phase);
        right = left;
    }
    else if (kind == "strings")
    {
        const double t = static_cast<double>(index) / sampleRate;
        const double env = 0.65 + 0.35 * std::sin(2.0 * doomgazer::kPi * 2.4 * t);
        const double freqs[] = { 82.41, 110.0, 146.83, 196.0 };
        double sum = 0.0;
        for (const double freq : freqs)
            sum += std::sin(2.0 * doomgazer::kPi * freq * t) + 0.35 * std::sin(2.0 * doomgazer::kPi * freq * 2.0 * t);
        left = 0.08 * env * sum;
        right = left;
    }
    else
    {
        // Deterministic bipolar noise so failures reproduce.
        uint32_t n = static_cast<uint32_t>(index) * 1664525u + 1013904223u;
        const double unit = (static_cast<double>(n >> 8) / static_cast<double>(1u << 24)) * 2.0 - 1.0;
        left = unit * 0.25;
        right = left;
    }
}

doomgazer::BlockMetrics run(doomgazer::Processor& processor, const Options& options, const doomgazer::Parameters* params)
{
    processor.prepare(options.sampleRate, options.blockSize);
    processor.reset();
    if (params != nullptr)
        processor.setParameters(*params);

    doomgazer::BlockMetrics metrics;
    std::vector<double> left(static_cast<std::size_t>(options.blockSize));
    std::vector<double> right(static_cast<std::size_t>(options.blockSize));
    int frame = 0;
    for (int block = 0; block < options.blocks; ++block)
    {
        for (int i = 0; i < options.blockSize; ++i, ++frame)
            renderSignal(options.signal, frame, options.sampleRate, left[static_cast<std::size_t>(i)], right[static_cast<std::size_t>(i)]);
        processor.process(left.data(), right.data(), options.blockSize, &metrics);
    }
    return metrics;
}

void printJson(const doomgazer::BlockMetrics& metrics)
{
    const bool passed = metrics.nonFinite == 0 && std::isfinite(metrics.peak);
    std::cout << std::fixed;
    std::cout << "{\n"
              << "  \"compiled\": true,\n"
              << "  \"has_nan_inf\": " << (metrics.nonFinite > 0 ? "true" : "false") << ",\n"
              << "  \"metrics\": {\n"
              << "    \"peak_dbfs\": " << metrics.peakDbfs() << ",\n"
              << "    \"rms_dbfs\": " << metrics.rmsDbfs() << ",\n"
              << "    \"dc_offset\": " << metrics.dcOffset() << ",\n"
              << "    \"non_finite\": " << metrics.nonFinite << ",\n"
              << "    \"passed_audio\": " << (passed ? "true" : "false") << "\n"
              << "  }\n"
              << "}\n";
}

bool expectFinite(const char* name, const doomgazer::BlockMetrics& metrics, double peakLimit)
{
    if (metrics.nonFinite != 0 || !std::isfinite(metrics.peak) || metrics.peak > peakLimit)
    {
        std::cerr << "FAIL " << name
                  << " nonfinite=" << metrics.nonFinite
                  << " peak=" << metrics.peak
                  << "\n";
        return false;
    }
    std::cout << "ok " << name << " peak=" << metrics.peak << "\n";
    return true;
}

int dividerFlips(double freq, double harmonic, double seconds)
{
    doomgazer::OctaveDivider divider;
    const double sampleRate = 48000.0;
    const int settle = static_cast<int>(0.30 * sampleRate);
    const int total = static_cast<int>(seconds * sampleRate);
    int changes = 0;
    double previous = 0.0;
    for (int i = 0; i < total; ++i)
    {
        const double t = static_cast<double>(i) / sampleRate;
        const double x = std::sin(2.0 * doomgazer::kPi * freq * t)
                       + harmonic * std::sin(2.0 * doomgazer::kPi * freq * 2.0 * t);
        const double y = divider.process(0.40 * x, sampleRate);
        if (i > settle && previous * y < 0.0)
            ++changes;
        previous = y;
    }
    return changes;
}

bool dividerLocks(const char* name, double freq, double harmonic, double seconds, int low, int high)
{
    const int flips = dividerFlips(freq, harmonic, seconds);
    const double measured = static_cast<double>(flips) / (seconds - 0.30);
    if (flips < low || flips > high)
    {
        std::cerr << "FAIL " << name << " flips=" << flips << " hz=" << measured << "\n";
        return false;
    }
    std::cout << "ok " << name << " flips=" << flips << " hz=" << measured << "\n";
    return true;
}

bool loudClickGuards();

bool selfTest()
{
    bool ok = true;
    const double x = 1.0;
    const double expected = x * (27.0 + x * x) / (27.0 + 9.0 * x * x);
    if (std::abs(doomgazer::saturate(1.0) - expected) > 1.0e-12
        || doomgazer::saturate(0.0) != 0.0
        || doomgazer::saturate(10.0) != 1.0
        || doomgazer::saturate(-10.0) != -1.0)
    {
        std::cerr << "FAIL saturate formula\n";
        ok = false;
    }
    else
    {
        std::cout << "ok saturate\n";
    }

    const double shaped = doomgazer::asymClip(0.8, 0.4);
    if (!std::isfinite(shaped) || shaped < 0.0)
    {
        std::cerr << "FAIL asym_clip\n";
        ok = false;
    }
    else
    {
        std::cout << "ok asym_clip\n";
    }

    doomgazer::Parameters defaults;
    if (defaults.at(1) != defaults.engineMode || defaults.at(61) != defaults.glideRoute
        || defaults.at(62) != defaults.inputGate || defaults.at(63) != defaults.inputTrimDb
        || defaults.at(64) != defaults.coreSubShift || defaults.at(65) != defaults.lowSubShift
        || defaults.at(69) != defaults.gritCharacter || defaults.at(70) != defaults.gritAmount
        || defaults.at(76) != defaults.phaseMix || defaults.at(78) != defaults.chorusMix
        || defaults.at(81) != defaults.swellAmount || defaults.at(84) != defaults.howlSense)
    {
        std::cerr << "FAIL parameter index map\n";
        ok = false;
    }
    else
    {
        std::cout << "ok parameter index map\n";
    }

    Options options;
    options.blockSize = 256;
    options.blocks = 16;
    options.signal = "zero";
    doomgazer::Processor silence;
    ok = expectFinite("silence-drone", run(silence, options, nullptr), 0.05) && ok;

    doomgazer::Parameters core;
    core.engineMode = 0.0;
    options.signal = "sine";
    options.blocks = 32;
    doomgazer::Processor harmonic;
    const auto harmonicMetrics = run(harmonic, options, &core);
    ok = expectFinite("harmonic-core-sine", harmonicMetrics, 0.72) && ok;
    if (harmonicMetrics.peak < 1.0e-4)
    {
        std::cerr << "FAIL harmonic core produced silence\n";
        ok = false;
    }

    options.signal = "noise";
    options.blocks = 48;
    doomgazer::Processor drone;
    ok = expectFinite("drone-noise", run(drone, options, nullptr), 0.72) && ok;

    for (int preset = 0; preset < doomgazer::kFactoryPresetCount; ++preset)
    {
        options.blocks = 24;
        options.signal = "noise";
        options.sampleRate = (preset % 2 == 0) ? 48000.0 : 44100.0;
        doomgazer::Processor processor;
        const auto params = doomgazer::factoryPreset(preset);
        const std::string name = "preset-" + std::to_string(preset);
        ok = expectFinite(name.c_str(), run(processor, options, &params), 0.72) && ok;
    }

    options.sampleRate = 96000.0;
    options.blocks = 20;
    options.signal = "sine";
    doomgazer::Processor highRate;
    ok = expectFinite("96k-sine", run(highRate, options, nullptr), 0.72) && ok;

    options.sampleRate = 48000.0;
    options.blocks = 8;
    options.signal = "noise";
    doomgazer::Processor fader;
    fader.prepare(options.sampleRate, options.blockSize);
    fader.reset();
    fader.requestPreset(doomgazer::factoryPreset(8));
    doomgazer::BlockMetrics fadeMetrics;
    std::vector<double> left(static_cast<std::size_t>(options.blockSize), 0.2);
    std::vector<double> right(left);
    for (int block = 0; block < options.blocks; ++block)
        fader.process(left.data(), right.data(), options.blockSize, &fadeMetrics);
    ok = expectFinite("preset-crossfade", fadeMetrics, 0.72) && ok;

    const double window = 1.0;
    ok = dividerLocks("divider-a-sharp-2", 116.54, 0.0, window, 70, 110) && ok;
    ok = dividerLocks("divider-a-sharp-2-harmonic", 116.54, 0.65, window, 70, 110) && ok;
    ok = dividerLocks("divider-a-sharp-1", 58.27, 0.45, window, 32, 55) && ok;
    ok = dividerLocks("divider-a-sharp-1-strong", 58.27, 1.80, window, 32, 55) && ok;
    ok = dividerLocks("divider-d-string", 155.56, 0.40, window, 95, 140) && ok;

    {
        doomgazer::Processor processor;
        processor.prepare(48000.0, 256);
        processor.reset();
        doomgazer::Parameters muted;
        muted.tunerMute = 1.0;
        processor.setParameters(muted);
        std::vector<double> inL(256);
        std::vector<double> inR(256);
        for (int block = 0; block < 200; ++block)
        {
            for (int i = 0; i < 256; ++i)
            {
                const double n = static_cast<double>(block * 256 + i);
                const double t = n / 48000.0;
                const double sample = 0.22 * std::sin(2.0 * doomgazer::kPi * 58.27 * t)
                                    + 0.40 * std::sin(2.0 * doomgazer::kPi * 116.54 * t);
                inL[static_cast<std::size_t>(i)] = sample;
                inR[static_cast<std::size_t>(i)] = sample;
            }
            processor.process(inL.data(), inR.data(), 256, nullptr);
        }
        processor.pollTuner();
        const auto reading = processor.tuner();
        const bool heard = reading.active && reading.hz > 50.0 && reading.hz < 70.0;
        const bool silentOut = std::abs(inL.back()) < 0.02;
        if (!heard || !silentOut)
        {
            std::cerr << "FAIL muted-tuner hz=" << reading.hz
                      << " active=" << reading.active
                      << " out=" << inL.back() << "\n";
            ok = false;
        }
        else
        {
            std::cout << "ok muted-tuner hz=" << reading.hz << "\n";
        }
    }

    ok = loudClickGuards() && ok;
    return ok;
}

enum class Stimulus
{
    Sine,
    Noise
};

struct WindowScore
{
    double maxDelta = 0.0;
    double edgeDelta = 0.0;
    int hot = 0;
    int nonFinite = 0;
    double peak = 0.0;
    bool sawEdge = false;
};

doomgazer::Parameters quietCore()
{
    doomgazer::Parameters params;
    params.engineMode = 0.0;
    params.doublerOn = 0.0;
    params.sustainOn = 0.0;
    params.limiterOn = 0.0;
    params.shroudOn = 0.0;
    params.delayMode = 0.0;
    params.reverbMode = 0.0;
    params.tremoloOn = 0.0;
    params.glideMode = 0.0;
    params.glideMix = 0.0;
    params.subOctave = 0.0;
    params.subWeight = 0.0;
    params.subOctLevel = 0.0;
    params.melodyCarve = 0.0;
    params.masterDb = -8.0;
    return params;
}

void writeStimulus(Stimulus stimulus, int index, double sampleRate, double& left, double& right)
{
    if (stimulus == Stimulus::Sine)
    {
        const double t = static_cast<double>(index) / sampleRate;
        const double sample = 0.22 * std::sin(2.0 * doomgazer::kPi * 110.0 * t)
                            + 0.08 * std::sin(2.0 * doomgazer::kPi * 220.0 * t);
        left = sample;
        right = sample;
        return;
    }

    const uint32_t n = static_cast<uint32_t>(index) * 1664525u + 1013904223u;
    const double sample = ((static_cast<double>(n >> 8) / static_cast<double>(1u << 24)) * 2.0 - 1.0) * 0.22;
    left = sample;
    right = sample;
}

// Settle, then score sample-to-sample deltas over the following window.
// A non-null step is applied on the block boundary where the window starts.
WindowScore scoreWindow(const doomgazer::Parameters& held,
                        const doomgazer::Parameters* step,
                        bool presetStep,
                        double sampleRate,
                        int blockSize,
                        Stimulus stimulus,
                        double settleSeconds,
                        double measureSeconds,
                        double hotLine)
{
    doomgazer::Processor processor;
    processor.prepare(sampleRate, blockSize);
    processor.reset();
    processor.setParameters(held);

    const int settle = std::max(blockSize, static_cast<int>(std::llround(settleSeconds * sampleRate)));
    const int settleAligned = ((settle + blockSize - 1) / blockSize) * blockSize;
    const int measure = std::max(blockSize, static_cast<int>(std::llround(measureSeconds * sampleRate)));
    const int measureAligned = ((measure + blockSize - 1) / blockSize) * blockSize;

    std::vector<double> left(static_cast<std::size_t>(blockSize));
    std::vector<double> right(static_cast<std::size_t>(blockSize));
    WindowScore score;
    double prevL = 0.0;
    double prevR = 0.0;
    bool havePrev = false;
    bool stepped = step == nullptr;

    for (int done = 0, frame = 0; done < settleAligned + measureAligned; done += blockSize)
    {
        for (int i = 0; i < blockSize; ++i)
            writeStimulus(stimulus, frame + i, sampleRate, left[static_cast<std::size_t>(i)], right[static_cast<std::size_t>(i)]);
        frame += blockSize;

        if (!stepped && done >= settleAligned)
        {
            if (presetStep)
                processor.requestPreset(*step);
            else
                processor.setParameters(*step);
            stepped = true;
        }

        processor.process(left.data(), right.data(), blockSize, nullptr);
        const bool record = done >= settleAligned;
        for (int i = 0; i < blockSize; ++i)
        {
            const double sampleL = left[static_cast<std::size_t>(i)];
            const double sampleR = right[static_cast<std::size_t>(i)];
            if (!std::isfinite(sampleL) || !std::isfinite(sampleR))
            {
                if (record)
                    ++score.nonFinite;
                havePrev = false;
                continue;
            }
            if (record)
                score.peak = std::max(score.peak, std::max(std::abs(sampleL), std::abs(sampleR)));
            if (havePrev && record)
            {
                const double delta = std::max(std::abs(sampleL - prevL), std::abs(sampleR - prevR));
                score.maxDelta = std::max(score.maxDelta, delta);
                if (!score.sawEdge)
                {
                    score.edgeDelta = delta;
                    score.sawEdge = true;
                }
                if (delta > hotLine)
                    ++score.hot;
            }
            prevL = sampleL;
            prevR = sampleR;
            havePrev = true;
        }
    }
    return score;
}

struct ClickCase
{
    const char* name;
    doomgazer::Parameters from;
    doomgazer::Parameters to;
    Stimulus stimulus = Stimulus::Sine;
    bool preset = false;
    double settle = 0.50;
    double measure = 0.012;
};

double silenceRms(doomgazer::Processor& processor, int blockSize, int samples)
{
    std::vector<double> left(static_cast<std::size_t>(blockSize), 0.0);
    std::vector<double> right(static_cast<std::size_t>(blockSize), 0.0);
    double sumSquares = 0.0;
    int counted = 0;
    for (int done = 0; done < samples; done += blockSize)
    {
        std::fill(left.begin(), left.end(), 0.0);
        std::fill(right.begin(), right.end(), 0.0);
        processor.process(left.data(), right.data(), blockSize, nullptr);
        const int take = std::min(blockSize, samples - done);
        for (int i = 0; i < take; ++i)
        {
            const double mid = 0.5 * (left[static_cast<std::size_t>(i)] + right[static_cast<std::size_t>(i)]);
            if (!std::isfinite(mid))
                continue;
            sumSquares += mid * mid;
            ++counted;
        }
    }
    if (counted <= 0)
        return 0.0;
    return std::sqrt(sumSquares / static_cast<double>(counted));
}

double presetTail(double sampleRate, int blockSize)
{
    const doomgazer::Parameters from = doomgazer::factoryPreset(0);
    const doomgazer::Parameters to = doomgazer::factoryPreset(2);
    doomgazer::Processor faded;
    faded.prepare(sampleRate, blockSize);
    faded.reset();
    faded.setParameters(from);

    const int fill = static_cast<int>(0.60 * sampleRate);
    const int fillAligned = ((fill + blockSize - 1) / blockSize) * blockSize;
    std::vector<double> left(static_cast<std::size_t>(blockSize));
    std::vector<double> right(static_cast<std::size_t>(blockSize));
    for (int done = 0, frame = 0; done < fillAligned; done += blockSize, frame += blockSize)
    {
        for (int i = 0; i < blockSize; ++i)
            writeStimulus(Stimulus::Noise, frame + i, sampleRate, left[static_cast<std::size_t>(i)], right[static_cast<std::size_t>(i)]);
        faded.process(left.data(), right.data(), blockSize, nullptr);
    }
    faded.requestPreset(to);
    std::fill(left.begin(), left.end(), 0.0);
    std::fill(right.begin(), right.end(), 0.0);
    const int fade = static_cast<int>(0.040 * sampleRate);
    const int fadeAligned = ((fade + blockSize - 1) / blockSize) * blockSize;
    for (int done = 0; done < fadeAligned; done += blockSize)
    {
        std::fill(left.begin(), left.end(), 0.0);
        std::fill(right.begin(), right.end(), 0.0);
        faded.process(left.data(), right.data(), blockSize, nullptr);
    }
    doomgazer::Processor fresh;
    fresh.prepare(sampleRate, blockSize);
    fresh.reset();
    fresh.setParameters(to);

    const int tail = static_cast<int>(0.20 * sampleRate);
    const double fadedRms = silenceRms(faded, blockSize, tail);
    const double freshRms = silenceRms(fresh, blockSize, tail);
    const double ratio = fadedRms / std::max(freshRms, 1.0e-12);
    std::cout << std::fixed << std::setprecision(5)
              << "tail sr=" << static_cast<int>(sampleRate)
              << " block=" << blockSize
              << " faded_rms=" << fadedRms
              << " fresh_rms=" << freshRms
              << " ratio=" << std::setprecision(2) << ratio
              << "\n";
    return fadedRms;
}

bool loudClickGuards()
{
    doomgazer::Parameters dry = quietCore();
    dry.shroudOn = 1.0;

    doomgazer::Parameters gainUp = dry;
    gainUp.masterDb = 0.0;
    doomgazer::Parameters gainDown = dry;
    gainDown.masterDb = -18.0;

    doomgazer::Parameters tremOff = dry;
    doomgazer::Parameters tremOn = dry;
    tremOn.tremoloOn = 1.0;
    tremOn.tremoloMix = 1.0;
    tremOn.tremoloDepth = 0.85;
    tremOn.tremoloHz = 4.2;

    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    bool ok = true;
    for (const double rate : rates)
    {
        const auto judge = [&](const char* name, const doomgazer::Parameters& from, const doomgazer::Parameters& to, bool tremolo) {
            const WindowScore heldFrom = scoreWindow(from, nullptr, false, rate, 64, Stimulus::Sine, 0.30, 0.012, 1.0e9);
            const WindowScore heldTo = scoreWindow(to, nullptr, false, rate, 64, Stimulus::Sine, 0.30, 0.012, 1.0e9);
            const double base = std::max(heldFrom.maxDelta, heldTo.maxDelta);
            const WindowScore stepped = scoreWindow(from, &to, false, rate, 64, Stimulus::Sine, 0.30, 0.012, 1.0e9);
            const double ratio = stepped.maxDelta / std::max(base, 1.0e-12);
            const double excess = stepped.maxDelta - base;
            bool fail = stepped.nonFinite > 0 || heldFrom.nonFinite > 0 || heldTo.nonFinite > 0;
            if (tremolo)
                fail = fail || ratio >= 8.0 || (stepped.edgeDelta > 0.5 * stepped.maxDelta && excess >= 0.01);
            else
                fail = fail || ratio >= 2.5 || excess >= 0.02;
            std::cout << std::fixed << std::setprecision(4)
                      << "guard " << name
                      << " sr=" << static_cast<int>(rate)
                      << " ratio=" << std::setprecision(2) << ratio
                      << " excess=" << std::setprecision(4) << excess
                      << " edge=" << stepped.edgeDelta
                      << (fail ? " FAIL" : " ok")
                      << "\n";
            if (fail)
                ok = false;
        };
        judge("master-gain", gainDown, gainUp, false);
        judge("tremolo-on", tremOff, tremOn, true);

        const double faded = presetTail(rate, 64);
        if (faded > 0.02)
        {
            std::cerr << "FAIL preset-tail sr=" << static_cast<int>(rate) << " faded_rms=" << faded << "\n";
            ok = false;
        }
        else
        {
            std::cout << "ok preset-tail sr=" << static_cast<int>(rate) << "\n";
        }
    }
    return ok;
}

bool runClicks()
{
    doomgazer::Parameters dry = quietCore();
    dry.shroudOn = 1.0;

    doomgazer::Parameters gainUp = dry;
    gainUp.masterDb = 0.0;
    doomgazer::Parameters gainDown = dry;
    gainDown.masterDb = -18.0;

    doomgazer::Parameters toneA = dry;
    toneA.highCutHz = 1500.0;
    doomgazer::Parameters toneB = dry;
    toneB.highCutHz = 3400.0;

    doomgazer::Parameters delayMixA = dry;
    delayMixA.delayMode = 1.0;
    delayMixA.delayMix = 0.0;
    delayMixA.delayMs = 280.0;
    delayMixA.delayFeedback = 0.55;
    doomgazer::Parameters delayMixB = delayMixA;
    delayMixB.delayMix = 1.0;

    doomgazer::Parameters delayTimeA = delayMixB;
    delayTimeA.delayMs = 180.0;
    doomgazer::Parameters delayTimeB = delayMixB;
    delayTimeB.delayMs = 900.0;

    doomgazer::Parameters delayOff = delayMixB;
    delayOff.delayMode = 0.0;

    doomgazer::Parameters reverbA = dry;
    reverbA.reverbMode = 1.0;
    reverbA.reverbMix = 0.75;
    reverbA.reverbSizeMs = 280.0;
    reverbA.reverbDecay = 0.82;
    doomgazer::Parameters reverbB = reverbA;
    reverbB.reverbSizeMs = 1800.0;
    doomgazer::Parameters reverbOff = reverbA;
    reverbOff.reverbMode = 0.0;

    doomgazer::Parameters shroudOff = dry;
    shroudOff.shroudOn = 0.0;

    doomgazer::Parameters tremOff = dry;
    doomgazer::Parameters tremOn = dry;
    tremOn.tremoloOn = 1.0;
    tremOn.tremoloMix = 1.0;
    tremOn.tremoloDepth = 0.85;
    tremOn.tremoloHz = 4.2;

    doomgazer::Parameters glideOn = dry;
    glideOn.glideMode = 1.0;
    glideOn.glideMix = 0.85;
    doomgazer::Parameters glideOff = glideOn;
    glideOff.glideMode = 0.0;

    doomgazer::Parameters sustainOn = dry;
    sustainOn.sustainOn = 1.0;
    sustainOn.sustain = 0.75;
    doomgazer::Parameters sustainOff = sustainOn;
    sustainOff.sustainOn = 0.0;

    doomgazer::Parameters limitOff = dry;
    limitOff.masterDb = 6.0;
    doomgazer::Parameters limitOn = limitOff;
    limitOn.limiterOn = 1.0;

    doomgazer::Parameters engineA = dry;
    engineA.engineMode = 0.0;
    doomgazer::Parameters engineB = dry;
    engineB.engineMode = 1.0;
    engineB.doublerOn = 1.0;

    const ClickCase cases[] = {
        { "master-gain", gainDown, gainUp, Stimulus::Sine, false, 0.30, 0.012 },
        { "tone-highcut", toneA, toneB, Stimulus::Sine, false, 0.30, 0.012 },
        { "delay-mix", delayMixA, delayMixB, Stimulus::Sine, false, 0.70, 0.012 },
        { "delay-time", delayTimeA, delayTimeB, Stimulus::Sine, false, 0.70, 0.012 },
        { "delay-bypass", delayMixB, delayOff, Stimulus::Sine, false, 0.70, 0.012 },
        { "reverb-size", reverbA, reverbB, Stimulus::Sine, false, 0.70, 0.012 },
        { "reverb-bypass", reverbA, reverbOff, Stimulus::Sine, false, 0.70, 0.012 },
        { "shroud-bypass", dry, shroudOff, Stimulus::Sine, false, 0.30, 0.012 },
        { "tremolo-on", tremOff, tremOn, Stimulus::Sine, false, 0.30, 0.012 },
        { "glide-bypass", glideOn, glideOff, Stimulus::Sine, false, 0.50, 0.012 },
        { "sustain-bypass", sustainOn, sustainOff, Stimulus::Sine, false, 0.30, 0.012 },
        { "limiter-on", limitOff, limitOn, Stimulus::Sine, false, 0.30, 0.012 },
        { "engine-switch", engineA, engineB, Stimulus::Sine, false, 0.30, 0.012 },
        { "preset-crossfade", doomgazer::factoryPreset(0), doomgazer::factoryPreset(2), Stimulus::Noise, true, 0.70, 0.030 },
    };

    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const int blocks[] = { 32, 64, 512 };
    bool finite = true;
    int flags = 0;

    std::cout << std::fixed;
    for (const ClickCase& item : cases)
    {
        for (const double rate : rates)
        {
            for (const int block : blocks)
            {
                const WindowScore heldFrom = scoreWindow(item.from, nullptr, false, rate, block, item.stimulus, item.settle, item.measure, 1.0e9);
                const WindowScore heldTo = scoreWindow(item.to, nullptr, false, rate, block, item.stimulus, item.settle, item.measure, 1.0e9);
                const double base = std::max(heldFrom.maxDelta, heldTo.maxDelta);
                const double hotLine = std::max(base * 2.0, 0.02);
                const WindowScore stepped = scoreWindow(item.from, &item.to, item.preset, rate, block, item.stimulus, item.settle, item.measure, hotLine);
                const double ratio = stepped.maxDelta / std::max(base, 1.0e-12);
                const double excess = stepped.maxDelta - base;
                const bool flag = stepped.nonFinite == 0 && ratio >= 2.0 && excess >= 0.02;
                if (stepped.nonFinite > 0 || heldFrom.nonFinite > 0 || heldTo.nonFinite > 0)
                    finite = false;
                if (flag)
                    ++flags;
                std::cout << std::setprecision(4)
                          << "click " << item.name
                          << " sr=" << static_cast<int>(rate)
                          << " block=" << block
                          << " base=" << base
                          << " step=" << stepped.maxDelta
                          << " edge=" << stepped.edgeDelta
                          << " ratio=" << std::setprecision(2) << ratio
                          << " hot=" << stepped.hot
                          << " excess=" << std::setprecision(4) << excess
                          << (flag ? " FLAG" : "")
                          << "\n";
            }
        }
    }

    bool ok = finite;
    for (const double rate : rates)
    {
        for (const int block : blocks)
        {
            const double tail = presetTail(rate, block);
            if (tail > 0.02)
            {
                std::cerr << "FAIL preset-tail sr=" << static_cast<int>(rate)
                          << " block=" << block << " faded_rms=" << tail << "\n";
                ok = false;
            }
        }
    }

    std::cout << "click-flags " << flags << "\n";
    if (flags != 0)
        ok = false;
    return ok;
}

doomgazer::Parameters perfPatch(const char* name)
{
    doomgazer::Parameters params = quietCore();
    params.masterDb = -6.0;
    if (std::string(name) == "harmonic-dry")
        return params;

    params.engineMode = 1.0;
    params.doublerOn = 1.0;
    if (std::string(name) == "drone-doubler")
        return params;

    params.shroudOn = 1.0;
    if (std::string(name) == "shroud")
        return params;

    if (std::string(name) == "delay")
    {
        params.delayMode = 1.0;
        params.delayMix = 0.50;
        params.delayMs = 420.0;
        params.delayFeedback = 0.45;
        return params;
    }
    if (std::string(name) == "reverb-shimmer")
    {
        params.reverbMode = 2.0;
        params.reverbMix = 0.45;
        params.reverbSizeMs = 900.0;
        params.reverbDecay = 0.75;
        return params;
    }
    if (std::string(name) == "tremolo")
    {
        params.tremoloOn = 1.0;
        params.tremoloMix = 1.0;
        params.tremoloDepth = 0.70;
        params.tremoloHz = 5.0;
        return params;
    }

    params.tunerMute = 1.0;
    return params;
}

void timeProcess(const char* name, int blockSize, double& usPerSample, double& maxBlockUs)
{
    doomgazer::Processor processor;
    processor.prepare(48000.0, blockSize);
    processor.reset();
    const doomgazer::Parameters params = perfPatch(name);
    processor.setParameters(params);

    const int warmupSamples = 48000 / 5;
    const int timedSamples = 48000;
    std::vector<double> left(static_cast<std::size_t>(blockSize));
    std::vector<double> right(static_cast<std::size_t>(blockSize));
    int frame = 0;
    auto render = [&](int samples) {
        for (int done = 0; done < samples; done += blockSize, frame += blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
                writeStimulus(Stimulus::Sine, frame + i, 48000.0, left[static_cast<std::size_t>(i)], right[static_cast<std::size_t>(i)]);
            processor.process(left.data(), right.data(), blockSize, nullptr);
        }
    };
    render(warmupSamples);

    using Clock = std::chrono::steady_clock;
    maxBlockUs = 0.0;
    const auto started = Clock::now();
    for (int done = 0; done < timedSamples; done += blockSize, frame += blockSize)
    {
        for (int i = 0; i < blockSize; ++i)
            writeStimulus(Stimulus::Sine, frame + i, 48000.0, left[static_cast<std::size_t>(i)], right[static_cast<std::size_t>(i)]);
        const auto blockStarted = Clock::now();
        processor.process(left.data(), right.data(), blockSize, nullptr);
        const double blockUs = std::chrono::duration<double, std::micro>(Clock::now() - blockStarted).count();
        maxBlockUs = std::max(maxBlockUs, blockUs);
    }
    const double totalUs = std::chrono::duration<double, std::micro>(Clock::now() - started).count();
    usPerSample = totalUs / static_cast<double>(timedSamples);
}

void timeCoefficientUpdate(int blockSize)
{
    doomgazer::Processor processor;
    processor.prepare(48000.0, blockSize);
    processor.reset();
    const doomgazer::Parameters params = perfPatch("reverb-shimmer");
    processor.setParameters(params);

    constexpr int calls = 2000;
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < calls; ++i)
        processor.setParameters(params);
    const double totalUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count();
    std::cout << std::fixed << std::setprecision(2)
              << "coeff block=" << blockSize
              << " us_per_update=" << (totalUs / static_cast<double>(calls))
              << "\n";
}

bool runAudit()
{
    std::cout << "audit discontinuity: ratio is step max |dy| over max steady |dy| of source and destination\n";
    const bool finite = runClicks();
    const bool guards = loudClickGuards();

    const char* names[] = {
        "harmonic-dry",
        "drone-doubler",
        "shroud",
        "delay",
        "reverb-shimmer",
        "tremolo",
        "tuner-mute",
    };
    const int blocks[] = { 64, 512 };
    std::cout << std::fixed;
    for (const int block : blocks)
    {
        double dryUs = 1.0;
        for (const char* name : names)
        {
            double usPerSample = 0.0;
            double maxBlockUs = 0.0;
            timeProcess(name, block, usPerSample, maxBlockUs);
            if (std::string(name) == "harmonic-dry")
                dryUs = std::max(usPerSample, 1.0e-9);
            const double samples = static_cast<double>(block);
            std::cout << std::setprecision(3)
                      << "perf " << name
                      << " block=" << block
                      << " us_per_sample=" << usPerSample
                      << " ms_per_block=" << (usPerSample * samples / 1000.0)
                      << " max_block_us=" << std::setprecision(1) << maxBlockUs
                      << " rel=" << std::setprecision(2) << (usPerSample / dryUs)
                      << "\n";
        }
        timeCoefficientUpdate(block);
    }

    return finite && guards;
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parseArgs(argc, argv, options))
        return argc > 1 && std::string(argv[1]) == "--help" ? 0 : 1;

    if (options.selfTest)
        return selfTest() ? 0 : 1;

    if (options.audit)
        return runAudit() ? 0 : 1;

    if (options.signal != "impulse" && options.signal != "sine"
        && options.signal != "noise" && options.signal != "zero"
        && options.signal != "strings")
    {
        std::cerr << "unknown signal " << options.signal << "\n";
        return 1;
    }

    doomgazer::Processor processor;
    const doomgazer::Parameters preset = options.preset >= 0
        ? doomgazer::factoryPreset(options.preset)
        : doomgazer::Parameters {};
    const auto metrics = run(processor, options, options.preset >= 0 ? &preset : nullptr);

    if (options.json)
        printJson(metrics);
    else
    {
        std::cout << "peak_dbfs=" << metrics.peakDbfs()
                  << " rms_dbfs=" << metrics.rmsDbfs()
                  << " dc=" << metrics.dcOffset()
                  << " nonfinite=" << metrics.nonFinite << "\n";
    }

    if (metrics.nonFinite > 0)
        return 3;
    return 0;
}
