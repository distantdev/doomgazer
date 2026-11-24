#pragma once

#include "doomgazer/BiquadFilter.h"
#include "doomgazer/Coefficients.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace doomgazer {

struct TunerReadout
{
    bool active = false;
    double hz = 0.0;
    double cents = 0.0;
    int midiNote = -1;
    int noteIndex = -1;
    int octave = 0;
};

inline const char* noteName(int index) noexcept
{
    switch (index)
    {
        case 0: return "C";
        case 1: return "C#";
        case 2: return "D";
        case 3: return "D#";
        case 4: return "E";
        case 5: return "F";
        case 6: return "F#";
        case 7: return "G";
        case 8: return "G#";
        case 9: return "A";
        case 10: return "A#";
        case 11: return "B";
        default: return "--";
    }
}

// Pre-distortion pitch tracker. The audio thread only fills a decimated
// buffer. YIN runs from the UI timer or the test harness, not per sample.
class Tuner
{
public:
    void reset() noexcept
    {
        hp.reset();
        lp.reset();
        buffer.fill(0.0);
        bassBuffer.fill(0.0);
        bassState = 0.0;
        bassState2 = 0.0;
        centsHist.fill(0.0);
        write = 0;
        decimCount = 0;
        env = 0.0;
        envPublished.store(0.0, std::memory_order_relaxed);
        readout = {};
        candidate = -1;
        candidateCount = 0;
        histIndex = 0;
        histCount = 0;
        silenceCount = 0;
        smoothCents = 0.0;
        sampleRate = 12000.0;
        feedSampleRate = -1.0;
        bassFollow = 0.0;
        snapshotFull.fill(0.0);
        snapshotBass.fill(0.0);
        snapshotSequence.store(0, std::memory_order_release);
        lastAnalysedSnapshot = 0;
    }

    void feed(double mono, const Coefficients& c) noexcept
    {
        sampleRate = c.tunerSampleRate;
        const double highpassed = hp.process(mono, c.tunerHpA);
        const double lowpassed = lp.process(highpassed, c.tunerLp);
        if (c.sampleRate != feedSampleRate)
        {
            feedSampleRate = c.sampleRate;
            bassFollow = 1.0 - std::exp(-2.0 * kPi * 90.0 / std::max(8000.0, c.sampleRate));
        }
        bassState += bassFollow * (lowpassed - bassState);
        bassState2 += bassFollow * (bassState - bassState2);
        env = env * 0.9992 + std::abs(mono) * 0.0008;
        envPublished.store(env, std::memory_order_relaxed);
        if (++decimCount >= c.tunerDecim)
        {
            decimCount = 0;
            buffer[static_cast<std::size_t>(write & 1023)] = lowpassed;
            bassBuffer[static_cast<std::size_t>(write & 1023)] = bassState2;
            write = (write + 1) & 1023;
            if (write == 0)
                publishSnapshot();
        }
    }

    double envelope() const noexcept { return envPublished.load(std::memory_order_relaxed); }

    uint32_t snapshotSerial() const noexcept
    {
        return snapshotSequence.load(std::memory_order_acquire);
    }

    TunerReadout reading() const noexcept
    {
        for (int attempt = 0; attempt < 4; ++attempt)
        {
            const uint32_t ticket = sequence.load(std::memory_order_acquire);
            if ((ticket & 1u) != 0u)
                continue;
            const TunerReadout copy = readout;
            if (sequence.load(std::memory_order_acquire) == ticket)
                return copy;
        }
        return {};
    }

    void noteSilence() noexcept
    {
        sequence.fetch_add(1u, std::memory_order_release);
        if (++silenceCount > 12)
        {
            readout.active = false;
            readout.midiNote = -1;
            readout.noteIndex = -1;
            readout.cents = 0.0;
            candidate = -1;
            candidateCount = 0;
            smoothCents = 0.0;
        }
        sequence.fetch_add(1u, std::memory_order_release);
    }

    void analyse() noexcept
    {
        const uint32_t published = snapshotSequence.load(std::memory_order_acquire);
        if (published == 0u || (published & 1u) != 0u || published == lastAnalysedSnapshot)
            return;

        // The audio thread publishes a complete ring at each wrap. Copy it under
        // the sequence check so YIN never reads samples being overwritten.
        std::array<double, 1024> full {};
        std::array<double, 1024> bass {};
        bool copied = false;
        for (int attempt = 0; attempt < 4 && !copied; ++attempt)
        {
            const uint32_t before = snapshotSequence.load(std::memory_order_acquire);
            if (before == 0u || (before & 1u) != 0u)
                continue;
            full = snapshotFull;
            bass = snapshotBass;
            const uint32_t after = snapshotSequence.load(std::memory_order_acquire);
            if (before == after)
            {
                lastAnalysedSnapshot = after;
                copied = true;
            }
        }
        if (!copied)
            return;

        sequence.fetch_add(1u, std::memory_order_release);
        silenceCount = 0;
        const auto bassHit = search(bass, 0, tauSamples(120.0), tauSamples(40.0), 512, 0.28);
        const auto fullHit = search(full, 0, tauSamples(500.0), tauSamples(45.0), 400, 0.20);
        const double bassLevel = meanAbs(bass, 0, 512);
        const double fullLevel = meanAbs(full, 0, 400);
        // A high string is periodic at half its pitch too. Require the low
        // band to carry the note before that half-pitch reading wins.
        const bool bassLoud = bassLevel > 0.004 && bassLevel > fullLevel * 0.35;
        const bool bassOk = bassHit.ok && bassLoud && bassHit.hz < 125.0;
        const bool fullIsOctave = bassOk && fullHit.ok && std::abs(fullHit.hz - bassHit.hz * 2.0) < bassHit.hz * 0.08;
        PitchHit chosen = fullHit;
        if (bassOk && (!fullHit.ok || fullIsOctave || bassHit.hz < 100.0))
            chosen = bassHit;
        if (!chosen.ok)
        {
            sequence.fetch_add(1u, std::memory_order_release);
            return;
        }

        const double hz = chosen.hz;
        if (hz <= 25.0 || hz >= 460.0)
        {
            sequence.fetch_add(1u, std::memory_order_release);
            return;
        }

        const double midi = 69.0 + 12.0 * (std::log(hz / 440.0) / std::log(2.0));
        int note = static_cast<int>(std::floor(midi + 0.5));
        int semitone = note % 12;
        if (semitone < 0)
            semitone += 12;
        const int octave = static_cast<int>(std::floor(static_cast<double>(note) / 12.0)) - 1;
        const double rawCents = (midi - static_cast<double>(note)) * 100.0;

        if (note == candidate)
            ++candidateCount;
        else
        {
            candidate = note;
            candidateCount = 1;
        }

        if (candidateCount >= 2 || readout.midiNote < 0)
        {
            if (readout.midiNote != note)
            {
                histCount = 0;
                histIndex = 0;
                smoothCents = rawCents;
            }
            readout.midiNote = note;
            readout.noteIndex = semitone;
            readout.octave = octave;
        }

        centsHist[static_cast<std::size_t>(histIndex)] = rawCents;
        histIndex = (histIndex + 1) % 4;
        histCount = std::min(4, histCount + 1);
        double sum = 0.0;
        for (int i = 0; i < histCount; ++i)
            sum += centsHist[static_cast<std::size_t>(i)];
        const double average = sum / static_cast<double>(histCount);
        smoothCents = smoothCents * 0.65 + average * 0.35;

        readout.active = true;
        readout.hz = hz;
        readout.cents = smoothCents;
        sequence.fetch_add(1u, std::memory_order_release);
    }

private:
    void publishSnapshot() noexcept
    {
        snapshotSequence.fetch_add(1, std::memory_order_release);
        snapshotFull = buffer;
        snapshotBass = bassBuffer;
        snapshotSequence.fetch_add(1, std::memory_order_release);
    }

    struct PitchHit
    {
        bool ok = false;
        double hz = 0.0;
    };

    int tauSamples(double hz) const noexcept
    {
        if (sampleRate < 1000.0 || hz <= 1.0)
            return 32;
        const int tau = static_cast<int>(sampleRate / hz);
        if (tau < 8)
            return 8;
        if (tau > 460)
            return 460;
        return tau;
    }

    double difference(const std::array<double, 1024>& source, int origin, int tau, int window) const noexcept
    {
        double diff = 0.0;
        for (int j = 0; j < window; ++j)
        {
            const double a = source[static_cast<std::size_t>((origin + j) & 1023)];
            const double b = source[static_cast<std::size_t>((origin + j + tau) & 1023)];
            diff += std::abs(a - b);
        }
        return diff;
    }

    double meanAbs(const std::array<double, 1024>& source, int origin, int window) const noexcept
    {
        double sum = 0.0;
        for (int j = 0; j < window; ++j)
            sum += std::abs(source[static_cast<std::size_t>((origin + j) & 1023)]);
        return sum / static_cast<double>(window);
    }

    PitchHit search(const std::array<double, 1024>& source, int origin, int tauMin, int tauMax,
                    int window, double threshold) const noexcept
    {
        if (tauMin < 2)
            tauMin = 2;
        if (tauMax > 480)
            tauMax = 480;
        if (window + tauMax > 1000)
            window = 1000 - tauMax;
        if (tauMax <= tauMin || window < 32)
            return {};

        double running = 0.0;
        int bestTau = -1;
        double bestNorm = 1.0;
        for (int tau = 1; tau <= tauMax && bestTau < 0; ++tau)
        {
            const double diff = difference(source, origin, tau, window);
            running += diff;
            const double norm = running > 0.0 ? diff / (running / static_cast<double>(tau)) : 1.0;
            if (tau < tauMin || norm >= threshold)
                continue;

            bestTau = tau;
            bestNorm = norm;
            while (bestTau + 1 <= tauMax)
            {
                const double nextDiff = difference(source, origin, bestTau + 1, window);
                const double nextRunning = running + nextDiff;
                const double nextNorm = nextRunning > 0.0
                    ? nextDiff / (nextRunning / static_cast<double>(bestTau + 1))
                    : 1.0;
                if (nextNorm < bestNorm)
                {
                    ++bestTau;
                    running = nextRunning;
                    bestNorm = nextNorm;
                }
                else
                {
                    break;
                }
            }
        }

        if (bestTau < tauMin)
            return {};

        const double y1 = difference(source, origin, bestTau - 1, window);
        const double y2 = difference(source, origin, bestTau, window);
        const double y3 = difference(source, origin, bestTau + 1, window);
        const double denom = 2.0 * (y1 - 2.0 * y2 + y3);
        double delta = std::abs(denom) > 0.00001 ? (y1 - y3) / denom : 0.0;
        delta = clamp(delta, -1.0, 1.0);
        const double tauSub = static_cast<double>(bestTau) + delta;
        if (tauSub <= 1.0)
            return {};
        const double hz = sampleRate / tauSub;
        if (hz <= 25.0 || hz >= 500.0)
            return {};
        return { true, hz };
    }

    OnePoleHp hp;
    Biquad lp;
    std::array<double, 1024> buffer {};
    std::array<double, 1024> bassBuffer {};
    double bassState = 0.0;
    double bassState2 = 0.0;
    std::array<double, 4> centsHist {};
    int write = 0;
    int decimCount = 0;
    double env = 0.0;
    double sampleRate = 12000.0;
    double feedSampleRate = -1.0;
    double bassFollow = 0.0;
    double smoothCents = 0.0;
    int candidate = -1;
    int candidateCount = 0;
    int histIndex = 0;
    int histCount = 0;
    int silenceCount = 0;
    TunerReadout readout;
    std::atomic<uint32_t> sequence { 0 };
    std::array<double, 1024> snapshotFull {};
    std::array<double, 1024> snapshotBass {};
    std::atomic<uint32_t> snapshotSequence { 0 };
    uint32_t lastAnalysedSnapshot = 0;
    std::atomic<double> envPublished { 0.0 };
};

} // namespace doomgazer
