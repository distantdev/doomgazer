#pragma once

#include "doomgazer/Coefficients.h"
#include "doomgazer/FirstOrderFilters.h"

namespace doomgazer {

class Tremolo
{
public:
    void reset() noexcept
    {
        phase = 0.0;
        lampL = 0.0;
        lampR = 0.0;
        xoverL = 0.0;
        xoverR = 0.0;
        flutterLevel = 0.0;
        flutterCrest = 0.0;
        flutterAmt = 0.0;
        flutterGainL = 1.0;
        flutterGainR = 1.0;
    }

    void process(double& left, double& right, double /*incoming*/, const Parameters& params, const Coefficients& c) noexcept
    {
        if (!asOn(params.tremoloOn) || params.tremoloMix <= 0.001)
            return;

        const double sampleRate = std::max(8000.0, c.sampleRate);
        const double dryL = left;
        const double dryR = right;
        const int shape = asInt(params.tremoloShape);
        if (shape >= 3)
        {
            if (sampleRate != flutterSr)
            {
                flutterSr = sampleRate;
                levelAtt = 1.0 - std::exp(-1.0 / (0.004 * sampleRate));
                levelRel = 1.0 - std::exp(-1.0 / (0.160 * sampleRate));
                crestForget = 1.0 - std::exp(-1.0 / (0.060 * sampleRate));
                amtAtt = 1.0 - std::exp(-1.0 / (0.010 * sampleRate));
                amtRel = 1.0 - std::exp(-1.0 / (0.070 * sampleRate));
                gainSlew = 1.0 - std::exp(-1.0 / (0.0015 * sampleRate));
            }

            // Follow the sound being fluttered, not the dry pick. A drone stays
            // loud long after the string's transient, and that is the level the
            // player hears falling.
            const double heard = std::max(std::abs(left), std::abs(right));
            const double levelFollow = heard > flutterLevel ? levelAtt : levelRel;
            flutterLevel += levelFollow * (heard - flutterLevel);
            if (flutterLevel >= flutterCrest)
                flutterCrest = flutterLevel;
            else if (flutterLevel < 0.0005 || flutterLevel < flutterCrest * 0.012)
                flutterCrest += crestForget * (flutterLevel - flutterCrest);

            // Full chatter while the note is still near its peak. Depth is gone
            // once the level has fallen about 10 dB, so a long tail finishes
            // fluttering while it is still clearly audible.
            double settle = 0.0;
            if (flutterCrest > 0.0015)
            {
                const double ratio = flutterLevel / flutterCrest;
                const double t = clamp((ratio - 0.33) / (0.70 - 0.33), 0.0, 1.0);
                settle = t * t * (3.0 - 2.0 * t);
            }
            const double amtFollow = settle > flutterAmt ? amtAtt : amtRel;
            flutterAmt += amtFollow * (settle - flutterAmt);

            // While the note is still up, rate runs past the Speed knob into
            // a cone chatter. Depth tracks flutterAmt, so it dies with the fall.
            double targetL = 1.0;
            double targetR = 1.0;
            if (flutterAmt > 0.0008)
            {
                const double rate = clamp(params.tremoloHz * std::exp(2.2 * flutterAmt), 0.35, 36.0);
                phase += (2.0 * kPi * rate) / sampleRate;
                while (phase >= 2.0 * kPi)
                    phase -= 2.0 * kPi;
                double phaseR = phase + params.tremoloSpread * kPi;
                while (phaseR >= 2.0 * kPi)
                    phaseR -= 2.0 * kPi;
                const double depth = params.tremoloDepth * flutterAmt;
                const double lfoL = coneLfo(phase, flutterAmt);
                const double lfoR = coneLfo(phaseR, flutterAmt);
                targetL = 1.0 - depth * (1.0 - lfoL);
                targetR = 1.0 - depth * (1.0 - lfoR);
            }
            flutterGainL += gainSlew * (targetL - flutterGainL);
            flutterGainR += gainSlew * (targetR - flutterGainR);
            left *= flutterGainL;
            right *= flutterGainR;
        }
        else
        {
            phase += c.tremLfoStep;
            if (phase >= 2.0 * kPi)
                phase -= 2.0 * kPi;

            double phaseR = phase + params.tremoloSpread * kPi;
            if (phaseR >= 2.0 * kPi)
                phaseR -= 2.0 * kPi;

            if (shape <= 0)
            {
                double lfoL = 0.5 + 0.5 * std::sin(phase);
                lfoL = lfoL * (0.82 + 0.18 * lfoL);
                double lfoR = 0.5 + 0.5 * std::sin(phaseR);
                lfoR = lfoR * (0.82 + 0.18 * lfoR);
                left *= 1.0 - params.tremoloDepth * (1.0 - lfoL);
                right *= 1.0 - params.tremoloDepth * (1.0 - lfoR);
            }
            else if (shape == 1)
            {
                const double strikeL = std::sin(phase) > -0.2 ? 1.0 : 0.0;
                const double strikeR = std::sin(phaseR) > -0.2 ? 1.0 : 0.0;
                lampL = lampL * 0.965 + strikeL * 0.035;
                lampR = lampR * 0.965 + strikeR * 0.035;
                left *= 1.0 - params.tremoloDepth * (1.0 - lampL);
                right *= 1.0 - params.tremoloDepth * (1.0 - lampR);
            }
            else
            {
                xoverL = c.tremXoverA * (left - xoverL) + xoverL + kDenorm;
                xoverR = c.tremXoverA * (right - xoverR) + xoverR + kDenorm;
                const double loL = xoverL;
                const double hiL = left - xoverL;
                const double loR = xoverR;
                const double hiR = right - xoverR;
                const double lfoLoL = 0.5 + 0.5 * std::sin(phase);
                const double lfoHiL = 0.5 + 0.5 * std::sin(phase + kPi);
                const double lfoLoR = 0.5 + 0.5 * std::sin(phaseR);
                const double lfoHiR = 0.5 + 0.5 * std::sin(phaseR + kPi);
                left = loL * (1.0 - params.tremoloDepth * (1.0 - lfoLoL))
                     + hiL * (1.0 - params.tremoloDepth * (1.0 - lfoHiL));
                right = loR * (1.0 - params.tremoloDepth * (1.0 - lfoLoR))
                      + hiR * (1.0 - params.tremoloDepth * (1.0 - lfoHiR));
            }
        }

        const double mix = clamp(params.tremoloMix, 0.0, 1.0);
        left = dryL * (1.0 - mix) + left * mix;
        right = dryR * (1.0 - mix) + right * mix;
    }

private:
    static double coneLfo(double ph, double drive) noexcept
    {
        const double s = std::sin(ph);
        double lfo = 0.5 + 0.5 * s;
        const double curve = 1.0 - 0.72 * drive;
        lfo = std::pow(std::max(lfo, 1.0e-6), curve);
        const double buzz = 0.5 + 0.5 * std::sin(ph * 2.07 + 0.35);
        return lfo * (1.0 - 0.40 * drive) + lfo * buzz * (0.40 * drive);
    }

    double phase = 0.0;
    double lampL = 0.0;
    double lampR = 0.0;
    double xoverL = 0.0;
    double xoverR = 0.0;
    double flutterLevel = 0.0;
    double flutterCrest = 0.0;
    double flutterAmt = 0.0;
    double flutterGainL = 1.0;
    double flutterGainR = 1.0;
    double flutterSr = -1.0;
    double levelAtt = 0.0;
    double levelRel = 0.0;
    double crestForget = 0.0;
    double amtAtt = 0.0;
    double amtRel = 0.0;
    double gainSlew = 1.0;
};

} // namespace doomgazer
