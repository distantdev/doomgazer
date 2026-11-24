#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace doomgazer {

// Display-only helpers fed from the audio thread (decimated).
struct ScopeMeter
{
    static constexpr int kScopeRing = 2048;
    static constexpr int kSpectrumBands = 28;

    void reset() noexcept
    {
        scopeDecim = 0;
        for (auto& sample : inputScope)
            sample = 0.0f;
        inputScopeIndex.store(0, std::memory_order_relaxed);
        for (auto& sample : outputScope)
            sample = 0.0f;
        outputScopeIndex.store(0, std::memory_order_relaxed);
        spectrumCountdown = 0;
        spectrumLadder.fill(0.0);
        spectrumEnergy.fill(0.0);
        for (auto& level : spectrumLevels)
            level = 0.0f;
        preTrimPeak.store(0.0f, std::memory_order_relaxed);
        postTrimPeak.store(0.0f, std::memory_order_relaxed);
    }

    // Block peaks since the UI last took them. The UI exchange clears the slot.
    void publishInputLevel(float preTrim, float postTrim) noexcept
    {
        raisePeak(preTrimPeak, preTrim);
        raisePeak(postTrimPeak, postTrim);
    }

    float takePreTrimPeak() noexcept
    {
        return preTrimPeak.exchange(0.0f, std::memory_order_acq_rel);
    }

    float takePostTrimPeak() noexcept
    {
        return postTrimPeak.exchange(0.0f, std::memory_order_acq_rel);
    }

    void pushInput(float sample) noexcept
    {
        if (++scopeDecim < kScopeDecimation)
            return;
        scopeDecim = 0;
        pushRing(inputScope, inputScopeIndex, sample);
        pushSpectrum(static_cast<double>(sample));
    }

    void pushOutput(float sample) noexcept
    {
        if (++outputDecim < kScopeDecimation)
            return;
        outputDecim = 0;
        pushRing(outputScope, outputScopeIndex, sample);
    }

    int inputWriteIndex() const noexcept { return inputScopeIndex.load(std::memory_order_acquire); }
    int outputWriteIndex() const noexcept { return outputScopeIndex.load(std::memory_order_acquire); }

    const std::array<float, kScopeRing>& inputBuffer() const noexcept { return inputScope; }
    const std::array<float, kScopeRing>& outputBuffer() const noexcept { return outputScope; }
    const std::array<float, kSpectrumBands>& spectrum() const noexcept { return spectrumLevels; }

private:
    static constexpr int kScopeDecimation = 6;

    std::array<float, kScopeRing> inputScope {};
    std::atomic<int> inputScopeIndex { 0 };
    std::array<float, kScopeRing> outputScope {};
    std::atomic<int> outputScopeIndex { 0 };
    int scopeDecim = 0;
    int outputDecim = 0;

    std::array<float, kSpectrumBands> spectrumLevels {};
    std::array<double, kSpectrumBands> spectrumEnergy {};
    std::array<double, 5> spectrumLadder {};
    int spectrumCountdown = 0;
    std::atomic<float> preTrimPeak { 0.0f };
    std::atomic<float> postTrimPeak { 0.0f };

    static void raisePeak(std::atomic<float>& slot, float sample) noexcept
    {
        float previous = slot.load(std::memory_order_relaxed);
        while (sample > previous
               && !slot.compare_exchange_weak(previous, sample, std::memory_order_release,
                                              std::memory_order_relaxed))
        {
        }
    }

    static void pushRing(std::array<float, kScopeRing>& ring, std::atomic<int>& index, float sample) noexcept
    {
        const int at = index.load(std::memory_order_relaxed);
        ring[static_cast<std::size_t>(at)] = sample;
        index.store((at + 1) % kScopeRing, std::memory_order_release);
    }

    void pushSpectrum(double mono) noexcept
    {
        if (--spectrumCountdown > 0)
            return;
        spectrumCountdown = 6;

        const double instant = std::abs(mono);
        double node = mono;
        for (std::size_t stage = 0; stage < spectrumLadder.size(); ++stage)
        {
            spectrumLadder[stage] += 0.07 * (node - spectrumLadder[stage]);
            node = spectrumLadder[stage];
        }

        for (int band = 0; band < kSpectrumBands; ++band)
        {
            const double t = static_cast<double>(band) / static_cast<double>(kSpectrumBands - 1);
            const std::size_t stage = std::min(spectrumLadder.size() - 1,
                                                 static_cast<std::size_t>(t * static_cast<double>(spectrumLadder.size())));
            const double pick = (band < 4)
                                    ? instant
                                    : std::max(instant * 0.35, std::abs(spectrumLadder[stage]));
            spectrumEnergy[static_cast<std::size_t>(band)] =
                0.90 * spectrumEnergy[static_cast<std::size_t>(band)] + 0.10 * pick;
            const double shaped = std::sqrt(spectrumEnergy[static_cast<std::size_t>(band)] + 1.0e-12);
            spectrumLevels[static_cast<std::size_t>(band)] =
                static_cast<float>(std::min(1.0, shaped * 7.5));
        }
    }
};

} // namespace doomgazer
