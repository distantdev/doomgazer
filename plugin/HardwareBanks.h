#pragma once

namespace doomgazer {

// The actions are the live MIDI targets. They are not host parameters: each
// one toggles or restores a group of parameters, and each can carry one MIDI
// binding (a CC or a note), stored in the project state.
enum class Stomp : int
{
    core = 0,
    delay,
    reverb,
    carve,
    subOctave,
    doubler,
    sustain,
    contour,
    tremolo,
    glide,
    swell,
    grit,
    phaser,
    chorus,
    howl,
    invert,
    limit,
    gate,
    pedal,
    presetPrev,
    presetNext
};

inline constexpr int kStompCount = static_cast<int>(Stomp::presetNext) + 1;

inline constexpr const char* kStompName[kStompCount] = {
    "Core", "Delay", "Reverb", "Carve", "Sub-Oct", "Doubler", "Sustain",
    "Contour", "Tremolo", "Glide", "Swell", "Grit", "Phaser",
    "Chorus", "Howl", "Invert", "Limit", "Gate", "Pedal",
    "Preset Prev", "Preset Next"
};

} // namespace doomgazer
