#pragma once

#include "doomgazer/MathConstants.h"

#include <array>
#include <string>

namespace doomgazer {

struct ParameterRange
{
    double min = 0.0;
    double max = 1.0;
    double step = 0.01;
};

// Live control state. Field order is the preset and project order, index 1..kParameterCount.
// Every field is also a host parameter. The host sees kHostParameterOrder, not this order.
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

inline constexpr const char* parameterName(int index) noexcept
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

// How the host should publish a parameter. A named two-position control (Core /
// Twin, Pre / Post) is a choice, so a control surface can show the names. An
// on/off switch is a toggle. MIDI learn still uses parameterIsSwitch.
enum class ParameterKind
{
    continuous,
    toggle,
    choice
};

struct ChoiceList
{
    const char* const* names = nullptr;
    int count = 0;
};

// Choice index 0 is parameter value 0, and so on. The lists match the panel.
inline ChoiceList parameterChoices(int index) noexcept
{
    static constexpr const char* core[] = { "Core", "Twin" };
    static constexpr const char* delay[] = { "Off", "Tape Echo", "Reverse" };
    static constexpr const char* place[] = { "Pre", "Post" };
    static constexpr const char* reverb[] = { "Off", "Abyss", "Shimmer", "Spring" };
    static constexpr const char* order[] = { "Delay First", "Reverb First" };
    static constexpr const char* tremolo[] = { "Tube Bias", "Opto", "Harmonic", "Flutter" };
    static constexpr const char* glide[] = { "Off", "Early Ref", "Reverse" };
    static constexpr const char* howl[] = { "H2", "H3", "H4", "H5" };
    static constexpr const char* image[] = { "Mono", "Stereo" };
    static constexpr const char* pedal[] = { "Volume", "Wah" };

    switch (index)
    {
        case 1: return { core, 2 };
        case 37: return { delay, 3 };
        case 41: return { place, 2 };
        case 43: return { reverb, 4 };
        case 47: return { place, 2 };
        case 49: return { order, 2 };
        case 53: return { tremolo, 4 };
        case 59: return { glide, 3 };
        case 61: return { place, 2 };
        case 82: return { howl, 4 };
        case 85: return { image, 2 };
        case 86: return { image, 2 };
        case 87: return { image, 2 };
        case 88: return { image, 2 };
        case 89: return { image, 2 };
        case 90: return { image, 2 };
        case 91: return { image, 2 };
        case 92: return { image, 2 };
        case 95: return { pedal, 2 };
        default: return {};
    }
}

inline ParameterKind parameterKind(int index) noexcept
{
    if (parameterChoices(index).count > 0)
        return ParameterKind::choice;
    if (parameterIsSwitch(index))
        return ParameterKind::toggle;
    return ParameterKind::continuous;
}

// Names the control surface shows. The editor keeps parameterName.
inline constexpr const char* parameterHostName(int index) noexcept
{
    switch (index)
    {
        case 6: return "Blend";
        case 11: return "Wall";
        case 12: return "Tilt";
        case 16: return "Dblr Level";
        case 17: return "Polarity";
        case 18: return "Dblr Thump";
        case 20: return "Sus Depth";
        case 25: return "CC Presets";
        case 30: return "Pre Cutoff";
        case 31: return "Split";
        case 32: return "Body Hz";
        case 40: return "Delay Fdbk";
        case 41: return "Delay Place";
        case 42: return "Delay Damp";
        case 47: return "Verb Place";
        case 49: return "Space Order";
        case 51: return "Trem Speed";
        case 52: return "Trem Depth";
        case 53: return "Trem Shape";
        case 54: return "Trem Width";
        case 57: return "Oct Level";
        case 61: return "Glide Place";
        case 65: return "Twin Octave";
        case 69: return "Grit Char";
        case 71: return "Grit Freq";
        case 75: return "Phaser Fdbk";
        case 79: return "Baritone";
        case 86: return "Verb Stereo";
        case 87: return "Chor Stereo";
        case 88: return "Phas Stereo";
        case 89: return "Trem Stereo";
        case 92: return "Cab Stereo";
        case 97: return "Dblr Scoop";
        case 98: return "Dblr Cut";
        case 100: return "Dblr Pres";
        default: return parameterName(index);
    }
}

// What the host is allowed to see, in bank order. 63 leaves room for the Bypass
// parameter JUCE adds on VST3: Live auto-maps a plugin only at 64 or fewer.
// Everything else stays in the plugin for the panel, presets, and MIDI learn.
inline constexpr int kHostVisibleCount = 63;
inline constexpr int kHostParameterOrder[kHostVisibleCount] = {
    24, 2, 3, 35, 4, 63, 20, 28,
    21, 22, 23, 32, 33, 7, 11, 12,
    38, 39, 40, 42, 44, 45, 46, 48,
    51, 52, 54, 56, 60, 80, 81, 73,
    74, 75, 76, 77, 78, 83, 84, 70,
    1, 37, 43, 53, 59, 82, 36, 55,
    66, 67, 62, 19, 34, 14, 50, 93,
    41, 47, 61, 49, 95, 9, 17,
};

constexpr std::array<bool, kParameterCount + 1> makeHostVisible() noexcept
{
    std::array<bool, kParameterCount + 1> table {};
    for (const int index : kHostParameterOrder)
        table[static_cast<std::size_t>(index)] = true;
    return table;
}

inline constexpr std::array<bool, kParameterCount + 1> kHostVisible = makeHostVisible();

inline bool parameterIsHostVisible(int index) noexcept
{
    return index >= 1 && index <= kParameterCount
        && kHostVisible[static_cast<std::size_t>(index)];
}

constexpr bool hostNamesFitControlSurface(int limit) noexcept
{
    for (int index = 1; index <= kParameterCount; ++index)
    {
        const char* name = parameterHostName(index);
        int length = 0;
        while (name[length] != '\0')
            ++length;
        if (length == 0 || length > limit)
            return false;
    }
    return true;
}

static_assert(hostNamesFitControlSurface(12));

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

// Controller mode, the preset mirror, and the preset CC are project state.
// A control surface skips anything that is not automatable.
inline constexpr bool parameterIsAutomatable(int index) noexcept
{
    return index != kControllerModeParameter
        && index != kPresetSlotParameter
        && index != kPresetCcParameter;
}

constexpr bool publishedSetFitsLive() noexcept
{
    bool seen[kParameterCount + 1] = {};
    for (const int index : kHostParameterOrder)
    {
        if (index < 1 || index > kParameterCount || seen[index])
            return false;
        if (!parameterIsAutomatable(index))
            return false;
        seen[index] = true;
    }
    int count = 0;
    for (int index = 1; index <= kParameterCount; ++index)
        if (seen[index])
            ++count;
    return count == kHostVisibleCount && kHostVisibleCount <= 63;
}

static_assert(publishedSetFitsLive());

inline constexpr int kFactoryPresetCount = 33;
inline constexpr int kPresetSlots = 128;

std::string presetName(int index);
// Browsing only. The name is the tone; the category groups it in the browser.
const char* presetCategory(int index);
Parameters factoryPreset(int index);

} // namespace doomgazer
