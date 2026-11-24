#pragma once

#include "doomgazer/MathConstants.h"

#include <cmath>

namespace doomgazer {

// Half-rate divider. A slow filter watches the low string (around drop A#, 58 Hz)
// so a louder second harmonic cannot hold the lock. Higher notes fall through
// to the faster filter.
class OctaveDivider
{
public:
    void reset() noexcept
    {
        slowWave = 0.0;
        slowWave2 = 0.0;
        fastWave = 0.0;
        bass.reset();
        fast.reset();
    }

    double process(double input, double sampleRate, int semitones = -12) noexcept
    {
        if (sampleRate < 8000.0)
            return 0.0;

        const int steps = static_cast<int>(clamp(static_cast<double>(semitones), -12.0, 12.0));
        const double ratio = semitoneRatio(steps);

        if (sampleRate != coeffSr)
        {
            coeffSr = sampleRate;
            slowFollow = 1.0 - std::exp(-2.0 * kPi * 52.0 / sampleRate);
            envAttack = 1.0 - std::exp(-1.0 / (0.003 * sampleRate));
            envRelease = 1.0 - std::exp(-1.0 / (0.090 * sampleRate));
            gateFollow = 1.0 - std::exp(-1.0 / (0.012 * sampleRate));
            fastHzCached = -1.0;
        }
        slowWave += slowFollow * (input - slowWave);
        slowWave2 += slowFollow * (slowWave - slowWave2);

        double fastHz = 160.0;
        if (fast.locked && fast.period > sampleRate / 520.0)
            fastHz = clamp(sampleRate / fast.period * 1.25, 100.0, 280.0);
        if (fastHz != fastHzCached)
        {
            fastHzCached = fastHz;
            fastFollow = 1.0 - std::exp(-2.0 * kPi * fastHz / sampleRate);
        }
        fastWave += fastFollow * (input - fastWave);

        bass.advance(slowWave2, sampleRate, 42.0, 110.0, ratio, envAttack, envRelease, gateFollow);
        fast.advance(fastWave, sampleRate, 80.0, 520.0, ratio, envAttack, envRelease, gateFollow);

        const bool useBass = bass.locked
                          && bass.env > 0.015
                          && fast.env > 0.0001
                          && bass.env > fast.env * 0.26
                          && bass.period > sampleRate / 105.0;
        const Follower& chosen = useBass ? bass : fast;
        // Sine at the chosen semitone offset, level following the note.
        return std::sin(chosen.sinePhase) * chosen.env * chosen.gate;
    }

private:
    struct Follower
    {
        double env = 0.0;
        double period = 0.0;
        double since = 0.0;
        double hold = 0.0;
        double gate = 0.0;
        double flip = 1.0;
        double sinePhase = 0.0;
        bool high = false;
        bool locked = false;

        void reset() noexcept
        {
            *this = {};
            flip = 1.0;
        }

        void advance(double x, double sampleRate, double minHz, double maxHz, double ratio,
                     double envAttack, double envRelease, double gateFollow) noexcept
        {
            const double minPeriod = sampleRate / maxHz;
            const double maxPeriod = sampleRate / minHz;
            const double mag = std::abs(x);
            const double envCoeff = mag > env ? envAttack : envRelease;
            env += envCoeff * (mag - env);

            const double thresh = std::max(0.010, env * 0.28);
            const bool rising = !high && x > thresh;
            const bool falling = high && x < -thresh;
            if (rising)
            {
                const bool inRange = since >= minPeriod && since <= maxPeriod;
                const bool tooSoon = locked && since < period * 0.72;
                if (inRange && !tooSoon)
                {
                    period = locked ? (period * 0.82 + since * 0.18) : since;
                    locked = true;
                    flip = -flip;
                    since = 0.0;
                    hold = 0.055 * sampleRate;
                }
                high = true;
            }
            else if (falling)
            {
                high = false;
            }

            since += 1.0;
            if (locked && since > period * 2.6)
                locked = false;
            if (hold > 0.0)
                hold -= 1.0;

            if (period > 1.0)
            {
                sinePhase += (2.0 * kPi * ratio) / period;
                while (sinePhase >= 2.0 * kPi)
                    sinePhase -= 2.0 * kPi;
            }

            const double target = (locked || hold > 0.0) ? 1.0 : 0.0;
            gate += gateFollow * (target - gate);
        }
    };

    static double semitoneRatio(int steps) noexcept
    {
        static constexpr double kRatio[25] = {
            0.5000000000000000,
            0.5297315471796477,
            0.5612310241546865,
            0.5946035575013605,
            0.6299605249474366,
            0.6674199270850172,
            0.7071067811865476,
            0.7491535384383408,
            0.7937005259840998,
            0.8408964152537145,
            0.8908987181403393,
            0.9438743126816935,
            1.0000000000000000,
            1.0594630943592953,
            1.1224620483093730,
            1.1892071150027210,
            1.2599210498948732,
            1.3348398541700344,
            1.4142135623730951,
            1.4983070768766815,
            1.5874010519681994,
            1.6817928305074290,
            1.7817974362806785,
            1.8877486253633868,
            2.0000000000000000,
        };
        return kRatio[steps + 12];
    }

    double slowWave = 0.0;
    double slowWave2 = 0.0;
    double fastWave = 0.0;
    double coeffSr = -1.0;
    double slowFollow = 0.0;
    double fastFollow = 0.0;
    double fastHzCached = -1.0;
    double envAttack = 0.0;
    double envRelease = 0.0;
    double gateFollow = 0.0;
    Follower bass;
    Follower fast;
};

// pitch 0 keeps the octave-down divider. 1 is the lowpassed note. 2 is a
// full-wave octave up, with dc tracking so the offset does not thump.
inline double subAtPitch(double lowpassed, double divided, int pitch, double& dc, double sampleRate) noexcept
{
    if (pitch <= 0)
        return divided;
    if (pitch == 1)
        return lowpassed;
    const double rect = std::abs(lowpassed);
    const double follow = 1.0 - std::exp(-2.0 * kPi * 8.0 / std::max(8000.0, sampleRate));
    dc += follow * (rect - dc);
    return (rect - dc) * 2.4;
}

} // namespace doomgazer
