#pragma once

#include "doomgazer/MathConstants.h"

#include <string>

namespace doomgazer {

struct ParameterRange
{
    double min = 0.0;
    double max = 1.0;
    double step = 0.01;
};

// Live control state. Field order is the host parameter order, index 1..kParameterCount.
// Every field is also a host parameter; the editor decides which page shows it.
struct Parameters
{
    double engineMode = 1.0;
    double coreGainDb = 6.0;
    double coreLevelDb = 0.0;
    double subWeight = 0.0;
    double subDrive = 0.25;
    double spectrumBlend = 0.50;
    double lowGainDb = 11.0;
    double subOctave = 0.25;
    double melodyCarve = 1.0;
    double carveDepth = 0.65;
    double harmonicGainDb = 34.0;
    double spectralTilt = 0.52;
    double inputTight = 0.55;
    double doublerOn = 1.0;
    double doublerSpread = 0.55;
    double doublerLevel = 0.50;
    double polarityInvert = 0.0;
    double doublerThumpDb = 4.2;
    double sustainOn = 1.0;
    double sustain = 0.65;
    double thumpDb = 4.2;
    double midDb = 0.0;
    double highCutHz = 2700.0;
    double masterDb = -4.5;
    double controllerMode = 0.0;
    double presetSlot = 0.0;
    double presetCc = 103.0;
    double sag = 0.42;
    double antiFizzHz = 1900.0;
    double preDriveHz = 110.0;
    double splitHz = 180.0;
    double shroudHz = 125.0;
    double presenceHz = 1350.0;
    double shroudOn = 1.0;
    double coreTone = 0.50;
    double limiterOn = 1.0;
    double delayMode = 0.0;
    double delayMix = 0.35;
    double delayMs = 480.0;
    double delayFeedback = 0.55;
    double delayRouting = 0.0;
    double delayDampHz = 2800.0;
    double reverbMode = 0.0;
    double reverbMix = 0.35;
    double reverbSizeMs = 650.0;
    double reverbDecay = 0.75;
    double reverbRouting = 0.0;
    double reverbDampHz = 3200.0;
    double fxOrder = 0.0;
    double tremoloOn = 0.0;
    double tremoloHz = 4.2;
    double tremoloDepth = 0.65;
    double tremoloShape = 0.0;
    double tremoloSpread = 0.0;
    double tunerMute = 0.0;
    double tremoloMix = 1.0;
    double subOctLevel = 0.0;
    double subOctToneHz = 140.0;
    double glideMode = 0.0;
    double glideMix = 0.0;
    double glideRoute = 0.0;
    double inputGate = 0.0;
    double inputTrimDb = 0.0;
    double coreSubShift = -12.0;
    double lowSubShift = -12.0;
    double freeze = 0.0;
    double sync = 0.0;
    double subSolo = 0.0;
    double gritCharacter = 0.0;
    double gritAmount = 0.0;
    double gritFreqHz = 900.0;
    double gritLevelDb = 0.0;
    double phaseRate = 0.15;
    double phaseDepth = 0.55;
    double phaseFeedback = 0.40;
    double phaseMix = 0.0;
    double chorusRate = 0.20;
    double chorusMix = 0.0;
    double chorusBaritone = 0.0;
    double swellAttack = 500.0;
    double swellAmount = 0.0;
    double howlPitch = 0.0;
    double howlAmount = 0.0;
    double howlSense = 0.50;
    double delayStereo = 1.0;
    double reverbStereo = 1.0;
    double chorusStereo = 1.0;
    double phaseStereo = 1.0;
    double tremStereo = 1.0;
    double glideStereo = 1.0;
    double howlStereo = 1.0;
    double shroudStereo = 1.0;
    double pedalOn = 0.0;
    double pedalToe = 1.0;
    double pedalMode = 0.0;
    double twinLevelDb = 0.0;
    double doublerMidDb = 0.0;
    double doublerHighCutHz = 2700.0;
    double doublerBodyHz = 125.0;
    double doublerPresenceHz = 1350.0;
    // Not a host parameter. 0 is octave down, 1 is the same octave, 2 is up one.
    int subPitch = 0;

    double& at(int index) noexcept;
    double at(int index) const noexcept;
    void clampIntoRange() noexcept;
    void setQuantized(int index, double value) noexcept;
};

inline double Parameters::* const kParameterFields[kParameterCount + 1] = {
    nullptr,
    &Parameters::engineMode,
    &Parameters::coreGainDb,
    &Parameters::coreLevelDb,
    &Parameters::subWeight,
    &Parameters::subDrive,
    &Parameters::spectrumBlend,
    &Parameters::lowGainDb,
    &Parameters::subOctave,
    &Parameters::melodyCarve,
    &Parameters::carveDepth,
    &Parameters::harmonicGainDb,
    &Parameters::spectralTilt,
    &Parameters::inputTight,
    &Parameters::doublerOn,
    &Parameters::doublerSpread,
    &Parameters::doublerLevel,
    &Parameters::polarityInvert,
    &Parameters::doublerThumpDb,
    &Parameters::sustainOn,
    &Parameters::sustain,
    &Parameters::thumpDb,
    &Parameters::midDb,
    &Parameters::highCutHz,
    &Parameters::masterDb,
    &Parameters::controllerMode,
    &Parameters::presetSlot,
    &Parameters::presetCc,
    &Parameters::sag,
    &Parameters::antiFizzHz,
    &Parameters::preDriveHz,
    &Parameters::splitHz,
    &Parameters::shroudHz,
    &Parameters::presenceHz,
    &Parameters::shroudOn,
    &Parameters::coreTone,
    &Parameters::limiterOn,
    &Parameters::delayMode,
    &Parameters::delayMix,
    &Parameters::delayMs,
    &Parameters::delayFeedback,
    &Parameters::delayRouting,
    &Parameters::delayDampHz,
    &Parameters::reverbMode,
    &Parameters::reverbMix,
    &Parameters::reverbSizeMs,
    &Parameters::reverbDecay,
    &Parameters::reverbRouting,
    &Parameters::reverbDampHz,
    &Parameters::fxOrder,
    &Parameters::tremoloOn,
    &Parameters::tremoloHz,
    &Parameters::tremoloDepth,
    &Parameters::tremoloShape,
    &Parameters::tremoloSpread,
    &Parameters::tunerMute,
    &Parameters::tremoloMix,
    &Parameters::subOctLevel,
    &Parameters::subOctToneHz,
    &Parameters::glideMode,
    &Parameters::glideMix,
    &Parameters::glideRoute,
    &Parameters::inputGate,
    &Parameters::inputTrimDb,
    &Parameters::coreSubShift,
    &Parameters::lowSubShift,
    &Parameters::freeze,
    &Parameters::sync,
    &Parameters::subSolo,
    &Parameters::gritCharacter,
    &Parameters::gritAmount,
    &Parameters::gritFreqHz,
    &Parameters::gritLevelDb,
    &Parameters::phaseRate,
    &Parameters::phaseDepth,
    &Parameters::phaseFeedback,
    &Parameters::phaseMix,
    &Parameters::chorusRate,
    &Parameters::chorusMix,
    &Parameters::chorusBaritone,
    &Parameters::swellAttack,
    &Parameters::swellAmount,
    &Parameters::howlPitch,
    &Parameters::howlAmount,
    &Parameters::howlSense,
    &Parameters::delayStereo,
    &Parameters::reverbStereo,
    &Parameters::chorusStereo,
    &Parameters::phaseStereo,
    &Parameters::tremStereo,
    &Parameters::glideStereo,
    &Parameters::howlStereo,
    &Parameters::shroudStereo,
    &Parameters::pedalOn,
    &Parameters::pedalToe,
    &Parameters::pedalMode,
    &Parameters::twinLevelDb,
    &Parameters::doublerMidDb,
    &Parameters::doublerHighCutHz,
    &Parameters::doublerBodyHz,
    &Parameters::doublerPresenceHz,
};

// One table for the whole parameter set: id, display name, unit, range, and
// whether it is a two-position switch. Everything else is derived from it.
struct ParameterInfo
{
    const char* id;
    const char* name;
    const char* unit;
    double min;
    double max;
    double step;
    bool isSwitch;
};

inline constexpr ParameterInfo kParameterInfo[kParameterCount + 1] = {
    { "", "", "", 0.0, 1.0, 0.01, false },
    { "engine", "Core", "", 0.0, 1.0, 1.0, true },
    { "coreGain", "Core Drive", "dB", 0.0, 30.0, 0.1, false },
    { "coreLevel", "Core Level", "dB", -12.0, 12.0, 0.1, false },
    { "subWeight", "Sub Weight", "", 0.0, 1.0, 0.01, false },
    { "subDrive", "Sub Drive", "", 0.0, 1.0, 0.01, false },
    { "spectrumBlend", "Spectrum Blend", "", 0.0, 1.0, 0.01, false },
    { "lowGain", "Low Gain", "dB", 0.0, 30.0, 0.1, false },
    { "subOctave", "Sub-Octave", "", 0.0, 1.0, 0.01, false },
    { "melodyCarve", "Melody Carve", "", 0.0, 1.0, 1.0, true },
    { "carveDepth", "Carve Depth", "", 0.0, 1.0, 0.01, false },
    { "harmonicGain", "Harmonic Wall", "dB", 6.0, 52.0, 0.1, false },
    { "spectralTilt", "Spectral Tilt", "", 0.0, 1.0, 0.01, false },
    { "inputTight", "Input Tight", "", 0.0, 1.0, 0.01, false },
    { "doubler", "Doubler", "", 0.0, 1.0, 1.0, true },
    { "doublerSpread", "Spread", "", 0.0, 1.0, 0.01, false },
    { "doublerLevel", "Doubler Level", "", 0.0, 1.0, 0.01, false },
    { "polarity", "Doubler Polarity", "", 0.0, 1.0, 1.0, true },
    { "doublerThump", "Doubler Thump", "dB", 0.0, 12.0, 0.1, false },
    { "sustainOn", "Sustain", "", 0.0, 1.0, 1.0, true },
    { "sustain", "Sustain Depth", "", 0.0, 1.0, 0.01, false },
    { "thump", "Thump", "dB", 0.0, 12.0, 0.1, false },
    { "midContour", "Mid Contour", "dB", -8.0, 8.0, 0.1, false },
    { "highCut", "High Cut", "Hz", 1400.0, 3600.0, 10.0, false },
    { "master", "Master", "dB", -24.0, 12.0, 0.1, false },
    { "controllerMode", "Controller Mode", "", 0.0, 1.0, 1.0, true },
    { "presetSlot", "Preset", "", 0.0, 127.0, 1.0, false },
    { "presetCc", "Preset CC", "", 0.0, 127.0, 1.0, false },
    { "sag", "Sag", "", 0.0, 1.0, 0.01, false },
    { "antiFizz", "Anti-Fizz", "Hz", 900.0, 4500.0, 20.0, false },
    { "preDriveHz", "Pre-Drive Cutoff", "Hz", 40.0, 250.0, 5.0, false },
    { "splitHz", "Split Frequency", "Hz", 100.0, 320.0, 5.0, false },
    { "shroudHz", "Body Frequency", "Hz", 95.0, 175.0, 1.0, false },
    { "presenceHz", "Presence", "Hz", 800.0, 2200.0, 20.0, false },
    { "shroud", "Contour", "", 0.0, 1.0, 1.0, true },
    { "coreTone", "Core Tone", "", 0.0, 1.0, 0.01, false },
    { "limiter", "Limiter", "", 0.0, 1.0, 1.0, true },
    { "delayMode", "Delay Mode", "", 0.0, 2.0, 1.0, false },
    { "delayMix", "Delay Mix", "", 0.0, 1.0, 0.01, false },
    { "delayTime", "Delay Time", "ms", 60.0, 1200.0, 10.0, false },
    { "delayFeedback", "Delay Feedback", "", 0.0, 1.25, 0.01, false },
    { "delayRouting", "Delay Position", "", 0.0, 1.0, 1.0, true },
    { "delayDamp", "Delay Damping", "Hz", 800.0, 6000.0, 50.0, false },
    { "reverbMode", "Reverb Mode", "", 0.0, 3.0, 1.0, false },
    { "reverbMix", "Reverb Mix", "", 0.0, 1.0, 0.01, false },
    { "reverbSize", "Reverb Size", "ms", 80.0, 2500.0, 10.0, false },
    { "reverbDecay", "Reverb Decay", "", 0.0, 0.99, 0.01, false },
    { "reverbRouting", "Reverb Position", "", 0.0, 1.0, 1.0, true },
    { "reverbDamp", "Reverb Sheen", "Hz", 800.0, 8000.0, 50.0, false },
    { "fxOrder", "Atmosphere Order", "", 0.0, 1.0, 1.0, true },
    { "tremolo", "Tremolo", "", 0.0, 1.0, 1.0, true },
    { "tremoloSpeed", "Tremolo Speed", "Hz", 0.5, 15.0, 0.1, false },
    { "tremoloDepth", "Tremolo Depth", "", 0.0, 1.0, 0.01, false },
    { "tremoloShape", "Tremolo Shape", "", 0.0, 3.0, 1.0, false },
    { "tremoloSpread", "Tremolo Width", "", 0.0, 1.0, 0.01, false },
    { "tunerMute", "Mute", "", 0.0, 1.0, 1.0, true },
    { "tremoloMix", "Tremolo Mix", "", 0.0, 1.0, 0.01, false },
    { "subOctLevel", "Sub-Oct Level", "", 0.0, 1.0, 0.01, false },
    { "subOctTone", "Sub-Oct Tone", "Hz", 50.0, 400.0, 5.0, false },
    { "glideMode", "Glide Wash", "", 0.0, 2.0, 1.0, false },
    { "glideMix", "Glide Mix", "", 0.0, 1.0, 0.01, false },
    { "glideRoute", "Glide Position", "", 0.0, 1.0, 1.0, true },
    { "inputGate", "Input Gate", "", 0.0, 1.0, 1.0, true },
    { "inputTrim", "Input Trim", "dB", -36.0, 12.0, 0.1, false },
    { "coreSubShift", "Core Octave", "st", -12.0, 12.0, 1.0, false },
    { "lowSubShift", "Twin Core Octave", "st", -12.0, 12.0, 1.0, false },
    { "freeze", "Freeze", "", 0.0, 1.0, 1.0, true },
    { "sync", "Sync", "", 0.0, 1.0, 1.0, true },
    { "subSolo", "Sub Solo", "", 0.0, 1.0, 0.01, false },
    { "gritCharacter", "Grit Character", "", 0.0, 1.0, 0.01, false },
    { "gritAmount", "Grit Amount", "", 0.0, 1.0, 0.01, false },
    { "gritFreq", "Grit Frequency", "Hz", 500.0, 2500.0, 10.0, false },
    { "gritLevel", "Grit Level", "dB", -12.0, 12.0, 0.1, false },
    { "phaseRate", "Phaser Rate", "Hz", 0.05, 4.0, 0.01, false },
    { "phaseDepth", "Phaser Depth", "", 0.0, 1.0, 0.01, false },
    { "phaseFeedback", "Phaser Feedback", "", 0.0, 0.85, 0.01, false },
    { "phaseMix", "Phaser Mix", "", 0.0, 1.0, 0.01, false },
    { "chorusRate", "Chorus Rate", "Hz", 0.08, 1.2, 0.01, false },
    { "chorusMix", "Chorus Mix", "", 0.0, 1.0, 0.01, false },
    { "chorusBaritone", "Chorus Baritone", "", 0.0, 1.0, 1.0, true },
    { "swellAttack", "Swell Attack", "ms", 20.0, 1200.0, 10.0, false },
    { "swellAmount", "Swell Amount", "", 0.0, 1.0, 0.01, false },
    { "howlPitch", "Howl Pitch", "", 0.0, 3.0, 1.0, false },
    { "howlAmount", "Howl Amount", "", 0.0, 1.0, 0.01, false },
    { "howlSense", "Howl Sense", "", 0.0, 1.0, 0.01, false },
    { "delayStereo", "Delay Stereo", "", 0.0, 1.0, 1.0, true },
    { "reverbStereo", "Reverb Stereo", "", 0.0, 1.0, 1.0, true },
    { "chorusStereo", "Chorus Stereo", "", 0.0, 1.0, 1.0, true },
    { "phaseStereo", "Phaser Stereo", "", 0.0, 1.0, 1.0, true },
    { "tremStereo", "Tremolo Stereo", "", 0.0, 1.0, 1.0, true },
    { "glideStereo", "Glide Stereo", "", 0.0, 1.0, 1.0, true },
    { "howlStereo", "Howl Stereo", "", 0.0, 1.0, 1.0, true },
    { "shroudStereo", "Contour Stereo", "", 0.0, 1.0, 1.0, true },
    { "pedal", "Pedal", "", 0.0, 1.0, 1.0, true },
    { "pedalToe", "Pedal Toe", "", 0.0, 1.0, 0.01, false },
    { "pedalMode", "Pedal Mode", "", 0.0, 1.0, 1.0, true },
    { "twinLevel", "Twin Level", "dB", -12.0, 12.0, 0.1, false },
    { "doublerMid", "Doubler Scoop", "dB", -8.0, 8.0, 0.1, false },
    { "doublerHighCut", "Doubler High Cut", "Hz", 1400.0, 3600.0, 10.0, false },
    { "doublerBody", "Doubler Body", "Hz", 95.0, 175.0, 1.0, false },
    { "doublerPresence", "Doubler Presence", "Hz", 800.0, 2200.0, 20.0, false },
};

inline ParameterRange parameterRange(int index) noexcept
{
    if (index < 1 || index > kParameterCount)
        return {};
    const ParameterInfo& info = kParameterInfo[index];
    return { info.min, info.max, info.step };
}

inline const char* parameterId(int index) noexcept
{
    if (index < 1 || index > kParameterCount)
        return "unused";
    return kParameterInfo[index].id;
}

inline const char* parameterName(int index) noexcept
{
    if (index < 1 || index > kParameterCount)
        return "Unused";
    return kParameterInfo[index].name;
}

inline const char* parameterUnit(int index) noexcept
{
    if (index < 1 || index > kParameterCount)
        return "";
    return kParameterInfo[index].unit;
}

// Switches come from the same table, so adding a two-position parameter cannot
// fall out of step with the list the MIDI code and the editor consult.
inline bool parameterIsSwitch(int index) noexcept
{
    if (index < 1 || index > kParameterCount)
        return false;
    return kParameterInfo[index].isSwitch;
}

inline double& Parameters::at(int index) noexcept
{
    return this->*kParameterFields[(index >= 1 && index <= kParameterCount) ? index : 1];
}

inline double Parameters::at(int index) const noexcept
{
    return this->*kParameterFields[(index >= 1 && index <= kParameterCount) ? index : 1];
}

inline void Parameters::clampIntoRange() noexcept
{
    for (int i = 1; i <= kParameterCount; ++i)
    {
        const ParameterRange range = parameterRange(i);
        at(i) = clamp(at(i), range.min, range.max);
    }
}

inline void Parameters::setQuantized(int index, double value) noexcept
{
    if (index < 1 || index > kParameterCount)
        return;
    const ParameterRange range = parameterRange(index);
    value = clamp(value, range.min, range.max);
    if (range.step >= 1.0)
        value = std::floor(value + 0.5);
    else if (range.step > 0.0)
        value = std::floor(value / range.step + 0.5) * range.step;
    at(index) = clamp(value, range.min, range.max);
}

// Two sounds are the same when every parameter agrees to within half a step.
// The host hands back float32, so comparing exactly would mark a freshly loaded
// preset dirty on the first tick.
inline bool parametersMatch(const Parameters& left, const Parameters& right) noexcept
{
    for (int index = 1; index <= kParameterCount; ++index)
    {
        const double step = std::max(1.0e-6, parameterRange(index).step);
        if (std::abs(left.at(index) - right.at(index)) > step * 0.51)
            return false;
    }
    return true;
}

// Session PA mono forces mono-safe processing without changing stored preset values.
inline void resolveStereoPolicy(Parameters& heard, bool paMono) noexcept
{
    if (!paMono)
    {
        if (!asOn(heard.tremStereo))
            heard.tremoloSpread = 0.0;
        return;
    }
    heard.doublerSpread = 0.0;
    heard.delayStereo = 0.0;
    heard.reverbStereo = 0.0;
    heard.chorusStereo = 0.0;
    heard.phaseStereo = 0.0;
    heard.tremStereo = 0.0;
    heard.glideStereo = 0.0;
    heard.howlStereo = 0.0;
    heard.shroudStereo = 0.0;
    heard.tremoloSpread = 0.0;
}

inline constexpr int kControllerModeParameter = 25;
inline constexpr int kPresetSlotParameter = 26;
inline constexpr int kPresetCcParameter = 27;
inline constexpr int kFreezeParameter = 66;

inline constexpr int kFactoryPresetCount = 33;
inline constexpr int kPresetSlots = 128;

std::string presetName(int index);
// Browsing only. The name is the tone; the category groups it in the browser.
const char* presetCategory(int index);
Parameters factoryPreset(int index);

} // namespace doomgazer
