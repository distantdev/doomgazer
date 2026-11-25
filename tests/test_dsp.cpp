#include "doomgazer/Coefficients.h"
#include "doomgazer/PresetIO.h"
#include "doomgazer/Processor.h"
#include "doomgazer/Saturation.h"
#include "doomgazer/Tremolo.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

TEST_CASE("saturate matches the JSFX polynomial and clamps the rails", "[dsp]")
{
    REQUIRE(doomgazer::saturate(0.0) == 0.0);
    REQUIRE(doomgazer::saturate(6.0) == 1.0);
    REQUIRE(doomgazer::saturate(-6.0) == -1.0);

    const double x = 0.75;
    const double expected = x * (27.0 + x * x) / (27.0 + 9.0 * x * x);
    REQUIRE_THAT(doomgazer::saturate(x), Catch::Matchers::WithinAbs(expected, 1.0e-12));
}

TEST_CASE("coefficient update keeps filter poles inside the unit interval", "[dsp]")
{
    doomgazer::Parameters params;
    const auto limits = doomgazer::makeBufferLimits(48000.0);
    doomgazer::Coefficients coeffs;
    coeffs.update(params, limits);

    REQUIRE(coeffs.xoverB1 > 0.0);
    REQUIRE(coeffs.xoverB1 < 1.0);
    REQUIRE(coeffs.xoverA0 == 1.0 - coeffs.xoverB1);
    REQUIRE(coeffs.cab1.a2 < 1.0);
    REQUIRE(coeffs.cab2.a2 < 1.0);
    REQUIRE(coeffs.delaySamps > 1);
    REQUIRE(coeffs.c1Len >= 32);
    REQUIRE(coeffs.fadeStep > 0.0);
    REQUIRE(coeffs.muteStep > 0.0);
    REQUIRE_THAT(coeffs.muteStep, Catch::Matchers::WithinAbs(1.0 / 240.0, 1.0e-12));
}

TEST_CASE("shrinking a ring keeps the cursor inside the new length", "[dsp]")
{
    doomgazer::RingBuffer ring;
    ring.assign(100);
    ring.setLength(80);
    for (int i = 0; i < 50; ++i)
        ring.advance();
    REQUIRE(ring.cursorIndex() == 50);
    ring.setLength(40);
    REQUIRE(ring.cursorIndex() == 10);
}

TEST_CASE("a bypassed delay does not replay its old tail", "[dsp]")
{
    doomgazer::BufferLimits limits = doomgazer::makeBufferLimits(48000.0);
    doomgazer::DelayEngine delay;
    delay.prepare(limits);

    doomgazer::Parameters params;
    params.delayMode = 1.0;
    params.delayMix = 1.0;
    params.delayMs = 100.0;
    params.delayFeedback = 0.0;
    params.delayStereo = 0.0;

    doomgazer::Coefficients coefficients;
    coefficients.update(params, limits);

    double left = 0.0;
    double right = 0.0;
    for (int i = 0; i < 12000; ++i)
    {
        const double sample = 0.5 * std::sin(2.0 * doomgazer::kPi * 440.0
                                             * static_cast<double>(i) / 48000.0);
        left = sample;
        right = sample;
        delay.process(left, right, params, coefficients);
    }

    params.delayMode = 0.0;
    for (int i = 0; i < 12000; ++i)
    {
        left = 0.0;
        right = 0.0;
        delay.process(left, right, params, coefficients);
    }

    params.delayMode = 1.0;
    double peak = 0.0;
    for (int i = 0; i < 8192; ++i)
    {
        left = 0.0;
        right = 0.0;
        delay.process(left, right, params, coefficients);
        peak = std::max(peak, std::max(std::abs(left), std::abs(right)));
    }
    REQUIRE(peak < 1.0e-6);
}

TEST_CASE("a bypassed glide wash does not replay its old tail", "[dsp]")
{
    doomgazer::BufferLimits limits = doomgazer::makeBufferLimits(48000.0);
    doomgazer::GlideWash glide;
    glide.prepare(limits);

    doomgazer::Parameters params;
    params.glideMode = 1.0;
    params.glideMix = 1.0;
    params.glideStereo = 1.0;

    doomgazer::Coefficients coefficients;
    coefficients.update(params, limits);

    for (int i = 0; i < 24000; ++i)
    {
        const double sample = 0.5 * std::sin(2.0 * doomgazer::kPi * 440.0
                                             * static_cast<double>(i) / 48000.0);
        glide.process(sample, sample, params, coefficients);
    }

    params.glideMode = 0.0;
    for (int i = 0; i < 24000; ++i)
        glide.process(0.0, 0.0, params, coefficients);

    params.glideMode = 1.0;
    double peak = 0.0;
    for (int i = 0; i < 4096; ++i)
    {
        const auto frame = glide.process(0.0, 0.0, params, coefficients);
        peak = std::max(peak, std::max(std::abs(frame.l), std::abs(frame.r)));
    }
    REQUIRE(peak < 1.0e-6);
}

TEST_CASE("factory presets stay inside slider ranges", "[dsp]")
{
    for (int slot = 0; slot < doomgazer::kPresetSlots; ++slot)
    {
        const auto preset = doomgazer::factoryPreset(slot);
        for (int index = 1; index <= doomgazer::kParameterCount; ++index)
        {
            const auto range = doomgazer::parameterRange(index);
            REQUIRE(preset.at(index) >= range.min);
            REQUIRE(preset.at(index) <= range.max);
        }
        REQUIRE(preset.presetSlot == static_cast<double>(slot));
    }
}

TEST_CASE("input level keeps the hottest pre-trim peak until it is read", "[dsp]")
{
    doomgazer::Processor processor;
    doomgazer::Parameters params;
    params.inputTrimDb = -6.0;
    processor.setParameters(params);
    processor.prepare(48000.0, 128);
    processor.reset();

    std::vector<double> left(128, 0.5);
    std::vector<double> right(128, 0.25);
    processor.process(left.data(), right.data(), 128);

    const float pre = processor.takeInputPrePeak();
    const float post = processor.takeInputPostPeak();
    REQUIRE_THAT(pre, Catch::Matchers::WithinAbs(0.5, 1.0e-6));
    REQUIRE_THAT(post, Catch::Matchers::WithinAbs(0.5 * doomgazer::dbToGain(-6.0), 1.0e-5));
    REQUIRE(processor.takeInputPrePeak() == 0.0f);
    REQUIRE(processor.takeInputPostPeak() == 0.0f);

    std::fill(left.begin(), left.end(), 1.0);
    std::fill(right.begin(), right.end(), 0.2);
    processor.process(left.data(), right.data(), 128);
    std::fill(left.begin(), left.end(), 0.05);
    std::fill(right.begin(), right.end(), 0.05);
    processor.process(left.data(), right.data(), 128);
    REQUIRE_THAT(processor.takeInputPrePeak(), Catch::Matchers::WithinAbs(1.0, 1.0e-6));
}

TEST_CASE("silence in stays finite and a driven sine is audible", "[dsp]")
{
    doomgazer::Processor processor;
    processor.prepare(48000.0, 128);
    processor.reset();

    std::vector<double> left(128, 0.0);
    std::vector<double> right(128, 0.0);
    doomgazer::BlockMetrics metrics;
    for (int i = 0; i < 8; ++i)
        processor.process(left.data(), right.data(), 128, &metrics);
    REQUIRE(metrics.nonFinite == 0);
    REQUIRE(metrics.peak < 0.05);

    metrics = {};
    for (int block = 0; block < 16; ++block)
    {
        for (int i = 0; i < 128; ++i)
        {
            const double n = static_cast<double>(block * 128 + i);
            left[static_cast<std::size_t>(i)] = 0.3 * std::sin(2.0 * doomgazer::kPi * 110.0 * n / 48000.0);
            right[static_cast<std::size_t>(i)] = left[static_cast<std::size_t>(i)];
        }
        processor.process(left.data(), right.data(), 128, &metrics);
    }
    REQUIRE(metrics.nonFinite == 0);
    REQUIRE(metrics.peak > 1.0e-3);
    REQUIRE(metrics.peak < 1.01);
}

TEST_CASE("flutter keeps chopping while a note is loud and is gone before the tail", "[dsp]")
{
    constexpr double sr = 48000.0;
    doomgazer::Parameters params;
    params.tremoloOn = 1.0;
    params.tremoloMix = 1.0;
    params.tremoloDepth = 0.90;
    params.tremoloHz = 1.8;
    params.tremoloShape = 3.0;
    doomgazer::Coefficients coeffs;
    coeffs.sampleRate = sr;

    auto run = [&](doomgazer::Tremolo& trem, double seconds, auto&& ampAt, auto&& onSample) {
        const int count = static_cast<int>(seconds * sr);
        for (int i = 0; i < count; ++i)
        {
            const double t = static_cast<double>(i) / sr;
            const double in = ampAt(t);
            double left = in;
            double right = in;
            trem.process(left, right, 0.0, params, coeffs);
            onSample(t, in, left);
        }
    };

    doomgazer::Tremolo held;
    int heldDips = 0;
    int heldCompared = 0;
    bool heldWas = false;
    run(held, 1.2, [](double) { return 0.45; }, [&](double t, double in, double out) {
        if (t < 0.25 || in < 0.05)
            return;
        ++heldCompared;
        const bool dip = std::abs(out) < 0.60 * in;
        if (dip && !heldWas)
            ++heldDips;
        heldWas = dip;
    });
    REQUIRE(heldCompared > 1000);
    REQUIRE(heldDips >= 8);

    doomgazer::Tremolo falling;
    int loudDips = 0;
    int tailDips = 0;
    int tailCompared = 0;
    double tailFloor = 1.0;
    bool loudWas = false;
    bool tailWas = false;
    run(falling, 5.2, [](double t) { return 0.50 * std::exp(-t / 2.5); }, [&](double t, double in, double out) {
        if (in < 0.05)
            return;
        const bool dip = std::abs(out) < 0.60 * in;
        if (t >= 0.30 && t <= 0.90)
        {
            if (dip && !loudWas)
                ++loudDips;
            loudWas = dip;
        }
        else
            loudWas = false;
        if (t >= 3.6 && t <= 5.0)
        {
            ++tailCompared;
            const double ratio = std::abs(out) / in;
            if (ratio < tailFloor)
                tailFloor = ratio;
            if (dip && !tailWas)
                ++tailDips;
            tailWas = dip;
        }
        else
            tailWas = false;
    });
    REQUIRE(loudDips >= 5);
    REQUIRE(tailCompared > 1000);
    REQUIRE(tailDips == 0);
    REQUIRE(tailFloor > 0.90);
}

TEST_CASE("sub solo at zero keeps the dry-plus-sub mix", "[dsp]")
{
    REQUIRE(doomgazer::mixSubSolo(0.5, 0.25, 0.0) == 0.75);
    REQUIRE(doomgazer::mixSubSolo(0.5, 0.25, 1.0) == 0.25);
}

TEST_CASE("tempo sync snaps a quarter note and a tremolo rate", "[dsp]")
{
    REQUIRE(doomgazer::snapDelayMs(480.0, 120.0, 100000, 48000.0) == 500.0);
    REQUIRE(doomgazer::snapTremoloHz(4.2, 120.0) == 4.0);
    REQUIRE(doomgazer::snapDelayMs(480.0, 0.0, 100000, 48000.0) == 480.0);
    REQUIRE(doomgazer::snapTremoloHz(4.2, 0.0) == 4.2);
}

TEST_CASE("factory scenes bake their macro trims", "[dsp]")
{
    // Room was 0.82 on this preset and Weight was 0.18 on the next; the trims
    // they used to apply at runtime are baked into the stored values now.
    const auto cathedral = doomgazer::factoryPreset(6);
    REQUIRE(cathedral.freeze > 0.5);
    REQUIRE(cathedral.reverbMix == 0.50);
    REQUIRE(cathedral.reverbSizeMs == 1230.0);
    REQUIRE(cathedral.reverbDecay == 0.81);

    const auto boneCrusher = doomgazer::factoryPreset(7);
    REQUIRE(boneCrusher.lowGainDb == 13.1);
    REQUIRE(boneCrusher.inputTight == 0.81);
}

TEST_CASE("cutoff and thump move the cab and a repeat does not", "[dsp]")
{
    const auto limits = doomgazer::makeBufferLimits(48000.0);
    doomgazer::Parameters stock;
    doomgazer::Coefficients first;
    doomgazer::Coefficients again;
    first.update(stock, limits);
    again.update(stock, limits);
    REQUIRE(first.cab1.b0 == again.cab1.b0);
    REQUIRE(first.thump.b0 == again.thump.b0);

    stock.highCutHz = 3400.0;
    doomgazer::Coefficients open;
    open.update(stock, limits);
    REQUIRE(open.cab1.b0 != first.cab1.b0);

    stock.highCutHz = 2700.0;
    stock.thumpDb = 8.0;
    doomgazer::Coefficients thumped;
    thumped.update(stock, limits);
    REQUIRE(thumped.thump.b0 != first.thump.b0);
}

TEST_CASE("freeze holds the reverb after the guitar stops", "[dsp]")
{
    constexpr int block = 128;
    constexpr double sr = 48000.0;
    auto renderTail = [&](bool frozen) {
        doomgazer::Processor processor;
        processor.prepare(sr, block);
        doomgazer::Parameters params;
        params.reverbMode = 1.0;
        params.reverbMix = 0.9;
        params.reverbDecay = 0.35;
        params.reverbSizeMs = 180.0;
        params.delayMode = 0.0;
        params.shroudOn = 0.0;
        params.limiterOn = 1.0;
        params.freeze = frozen ? 1.0 : 0.0;
        processor.setParameters(params);

        std::vector<double> left(block);
        std::vector<double> right(block);
        for (int i = 0; i < 80; ++i)
        {
            for (int n = 0; n < block; ++n)
            {
                const double t = static_cast<double>(i * block + n) / sr;
                left[static_cast<std::size_t>(n)] = 0.4 * std::sin(2.0 * doomgazer::kPi * 82.0 * t);
                right[static_cast<std::size_t>(n)] = left[static_cast<std::size_t>(n)];
            }
            processor.process(left.data(), right.data(), block, nullptr);
        }
        double energy = 0.0;
        for (int i = 0; i < 200; ++i)
        {
            std::fill(left.begin(), left.end(), 0.0);
            std::fill(right.begin(), right.end(), 0.0);
            processor.process(left.data(), right.data(), block, nullptr);
            if (i > 160)
            {
                for (double sample : left)
                    energy += sample * sample;
            }
        }
        return energy;
    };

    const double held = renderTail(true);
    const double open = renderTail(false);
    REQUIRE(held > open * 4.0);
    REQUIRE(held > 1.0e-4);
}

TEST_CASE("grit at zero leaves the drone unchanged", "[dsp]")
{
    doomgazer::Parameters plain;
    doomgazer::Parameters colored = plain;
    colored.gritCharacter = 1.0;
    colored.gritFreqHz = 2100.0;
    colored.gritLevelDb = 6.0;

    doomgazer::Processor dry;
    doomgazer::Processor bypass;
    dry.prepare(48000.0, 128);
    bypass.prepare(48000.0, 128);
    dry.reset();
    bypass.reset();
    dry.setParameters(plain);
    bypass.setParameters(colored);

    for (int block = 0; block < 8; ++block)
    {
        std::vector<double> left(128);
        std::vector<double> right(128);
        std::vector<double> leftB(128);
        std::vector<double> rightB(128);
        for (int i = 0; i < 128; ++i)
        {
            const double n = static_cast<double>(block * 128 + i);
            const double sample = 0.2 * std::sin(2.0 * doomgazer::kPi * 82.0 * n / 48000.0);
            left[static_cast<std::size_t>(i)] = sample;
            right[static_cast<std::size_t>(i)] = sample;
            leftB[static_cast<std::size_t>(i)] = sample;
            rightB[static_cast<std::size_t>(i)] = sample;
        }
        dry.process(left.data(), right.data(), 128, nullptr);
        bypass.process(leftB.data(), rightB.data(), 128, nullptr);
        for (int i = 0; i < 128; ++i)
        {
            REQUIRE_THAT(leftB[static_cast<std::size_t>(i)],
                         Catch::Matchers::WithinAbs(left[static_cast<std::size_t>(i)], 1.0e-9));
            REQUIRE_THAT(rightB[static_cast<std::size_t>(i)],
                         Catch::Matchers::WithinAbs(right[static_cast<std::size_t>(i)], 1.0e-9));
        }
    }
}

TEST_CASE("motion modules at zero leave the drone unchanged", "[dsp]")
{
    doomgazer::Parameters plain;
    doomgazer::Parameters driven = plain;
    driven.phaseRate = 1.4;
    driven.phaseDepth = 0.88;
    driven.phaseFeedback = 0.75;
    driven.chorusRate = 0.9;
    driven.chorusBaritone = 1.0;
    driven.swellAttack = 220.0;
    driven.howlPitch = 2.0;
    driven.howlSense = 0.95;

    doomgazer::Processor dry;
    doomgazer::Processor bypass;
    dry.prepare(48000.0, 128);
    bypass.prepare(48000.0, 128);
    dry.reset();
    bypass.reset();
    dry.setParameters(plain);
    bypass.setParameters(driven);

    for (int block = 0; block < 8; ++block)
    {
        std::vector<double> left(128);
        std::vector<double> right(128);
        std::vector<double> leftB(128);
        std::vector<double> rightB(128);
        for (int i = 0; i < 128; ++i)
        {
            const double n = static_cast<double>(block * 128 + i);
            const double sample = 0.2 * std::sin(2.0 * doomgazer::kPi * 82.0 * n / 48000.0);
            left[static_cast<std::size_t>(i)] = sample;
            right[static_cast<std::size_t>(i)] = sample;
            leftB[static_cast<std::size_t>(i)] = sample;
            rightB[static_cast<std::size_t>(i)] = sample;
        }
        dry.process(left.data(), right.data(), 128, nullptr);
        bypass.process(leftB.data(), rightB.data(), 128, nullptr);
        for (int i = 0; i < 128; ++i)
        {
            REQUIRE_THAT(leftB[static_cast<std::size_t>(i)],
                         Catch::Matchers::WithinAbs(left[static_cast<std::size_t>(i)], 1.0e-9));
            REQUIRE_THAT(rightB[static_cast<std::size_t>(i)],
                         Catch::Matchers::WithinAbs(right[static_cast<std::size_t>(i)], 1.0e-9));
        }
    }
}

TEST_CASE("howl stays finite with a hard cap", "[dsp]")
{
    doomgazer::Parameters params;
    params.engineMode = 1.0;
    params.howlAmount = 1.0;
    params.howlSense = 1.0;
    params.howlPitch = 0.0;
    params.gritAmount = 0.55;
    params.gritFreqHz = 700.0;

    doomgazer::Processor processor;
    processor.prepare(48000.0, 128);
    processor.reset();
    processor.setParameters(params);

    double peak = 0.0;
    for (int block = 0; block < 48; ++block)
    {
        std::vector<double> left(128);
        std::vector<double> right(128);
        for (int i = 0; i < 128; ++i)
        {
            const double n = static_cast<double>(block * 128 + i);
            const double drive = block < 24 ? 0.35 * std::sin(2.0 * doomgazer::kPi * 110.0 * n / 48000.0) : 0.0;
            left[static_cast<std::size_t>(i)] = drive;
            right[static_cast<std::size_t>(i)] = drive;
        }
        processor.process(left.data(), right.data(), 128, nullptr);
        for (int i = 0; i < 128; ++i)
        {
            REQUIRE(std::isfinite(left[static_cast<std::size_t>(i)]));
            REQUIRE(std::isfinite(right[static_cast<std::size_t>(i)]));
            peak = std::max(peak, std::max(std::abs(left[static_cast<std::size_t>(i)]),
                                           std::abs(right[static_cast<std::size_t>(i)])));
        }
    }
    REQUIRE(peak < 1.5);
}

TEST_CASE("howl at full amount keeps the dry note", "[dsp]")
{
    auto render = [](double howlAmount) {
        doomgazer::Parameters params;
        params.engineMode = 0.0;
        params.coreGainDb = 6.0;
        params.sustainOn = 0.0;
        params.delayMode = 0.0;
        params.reverbMode = 0.0;
        params.shroudOn = 0.0;
        params.limiterOn = 1.0;
        params.masterDb = -6.0;
        params.doublerOn = 0.0;
        params.howlAmount = howlAmount;
        params.howlSense = 1.0;
        params.howlPitch = 0.0;

        doomgazer::Processor processor;
        processor.prepare(48000.0, 128);
        processor.reset();
        processor.setParameters(params);

        double energy = 0.0;
        int counted = 0;
        for (int block = 0; block < 40; ++block)
        {
            std::vector<double> left(128);
            std::vector<double> right(128);
            for (int i = 0; i < 128; ++i)
            {
                const double n = static_cast<double>(block * 128 + i);
                const double drive = 0.20 * std::sin(2.0 * doomgazer::kPi * 110.0 * n / 48000.0);
                left[static_cast<std::size_t>(i)] = drive;
                right[static_cast<std::size_t>(i)] = drive;
            }
            processor.process(left.data(), right.data(), 128, nullptr);
            if (block < 8)
                continue;
            for (int i = 0; i < 128; ++i)
            {
                energy += left[static_cast<std::size_t>(i)] * left[static_cast<std::size_t>(i)];
                ++counted;
            }
        }
        return counted > 0 ? std::sqrt(energy / static_cast<double>(counted)) : 0.0;
    };

    const double dry = render(0.0);
    const double howling = render(1.0);
    REQUIRE(dry > 0.01);
    REQUIRE(howling > dry * 1.25);
}

TEST_CASE("factory tone presets use grit, wah, and spring", "[dsp]")
{
    REQUIRE(doomgazer::presetName(14) == "WOOL VEIL");
    REQUIRE(doomgazer::presetName(15) == "DRY CHOKE");
    REQUIRE(doomgazer::presetName(27) == "SPRING DRIP");
    REQUIRE(doomgazer::factoryPreset(0).gritAmount == 0.0);
    REQUIRE(doomgazer::factoryPreset(0).pedalOn == 0.0);
    REQUIRE(doomgazer::factoryPreset(24).gritAmount > 0.5);
    REQUIRE(doomgazer::factoryPreset(24).gritCharacter == 0.0);
    REQUIRE(doomgazer::factoryPreset(25).gritCharacter == 1.0);
    REQUIRE(doomgazer::factoryPreset(26).pedalOn == 1.0);
    REQUIRE(doomgazer::factoryPreset(26).pedalMode == 1.0);
    REQUIRE(doomgazer::factoryPreset(26).pedalToe == 0.81);
    REQUIRE(doomgazer::factoryPreset(27).reverbMode == 3.0);
    REQUIRE(doomgazer::factoryPreset(19).sync == 1.0);
    REQUIRE(doomgazer::factoryPreset(15).highCutHz == 3500.0);
    REQUIRE(doomgazer::factoryPreset(18).freeze == 0.0);
    REQUIRE(doomgazer::presetName(28) == "PHASE RITUAL");
    REQUIRE(doomgazer::presetName(32) == "SPEAKER HOWL");
    REQUIRE(doomgazer::factoryPreset(14).phaseMix == 0.0);
    REQUIRE(doomgazer::factoryPreset(27).chorusMix == 0.0);
    REQUIRE(doomgazer::factoryPreset(28).sync == 1.0);
    REQUIRE(doomgazer::factoryPreset(28).phaseMix > 0.0);
    REQUIRE(doomgazer::factoryPreset(29).chorusBaritone == 1.0);
    REQUIRE(doomgazer::factoryPreset(29).doublerOn == 0.0);
    REQUIRE(doomgazer::factoryPreset(30).gritCharacter == 0.0);
    REQUIRE(doomgazer::factoryPreset(30).reverbMode == 2.0);
    REQUIRE(doomgazer::factoryPreset(30).glideMode == 1.0);
    REQUIRE(doomgazer::factoryPreset(31).sync == 1.0);
    REQUIRE(doomgazer::factoryPreset(31).swellAmount > 0.0);
    REQUIRE(doomgazer::factoryPreset(32).howlAmount > 0.0);
    REQUIRE(doomgazer::factoryPreset(32).subSolo == 0.0);
}

TEST_CASE("spring reverb stays finite and audible", "[dsp]")
{
    doomgazer::Parameters params;
    params.reverbMode = 3.0;
    params.reverbMix = 0.45;
    params.reverbDecay = 0.40;
    params.reverbSizeMs = 240.0;
    params.engineMode = 1.0;

    doomgazer::Processor processor;
    processor.prepare(48000.0, 128);
    processor.reset();
    processor.setParameters(params);

    doomgazer::BlockMetrics metrics;
    std::vector<double> left(128);
    std::vector<double> right(128);
    for (int block = 0; block < 24; ++block)
    {
        for (int i = 0; i < 128; ++i)
        {
            const double n = static_cast<double>(block * 128 + i);
            left[static_cast<std::size_t>(i)] = 0.25 * std::sin(2.0 * doomgazer::kPi * 98.0 * n / 48000.0);
            right[static_cast<std::size_t>(i)] = left[static_cast<std::size_t>(i)];
        }
        processor.process(left.data(), right.data(), 128, &metrics);
    }
    REQUIRE(metrics.nonFinite == 0);
    REQUIRE(metrics.peak > 1.0e-3);
    REQUIRE(metrics.peak < 1.5);
}

TEST_CASE("doubler spread pans the original opposite the second contour", "[dsp]")
{
    constexpr int block = 128;
    constexpr double sr = 48000.0;

    struct Take
    {
        std::vector<double> left;
        std::vector<double> right;
    };

    auto render = [&](double doublerOn, double spread, double level = 1.0, double thump = 4.2) {
        doomgazer::Processor processor;
        processor.prepare(sr, block);
        processor.reset();
        doomgazer::Parameters params;
        params.engineMode = 1.0;
        params.doublerOn = doublerOn;
        params.doublerLevel = level;
        params.doublerSpread = spread;
        params.doublerThumpDb = thump;
        params.doublerHighCutHz = 2700.0;
        params.spectrumBlend = 1.0;
        params.shroudOn = 1.0;
        params.limiterOn = 0.0;
        params.sustainOn = 0.0;
        params.delayMode = 0.0;
        params.reverbMode = 0.0;
        params.tremoloOn = 0.0;
        processor.setParameters(params);

        Take take;
        std::vector<double> left(static_cast<std::size_t>(block));
        std::vector<double> right(static_cast<std::size_t>(block));
        const int total = static_cast<int>(sr);
        for (int n = 0; n < total; n += block)
        {
            for (int i = 0; i < block; ++i)
            {
                const double t = static_cast<double>(n + i) / sr;
                const double sample = 0.20 * std::sin(2.0 * doomgazer::kPi * 110.0 * t)
                    + 0.14 * std::sin(2.0 * doomgazer::kPi * 440.0 * t)
                    + 0.08 * std::sin(2.0 * doomgazer::kPi * 1320.0 * t);
                left[static_cast<std::size_t>(i)] = sample;
                right[static_cast<std::size_t>(i)] = sample;
            }
            processor.process(left.data(), right.data(), block, nullptr);
            if (n >= total / 2)
            {
                take.left.insert(take.left.end(), left.begin(), left.end());
                take.right.insert(take.right.end(), right.begin(), right.end());
            }
        }
        return take;
    };

    auto maxDelta = [](const Take& take) {
        double delta = 0.0;
        REQUIRE(take.left.size() == take.right.size());
        for (std::size_t i = 0; i < take.left.size(); ++i)
        {
            REQUIRE(std::isfinite(take.left[i]));
            REQUIRE(std::isfinite(take.right[i]));
            delta = std::max(delta, std::abs(take.left[i] - take.right[i]));
        }
        return delta;
    };

    const Take dry = render(0.0, 1.0);
    REQUIRE(maxDelta(dry) < 1.0e-9);

    const Take centered = render(1.0, 0.0);
    REQUIRE(maxDelta(centered) < 1.0e-9);

    const Take wide = render(1.0, 1.0, 1.0, 4.2);
    const Take dark = render(1.0, 1.0, 1.0, 12.0);
    const Take silent = render(1.0, 1.0, 0.0, 4.2);
    REQUIRE(maxDelta(dark) > 0.01);
    REQUIRE(wide.left.size() == dry.left.size());
    double leftDrift = 0.0;
    double rightEnergy = 0.0;
    double silentRight = 0.0;
    double toneOnLeft = 0.0;
    double toneOnRight = 0.0;
    for (std::size_t i = 0; i < wide.left.size(); ++i)
    {
        leftDrift = std::max(leftDrift, std::abs(dark.left[i] - dry.left[i]));
        rightEnergy += dark.right[i] * dark.right[i];
        silentRight = std::max(silentRight, std::abs(silent.right[i]));
        toneOnLeft = std::max(toneOnLeft, std::abs(wide.left[i] - dark.left[i]));
        toneOnRight = std::max(toneOnRight, std::abs(wide.right[i] - dark.right[i]));
    }
    REQUIRE(leftDrift < 1.0e-6);
    REQUIRE(rightEnergy > 1.0e-6);
    REQUIRE(silentRight < 1.0e-6);
    REQUIRE(toneOnLeft < 1.0e-6);
    REQUIRE(toneOnRight > 0.01);
}

TEST_CASE("doubler tone opens the second contour", "[dsp]")
{
    constexpr int block = 128;
    constexpr double sr = 48000.0;

    auto highEnergy = [&](double tone) {
        doomgazer::Processor processor;
        processor.prepare(sr, block);
        processor.reset();
        doomgazer::Parameters params;
        params.engineMode = 1.0;
        params.doublerOn = 1.0;
        params.doublerSpread = 0.0;
        params.doublerLevel = 1.0;
        params.doublerThumpDb = tone < 0.5 ? 12.0 : 0.0;
        params.doublerMidDb = tone < 0.5 ? 6.0 : -6.0;
        params.doublerHighCutHz = tone < 0.5 ? 1400.0 : 3600.0;
        params.doublerPresenceHz = tone < 0.5 ? 800.0 : 2200.0;
        params.shroudOn = 0.0;
        params.limiterOn = 0.0;
        params.sustainOn = 0.0;
        params.delayMode = 0.0;
        params.reverbMode = 0.0;
        params.tremoloOn = 0.0;
        processor.setParameters(params);

        const double pole = std::exp(-2.0 * doomgazer::kPi * 2200.0 / sr);
        double lowpass = 0.0;
        double energy = 0.0;
        std::vector<double> left(static_cast<std::size_t>(block));
        std::vector<double> right(static_cast<std::size_t>(block));
        const int total = static_cast<int>(sr);
        for (int n = 0; n < total; n += block)
        {
            for (int i = 0; i < block; ++i)
            {
                const double t = static_cast<double>(n + i) / sr;
                const double sample = 0.22 * std::sin(2.0 * doomgazer::kPi * 110.0 * t)
                    + 0.16 * std::sin(2.0 * doomgazer::kPi * 880.0 * t);
                left[static_cast<std::size_t>(i)] = sample;
                right[static_cast<std::size_t>(i)] = sample;
            }
            processor.process(left.data(), right.data(), block, nullptr);
            if (n < total / 2)
                continue;
            for (double sample : left)
            {
                lowpass = (1.0 - pole) * sample + pole * lowpass;
                const double high = sample - lowpass;
                energy += high * high;
            }
        }
        return energy;
    };

    const double dark = highEnergy(0.0);
    const double bright = highEnergy(1.0);
    REQUIRE(dark > 1.0e-6);
    REQUIRE(bright > dark * 1.2);
}

TEST_CASE("dual core sits with the harmonic core and keeps the low band in check", "[dsp]")
{
    constexpr int block = 128;
    constexpr double sr = 48000.0;

    auto render = [&](bool dual) {
        doomgazer::Processor processor;
        processor.prepare(sr, block);
        processor.reset();
        doomgazer::Parameters params;
        params.engineMode = dual ? 1.0 : 0.0;
        params.doublerOn = 0.0;
        params.limiterOn = 0.0;
        params.delayMode = 0.0;
        params.reverbMode = 0.0;
        params.tremoloOn = 0.0;
        processor.setParameters(params);

        const double pole = std::exp(-2.0 * doomgazer::kPi * 250.0 / sr);
        double low = 0.0;
        double full = 0.0;
        double lowState = 0.0;
        std::vector<double> left(static_cast<std::size_t>(block));
        std::vector<double> right(static_cast<std::size_t>(block));
        const int total = static_cast<int>(sr * 0.8);
        for (int n = 0; n < total; n += block)
        {
            for (int i = 0; i < block; ++i)
            {
                const double t = static_cast<double>(n + i) / sr;
                const double sample = 0.22 * std::sin(2.0 * doomgazer::kPi * 110.0 * t)
                    + 0.18 * std::sin(2.0 * doomgazer::kPi * 440.0 * t);
                left[static_cast<std::size_t>(i)] = sample;
                right[static_cast<std::size_t>(i)] = sample;
            }
            processor.process(left.data(), right.data(), block, nullptr);
            if (n < total / 2)
                continue;
            for (int i = 0; i < block; ++i)
            {
                const double sample = 0.5 * (left[static_cast<std::size_t>(i)] + right[static_cast<std::size_t>(i)]);
                lowState = (1.0 - pole) * sample + pole * lowState;
                low += lowState * lowState;
                full += sample * sample;
            }
        }
        return std::pair<double, double> { std::sqrt(full), low / std::max(full, 1.0e-18) };
    };

    const auto dual = render(true);
    const auto harmonic = render(false);
    const double dualDb = 20.0 * std::log10(std::max(dual.first, 1.0e-12));
    const double harmDb = 20.0 * std::log10(std::max(harmonic.first, 1.0e-12));
    INFO("dual " << dualDb << " dB, harmonic " << harmDb << " dB, low share " << dual.second);
    REQUIRE(dualDb < harmDb + 10.0);
    REQUIRE(dualDb > harmDb - 2.0);
    REQUIRE(dual.second < 0.62);
}

TEST_CASE("mono sum helper respects invert R", "[dsp]")
{
    REQUIRE_THAT(doomgazer::monoSum(1.0, 1.0, false), Catch::Matchers::WithinAbs(1.0, 1.0e-12));
    REQUIRE_THAT(doomgazer::monoSum(1.0, 1.0, true), Catch::Matchers::WithinAbs(0.0, 1.0e-12));
    REQUIRE_THAT(doomgazer::monoSum(0.6, 0.4, false), Catch::Matchers::WithinAbs(0.5, 1.0e-12));
}

TEST_CASE("delay mono folds echo to both channels", "[dsp]")
{
    constexpr int block = 128;
    constexpr double sr = 48000.0;
    doomgazer::Processor processor;
    processor.prepare(sr, block);
    processor.reset();
    doomgazer::Parameters params;
    params.engineMode = 1.0;
    params.doublerOn = 0.0;
    params.shroudOn = 0.0;
    params.limiterOn = 0.0;
    params.delayMode = 1.0;
    params.delayMix = 0.65;
    params.delayMs = 220.0;
    params.delayStereo = 0.0;
    params.reverbMode = 0.0;
    params.tremoloOn = 0.0;
    processor.setParameters(params);

    std::vector<double> left(static_cast<std::size_t>(block));
    std::vector<double> right(static_cast<std::size_t>(block));
    double maxDelta = 0.0;
    const int total = static_cast<int>(sr * 0.6);
    for (int n = 0; n < total; n += block)
    {
        for (int i = 0; i < block; ++i)
        {
            const double t = static_cast<double>(n + i) / sr;
            left[static_cast<std::size_t>(i)] = 0.25 * std::sin(2.0 * doomgazer::kPi * 330.0 * t);
            right[static_cast<std::size_t>(i)] = 0.25 * std::sin(2.0 * doomgazer::kPi * 440.0 * t);
        }
        processor.process(left.data(), right.data(), block, nullptr);
        if (n >= total / 3)
        {
            for (int i = 0; i < block; ++i)
                maxDelta = std::max(maxDelta, std::abs(left[static_cast<std::size_t>(i)]
                                                       - right[static_cast<std::size_t>(i)]));
        }
    }
    REQUIRE(maxDelta < 1.0e-6);
}

TEST_CASE("PA mono policy duplicates outputs", "[dsp]")
{
    constexpr int block = 128;
    constexpr double sr = 48000.0;
    doomgazer::Processor processor;
    processor.prepare(sr, block);
    processor.reset();
    processor.setPaMonoPolicy(true, false);
    doomgazer::Parameters params;
    params.engineMode = 1.0;
    params.doublerOn = 1.0;
    params.doublerSpread = 1.0;
    params.delayMode = 1.0;
    params.delayMix = 0.5;
    params.delayStereo = 1.0;
    params.shroudOn = 0.0;
    params.limiterOn = 0.0;
    processor.setParameters(params);

    std::vector<double> left(static_cast<std::size_t>(block));
    std::vector<double> right(static_cast<std::size_t>(block));
    double maxDelta = 0.0;
    const int total = static_cast<int>(sr * 0.5);
    for (int n = 0; n < total; n += block)
    {
        for (int i = 0; i < block; ++i)
        {
            const double t = static_cast<double>(n + i) / sr;
            const double sample = 0.3 * std::sin(2.0 * doomgazer::kPi * 220.0 * t);
            left[static_cast<std::size_t>(i)] = sample;
            right[static_cast<std::size_t>(i)] = sample;
        }
        processor.process(left.data(), right.data(), block, nullptr);
        if (n >= total / 2)
        {
            for (int i = 0; i < block; ++i)
                maxDelta = std::max(maxDelta, std::abs(left[static_cast<std::size_t>(i)]
                                                       - right[static_cast<std::size_t>(i)]));
        }
    }
    REQUIRE(maxDelta < 1.0e-8);
}

TEST_CASE("doubler mono stays centred and quieter than a hard pan", "[dsp]")
{
    constexpr int block = 128;
    constexpr double sr = 48000.0;

    auto render = [&](double doublerOn, double stereo) {
        doomgazer::Processor processor;
        processor.prepare(sr, block);
        processor.reset();
        doomgazer::Parameters params;
        params.engineMode = 1.0;
        params.doublerOn = doublerOn;
        params.doublerLevel = 0.50;
        params.doublerSpread = stereo;
        params.shroudOn = 1.0;
        params.limiterOn = 0.0;
        params.sustainOn = 0.0;
        params.delayMode = 0.0;
        params.reverbMode = 0.0;
        params.tremoloOn = 0.0;
        processor.setParameters(params);

        double energy = 0.0;
        double maxDelta = 0.0;
        int counted = 0;
        std::vector<double> left(static_cast<std::size_t>(block));
        std::vector<double> right(static_cast<std::size_t>(block));
        const int total = static_cast<int>(sr);
        for (int n = 0; n < total; n += block)
        {
            for (int i = 0; i < block; ++i)
            {
                const double t = static_cast<double>(n + i) / sr;
                const double sample = 0.28 * std::sin(2.0 * doomgazer::kPi * 110.0 * t)
                    + 0.12 * std::sin(2.0 * doomgazer::kPi * 330.0 * t);
                left[static_cast<std::size_t>(i)] = sample;
                right[static_cast<std::size_t>(i)] = sample;
            }
            processor.process(left.data(), right.data(), block, nullptr);
            if (n < total / 2)
                continue;
            for (int i = 0; i < block; ++i)
            {
                const double l = left[static_cast<std::size_t>(i)];
                const double r = right[static_cast<std::size_t>(i)];
                maxDelta = std::max(maxDelta, std::abs(l - r));
                const double m = 0.5 * (l + r);
                energy += m * m;
                ++counted;
            }
        }
        const double rms = counted > 0 ? std::sqrt(energy / static_cast<double>(counted)) : 0.0;
        return std::pair<double, double> { rms, maxDelta };
    };

    const auto dry = render(0.0, 0.0);
    const auto mono = render(1.0, 0.0);
    const auto wide = render(1.0, 1.0);
    REQUIRE(dry.first > 0.01);
    REQUIRE(mono.second < 1.0e-6);
    REQUIRE(mono.first > dry.first * 1.04);
    REQUIRE(mono.first < dry.first * 1.60);
    REQUIRE(wide.second > 0.02);
}
TEST_CASE("preset text round-trips every parameter at its step", "[preset]")
{
    doomgazer::PresetDocument document;
    document.name = "DOOM TEST";
    document.category = "Doom";
    document.notes = "A note = with an equals sign.";
    document.params = doomgazer::factoryPreset(24);

    const std::string text = doomgazer::writePresetText(document);
    REQUIRE(text.rfind("doomgazer-preset 1\n", 0) == 0);
    REQUIRE(text.find("name=DOOM TEST") != std::string::npos);
    REQUIRE(text.find("notes=A note = with an equals sign.") != std::string::npos);

    doomgazer::PresetDocument readBack;
    const doomgazer::PresetParseReport report = doomgazer::readPresetText(text, readBack);
    REQUIRE(report.ok());
    REQUIRE(report.unknownKeys == 0);
    REQUIRE(report.ignoredValues == 0);
    REQUIRE(readBack.name == "DOOM TEST");
    REQUIRE(readBack.category == "Doom");
    REQUIRE(readBack.notes == "A note = with an equals sign.");
    REQUIRE(readBack.program == -1);
    REQUIRE(doomgazer::parametersMatch(readBack.params, document.params));

    document.program = 12;
    const std::string coded = doomgazer::writePresetText(document);
    REQUIRE(coded.find("pc=12\n") != std::string::npos);
    REQUIRE(doomgazer::readPresetText(coded, readBack).ok());
    REQUIRE(readBack.program == 12);
    REQUIRE(doomgazer::readPresetText("doomgazer-preset 1\nname=NO PC\n", readBack).ok());
    REQUIRE(readBack.program == -1);
}

TEST_CASE("preset text snaps values and clamps on read", "[preset]")
{
    doomgazer::PresetDocument document;
    document.params = doomgazer::Parameters {};
    document.params.thumpDb = 4.2000000001;
    // Out of range on purpose: a hand-edited file must not carry it through.
    const std::string text = doomgazer::writePresetText(document);
    REQUIRE(text.find("thump=4.2\n") != std::string::npos);

    std::string edited = text;
    const std::string key = "highCut=";
    const auto at = edited.find(key);
    REQUIRE(at != std::string::npos);
    const auto end = edited.find('\n', at);
    edited.replace(at, end - at, "highCut=999999");

    doomgazer::PresetDocument readBack;
    REQUIRE(doomgazer::readPresetText(edited, readBack).ok());
    REQUIRE(readBack.params.highCutHz == 3600.0);
}

TEST_CASE("preset text rejects what it cannot read", "[preset]")
{
    doomgazer::PresetDocument document;
    document.params = doomgazer::factoryPreset(3);

    REQUIRE(doomgazer::readPresetText("", document).result
            == doomgazer::PresetParseResult::empty);
    REQUIRE(doomgazer::readPresetText("not a preset\n", document).result
            == doomgazer::PresetParseResult::notPreset);
    REQUIRE(doomgazer::readPresetText("doomgazer-preset 2\nname=X\n", document).result
            == doomgazer::PresetParseResult::unsupportedVersion);
    REQUIRE(doomgazer::readPresetText("subdoom-preset 1\nname=X\n", document).result
            == doomgazer::PresetParseResult::notPreset);

    // Unknown keys are counted, missing keys keep the defaults, and a bad value
    // is ignored rather than becoming zero.
    const auto report = doomgazer::readPresetText(
        "doomgazer-preset 1\nname=MISSING KEYS\nnotAParameter=3\nthump=loud\n", document);
    REQUIRE(report.ok());
    REQUIRE(report.unknownKeys == 1);
    REQUIRE(report.ignoredValues == 1);
    REQUIRE(document.params.thumpDb == doomgazer::Parameters {}.thumpDb);
    REQUIRE(document.params.subWeight == doomgazer::Parameters {}.subWeight);
}

TEST_CASE("the parameter table is stable and self-consistent", "[preset]")
{
    REQUIRE(doomgazer::kParameterCount == 100);
    REQUIRE(std::string(doomgazer::parameterId(1)) == "engine");
    REQUIRE(std::string(doomgazer::parameterId(14)) == "doubler");
    REQUIRE(std::string(doomgazer::parameterId(16)) == "doublerLevel");
    REQUIRE(std::string(doomgazer::parameterId(18)) == "doublerThump");
    REQUIRE(std::string(doomgazer::parameterId(72)) == "gritLevel");
    REQUIRE(std::string(doomgazer::parameterId(73)) == "phaseRate");
    REQUIRE(std::string(doomgazer::parameterId(95)) == "pedalMode");
    REQUIRE(std::string(doomgazer::parameterId(96)) == "twinLevel");
    REQUIRE(std::string(doomgazer::parameterId(100)) == "doublerPresence");
    REQUIRE(std::string(doomgazer::parameterName(24)) == "Master");
    REQUIRE(std::string(doomgazer::parameterHostName(24)) == "Master");
    REQUIRE(std::string(doomgazer::parameterHostName(65)) == "Twin Octave");
    REQUIRE(doomgazer::parameterKind(1) == doomgazer::ParameterKind::choice);
    REQUIRE(doomgazer::parameterKind(36) == doomgazer::ParameterKind::toggle);
    REQUIRE(doomgazer::parameterKind(24) == doomgazer::ParameterKind::continuous);
    REQUIRE_FALSE(doomgazer::parameterIsAutomatable(25));
    REQUIRE_FALSE(doomgazer::parameterIsAutomatable(26));
    REQUIRE_FALSE(doomgazer::parameterIsAutomatable(27));
    REQUIRE(doomgazer::parameterIsAutomatable(24));

    const doomgazer::ChoiceList delay = doomgazer::parameterChoices(37);
    REQUIRE(delay.count == 3);
    REQUIRE(std::string(delay.names[0]) == "Off");
    REQUIRE(std::string(delay.names[1]) == "Tape Echo");
    REQUIRE(std::string(delay.names[2]) == "Reverse");
    const doomgazer::ChoiceList reverb = doomgazer::parameterChoices(43);
    REQUIRE(reverb.count == 4);
    REQUIRE(std::string(reverb.names[1]) == "Abyss");
    const doomgazer::ChoiceList shape = doomgazer::parameterChoices(53);
    REQUIRE(shape.count == 4);
    REQUIRE(std::string(shape.names[0]) == "Tube Bias");
    const doomgazer::ChoiceList glide = doomgazer::parameterChoices(59);
    REQUIRE(glide.count == 3);
    const doomgazer::ChoiceList howl = doomgazer::parameterChoices(82);
    REQUIRE(howl.count == 4);
    REQUIRE(std::string(howl.names[0]) == "H2");

    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
    {
        const auto range = doomgazer::parameterRange(index);
        REQUIRE(range.min <= range.max);
        REQUIRE(range.step > 0.0);
        REQUIRE(std::string(doomgazer::parameterId(index)) != "unused");
        // Every switch really is a two-position parameter, and nothing else is.
        if (doomgazer::parameterIsSwitch(index))
        {
            REQUIRE(range.min == 0.0);
            REQUIRE(range.max == 1.0);
            REQUIRE(range.step == 1.0);
        }
        const doomgazer::ChoiceList choices = doomgazer::parameterChoices(index);
        if (doomgazer::parameterKind(index) == doomgazer::ParameterKind::choice)
        {
            const int span = static_cast<int>(std::lround((range.max - range.min) / range.step)) + 1;
            REQUIRE(choices.count == span);
            REQUIRE(choices.names != nullptr);
        }
        else
        {
            REQUIRE(choices.count == 0);
        }
    }
}

TEST_CASE("every factory preset carries a category", "[preset]")
{
    for (int index = 0; index < doomgazer::kFactoryPresetCount; ++index)
    {
        REQUIRE(std::string(doomgazer::presetName(index)).size() > 0);
        REQUIRE(std::string(doomgazer::presetCategory(index)).size() > 0);
    }
    REQUIRE(std::string(doomgazer::presetCategory(0)) == "Doom");
    REQUIRE(std::string(doomgazer::presetCategory(8)) == "Noise");
}

TEST_CASE("tuner locks the open low and high strings", "[tuner]")
{
    const auto tune = [](double freq, int midi, double fundamental, double second, double third) {
        doomgazer::Processor processor;
        processor.prepare(48000.0, 256);
        processor.reset();
        std::vector<double> left(256);
        std::vector<double> right(256);
        for (int block = 0; block < 240; ++block)
        {
            for (int i = 0; i < 256; ++i)
            {
                const double t = static_cast<double>(block * 256 + i) / 48000.0;
                const double sample = fundamental * std::sin(2.0 * doomgazer::kPi * freq * t)
                                    + second * std::sin(2.0 * doomgazer::kPi * freq * 2.0 * t)
                                    + third * std::sin(2.0 * doomgazer::kPi * freq * 3.0 * t);
                left[static_cast<std::size_t>(i)] = sample;
                right[static_cast<std::size_t>(i)] = sample;
            }
            processor.process(left.data(), right.data(), 256, nullptr);
        }
        processor.pollTuner();
        const auto reading = processor.tuner();
        INFO(freq << " Hz -> " << reading.hz << " midi " << reading.midiNote);
        REQUIRE(reading.active);
        REQUIRE(reading.midiNote == midi);
        REQUIRE(std::abs(reading.hz - freq) < freq * 0.03);
    };

    // Low strings, with a stronger octave the way a wound string speaks.
    tune(55.00, 33, 0.16, 0.22, 0.08);   // A1, A standard
    tune(58.27, 34, 0.22, 0.40, 0.0);    // A#1 with a louder octave
    tune(58.27, 34, 0.16, 0.22, 0.08);   // A#1, drop A#
    tune(65.41, 36, 0.16, 0.22, 0.08);   // C2, C standard
    // Plain high strings: the fundamental is the weaker partial.
    tune(220.00, 57, 0.06, 0.18, 0.10);  // A3, A standard
    tune(261.63, 60, 0.06, 0.18, 0.10);  // C4, C standard and drop A#
}